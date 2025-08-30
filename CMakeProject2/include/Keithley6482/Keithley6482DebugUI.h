#pragma once

#include "imgui.h"
#include "Keithley6482Manager.h"
#include "IK6482MeasurementSubscriber.h"
#include <memory>
#include <mutex>
#include <map>
#include <deque>
#include <chrono>
#include <iomanip>
#include <sstream>

/**
 * @brief Debug UI window for Keithley 6482 monitoring and control
 *
 * Provides real-time display of measurements, device status, and control buttons
 */
class Keithley6482DebugUI : public Keithley::IK6482MeasurementSubscriber {
private:
  // Manager reference
  Keithley::Keithley6482Manager* m_manager;

  // UI state
  bool m_windowOpen = true;
  bool m_autoScroll = true;
  int m_pollingInterval = 100;

  // Measurement data storage
  struct ChannelData {
    double currentValue = 0.0;
    double voltageValue = 0.0;
    std::deque<double> currentHistory;
    std::deque<double> voltageHistory;
    double minCurrent = 1e12;
    double maxCurrent = -1e12;
    double avgCurrent = 0.0;
    size_t sampleCount = 0;
    std::chrono::steady_clock::time_point lastUpdate;
  };

  struct DeviceData {
    bool isConnected = false;
    std::map<int, ChannelData> channels;
    std::chrono::steady_clock::time_point lastStatusUpdate;
  };

  mutable std::mutex m_dataMutex;
  std::map<std::string, DeviceData> m_deviceData;

  // Display settings
  int m_historySize = 100;
  float m_displayScale = 1e12;  // Convert to pA by default
  std::string m_displayUnit = "pA";

  // Log messages
  struct LogMessage {
    std::chrono::steady_clock::time_point timestamp;
    std::string message;
    ImVec4 color;
  };
  std::deque<LogMessage> m_logMessages;
  size_t m_maxLogMessages = 100;

public:
  explicit Keithley6482DebugUI(Keithley::Keithley6482Manager* manager)
    : m_manager(manager) {
    // Subscribe to manager
    if (m_manager) {
      m_manager->Subscribe("DebugUI",
        std::shared_ptr<IK6482MeasurementSubscriber>(this, [](auto*) {}));
    }
  }

  ~Keithley6482DebugUI() {
    // Unsubscribe from manager
    if (m_manager) {
      m_manager->Unsubscribe("DebugUI");
    }
  }

  /**
   * @brief Render the debug UI window
   */
  void Render() {
    if (!m_windowOpen) return;

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Keithley 6482 Debug Monitor", &m_windowOpen)) {
      RenderHeader();
      ImGui::Separator();

      if (ImGui::BeginTabBar("K6482Tabs")) {
        if (ImGui::BeginTabItem("Live Measurements")) {
          RenderMeasurements();
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Device Control")) {
          RenderControl();
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Statistics")) {
          RenderStatistics();
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Log")) {
          RenderLog();
          ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
      }
    }
    ImGui::End();
  }

  // IK6482MeasurementSubscriber interface implementation
  void OnMeasurementUpdate(const Keithley::K6482MeasurementData& data) override {
    std::lock_guard<std::mutex> lock(m_dataMutex);

    auto& device = m_deviceData[data.deviceName];
    auto& channel = device.channels[data.channel];

    // Update current values
    channel.currentValue = data.current;
    channel.voltageValue = data.voltage;
    channel.lastUpdate = data.timestamp;

    // Update history
    channel.currentHistory.push_back(data.current);
    if (channel.currentHistory.size() > m_historySize) {
      channel.currentHistory.pop_front();
    }

    // Update statistics
    channel.sampleCount++;
    channel.minCurrent = std::min(channel.minCurrent, data.current);
    channel.maxCurrent = std::max(channel.maxCurrent, data.current);

    // Update running average
    if (channel.sampleCount == 1) {
      channel.avgCurrent = data.current;
    }
    else {
      channel.avgCurrent = (channel.avgCurrent * (channel.sampleCount - 1) + data.current)
        / channel.sampleCount;
    }
  }

  void OnDeviceStatusUpdate(const Keithley::K6482DeviceStatus& status) override {
    std::lock_guard<std::mutex> lock(m_dataMutex);

    auto& device = m_deviceData[status.deviceName];
    device.isConnected = status.isConnected;
    device.lastStatusUpdate = status.timestamp;
  }

  void OnDeviceConnectionChange(const std::string& deviceName, bool connected) override {
    std::lock_guard<std::mutex> lock(m_dataMutex);

    m_deviceData[deviceName].isConnected = connected;

    LogMessage msg;
    msg.timestamp = std::chrono::steady_clock::now();
    msg.message = deviceName + (connected ? " connected" : " disconnected");
    msg.color = connected ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);

