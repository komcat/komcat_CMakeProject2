// pi_analog_manager.cpp
#include "../include/motions/pi_analog_manager.h"
#include "imgui.h"
#include "include/data/global_data_store.h" // Add this include

PIAnalogManager::PIAnalogManager(PIControllerManager& controllerManager, MotionConfigManager& configManager)
  : m_controllerManager(controllerManager), m_configManager(configManager) {

  m_logger = Logger::GetInstance();
  m_logger->LogInfo("PIAnalogManager: Initializing");

  // Get the Global Data Store instance
  m_dataStore = GlobalDataStore::GetInstance();

  // Initialize readers for all available controllers
  InitializeReaders();

  // Start polling by default
  startPolling(100); // 100ms refresh rate
}

PIAnalogManager::~PIAnalogManager() {
  m_logger->LogInfo("PIAnalogManager: Shutting down");

  // Stop polling thread if active
  stopPolling();

  // Cleanup readers
  cleanupReaders();
}

void PIAnalogManager::cleanupReaders() {
  m_readers.clear();
  m_logger->LogInfo("PIAnalogManager: Readers cleared");
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
  // Focus only on target devices and channels
  const std::vector<std::string> targetDevices = { "hex-left", "hex-right" };
  const int targetChannels[] = { 5, 6 };
  const int numTargetChannels = 2;

  for (const auto& deviceName : targetDevices) {
    auto it = m_readers.find(deviceName);
    if (it != m_readers.end() && it->second.get() != nullptr) {
      auto reader = it->second.get();
      PIController* controller = m_controllerManager.GetController(deviceName);

      // Only update readings for connected controllers
      if (controller && controller->IsConnected()) {
        // First update all values in the reader
        reader->UpdateAllValues();

        // Then get just the voltage values we need
        std::map<int, double> voltageValues;
        if (reader->GetVoltageValues(voltageValues)) {
          // Store values for specific channels in global data store
          for (int i = 0; i < numTargetChannels; i++) {
            int channel = targetChannels[i];
            auto voltIt = voltageValues.find(channel);

            if (voltIt != voltageValues.end()) {
              // Create a server ID based on device and channel
              std::string serverId = deviceName + "-A-" + std::to_string(channel);

              // Store the voltage value in the global data store
              if (m_dataStore) {
                m_dataStore->SetValue(serverId, static_cast<float>(voltIt->second));

                char logBuffer[128];
                snprintf(logBuffer, sizeof(logBuffer),
                  "Updated global data store: %s = %.4f V",
                  serverId.c_str(), voltIt->second);

                if (m_enableDebugLogging) {
                  m_logger->LogInfo(logBuffer);
                }
              }
            }
          }
        }
      }
    }
  }
}

