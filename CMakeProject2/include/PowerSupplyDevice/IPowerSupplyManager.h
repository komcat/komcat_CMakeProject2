// IPowerSupplyManager.h
#ifndef IPOWERSUPPLYMANAGER_H
#define IPOWERSUPPLYMANAGER_H

#include "IPowerSupplyDevice.h"
#include "IResultStorage.h"
#include <memory>
#include <vector>
#include <map>
#include <string>

class IPowerSupplyManager {
public:
  // Device status information
  struct DeviceStatus {
    std::string deviceId;
    bool connected;
    std::map<int, bool> channelOutputOn;  // channel -> on/off status
    IPowerSupplyDevice::Measurement lastMeasurement;
    int lastChannel;
    bool sweepRunning;
    float sweepProgress;
    std::chrono::system_clock::time_point lastUpdated;
  };

  // Batch operation results
  struct BatchOperationResult {
    std::map<std::string, bool> deviceResults;  // deviceId -> success
    int successCount;
    int failureCount;
    std::vector<std::string> errorMessages;
  };

  virtual ~IPowerSupplyManager() = default;

  // ========== Device Management ==========
  virtual bool AddDevice(std::shared_ptr<IPowerSupplyDevice> device, const std::string& uniqueId) = 0;
  virtual bool RemoveDevice(const std::string& deviceId) = 0;
  virtual std::shared_ptr<IPowerSupplyDevice> GetDevice(const std::string& deviceId) = 0;
  virtual std::vector<std::string> GetDeviceIds() const = 0;
  virtual size_t GetDeviceCount() const = 0;
  virtual bool HasDevice(const std::string& deviceId) const = 0;

  // ========== Connection Management ==========
  virtual bool ConnectDevice(const std::string& deviceId) = 0;
  virtual bool DisconnectDevice(const std::string& deviceId) = 0;
  virtual BatchOperationResult ConnectAllDevices() = 0;
  virtual BatchOperationResult DisconnectAllDevices() = 0;
  virtual bool IsDeviceConnected(const std::string& deviceId) const = 0;

  // ========== Status Management ==========
  virtual DeviceStatus GetDeviceStatus(const std::string& deviceId) const = 0;
  virtual std::map<std::string, DeviceStatus> GetAllDeviceStatus() const = 0;
  virtual bool UpdateDeviceStatus(const std::string& deviceId) = 0;
  virtual BatchOperationResult UpdateAllDeviceStatus() = 0;

  // ========== Control Operations ==========
  // Single device operations
  virtual bool SetVoltage(const std::string& deviceId, float voltage, int channel = 1) = 0;
  virtual bool SetCurrent(const std::string& deviceId, float current, int channel = 1) = 0;
  virtual bool TurnOn(const std::string& deviceId, int channel = 1) = 0;
  virtual bool TurnOff(const std::string& deviceId, int channel = 1) = 0;
  virtual bool SetModeConstantVoltage(const std::string& deviceId, int channel = 1) = 0;
  virtual bool SetModeConstantCurrent(const std::string& deviceId, int channel = 1) = 0;

  // Batch operations
  virtual BatchOperationResult TurnOnAll(int channel = 1) = 0;
  virtual BatchOperationResult TurnOffAll(int channel = 1) = 0;
  virtual BatchOperationResult SetVoltageAll(float voltage, int channel = 1) = 0;
  virtual BatchOperationResult SetCurrentAll(float current, int channel = 1) = 0;

  // ========== Read Operations ==========
  virtual float ReadVoltage(const std::string& deviceId, int channel = 1) = 0;
  virtual float ReadCurrent(const std::string& deviceId, int channel = 1) = 0;
  virtual IPowerSupplyDevice::Measurement ReadMeasurement(const std::string& deviceId, int channel = 1) = 0;
  virtual std::map<std::string, IPowerSupplyDevice::Measurement> ReadAllMeasurements(int channel = 1) = 0;

  // ========== Sweep Operations ==========
  virtual bool StartSweep(const std::string& deviceId, const IPowerSupplyDevice::SweepConfig& config) = 0;
  virtual bool StopSweep(const std::string& deviceId) = 0;
  virtual BatchOperationResult StopAllSweeps() = 0;
  virtual bool IsSweepRunning(const std::string& deviceId) const = 0;
  virtual float GetSweepProgress(const std::string& deviceId) const = 0;
  virtual IPowerSupplyDevice::SweepResult GetSweepResults(const std::string& deviceId) const = 0;
  virtual IPowerSupplyDevice::SweepResult ExecuteSweepBlocking(const std::string& deviceId,
    const IPowerSupplyDevice::SweepConfig& config) = 0;

  // ========== Storage Integration ==========
  virtual void SetResultStorage(std::shared_ptr<IResultStorage> storage) = 0;
  virtual std::shared_ptr<IResultStorage> GetResultStorage() const = 0;

  // Store current measurements
  virtual bool StoreCurrentMeasurement(const std::string& deviceId,
    const std::string& label = "",
    int channel = 1) = 0;
  virtual bool StoreAllCurrentMeasurements(const std::string& label = "") = 0;

  // Store sweep results
  virtual bool StoreSweepResults(const std::string& deviceId,
    const std::string& label = "") = 0;

  // Query stored results
  virtual std::vector<IResultStorage::Record> QueryStoredResults(
    const IResultStorage::QueryFilter& filter) const = 0;

  // ========== Utility Operations ==========
  virtual void SetDefaultTimeout(int milliseconds) = 0;
  virtual int GetDefaultTimeout() const = 0;

  // Thread safety mode
  virtual void SetThreadSafe(bool enable) = 0;
  virtual bool IsThreadSafe() const = 0;

  // Error handling
  virtual std::string GetLastError() const = 0;
  virtual void ClearErrors() = 0;
};

#endif // IPOWERSUPPLYMANAGER_H