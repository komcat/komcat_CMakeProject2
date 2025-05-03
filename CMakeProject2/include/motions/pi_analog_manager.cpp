// pi_analog_manager.cpp - Simplified implementation
#include "../include/motions/pi_analog_manager.h"
#include "imgui.h"
#include <chrono>
#include <iostream>
#include <sstream>

PIAnalogManager::PIAnalogManager(PIControllerManager& controllerManager, MotionConfigManager& configManager)
  : m_controllerManager(controllerManager),
  m_configManager(configManager),
  m_stopPolling(false),
  m_pollingInterval(100) { // Default 100ms polling interval

  m_logger = Logger::GetInstance();
  m_logger->LogInfo("PIAnalogManager: Initializing");

  m_dataStore = GlobalDataStore::GetInstance();

  // Initialize readers for all available controllers
  InitializeReaders();

  // Start polling automatically
  startPolling();
}

PIAnalogManager::~PIAnalogManager() {
  m_logger->LogInfo("PIAnalogManager: Beginning shutdown");

  // First stop polling thread if active
  if (isPolling()) {
    m_logger->LogInfo("PIAnalogManager: Stopping polling thread");
    stopPolling();
    m_logger->LogInfo("PIAnalogManager: Polling thread stopped");
  }

  // Add a short delay to ensure thread is fully stopped
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Clear readers with a simple approach - avoid complex iteration
  m_logger->LogInfo("PIAnalogManager: Clearing readers");
  {
    std::lock_guard<std::mutex> lock(m_readersMutex);
    m_readers.clear();
  }

  m_logger->LogInfo("PIAnalogManager: Shutdown complete");
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

    // Store the readings in the global data store
    StoreReadingsInGlobalStore();

    std::this_thread::sleep_for(std::chrono::milliseconds(m_pollingInterval));
  }

  m_logger->LogInfo("PIAnalogManager: Polling thread stopped");
}

void PIAnalogManager::StoreReadingsInGlobalStore() {
  std::lock_guard<std::mutex> lock(m_readersMutex);

  // Store each reader's voltage values in the global data store
  for (const auto& [deviceName, reader] : m_readers) {
    const auto& voltageValues = reader->GetLatestVoltageValues();

    for (const auto& [channel, voltage] : voltageValues) {
      // Create a unique key for each device and channel
      std::string key = deviceName + "-Analog-Ch" + std::to_string(channel);

      // Store the voltage value
      m_dataStore->SetValue(key, static_cast<float>(voltage));
    }
  }
}

void PIAnalogManager::cleanupReaders() {
  m_logger->LogInfo("PIAnalogManager: Starting reader cleanup");

  // First stop polling if active
  if (isPolling()) {
    m_logger->LogInfo("PIAnalogManager: Stopping polling as part of cleanup");
    stopPolling();
  }

  // Small delay
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Simple clear approach
  {
    std::lock_guard<std::mutex> lock(m_readersMutex);
    m_readers.clear();
  }

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

void PIAnalogManager::RenderUI() {
  // Skip rendering if window is hidden
  if (!m_showWindow) {
    return;
  }

  if (!ImGui::Begin(m_windowTitle.c_str(), &m_showWindow, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::End();
    return;
  }

  // Polling status and controls
  bool isCurrentlyPolling = isPolling();
  if (isCurrentlyPolling) {
    ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "Auto-Update: Running (%d ms interval)", m_pollingInterval);
    if (ImGui::Button("Stop Auto Updates")) {
      stopPolling();
    }
  }
  else {
    ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1.0f), "Auto-Update: Stopped");
    if (ImGui::Button("Start Auto Updates")) {
      startPolling(100); // 100ms polling interval
    }
  }

  // Interval slider
  if (isCurrentlyPolling) {
    int interval = m_pollingInterval;
    if (ImGui::SliderInt("Polling Interval (ms)", &interval, 50, 1000)) {
      // Only update if changed
      if (interval != m_pollingInterval) {
        stopPolling();
        startPolling(interval);
      }
    }
  }

  ImGui::Separator();

  // Simple array-based structures for rendering
  struct AnalogValue {
    std::string deviceName;
    int channel;
    double voltage;
  };

  // Pre-allocate array with maximum expected size (assume 10 devices with 2 channels each)
  const int MAX_VALUES = 20;
  AnalogValue values[MAX_VALUES];
  int valueCount = 0;

  // Fill the array with data while holding the lock
  {
    std::lock_guard<std::mutex> lock(m_readersMutex);

    for (const auto& entry : m_readers) {
      const std::string& deviceName = entry.first;
      PIAnalogReader* reader = entry.second.get();

      if (reader) {
        // Get a reference to the voltage values
        const auto& voltageValues = reader->GetLatestVoltageValues();

        // Copy each value to our simple array
        for (const auto& volt : voltageValues) {
          if (valueCount < MAX_VALUES) {
            values[valueCount].deviceName = deviceName;
            values[valueCount].channel = volt.first;  // Channel number
            values[valueCount].voltage = volt.second; // Voltage value
            valueCount++;
          }
        }
      }
    }
  }

  // Display the values in a table
  if (ImGui::BeginTable("AllAnalogReadingsTable", 3, ImGuiTableFlags_Borders)) {
    ImGui::TableSetupColumn("Device");
    ImGui::TableSetupColumn("Channel");
    ImGui::TableSetupColumn("Voltage (V)");
    ImGui::TableHeadersRow();

    if (valueCount == 0) {
      // No data case
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("No data");
      ImGui::TableNextColumn();
      ImGui::Text("--");
      ImGui::TableNextColumn();
      ImGui::Text("--");
    }
    else {
      // Display each value from our array
      for (int i = 0; i < valueCount; i++) {
        ImGui::TableNextRow();

        // Device
        ImGui::TableNextColumn();
        ImGui::Text("%s", values[i].deviceName.c_str());

        // Channel
        ImGui::TableNextColumn();
        ImGui::Text("%d", values[i].channel);

        // Voltage
        ImGui::TableNextColumn();
        ImGui::Text("%.4f V", values[i].voltage);
      }
    }

    ImGui::EndTable();
  }

  ImGui::End();
}