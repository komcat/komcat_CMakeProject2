#include "SiPhOGClientDebugUI.h"
#include <imgui.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

SiPhOGClientDebugUI::SiPhOGClientDebugUI()
  : m_socket(std::make_unique<SocketWrapper>())
  , m_connected(false)
  , m_dataCollectionActive(false)
  , m_shouldStop(false)
  , m_isVisible(false)
  , m_windowName("SiPhOG Client Debug")
  , m_portBuffer(65432)
  , m_autoScroll(true)
  , m_showRawData(false)
  , m_showParseErrors(true)
{
  InitializeDataKeys();
  strcpy_s(m_hostBuffer, sizeof(m_hostBuffer), "127.0.0.1");
}

SiPhOGClientDebugUI::~SiPhOGClientDebugUI() {
  StopDataCollection();
  Disconnect();
}

void SiPhOGClientDebugUI::InitializeDataKeys() {
  m_dataKeys = {
      "SLED_Current (mA)",
      "Photo Current (uA)",
      "SLED_Temp (C)",
      "Target SAG_PWR (V)",
      "SAG_PWR (V)",
      "TEC_Current (mA)"
  };
}

bool SiPhOGClientDebugUI::Connect(const std::string& host, int port) {
  if (m_connected) {
    return true;
  }

  m_host = host;
  m_port = port;

  if (!m_socket->Connect(host, port)) {
    return false;
  }

  // Set timeout
  m_socket->SetTimeout(100);

  m_connected = true;

  // Initialize stats
  {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats = ConnectionStats();
    m_stats.startTime = std::chrono::steady_clock::now();
    m_stats.isConnected = true;
  }

  return true;
}

void SiPhOGClientDebugUI::Disconnect() {
  if (!m_connected) {
    return;
  }

  m_connected = false;
  m_socket->Disconnect();

  {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats.isConnected = false;
  }
}

bool SiPhOGClientDebugUI::StartDataCollection() {
  if (m_dataCollectionActive || !m_connected) {
    return false;
  }

  m_shouldStop = false;
  m_dataCollectionActive = true;

  m_dataThread = std::thread(&SiPhOGClientDebugUI::DataCollectionLoop, this);

  {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats.isCollecting = true;
  }

  return true;
}

void SiPhOGClientDebugUI::StopDataCollection() {
  if (!m_dataCollectionActive) {
    return;
  }

  m_shouldStop = true;
  m_dataCollectionActive = false;

  if (m_dataThread.joinable()) {
    m_dataThread.join();
  }

  {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats.isCollecting = false;
  }
}

