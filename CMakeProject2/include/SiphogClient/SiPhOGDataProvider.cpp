#include "SiPhOGDataProvider.h"
#include <iostream>
#include <sstream>
#include <iomanip>

SiPhOGDataProvider::SiPhOGDataProvider(const std::string& host, int port)
  : m_host(host)
  , m_port(port)
  , m_socket(std::make_unique<SocketWrapper>())
  , m_connected(false)
  , m_dataCollectionActive(false)
  , m_shouldStop(false)
  , m_debugMode(false)
{
  InitializeDataKeys();
}

SiPhOGDataProvider::~SiPhOGDataProvider() {
  StopDataCollection();
  Disconnect();
}

void SiPhOGDataProvider::InitializeDataKeys() {
  m_dataKeys = {
      "SLED_Current (mA)",
      "Photo Current (uA)",
      "SLED_Temp (C)",
      "Target SAG_PWR (V)",
      "SAG_PWR (V)",
      "TEC_Current (mA)"
  };
}

// IDataProvider interface implementation
std::string SiPhOGDataProvider::GetProviderName() const {
  return "SiPhOG";
}

bool SiPhOGDataProvider::StartDataCollection(int intervalMs) {
  if (m_dataCollectionActive) {
    DebugLog("Data collection already active");
    return true;
  }

  if (!m_connected) {
    DebugLog("Not connected - attempting to connect first");
    if (!Connect()) {
      DebugLog("Failed to connect to SiPhOG server");
      return false;
    }
  }

  m_shouldStop = false;
  m_dataCollectionActive = true;

  // Initialize stats
  {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats = Stats();
    m_stats.startTime = std::chrono::steady_clock::now();
  }

  // Start data collection thread
  m_dataThread = std::thread(&SiPhOGDataProvider::DataCollectionLoop, this);

  DebugLog("Data collection started");
  return true;
}

void SiPhOGDataProvider::StopDataCollection() {
  if (!m_dataCollectionActive) {
    return;
  }

  m_shouldStop = true;
  m_dataCollectionActive = false;

  if (m_dataThread.joinable()) {
    m_dataThread.join();
  }

  DebugLog("Data collection stopped");
}

bool SiPhOGDataProvider::IsDataCollectionActive() const {
  return m_dataCollectionActive;
}

void SiPhOGDataProvider::SetDataUpdateCallback(std::function<void(const std::string&, const std::string&)> callback) {
  std::lock_guard<std::mutex> lock(m_callbackMutex);
  m_dataCallback = callback;
  DebugLog("Data update callback set");
}

std::vector<std::string> SiPhOGDataProvider::GetDeviceNames() const {
  // SiPhOG represents one logical device
  return { "SiPhOG-Device" };
}

std::map<std::string, std::string> SiPhOGDataProvider::GetChannelSuffixes() const {
  return {
      {"SLED_Current", "SLED Current in mA"},
      {"Photo_Current", "Photo Current in uA"},
      {"SLED_Temp", "SLED Temperature in Celsius"},
      {"Target_SAG_PWR", "Target Sagnac Power in V"},
      {"SAG_PWR", "Actual Sagnac Power in V"},
      {"TEC_Current", "TEC Current in mA"}
  };
}

// SiPhOG-specific methods
bool SiPhOGDataProvider::Connect() {
  if (m_connected) {
    return true;
  }

  DebugLog("Connecting to " + m_host + ":" + std::to_string(m_port));

  if (!m_socket->Connect(m_host, m_port)) {
    DebugLog("Failed to connect: " + m_socket->GetLastError());
    return false;
  }

  // Set timeout for non-blocking receive
  m_socket->SetTimeout(100);
  m_connected = true;

  DebugLog("Connected successfully");
  return true;
}

void SiPhOGDataProvider::Disconnect() {
  if (!m_connected) {
    return;
  }

  m_connected = false;
  m_socket->Disconnect();
  DebugLog("Disconnected");
}

bool SiPhOGDataProvider::IsConnected() const {
  return m_connected;
}

SiPhOGDataProvider::Stats SiPhOGDataProvider::GetStats() const {
  std::lock_guard<std::mutex> lock(m_statsMutex);
  return m_stats;
}

