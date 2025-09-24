// PowerSupplyManager.cpp
#include "PowerSupplyManager.h"
#include "../include/PowerSupplyDevice/Siglent/siglent_power_supply.h" 
#include "../include/PowerSupplyDevice/MockPowerSupplyDevice.h"
#include "../include/PowerSupplyDevice/KeysightE36103B/KeysightE36103B.h"
#include <chrono>
#include "logger.h"
#include <iostream>

// Helper function to log messages (replace with your Logger when available)
static void LogMessage(const std::string& level, const std::string& message) {
  std::cout << "[" << level << "] " << message << std::endl;
}


PowerSupplyManager::~PowerSupplyManager() {
  // Disconnect all devices on destruction
  DisconnectAllDevices();
}

PowerSupplyManager::DeviceEntry* PowerSupplyManager::GetDeviceEntry(const std::string& deviceId) {
  auto it = devices.find(deviceId);
  return (it != devices.end()) ? it->second.get() : nullptr;
}

const PowerSupplyManager::DeviceEntry* PowerSupplyManager::GetDeviceEntry(const std::string& deviceId) const {
  auto it = devices.find(deviceId);
  return (it != devices.end()) ? it->second.get() : nullptr;
}

bool PowerSupplyManager::AddDevice(std::shared_ptr<IPowerSupplyDevice> device, const std::string& uniqueId) {
  if (!device) {
    lastError = "Cannot add null device";
    return false;
  }

  std::lock_guard<std::mutex> lock(managerMutex);

  if (devices.find(uniqueId) != devices.end()) {
    lastError = "Device ID already exists: " + uniqueId;
    return false;
  }

  auto entry = std::make_unique<DeviceEntry>();
  entry->device = device;
  entry->status.deviceId = uniqueId;
  entry->status.connected = false;
  entry->status.sweepRunning = false;
  entry->status.sweepProgress = 0.0f;

  devices[uniqueId] = std::move(entry);
  return true;
}

bool PowerSupplyManager::RemoveDevice(const std::string& deviceId) {
  std::lock_guard<std::mutex> lock(managerMutex);

  auto it = devices.find(deviceId);
  if (it == devices.end()) {
    lastError = "Device not found: " + deviceId;
    return false;
  }

  // Disconnect before removing
  if (it->second->device->IsConnected()) {
    it->second->device->Disconnect();
  }

  devices.erase(it);
  return true;
}

std::shared_ptr<IPowerSupplyDevice> PowerSupplyManager::GetDevice(const std::string& deviceId) {
  std::lock_guard<std::mutex> lock(managerMutex);

  auto entry = GetDeviceEntry(deviceId);
  return entry ? entry->device : nullptr;
}

std::vector<std::string> PowerSupplyManager::GetDeviceIds() const {
  std::lock_guard<std::mutex> lock(managerMutex);

  std::vector<std::string> ids;
  for (const auto& [id, entry] : devices) {
    ids.push_back(id);
  }
  return ids;
}

bool PowerSupplyManager::ConnectDevice(const std::string& deviceId) {
  auto entry = GetDeviceEntry(deviceId);
  if (!entry) {
    lastError = "Device not found: " + deviceId;
    return false;
  }

  std::lock_guard<std::mutex> lock(entry->deviceMutex);

  bool success = entry->device->Connect();
  entry->status.connected = entry->device->IsConnected();
  entry->status.lastUpdated = std::chrono::system_clock::now();

  return success;
}

bool PowerSupplyManager::DisconnectDevice(const std::string& deviceId) {
  auto entry = GetDeviceEntry(deviceId);
  if (!entry) {
    lastError = "Device not found: " + deviceId;
    return false;
  }

  std::lock_guard<std::mutex> lock(entry->deviceMutex);

  bool success = entry->device->Disconnect();
  entry->status.connected = false;
  entry->status.lastUpdated = std::chrono::system_clock::now();

  return success;
}

