#include "IOControlPanel.h"
#include <iostream>

IOControlPanel::IOControlPanel(EziIOManager& manager)
  : m_ioManager(manager)
{
  // Initialize the pins to be controlled
  InitializePins();
  std::cout << "IOControlPanel initialized with " << m_outputPins.size() << " pins" << std::endl;
}

void IOControlPanel::InitializePins() {
  // Clear existing pins
  m_outputPins.clear();

  // Add the output pins you want to control (hardcoded)
  // Format: deviceName, deviceId, pinNumber, label

  // IOBottom pins (Example from your IOConfig.json)
  m_outputPins.push_back({ "IOBottom", 0, 0, "L_Gripper" });
  m_outputPins.push_back({ "IOBottom", 0, 2, "R_Gripper" });
  m_outputPins.push_back({ "IOBottom", 0, 10, "Vacuum_Base" });
  m_outputPins.push_back({ "IOBottom", 0, 15, "Dispenser_Shot" });
  m_outputPins.push_back({ "IOBottom", 0, 4, "UV_Head" });
  m_outputPins.push_back({ "IOBottom", 0, 5, "Dispenser_Head" });
  m_outputPins.push_back({ "IOBottom", 0, 14, "UV_PLC1" });
  m_outputPins.push_back({ "IOBottom", 0, 13, "UV_PLC1" });

  // You can add pins from IOTop if needed
  // m_outputPins.push_back({"IOTop", 1, 0, "Output0"});
}

void IOControlPanel::RenderUI() {
  if (!m_showWindow) return;

  // Create and configure window
  ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("IO Control Panel", &m_showWindow, ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    return;
  }

  // Title
  ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "Output Pin Controls");
  ImGui::Separator();

  // Create a table for the buttons
  if (ImGui::BeginTable("OutputPinTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    // Setup columns
    ImGui::TableSetupColumn("Pin", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    // For each pin in our list
    for (const auto& pin : m_outputPins) {
      ImGui::TableNextRow();

      // Column 1: Pin number
      ImGui::TableNextColumn();
      ImGui::Text("%d", pin.pinNumber);

      // Column 2: Pin label
      ImGui::TableNextColumn();
      ImGui::Text("%s", pin.label.c_str());

      // Column 3: Control buttons
      ImGui::TableNextColumn();

      // Read current state
      uint32_t outputs = 0, status = 0;
      bool success = m_ioManager.getLastOutputStatus(pin.deviceId, outputs, status);

      // Get the correct mask for this pin
      uint32_t mask = 0;

      // For IOBottom (deviceId 0), pins use different mask calculation
      if (pin.deviceId == 0) {
        mask = 0x10000 << pin.pinNumber; // Specific to IOBottom
      }
      else if (pin.deviceId == 1) {
        mask = 0x100 << pin.pinNumber; // Specific to IOTop
      }

      // Determine current state
      bool isOn = false;
      if (success) {
        isOn = (outputs & mask) != 0;
      }

      // Generate unique ID for buttons
      std::string id = "##" + pin.deviceName + "_" + std::to_string(pin.pinNumber);

      // Toggle button (changes color based on state)
      ImGui::PushStyleColor(ImGuiCol_Button,
        isOn ? ImVec4(0.0f, 0.8f, 0.0f, 0.8f) : ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        isOn ? ImVec4(0.0f, 1.0f, 0.0f, 0.9f) : ImVec4(0.7f, 0.7f, 0.7f, 0.9f));

      // Create button with pin state indicator
      std::string buttonLabel = (isOn ? "ON" : "OFF") + id;
      if (ImGui::Button(buttonLabel.c_str(), ImVec2(-FLT_MIN, 24))) {
        // Toggle the state
        m_ioManager.setOutput(pin.deviceId, pin.pinNumber, !isOn);
        std::cout << "Toggled " << pin.deviceName << " pin " << pin.pinNumber
          << " (" << pin.label << ") to " << (!isOn ? "ON" : "OFF") << std::endl;
      }

      ImGui::PopStyleColor(2);
    }

    ImGui::EndTable();
  }

  // Force refresh button at the bottom
  if (ImGui::Button("Refresh Status", ImVec2(-FLT_MIN, 30))) {
    // Force refresh from hardware
    for (const auto& pin : m_outputPins) {
      uint32_t outputs = 0, status = 0;
      m_ioManager.getOutputs(pin.deviceId, outputs, status);
    }
  }

  ImGui::End();
}