void SiPhOGClientDebugUI::DataCollectionLoop() {
  std::string buffer;
  char recvBuffer[1024];

  std::cout << "[DEBUG] Starting data collection loop..." << std::endl;

  while (!m_shouldStop && m_connected) {
    try {
      int bytesReceived = m_socket->Receive(recvBuffer, sizeof(recvBuffer) - 1);

      if (bytesReceived > 0) {
        recvBuffer[bytesReceived] = '\0';
        buffer += std::string(recvBuffer);

        // Debug: Show raw received data periodically
        static int debugCounter = 0;
        if (++debugCounter % 100 == 1) {
          std::cout << "[DEBUG] Received " << bytesReceived << " bytes, buffer size: " << buffer.size() << std::endl;
          std::cout << "[DEBUG] Raw data sample: " << buffer.substr(0, (std::min)((size_t)100, buffer.size())) << "..." << std::endl;
        }

        // Look for complete CSV lines (ending with newline or having 6 comma-separated values)
        size_t lineStart = 0;

        while (lineStart < buffer.length()) {
          // Find potential line ending
          size_t lineEnd = buffer.find('\n', lineStart);

          std::string candidateLine;
          bool hasNewline = false;

          if (lineEnd != std::string::npos) {
            // Found newline - extract line
            candidateLine = buffer.substr(lineStart, lineEnd - lineStart);
            hasNewline = true;
          }
          else {
            // No newline found - check if we have enough comma-separated values
            candidateLine = buffer.substr(lineStart);

            // Count commas to see if we have a complete message
            size_t commaCount = 0;
            for (char c : candidateLine) {
              if (c == ',') commaCount++;
            }

            // We need exactly 5 commas for 6 values
            if (commaCount < 5) {
              // Incomplete line, wait for more data
              break;
            }

            // We have enough commas, but let's be more careful
            // Extract exactly 6 values
            std::vector<std::string> parts;
            std::stringstream ss(candidateLine);
            std::string item;

            while (std::getline(ss, item, ',') && parts.size() < 6) {
              parts.push_back(item);
            }

            if (parts.size() < 6) {
              // Still not enough, wait for more data
              break;
            }

            // Reconstruct line with exactly 6 values
            candidateLine = "";
            for (size_t i = 0; i < 6; ++i) {
              if (i > 0) candidateLine += ",";
              candidateLine += parts[i];
            }
          }

          // Clean up the line
          candidateLine.erase(0, candidateLine.find_first_not_of(" \t\r\n"));
          candidateLine.erase(candidateLine.find_last_not_of(" \t\r\n") + 1);

          if (!candidateLine.empty() && candidateLine.find(',') != std::string::npos) {
            // Count commas to validate
            size_t commaCount = 0;
            for (char c : candidateLine) {
              if (c == ',') commaCount++;
            }

            if (commaCount == 5) { // Exactly 6 values
              std::map<std::string, float> values;
              bool success = ParseData(candidateLine, values);
              AddDataPoint(candidateLine, values, success);
              UpdateStats(success);

              if (debugCounter % 100 == 1) {
                std::cout << "[DEBUG] Processed line: " << candidateLine << " -> " << (success ? "SUCCESS" : "FAILED") << std::endl;
              }
            }
          }

          // Move to next line
          if (hasNewline) {
            lineStart = lineEnd + 1;
          }
          else {
            // We processed a line without newline, need to calculate how much to remove
            // Remove the processed part based on the number of values we extracted
            std::stringstream ss(buffer.substr(lineStart));
            std::string item;
            size_t consumedChars = 0;
            size_t valueCount = 0;

            while (std::getline(ss, item, ',') && valueCount < 6) {
              consumedChars += item.length() + 1; // +1 for comma
              valueCount++;
            }

            if (valueCount == 6) {
              consumedChars--; // Remove last comma count
              lineStart += consumedChars;
            }
            else {
              break; // Couldn't find complete line
            }
          }
        }

        // Remove processed data from buffer
        if (lineStart > 0) {
          buffer.erase(0, lineStart);
        }

        // Prevent buffer from growing too large
        if (buffer.size() > 10000) {
          std::cout << "[WARNING] Buffer too large (" << buffer.size() << "), clearing..." << std::endl;
          buffer.clear();
        }

      }
      else if (bytesReceived == -1) {
        // Error or disconnect
        std::cout << "[DEBUG] Socket error or disconnect" << std::endl;
        m_connected = false;
        break;
      }
      // bytesReceived == 0 means timeout, continue

      std::this_thread::sleep_for(std::chrono::milliseconds(1));

    }
    catch (const std::exception& e) {
      std::cout << "[ERROR] Exception in data loop: " << e.what() << std::endl;
      break;
    }
  }

  std::cout << "[DEBUG] Data collection loop ended" << std::endl;
  m_dataCollectionActive = false;
}

bool SiPhOGClientDebugUI::ParseData(const std::string& dataStr, std::map<std::string, float>& values) {
  try {
    values.clear();
    std::vector<float> floatValues;
    std::stringstream ss(dataStr);
    std::string item;

    while (std::getline(ss, item, ',') && floatValues.size() < m_dataKeys.size()) {
      item.erase(0, item.find_first_not_of(" \t"));
      item.erase(item.find_last_not_of(" \t") + 1);

      if (!item.empty()) {
        float value = std::stof(item);
        floatValues.push_back(value);
      }
    }

    if (floatValues.size() == m_dataKeys.size()) {
      for (size_t i = 0; i < m_dataKeys.size(); ++i) {
        values[m_dataKeys[i]] = floatValues[i];
      }
      return true;
    }

    return false;

  }
  catch (const std::exception& e) {
    return false;
  }
}

