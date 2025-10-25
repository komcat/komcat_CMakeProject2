// acs_controller.h
#pragma once
// Prevent Windows.h conflicts
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX  // Prevent min/max macro conflicts
#endif
#include <Windows.h>
#endif

#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <map>
#include "include/logger.h"
#include "include/motions/MotionTypes.h"  // Make sure this is included
#include "IPositionSubscriber.h"
#include <map>

// Include ACS controller library
#include "include/ACSC_wrapper.h"

class ACSController {
public:
  ACSController();
  ~ACSController();

  // Connection methods
  bool Connect(const std::string& ipAddress, int port = ACSC_SOCKET_STREAM_PORT);
  void Disconnect();
  bool IsConnected() const { return m_isConnected; }

  // Basic motion commands
  bool MoveToPosition(const std::string& axis, double position, bool blocking = true);
  bool MoveRelative(const std::string& axis, double distance, bool blocking = true);
  bool HomeAxis(const std::string& axis);
  bool StopAxis(const std::string& axis);
  bool StopAllAxes();
  bool StopAllMotion();


  //usage result = controller->MoveToAbsolutePosition(deviceX, deviceY, deviceZ, velocity);
	bool MoveToAbsolutePosition(double x, double y, double z, double velocity, bool blocking = true);

  // Status methods
  bool IsMoving(const std::string& axis);
  bool IsInMotion();
  bool GetPosition(const std::string& axis, double& position);
	float GetAxisPositionFloat(const std::string& axis);
  bool GetPositions(std::map<std::string, double>& positions);
	PositionStruct GetCurrentPosition(); // New method to get all axes positions X Y Z
  
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

  // Control window visibility
  void SetWindowVisible(bool visible) { m_showWindow = visible; }
  void SetWindowTitle(const std::string& title) { m_windowTitle = title; }

  // Add this method to expose available axes
  const std::vector<std::string>& GetAvailableAxes() const { return m_availableAxes; }
  // In acs_controller.h in the public section:
  bool MoveToPositionMultiAxis(const std::vector<std::string>& axes,
    const std::vector<double>& positions,
    bool blocking = true);
  bool CopyPositionToClipboard(); // New method to copy current position as JSON

  // Add this method to expose available axes
  // Buffer program control
  bool RunBuffer(int bufferNumber, const std::string& labelName = "");
  bool StopBuffer(int bufferNumber);

  // Optional: Helper to stop all buffers
  bool StopAllBuffers();

  // Optional: Check if buffer is running (if you need status)
  bool IsBufferRunning(int bufferNumber);


  // Subscribe/Unsubscribe for position updates
  void SubscribeToPositions(IPositionSubscriber* subscriber, const std::string& subscriberId);
  void UnsubscribeFromPositions(const std::string& subscriberId);

  // Communication thread methods
  void StartCommunicationThread();
  void StopCommunicationThread();
  void CommunicationThreadFunc();

  // Polling mode control
  enum class PollingMode { FAST, NORMAL, SLOW };
  void SetPollingMode(PollingMode mode) { m_pollingMode = mode; }
  PollingMode GetPollingMode() const { return m_pollingMode; }

private:

  // Add to the private section of ACSController class in acs_controller.h



  void ProcessCommandQueue();
  void UpdatePositions();
  void UpdateMotorStatus();
  // Add to public methods section in acs_controller.h
  bool StartMotion(const std::string& axis);
  // Command queue structure
  struct MotorCommand {
    std::string axis;
    double distance;
    bool executed;
  };

  std::string m_windowTitle = "ACS Controller"; // Default title

  // Thread-related members
  std::thread m_communicationThread;
  std::mutex m_mutex;
  std::condition_variable m_condVar;
  std::atomic<bool> m_threadRunning{ false };
  std::atomic<bool> m_terminateThread{ false };
  std::atomic<bool> m_isConnected{ false };
  std::string m_deviceName;
  // Command queue
  std::vector<MotorCommand> m_commandQueue;
  std::mutex m_commandMutex;

  // Controller handle
  HANDLE m_controllerId;  // Handle for the ACS controller

  // Configuration
  std::string m_ipAddress;
  int m_port;
  std::vector<std::string> m_availableAxes;

  // Status monitoring
  std::map<std::string, double> m_axisPositions;
  std::map<std::string, bool> m_axisMoving;
  std::map<std::string, bool> m_axisServoEnabled;

  // Logging
  Logger* m_logger;

  // UI state
  bool m_showWindow = false;
  double m_jogDistance = 1.0;  // Default jog distance in mm

  // Convert between string axis names and ACS axis indices
  int GetAxisIndex(const std::string& axis);

  // Debug flag
  bool m_enableDebug = false;  // Enable debug logging

  // Cache status
  std::chrono::steady_clock::time_point m_lastStatusUpdate;
  std::chrono::steady_clock::time_point m_lastPositionUpdate;
  const int m_statusUpdateInterval = 200;  // 5Hz updates

  std::string m_statusMessage;
  float m_statusMessageTime = 0.0f;

  // Position subscribers
  std::map<std::string, IPositionSubscriber*> m_positionSubscribers;
  mutable std::mutex m_subscribersMutex;
  // Notify all subscribers
  void NotifyPositionSubscribers(const std::map<std::string, double>& positions);
  void NotifyMotionStatusSubscribers(const std::string& axis, bool isMoving);

  // Adaptive polling mode
  std::atomic<PollingMode> m_pollingMode{ PollingMode::NORMAL };
};