// PowerSupplyManager.cpp
#include "PowerSupplyManager.h"
#include <chrono>

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