void PIAnalogManager::RenderUI() {
  if (!m_showWindow) return;

  ImGui::Begin("PI Analog Monitor", &m_showWindow);

  ImGui::Text("Monitoring analog inputs from PI controllers");

  // Show polling status and controls
  bool pollingActive = this->isPolling();
  if (ImGui::Checkbox("Auto-Update", &pollingActive)) {
    if (pollingActive) {
      startPolling(100); // 100ms refresh rate
    }
    else {
      stopPolling();
    }
  }

  ImGui::SameLine();

  // Manual update button
  if (ImGui::Button("Update Now")) {
    UpdateAllReadings();
  }

  ImGui::SameLine();

  // Debug logging toggle
  if (ImGui::Checkbox("Debug Logging", &m_enableDebugLogging)) {
    m_logger->LogInfo(m_enableDebugLogging ?
      "PIAnalogManager: Debug logging enabled" :
      "PIAnalogManager: Debug logging disabled");
  }

  ImGui::Separator();

  // Create a table to display the hardcoded channels for hex-left and hex-right
  if (ImGui::BeginTable("AnalogChannelsTable", 4, ImGuiTableFlags_Borders)) {
    ImGui::TableSetupColumn("Device");
    ImGui::TableSetupColumn("Channel");
    ImGui::TableSetupColumn("Voltage (V)");
    ImGui::TableSetupColumn("DataStore ID");
    ImGui::TableHeadersRow();

    // Hardcoded channels to display (5 and 6)
    const int targetChannels[] = { 5, 6 };
    const int numTargetChannels = 2;

    // Only display these specific devices
    const std::vector<std::string> targetDevices = { "hex-left", "hex-right" };

    for (const auto& deviceName : targetDevices) {
      auto it = m_readers.find(deviceName);
      if (it != m_readers.end() && it->second.get() != nullptr) {
        auto reader = it->second.get();
        PIController* controller = m_controllerManager.GetController(deviceName);

        if (controller && controller->IsConnected()) {
          // Get all voltage values
          std::map<int, double> voltageValues;
          if (reader->GetVoltageValues(voltageValues)) {
            // Display only the target channels
            for (int i = 0; i < numTargetChannels; i++) {
              int channel = targetChannels[i];

              auto voltIt = voltageValues.find(channel);
              if (voltIt != voltageValues.end()) {
                ImGui::TableNextRow();

                // Column 1: Device name
                ImGui::TableNextColumn();
                ImGui::Text("%s", deviceName.c_str());

                // Column 2: Channel number
                ImGui::TableNextColumn();
                ImGui::Text("Channel #%d", channel);

                // Column 3: Voltage value
                ImGui::TableNextColumn();
                ImGui::Text("%.4f V", voltIt->second);

                // Column 4: Data Store ID
                ImGui::TableNextColumn();
                std::string serverId = deviceName + "_channel_" + std::to_string(channel);
                ImGui::Text("%s", serverId.c_str());

                // Also display the data store value if debug logging is enabled
                if (m_enableDebugLogging && m_dataStore && m_dataStore->HasValue(serverId)) {
                  ImGui::SameLine();
                  float storedValue = m_dataStore->GetValue(serverId);
                  ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "(%.4f V)", storedValue);
                }
              }
            }
          }
          else {
            // If failed to get values, display error for this device
            for (int i = 0; i < numTargetChannels; i++) {
              ImGui::TableNextRow();

              ImGui::TableNextColumn();
              ImGui::Text("%s", deviceName.c_str());

              ImGui::TableNextColumn();
              ImGui::Text("Channel #%d", targetChannels[i]);

              ImGui::TableNextColumn();
              ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error reading channel");

              ImGui::TableNextColumn();
              ImGui::Text("%s_channel_%d", deviceName.c_str(), targetChannels[i]);
            }
          }
        }
        else {
          // If controller not connected, display "Not connected" for this device
          for (int i = 0; i < numTargetChannels; i++) {
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("%s", deviceName.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("Channel #%d", targetChannels[i]);

            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Not connected");

            ImGui::TableNextColumn();
            ImGui::Text("%s_channel_%d", deviceName.c_str(), targetChannels[i]);
          }
        }
      }
    }

    ImGui::EndTable();
  }

  // Add a simple bar graph visualization for the voltages
  ImGui::Separator();
  ImGui::Text("Voltage Visualization");

  for (const auto& deviceName : { "hex-left", "hex-right" }) {
    auto it = m_readers.find(deviceName);
    if (it != m_readers.end() && it->second.get() != nullptr) {
      auto reader = it->second.get();
      PIController* controller = m_controllerManager.GetController(deviceName);

      if (controller && controller->IsConnected()) {
        // Get all voltage values
        std::map<int, double> voltageValues;
        if (reader->GetVoltageValues(voltageValues)) {
          // Display only channels 5 and 6
          for (int channel : {5, 6}) {
            auto voltIt = voltageValues.find(channel);
            if (voltIt != voltageValues.end()) {
              char buffer[64];
              snprintf(buffer, sizeof(buffer), "%s - Channel #%d", deviceName, channel);
              ImGui::Text("%s", buffer);

              // Normalize the voltage for the bar (0-5V typical range for analog inputs)
              float normalizedVoltage = static_cast<float>(voltIt->second) / 5.0f;
              normalizedVoltage = (std::min)(1.0f, (std::max)(0.0f, normalizedVoltage)); // Clamp to 0-1

              // Choose color based on device
              ImVec4 barColor;
              if (strcmp(deviceName, "hex-left") == 0) {
                barColor = ImVec4(0.2f, 0.6f, 1.0f, 0.8f); // Blue for hex-left
              }
              else {
                barColor = ImVec4(0.8f, 0.2f, 0.6f, 0.8f); // Purple for hex-right
              }

              // Convert voltage to string for display in progress bar
              char voltageStr[16];
              snprintf(voltageStr, sizeof(voltageStr), "%.4f V", voltIt->second);

              // Draw the bar
              ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
              ImGui::ProgressBar(normalizedVoltage, ImVec2(-1.0f, 0.0f), voltageStr);
              ImGui::PopStyleColor();
            }
          }
        }
      }
    }
  }

  ImGui::End();
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
  // Signal the thread to stop
  m_stopPolling = true;

  // Wait for the thread to exit
  if (m_pollingThread) {
    if (m_pollingThread->joinable()) {
      m_pollingThread->join();
    }
    delete m_pollingThread;
    m_pollingThread = nullptr;
  }

  m_logger->LogInfo("PIAnalogManager: Polling stopped");
}

bool PIAnalogManager::isPolling() const {
  return m_pollingThread != nullptr && !m_stopPolling;
}

void PIAnalogManager::pollingThreadFunc() {
  m_logger->LogInfo("PIAnalogManager: Polling thread started");

  while (!m_stopPolling) {
    try {
      // Update all analog readings
      std::lock_guard<std::mutex> lock(m_readersMutex);
      UpdateAllReadings();
    }
    catch (const std::exception& e) {
      m_logger->LogError("PIAnalogManager: Error in polling thread: " + std::string(e.what()));
    }
    catch (...) {
      m_logger->LogError("PIAnalogManager: Unknown error in polling thread");
    }

    // Sleep for the specified interval
    std::this_thread::sleep_for(std::chrono::milliseconds(m_pollingInterval));
  }

  m_logger->LogInfo("PIAnalogManager: Polling thread stopped");
}