// pi_analog_manager.cpp
#include "../include/motions/pi_analog_manager.h"
#include "imgui.h"

PIAnalogManager::PIAnalogManager(PIControllerManager& controllerManager, MotionConfigManager& configManager)
  : m_controllerManager(controllerManager), m_configManager(configManager) {

  m_logger = Logger::GetInstance();
  m_logger->LogInfo("PIAnalogManager: Initializing");

  // Initialize readers for all available controllers
  InitializeReaders();
}

PIAnalogManager::~PIAnalogManager() {
  m_logger->LogInfo("PIAnalogManager: Shutting down");
}

std::vector<std::string> PIAnalogManager::GetPIControllerDeviceNames() const {
  std::vector<std::string> deviceNames;

  // Get all devices from the config manager
  const auto& allDevices = m_configManager.GetAllDevices();

  // Filter for PI controllers (devices with port 50000)
  for (const auto& [name, device] : allDevices) {
    if (device.Port == 50000 && m_controllerManager.HasController(name)) {
      deviceNames.push_back(name);
    }
  }

  return deviceNames;
}

void PIAnalogManager::InitializeReaders() {
  m_logger->LogInfo("PIAnalogManager: Creating analog readers for all controllers");

  // Get all PI device names
  std::vector<std::string> deviceNames = GetPIControllerDeviceNames();

  for (const auto& deviceName : deviceNames) {
    // Get the controller
    PIController* controller = m_controllerManager.GetController(deviceName);

    if (controller) {
      m_logger->LogInfo("PIAnalogManager: Creating analog reader for device " + deviceName);

      // Create a new analog reader for this controller
      auto reader = std::make_unique<PIAnalogReader>(*controller, deviceName);

      // Store it in our map
      m_readers[deviceName] = std::move(reader);
    }
  }

  m_logger->LogInfo("PIAnalogManager: Created " + std::to_string(m_readers.size()) + " analog readers");
}

PIAnalogReader* PIAnalogManager::GetReader(const std::string& deviceName) {
  auto it = m_readers.find(deviceName);
  if (it != m_readers.end()) {
    return it->second.get();
  }

  return nullptr;
}

void PIAnalogManager::UpdateAllReadings() {
  for (auto& [deviceName, reader] : m_readers) {
    // Get the controller
    PIController* controller = m_controllerManager.GetController(deviceName);

    // Only update readings for connected controllers
    if (controller && controller->IsConnected()) {
      reader->UpdateAllValues();
    }
  }
}

void PIAnalogManager::RenderUI() {
  if (!m_showWindow) {
    return;
  }

  if (!ImGui::Begin(m_windowTitle.c_str(), &m_showWindow)) {
    ImGui::End();
    return;
  }

  // Update button
  if (ImGui::Button("Update All Readings")) {
    UpdateAllReadings();
  }

  ImGui::SameLine();

  // Show/hide all reader windows button
  static bool showAllReaders = false;
  if (ImGui::Button(showAllReaders ? "Hide All Readers" : "Show All Readers")) {
    showAllReaders = !showAllReaders;

    // Set visibility for all readers
    for (auto& [deviceName, reader] : m_readers) {
      reader->SetWindowVisible(showAllReaders);
    }
  }

  ImGui::Separator();

  // Display controllers and their analog readers
  for (auto& [deviceName, reader] : m_readers) {
    if (ImGui::CollapsingHeader(deviceName.c_str())) {
      // Get the controller
      PIController* controller = m_controllerManager.GetController(deviceName);

      if (controller) {
        ImGui::Text("Connection Status: %s",
          controller->IsConnected() ? "Connected" : "Disconnected");

        if (controller->IsConnected()) {
          // Display analog readings
          std::map<int, double> voltageValues;
          if (reader->GetVoltageValues(voltageValues)) {
            if (voltageValues.empty()) {
              ImGui::Text("No analog channels available");
            }
            else {
              // Create table for voltage values
              if (ImGui::BeginTable("VoltageTable", 2, ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Channel");
                ImGui::TableSetupColumn("Voltage (V)");
                ImGui::TableHeadersRow();

                for (const auto& [channel, voltage] : voltageValues) {
                  ImGui::TableNextRow();

                  ImGui::TableNextColumn();
                  ImGui::Text("%d", channel);

                  ImGui::TableNextColumn();
                  ImGui::Text("%.4f V", voltage);
                }

                ImGui::EndTable();
              }
            }
          }
          else {
            ImGui::Text("Failed to read analog values");
          }

          // Add a button to toggle the detailed reader UI
          bool isVisible = false; // We don't have direct access to the visibility state
          if (ImGui::Button(("Open/Close Detailed View##" + deviceName).c_str())) {
            // Toggle visibility for this reader
            reader->SetWindowVisible(!isVisible);
          }
        }
        else {
          ImGui::Text("Connect the controller to read analog values");
        }
      }
      else {
        ImGui::Text("Controller not available");
      }
    }
  }

  ImGui::End();

  // Render individual reader UIs - they'll manage their own visibility
  for (auto& [deviceName, reader] : m_readers) {
    reader->RenderUI();
  }
}