IPowerSupplyManager::BatchOperationResult PowerSupplyManager::ConnectAllDevices() {
  BatchOperationResult result;
  result.successCount = 0;
  result.failureCount = 0;

  for (const auto& [id, entry] : devices) {
    if (ConnectDevice(id)) {
      result.deviceResults[id] = true;
      result.successCount++;
    }
    else {
      result.deviceResults[id] = false;
      result.failureCount++;
      result.errorMessages.push_back("Failed to connect " + id + ": " + lastError);
    }
  }

  return result;
}

bool PowerSupplyManager::SetVoltage(const std::string& deviceId, float voltage, int channel) {
  auto entry = GetDeviceEntry(deviceId);
  if (!entry) {
    lastError = "Device not found: " + deviceId;
    return false;
  }

  if (threadSafeMode) {
    std::lock_guard<std::mutex> lock(entry->deviceMutex);
    return entry->device->SetVoltage(voltage, channel);
  }
  else {
    return entry->device->SetVoltage(voltage, channel);
  }
}

bool PowerSupplyManager::TurnOn(const std::string& deviceId, int channel) {
  auto entry = GetDeviceEntry(deviceId);
  if (!entry) {
    lastError = "Device not found: " + deviceId;
    return false;
  }

  if (threadSafeMode) {
    std::lock_guard<std::mutex> lock(entry->deviceMutex);
    bool success = entry->device->TurnOn(channel);
    if (success) {
      entry->status.channelOutputOn[channel] = true;
    }
    return success;
  }
  else {
    bool success = entry->device->TurnOn(channel);
    if (success) {
      entry->status.channelOutputOn[channel] = true;
    }
    return success;
  }
}

IPowerSupplyDevice::Measurement PowerSupplyManager::ReadMeasurement(const std::string& deviceId, int channel) {
  auto entry = GetDeviceEntry(deviceId);
  if (!entry) {
    lastError = "Device not found: " + deviceId;
    return { 0.0f, 0.0f };
  }

  if (threadSafeMode) {
    std::lock_guard<std::mutex> lock(entry->deviceMutex);
    auto measurement = entry->device->ReadVoltageCurrent(channel);
    entry->status.lastMeasurement = measurement;
    entry->status.lastChannel = channel;
    entry->status.lastUpdated = std::chrono::system_clock::now();
    return measurement;
  }
  else {
    auto measurement = entry->device->ReadVoltageCurrent(channel);
    entry->status.lastMeasurement = measurement;
    entry->status.lastChannel = channel;
    entry->status.lastUpdated = std::chrono::system_clock::now();
    return measurement;
  }
}

bool PowerSupplyManager::StartSweep(const std::string& deviceId, const IPowerSupplyDevice::SweepConfig& config) {
  auto entry = GetDeviceEntry(deviceId);
  if (!entry) {
    lastError = "Device not found: " + deviceId;
    return false;
  }

  if (threadSafeMode) {
    std::lock_guard<std::mutex> lock(entry->deviceMutex);
    bool success = entry->device->StartSweep(config);
    if (success) {
      entry->status.sweepRunning = true;
      entry->status.sweepProgress = 0.0f;
    }
    return success;
  }
  else {
    bool success = entry->device->StartSweep(config);
    if (success) {
      entry->status.sweepRunning = true;
      entry->status.sweepProgress = 0.0f;
    }
    return success;
  }
}

IPowerSupplyDevice::SweepResult PowerSupplyManager::ExecuteSweepBlocking(
  const std::string& deviceId, const IPowerSupplyDevice::SweepConfig& config) {

  auto entry = GetDeviceEntry(deviceId);
  if (!entry) {
    lastError = "Device not found: " + deviceId;
    IPowerSupplyDevice::SweepResult emptyResult;
    emptyResult.completed = false;
    emptyResult.errorMessage = lastError;
    return emptyResult;
  }

  if (threadSafeMode) {
    std::lock_guard<std::mutex> lock(entry->deviceMutex);
    entry->status.sweepRunning = true;
    auto result = entry->device->ExecuteSweepBlocking(config);
    entry->status.sweepRunning = false;
    entry->status.sweepProgress = 1.0f;
    return result;
  }
  else {
    entry->status.sweepRunning = true;
    auto result = entry->device->ExecuteSweepBlocking(config);
    entry->status.sweepRunning = false;
    entry->status.sweepProgress = 1.0f;
    return result;
  }
}

