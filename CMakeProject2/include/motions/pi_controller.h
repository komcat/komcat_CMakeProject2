// pi_controller.h - Updates to support optimizations
#pragma once
#include <Windows.h>  // Include this first
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <map>
#include "../logger.h"
#include "../motions/MotionTypes.h"

// Include PI GCS2 library
#include "PI_GCS2_DLL.h"

class PIController {
public:
  PIController();
  ~PIController();
  // Toggle verbose debug output
  void SetDebugVerbose(bool enabled) { m_debugVerbose = enabled; }
  bool GetDebugVerbose() const { return m_debugVerbose; }
  // Connection methods
  bool Connect(const std::string& ipAddress, int port = 50000);
  void Disconnect();
  bool IsConnected() const { return m_isConnected; }

  // Basic motion commands
  bool MoveToPosition(const std::string& axis, double position, bool blocking = true);
  bool MoveRelative(const std::string& axis, double distance, bool blocking = true);
  bool HomeAxis(const std::string& axis);
  bool StopAxis(const std::string& axis);
  bool StopAllAxes();

  // Status methods
  bool IsMoving(const std::string& axis);
  bool GetPosition(const std::string& axis, double& position);
  bool GetPositions(std::map<std::string, double>& positions);

  // Servo control
  bool EnableServo(const std::string& axis, bool enable);
  bool IsServoEnabled(const std::string& axis, bool& enabled);

  // Motion configuration
  bool SetVelocity(const std::string& axis, double velocity);
  bool GetVelocity(const std::string& axis, double& velocity);

  // Configuration from MotionDevice
  bool ConfigureFromDevice(const MotionDevice& device);

  // Moving to named positions from MotionTypes
  bool MoveToNamedPosition(const std::string& deviceName, const std::string& positionName);

  // Helper methods
  bool WaitForMotionCompletion(const std::string& axis, double timeoutSeconds = 30.0);

  // UI rendering
  void RenderUI();

  // Window control
  void SetWindowVisible(bool visible) { m_showWindow = visible; }
  void SetWindowTitle(const std::string& title) { m_windowTitle = title; }
  // Add this to the public section of PIController class
  bool MoveToPositionAll(double x, double y, double z, double u, double v, double w, bool blocking = true);

  void RenderJogDistanceControl();

  int GetControllerId() const { return m_controllerId; }
  // Add this to the public section of the PIController class in pi_controller.h
  bool MoveToPositionMultiAxis(const std::vector<std::string>& axes,
    const std::vector<double>& positions,
    bool blocking = true);



  // Add to pi_controller.h in the public section:

/**
 * Starts a scanning procedure to determine the maximum intensity of an analog input signal in a plane.
 * The search consists of two subprocedures:
 * - "Coarse portion" (similar to FSC function)
 * - "Fine portion" (similar to AAP function)
 * The fine portion is only executed when the coarse portion has previously been successfully completed.
 *
 * @param axis1 First axis that defines scanning area (X, Y, or Z). During the coarse portion,
 *              the platform moves in this axis from scanning line to scanning line by the distance given by distance.
 * @param length1 Length of scanning area along axis1 in mm
 * @param axis2 Second axis that defines scanning area (X, Y, or Z). During the coarse portion,
 *              the scanning lines are in this axis.
 * @param length2 Length of scanning area along axis2 in mm
 * @param threshold Intensity threshold of the analog input signal, in V
 * @param distance Distance between the scanning lines in mm, used only during the coarse portion
 * @param alignStep Starting value for the step size in mm, used only during the fine portion
 * @param analogInput Identifier of the analog input signal whose maximum intensity is sought
 * @return TRUE if scan started successfully, FALSE otherwise
 */
  bool FSA(const std::string& axis1, double length1,
    const std::string& axis2, double length2,
    double threshold, double distance,
    double alignStep, int analogInput);

  /**
   * Starts a scanning procedure which scans a specified area ("scanning area") until the analog
   * input signal reaches a specified intensity threshold.
   * The scanning procedure corresponds to the "coarse portion" of the scanning procedure
   * that is started with the FSA function.
   *
   * @param axis1 The axis in which the platform moves from scanning line to scanning line
   *              by the distance given by distance (X, Y, or Z).
   * @param length1 Length of scanning area along axis1 in mm
   * @param axis2 The axis in which the scanning lines are located (X, Y, or Z)
   * @param length2 Length of scanning area along axis2 in mm
   * @param threshold Intensity threshold of the analog input signal, in V
   * @param distance Distance between the scanning lines in mm
   * @param analogInput Identifier of the analog input signal whose maximum intensity is sought
   * @return TRUE if scan started successfully, FALSE otherwise
   */
  bool FSC(const std::string& axis1, double length1,
    const std::string& axis2, double length2,
    double threshold, double distance,
    int analogInput);

  /**
   * Starts a scanning procedure to determine the global maximum intensity of an analog
   * input signal in a plane. Unlike FSC, this method scans the entire area to find the
   * global maximum rather than stopping when a threshold is reached.
   *
   * @param axis1 The axis in which the platform moves from scanning line to scanning line
   *              by the distance given by distance (X, Y, or Z).
   * @param length1 Length of scanning area along axis1 in mm
   * @param axis2 The axis in which the scanning lines are located (X, Y, or Z)
   * @param length2 Length of scanning area along axis2 in mm
   * @param threshold Intensity threshold of the analog input signal, in V
   * @param distance Distance between the scanning lines in mm
   * @param analogInput Identifier of the analog input signal whose maximum intensity is sought
   * @return TRUE if scan started successfully, FALSE otherwise
   */
  bool FSM(const std::string& axis1, double length1,
    const std::string& axis2, double length2,
    double threshold, double distance,
    int analogInput);

private:
  bool m_debugVerbose = false;


  // Communication thread methods
  void StartCommunicationThread();
  void StopCommunicationThread();
  void CommunicationThreadFunc();
  bool enableDebug = false;
  std::string m_windowTitle = "PI Controller"; // Default title

  // Thread-related members
  std::thread m_communicationThread;
  std::mutex m_mutex;
  std::condition_variable m_condVar;

  std::atomic<bool> m_threadRunning{ false };
  std::atomic<bool> m_terminateThread{ false };
  std::atomic<bool> m_isConnected{ false };
  int m_controllerId;  // Handle for the PI controller

  // Configuration
  std::string m_ipAddress;
  int m_port;
  std::vector<std::string> m_availableAxes;

  // Status monitoring - these are now cached values updated by the communication thread
  std::map<std::string, double> m_axisPositions;
  std::map<std::string, bool> m_axisMoving;
  std::map<std::string, bool> m_axisServoEnabled;

  // Logging
  Logger* m_logger;

  // UI state
  bool m_showWindow = false;
  double m_jogDistance = 1.0;  // Default jog distance in mm

  // Performance optimization
  bool m_enableDebug = false;  // Enable debug logging

  // Cache status
  std::chrono::steady_clock::time_point m_lastStatusUpdate;

  // Added for optimization: last position update timestamp
  std::chrono::steady_clock::time_point m_lastPositionUpdate;

  // Added for optimization: status update interval (ms)
  const int m_statusUpdateInterval = 200;  // 5Hz updates

  
};