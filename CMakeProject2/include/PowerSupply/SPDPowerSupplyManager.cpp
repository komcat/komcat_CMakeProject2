#include "SPDPowerSupplyManager.h"
#include "logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <iostream>

// Use PowerSupply namespace
using PowerSupply::SPDPowerSupply;

// Include ImGui for UI rendering
#ifdef ENABLE_IMGUI
#include <imgui.h>
#include <imgui_internal.h>
#endif

// Include VISA for device discovery
#ifdef _WIN32
#include <visa.h>
#pragma comment(lib, "visa64.lib")
#endif

using json = nlohmann::json;

// === Construction & Destruction ===

SPDPowerSupplyManager::SPDPowerSupplyManager()
  : m_managerName("SPD Power Supply Manager")
  , m_configFile("")
  , m_autoConnect(true)
  , m_defaultPollingInterval(1000)
  , m_autoDiscovery(true)
  , m_safetyMode(true)
  , m_showWindow(false)
  , m_pollingActive(false)
  , m_pollingInterval(1000)
  , m_discoveryInProgress(false)  // Add this
  , m_discoveryComplete(false)     // Add this
  , m_logger(nullptr)
{
  // Get logger instance if available
  m_logger = Logger::GetInstance();
  LogMessage("INFO", "SPDPowerSupplyManager created");
}