bool PowerSupplyManager::StoreCurrentMeasurement(const std::string& deviceId, const std::string& label, int channel) {
  if (!storage) {
    lastError = "No storage backend configured";
    return false;
  }

  auto measurement = ReadMeasurement(deviceId, channel);
  auto record = CreateMeasurementRecord(deviceId, measurement, label, channel);

  return storage->Store(record);
}

IResultStorage::Record PowerSupplyManager::CreateMeasurementRecord(
  const std::string& deviceId,
  const IPowerSupplyDevice::Measurement& measurement,
  const std::string& label,
  int channel) {

  IResultStorage::Record record;
  record.deviceId = deviceId;
  record.deviceType = "PowerSupply";
  record.timestamp = std::chrono::system_clock::now();
  record.resultType = "measurement";
  record.label = label;

  record.numericValues["voltage"] = measurement.voltage;
  record.numericValues["current"] = measurement.current;
  record.numericValues["channel"] = channel;

  return record;
}

IResultStorage::Record PowerSupplyManager::CreateSweepRecord(
  const std::string& deviceId,
  const IPowerSupplyDevice::SweepResult& result,
  const std::string& label) {

  IResultStorage::Record record;
  record.deviceId = deviceId;
  record.deviceType = "PowerSupply";
  record.timestamp = std::chrono::system_clock::now();
  record.resultType = "sweep";
  record.label = label;

  // Store sweep config
  record.stringValues["mode"] = (result.config.mode == IPowerSupplyDevice::SweepConfig::Mode::CONSTANT_VOLTAGE)
    ? "CV" : "CC";
  record.numericValues["startValue"] = result.config.startValue;
  record.numericValues["endValue"] = result.config.endValue;
  record.numericValues["stepSize"] = result.config.stepSize;
  record.numericValues["channel"] = result.config.channel;

  // Store measurements
  std::vector<double> voltages, currents;
  for (const auto& m : result.measurements) {
    voltages.push_back(m.voltage);
    currents.push_back(m.current);
  }
  record.arrayValues["voltages"] = voltages;
  record.arrayValues["currents"] = currents;
  record.arrayValues["sweepValues"] = std::vector<double>(result.sweepValues.begin(), result.sweepValues.end());

  return record;
}

// PowerSupplyManager.cpp (additional methods to complete the implementation)

// Continue from where we left off...

bool PowerSupplyManager::HasDevice(const std::string& deviceId) const {
  std::lock_guard<std::mutex> lock(managerMutex);
  return devices.find(deviceId) != devices.end();
}

size_t PowerSupplyManager::GetDeviceCount() const {
  std::lock_guard<std::mutex> lock(managerMutex);
  return devices.size();
}

IPowerSupplyManager::BatchOperationResult PowerSupplyManager::DisconnectAllDevices() {
  BatchOperationResult result;
  result.successCount = 0;
  result.failureCount = 0;

  for (const auto& [id, entry] : devices) {
    if (DisconnectDevice(id)) {
      result.deviceResults[id] = true;
      result.successCount++;
    }
    else {
      result.deviceResults[id] = false;
      result.failureCount++;
      result.errorMessages.push_back("Failed to disconnect " + id + ": " + lastError);
    }
  }

  return result;
}

bool PowerSupplyManager::IsDeviceConnected(const std::string& deviceId) const {
  auto entry = GetDeviceEntry(deviceId);
  if (!entry) {
    return false;
  }

  std::lock_guard<std::mutex> lock(entry->deviceMutex);
  return entry->device->IsConnected();
}

IPowerSupplyManager::DeviceStatus PowerSupplyManager::GetDeviceStatus(const std::string& deviceId) const {
  auto entry = GetDeviceEntry(deviceId);
  if (!entry) {
    return DeviceStatus{};
  }

  std::lock_guard<std::mutex> lock(entry->deviceMutex);
  return entry->status;
}

