// PowerSupplyManager.h
#ifndef POWERSUPPLYMANAGER_H
#define POWERSUPPLYMANAGER_H

#include "PowerSupplyDevice/IPowerSupplyManager.h"
#include <mutex>
#include <atomic>
#include <nlohmann/json.hpp> // For JSON parsing
using json = nlohmann::json;

class PowerSupplyManager : public IPowerSupplyManager {
private:
  // Extended DeviceEntry to store metadata
  struct DeviceEntry {
    std::shared_ptr<IPowerSupplyDevice> device;
    DeviceStatus status;
    mutable std::mutex deviceMutex;

    // Add these new fields:
    bool autoConnect = false;
    std::string deviceName;
  };

  std::map<std::string, std::unique_ptr<DeviceEntry>> devices;
  std::shared_ptr<IResultStorage> storage;

  mutable std::mutex managerMutex;
  std::atomic<bool> threadSafeMode{ true };
  std::string lastError;
  int defaultTimeoutMs = 5000;

  // Helper methods
  DeviceEntry* GetDeviceEntry(const std::string& deviceId);
  const DeviceEntry* GetDeviceEntry(const std::string& deviceId) const;
  void UpdateDeviceStatusInternal(DeviceEntry* entry, int channel = 1);
  IResultStorage::Record CreateMeasurementRecord(const std::string& deviceId,
    const IPowerSupplyDevice::Measurement& measurement,
    const std::string& label, int channel);
  IResultStorage::Record CreateSweepRecord(const std::string& deviceId,
    const IPowerSupplyDevice::SweepResult& result,
    const std::string& label);

  bool RunQuickDeviceTest(const std::string& deviceId);

  // Add this helper method in private section:
  std::shared_ptr<IPowerSupplyDevice> CreateDeviceFromConfig(const nlohmann::json& deviceConfig);


  void DebugPrintDevices() const;

public:
  PowerSupplyManager() = default;
  virtual ~PowerSupplyManager();

  // Initialization methods
  bool Initialize(const std::string& configFile = "");
  bool LoadConfiguration(const std::string& configFile);
  std::vector<std::string> GetDeviceNames() const;


  // Device Management
  bool AddDevice(std::shared_ptr<IPowerSupplyDevice> device, const std::string& uniqueId) override;
  bool RemoveDevice(const std::string& deviceId) override;
  std::shared_ptr<IPowerSupplyDevice> GetDevice(const std::string& deviceId) override;
  std::vector<std::string> GetDeviceIds() const override;
  size_t GetDeviceCount() const override;
  bool HasDevice(const std::string& deviceId) const override;

  // Connection Management
  bool ConnectDevice(const std::string& deviceId) override;
  bool DisconnectDevice(const std::string& deviceId) override;
  BatchOperationResult ConnectAllDevices() override;
  BatchOperationResult DisconnectAllDevices() override;
  bool IsDeviceConnected(const std::string& deviceId) const override;

  // Status Management
  DeviceStatus GetDeviceStatus(const std::string& deviceId) const override;
  std::map<std::string, DeviceStatus> GetAllDeviceStatus() const override;
  bool UpdateDeviceStatus(const std::string& deviceId) override;
  BatchOperationResult UpdateAllDeviceStatus() override;

  // Control Operations
  bool SetVoltage(const std::string& deviceId, float voltage, int channel = 1) override;
  bool SetCurrent(const std::string& deviceId, float current, int channel = 1) override;
  bool TurnOn(const std::string& deviceId, int channel = 1) override;
  bool TurnOff(const std::string& deviceId, int channel = 1) override;
  bool SetModeConstantVoltage(const std::string& deviceId, int channel = 1) override;
  bool SetModeConstantCurrent(const std::string& deviceId, int channel = 1) override;

  // Batch operations
  BatchOperationResult TurnOnAll(int channel = 1) override;
  BatchOperationResult TurnOffAll(int channel = 1) override;
  BatchOperationResult SetVoltageAll(float voltage, int channel = 1) override;
  BatchOperationResult SetCurrentAll(float current, int channel = 1) override;

  // Read Operations
  float ReadVoltage(const std::string& deviceId, int channel = 1) override;
  float ReadCurrent(const std::string& deviceId, int channel = 1) override;
  IPowerSupplyDevice::Measurement ReadMeasurement(const std::string& deviceId, int channel = 1) override;
  std::map<std::string, IPowerSupplyDevice::Measurement> ReadAllMeasurements(int channel = 1) override;

  // Sweep Operations
  bool StartSweep(const std::string& deviceId, const IPowerSupplyDevice::SweepConfig& config) override;
  bool StopSweep(const std::string& deviceId) override;
  BatchOperationResult StopAllSweeps() override;
  bool IsSweepRunning(const std::string& deviceId) const override;
  float GetSweepProgress(const std::string& deviceId) const override;
  IPowerSupplyDevice::SweepResult GetSweepResults(const std::string& deviceId) const override;
  IPowerSupplyDevice::SweepResult ExecuteSweepBlocking(const std::string& deviceId,
    const IPowerSupplyDevice::SweepConfig& config) override;

  // Storage Integration
  void SetResultStorage(std::shared_ptr<IResultStorage> storage) override;
  std::shared_ptr<IResultStorage> GetResultStorage() const override;
  bool StoreCurrentMeasurement(const std::string& deviceId, const std::string& label = "", int channel = 1) override;
  bool StoreAllCurrentMeasurements(const std::string& label = "") override;
  bool StoreSweepResults(const std::string& deviceId, const std::string& label = "") override;
  std::vector<IResultStorage::Record> QueryStoredResults(const IResultStorage::QueryFilter& filter) const override;

  // Utility Operations
  void SetDefaultTimeout(int milliseconds) override { defaultTimeoutMs = milliseconds; }
  int GetDefaultTimeout() const override { return defaultTimeoutMs; }
  void SetThreadSafe(bool enable) override { threadSafeMode = enable; }
  bool IsThreadSafe() const override { return threadSafeMode; }
  std::string GetLastError() const override { return lastError; }
  void ClearErrors() override { lastError.clear(); }
};

#endif // POWERSUPPLYMANAGER_H