// pi_analog_reader.cpp - Simplified implementation
#include "../include/motions/pi_analog_reader.h"
#include "imgui.h"
#include <iostream>
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
    m_logger->LogInfo("PIAnalogReader: Found " + std::to_string(m_numChannels) +
      " analog channels for " + m_deviceName);
  }
  else {
    m_logger->LogWarning("PIAnalogReader: Could not determine the number of analog channels for " +
      m_deviceName);
  }
}

PIAnalogReader::~PIAnalogReader() {
  try {
    // Only clear the containers, don't try to delete the controller
    m_voltageValues.clear();

    // Log the shutdown
    m_logger->LogInfo("PIAnalogReader: Reader for device " + m_deviceName + " destroyed");
  }
  catch (...) {
    // Swallow exceptions in destructor
  }
}

bool PIAnalogReader::IsControllerValid() const {
  return m_controller.IsConnected();
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

bool PIAnalogReader::GetVoltageValues(std::map<int, double>& voltageValues) {
  if (!IsControllerValid() || m_numChannels <= 0) {
    return false;
  }

  int controllerId = m_controller.GetControllerId();

  if (controllerId < 0) {
    return false;
  }

  // Prepare arrays for the API call - Only read channels 5 and 6 by default
  const int NUM_CHANNELS_TO_READ = 2;
  int channelIds[NUM_CHANNELS_TO_READ] = { 5, 6 };
  double values[NUM_CHANNELS_TO_READ] = { 0.0 };

  // Call the API function
  if (!PI_qTAV(controllerId, channelIds, values, NUM_CHANNELS_TO_READ)) {
    int error = PI_GetError(controllerId);
    m_logger->LogError("PIAnalogReader: Failed to get voltage values for " +
      m_deviceName + ". Error code: " + std::to_string(error));
    return false;
  }

  // Convert to map
  voltageValues.clear();
  for (int i = 0; i < NUM_CHANNELS_TO_READ; i++) {
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

  // Update voltage values (only channels 5 and 6)
  return GetVoltageValues(m_voltageValues);
}

void PIAnalogReader::RenderUI() {
  if (!m_showWindow) {
    return;
  }

  // Initialize the window
  if (!ImGui::Begin(m_windowTitle.c_str(), &m_showWindow, ImGuiWindowFlags_AlwaysAutoResize)) {
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

  // Manual refresh button
  if (ImGui::Button("Refresh Now")) {
    UpdateAllValues();
  }

  ImGui::Separator();

  // Display voltage readings in a simple table
  if (ImGui::BeginTable("AnalogReadingsTable", 2, ImGuiTableFlags_Borders)) {
    ImGui::TableSetupColumn("Channel");
    ImGui::TableSetupColumn("Voltage (V)");
    ImGui::TableHeadersRow();

    for (const auto& [channel, voltage] : m_voltageValues) {
      ImGui::TableNextRow();

      // Channel number
      ImGui::TableNextColumn();
      ImGui::Text("%d", channel);

      // Voltage value
      ImGui::TableNextColumn();
      ImGui::Text("%.4f V", voltage);
    }

    ImGui::EndTable();
  }

  ImGui::End();
}