std::map<std::string, IPowerSupplyManager::DeviceStatus> PowerSupplyManager::GetAllDeviceStatus() const {
  std::map<std::string, DeviceStatus> allStatus;

  for (const auto& [id, entry] : devices) {
    std::lock_guard<std::mutex> lock(entry->deviceMutex);
    allStatus[id] = entry->status;
  }

  return allStatus;
}

bool PowerSupplyManager::UpdateDeviceStatus(const std::string& deviceId) {
  auto entry = GetDeviceEntry(deviceId);
  if (!entry) {
    lastError = "Device not found: " + deviceId;
    return false;
  }

  std::lock_guard<std::mutex> lock(entry->deviceMutex);
  entry->status.connected = entry->device->IsConnected();
  entry->status.sweepRunning = entry->device->IsSweepRunning();
  entry->status.sweepProgress = entry->device->GetSweepProgress();
  entry->status.lastUpdated = std::chrono::system_clock::now();

  return true;
}

IPowerSupplyManager::BatchOperationResult PowerSupplyManager::UpdateAllDeviceStatus() {
  BatchOperationResult result;
  result.successCount = 0;
  result.failureCount = 0;

  for (const auto& [id, entry] : devices) {
    if (UpdateDeviceStatus(id)) {
      result.deviceResults[id] = true;
      result.successCount++;
    }
    else {
      result.deviceResults[id] = false;
      result.failureCount++;
    }
  }

  return result;
}

bool PowerSupplyManager::SetCurrent(const std::string& deviceId, float current, int channel) {
  auto entry = GetDeviceEntry(deviceId);
  if (!entry) {
    lastError = "Device not found: " + deviceId;
    return false;
  }

  if (threadSafeMode) {
    std::lock_guard<std::mutex> lock(entry->deviceMutex);
    return entry->device->SetCurrent(current, channel);
  }
  else {
    return entry->device->SetCurrent(current, channel);
  }
}

bool PowerSupplyManager::TurnOff(const std::string& deviceId, int channel) {
  auto entry = GetDeviceEntry(deviceId);
  if (!entry) {
    lastError = "Device not found: " + deviceId;
    return false;
  }

  if (threadSafeMode) {
    std::lock_guard<std::mutex> lock(entry->deviceMutex);
    bool success = entry->device->TurnOff(channel);
    if (success) {
      entry->status.channelOutputOn[channel] = false;
    }
    return success;
  }
  else {
    bool success = entry->device->TurnOff(channel);
    if (success) {
      entry->status.channelOutputOn[channel] = false;
    }
    return success;
  }
}

bool PowerSupplyManager::SetModeConstantVoltage(const std::string& deviceId, int channel) {
  auto entry = GetDeviceEntry(deviceId);
  if (!entry) {
    lastError = "Device not found: " + deviceId;
    return false;
  }

  if (threadSafeMode) {
    std::lock_guard<std::mutex> lock(entry->deviceMutex);
    return entry->device->SetModeConstantVoltage(channel);
  }
  else {
    return entry->device->SetModeConstantVoltage(channel);
  }
}

bool PowerSupplyManager::SetModeConstantCurrent(const std::string& deviceId, int channel) {
  auto entry = GetDeviceEntry(deviceId);
  if (!entry) {
    lastError = "Device not found: " + deviceId;
    return false;
  }

  if (threadSafeMode) {
    std::lock_guard<std::mutex> lock(entry->deviceMutex);
    return entry->device->SetModeConstantCurrent(channel);
  }
  else {
    return entry->device->SetModeConstantCurrent(channel);
  }
}

IPowerSupplyManager::BatchOperationResult PowerSupplyManager::TurnOnAll(int channel) {
  BatchOperationResult result;
  result.successCount = 0;
  result.failureCount = 0;

  for (const auto& [id, entry] : devices) {
    if (TurnOn(id, channel)) {
      result.deviceResults[id] = true;
      result.successCount++;
    }
    else {
      result.deviceResults[id] = false;
      result.failureCount++;
      result.errorMessages.push_back("Failed to turn on " + id);
    }
  }

  return result;
}

