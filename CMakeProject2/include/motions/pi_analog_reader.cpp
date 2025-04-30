// pi_analog_reader.cpp
#include "../include/motions/pi_analog_reader.h"
#include "imgui.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>

PIAnalogReader::PIAnalogReader(PIController& controller, const std::string& deviceName)
  : m_controller(controller), m_deviceName(deviceName) {

  m_logger = Logger::GetInstance();
  m_logger->LogInfo("PIAnalogReader: Initializing analog reader for " + m_deviceName);

  // Set the window title based on the device name
  m_windowTitle = "Analog Monitor: " + m_deviceName;

  // Try to get the number of channels initially
  int numChannels = 0;
  if (GetNumberOfChannels(numChannels)) {
    m_numChannels = numChannels;
    m_logger->LogInfo("PIAnalogReader: Found " + std::to_string(m_numChannels) + " analog channels for " + m_deviceName);
  }
  else {
    m_logger->LogWarning("PIAnalogReader: Could not determine the number of analog channels for " + m_deviceName);
  }
}

PIAnalogReader::~PIAnalogReader() {
  m_logger->LogInfo("PIAnalogReader: Shutting down analog reader for " + m_deviceName);
}

bool PIAnalogReader::IsControllerValid() const {
  if (!m_controller.IsConnected()) {
    // Don't log every time - this would flood the logs
    return false;
  }

  return true;
}

bool PIAnalogReader::GetNumberOfChannels(int& numChannels) {
  if (!IsControllerValid()) {
    return false;
  }

  // Get the controller ID
  int controllerId = m_controller.GetControllerId();

  if (controllerId < 0) {
    m_logger->LogError("PIAnalogReader: Invalid controller ID for " + m_deviceName);
    return false;
  }

  // Call the PI API function
  if (!PI_qTAC(controllerId, &numChannels)) {
    int error = PI_GetError(controllerId);
    m_logger->LogError("PIAnalogReader: Failed to get number of analog channels for " +
      m_deviceName + ". Error code: " + std::to_string(error));
    return false;
  }

  return true;
}

bool PIAnalogReader::GetRawValues(std::map<int, int>& rawValues) {
  if (!IsControllerValid() || m_numChannels <= 0) {
    return false;
  }

  int controllerId = m_controller.GetControllerId();

  if (controllerId < 0) {
    return false;
  }

  // Prepare arrays for the API call
  std::vector<int> channelIds(m_numChannels);
  std::vector<int> values(m_numChannels);

  // Fill the channel IDs (1-based indexing is common in PI controllers)
  for (int i = 0; i < m_numChannels; i++) {
    channelIds[i] = i + 1;
  }

  // Call the API function
  if (!PI_qTAD(controllerId, channelIds.data(), values.data(), m_numChannels)) {
    int error = PI_GetError(controllerId);
    m_logger->LogError("PIAnalogReader: Failed to get raw ADC values for " +
      m_deviceName + ". Error code: " + std::to_string(error));
    return false;
  }

  // Convert to map
  rawValues.clear();
  for (int i = 0; i < m_numChannels; i++) {
    rawValues[channelIds[i]] = values[i];
  }

  return true;
}

bool PIAnalogReader::GetRawValue(int channel, int& value) {
  if (!IsControllerValid()) {
    return false;
  }

  int controllerId = m_controller.GetControllerId();

  if (controllerId < 0) {
    return false;
  }

  // Call the API function for a single channel
  int channelId = channel;
  if (!PI_qTAD(controllerId, &channelId, &value, 1)) {
    int error = PI_GetError(controllerId);
    m_logger->LogError("PIAnalogReader: Failed to get raw ADC value for channel " +
      std::to_string(channel) + " on " + m_deviceName +
      ". Error code: " + std::to_string(error));
    return false;
  }

  return true;
}

