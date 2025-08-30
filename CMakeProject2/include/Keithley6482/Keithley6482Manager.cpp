#include "Keithley6482Manager.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Keithley {

  // === Subscriber Management ===

  // Subscribe to updates
  bool Keithley6482Manager::Subscribe(const std::string& subscriberName,
    std::shared_ptr<IK6482MeasurementSubscriber> subscriber) {
    std::lock_guard<std::mutex> lock(m_subscribersMutex);

    if (m_subscribers.find(subscriberName) != m_subscribers.end()) {
      SetError("Subscriber '" + subscriberName + "' already exists");
      return false;
    }

    m_subscribers[subscriberName] = subscriber;
    std::cout << "Subscriber '" << subscriberName << "' added to Keithley6482Manager" << std::endl;

    // Notify new subscriber of current device states
    auto statuses = GetAllStatuses();
    for (const auto& [deviceName, status] : statuses) {
      try {
        subscriber->OnDeviceStatusUpdate(status);
        subscriber->OnDeviceConnectionChange(deviceName, status.isConnected);
      }
      catch (const std::exception& e) {
        std::cerr << "Exception notifying new subscriber: " << e.what() << std::endl;
      }
    }

    return true;
  }

  // Unsubscribe from updates
  bool Keithley6482Manager::Unsubscribe(const std::string& subscriberName) {
    std::lock_guard<std::mutex> lock(m_subscribersMutex);

    auto it = m_subscribers.find(subscriberName);
    if (it != m_subscribers.end()) {
      m_subscribers.erase(it);
      std::cout << "Subscriber '" << subscriberName << "' removed from Keithley6482Manager" << std::endl;
      return true;
    }

    SetError("Subscriber '" + subscriberName + "' not found");
    return false;
  }

  // Get subscriber names
  std::vector<std::string> Keithley6482Manager::GetSubscriberNames() const {
    std::lock_guard<std::mutex> lock(m_subscribersMutex);

    std::vector<std::string> names;
    names.reserve(m_subscribers.size());

    for (const auto& [name, subscriber] : m_subscribers) {
      names.push_back(name);
    }

    return names;
  }

  // Get subscriber count
  size_t Keithley6482Manager::GetSubscriberCount() const {
    std::lock_guard<std::mutex> lock(m_subscribersMutex);
    return m_subscribers.size();
  }

  // === Notification Methods ===

  // Notify measurement update
  void Keithley6482Manager::NotifyMeasurementUpdate(const K6482MeasurementData& data) {
    std::lock_guard<std::mutex> lock(m_subscribersMutex);

    for (const auto& [name, subscriber] : m_subscribers) {
      try {
        subscriber->OnMeasurementUpdate(data);
      }
      catch (const std::exception& e) {
        std::cerr << "Exception notifying subscriber '" << name
          << "' of measurement update: " << e.what() << std::endl;
      }
    }
  }

  // Notify device status update
  void Keithley6482Manager::NotifyDeviceStatusUpdate(const K6482DeviceStatus& status) {
    std::lock_guard<std::mutex> lock(m_subscribersMutex);

    for (const auto& [name, subscriber] : m_subscribers) {
      try {
        subscriber->OnDeviceStatusUpdate(status);
      }
      catch (const std::exception& e) {
        std::cerr << "Exception notifying subscriber '" << name
          << "' of status update: " << e.what() << std::endl;
      }
    }
  }

  // Notify connection change
  void Keithley6482Manager::NotifyConnectionChange(const std::string& deviceName, bool connected) {
    std::lock_guard<std::mutex> lock(m_subscribersMutex);

    for (const auto& [name, subscriber] : m_subscribers) {
      try {
        subscriber->OnDeviceConnectionChange(deviceName, connected);
      }
      catch (const std::exception& e) {
        std::cerr << "Exception notifying subscriber '" << name
          << "' of connection change: " << e.what() << std::endl;
      }
    }
  }

  // Notify polling started
  void Keithley6482Manager::NotifyPollingStarted(const std::string& deviceName, int intervalMs) {
    std::lock_guard<std::mutex> lock(m_subscribersMutex);

    for (const auto& [name, subscriber] : m_subscribers) {
      try {
        subscriber->OnPollingStarted(deviceName, intervalMs);
      }
      catch (const std::exception& e) {
        std::cerr << "Exception notifying subscriber '" << name
          << "' of polling start: " << e.what() << std::endl;
      }
    }
  }

  // Notify polling stopped
  void Keithley6482Manager::NotifyPollingStopped(const std::string& deviceName) {
    std::lock_guard<std::mutex> lock(m_subscribersMutex);

    for (const auto& [name, subscriber] : m_subscribers) {
      try {
        subscriber->OnPollingStopped(deviceName);
      }
      catch (const std::exception& e) {
        std::cerr << "Exception notifying subscriber '" << name
          << "' of polling stop: " << e.what() << std::endl;
      }
    }
  }

  // Constructor
  Keithley6482Manager::Keithley6482Manager()
    : m_pollingActive(false)
    , m_pollingInterval(100) {
  }

  // Destructor
  Keithley6482Manager::~Keithley6482Manager() {
    StopAllPolling();
    DisconnectAll();
  }

  // Add device
  bool Keithley6482Manager::AddDevice(const std::string& name, const std::string& resourceString) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    // Check if device already exists
    if (m_devices.find(name) != m_devices.end()) {
      SetError("Device '" + name + "' already exists");
      return false;
    }

    try {
      auto deviceInfo = std::make_unique<K6482DeviceInfo>();
      deviceInfo->device = std::make_unique<Keithley6482>(resourceString);
      deviceInfo->resourceString = resourceString;
      deviceInfo->description = "Keithley 6482 Picoammeter";
      deviceInfo->autoConnect = false;
      deviceInfo->pollingInterval = 100;

      m_devices[name] = std::move(deviceInfo);

      std::cout << "Added device: " << name << " (" << resourceString << ")" << std::endl;
      return true;

    }
    catch (const std::exception& e) {
      SetError("Failed to add device '" + name + "': " + std::string(e.what()));
      return false;
    }
  }

  // Get device
  Keithley6482* Keithley6482Manager::GetDevice(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    auto it = m_devices.find(name);
    if (it != m_devices.end()) {
      return it->second->device.get();
    }

    return nullptr;
  }

  // Remove device
  bool Keithley6482Manager::RemoveDevice(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    auto it = m_devices.find(name);
    if (it != m_devices.end()) {
      // Disconnect if connected
      if (it->second->device && it->second->device->isConnected()) {
        it->second->device->disconnect();
      }

      m_devices.erase(it);

      std::cout << "Removed device: " << name << std::endl;

      // Notify callback
      if (m_connectionStateCallback) {
        m_connectionStateCallback(name, false);
      }

      return true;
    }

    SetError("Device '" + name + "' not found");
    return false;
  }

  // Get device names
  std::vector<std::string> Keithley6482Manager::GetDeviceNames() const {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    std::vector<std::string> names;
    names.reserve(m_devices.size());

    for (const auto& pair : m_devices) {
      names.push_back(pair.first);
    }

    return names;
  }

  // Get connected count
  int Keithley6482Manager::GetConnectedCount() const {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    int count = 0;
    for (const auto& pair : m_devices) {
      if (pair.second->device && pair.second->device->isConnected()) {
        count++;
      }
    }

    return count;
  }

  // Connect all
  int Keithley6482Manager::ConnectAll() {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    int connected = 0;

    for (auto& pair : m_devices) {
      const std::string& name = pair.first;
      auto& deviceInfo = pair.second;

      if (deviceInfo->device && !deviceInfo->device->isConnected()) {
        try {
          if (deviceInfo->device->connect(deviceInfo->resourceString)) {
            connected++;
            std::cout << "Connected device: " << name << std::endl;

            // Notify subscribers of connection
            NotifyConnectionChange(name, true);

            // Notify callback if set (legacy support)
            if (m_connectionStateCallback) {
              m_connectionStateCallback(name, true);
            }
          }
          else {
            std::cerr << "Failed to connect device: " << name << std::endl;
          }
        }
        catch (const std::exception& e) {
          std::cerr << "Exception connecting device " << name << ": " << e.what() << std::endl;
        }
      }
    }

    std::cout << "Connected " << connected << " devices" << std::endl;
    return connected;
  }

  // Disconnect all
  void Keithley6482Manager::DisconnectAll() {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    for (auto& pair : m_devices) {
      const std::string& name = pair.first;
      auto& deviceInfo = pair.second;

      if (deviceInfo->device && deviceInfo->device->isConnected()) {
        try {
          deviceInfo->device->disconnect();
          std::cout << "Disconnected device: " << name << std::endl;

          // Notify subscribers of disconnection
          NotifyConnectionChange(name, false);

          // Notify callback if set (legacy support)
          if (m_connectionStateCallback) {
            m_connectionStateCallback(name, false);
          }
        }
        catch (const std::exception& e) {
          std::cerr << "Exception disconnecting device " << name << ": " << e.what() << std::endl;
        }
      }
    }
  }

  // Are all connected
  bool Keithley6482Manager::AreAllConnected() const {
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

  // Discover devices
  int Keithley6482Manager::DiscoverDevices(bool connectImmediately) {
    std::cout << "Starting device discovery..." << std::endl;

    std::vector<std::string> resources = FindKeithleyDevices();
    int added = 0;

    for (const auto& resourceString : resources) {
      // Generate unique name
      std::string deviceName = "K6482_" + std::to_string(added + 1);

      // Check if already exists
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
          std::cout << "Discovered device: " << deviceName << " (" << resourceString << ")" << std::endl;

          if (connectImmediately) {
            auto device = GetDevice(deviceName);
            if (device && device->connect()) {
              std::cout << "Connected to: " << deviceName << std::endl;
            }
          }
        }
      }
    }

    std::cout << "Discovery complete. Found " << added << " devices" << std::endl;
    return added;
  }

  // Set all source voltages
  bool Keithley6482Manager::SetAllSourceVoltages(int channel, double voltage) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    bool allSuccess = true;

    for (const auto& pair : m_devices) {
      if (pair.second->device && pair.second->device->isConnected()) {
        if (!pair.second->device->setSourceVoltage(channel, voltage)) {
          allSuccess = false;
          std::cerr << "Failed to set voltage for device: " << pair.first << std::endl;
        }
      }
    }

    return allSuccess;
  }

  // Enable all source voltages
  bool Keithley6482Manager::EnableAllSourceVoltages(int channel, bool enable) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    bool allSuccess = true;

    for (const auto& pair : m_devices) {
      if (pair.second->device && pair.second->device->isConnected()) {
        if (!pair.second->device->enableSourceVoltage(channel, enable)) {
          allSuccess = false;
          std::cerr << "Failed to enable voltage for device: " << pair.first << std::endl;
        }
      }
    }

    return allSuccess;
  }

  // Set all current ranges
  bool Keithley6482Manager::SetAllCurrentRanges(int channel, double range) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    bool allSuccess = true;

    for (const auto& pair : m_devices) {
      if (pair.second->device && pair.second->device->isConnected()) {
        if (!pair.second->device->setCurrentRange(channel, range)) {
          allSuccess = false;
          std::cerr << "Failed to set range for device: " << pair.first << std::endl;
        }
      }
    }

    return allSuccess;
  }

  // Set all auto range
  bool Keithley6482Manager::SetAllAutoRange(int channel, bool enable) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    bool allSuccess = true;

    for (const auto& pair : m_devices) {
      if (pair.second->device && pair.second->device->isConnected()) {
        if (!pair.second->device->setAutoRange(channel, enable)) {
          allSuccess = false;
          std::cerr << "Failed to set auto range for device: " << pair.first << std::endl;
        }
      }
    }

    return allSuccess;
  }

  // Reset all
  bool Keithley6482Manager::ResetAll() {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    bool allSuccess = true;

    for (const auto& pair : m_devices) {
      if (pair.second->device && pair.second->device->isConnected()) {
        if (!pair.second->device->reset()) {
          allSuccess = false;
          std::cerr << "Failed to reset device: " << pair.first << std::endl;
        }
      }
    }

    return allSuccess;
  }

  // Start all polling
  void Keithley6482Manager::StartAllPolling(int intervalMs) {
    if (m_pollingActive.load()) {
      return;
    }

    m_pollingInterval.store(intervalMs);

    // Notify subscribers that polling is starting
    {
      std::lock_guard<std::mutex> lock(m_devicesMutex);
      for (const auto& [deviceName, deviceInfo] : m_devices) {
        if (deviceInfo->device && deviceInfo->device->isConnected()) {
          NotifyPollingStarted(deviceName, intervalMs);
        }
      }
    }

    // Start polling on each device
    {
      std::lock_guard<std::mutex> lock(m_devicesMutex);
      for (auto& pair : m_devices) {
        if (pair.second->device && pair.second->device->isConnected()) {
          pair.second->device->startPolling(intervalMs);
        }
      }
    }

    // Start manager polling thread
    m_pollingActive.store(true);
    m_pollingThread = std::thread(&Keithley6482Manager::PollingThreadFunction, this);

    std::cout << "Started polling with interval: " << intervalMs << "ms" << std::endl;
  }

  // Stop all polling
  void Keithley6482Manager::StopAllPolling() {
    if (!m_pollingActive.load()) {
      return;
    }

    m_pollingActive.store(false);

    // Wait for polling thread
    if (m_pollingThread.joinable()) {
      m_pollingThread.join();
    }

    // Stop device polling and notify subscribers
    {
      std::lock_guard<std::mutex> lock(m_devicesMutex);
      for (auto& [deviceName, deviceInfo] : m_devices) {
        if (deviceInfo->device) {
          deviceInfo->device->stopPolling();
          NotifyPollingStopped(deviceName);
        }
      }
    }

    std::cout << "Stopped polling" << std::endl;
  }

  // Get all statuses
  std::unordered_map<std::string, K6482DeviceStatus> Keithley6482Manager::GetAllStatuses() const {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    std::unordered_map<std::string, K6482DeviceStatus> statuses;

    for (const auto& pair : m_devices) {
      const std::string& name = pair.first;
      const auto& deviceInfo = pair.second;

      K6482DeviceStatus status;
      status.deviceName = name;
      status.isConnected = deviceInfo->device && deviceInfo->device->isConnected();
      status.timestamp = std::chrono::steady_clock::now();

      if (status.isConnected) {
        auto measurement = deviceInfo->device->getLatestMeasurement();
        if (measurement) {
          status.channel1Current = measurement->channel1_current;
          status.channel2Current = measurement->channel2_current;
          status.channel1Voltage = measurement->channel1_voltage;
          status.channel2Voltage = measurement->channel2_voltage;
        }
        else {
          // Try direct read if no cached measurement
          auto ch1 = deviceInfo->device->readCurrent(1);
          auto ch2 = deviceInfo->device->readCurrent(2);
          status.channel1Current = ch1.value_or(0.0);
          status.channel2Current = ch2.value_or(0.0);

          auto v1 = deviceInfo->device->getSourceVoltage(1);
          auto v2 = deviceInfo->device->getSourceVoltage(2);
          status.channel1Voltage = v1.value_or(0.0);
          status.channel2Voltage = v2.value_or(0.0);
        }
      }
      else {
        status.channel1Current = 0.0;
        status.channel2Current = 0.0;
        status.channel1Voltage = 0.0;
        status.channel2Voltage = 0.0;
      }

      statuses[name] = status;
    }

    return statuses;
  }

  // Read all currents
  std::unordered_map<std::string, double> Keithley6482Manager::ReadAllCurrents(int channel) const {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    std::unordered_map<std::string, double> currents;

    for (const auto& pair : m_devices) {
      if (pair.second->device && pair.second->device->isConnected()) {
        auto current = pair.second->device->readCurrent(channel);
        if (current) {
          currents[pair.first] = *current;
        }
      }
    }

    return currents;
  }

  // Initialize from config file
  bool Keithley6482Manager::Initialize(const std::string& configFile) {
    std::cout << "Initializing Keithley6482Manager from: " << configFile << std::endl;

    // Check if config file exists
    std::ifstream checkFile(configFile);
    if (!checkFile.is_open()) {
      std::cout << "Config file not found. Creating default configuration..." << std::endl;

      // Generate default config file
      if (!GenerateDefaultConfigFile(configFile)) {
        std::cerr << "Failed to generate default config file" << std::endl;
        LoadDefaultConfiguration();
        return false;
      }
    }
    checkFile.close();

    // Load the configuration
    if (!LoadConfiguration(configFile)) {
      std::cerr << "Failed to load configuration. Using defaults." << std::endl;
      LoadDefaultConfiguration();
      return false;
    }

    return true;
  }

  // Load default configuration
  void Keithley6482Manager::LoadDefaultConfiguration() {
    std::cout << "Loading default Keithley6482 configuration..." << std::endl;

    // Clear existing devices
    DisconnectAll();
    m_devices.clear();

    // Add a default device
    AddDevice("Keithley6482_1", "GPIB0::1::INSTR");

    // Set default polling interval
    m_pollingInterval.store(100);
  }

  // Generate default config file
  bool Keithley6482Manager::GenerateDefaultConfigFile(const std::string& configFile) const {
    try {
      json config;

      // Manager settings
      config["keithley6482Manager"] = {
          {"name", "Keithley6482Manager"},
          {"autoConnect", false},
          {"defaultPollingInterval", 100},
          {"autoDiscovery", false},
          {"enableDataLogging", true}
      };

      // Default devices array
      json devices = json::array();

      // Add example devices
      json device1 = {
          {"name", "Keithley6482_1"},
          {"resourceString", "GPIB0::1::INSTR"},
          {"description", "Primary Keithley 6482 Picoammeter"},
          {"autoConnect", true},
          {"pollingInterval", 100},
          {"settings", {
              {"autoRange", true},
              {"integrationTime", 1.0},
              {"filterEnabled", true},
              {"filterCount", 10},
              {"channel1", {
                  {"enabled", true},
                  {"range", "AUTO"},
                  {"sourceVoltage", 0.0},
                  {"sourceEnabled", false}
              }},
              {"channel2", {
                  {"enabled", true},
                  {"range", "AUTO"},
                  {"sourceVoltage", 0.0},
                  {"sourceEnabled", false}
              }}
          }}
      };
      devices.push_back(device1);

      // Add a second device example (commented out in actual use)
      json device2 = {
          {"name", "Keithley6482_2"},
          {"resourceString", "GPIB0::2::INSTR"},
          {"description", "Secondary Keithley 6482 Picoammeter"},
          {"autoConnect", false},
          {"pollingInterval", 100},
          {"settings", {
              {"autoRange", true},
              {"integrationTime", 1.0},
              {"filterEnabled", false},
              {"filterCount", 10},
              {"channel1", {
                  {"enabled", true},
                  {"range", "20e-9"},  // 20nA range
                  {"sourceVoltage", 5.0},
                  {"sourceEnabled", true}
              }},
              {"channel2", {
                  {"enabled", true},
                  {"range", "20e-6"},  // 20uA range
                  {"sourceVoltage", 0.0},
                  {"sourceEnabled", false}
              }}
          }}
      };
      devices.push_back(device2);

      config["devices"] = devices;

      // Global settings
      config["globalSettings"] = {
          {"enableSourceVoltageOnConnect", false},
          {"resetOnConnect", true},
          {"logAllMeasurements", false},
          {"measurementTimeout", 5000},
          {"dataFormat", "CSV"},
          {"currentRanges", {
              "2nA", "20nA", "200nA",
              "2uA", "20uA", "200uA",
              "2mA", "20mA"
          }}
      };

      // Write to file
      std::ofstream file(configFile);
      if (!file.is_open()) {
        SetError("Failed to create config file: " + configFile);
        return false;
      }

      file << config.dump(4);  // Pretty print with 4-space indent
      file.close();

      std::cout << "Generated default config file: " << configFile << std::endl;
      return true;

    }
    catch (const std::exception& e) {
      SetError("Failed to generate config file: " + std::string(e.what()));
      return false;
    }
  }

  // Load configuration
  bool Keithley6482Manager::LoadConfiguration(const std::string& configFile) {
    try {
      std::ifstream file(configFile);
      if (!file.is_open()) {
        SetError("Failed to open config file: " + configFile);
        return false;
      }

      json config;
      file >> config;
      file.close();

      // Clear existing devices
      DisconnectAll();
      m_devices.clear();

      // Load manager settings
      if (config.contains("keithley6482Manager")) {
        const auto& mgr = config["keithley6482Manager"];

        if (mgr.contains("defaultPollingInterval")) {
          m_pollingInterval.store(mgr["defaultPollingInterval"]);
        }
      }

      // Load devices
      if (config.contains("devices") && config["devices"].is_array()) {
        for (const auto& deviceConfig : config["devices"]) {
          std::string name = deviceConfig["name"];
          std::string resourceString = deviceConfig["resourceString"];

          std::cout << "Adding device: " << name << " (" << resourceString << ")" << std::endl;

          if (AddDevice(name, resourceString)) {
            auto& deviceInfo = m_devices[name];

            // Load device-specific settings
            if (deviceConfig.contains("autoConnect")) {
              deviceInfo->autoConnect = deviceConfig["autoConnect"];
            }
            if (deviceConfig.contains("pollingInterval")) {
              deviceInfo->pollingInterval = deviceConfig["pollingInterval"];
            }
            if (deviceConfig.contains("description")) {
              deviceInfo->description = deviceConfig["description"];
            }

            // Auto-connect if specified
            if (deviceInfo->autoConnect) {
              if (deviceInfo->device->connect()) {
                std::cout << "Auto-connected to: " << name << std::endl;

                // Apply device settings if connected
                if (deviceConfig.contains("settings")) {
                  const auto& settings = deviceConfig["settings"];

                  // Apply integration time
                  if (settings.contains("integrationTime")) {
                    deviceInfo->device->setIntegrationTime(settings["integrationTime"]);
                  }

                  // Apply filter settings
                  if (settings.contains("filterEnabled") && settings.contains("filterCount")) {
                    bool filterEnabled = settings["filterEnabled"];
                    int filterCount = settings["filterCount"];
                    deviceInfo->device->setFilter(1, filterEnabled, filterCount);
                    deviceInfo->device->setFilter(2, filterEnabled, filterCount);
                  }

                  // Apply channel 1 settings
                  if (settings.contains("channel1")) {
                    const auto& ch1 = settings["channel1"];

                    if (ch1.contains("range")) {
                      std::string range = ch1["range"];
                      if (range == "AUTO") {
                        deviceInfo->device->setAutoRange(1, true);
                      }
                      else {
                        try {
                          double rangeValue = std::stod(range);
                          deviceInfo->device->setCurrentRange(1, rangeValue);
                        }
                        catch (...) {
                          deviceInfo->device->setAutoRange(1, true);
                        }
                      }
                    }

                    if (ch1.contains("sourceVoltage")) {
                      deviceInfo->device->setSourceVoltage(1, ch1["sourceVoltage"]);
                    }

                    if (ch1.contains("sourceEnabled")) {
                      deviceInfo->device->enableSourceVoltage(1, ch1["sourceEnabled"]);
                    }
                  }

                  // Apply channel 2 settings
                  if (settings.contains("channel2")) {
                    const auto& ch2 = settings["channel2"];

                    if (ch2.contains("range")) {
                      std::string range = ch2["range"];
                      if (range == "AUTO") {
                        deviceInfo->device->setAutoRange(2, true);
                      }
                      else {
                        try {
                          double rangeValue = std::stod(range);
                          deviceInfo->device->setCurrentRange(2, rangeValue);
                        }
                        catch (...) {
                          deviceInfo->device->setAutoRange(2, true);
                        }
                      }
                    }

                    if (ch2.contains("sourceVoltage")) {
                      deviceInfo->device->setSourceVoltage(2, ch2["sourceVoltage"]);
                    }

                    if (ch2.contains("sourceEnabled")) {
                      deviceInfo->device->enableSourceVoltage(2, ch2["sourceEnabled"]);
                    }
                  }
                }

                // Notify callback
                if (m_connectionStateCallback) {
                  m_connectionStateCallback(name, true);
                }
              }
              else {
                std::cerr << "Failed to auto-connect to: " << name << std::endl;
              }
            }
          }
        }
      }

      std::cout << "Loaded " << m_devices.size() << " devices from configuration" << std::endl;
      return true;

    }
    catch (const std::exception& e) {
      SetError("Failed to load configuration: " + std::string(e.what()));
      return false;
    }
  }

  // Save configuration
  bool Keithley6482Manager::SaveConfiguration(const std::string& configFile) const {
    try {
      json config;
      json devices = json::array();

      {
        std::lock_guard<std::mutex> lock(m_devicesMutex);

        for (const auto& pair : m_devices) {
          json deviceConfig = {
              {"name", pair.first},
              {"resourceString", pair.second->resourceString},
              {"description", pair.second->description},
              {"autoConnect", pair.second->autoConnect},
              {"pollingInterval", pair.second->pollingInterval}
          };
          devices.push_back(deviceConfig);
        }
      }

      config["devices"] = devices;

      std::ofstream file(configFile);
      if (!file.is_open()) {
        SetError("Failed to open config file for writing: " + configFile);
        return false;
      }

      file << config.dump(4);

      std::cout << "Saved configuration to: " << configFile << std::endl;
      return true;

    }
    catch (const std::exception& e) {
      SetError("Failed to save configuration: " + std::string(e.what()));
      return false;
    }
  }

  // === Private Helper Methods ===

  // Set error
  void Keithley6482Manager::SetError(const std::string& error) const {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_lastError = error;
    std::cerr << "Keithley6482Manager Error: " << error << std::endl;
  }

  // Polling thread function
  void Keithley6482Manager::PollingThreadFunction() {
    std::cout << "Manager polling thread started" << std::endl;

    while (m_pollingActive.load()) {
      try {
        auto statuses = GetAllStatuses();

        // Notify subscribers with detailed measurement data
        for (const auto& [deviceName, status] : statuses) {
          // Notify device status
          NotifyDeviceStatusUpdate(status);

          // Create and notify channel 1 measurement
          K6482MeasurementData ch1Data;
          ch1Data.deviceName = deviceName;
          ch1Data.channel = 1;
          ch1Data.current = status.channel1Current;
          ch1Data.voltage = status.channel1Voltage;
          ch1Data.currentRange = "AUTO";  // Could be enhanced to get actual range
          ch1Data.timestamp = status.timestamp;
          NotifyMeasurementUpdate(ch1Data);

          // Create and notify channel 2 measurement
          K6482MeasurementData ch2Data;
          ch2Data.deviceName = deviceName;
          ch2Data.channel = 2;
          ch2Data.current = status.channel2Current;
          ch2Data.voltage = status.channel2Voltage;
          ch2Data.currentRange = "AUTO";
          ch2Data.timestamp = status.timestamp;
          NotifyMeasurementUpdate(ch2Data);
        }

        // Also notify legacy callbacks if set
        if (m_statusUpdateCallback) {
          for (const auto& pair : statuses) {
            m_statusUpdateCallback(pair.first, pair.second);
          }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(m_pollingInterval.load()));

      }
      catch (const std::exception& e) {
        SetError("Polling exception: " + std::string(e.what()));
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      }
    }

    std::cout << "Manager polling thread ended" << std::endl;
  }

  // Find Keithley devices
  std::vector<std::string> Keithley6482Manager::FindKeithleyDevices() const {
    std::vector<std::string> keithleyDevices;

    // Get all VISA resources
    std::vector<std::string> allResources = Keithley6482::scanAvailableResources();

    // Check each resource
    for (const auto& resource : allResources) {
      try {
        Keithley6482 temp(resource);
        if (temp.connect()) {
          std::string idn = temp.getInstrumentID();

          // Check if it's a Keithley 6482
          if (idn.find("KEITHLEY") != std::string::npos &&
            idn.find("6482") != std::string::npos) {
            keithleyDevices.push_back(resource);
            std::cout << "Found Keithley 6482: " << resource << std::endl;
          }

          temp.disconnect();
        }
      }
      catch (const std::exception& e) {
        // Skip this resource
      }
    }

    return keithleyDevices;
  }

} // namespace Keithley