IPowerSupplyManager::BatchOperationResult PowerSupplyManager::TurnOffAll(int channel) {
  BatchOperationResult result;
  result.successCount = 0;
  result.failureCount = 0;

  for (const auto& [id, entry] : devices) {
    if (TurnOff(id, channel)) {
      result.deviceResults[id] = true;
      result.successCount++;
    }
    else {
      result.deviceResults[id] = false;
      result.failureCount++;
      result.errorMessages.push_back("Failed to turn off " + id);
    }
  }

  return result;
}

IPowerSupplyManager::BatchOperationResult PowerSupplyManager::SetVoltageAll(float voltage, int channel) {
  BatchOperationResult result;
  result.successCount = 0;
  result.failureCount = 0;

  for (const auto& [id, entry] : devices) {
    if (SetVoltage(id, voltage, channel)) {
      result.deviceResults[id] = true;
      result.successCount++;
    }
    else {
      result.deviceResults[id] = false;
      result.failureCount++;
    }
  }

  return result;
}

IPowerSupplyManager::BatchOperationResult PowerSupplyManager::SetCurrentAll(float current, int channel) {
  BatchOperationResult result;
  result.successCount = 0;
  result.failureCount = 0;

  for (const auto& [id, entry] : devices) {
    if (SetCurrent(id, current, channel)) {
      result.deviceResults[id] = true;
      result.successCount++;
    }
    else {
      result.deviceResults[id] = false;
      result.failureCount++;
    }
  }

  return result;
}

float PowerSupplyManager::ReadVoltage(const std::string& deviceId, int channel) {
  auto entry = GetDeviceEntry(deviceId);
  if (!entry) {
    lastError = "Device not found: " + deviceId;
    return 0.0f;
  }

  if (threadSafeMode) {
    std::lock_guard<std::mutex> lock(entry->deviceMutex);
    return entry->device->ReadVoltage(channel);
  }
  else {
    return entry->device->ReadVoltage(channel);
  }
}

float PowerSupplyManager::ReadCurrent(const std::string& deviceId, int channel) {
  auto entry = GetDeviceEntry(deviceId);
  if (!entry) {
    lastError = "Device not found: " + deviceId;
    return 0.0f;
  }

  if (threadSafeMode) {
    std::lock_guard<std::mutex> lock(entry->deviceMutex);
    return entry->device->ReadCurrent(channel);
  }
  else {
    return entry->device->ReadCurrent(channel);
  }
}

std::map<std::string, IPowerSupplyDevice::Measurement> PowerSupplyManager::ReadAllMeasurements(int channel) {
  std::map<std::string, IPowerSupplyDevice::Measurement> measurements;

  for (const auto& [id, entry] : devices) {
    measurements[id] = ReadMeasurement(id, channel);
  }

  return measurements;
}

bool PowerSupplyManager::StopSweep(const std::string& deviceId) {
  auto entry = GetDeviceEntry(deviceId);
  if (!entry) {
    lastError = "Device not found: " + deviceId;
    return false;
  }

  if (threadSafeMode) {
    std::lock_guard<std::mutex> lock(entry->deviceMutex);
    bool success = entry->device->StopSweep();
    if (success) {
      entry->status.sweepRunning = false;
    }
    return success;
  }
  else {
    bool success = entry->device->StopSweep();
    if (success) {
      entry->status.sweepRunning = false;
    }
    return success;
  }
}

IPowerSupplyManager::BatchOperationResult PowerSupplyManager::StopAllSweeps() {
  BatchOperationResult result;
  result.successCount = 0;
  result.failureCount = 0;

  for (const auto& [id, entry] : devices) {
    if (entry->device->IsSweepRunning()) {
      if (StopSweep(id)) {
        result.deviceResults[id] = true;
        result.successCount++;
      }
      else {
        result.deviceResults[id] = false;
        result.failureCount++;
      }
    }
  }

  return result;
}

