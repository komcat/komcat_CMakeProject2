// io_ops.h
#pragma once

#include "include/eziio/EziIO_Manager.h"
#include "include/eziio/PneumaticManager.h"
#include "include/logger.h"
#include "include/data/DatabaseManager.h"
#include "include/data/OperationResultsManager.h"
#include <string>
#include <chrono>
#include <thread>
#include <memory>

class IOOps {
public:
  IOOps(
    EziIOManager& ioManager,
    PneumaticManager& pneumaticManager,
    std::shared_ptr<DatabaseManager> dbManager = nullptr,
    std::shared_ptr<OperationResultsManager> resultsManager = nullptr
  );
  ~IOOps();

  bool Initialize();

  // Digital I/O control methods with caller context tracking
  bool SetOutput(const std::string& deviceName, int outputPin, bool state,
    const std::string& callerContext = "");
  bool SetOutput(int deviceId, int outputPin, bool state);

  bool ReadInput(const std::string& deviceName, int inputPin, bool& state,
    const std::string& callerContext = "");
  bool ReadInput(int deviceId, int inputPin, bool& state);

  bool ClearLatch(const std::string& deviceName, int inputPin,
    const std::string& callerContext = "");
  bool ClearLatch(int deviceId, uint32_t latchMask);

  bool ClearOutput(const std::string& deviceName, int outputPin,
    const std::string& callerContext = "");

  // Pneumatic control methods with caller context tracking
  bool ExtendSlide(const std::string& slideName, bool waitForCompletion = true,
    int timeoutMs = 5000, const std::string& callerContext = "");

  bool RetractSlide(const std::string& slideName, bool waitForCompletion = true,
    int timeoutMs = 5000, const std::string& callerContext = "");

  SlideState GetSlideState(const std::string& slideName);

  bool WaitForSlideState(const std::string& slideName, SlideState targetState,
    int timeoutMs = 5000, const std::string& callerContext = "");

  // Device status methods
  bool IsSlideExtended(const std::string& slideName);
  bool IsSlideRetracted(const std::string& slideName);
  bool IsSlideMoving(const std::string& slideName);
  bool IsSlideInError(const std::string& slideName);

  // Helper method to get EziIO device ID from name
  int GetDeviceId(const std::string& deviceName);

  // Access to underlying managers
  EziIOManager* GetEziIOManager() { return &m_ioManager; }
  PneumaticManager* GetPneumaticManager() { return &m_pneumaticManager; }

  // Logging methods
  void LogInfo(const std::string& message) const;
  void LogWarning(const std::string& message) const;
  void LogError(const std::string& message) const;

private:
  Logger* m_logger;

  // Core system references
  EziIOManager& m_ioManager;
  PneumaticManager& m_pneumaticManager;

  // Database and result tracking
  std::shared_ptr<DatabaseManager> m_dbManager;
  std::shared_ptr<OperationResultsManager> m_resultsManager;

  // Helper methods
  bool ConvertPinStateToBoolean(uint32_t inputs, int pin);
};