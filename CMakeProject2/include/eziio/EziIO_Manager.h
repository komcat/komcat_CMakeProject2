#pragma once

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include "FAS_EziMOTIONPlusE.h"
#include "EziIO_Observer.h"

enum class EziIOError {
  SUCCESS = 0,
  DEVICE_NOT_FOUND,
  DEVICE_DISCONNECTED,
  OPERATION_FAILED
};

class EziIODevice : public EziIOPublisher {
public:
  EziIODevice(int id, const std::string& name, const std::string& ip,
    int inputCount, int outputCount);
  ~EziIODevice();

  bool connect();
  bool disconnect();
  bool isConnected() const { return m_isConnected; }

  // Input operations
  bool readInputs(uint32_t& inputs, uint32_t& latch);
  bool getLastInputStatus(uint32_t& inputs, uint32_t& latch);
  bool clearLatch(uint32_t latchMask);

  // Output operations
  bool getOutputs(uint32_t& outputs, uint32_t& status);
  bool getLastOutputStatus(uint32_t& outputs, uint32_t& status);
  bool setOutputs(uint32_t setMask, uint32_t clearMask);
  bool setOutput(int outputPin, bool state);

  // Info getters
  int getDeviceId() const { return m_deviceId; }
  std::string getName() const { return m_name; }
  std::string getIpAddress() const { return m_ipAddress; }
  int getInputCount() const { return m_inputCount; }
  int getOutputCount() const { return m_outputCount; }

  // Enable/disable change detection
  void setChangeDetectionEnabled(bool enabled) { m_changeDetectionEnabled = enabled; }
  bool isChangeDetectionEnabled() const { return m_changeDetectionEnabled; }

private:
  // Device specific pin masks based on output pin count
  static const uint32_t OUTPUT_PIN_MASKS_16[16];
  static const uint32_t OUTPUT_PIN_MASKS_8[8];

  uint32_t getOutputPinMask(int pin) const;

  int m_deviceId;
  std::string m_name;
  std::string m_ipAddress;
  int m_inputCount;
  int m_outputCount;
  bool m_isConnected;
  uint8_t m_ipBytes[4]; // IP address as bytes
  const uint32_t* m_currentOutputMasks; // Reference to the appropriate mask array

  // Status tracking
  std::mutex m_statusMutex;
  uint32_t m_lastInputs;
  uint32_t m_lastLatch;
  uint32_t m_lastOutputs;
  uint32_t m_lastOutputStatus;
  bool m_statusUpdated;

  // Observer pattern support
  bool m_changeDetectionEnabled;
  uint32_t m_previousInputs;
  uint32_t m_previousOutputs;
  bool m_firstRead;

  // Helper methods for change detection
  void checkAndNotifyInputChanges(uint32_t newInputs);
  void checkAndNotifyOutputChanges(uint32_t newOutputs);
  uint32_t getCurrentTimestamp() const;
};

class EziIOManager : public EziIOPublisher {
public:
  EziIOManager();
  ~EziIOManager();

  bool initialize();
  void shutdown();

  bool addDevice(int id, const std::string& name, const std::string& ip,
    int inputCount, int outputCount);
  bool connectAll();
  bool disconnectAll();

  bool connectDevice(int deviceId);
  bool disconnectDevice(int deviceId);

  // Input operations - now return EziIOError
  EziIOError readInputs(int deviceId, uint32_t& inputs, uint32_t& latch);
  EziIOError getLastInputStatus(int deviceId, uint32_t& inputs, uint32_t& latch);

  // Output operations - now return EziIOError
  EziIOError getOutputs(int deviceId, uint32_t& outputs, uint32_t& status);
  EziIOError getLastOutputStatus(int deviceId, uint32_t& outputs, uint32_t& status);
  EziIOError setOutputs(int deviceId, uint32_t setMask, uint32_t clearMask);
  EziIOError setOutput(int deviceId, int outputPin, bool state);

  // Polling thread control
  void startPolling(unsigned int intervalMs = 100);
  void stopPolling();
  void setPollingInterval(unsigned int intervalMs);
  bool isPolling() const;

  // Utility
  EziIODevice* getDevice(int deviceId);
  EziIODevice* getDeviceByName(const std::string& name);

  const std::vector<std::shared_ptr<EziIODevice>>& getDevices() const { return m_devices; }
  int getConnectedDeviceCount() const { return m_connectedDeviceCount; }

  void setLogging(bool enable) { m_enableLogging = enable; }

  // Helper to get error message
  static std::string getErrorString(EziIOError error);

  // Observer pattern - subscribe to specific device
  void subscribeToDevice(int deviceId, std::shared_ptr<IEziIOObserver> observer);
  void subscribeCallbackToDevice(int deviceId, std::function<void(const PinChangeEvent&)> callback);

  // Subscribe to all devices
  void subscribeToAllDevices(std::shared_ptr<IEziIOObserver> observer);
  void subscribeCallbackToAllDevices(std::function<void(const PinChangeEvent&)> callback);

  // Enable/disable change detection for all devices
  void setChangeDetectionEnabled(bool enabled);

private:
  bool m_enableLogging = true;
  std::vector<std::shared_ptr<EziIODevice>> m_devices;
  std::map<int, std::shared_ptr<EziIODevice>> m_deviceMap;
  std::map<std::string, std::shared_ptr<EziIODevice>> m_deviceNameMap;
  bool m_initialized;
  std::atomic<int> m_connectedDeviceCount{ 0 };

  // Threading
  std::thread* m_pollingThread;
  std::atomic<bool> m_stopPolling;
  unsigned int m_pollingInterval;

  // Thread function
  void pollingThreadFunc();

  // Helper method for validation
  EziIOError validateDevice(int deviceId, EziIODevice** device, const std::string& operation);
};