bool PowerSupplyManager::IsSweepRunning(const std::string& deviceId) const {
  auto entry = GetDeviceEntry(deviceId);
  if (!entry) {
    return false;
  }

  std::lock_guard<std::mutex> lock(entry->deviceMutex);
  return entry->device->IsSweepRunning();
}

float PowerSupplyManager::GetSweepProgress(const std::string& deviceId) const {
  auto entry = GetDeviceEntry(deviceId);
  if (!entry) {
    return 0.0f;
  }

  std::lock_guard<std::mutex> lock(entry->deviceMutex);
  return entry->device->GetSweepProgress();
}

IPowerSupplyDevice::SweepResult PowerSupplyManager::GetSweepResults(const std::string& deviceId) const {
  auto entry = GetDeviceEntry(deviceId);
  if (!entry) {
    IPowerSupplyDevice::SweepResult emptyResult;
    emptyResult.completed = false;
    emptyResult.errorMessage = "Device not found";
    return emptyResult;
  }

  std::lock_guard<std::mutex> lock(entry->deviceMutex);
  return entry->device->GetSweepResults();
}

void PowerSupplyManager::SetResultStorage(std::shared_ptr<IResultStorage> newStorage) {
  std::lock_guard<std::mutex> lock(managerMutex);
  storage = newStorage;
}

std::shared_ptr<IResultStorage> PowerSupplyManager::GetResultStorage() const {
  std::lock_guard<std::mutex> lock(managerMutex);
  return storage;
}

bool PowerSupplyManager::StoreAllCurrentMeasurements(const std::string& label) {
  if (!storage) {
    lastError = "No storage backend configured";
    return false;
  }

  bool allSuccess = true;
  auto measurements = ReadAllMeasurements();

  for (const auto& [deviceId, measurement] : measurements) {
    auto record = CreateMeasurementRecord(deviceId, measurement, label, 1);
    if (!storage->Store(record)) {
      allSuccess = false;
    }
  }

  return allSuccess;
}

bool PowerSupplyManager::StoreSweepResults(const std::string& deviceId, const std::string& label) {
  if (!storage) {
    lastError = "No storage backend configured";
    return false;
  }

  auto sweepResult = GetSweepResults(deviceId);
  if (!sweepResult.completed) {
    lastError = "Sweep not completed";
    return false;
  }

  auto record = CreateSweepRecord(deviceId, sweepResult, label);
  return storage->Store(record);
}

std::vector<IResultStorage::Record> PowerSupplyManager::QueryStoredResults(
  const IResultStorage::QueryFilter& filter) const {

  if (!storage) {
    return {};
  }

  return storage->Query(filter);
}


// Add these methods to PowerSupplyManager.cpp:



bool PowerSupplyManager::RunQuickDeviceTest(const std::string& deviceId) {
  auto entry = GetDeviceEntry(deviceId);
  if (!entry || !entry->device->IsConnected()) {
    LogMessage("WARNING", "Device " + deviceId + " not available for testing");
    return false;
  }

  LogMessage("INFO", "Running quick test on " + deviceId + "...");

  // Test parameters
  const float testVoltage = 3.3f;
  const float testCurrentLimit = 0.1f;
  const int testChannel = 1;
  const int stabilizationDelayMs = 1000;

  // Set test parameters
  if (!entry->device->SetVoltage(testVoltage, testChannel)) {
    LogMessage("WARNING", "  Failed to set voltage");
    return false;
  }

  if (!entry->device->SetCurrent(testCurrentLimit, testChannel)) {
    LogMessage("WARNING", "  Failed to set current limit");
    return false;
  }

  LogMessage("INFO", "  Test parameters: " + std::to_string(testVoltage) + "V, " +
    std::to_string(testCurrentLimit * 1000) + "mA limit");

  // Turn on output
  if (!entry->device->TurnOn(testChannel)) {
    LogMessage("WARNING", "  Failed to turn on output");
    return false;
  }

  LogMessage("INFO", "  Output ON - waiting for stabilization...");

  // Wait for stabilization
  std::this_thread::sleep_for(std::chrono::milliseconds(stabilizationDelayMs));

  // Read measurements
  auto measurement = entry->device->ReadVoltageCurrent(testChannel);

  // Format and log results
  char buffer[256];
  snprintf(buffer, sizeof(buffer), "  Measured: %.3f V, %.4f A",
    measurement.voltage, measurement.current);
  LogMessage("INFO", buffer);

  // Verify voltage is within tolerance (±10%)
  float tolerance = testVoltage * 0.1f;
  bool voltageOk = std::abs(measurement.voltage - testVoltage) <= tolerance;

  if (voltageOk) {
    LogMessage("INFO", "  Voltage within tolerance ✓");
  }
  else {
    LogMessage("WARNING", "  Voltage outside tolerance (expected " +
      std::to_string(testVoltage) + "V ±10%)");
  }

  // Turn off output
  entry->device->TurnOff(testChannel);
  LogMessage("INFO", "  Output OFF - Test completed");

  return voltageOk;
}