bool PIAnalogReader::GetVoltageValues(std::map<int, double>& voltageValues) {
  if (!IsControllerValid() || m_numChannels <= 0) {
    return false;
  }

  int controllerId = m_controller.GetControllerId();

  if (controllerId < 0) {
    return false;
  }

  // Prepare arrays for the API call
  std::vector<int> channelIds(m_numChannels);
  std::vector<double> values(m_numChannels);

  // Fill the channel IDs (1-based indexing is common in PI controllers)
  for (int i = 0; i < m_numChannels; i++) {
    channelIds[i] = i + 1;
  }

  // Call the API function
  if (!PI_qTAV(controllerId, channelIds.data(), values.data(), m_numChannels)) {
    int error = PI_GetError(controllerId);
    m_logger->LogError("PIAnalogReader: Failed to get voltage values for " +
      m_deviceName + ". Error code: " + std::to_string(error));
    return false;
  }

  // Convert to map
  voltageValues.clear();
  for (int i = 0; i < m_numChannels; i++) {
    voltageValues[channelIds[i]] = values[i];
  }

  return true;
}

bool PIAnalogReader::GetVoltageValue(int channel, double& voltage) {
  if (!IsControllerValid()) {
    return false;
  }

  int controllerId = m_controller.GetControllerId();

  if (controllerId < 0) {
    return false;
  }

  // Call the API function for a single channel
  int channelId = channel;
  if (!PI_qTAV(controllerId, &channelId, &voltage, 1)) {
    int error = PI_GetError(controllerId);
    m_logger->LogError("PIAnalogReader: Failed to get voltage value for channel " +
      std::to_string(channel) + " on " + m_deviceName +
      ". Error code: " + std::to_string(error));
    return false;
  }

  return true;
}

bool PIAnalogReader::UpdateAllValues() {
  if (!IsControllerValid()) {
    return false;
  }

  // Update the number of channels if not already known
  if (m_numChannels <= 0) {
    int numChannels = 0;
    if (GetNumberOfChannels(numChannels)) {
      m_numChannels = numChannels;
    }
    else {
      return false;
    }
  }

  // Update raw ADC values
  if (!GetRawValues(m_rawValues)) {
    return false;
  }

  // Update voltage values
  if (!GetVoltageValues(m_voltageValues)) {
    return false;
  }

  return true;
}