SPDPowerSupplyManager::~SPDPowerSupplyManager() {
  LogMessage("INFO", "SPDPowerSupplyManager destructor called");

  // Wait for discovery to complete if in progress
  if (m_discoveryInProgress.load()) {
    LogMessage("INFO", "Waiting for discovery to complete...");
    // Give it max 1 second to finish
    for (int i = 0; i < 10 && m_discoveryInProgress.load(); i++) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  

  // Stop polling first
  StopAllPolling();

  // Disconnect all devices
  DisconnectAll();

  // Clear devices
  std::lock_guard<std::mutex> lock(m_devicesMutex);
  m_devices.clear();

  LogMessage("INFO", "SPDPowerSupplyManager destroyed");
}

// === Configuration & Initialization ===

bool SPDPowerSupplyManager::Initialize(const std::string& configFile) {
  LogMessage("INFO", "Initializing SPDPowerSupplyManager with config: " + configFile);

  m_configFile = configFile;

  // Try to parse configuration file
  if (!configFile.empty() && ParseConfigurationFile(configFile)) {
    LogMessage("INFO", "Configuration loaded successfully from: " + configFile);

    // Auto-connect devices if enabled
    if (m_autoConnect) {
      int connected = ConnectAll();
      LogMessage("INFO", "Auto-connected " + std::to_string(connected) + " devices");
    }

    return true;
  }
  else {
    LogMessage("WARNING", "Failed to load configuration file, using defaults");
    LoadDefaultConfiguration();
    return false;
  }
}

void SPDPowerSupplyManager::LoadDefaultConfiguration() {
  LogMessage("INFO", "Loading default configuration");

  m_managerName = "SPD Power Supply Manager";
  m_autoConnect = true;
  m_defaultPollingInterval = 1000;
  m_autoDiscovery = true;
  m_safetyMode = true;

  // Use async discovery instead of blocking
  if (m_autoDiscovery) {
    StartAsyncDiscovery(m_autoConnect);
    LogMessage("INFO", "Started async device discovery (non-blocking)");
  }
}


// === Device Management ===

bool SPDPowerSupplyManager::AddDevice(const std::string& name, const std::string& resourceString, int port) {
  std::lock_guard<std::mutex> lock(m_devicesMutex);

  // Check if device already exists
  if (m_devices.find(name) != m_devices.end()) {
    SetError("Device with name '" + name + "' already exists");
    return false;
  }

  try {
    // Create device info
    auto deviceInfo = std::make_unique<DeviceInfo>(resourceString, port);
    deviceInfo->device = std::make_unique<SPDPowerSupply>(resourceString);
    deviceInfo->description = "SPD Power Supply at " + resourceString;
    deviceInfo->pollingInterval = m_defaultPollingInterval;

    // Store device
    m_devices[name] = std::move(deviceInfo);

    LogMessage("INFO", "Added device: " + name + " (" + resourceString + ")");
    return true;

  }
  catch (const std::exception& e) {
    SetError("Failed to add device '" + name + "': " + e.what());
    return false;
  }
}

SPDPowerSupply* SPDPowerSupplyManager::GetDevice(const std::string& name) {
  std::lock_guard<std::mutex> lock(m_devicesMutex);

  auto it = m_devices.find(name);
  if (it != m_devices.end()) {
    return it->second->device.get();
  }

  return nullptr;
}

bool SPDPowerSupplyManager::RemoveDevice(const std::string& name) {
  std::lock_guard<std::mutex> lock(m_devicesMutex);

  auto it = m_devices.find(name);
  if (it != m_devices.end()) {
    // Disconnect device if connected
    if (it->second->device && it->second->device->isConnected()) {
      it->second->device->disconnect();
    }

    // Remove from map
    m_devices.erase(it);

    LogMessage("INFO", "Removed device: " + name);

    // Notify callback if set
    if (m_connectionStateCallback) {
      m_connectionStateCallback(name, false);
    }

    return true;
  }

  SetError("Device '" + name + "' not found");
  return false;
}

std::vector<std::string> SPDPowerSupplyManager::GetDeviceNames() const {
  std::lock_guard<std::mutex> lock(m_devicesMutex);

  std::vector<std::string> names;
  names.reserve(m_devices.size());

  for (const auto& pair : m_devices) {
    names.push_back(pair.first);
  }

  return names;
}

int SPDPowerSupplyManager::GetConnectedCount() const {
  std::lock_guard<std::mutex> lock(m_devicesMutex);

  int count = 0;
  for (const auto& pair : m_devices) {
    if (pair.second->device && pair.second->device->isConnected()) {
      count++;
    }
  }

  return count;
}

// === Connection Management ===

int SPDPowerSupplyManager::ConnectAll() {
  std::lock_guard<std::mutex> lock(m_devicesMutex);

  int connected = 0;

  for (auto& pair : m_devices) {
    const std::string& name = pair.first;
    auto& deviceInfo = pair.second;

    if (!deviceInfo->device) {
      continue;
    }

    try {
      // Skip if already connected
      if (deviceInfo->device->isConnected()) {
        connected++;
        continue;
      }

      // Try to connect
      if (deviceInfo->device->connect()) {
        connected++;
        LogMessage("INFO", "Connected device: " + name);

        // Notify callback if set
        if (m_connectionStateCallback) {
          m_connectionStateCallback(name, true);
        }

        // Initialize device if safety mode is enabled
        if (m_safetyMode) {
          deviceInfo->device->setOutput(1, false);  // Assuming single channel
        }

      }
      else {
        LogMessage("WARNING", "Failed to connect device: " + name);
      }

    }
    catch (const std::exception& e) {
      LogMessage("ERROR", "Exception connecting device " + name + ": " + e.what());
    }
  }

  LogMessage("INFO", "Connected " + std::to_string(connected) + "/" +
    std::to_string(m_devices.size()) + " devices");

  return connected;
}

void SPDPowerSupplyManager::DisconnectAll() {
  StopAllPolling();

  std::lock_guard<std::mutex> lock(m_devicesMutex);

  for (auto& pair : m_devices) {
    const std::string& name = pair.first;
    auto& deviceInfo = pair.second;

    if (deviceInfo->device && deviceInfo->device->isConnected()) {
      try {
        // Disable output before disconnecting if safety mode enabled
        if (m_safetyMode) {
          deviceInfo->device->setOutput(1, false);
        }

        deviceInfo->device->disconnect();
        LogMessage("INFO", "Disconnected device: " + name);

        // Notify callback if set
        if (m_connectionStateCallback) {
          m_connectionStateCallback(name, false);
        }

      }
      catch (const std::exception& e) {
        LogMessage("ERROR", "Exception disconnecting device " + name + ": " + e.what());
      }
    }
  }
}

bool SPDPowerSupplyManager::AreAllConnected() const {
  std::lock_guard<std::mutex> lock(m_devicesMutex);

  if (m_devices.empty()) {
    return false;
  }

  for (const auto& pair : m_devices) {
    if (!pair.second->device || !pair.second->device->isConnected()) {
      return false;
    }
  }

  return true;
}

// === Device Discovery ===

int SPDPowerSupplyManager::AddDiscoveredDevices(bool connectImmediately) {
  LogMessage("INFO", "Starting device discovery...");

  std::vector<std::string> discoveredDevices = GetAvailableSPDDevices();
  int added = 0;

  for (const auto& resourceString : discoveredDevices) {
    // Generate a unique name for the device
    std::string deviceName = "SPD_" + std::to_string(added + 1);

    // Check if we already have this resource string
    bool alreadyExists = false;
    {
      std::lock_guard<std::mutex> lock(m_devicesMutex);
      for (const auto& pair : m_devices) {
        if (pair.second->resourceString == resourceString) {
          alreadyExists = true;
          break;
        }
      }
    }

    if (!alreadyExists) {
      if (AddDevice(deviceName, resourceString)) {
        added++;
        LogMessage("INFO", "Discovered and added device: " + deviceName + " (" + resourceString + ")");

        // Connect immediately if requested
        if (connectImmediately) {
          auto device = GetDevice(deviceName);
          if (device && device->connect(resourceString)) {
            LogMessage("INFO", "Connected discovered device: " + deviceName);
          }
        }
      }
    }
  }

  LogMessage("INFO", "Device discovery completed. Added " + std::to_string(added) + " devices");
  return added;
}

// === Synchronized Operations ===

bool SPDPowerSupplyManager::SetAllOutputs(bool enable) {
  std::lock_guard<std::mutex> lock(m_devicesMutex);

  bool allSuccess = true;
  int operationCount = 0;

  for (const auto& pair : m_devices) {
    const std::string& name = pair.first;
    auto& deviceInfo = pair.second;

    if (deviceInfo->device && deviceInfo->device->isConnected()) {
      try {
        if (deviceInfo->device->setOutput(1, enable)) {  // Channel 1
          operationCount++;
        }
        else {
          LogMessage("WARNING", "Failed to set output for device: " + name);
          allSuccess = false;
        }
      }
      catch (const std::exception& e) {
        LogMessage("ERROR", "Exception setting output for device " + name + ": " + e.what());
        allSuccess = false;
      }
    }
  }

  LogMessage("INFO", (enable ? "Enabled" : "Disabled") + std::string(" outputs on ") +
    std::to_string(operationCount) + " devices");

  return allSuccess && operationCount > 0;
}

bool SPDPowerSupplyManager::ResetAllInstruments() {
  std::lock_guard<std::mutex> lock(m_devicesMutex);

  bool allSuccess = true;
  int operationCount = 0;

  for (const auto& pair : m_devices) {
    const std::string& name = pair.first;
    auto& deviceInfo = pair.second;

    if (deviceInfo->device && deviceInfo->device->isConnected()) {
      try {
        // Reset command - assuming there's a reset method or we disable output
        deviceInfo->device->setOutput(1, false);  // Safety reset
        operationCount++;
        LogMessage("INFO", "Reset device: " + name);
      }
      catch (const std::exception& e) {
        LogMessage("ERROR", "Exception resetting device " + name + ": " + e.what());
        allSuccess = false;
      }
    }
  }

  LogMessage("INFO", "Reset " + std::to_string(operationCount) + " devices");
  return allSuccess && operationCount > 0;
}

bool SPDPowerSupplyManager::SetAllVoltages(double voltage) {
  std::lock_guard<std::mutex> lock(m_devicesMutex);

  bool allSuccess = true;
  int operationCount = 0;

  for (const auto& pair : m_devices) {
    const std::string& name = pair.first;
    auto& deviceInfo = pair.second;

    if (deviceInfo->device && deviceInfo->device->isConnected()) {
      try {
        if (deviceInfo->device->setVoltage(1, voltage)) {  // Channel 1
          operationCount++;
        }
        else {
          LogMessage("WARNING", "Failed to set voltage for device: " + name);
          allSuccess = false;
        }
      }
      catch (const std::exception& e) {
        LogMessage("ERROR", "Exception setting voltage for device " + name + ": " + e.what());
        allSuccess = false;
      }
    }
  }

  LogMessage("INFO", "Set voltage to " + std::to_string(voltage) + "V on " +
    std::to_string(operationCount) + " devices");

  return allSuccess && operationCount > 0;
}

bool SPDPowerSupplyManager::SetAllCurrentLimits(double current) {
  std::lock_guard<std::mutex> lock(m_devicesMutex);

  bool allSuccess = true;
  int operationCount = 0;

  for (const auto& pair : m_devices) {
    const std::string& name = pair.first;
    auto& deviceInfo = pair.second;

    if (deviceInfo->device && deviceInfo->device->isConnected()) {
      try {
        if (deviceInfo->device->setCurrent(1, current)) {  // Channel 1
          operationCount++;
        }
        else {
          LogMessage("WARNING", "Failed to set current limit for device: " + name);
          allSuccess = false;
        }
      }
      catch (const std::exception& e) {
        LogMessage("ERROR", "Exception setting current limit for device " + name + ": " + e.what());
        allSuccess = false;
      }
    }
  }

  LogMessage("INFO", "Set current limit to " + std::to_string(current) + "A on " +
    std::to_string(operationCount) + " devices");

  return allSuccess && operationCount > 0;
}

std::unordered_map<std::string, std::string> SPDPowerSupplyManager::GetAllStatuses() const {
  std::lock_guard<std::mutex> lock(m_devicesMutex);
  std::unordered_map<std::string, std::string> statuses;

  for (const auto& pair : m_devices) {
    const std::string& name = pair.first;
    auto& deviceInfo = pair.second;

    if (deviceInfo->device) {
      if (deviceInfo->device->isConnected()) {
        try {
          auto outputEnabled = deviceInfo->device->getOutputState(1);
          auto voltage = deviceInfo->device->getVoltage(1);
          auto current = deviceInfo->device->getCurrent(1);

          if (outputEnabled.has_value() && voltage.has_value() && current.has_value()) {
            std::ostringstream status;
            status << "Connected | Output: " << (outputEnabled.value() ? "ON" : "OFF")
              << " | V: " << std::fixed << std::setprecision(3) << voltage.value()
              << "V | I: " << std::fixed << std::setprecision(3) << current.value() << "A";
            statuses[name] = status.str();
          }
          else {
            statuses[name] = "Connected | Status read failed";
          }
        }
        catch (const std::exception& e) {
          statuses[name] = "Connected | Error: " + std::string(e.what());
        }
      }
      else {
        statuses[name] = "Disconnected";
      }
    }
    else {
      statuses[name] = "Device not initialized";
    }
  }

  return statuses;  // ← This was incorrectly placed inside the loop
}


  // === Polling & Monitoring ===

  void SPDPowerSupplyManager::StartAllPolling(int intervalMs) {
    if (m_pollingActive.load()) {
      LogMessage("WARNING", "Polling already active");
      return;
    }

    m_pollingInterval.store(intervalMs);
    m_pollingActive.store(true);

    m_pollingThread = std::thread(&SPDPowerSupplyManager::PollingThreadFunction, this);

    LogMessage("INFO", "Started polling with interval: " + std::to_string(intervalMs) + "ms");
  }

  void SPDPowerSupplyManager::StopAllPolling() {
    if (!m_pollingActive.load()) {
      return;
    }

    m_pollingActive.store(false);

    if (m_pollingThread.joinable()) {
      m_pollingThread.join();
    }

    LogMessage("INFO", "Stopped polling");
  }

  // === UI Management ===

  void SPDPowerSupplyManager::RenderUI() {
#ifdef ENABLE_IMGUI
    if (!m_showWindow) {
      return;
    }

    if (ImGui::Begin("SPD Power Supply Manager", &m_showWindow)) {
      // Manager status section
      ImGui::Text("Manager: %s", m_managerName.c_str());
      ImGui::Text("Devices: %zu", GetDeviceCount());
      ImGui::Text("Connected: %d", GetConnectedCount());
      ImGui::Text("Polling: %s", IsPollingActive() ? "Active" : "Stopped");

      ImGui::Separator();

      // Control buttons
      if (ImGui::Button("Connect All")) {
        ConnectAll();
      }
      ImGui::SameLine();
      if (ImGui::Button("Disconnect All")) {
        DisconnectAll();
      }
      ImGui::SameLine();
      if (ImGui::Button("Reset All")) {
        ResetAllInstruments();
      }

      ImGui::Separator();

      // Global controls
      static bool allOutputsEnabled = false;
      if (ImGui::Checkbox("All Outputs Enable", &allOutputsEnabled)) {
        SetAllOutputs(allOutputsEnabled);
      }

      static float globalVoltage = 0.0f;
      if (ImGui::SliderFloat("Global Voltage (V)", &globalVoltage, 0.0f, 30.0f)) {
        SetAllVoltages(globalVoltage);
      }

      static float globalCurrent = 0.5f;
      if (ImGui::SliderFloat("Global Current Limit (A)", &globalCurrent, 0.0f, 5.0f)) {
        SetAllCurrentLimits(globalCurrent);
      }

      ImGui::Separator();

      // Polling controls
      static int pollingInterval = 1000;
      ImGui::SliderInt("Polling Interval (ms)", &pollingInterval, 100, 5000);

      if (!IsPollingActive()) {
        if (ImGui::Button("Start Polling")) {
          StartAllPolling(pollingInterval);
        }
      }
      else {
        if (ImGui::Button("Stop Polling")) {
          StopAllPolling();
        }
      }

      ImGui::Separator();

      // Device discovery
      if (ImGui::Button("Discover Devices")) {
        AddDiscoveredDevices(false);
      }
      ImGui::SameLine();
      if (ImGui::Button("Discover & Connect")) {
        AddDiscoveredDevices(true);
      }

      ImGui::Separator();

      // Individual device panels
      std::lock_guard<std::mutex> lock(m_devicesMutex);
      for (auto& pair : m_devices) {
        RenderDevicePanel(pair.first, *pair.second);
      }

      // Emergency stop button
      ImGui::Separator();
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
      if (ImGui::Button("EMERGENCY STOP", ImVec2(-1, 30))) {
        EmergencyStop();
      }
      ImGui::PopStyleColor();
    }
    ImGui::End();
#endif
  }

  void SPDPowerSupplyManager::ToggleWindow() {
    m_showWindow = !m_showWindow;
  }

  // === Safety & Error Handling ===

  bool SPDPowerSupplyManager::EmergencyStop() {
    LogMessage("WARNING", "EMERGENCY STOP ACTIVATED");

    bool success = SetAllOutputs(false);

    if (success) {
      LogMessage("INFO", "Emergency stop completed successfully");
    }
    else {
      LogMessage("ERROR", "Emergency stop may not have completed successfully");
    }

    return success;
  }

  // === Configuration Export/Import ===

  bool SPDPowerSupplyManager::ExportConfiguration(const std::string & filename) const {
    try {
      std::string configJson = GetConfigurationJSON();

      std::ofstream file(filename);
      if (!file.is_open()) {
        SetError("Failed to open file for writing: " + filename);
        return false;
      }

      file << configJson;
      file.close();

      LogMessage("INFO", "Configuration exported to: " + filename);
      return true;

    }
    catch (const std::exception& e) {
      SetError("Failed to export configuration: " + std::string(e.what()));
      return false;
    }
  }

  std::string SPDPowerSupplyManager::GetConfigurationJSON() const {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    json config;

    // Manager settings
    config["spdManager"] = {
        {"name", m_managerName},
        {"version", "1.0"},
        {"autoConnect", m_autoConnect},
        {"defaultPollingInterval", m_defaultPollingInterval},
        {"autoDiscovery", m_autoDiscovery},
        {"safetyMode", m_safetyMode}
    };

    // Device configurations
    json devices = json::array();
    for (const auto& pair : m_devices) {
      const std::string& name = pair.first;
      const auto& deviceInfo = pair.second;

      json deviceConfig = {
          {"name", name},
          {"resourceString", deviceInfo->resourceString},
          {"maxChannels", 1},  // SPD devices typically have 1 channel
          {"autoConnect", deviceInfo->autoConnect},
          {"pollingInterval", deviceInfo->pollingInterval},
          {"description", deviceInfo->description}
      };

      devices.push_back(deviceConfig);
    }
    config["devices"] = devices;

    // Global settings
    config["globalSettings"] = {
        {"enableOutputOnConnect", false},
        {"resetOnConnect", false},
        {"logAllOperations", false},
        {"safetyTimeoutMs", 10000}
    };

    return config.dump(4);  // Pretty print with 4-space indentation
  }

  // === Private Methods ===

  void SPDPowerSupplyManager::SetError(const std::string & error) const {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_lastError = error;
    LogMessage("ERROR", error);
  }

  void SPDPowerSupplyManager::LogMessage(const std::string & level, const std::string & message) const {
    if (m_logger) {
      if (level == "INFO") {
        m_logger->LogInfo("SPDManager: " + message);
      }
      else if (level == "WARNING") {
        m_logger->LogWarning("SPDManager: " + message);
      }
      else if (level == "ERROR") {
        m_logger->LogError("SPDManager: " + message);
      }
    }
  }


  void SPDPowerSupplyManager::PollingThreadFunction() {
    LogMessage("INFO", "Polling thread started");
    int iterationCount = 0;

    while (m_pollingActive.load()) {
      iterationCount++;
      try {
        auto statuses = GetAllStatuses();

        if (m_statusUpdateCallback) {
          for (const auto& pair : statuses) {
            m_statusUpdateCallback(pair.first, pair.second);
          }
        }
        else {
          LogMessage("ERROR", "No callback set - iteration #" + std::to_string(iterationCount));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(m_pollingInterval.load()));
      }
      catch (const std::exception& e) {
        LogMessage("ERROR", "Polling exception: " + std::string(e.what()));
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      }
    }

    LogMessage("INFO", "Polling thread ended");
  }


  // Add this helper method
  void SPDPowerSupplyManager::NotifySubscribers(const std::unordered_map<std::string, SPDDeviceStatus>& statuses) {
    std::lock_guard<std::mutex> lock(m_subscribersMutex);

    if (m_subscribers.empty()) return;

    for (const auto& statusPair : statuses) {
      const SPDDeviceStatus& status = statusPair.second;

      // Notify all subscribers of status update
      for (const auto& subscriberPair : m_subscribers) {
        try {
          subscriberPair.second->OnDeviceStatusUpdate(status);
        }
        catch (const std::exception& e) {
          LogMessage("ERROR", "Exception notifying subscriber " + subscriberPair.first + ": " + e.what());
        }
      }
    }
  }


  bool SPDPowerSupplyManager::ParseConfigurationFile(const std::string & configFile) {
    try {
      std::ifstream file(configFile);
      if (!file.is_open()) {
        SetError("Failed to open config file: " + configFile);
        return false;
      }

      json config;
      file >> config;

      // Parse manager settings
      if (config.contains("spdManager")) {
        const auto& mgr = config["spdManager"];

        if (mgr.contains("name")) m_managerName = mgr["name"];
        if (mgr.contains("autoConnect")) m_autoConnect = mgr["autoConnect"];
        if (mgr.contains("defaultPollingInterval")) m_defaultPollingInterval = mgr["defaultPollingInterval"];
        if (mgr.contains("autoDiscovery")) m_autoDiscovery = mgr["autoDiscovery"];
        if (mgr.contains("safetyMode")) m_safetyMode = mgr["safetyMode"];
      }

      // Parse devices
      if (config.contains("devices") && config["devices"].is_array()) {
        for (const auto& deviceConfig : config["devices"]) {
          if (!ValidateDeviceConfig(deviceConfig)) {
            continue;
          }

          std::string name = deviceConfig["name"];
          CreateDeviceFromConfig(name, deviceConfig);
        }
      }

      return true;

    }
    catch (const std::exception& e) {
      SetError("Failed to parse config file: " + std::string(e.what()));
      return false;
    }
  }

  bool SPDPowerSupplyManager::CreateDeviceFromConfig(const std::string & name, const nlohmann::json & config) {
    try {
      std::string resourceString = config["resourceString"];
      int port = config.value("port", 5555);

      if (AddDevice(name, resourceString, port)) {
        // Set additional properties
        auto device = GetDevice(name);
        if (device) {
          std::lock_guard<std::mutex> lock(m_devicesMutex);
          auto it = m_devices.find(name);
          if (it != m_devices.end()) {
            auto& deviceInfo = it->second;

            deviceInfo->autoConnect = config.value("autoConnect", true);
            deviceInfo->pollingInterval = config.value("pollingInterval", m_defaultPollingInterval);
            deviceInfo->description = config.value("description", "SPD Power Supply");
          }
        }
        return true;
      }

    }
    catch (const std::exception& e) {
      SetError("Failed to create device from config: " + std::string(e.what()));
    }

    return false;
  }

  void SPDPowerSupplyManager::RenderDevicePanel(const std::string & name, DeviceInfo & deviceInfo) {
#ifdef ENABLE_IMGUI
    if (ImGui::CollapsingHeader(name.c_str())) {
      ImGui::Indent();

      // Device info
      ImGui::Text("Resource: %s", deviceInfo.resourceString.c_str());
      ImGui::Text("Description: %s", deviceInfo.description.c_str());

      // Connection status
      bool connected = deviceInfo.device && deviceInfo.device->isConnected();
      ImGui::Text("Status: %s", connected ? "Connected" : "Disconnected");

      if (connected) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "●");
      }
      else {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "●");
      }

      // Connection control
      if (!connected) {
        if (ImGui::Button(("Connect##" + name).c_str())) {
          if (deviceInfo.device->connect()) {
            LogMessage("INFO", "Connected device: " + name);
          }
        }
      }
      else {
        if (ImGui::Button(("Disconnect##" + name).c_str())) {
          deviceInfo.device->disconnect();
          LogMessage("INFO", "Disconnected device: " + name);
        }

        // Device controls (only if connected)
        static std::unordered_map<std::string, bool> outputStates;
        static std::unordered_map<std::string, float> voltageSettings;
        static std::unordered_map<std::string, float> currentSettings;

        // Initialize states if not present
        if (outputStates.find(name) == outputStates.end()) {
          outputStates[name] = false;
          voltageSettings[name] = 0.0f;
          currentSettings[name] = 0.5f;
        }

        if (ImGui::Checkbox(("Output Enable##" + name).c_str(), &outputStates[name])) {
          deviceInfo.device->setOutput(1, outputStates[name]);
        }

        if (ImGui::SliderFloat(("Voltage (V)##" + name).c_str(), &voltageSettings[name], 0.0f, 30.0f)) {
          deviceInfo.device->setVoltage(1, voltageSettings[name]);
        }

        if (ImGui::SliderFloat(("Current Limit (A)##" + name).c_str(), &currentSettings[name], 0.0f, 5.0f)) {
          deviceInfo.device->setCurrent(1, currentSettings[name]);
        }

        // Measurements display
        try {
          auto voltage = deviceInfo.device->getVoltage(1);
          auto current = deviceInfo.device->getCurrent(1);
          if (voltage.has_value() && current.has_value()) {
            ImGui::Text("Measured: %.3fV, %.3fA", voltage.value(), current.value());
          }
        }
        catch (...) {
          ImGui::Text("Measurement read error");
        }
      }

      ImGui::Unindent();
    }