    m_logMessages.push_back(msg);
    if (m_logMessages.size() > m_maxLogMessages) {
      m_logMessages.pop_front();
    }
  }

  void OnPollingStarted(const std::string& deviceName, int intervalMs) override {
    LogMessage msg;
    msg.timestamp = std::chrono::steady_clock::now();
    msg.message = "Polling started for " + deviceName + " at " + std::to_string(intervalMs) + "ms";
    msg.color = ImVec4(0.0f, 1.0f, 1.0f, 1.0f);

    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_logMessages.push_back(msg);
    if (m_logMessages.size() > m_maxLogMessages) {
      m_logMessages.pop_front();
    }
  }

  void OnPollingStopped(const std::string& deviceName) override {
    LogMessage msg;
    msg.timestamp = std::chrono::steady_clock::now();
    msg.message = "Polling stopped for " + deviceName;
    msg.color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);

    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_logMessages.push_back(msg);
    if (m_logMessages.size() > m_maxLogMessages) {
      m_logMessages.pop_front();
    }
  }

private:
  void RenderHeader() {
    if (!m_manager) {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Manager not available");
      return;
    }

    // Status line
    ImGui::Text("Devices: %zu | Connected: %d | Polling: %s",
      m_manager->GetDeviceCount(),
      m_manager->GetConnectedCount(),
      m_manager->IsPollingActive() ? "ACTIVE" : "STOPPED");

    ImGui::SameLine(ImGui::GetWindowWidth() - 200);

    // Polling indicator
    if (m_manager->IsPollingActive()) {
      ImGui::TextColored(ImVec4(0, 1, 0, 1), "● POLLING");
    }
    else {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "● STOPPED");
    }
  }

  void RenderMeasurements() {
    std::lock_guard<std::mutex> lock(m_dataMutex);

    // Unit selector
    ImGui::Text("Display Unit:");
    ImGui::SameLine();
    if (ImGui::RadioButton("pA", m_displayUnit == "pA")) {
      m_displayUnit = "pA";
      m_displayScale = 1e12;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("nA", m_displayUnit == "nA")) {
      m_displayUnit = "nA";
      m_displayScale = 1e9;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("uA", m_displayUnit == "uA")) {
      m_displayUnit = "uA";
      m_displayScale = 1e6;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("mA", m_displayUnit == "mA")) {
      m_displayUnit = "mA";
      m_displayScale = 1e3;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("A", m_displayUnit == "A")) {
      m_displayUnit = "A";
      m_displayScale = 1.0;
    }

    ImGui::Separator();

    // Display each device
    for (const auto& [deviceName, deviceData] : m_deviceData) {
      if (ImGui::TreeNode(deviceName.c_str())) {
        // Connection status
        if (deviceData.isConnected) {
          ImGui::TextColored(ImVec4(0, 1, 0, 1), "Connected");
        }
        else {
          ImGui::TextColored(ImVec4(1, 0, 0, 1), "Disconnected");
        }

        // Channels table
        if (ImGui::BeginTable("Channels", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
          ImGui::TableSetupColumn("Channel");
          ImGui::TableSetupColumn("Current");
          ImGui::TableSetupColumn("Voltage");
          ImGui::TableSetupColumn("Graph");
          ImGui::TableHeadersRow();

          for (const auto& [channelNum, channelData] : deviceData.channels) {
            ImGui::TableNextRow();

            // Channel number
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Ch%d", channelNum);

            // Current value
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.3f %s", channelData.currentValue * m_displayScale, m_displayUnit.c_str());

            // Voltage value
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.3f V", channelData.voltageValue);

            // Mini graph
            ImGui::TableSetColumnIndex(3);
            if (!channelData.currentHistory.empty()) {
              std::vector<float> values;
              for (double val : channelData.currentHistory) {
                values.push_back(static_cast<float>(val * m_displayScale));
              }
              ImGui::PlotLines("", values.data(), static_cast<int>(values.size()),
                0, nullptr, FLT_MAX, FLT_MAX, ImVec2(150, 30));
            }
          }

          ImGui::EndTable();
        }

        ImGui::TreePop();
      }
    }
  }

  void RenderControl() {
    if (!m_manager) return;

    // Connection controls
    ImGui::Text("Connection Control:");

    if (ImGui::Button("Connect All", ImVec2(120, 0))) {
      m_manager->ConnectAll();
    }
    ImGui::SameLine();
    if (ImGui::Button("Disconnect All", ImVec2(120, 0))) {
      m_manager->DisconnectAll();
    }
    ImGui::SameLine();
    if (ImGui::Button("Discover Devices", ImVec2(120, 0))) {
      m_manager->DiscoverDevices(true);
    }

    ImGui::Separator();

    // Polling controls
    ImGui::Text("Polling Control:");

    bool isPolling = m_manager->IsPollingActive();

    if (!isPolling) {
      ImGui::InputInt("Polling Interval (ms)", &m_pollingInterval);
      if (m_pollingInterval < 10) m_pollingInterval = 10;
      if (m_pollingInterval > 10000) m_pollingInterval = 10000;

      if (ImGui::Button("Start Polling", ImVec2(120, 0))) {
        m_manager->StartAllPolling(m_pollingInterval);
      }
    }
    else {
      if (ImGui::Button("Stop Polling", ImVec2(120, 0))) {
        m_manager->StopAllPolling();
      }
    }

    ImGui::Separator();

    // Device controls
    ImGui::Text("Device Operations:");

    if (ImGui::Button("Reset All", ImVec2(120, 0))) {
      m_manager->ResetAll();
    }
    ImGui::SameLine();
    if (ImGui::Button("Auto Range All", ImVec2(120, 0))) {
      m_manager->SetAllAutoRange(1, true);
      m_manager->SetAllAutoRange(2, true);
    }

    // Range settings
    static int rangeSelection = 0;
    const char* ranges[] = { "Auto", "2nA", "20nA", "200nA", "2uA", "20uA", "200uA", "2mA", "20mA" };

    ImGui::Text("Set Range:");
    ImGui::Combo("Range", &rangeSelection, ranges, IM_ARRAYSIZE(ranges));

    if (ImGui::Button("Apply to Ch1", ImVec2(120, 0))) {
      if (rangeSelection == 0) {
        m_manager->SetAllAutoRange(1, true);
      }
      else {
        double rangeValues[] = { 0, 2e-9, 20e-9, 200e-9, 2e-6, 20e-6, 200e-6, 2e-3, 20e-3 };
        m_manager->SetAllCurrentRanges(1, rangeValues[rangeSelection]);
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply to Ch2", ImVec2(120, 0))) {
      if (rangeSelection == 0) {
        m_manager->SetAllAutoRange(2, true);
      }
      else {
        double rangeValues[] = { 0, 2e-9, 20e-9, 200e-9, 2e-6, 20e-6, 200e-6, 2e-3, 20e-3 };
        m_manager->SetAllCurrentRanges(2, rangeValues[rangeSelection]);
      }
    }
  }

  void RenderStatistics() {
    std::lock_guard<std::mutex> lock(m_dataMutex);

    for (const auto& [deviceName, deviceData] : m_deviceData) {
      if (ImGui::TreeNode(deviceName.c_str())) {
        if (ImGui::BeginTable("Stats", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
          ImGui::TableSetupColumn("Channel");
          ImGui::TableSetupColumn("Samples");
          ImGui::TableSetupColumn("Average");
          ImGui::TableSetupColumn("Min");
          ImGui::TableSetupColumn("Max");
          ImGui::TableHeadersRow();

          for (const auto& [channelNum, channelData] : deviceData.channels) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Ch%d", channelNum);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%zu", channelData.sampleCount);

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.3f %s", channelData.avgCurrent * m_displayScale, m_displayUnit.c_str());

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.3f %s", channelData.minCurrent * m_displayScale, m_displayUnit.c_str());

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.3f %s", channelData.maxCurrent * m_displayScale, m_displayUnit.c_str());
          }

          ImGui::EndTable();
        }

        if (ImGui::Button(("Reset Stats##" + deviceName).c_str())) {
          for (auto& [channelNum, channelData] : m_deviceData[deviceName].channels) {
            channelData.sampleCount = 0;
            channelData.minCurrent = 1e12;
            channelData.maxCurrent = -1e12;
            channelData.avgCurrent = 0.0;
            channelData.currentHistory.clear();
          }
        }

        ImGui::TreePop();
      }
    }
  }

  void RenderLog() {
    ImGui::Checkbox("Auto-scroll", &m_autoScroll);
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
      std::lock_guard<std::mutex> lock(m_dataMutex);
      m_logMessages.clear();
    }

    ImGui::Separator();

    ImGui::BeginChild("LogScrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    {
      std::lock_guard<std::mutex> lock(m_dataMutex);
      for (const auto& msg : m_logMessages) {
        // Format timestamp - simpler approach
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - msg.timestamp).count();

        std::stringstream ss;
        if (elapsed < 60) {
          ss << elapsed << "s ago";
        }
        else if (elapsed < 3600) {
          ss << (elapsed / 60) << "m ago";
        }
        else {
          ss << (elapsed / 3600) << "h ago";
        }

        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "[%s]", ss.str().c_str());
        ImGui::SameLine();
        ImGui::TextColored(msg.color, "%s", msg.message.c_str());
      }
    }

    if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
      ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
  }
};