void PIAnalogReader::RenderUI() {
  if (!m_showWindow) {
    return;
  }

  // Initialize the window
  if (!ImGui::Begin(m_windowTitle.c_str(), &m_showWindow)) {
    ImGui::End();
    return;
  }

  // Connection status
  bool isConnected = m_controller.IsConnected();
  ImGui::Text("Controller Status: %s", isConnected ? "Connected" : "Disconnected");

  if (!isConnected) {
    ImGui::Text("Connect the controller to read analog values");
    ImGui::End();
    return;
  }

  // Auto-refresh control
  ImGui::Checkbox("Auto Refresh", &m_autoRefresh);
  ImGui::SameLine();
  ImGui::SliderFloat("Refresh Interval (s)", &m_refreshInterval, 0.1f, 5.0f);

  // Manual refresh button
  if (ImGui::Button("Refresh Now") ||
    (m_autoRefresh && ImGui::GetTime() > m_lastRefreshTime + m_refreshInterval)) {
    UpdateAllValues();
    m_lastRefreshTime = ImGui::GetTime();
  }

  ImGui::Separator();

  // Display channel information
  if (m_numChannels <= 0) {
    ImGui::Text("No analog channels detected");
  }
  else {
    ImGui::Text("Analog Channels: %d", m_numChannels);

    // Create a table to display the values
    if (ImGui::BeginTable("AnalogChannelsTable", 3, ImGuiTableFlags_Borders)) {
      ImGui::TableSetupColumn("Channel");
      ImGui::TableSetupColumn("ADC Value");
      ImGui::TableSetupColumn("Voltage (V)");
      ImGui::TableHeadersRow();

      for (const auto& [channel, rawValue] : m_rawValues) {
        ImGui::TableNextRow();

        // Column 1: Channel number
        ImGui::TableNextColumn();
        ImGui::Text("%d", channel);

        // Column 2: Raw ADC value
        ImGui::TableNextColumn();
        ImGui::Text("%d", rawValue);

        // Column 3: Voltage value
        ImGui::TableNextColumn();
        auto voltIt = m_voltageValues.find(channel);
        if (voltIt != m_voltageValues.end()) {
          ImGui::Text("%.4f V", voltIt->second);
        }
        else {
          ImGui::Text("N/A");
        }
      }

      ImGui::EndTable();
    }

    // Add a simple bar graph for the voltage values if we have them
    if (!m_voltageValues.empty()) {
      ImGui::Separator();
      ImGui::Text("Voltage Readings");

      // Get min/max values for scaling
      auto [minIt, maxIt] = std::minmax_element(
        m_voltageValues.begin(), m_voltageValues.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; }
      );

      double minVoltage = minIt->second;
      double maxVoltage = maxIt->second;

      // Add some padding to the range
      double range = maxVoltage - minVoltage;
      if (range < 0.1) range = 0.1; // Prevent division by zero or very small ranges

      minVoltage -= range * 0.1;
      maxVoltage += range * 0.1;

      // Plot bar graph
      float barWidth = ImGui::GetContentRegionAvail().x / m_voltageValues.size();
      float barHeightMax = 100.0f; // Maximum height of bars in pixels

      ImDrawList* drawList = ImGui::GetWindowDrawList();
      ImVec2 cursor = ImGui::GetCursorScreenPos();

      // Draw baseline and Y-axis labels
      ImVec2 baselineStart(cursor.x, cursor.y + barHeightMax);
      ImVec2 baselineEnd(cursor.x + barWidth * m_voltageValues.size(), cursor.y + barHeightMax);
      drawList->AddLine(baselineStart, baselineEnd, ImGui::GetColorU32(ImGuiCol_Text));

      // Draw voltage bars and labels
      int index = 0;
      for (const auto& [channel, voltage] : m_voltageValues) {
        // Calculate bar height proportional to voltage
        float barHeight = barHeightMax * static_cast<float>((voltage - minVoltage) / (maxVoltage - minVoltage));

        // Ensure minimum visible height for non-zero values
        if (voltage > 0 && barHeight < 2.0f) barHeight = 2.0f;

        // Bar position
        ImVec2 barBottomLeft(cursor.x + index * barWidth + 2.0f, baselineStart.y);
        ImVec2 barTopRight(cursor.x + (index + 1) * barWidth - 2.0f, baselineStart.y - barHeight);

        // Draw the bar
        ImU32 barColor = ImGui::GetColorU32(ImVec4(0.2f, 0.6f, 1.0f, 0.8f));
        drawList->AddRectFilled(barBottomLeft, barTopRight, barColor);

        // Draw channel label under the bar
        std::string channelLabel = std::to_string(channel);
        drawList->AddText(
          ImVec2(barBottomLeft.x + (barWidth - ImGui::CalcTextSize(channelLabel.c_str()).x) * 0.5f,
            barBottomLeft.y + 2),
          ImGui::GetColorU32(ImGuiCol_Text),
          channelLabel.c_str()
        );

        // Draw voltage value above the bar
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << voltage << "V";
        std::string voltText = ss.str();

        drawList->AddText(
          ImVec2(barBottomLeft.x + (barWidth - ImGui::CalcTextSize(voltText.c_str()).x) * 0.5f,
            barTopRight.y - 15),
          ImGui::GetColorU32(ImGuiCol_Text),
          voltText.c_str()
        );

        index++;
      }

      // Add space for the graph
      ImGui::Dummy(ImVec2(0, barHeightMax + 20));
    }
  }

  ImGui::End();
}