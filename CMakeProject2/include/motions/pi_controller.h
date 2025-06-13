// pi_controller.h - Updated with integrated analog reading
#pragma once
#include <Windows.h>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <map>
#include "../logger.h"
#include "../motions/MotionTypes.h"
#include <iomanip>

// Include PI GCS2 library
#include "PI_GCS2_DLL.h"

// Forward declaration
class GlobalDataStore;

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

  // Multi-axis moves
  bool MoveToPositionAll(double x, double y, double z, double u, double v, double w, bool blocking = true);
  bool MoveToPositionMultiAxis(const std::vector<std::string>& axes,
    const std::vector<double>& positions,
    bool blocking = true);

  // UI rendering
  void RenderUI();
  void RenderJogDistanceControl();

  // Window control
  void SetWindowVisible(bool visible) { m_showWindow = visible; }
  void SetWindowTitle(const std::string& title) { m_windowTitle = title; }

  // Utility methods
  int GetControllerId() const { return m_controllerId; }
  const std::vector<std::string>& GetAvailableAxes() const { return m_availableAxes; }
  bool CopyPositionToClipboard();

  // Built-in scanning functions
  bool FSA(const std::string& axis1, double length1, const std::string& axis2, double length2,
    double threshold, double distance, double alignStep, int analogInput);
  bool FSC(const std::string& axis1, double length1, const std::string& axis2, double length2,
    double threshold, double distance, int analogInput);
  bool FSM(const std::string& axis1, double length1, const std::string& axis2, double length2,
    double threshold, double distance, int analogInput);

  // NEW: Analog reading methods
  bool GetAnalogChannelCount(int& numChannels);
  bool GetAnalogVoltage(int channel, double& voltage);
  bool GetAnalogVoltages(std::vector<int> channels, std::map<int, double>& voltages);

  // Enable/disable analog reading in communication thread
  void EnableAnalogReading(bool enable) { m_enableAnalogReading = enable; }
  bool IsAnalogReadingEnabled() const { return m_enableAnalogReading; }

  void StopCommunicationThread();

private:
  bool m_debugVerbose = false;
  bool enableDebug = false;

  // Communication thread methods
  void StartCommunicationThread();
  void CommunicationThreadFunc();

  // NEW: Analog reading methods for communication thread
  void UpdateAnalogReadings();
  void InitializeAnalogChannels();

  std::string m_windowTitle = "PI Controller";

  // Thread-related members
  std::thread m_communicationThread;
  std::mutex m_mutex;
  std::condition_variable m_condVar;

  std::atomic<bool> m_threadRunning{ false };
  std::atomic<bool> m_terminateThread{ false };
  std::atomic<bool> m_isConnected{ false };
  int m_controllerId;

  // Configuration
  std::string m_ipAddress;
  int m_port;
  std::vector<std::string> m_availableAxes;

  // Motion status - cached values updated by communication thread
  std::map<std::string, double> m_axisPositions;
  std::map<std::string, bool> m_axisMoving;
  std::map<std::string, bool> m_axisServoEnabled;

  // NEW: Analog reading state
  std::atomic<bool> m_enableAnalogReading{ true };  // Enable by default
  int m_numAnalogChannels = 0;
  std::map<int, double> m_analogVoltages;  // Cache analog readings
  std::vector<int> m_activeAnalogChannels = { 5, 6 };  // Default to channels 5 and 6

  // Reference to global data store
  GlobalDataStore* m_dataStore = nullptr;
  std::string m_deviceName;  // Device name for data store keys

  // Logging
  Logger* m_logger;

  // UI state
  bool m_showWindow = false;
  double m_jogDistance = 1.0;

  // Performance optimization
  std::chrono::steady_clock::time_point m_lastStatusUpdate;
  std::chrono::steady_clock::time_point m_lastPositionUpdate;
  const int m_statusUpdateInterval = 200;  // 5Hz updates

  bool m_enableDebug = false;  // Add this line
};