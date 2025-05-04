// machine_operations.h
#pragma once

#include "include/motions/motion_control_layer.h"
#include "include/motions/pi_controller_manager.h"
#include "include/eziio/EziIO_Manager.h"
#include "include/eziio/PneumaticManager.h"
#include "include/data/global_data_store.h"
#include "include/scanning/scanning_algorithm.h"
#include "include/logger.h"
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <memory>

class MachineOperations {
public:
  MachineOperations(
    MotionControlLayer& motionLayer,
    PIControllerManager& piControllerManager,
    EziIOManager& ioManager,
    PneumaticManager& pneumaticManager
  );

  ~MachineOperations();

  // Motion control methods
  bool MoveDeviceToNode(const std::string& deviceName, const std::string& graphName,
    const std::string& targetNodeId, bool blocking = true);

  bool MovePathFromTo(const std::string& deviceName, const std::string& graphName,
    const std::string& startNodeId, const std::string& endNodeId,
    bool blocking = true);

  // IO control methods
  bool SetOutput(const std::string& deviceName, int outputPin, bool state);
  bool SetOutput(int deviceId, int outputPin, bool state);
  bool ReadInput(const std::string& deviceName, int inputPin, bool& state);
  bool ReadInput(int deviceId, int inputPin, bool& state);
  bool ClearLatch(const std::string& deviceName, int inputPin);
  bool ClearLatch(int deviceId, uint32_t latchMask);

  // Pneumatic control methods
  bool ExtendSlide(const std::string& slideName, bool waitForCompletion = true, int timeoutMs = 5000);
  bool RetractSlide(const std::string& slideName, bool waitForCompletion = true, int timeoutMs = 5000);
  SlideState GetSlideState(const std::string& slideName);
  bool WaitForSlideState(const std::string& slideName, SlideState targetState, int timeoutMs = 5000);

  // Utility methods
  void Wait(int milliseconds);
  float ReadDataValue(const std::string& dataId, float defaultValue = 0.0f);
  bool HasDataValue(const std::string& dataId);

  // Scanning methods
  bool PerformScan(const std::string& deviceName, const std::string& dataChannel,
    const std::vector<double>& stepSizes, int settlingTimeMs,
    const std::vector<std::string>& axesToScan = { "Z", "X", "Y" });

  // Device status methods
  bool IsDeviceConnected(const std::string& deviceName);
  bool IsSlideExtended(const std::string& slideName);
  bool IsSlideRetracted(const std::string& slideName);
  bool IsSlideMoving(const std::string& slideName);
  bool IsSlideInError(const std::string& slideName);

  // Helper method to get EziIO device ID from name
  int GetDeviceId(const std::string& deviceName);

private:
  MotionControlLayer& m_motionLayer;
  PIControllerManager& m_piControllerManager;
  EziIOManager& m_ioManager;
  PneumaticManager& m_pneumaticManager;
  Logger* m_logger;

  // Helper methods
  bool ConvertPinStateToBoolean(uint32_t inputs, int pin);
};