void SiPhOGClientDebugUI::AddDataPoint(const std::string& rawData, const std::map<std::string, float>& values, bool parseSuccess) {
  std::lock_guard<std::mutex> lock(m_dataMutex);

  DataPoint point;
  point.timestamp = std::chrono::steady_clock::now();
  point.values = values;
  point.rawData = rawData;
  point.parseSuccess = parseSuccess;

  m_dataHistory.push_back(point);

  if (m_dataHistory.size() > MAX_HISTORY) {
    m_dataHistory.pop_front();
  }
}

void SiPhOGClientDebugUI::UpdateStats(bool messageReceived) {
  std::lock_guard<std::mutex> lock(m_statsMutex);

  auto now = std::chrono::steady_clock::now();
  m_stats.totalMessages++;

  if (messageReceived) {
    m_stats.validMessages++;
    m_stats.lastMessageTime = now;
  }
  else {
    m_stats.errorMessages++;
  }

  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_stats.startTime);
  if (duration.count() > 1000) {
    m_stats.currentRate = (double)m_stats.validMessages / (duration.count() / 1000.0);
  }
}

SiPhOGClientDebugUI::ConnectionStats SiPhOGClientDebugUI::GetStats() const {
  std::lock_guard<std::mutex> lock(m_statsMutex);
  return m_stats;
}

std::vector<SiPhOGClientDebugUI::DataPoint> SiPhOGClientDebugUI::GetRecentData(size_t count) const {
  std::lock_guard<std::mutex> lock(m_dataMutex);

  std::vector<DataPoint> result;
  size_t start = m_dataHistory.size() > count ? m_dataHistory.size() - count : 0;

  for (size_t i = start; i < m_dataHistory.size(); ++i) {
    result.push_back(m_dataHistory[i]);
  }

  return result;
}

SiPhOGClientDebugUI::DataPoint SiPhOGClientDebugUI::GetLatestData() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);

  if (!m_dataHistory.empty()) {
    return m_dataHistory.back();
  }

  return DataPoint();
}

std::string SiPhOGClientDebugUI::GetConnectionStatus() const {
  auto stats = GetStats();

  if (!stats.isConnected) {
    return "❌ Disconnected";
  }
  else if (!stats.isCollecting) {
    return "🟡 Connected (Not Collecting)";
  }
  else {
    return "✅ Connected & Collecting";
  }
}

// IImguiUI interface implementation
void SiPhOGClientDebugUI::Render() {
  if (!m_isVisible) {
    return;
  }

  RenderDebugUI();
}

void SiPhOGClientDebugUI::Show() {
  m_isVisible = true;
}

void SiPhOGClientDebugUI::Hide() {
  m_isVisible = false;
}

bool SiPhOGClientDebugUI::IsVisible() const {
  return m_isVisible;
}

const std::string& SiPhOGClientDebugUI::GetName() const {
  return m_windowName;
}