// Modified Initialize method using the refactored test:
bool PowerSupplyManager::Initialize(const std::string& configFile) {
  // Clear any existing devices
  if (devices.size() > 0) {
    DisconnectAllDevices();
    devices.clear();
  }

  // Check if config file exists
  if (configFile.empty()) {
    LogMessage("WARNING", "No config file specified for PowerSupplyManager");
    return false;
  }

  std::ifstream file(configFile);
  if (!file.is_open()) {
    LogMessage("ERROR", "Power supply config file not found: " + configFile);
    return false;
  }
  file.close();

  // Load configuration
  if (!LoadConfiguration(configFile)) {
    LogMessage("ERROR", "Failed to load power supply configuration from: " + configFile);
    return false;
  }

  LogMessage("INFO", "PowerSupplyManager initialized with " +
    std::to_string(devices.size()) + " devices from: " + configFile);

  // Auto-connect devices and test them
  int connectedCount = 0;
  int testedCount = 0;

  for (const auto& [id, entry] : devices) {
    if (entry->autoConnect) {
      if (ConnectDevice(id)) {
        LogMessage("INFO", "Auto-connected to: " + entry->deviceName + " [" + id + "]");
        connectedCount++;

        // Run quick test on connected device
        if (RunQuickDeviceTest(id)) {
          testedCount++;
        }
      }
      else {
        LogMessage("WARNING", "Failed to auto-connect: " + entry->deviceName + " [" + id + "]");
      }
    }
  }

  // Summary
  if (connectedCount > 0) {
    LogMessage("INFO", "Successfully connected to " + std::to_string(connectedCount) + " devices");
    LogMessage("INFO", std::to_string(testedCount) + "/" + std::to_string(connectedCount) +
      " devices passed quick test");
  }

  return true;
}

