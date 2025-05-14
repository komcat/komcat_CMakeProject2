#include "IOControlPanel.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

// Define the default config file name
const std::string IOControlPanel::DEFAULT_CONFIG_FILE = "io_panel_config.json";

IOControlPanel::IOControlPanel(EziIOManager& manager)
  : m_ioManager(manager)
{
  // Try to load configuration from the default file
  if (!LoadConfiguration(DEFAULT_CONFIG_FILE)) {
    std::cerr << "Failed to load IO Panel configuration, using hardcoded defaults" << std::endl;
    // Fall back to hardcoded initialization
    InitializePins();
  }
  std::cout << "IOControlPanel initialized with " << m_outputPins.size() << " pins" << std::endl;
}

bool IOControlPanel::LoadConfiguration(const std::string& filename) {
  try {
    std::ifstream file(filename);
    if (!file.is_open()) {
      std::cerr << "Could not open configuration file: " << filename << std::endl;
      return false;
    }

    nlohmann::json config;
    file >> config;

    // Clear existing pins
    m_outputPins.clear();

    // Check if we have panels in the configuration
    if (!config.contains("panels") || !config["panels"].is_array()) {
      std::cerr << "Invalid configuration format: missing 'panels' array" << std::endl;
      return false;
    }

    // For now, we'll just load the first panel
    // In the future, you could support multiple panels or panel selection
    const auto& panels = config["panels"];
    if (panels.empty()) {
      std::cerr << "No panels defined in configuration" << std::endl;
      return false;
    }

    const auto& panel = panels[0];

    // Load panel name if available
    if (panel.contains("name") && panel["name"].is_string()) {
      m_name = panel["name"].get<std::string>();
    }

    // Load pins
    if (!panel.contains("pins") || !panel["pins"].is_array()) {
      std::cerr << "No pins defined in panel" << std::endl;
      return false;
    }

    for (const auto& pinData : panel["pins"]) {
      PinConfig pin;

      // Validate and load pin configuration
      if (pinData.contains("deviceName") && pinData["deviceName"].is_string()) {
        pin.deviceName = pinData["deviceName"].get<std::string>();
      }
      else {
        std::cerr << "Missing or invalid deviceName in pin configuration" << std::endl;
        continue;
      }

      if (pinData.contains("deviceId") && pinData["deviceId"].is_number_integer()) {
        pin.deviceId = pinData["deviceId"].get<int>();
      }
      else {
        std::cerr << "Missing or invalid deviceId in pin configuration" << std::endl;
        continue;
      }

      if (pinData.contains("pinNumber") && pinData["pinNumber"].is_number_integer()) {
        pin.pinNumber = pinData["pinNumber"].get<int>();
      }
      else {
        std::cerr << "Missing or invalid pinNumber in pin configuration" << std::endl;
        continue;
      }

      if (pinData.contains("label") && pinData["label"].is_string()) {
        pin.label = pinData["label"].get<std::string>();
      }
      else {
        std::cerr << "Missing or invalid label in pin configuration" << std::endl;
        continue;
      }

      // Add the pin to our list
      m_outputPins.push_back(pin);
    }

    std::cout << "Successfully loaded " << m_outputPins.size() << " pins from " << filename << std::endl;
    return true;

  }
  catch (const nlohmann::json::exception& e) {
    std::cerr << "JSON parsing error: " << e.what() << std::endl;
    return false;
  }
  catch (const std::exception& e) {
    std::cerr << "Error loading configuration: " << e.what() << std::endl;
    return false;
  }
}

void IOControlPanel::InitializePins() {
  // Clear existing pins
  m_outputPins.clear();

  // Add the output pins you want to control (hardcoded) - use this as fallback
  // Format: deviceName, deviceId, pinNumber, label

  // IOBottom pins (Example from your IOConfig.json)
  m_outputPins.push_back({ "IOBottom", 0, 0, "L_Gripper" });
  m_outputPins.push_back({ "IOBottom", 0, 2, "R_Gripper" });
  m_outputPins.push_back({ "IOBottom", 0, 10, "Vacuum_Base" });
  m_outputPins.push_back({ "IOBottom", 0, 15, "Dispenser_Shot" });
  m_outputPins.push_back({ "IOBottom", 0, 4, "UV_Head" });
  m_outputPins.push_back({ "IOBottom", 0, 5, "Dispenser_Head" });
  m_outputPins.push_back({ "IOBottom", 0, 14, "UV_PLC1" });
  m_outputPins.push_back({ "IOBottom", 0, 13, "UV_PLC2" });

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