#endif
  }

  std::vector<std::string> SPDPowerSupplyManager::GetAvailableSPDDevices() const {
    std::vector<std::string> devices;

#ifdef _WIN32
    ViSession defaultRM;
    ViFindList findList;
    ViChar instrDesc[VI_FIND_BUFLEN];
    ViUInt32 nmatches;

    // Initialize VISA
    if (viOpenDefaultRM(&defaultRM) == VI_SUCCESS) {
      // Set 3 second timeout for all VISA operations
      viSetAttribute(defaultRM, VI_ATTR_TMO_VALUE, 3000);

      // Search for SPD devices using different patterns
      std::vector<std::string> searchPatterns = {
          "USB?*SPD*::INSTR",
          "TCPIP?*::?*::5555::SOCKET",
          "*SPD*",
          "USB?*F4EC*::INSTR"  // Siglent vendor ID
      };

      for (const auto& pattern : searchPatterns) {
        // Set timeout for find operations
        ViStatus status = viFindRsrc(defaultRM, const_cast<ViChar*>(pattern.c_str()),
          &findList, &nmatches, instrDesc);

        if (status == VI_SUCCESS) {
          devices.push_back(std::string(instrDesc));

          // Get additional matches
          for (ViUInt32 i = 1; i < nmatches; i++) {
            if (viFindNext(findList, instrDesc) == VI_SUCCESS) {
              devices.push_back(std::string(instrDesc));
            }
          }
          viClose(findList);
        }
        else if (status == VI_ERROR_TMO) {
          LogMessage("WARNING", "VISA discovery timeout for pattern: " + pattern);
          break; // Stop searching if we hit timeout
        }
      }

      viClose(defaultRM);
    }
#endif

    // Remove duplicates
    std::sort(devices.begin(), devices.end());
    devices.erase(std::unique(devices.begin(), devices.end()), devices.end());

    return devices;
  }

  void SPDPowerSupplyManager::StartAsyncDiscovery(bool connectImmediately) {
    if (m_discoveryInProgress.load()) {
      LogMessage("WARNING", "Discovery already in progress");
      return;
    }

    m_discoveryInProgress.store(true);
    m_discoveryComplete.store(false);

    // Launch discovery in a separate thread
    m_discoveryThread = std::thread([this, connectImmediately]() {
      LogMessage("INFO", "Starting async device discovery...");

      try {
        int discovered = AddDiscoveredDevices(connectImmediately);
        LogMessage("INFO", "Async discovery completed. Found " + std::to_string(discovered) + " devices");
      }
      catch (const std::exception& e) {
        LogMessage("ERROR", "Exception during async discovery: " + std::string(e.what()));
      }

      m_discoveryInProgress.store(false);
      m_discoveryComplete.store(true);
    });

    // Detach the thread so it runs independently
    m_discoveryThread.detach();
  }

  bool SPDPowerSupplyManager::ValidateDeviceConfig(const nlohmann::json & config) const {
    // Check required fields
    if (!config.contains("name") || !config.contains("resourceString")) {
      LogMessage("WARNING", "Device config missing required fields (name, resourceString)");
      return false;
    }

    // Check field types
    if (!config["name"].is_string() || !config["resourceString"].is_string()) {
      LogMessage("WARNING", "Device config has invalid field types");
      return false;
    }

    return true;
  }

  // === Callback Management ===


  void SPDPowerSupplyManager::SetStatusUpdateCallback(
    std::function<void(const std::string&, const std::string&)> callback) {

    std::cout << "[SPD DEBUG] SetStatusUpdateCallback called!" << std::endl;
    std::cout << "[SPD DEBUG] Callback address: " << &callback << std::endl;
    std::cout << "[SPD DEBUG] Callback is null: " << (callback == nullptr ? "YES" : "NO") << std::endl;

    m_statusUpdateCallback = callback;

    // Verify it was set
    bool isSet = (m_statusUpdateCallback != nullptr);
    std::cout << "[SPD DEBUG] Callback stored successfully: " << (isSet ? "YES" : "NO") << std::endl;

    LogMessage("INFO", "Status update callback " + std::string(isSet ? "set" : "cleared"));
  }

  void SPDPowerSupplyManager::SetConnectionStateCallback(
    std::function<void(const std::string&, bool)> callback) {
    m_connectionStateCallback = callback;
  }

  // === Mode-Specific Operations ===

  bool SPDPowerSupplyManager::SetConstantVoltageMode(double voltage, double currentLimit) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    bool allSuccess = true;
    int operationCount = 0;

    for (const auto& pair : m_devices) {
      const std::string& name = pair.first;
      auto& deviceInfo = pair.second;

      if (deviceInfo->device && deviceInfo->device->isConnected()) {
        try {
          // Set voltage first (CV mode target)
          bool voltageSet = deviceInfo->device->setVoltage(1, voltage);
          // Set current limit (compliance)
          bool currentSet = deviceInfo->device->setCurrent(1, currentLimit);

          if (voltageSet && currentSet) {
            operationCount++;
            LogMessage("INFO", "Set " + name + " to CV mode: " +
              std::to_string(voltage) + "V, " + std::to_string(currentLimit) + "A limit");
          }
          else {
            LogMessage("WARNING", "Failed to set CV mode for device: " + name);
            allSuccess = false;
          }
        }
        catch (const std::exception& e) {
          LogMessage("ERROR", "Exception setting CV mode for device " + name + ": " + e.what());
          allSuccess = false;
        }
      }
    }

    LogMessage("INFO", "Set CV mode (" + std::to_string(voltage) + "V, " +
      std::to_string(currentLimit) + "A limit) on " + std::to_string(operationCount) + " devices");

    return allSuccess && operationCount > 0;
  }

  bool SPDPowerSupplyManager::SetConstantCurrentMode(double current, double voltageLimit) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    bool allSuccess = true;
    int operationCount = 0;

    for (const auto& pair : m_devices) {
      const std::string& name = pair.first;
      auto& deviceInfo = pair.second;

      if (deviceInfo->device && deviceInfo->device->isConnected()) {
        try {
          // Set current first (CC mode target)
          bool currentSet = deviceInfo->device->setCurrent(1, current);
          // Set voltage limit (compliance)
          bool voltageSet = deviceInfo->device->setVoltage(1, voltageLimit);

          if (currentSet && voltageSet) {
            operationCount++;
            LogMessage("INFO", "Set " + name + " to CC mode: " +
              std::to_string(current) + "A, " + std::to_string(voltageLimit) + "V limit");
          }
          else {
            LogMessage("WARNING", "Failed to set CC mode for device: " + name);
            allSuccess = false;
          }
        }
        catch (const std::exception& e) {
          LogMessage("ERROR", "Exception setting CC mode for device " + name + ": " + e.what());
          allSuccess = false;
        }
      }
    }

    LogMessage("INFO", "Set CC mode (" + std::to_string(current) + "A, " +
      std::to_string(voltageLimit) + "V limit) on " + std::to_string(operationCount) + " devices");

    return allSuccess && operationCount > 0;
  }

  void SPDPowerSupplyManager::Subscribe(ISPDStatusSubscriber* subscriber, const std::string& subscriberId) {
    std::lock_guard<std::mutex> lock(m_subscribersMutex);

    if (m_subscribers.find(subscriberId) != m_subscribers.end()) {
      LogMessage("WARNING", "Subscriber ID already exists, replacing: " + subscriberId);
    }

    m_subscribers[subscriberId] = subscriber;
    LogMessage("INFO", "Subscriber registered: " + subscriberId);
  }

  void SPDPowerSupplyManager::Unsubscribe(const std::string& subscriberId) {
    std::lock_guard<std::mutex> lock(m_subscribersMutex);

    auto it = m_subscribers.find(subscriberId);
    if (it != m_subscribers.end()) {
      m_subscribers.erase(it);
      LogMessage("INFO", "Subscriber unregistered: " + subscriberId);
    }
  }

  std::vector<std::string> SPDPowerSupplyManager::GetSubscriberIds() const {
    std::lock_guard<std::mutex> lock(m_subscribersMutex);

    std::vector<std::string> ids;
    ids.reserve(m_subscribers.size());

    for (const auto& pair : m_subscribers) {
      ids.push_back(pair.first);
    }

    return ids;
  }

  // Add these to SPDPowerSupplyManager.cpp