bool PowerSupplyManager::LoadConfiguration(const std::string& configFile) {
  try {
    // Read config file
    std::ifstream file(configFile);
    if (!file.is_open()) {
      return false;
    }

    json config;
    file >> config;
    file.close();

    // Load default settings
    if (config.contains("defaultSettings")) {
      auto settings = config["defaultSettings"];

      if (settings.contains("threadSafeMode")) {
        SetThreadSafe(settings["threadSafeMode"].get<bool>());
        LogMessage("DEBUG", "Thread-safe mode: " +
          std::string(IsThreadSafe() ? "enabled" : "disabled"));
      }

      if (settings.contains("defaultTimeoutMs")) {
        int timeout = settings["defaultTimeoutMs"].get<int>();
        SetDefaultTimeout(timeout);
        LogMessage("DEBUG", "Default timeout: " + std::to_string(timeout) + "ms");
      }
    }

    // Load devices
    if (!config.contains("devices")) {
      LogMessage("WARNING", "No devices section in config file");
      return false;
    }

    int successCount = 0;
    int failCount = 0;

    for (const auto& deviceConfig : config["devices"]) {
      try {
        std::string deviceId = deviceConfig["id"].get<std::string>();
        std::string deviceName = deviceConfig["name"].get<std::string>();
        std::string deviceType = deviceConfig["type"].get<std::string>();
        bool autoConnect = deviceConfig.value("autoConnect", false);

        LogMessage("DEBUG", "Processing device: " + deviceName +
          " [Type: " + deviceType + ", ID: " + deviceId + "]");

        // Create device using factory method
        auto device = CreateDeviceFromConfig(deviceConfig);

        if (device) {
          // Create extended device entry
          auto entry = std::make_unique<DeviceEntry>();
          entry->device = device;
          entry->status.deviceId = deviceId;
          entry->status.connected = false;
          entry->status.sweepRunning = false;
          entry->status.sweepProgress = 0.0f;
          entry->autoConnect = autoConnect;
          entry->deviceName = deviceName;

          // Add to manager
          {
            std::lock_guard<std::mutex> lock(managerMutex);
            devices[deviceId] = std::move(entry);
          }

          LogMessage("INFO", "✓ Added device: " + deviceName + " [" + deviceId + "]");
          successCount++;

        }
        else {
          LogMessage("ERROR", "✗ Failed to create device: " + deviceName +
            " (type: " + deviceType + " may not be supported)");
          failCount++;
        }

      }
      catch (const std::exception& e) {
        LogMessage("ERROR", "Exception processing device: " + std::string(e.what()));
        failCount++;
      }
    }

    if (successCount > 0) {
      LogMessage("INFO", "Successfully loaded " + std::to_string(successCount) + " devices");
    }

    if (failCount > 0) {
      LogMessage("WARNING", "Failed to load " + std::to_string(failCount) + " devices");
    }

    return successCount > 0;

  }
  catch (const std::exception& e) {
    LogMessage("ERROR", "Failed to parse power supply config: " + std::string(e.what()));
    lastError = e.what();
    return false;
  }
}




// Replace the CreateDeviceFromConfig method in PowerSupplyManager.cpp with this updated version:

std::shared_ptr<IPowerSupplyDevice> PowerSupplyManager::CreateDeviceFromConfig(const json& deviceConfig) {
  try {
    std::string deviceType = deviceConfig["type"].get<std::string>();
    std::string resourceString = deviceConfig.value("resourceString", "");

    // Create device based on type
    if (deviceType == "Siglent_SPD1305X" || deviceType == "Siglent_SPD3303X") {
      // Create Siglent device
      auto device = std::make_shared<SiglentPowerSupply>(resourceString);

      if (deviceConfig.contains("debugMode")) {
        device->SetDebugMode(deviceConfig["debugMode"].get<bool>());
      }

      LogMessage("INFO", "Created Siglent device: " + deviceConfig.value("name", "Unknown"));
      return device;

    }
    else if (deviceType == "Keysight_E36103B" || deviceType == "Keysight_E36100B") {
      // Create Keysight E36103B device
      auto device = std::make_shared<KeysightE36103B>(resourceString);

      if (deviceConfig.contains("debugMode")) {
        device->SetDebugMode(deviceConfig["debugMode"].get<bool>());
      }

      LogMessage("INFO", "Created Keysight E36103B device: " + deviceConfig.value("name", "Unknown"));
      return device;

    }
    else if (deviceType == "Mock" || deviceType == "Simulated") {
      // Create mock device for testing
      auto device = std::make_shared<MockPowerSupplyDevice>();

      LogMessage("INFO", "Created mock device: " + deviceConfig.value("name", "Unknown"));
      return device;

    }
    else {
      LogMessage("WARNING", "Unknown device type: " + deviceType);
      return nullptr;
    }

  }
  catch (const std::exception& e) {
    LogMessage("ERROR", "Error creating device from config: " + std::string(e.what()));
    return nullptr;
  }
}



std::vector<std::string> PowerSupplyManager::GetDeviceNames() const {
  std::vector<std::string> names;

  std::lock_guard<std::mutex> lock(managerMutex);
  for (const auto& [id, entry] : devices) {
    names.push_back(id);
  }

  return names;
}