// Legacy method for direct UI rendering (kept for compatibility)
void SiPhOGClientDebugUI::RenderDebugUI() {
  if (!m_isVisible) {
    return;
  }

  bool windowOpen = m_isVisible;
  if (ImGui::Begin(m_windowName.c_str(), &windowOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
    RenderConnectionPanel();
    ImGui::Separator();
    RenderStatsPanel();
    ImGui::Separator();
    RenderDataPanel();
    ImGui::Separator();
    RenderRawDataPanel();
  }
  ImGui::End();

  // Update visibility based on window close button
  if (!windowOpen) {
    m_isVisible = false;
  }
}

void SiPhOGClientDebugUI::RenderConnectionPanel() {
  ImGui::Text("🔌 Connection");

  // Connection status
  std::string status = GetConnectionStatus();
  ImGui::Text("Status: %s", status.c_str());

  // Connection controls
  ImGui::Text("Host:"); ImGui::SameLine();
  ImGui::SetNextItemWidth(150);
  ImGui::InputText("##host", m_hostBuffer, sizeof(m_hostBuffer));

  ImGui::SameLine();
  ImGui::Text("Port:"); ImGui::SameLine();
  ImGui::SetNextItemWidth(80);
  ImGui::InputInt("##port", &m_portBuffer);

  // Buttons
  if (!IsConnected()) {
    if (ImGui::Button("Connect")) {
      Connect(std::string(m_hostBuffer), m_portBuffer);
    }
  }
  else {
    if (ImGui::Button("Disconnect")) {
      StopDataCollection();
      Disconnect();
    }

    ImGui::SameLine();
    if (!IsCollecting()) {
      if (ImGui::Button("Start Collecting")) {
        StartDataCollection();
      }
    }
    else {
      if (ImGui::Button("Stop Collecting")) {
        StopDataCollection();
      }
    }
  }
}

void SiPhOGClientDebugUI::RenderStatsPanel() {
  ImGui::Text("📊 Statistics");

  auto stats = GetStats();

  ImGui::Text("Total Messages: %llu", (unsigned long long)stats.totalMessages);
  ImGui::SameLine(200);
  ImGui::Text("Valid: %llu", (unsigned long long)stats.validMessages);
  ImGui::SameLine(300);
  ImGui::Text("Errors: %llu", (unsigned long long)stats.errorMessages);

  ImGui::Text("Rate: %.1f Hz", stats.currentRate);

  if (stats.validMessages > 0) {
    auto successRate = (double)stats.validMessages / stats.totalMessages * 100.0;
    ImGui::Text("Success Rate: %.1f%%", successRate);
  }
}

void SiPhOGClientDebugUI::RenderDataPanel() {
  ImGui::Text("📈 Latest Data");

  auto latest = GetLatestData();

  if (latest.parseSuccess && !latest.values.empty()) {
    // Show timestamp
    auto now = std::chrono::steady_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - latest.timestamp);
    ImGui::Text("Last Update: %lld ms ago", (long long)diff.count());

    // Data table
    if (ImGui::BeginTable("DataTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Channel", ImGuiTableColumnFlags_WidthFixed, 200);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 120);
      ImGui::TableHeadersRow();

      for (const auto& [key, value] : latest.values) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%s", key.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%.3f", value);
      }

      ImGui::EndTable();
    }
  }
  else {
    ImGui::Text("No valid data received yet...");
  }
}

void SiPhOGClientDebugUI::RenderRawDataPanel() {
  ImGui::Text("🔍 Raw Data Monitor");

  ImGui::Checkbox("Show Raw Data", &m_showRawData);
  ImGui::SameLine();
  ImGui::Checkbox("Show Parse Errors", &m_showParseErrors);
  ImGui::SameLine();
  ImGui::Checkbox("Auto Scroll", &m_autoScroll);

  if (m_showRawData || m_showParseErrors) {
    ImGui::Separator();

    if (ImGui::BeginChild("RawDataScroll", ImVec2(0, 200), true)) {
      auto recentData = GetRecentData(50);

      for (const auto& point : recentData) {
        bool shouldShow = (m_showRawData && point.parseSuccess) ||
          (m_showParseErrors && !point.parseSuccess);

        if (shouldShow) {
          auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            point.timestamp.time_since_epoch());

          ImVec4 color = point.parseSuccess ?
            ImVec4(0.0f, 1.0f, 0.0f, 1.0f) :  // Green for success
            ImVec4(1.0f, 0.0f, 0.0f, 1.0f);   // Red for error

          ImGui::TextColored(color, "[%lld] %s %s",
            (long long)(duration.count() % 100000),
            point.parseSuccess ? "✅" : "❌",
            point.rawData.c_str());
        }
      }

      if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
      }
    }
    ImGui::EndChild();
  }
}