// === Sweep Operations Implementation ===

  bool SPDPowerSupplyManager::PerformVoltageSweep(const std::string& deviceName, int channel,
    double startV, double stopV, int steps,
    double currentLimit, double delayMs,
    std::vector<SPDSweepResult>& results) {
    LogMessage("INFO", "Starting voltage sweep on device '" + deviceName + "' from " +
      std::to_string(startV) + "V to " + std::to_string(stopV) + "V");

    // Get device
    SPDPowerSupply* device = GetDevice(deviceName);
    if (!device) {
      SetError("Device '" + deviceName + "' not found for voltage sweep");
      return false;
    }

    // Check if connected
    if (!device->isConnected()) {
      SetError("Device '" + deviceName + "' is not connected for voltage sweep");
      return false;
    }

    // Perform sweep using device's method
    bool success = device->voltageSweep(channel, startV, stopV, steps, currentLimit, delayMs, results);

    if (success) {
      LogMessage("INFO", "Voltage sweep completed successfully on '" + deviceName + "' with " +
        std::to_string(results.size()) + " data points");
    }
    else {
      SetError("Voltage sweep failed on device '" + deviceName + "'");
    }

    return success;
  }

  bool SPDPowerSupplyManager::PerformCurrentSweep(const std::string& deviceName, int channel,
    double startA, double stopA, int steps,
    double voltageLimit, double delayMs,
    std::vector<SPDSweepResult>& results) {
    LogMessage("INFO", "Starting current sweep on device '" + deviceName + "' from " +
      std::to_string(startA) + "A to " + std::to_string(stopA) + "A");

    // Get device
    SPDPowerSupply* device = GetDevice(deviceName);
    if (!device) {
      SetError("Device '" + deviceName + "' not found for current sweep");
      return false;
    }

    // Check if connected
    if (!device->isConnected()) {
      SetError("Device '" + deviceName + "' is not connected for current sweep");
      return false;
    }

    // Perform sweep using device's method
    bool success = device->currentSweep(channel, startA, stopA, steps, voltageLimit, delayMs, results);

    if (success) {
      LogMessage("INFO", "Current sweep completed successfully on '" + deviceName + "' with " +
        std::to_string(results.size()) + " data points");
    }
    else {
      SetError("Current sweep failed on device '" + deviceName + "'");
    }

    return success;
  }

  int SPDPowerSupplyManager::PerformVoltageSweepAll(int channel, double startV, double stopV, int steps,
    double currentLimit, double delayMs,
    std::unordered_map<std::string, std::vector<SPDSweepResult>>& allResults) {
    LogMessage("INFO", "Starting voltage sweep on all connected devices from " +
      std::to_string(startV) + "V to " + std::to_string(stopV) + "V");

    std::lock_guard<std::mutex> lock(m_devicesMutex);

    allResults.clear();
    int successCount = 0;

    // Iterate through all devices
    for (const auto& pair : m_devices) {
      const std::string& deviceName = pair.first;
      const auto& deviceInfo = pair.second;

      // Skip if not connected
      if (!deviceInfo->device || !deviceInfo->device->isConnected()) {
        LogMessage("WARNING", "Skipping voltage sweep on '" + deviceName + "' - not connected");
        continue;
      }

      // Perform sweep on this device
      std::vector<SPDSweepResult> deviceResults;
      bool success = deviceInfo->device->voltageSweep(channel, startV, stopV, steps,
        currentLimit, delayMs, deviceResults);

      if (success) {
        allResults[deviceName] = std::move(deviceResults);
        successCount++;
        LogMessage("INFO", "Voltage sweep succeeded on '" + deviceName + "' with " +
          std::to_string(allResults[deviceName].size()) + " points");
      }
      else {
        LogMessage("ERROR", "Voltage sweep failed on '" + deviceName + "'");
        // Add empty results vector to indicate this device was attempted
        allResults[deviceName] = std::vector<SPDSweepResult>();
      }
    }

    LogMessage("INFO", "Voltage sweep completed on " + std::to_string(successCount) +
      " out of " + std::to_string(m_devices.size()) + " devices");

    return successCount;
  }

  int SPDPowerSupplyManager::PerformCurrentSweepAll(int channel, double startA, double stopA, int steps,
    double voltageLimit, double delayMs,
    std::unordered_map<std::string, std::vector<SPDSweepResult>>& allResults) {
    LogMessage("INFO", "Starting current sweep on all connected devices from " +
      std::to_string(startA) + "A to " + std::to_string(stopA) + "A");

    std::lock_guard<std::mutex> lock(m_devicesMutex);

    allResults.clear();
    int successCount = 0;

    // Iterate through all devices
    for (const auto& pair : m_devices) {
      const std::string& deviceName = pair.first;
      const auto& deviceInfo = pair.second;

      // Skip if not connected
      if (!deviceInfo->device || !deviceInfo->device->isConnected()) {
        LogMessage("WARNING", "Skipping current sweep on '" + deviceName + "' - not connected");
        continue;
      }

      // Perform sweep on this device
      std::vector<SPDSweepResult> deviceResults;
      bool success = deviceInfo->device->currentSweep(channel, startA, stopA, steps,
        voltageLimit, delayMs, deviceResults);

      if (success) {
        allResults[deviceName] = std::move(deviceResults);
        successCount++;
        LogMessage("INFO", "Current sweep succeeded on '" + deviceName + "' with " +
          std::to_string(allResults[deviceName].size()) + " points");
      }
      else {
        LogMessage("ERROR", "Current sweep failed on '" + deviceName + "'");
        // Add empty results vector to indicate this device was attempted
        allResults[deviceName] = std::vector<SPDSweepResult>();
      }
    }

    LogMessage("INFO", "Current sweep completed on " + std::to_string(successCount) +
      " out of " + std::to_string(m_devices.size()) + " devices");

    return successCount;
  }


  // Add these method implementations to SPDPowerSupplyManager.cpp:

  bool SPDPowerSupplyManager::ReadVoltage(const std::string& deviceName, int channel, double& voltage) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    auto it = m_devices.find(deviceName);
    if (it == m_devices.end()) {
      SetError("Device '" + deviceName + "' not found");
      LogMessage("ERROR", "ReadVoltage: Device '" + deviceName + "' not found");
      return false;
    }

    auto& deviceInfo = it->second;
    if (!deviceInfo->device || !deviceInfo->device->isConnected()) {
      SetError("Device '" + deviceName + "' is not connected");
      LogMessage("ERROR", "ReadVoltage: Device '" + deviceName + "' is not connected");
      return false;
    }

    try {
      auto voltageOpt = deviceInfo->device->getVoltage(channel);
      if (voltageOpt.has_value()) {
        voltage = voltageOpt.value();
        LogMessage("DEBUG", "ReadVoltage: " + deviceName + " CH" + std::to_string(channel) +
          " = " + std::to_string(voltage) + "V");
        return true;
      }
      else {
        SetError("Failed to read voltage from device '" + deviceName + "' channel " + std::to_string(channel));
        LogMessage("ERROR", "ReadVoltage: Failed to read voltage from " + deviceName + " CH" + std::to_string(channel));
        return false;
      }
    }
    catch (const std::exception& e) {
      SetError("Exception reading voltage from '" + deviceName + "': " + e.what());
      LogMessage("ERROR", "ReadVoltage: Exception for " + deviceName + ": " + e.what());
      return false;
    }
  }

  bool SPDPowerSupplyManager::ReadCurrent(const std::string& deviceName, int channel, double& current) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    auto it = m_devices.find(deviceName);
    if (it == m_devices.end()) {
      SetError("Device '" + deviceName + "' not found");
      LogMessage("ERROR", "ReadCurrent: Device '" + deviceName + "' not found");
      return false;
    }

    auto& deviceInfo = it->second;
    if (!deviceInfo->device || !deviceInfo->device->isConnected()) {
      SetError("Device '" + deviceName + "' is not connected");
      LogMessage("ERROR", "ReadCurrent: Device '" + deviceName + "' is not connected");
      return false;
    }

    try {
      auto currentOpt = deviceInfo->device->getCurrent(channel);
      if (currentOpt.has_value()) {
        current = currentOpt.value();
        LogMessage("DEBUG", "ReadCurrent: " + deviceName + " CH" + std::to_string(channel) +
          " = " + std::to_string(current) + "A");
        return true;
      }
      else {
        SetError("Failed to read current from device '" + deviceName + "' channel " + std::to_string(channel));
        LogMessage("ERROR", "ReadCurrent: Failed to read current from " + deviceName + " CH" + std::to_string(channel));
        return false;
      }
    }
    catch (const std::exception& e) {
      SetError("Exception reading current from '" + deviceName + "': " + e.what());
      LogMessage("ERROR", "ReadCurrent: Exception for " + deviceName + ": " + e.what());
      return false;
    }
  }

  bool SPDPowerSupplyManager::IsOutputEnabled(const std::string& deviceName, int channel, bool& outputEnabled) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    auto it = m_devices.find(deviceName);
    if (it == m_devices.end()) {
      SetError("Device '" + deviceName + "' not found");
      LogMessage("ERROR", "IsOutputEnabled: Device '" + deviceName + "' not found");
      return false;
    }

    auto& deviceInfo = it->second;
    if (!deviceInfo->device || !deviceInfo->device->isConnected()) {
      SetError("Device '" + deviceName + "' is not connected");
      LogMessage("ERROR", "IsOutputEnabled: Device '" + deviceName + "' is not connected");
      return false;
    }

    try {
      auto outputOpt = deviceInfo->device->getOutputState(channel);
      if (outputOpt.has_value()) {
        outputEnabled = outputOpt.value();
        LogMessage("DEBUG", "IsOutputEnabled: " + deviceName + " CH" + std::to_string(channel) +
          " = " + (outputEnabled ? "ON" : "OFF"));
        return true;
      }
      else {
        SetError("Failed to read output state from device '" + deviceName + "' channel " + std::to_string(channel));
        LogMessage("ERROR", "IsOutputEnabled: Failed to read output state from " + deviceName + " CH" + std::to_string(channel));
        return false;
      }
    }
    catch (const std::exception& e) {
      SetError("Exception reading output state from '" + deviceName + "': " + e.what());
      LogMessage("ERROR", "IsOutputEnabled: Exception for " + deviceName + ": " + e.what());
      return false;
    }
  }

  bool SPDPowerSupplyManager::ReadVoltageAndCurrent(const std::string& deviceName, int channel, double& voltage, double& current) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    auto it = m_devices.find(deviceName);
    if (it == m_devices.end()) {
      SetError("Device '" + deviceName + "' not found");
      LogMessage("ERROR", "ReadVoltageAndCurrent: Device '" + deviceName + "' not found");
      return false;
    }

    auto& deviceInfo = it->second;
    if (!deviceInfo->device || !deviceInfo->device->isConnected()) {
      SetError("Device '" + deviceName + "' is not connected");
      LogMessage("ERROR", "ReadVoltageAndCurrent: Device '" + deviceName + "' is not connected");
      return false;
    }

    try {
      auto voltageOpt = deviceInfo->device->getVoltage(channel);
      auto currentOpt = deviceInfo->device->getCurrent(channel);

      if (voltageOpt.has_value() && currentOpt.has_value()) {
        voltage = voltageOpt.value();
        current = currentOpt.value();
        LogMessage("DEBUG", "ReadVoltageAndCurrent: " + deviceName + " CH" + std::to_string(channel) +
          " = " + std::to_string(voltage) + "V, " + std::to_string(current) + "A");
        return true;
      }
      else {
        SetError("Failed to read voltage/current from device '" + deviceName + "' channel " + std::to_string(channel));
        LogMessage("ERROR", "ReadVoltageAndCurrent: Failed to read measurements from " + deviceName + " CH" + std::to_string(channel));
        return false;
      }
    }
    catch (const std::exception& e) {
      SetError("Exception reading voltage/current from '" + deviceName + "': " + e.what());
      LogMessage("ERROR", "ReadVoltageAndCurrent: Exception for " + deviceName + ": " + e.what());
      return false;
    }
  }

  // Note: Also need to add GetConnectedDeviceCount() method if it doesn't exist:
  int SPDPowerSupplyManager::GetConnectedDeviceCount() const {
    return GetConnectedCount(); // This method already exists as GetConnectedCount()
  }

  bool SPDPowerSupplyManager::SetOutput(const std::string& deviceName, int channel, bool enable) {
    LogMessage("INFO", std::string(enable ? "Enabling" : "Disabling") +
      " output on device '" + deviceName + "' channel " + std::to_string(channel));

    SPDPowerSupply* device = GetDevice(deviceName);
    if (!device) {
      SetError("Device '" + deviceName + "' not found for output control");
      return false;
    }

    if (!device->isConnected()) {
      SetError("Device '" + deviceName + "' is not connected");
      return false;
    }

    return device->setOutput(channel, enable);
  }

  bool SPDPowerSupplyManager::SetConstantVoltageMode(const std::string& deviceName,
    double voltage, double currentLimit) {
    LogMessage("INFO", "Setting CV mode on device '" + deviceName + "': " +
      std::to_string(voltage) + "V, " + std::to_string(currentLimit) + "A limit");

    SPDPowerSupply* device = GetDevice(deviceName);
    if (!device) {
      SetError("Device '" + deviceName + "' not found for CV mode");
      return false;
    }

    if (!device->isConnected()) {
      SetError("Device '" + deviceName + "' is not connected");
      return false;
    }

    return device->setConstantVoltageMode(1, voltage, currentLimit);
  }

  bool SPDPowerSupplyManager::SetConstantCurrentMode(const std::string& deviceName,
    double current, double voltageLimit) {
    LogMessage("INFO", "Setting CC mode on device '" + deviceName + "': " +
      std::to_string(current) + "A, " + std::to_string(voltageLimit) + "V limit");

    SPDPowerSupply* device = GetDevice(deviceName);
    if (!device) {
      SetError("Device '" + deviceName + "' not found for CC mode");
      return false;
    }

    if (!device->isConnected()) {
      SetError("Device '" + deviceName + "' is not connected");
      return false;
    }

    return device->setConstantCurrentMode(1, current, voltageLimit);
  }