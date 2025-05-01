// pi_analog_manager.cpp
#include "../include/motions/pi_analog_manager.h"
#include "imgui.h"
#include <chrono>
#include <iostream>

PIAnalogManager::PIAnalogManager(PIControllerManager& controllerManager, MotionConfigManager& configManager)
  : m_controllerManager(controllerManager),
  m_configManager(configManager),
  m_stopPolling(false),
  m_pollingInterval(100), // Default 100ms polling interval
  m_pollingThread(nullptr) {

  m_logger = Logger::GetInstance();
  m_logger->LogInfo("PIAnalogManager: Initializing");

  // Initialize readers for all available controllers
  InitializeReaders();
}

PIAnalogManager::~PIAnalogManager() {
  // Stop polling thread if active
  stopPolling();

  // First cleanup readers in a controlled manner
  cleanupReaders();



  m_logger->LogInfo("PIAnalogManager: Shutting down");
}

void PIAnalogManager::startPolling(unsigned int intervalMs) {
  // Don't start if already running
  if (m_pollingThread) {
    return;
  }

  // Set the polling interval
  m_pollingInterval = intervalMs;

  // Reset the stop flag
  m_stopPolling = false;

  // Start the polling thread
  m_pollingThread = new std::thread(&PIAnalogManager::pollingThreadFunc, this);

  m_logger->LogInfo("PIAnalogManager: Polling thread started with interval " +
    std::to_string(m_pollingInterval) + "ms");
}

// Add this method if you haven't already
void PIAnalogManager::stopPolling() {
  if (!m_pollingThread) {
    return;
  }

  // Set the stop flag
  m_stopPolling = true;

  // Wait for the thread to finish
  if (m_pollingThread->joinable()) {
    m_pollingThread->join();
  }

  // Clean up the thread object
  delete m_pollingThread;
  m_pollingThread = nullptr;

  m_logger->LogInfo("PIAnalogManager: Polling stopped");
}

bool PIAnalogManager::isPolling() const {
  return m_pollingThread != nullptr && !m_stopPolling;
}

void PIAnalogManager::pollingThreadFunc() {
  // At the beginning of the function
  m_logger->LogInfo("PIAnalogManager: Polling thread started");

  // During polling, use safe access to readers
  while (!m_stopPolling) {
    std::vector<std::string> readerNames;

    // First collect the names to avoid holding the lock during update
    {
      std::lock_guard<std::mutex> lock(m_readersMutex);
      for (const auto& pair : m_readers) {
        readerNames.push_back(pair.first);
      }
    }

    // Then update each reader by name
    for (const auto& name : readerNames) {
      PIAnalogReader* reader = nullptr;
      {
        std::lock_guard<std::mutex> lock(m_readersMutex);
        auto it = m_readers.find(name);
        if (it != m_readers.end()) {
          reader = it->second.get();
        }
      }

      if (reader) {
        try {
          reader->UpdateAllValues();
        }
        catch (...) {
          m_logger->LogWarning("Exception while updating reader: " + name);
        }
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(m_pollingInterval));
  }

  m_logger->LogInfo("PIAnalogManager: Polling thread stopped");
}


// In pi_analog_manager.cpp, update the cleanupReaders method:
void PIAnalogManager::cleanupReaders() {
  // First stop polling
  stopPolling();

  // Clear the map in a thread-safe way
  {
    std::lock_guard<std::mutex> lock(m_readersMutex);

    // Clear the map explicitly (which will destroy all readers)
    m_readers.clear();
  }

  // Log after cleanup
  m_logger->LogInfo("PIAnalogManager: All readers cleared");
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
  std::lock_guard<std::mutex> lock(m_readersMutex);

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
  // Skip rendering if window is hidden
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

  // Start/stop polling button
  bool isCurrentlyPolling = isPolling();
  if (isCurrentlyPolling) {
    if (ImGui::Button("Stop Auto Updates")) {
      stopPolling();
    }
  }
  else {
    if (ImGui::Button("Start Auto Updates")) {
      startPolling(100); // 100ms polling interval
    }
  }

  ImGui::SameLine();

  // Show/hide all reader windows button
  static bool showAllReaders = false;
  if (ImGui::Button(showAllReaders ? "Hide All Readers" : "Show All Readers")) {
    showAllReaders = !showAllReaders;

    // Set visibility for all readers
    std::lock_guard<std::mutex> lock(m_readersMutex);
    for (auto& [deviceName, reader] : m_readers) {
      reader->SetWindowVisible(showAllReaders);
    }
  }

  ImGui::Separator();

  // Display controllers and their analog readers
  std::lock_guard<std::mutex> lock(m_readersMutex);
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