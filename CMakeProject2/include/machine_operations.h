// machine_operations.h
#pragma once

#include "include/motions/motion_control_layer.h"
#include "include/motions/pi_controller_manager.h"
#include "include/eziio/EziIO_Manager.h"
#include "include/eziio/PneumaticManager.h"
#include "include/data/global_data_store.h"
#include "include/scanning/scanning_algorithm.h"
#include "include/logger.h"
#include "include/camera/pylon_camera_test.h"
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <memory>
// Forward declare CLD101xOperations instead of including the header
class CLD101xOperations;  // Forward declaration


class MachineOperations {
public:
  MachineOperations(
    MotionControlLayer& motionLayer,
    PIControllerManager& piControllerManager,
    EziIOManager& ioManager,
    PneumaticManager& pneumaticManager,
    CLD101xOperations* laserOps = nullptr,
    PylonCameraTest* cameraTest = nullptr  // Add camera parameter
  );

  ~MachineOperations();

  // Motion control methods
  bool MoveDeviceToNode(const std::string& deviceName, const std::string& graphName,
    const std::string& targetNodeId, bool blocking = true);

  bool MovePathFromTo(const std::string& deviceName, const std::string& graphName,
    const std::string& startNodeId, const std::string& endNodeId,
    bool blocking = true);

  // In the public section of MachineOperations class in machine_operations.h
  bool MoveToPointName(const std::string& deviceName, const std::string& positionName, bool blocking = true);
  bool MoveRelative(const std::string& deviceName, const std::string& axis,
    double distance, bool blocking = true);
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

  // Laser and TEC control methods - add these new methods
  bool LaserOn(const std::string& laserName = "");
  bool LaserOff(const std::string& laserName = "");
  bool TECOn(const std::string& laserName = "");
  bool TECOff(const std::string& laserName = "");
  bool SetLaserCurrent(float current, const std::string& laserName = "");
  bool SetTECTemperature(float temperature, const std::string& laserName = "");
  float GetLaserTemperature(const std::string& laserName = "");
  float GetLaserCurrent(const std::string& laserName = "");
  bool WaitForLaserTemperature(float targetTemp, float tolerance = 0.5f,
    int timeoutMs = 30000, const std::string& laserName = "");


  // Add these to the public section of MachineOperations class:

// Scanning methods
  bool StartScan(const std::string& deviceName, const std::string& dataChannel,
    const std::vector<double>& stepSizes, int settlingTimeMs,
    const std::vector<std::string>& axesToScan = { "Z", "X", "Y" });

  bool StopScan(const std::string& deviceName);

  // Get scan status
  bool IsScanActive(const std::string& deviceName) const;
  double GetScanProgress(const std::string& deviceName) const;
  std::string GetScanStatus(const std::string& deviceName) const;

  // Get scan results
  bool GetScanPeak(const std::string& deviceName, double& value, PositionStruct& position) const;

  bool MachineOperations::SafelyCleanupScanner(const std::string& deviceName);

  bool MachineOperations::WaitForDeviceMotionCompletion(const std::string& deviceName, int timeoutMs);
  bool MachineOperations::IsDeviceMoving(const std::string& deviceName);

  bool MachineOperations::IsDevicePIController(const std::string& deviceName) const;

  // Public logging methods for operation classes to use
  void LogInfo(const std::string& message) const {
    if (m_logger) {
      m_logger->LogInfo("MachineOperations: " + message);
    }
  }

  void LogWarning(const std::string& message) const {
    if (m_logger) {
      m_logger->LogWarning("MachineOperations: " + message);
    }
  }

  void LogError(const std::string& message) const {
    if (m_logger) {
      m_logger->LogError("MachineOperations: " + message);
    }
  }

  // Camera control methods
  bool InitializeCamera();
  bool ConnectCamera();
  bool DisconnectCamera();
  bool StartCameraGrabbing();
  bool StopCameraGrabbing();
  bool IsCameraInitialized() const;
  bool IsCameraConnected() const;
  bool IsCameraGrabbing() const;

  // Camera capture methods
  bool CaptureImageToFile(const std::string& filename = "");
  bool UpdateCameraDisplay(); // Call this from your main loop to update the camera display
  bool MachineOperations::IntegrateCameraWithMotion(PylonCameraTest* cameraTest);

  // Get current position information
  std::string GetDeviceCurrentNode(const std::string& deviceName, const std::string& graphName);
  std::string GetDeviceCurrentPositionName(const std::string& deviceName);
  bool GetDeviceCurrentPosition(const std::string& deviceName, PositionStruct& position);

  // Get distance between positions
  double GetDistanceBetweenPositions(const PositionStruct& pos1, const PositionStruct& pos2,
    bool includeRotation = false);



private:
  MotionControlLayer& m_motionLayer;
  PIControllerManager& m_piControllerManager;
  EziIOManager& m_ioManager;
  PneumaticManager& m_pneumaticManager;
  Logger* m_logger;
  CLD101xOperations* m_laserOps; // Add this member variable

  // Helper methods
  bool ConvertPinStateToBoolean(uint32_t inputs, int pin);

  std::map<std::string, std::unique_ptr<ScanningAlgorithm>> m_activeScans;
  std::mutex m_scanMutex; // For thread safety

  // Scan status information (tracked per device)
  struct ScanInfo {
    std::atomic<bool> isActive{ false };
    std::atomic<double> progress{ 0.0 };
    std::string status;
    mutable std::mutex statusMutex;  // Add mutable here
    double peakValue{ 0.0 };
    PositionStruct peakPosition;
    mutable std::mutex peakMutex;    // Add mutable here
  };
  std::map<std::string, ScanInfo> m_scanInfo;

  // Add camera reference
  PylonCameraTest* m_cameraTest;


};