void SiPhOGDataProvider::DataCollectionLoop() {
  std::string buffer;
  char recvBuffer[1024];

  DebugLog("Starting data collection loop");

  while (!m_shouldStop && m_connected) {
    try {
      int bytesReceived = m_socket->Receive(recvBuffer, sizeof(recvBuffer) - 1);

      if (bytesReceived > 0) {
        recvBuffer[bytesReceived] = '\0';
        buffer += std::string(recvBuffer);

        // Process complete CSV lines
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
              break; // Incomplete line, wait for more data
            }

            // Extract exactly 6 values
            std::vector<std::string> parts;
            std::stringstream ss(candidateLine);
            std::string item;

            while (std::getline(ss, item, ',') && parts.size() < 6) {
              parts.push_back(item);
            }

            if (parts.size() < 6) {
              break; // Still not enough, wait for more data
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
            // Validate comma count
            size_t commaCount = 0;
            for (char c : candidateLine) {
              if (c == ',') commaCount++;
            }

            if (commaCount == 5) { // Exactly 6 values
              bool success = ParseAndPublishData(candidateLine);
              UpdateStats(success);
            }
          }

          // Move to next line
          if (hasNewline) {
            lineStart = lineEnd + 1;
          }
          else {
            // Calculate how much to remove based on processed values
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
              break;
            }
          }
        }

        // Remove processed data from buffer
        if (lineStart > 0) {
          buffer.erase(0, lineStart);
        }

        // Prevent buffer from growing too large
        if (buffer.size() > 10000) {
          DebugLog("Buffer too large, clearing");
          buffer.clear();
        }

      }
      else if (bytesReceived == -1) {
        // Error or disconnect
        DebugLog("Socket error or disconnect");
        m_connected = false;
        break;
      }
      // bytesReceived == 0 means timeout, continue

      std::this_thread::sleep_for(std::chrono::milliseconds(1));

    }
    catch (const std::exception& e) {
      DebugLog("Exception in data loop: " + std::string(e.what()));
      break;
    }
  }

  DebugLog("Data collection loop ended");
  m_dataCollectionActive = false;
}

bool SiPhOGDataProvider::ParseAndPublishData(const std::string& csvLine) {
  try {
    std::vector<float> values;
    std::stringstream ss(csvLine);
    std::string item;

    // Parse CSV values
    while (std::getline(ss, item, ',') && values.size() < m_dataKeys.size()) {
      item.erase(0, item.find_first_not_of(" \t"));
      item.erase(item.find_last_not_of(" \t") + 1);

      if (!item.empty()) {
        float value = std::stof(item);
        values.push_back(value);
      }
    }

    if (values.size() == m_dataKeys.size()) {
      // Create value map
      std::map<std::string, float> valueMap;
      for (size_t i = 0; i < m_dataKeys.size(); ++i) {
        valueMap[m_dataKeys[i]] = values[i];
      }

      // Publish the data
      PublishDataUpdate(valueMap);

      return true;
    }

    return false;

  }
  catch (const std::exception& e) {
    DebugLog("Parse error: " + std::string(e.what()));
    return false;
  }
}

void SiPhOGDataProvider::PublishDataUpdate(const std::map<std::string, float>& values) {
  std::lock_guard<std::mutex> lock(m_callbackMutex);

  if (m_dataCallback) {
    // Format data as individual channel updates (similar to SPD format)
    // For each data point, create a separate status string that GlobalDataStore can parse

    // Create a status string that includes all values in a parseable format
    std::stringstream statusStr;
    statusStr << "Connected | Output: ON";

    // Add each channel as a separate parameter
    for (const auto& [key, value] : values) {
      // Convert key to shorter format for parsing
      std::string shortKey = key;
      if (key == "SLED_Current (mA)") shortKey = "SLED_Current";
      else if (key == "Photo Current (uA)") shortKey = "Photo_Current";
      else if (key == "SLED_Temp (C)") shortKey = "SLED_Temp";
      else if (key == "Target SAG_PWR (V)") shortKey = "Target_SAG_PWR";
      else if (key == "SAG_PWR (V)") shortKey = "SAG_PWR";
      else if (key == "TEC_Current (mA)") shortKey = "TEC_Current";

      statusStr << " | " << shortKey << ": " << std::fixed << std::setprecision(3) << value;
    }

    std::string statusString = statusStr.str();

    // DEBUG: Log the callback attempt
    if (m_debugMode) {
      static int callbackCounter = 0;
      callbackCounter++;
      if (callbackCounter % 50 == 1) {
        //DebugLog("*** CALLING CALLBACK *** Device: SiPhOG-Device, Status: " + statusString.substr(0, 80) + "...");
        //DebugLog("Callback function address: " + std::to_string(reinterpret_cast<uintptr_t>(&m_dataCallback)));
      }
    }

    // Call the callback with device name and status
    m_dataCallback("SiPhOG-Device", statusString);

    if (m_debugMode) {
      static int publishCounter = 0;
      if (++publishCounter % 100 == 1) {
        //DebugLog("Published data: " + statusString.substr(0, 100) + "...");
      }
    }
  }
  else {
    // DEBUG: Log when callback is null
    if (m_debugMode) {
      static int nullCallbackCounter = 0;
      if (++nullCallbackCounter % 100 == 1) {
        DebugLog("*** WARNING: Callback is NULL! Cannot publish data ***");
      }
    }
  }
}

void SiPhOGDataProvider::UpdateStats(bool messageReceived) {
  std::lock_guard<std::mutex> lock(m_statsMutex);

  auto now = std::chrono::steady_clock::now();
  m_stats.totalMessages++;

  if (messageReceived) {
    m_stats.validMessages++;
  }
  else {
    m_stats.errorMessages++;
  }

  // Calculate current rate
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_stats.startTime);
  if (duration.count() > 1000) {
    m_stats.currentRate = (double)m_stats.validMessages / (duration.count() / 1000.0);
  }
}

void SiPhOGDataProvider::DebugLog(const std::string& message) {
  if (m_debugMode) {
    auto now = std::chrono::steady_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::cout << "[DEBUG SiPhOGDataProvider "
      << std::put_time(std::localtime(&time_t), "%H:%M:%S")
      << "] " << message << std::endl;
  }
}