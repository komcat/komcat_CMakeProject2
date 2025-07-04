﻿// acs_controller.cpp
#include "../include/motions/acs_controller.h"
#include "imgui.h"
#include <iostream>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <iomanip>  // For std::setprecision

// Constructor - initialize with correct axis identifiers
ACSController::ACSController()
  : m_controllerId(ACSC_INVALID),
  m_port(ACSC_SOCKET_STREAM_PORT) {

  // Initialize atomic variables
  m_isConnected.store(false);
  m_threadRunning.store(false);
  m_terminateThread.store(false);

  m_logger = Logger::GetInstance();
  m_logger->LogInfo("ACSController: Initializing controller");

  // Initialize available axes with string identifiers (consistent with PI controller)
  m_availableAxes = { "X", "Y", "Z"};

  // Start communication thread
  StartCommunicationThread();
}

ACSController::~ACSController() {
  m_logger->LogInfo("ACSController: Shutting down controller");

  // Stop communication thread
  StopCommunicationThread();

  // Disconnect if still connected
  if (m_isConnected) {
    Disconnect();
  }
}

void ACSController::StartCommunicationThread() {
  if (!m_threadRunning) {
    m_threadRunning.store(true);
    m_terminateThread.store(false);
    m_communicationThread = std::thread(&ACSController::CommunicationThreadFunc, this);
    m_logger->LogInfo("ACSController: Communication thread started");
  }
}

void ACSController::StopCommunicationThread() {
  if (m_threadRunning) {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_terminateThread.store(true);
    }
    m_condVar.notify_all();

    if (m_communicationThread.joinable()) {
      m_communicationThread.join();
    }

    m_threadRunning.store(false);
    m_logger->LogInfo("ACSController: Communication thread stopped");
  }
}

// In ACSController.cpp - improve CommunicationThreadFunc
void ACSController::CommunicationThreadFunc() {
  // Set update interval to 200ms (5 Hz)
  const auto updateInterval = std::chrono::milliseconds(200);

  // Frame counter for less frequent updates
  int frameCounter = 0;

  // Initialization of last update timestamps
  m_lastStatusUpdate = std::chrono::steady_clock::now();
  m_lastPositionUpdate = m_lastStatusUpdate;

  while (!m_terminateThread) {
    auto cycleStartTime = std::chrono::steady_clock::now();

    // Process any pending motor commands first for responsiveness
    {
      std::lock_guard<std::mutex> lock(m_commandMutex);
      for (auto& cmd : m_commandQueue) {
        if (!cmd.executed) {
          // Execute the command
          MoveRelative(cmd.axis, cmd.distance, false);
          cmd.executed = true;
        }
      }

      // Remove executed commands
      m_commandQueue.erase(
        std::remove_if(m_commandQueue.begin(), m_commandQueue.end(),
          [](const MotorCommand& cmd) { return cmd.executed; }),
        m_commandQueue.end());
    }

    // Only update if connected
    if (m_isConnected) {
      frameCounter++;

      // Always update positions
      std::map<std::string, double> positions;
      if (GetPositions(positions)) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_axisPositions = positions;
        m_lastPositionUpdate = std::chrono::steady_clock::now();
      }

      // Update other status less frequently (every 3rd frame, ~1.67Hz)
      if (frameCounter % 3 == 0) {
        // Update axis motion status for X, Y, Z only
        for (const auto& axis : { "X", "Y", "Z" }) {
          bool moving = IsMoving(axis);
          std::lock_guard<std::mutex> lock(m_mutex);
          m_axisMoving[axis] = moving;
        }

        // Update servo status for X, Y, Z only
        for (const auto& axis : { "X", "Y", "Z" }) {
          bool enabled;
          if (IsServoEnabled(axis, enabled)) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_axisServoEnabled[axis] = enabled;
          }
        }

        m_lastStatusUpdate = std::chrono::steady_clock::now();
      }
    }

    // Calculate how long to sleep to maintain consistent update rate
    auto cycleEndTime = std::chrono::steady_clock::now();
    auto cycleDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
      cycleEndTime - cycleStartTime);
    auto sleepTime = updateInterval - cycleDuration;

    // Wait for next update or termination
    std::unique_lock<std::mutex> lock(m_mutex);
    if (sleepTime.count() > 0) {
      m_condVar.wait_for(lock, sleepTime, [this]() { return m_terminateThread.load(); });
    }
    else {
      // No sleep needed if we're already behind schedule, but yield to let other threads run
      lock.unlock();
      std::this_thread::yield();
    }
  }
}


// Helper method to process the command queue
void ACSController::ProcessCommandQueue() {
  std::lock_guard<std::mutex> lock(m_commandMutex);
  for (auto& cmd : m_commandQueue) {
    if (!cmd.executed) {
      // Execute the command
      MoveRelative(cmd.axis, cmd.distance, false);
      cmd.executed = true;
    }
  }

  // Remove executed commands
  m_commandQueue.erase(
    std::remove_if(m_commandQueue.begin(), m_commandQueue.end(),
      [](const MotorCommand& cmd) { return cmd.executed; }),
    m_commandQueue.end());
}

// Helper method to update positions using batch query
void ACSController::UpdatePositions() {
  if (!m_isConnected) return;

  std::map<std::string, double> positions;
  if (GetPositions(positions)) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_axisPositions = positions;
    m_lastPositionUpdate = std::chrono::steady_clock::now();
  }
}

// Helper method to update motor status (moving, servo state)
void ACSController::UpdateMotorStatus() {
  if (!m_isConnected) return;

  auto now = std::chrono::steady_clock::now();

  // Use batch queries if possible, otherwise query each axis
  for (const auto& axis : m_availableAxes) {
    int axisIndex = GetAxisIndex(axis);
    if (axisIndex >= 0) {
      int state = 0;
      if (acsc_GetMotorState(m_controllerId, axisIndex, &state, NULL)) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_axisMoving[axis] = (state & ACSC_MST_MOVE) != 0;
        m_axisServoEnabled[axis] = (state & ACSC_MST_ENABLE) != 0;
      }
    }
  }

  m_lastStatusUpdate = now;
}

// Helper to convert string axis identifiers to ACS axis indices
int ACSController::GetAxisIndex(const std::string& axis) {
  if (axis == "X") return ACSC_AXIS_X;
  if (axis == "Y") return ACSC_AXIS_Y;
  if (axis == "Z") return ACSC_AXIS_Z;

  m_logger->LogWarning("ACSController: Unknown axis identifier: " + axis);
  return -1;
}

bool ACSController::Connect(const std::string& ipAddress, int port) {
  // Check if already connected
  if (m_isConnected) {
    m_logger->LogWarning("ACSController: Already connected to a controller");
    return true;
  }

  std::cout << "ACSController: Attempting connection to " << ipAddress << ":" << port << std::endl;
  m_logger->LogInfo("ACSController: Connecting to controller at " + ipAddress + ":" + std::to_string(port));

  // Store connection parameters
  m_ipAddress = ipAddress;
  m_port = port;

  // Attempt to connect to the controller
  // ACS requires a non-const char buffer for the IP address
  char ipBuffer[64];
  strncpy(ipBuffer, m_ipAddress.c_str(), sizeof(ipBuffer) - 1);
  ipBuffer[sizeof(ipBuffer) - 1] = '\0'; // Ensure null termination

  m_controllerId = acsc_OpenCommEthernet(ipBuffer, m_port);

  if (m_controllerId == ACSC_INVALID) {
    int errorCode = acsc_GetLastError();
    std::string errorMsg = "ACSController: Failed to connect to controller. Error code: " + std::to_string(errorCode);
    m_logger->LogError(errorMsg);
    std::cout << errorMsg << std::endl;
    return false;
  }

  m_isConnected.store(true);
  std::cout << "ACSController: Successfully connected to " << ipAddress << std::endl;
  m_logger->LogInfo("ACSController: Successfully connected to controller");

  // Enable all configured axes
  for (const auto& axis : m_availableAxes) {
    int axisIndex = GetAxisIndex(axis);
    if (axisIndex >= 0) {
      if (acsc_Enable(m_controllerId, axisIndex, NULL)) {
        m_logger->LogInfo("ACSController: Enabled axis " + axis);
      }
      else {
        int error = acsc_GetLastError();
        m_logger->LogError("ACSController: Failed to enable axis " + axis + ". Error: " + std::to_string(error));
      }
    }
  }

  // Initialize position cache immediately
  std::map<std::string, double> initialPositions;
  if (GetPositions(initialPositions)) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_axisPositions = initialPositions;
    m_lastPositionUpdate = std::chrono::steady_clock::now();

    // Log initial positions for debugging
    if (m_enableDebug) {
      std::stringstream ss;
      ss << "Initial positions: ";
      for (const auto& [axis, pos] : initialPositions) {
        ss << axis << "=" << pos << " ";
      }
      m_logger->LogInfo(ss.str());
    }
  }
  else {
    m_logger->LogWarning("ACSController: Failed to initialize position cache after connection");
  }

  return true;
}

void ACSController::Disconnect() {
  if (!m_isConnected) {
    return;
  }

  m_logger->LogInfo("ACSController: Disconnecting from controller");

  // Ensure all axes are stopped before disconnecting
  StopAllAxes();

  // Close connection
  acsc_CloseComm(m_controllerId);

  m_isConnected.store(false);
  m_controllerId = ACSC_INVALID;

  m_logger->LogInfo("ACSController: Disconnected from controller");
}

bool ACSController::MoveToPosition(const std::string& axis, double position, bool blocking) {
  if (!m_isConnected) {
    m_logger->LogError("ACSController: Cannot move axis - not connected");
    return false;
  }

  int axisIndex = GetAxisIndex(axis);
  if (axisIndex < 0) {
    return false;
  }

  m_logger->LogInfo("ACSController: Moving axis " + axis + " to position " + std::to_string(position));

  // Set up arrays for acsc_ToPointM
  int axes[2] = { axisIndex, -1 }; // -1 marks the end of the array
  double points[1] = { position };

  // Command the move - using absolute positioning
  if (!acsc_ToPointM(m_controllerId, ACSC_AMF_WAIT, axes, points, NULL)) {
    int error = acsc_GetLastError();
    m_logger->LogError("ACSController: Failed to move axis. Error code: " + std::to_string(error));
    return false;
  }

  // Start the motion
  if (!StartMotion(axis)) {
    return false;
  }

  // If blocking mode, wait for motion to complete
  if (blocking) {
    return WaitForMotionCompletion(axis);
  }

  return true;
}


bool ACSController::MoveRelative(const std::string& axis, double distance, bool blocking) {
  if (!m_isConnected) {
    m_logger->LogError("ACSController: Cannot move axis - not connected");
    return false;
  }

  int axisIndex = GetAxisIndex(axis);
  if (axisIndex < 0) {
    return false;
  }

  m_logger->LogInfo("ACSController: Moving axis " + axis + " relative distance " +
    std::to_string(distance));

  // Log pre-move position if debugging is enabled
  if (m_enableDebug) {
    double currentPos = 0.0;
    if (GetPosition(axis, currentPos)) {
      std::cout << "ACSController: Pre-move position of axis " << axis << " = "
        << currentPos << std::endl;
    }
  }

  // Set up arrays for acsc_ToPointM with RELATIVE flag
  int axes[2] = { axisIndex, -1 }; // -1 marks the end of the array
  double distances[1] = { distance };

  // Command the relative move
  if (!acsc_ToPointM(m_controllerId, ACSC_AMF_WAIT | ACSC_AMF_RELATIVE, axes, distances, NULL)) {
    int error = acsc_GetLastError();
    m_logger->LogError("ACSController: Failed to move axis relatively. Error code: " +
      std::to_string(error));
    return false;
  }

  // Start the motion
  if (!StartMotion(axis)) {
    return false;
  }

  // If blocking mode, wait for motion to complete
  if (blocking) {
    return WaitForMotionCompletion(axis);
  }

  return true;
}



bool ACSController::HomeAxis(const std::string& axis) {
  if (!m_isConnected) {
    m_logger->LogError("ACSController: Cannot home axis - not connected");
    return false;
  }

  int axisIndex = GetAxisIndex(axis);
  if (axisIndex < 0) {
    return false;
  }

  m_logger->LogInfo("ACSController: Homing axis " + axis);

  // For ACS, we use a specific homing command
  // This implementation may need to be adjusted based on your specific hardware
 // Option 1: Use a direct FaultClear + Home sequence
  if (!acsc_FaultClear(m_controllerId, axisIndex, NULL)) {
    int error = acsc_GetLastError();
    m_logger->LogError("ACSController: Failed to clear faults for homing. Error code: " + std::to_string(error));
    // Continue anyway as the axis might not have faults
  }

  // Wait for homing to complete
  return WaitForMotionCompletion(axis);
}

bool ACSController::StopAxis(const std::string& axis) {
  if (!m_isConnected) {
    m_logger->LogError("ACSController: Cannot stop axis - not connected");
    return false;
  }

  int axisIndex = GetAxisIndex(axis);
  if (axisIndex < 0) {
    return false;
  }

  m_logger->LogInfo("ACSController: Stopping axis " + axis);

  // Command the stop
  if (!acsc_Halt(m_controllerId, axisIndex, NULL)) {
    int error = acsc_GetLastError();
    m_logger->LogError("ACSController: Failed to stop axis. Error code: " + std::to_string(error));
    return false;
  }

  return true;
}

bool ACSController::StopAllAxes() {
  if (!m_isConnected) {
    m_logger->LogError("ACSController: Cannot stop all axes - not connected");
    return false;
  }

  m_logger->LogInfo("ACSController: Stopping all axes");

  // Command the stop for all axes
  if (!acsc_KillAll(m_controllerId, NULL)) {
    int error = acsc_GetLastError();
    m_logger->LogError("ACSController: Failed to stop all axes. Error code: " + std::to_string(error));
    return false;
  }

  return true;
}

// In ACSController.cpp - modify IsMoving to use cached values
bool ACSController::IsMoving(const std::string& axis) {
  if (!m_isConnected) {
    return false;
  }

  // Check if we have a recent cached value (less than 200ms old)
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    now - m_lastStatusUpdate).count();

  if (elapsed < m_statusUpdateInterval) {
    // Use cached value if it exists and is recent
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_axisMoving.find(axis);
    if (it != m_axisMoving.end()) {
      return it->second;
    }
  }

  // If no recent cached value, do direct query
  int axisIndex = GetAxisIndex(axis);
  if (axisIndex < 0) {
    return false;
  }

  // Get the motion state
  int state = 0;
  if (!acsc_GetMotorState(m_controllerId, axisIndex, &state, NULL)) {
    return false;
  }

  // Check if the axis is moving based on state bits
  bool isMoving = (state & ACSC_MST_MOVE) != 0;

  // Update the cache
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_axisMoving[axis] = isMoving;
    m_lastStatusUpdate = now;
  }

  return isMoving;
}


// Modify logging in performance-critical areas
// For example, in GetPosition method:
bool ACSController::GetPosition(const std::string& axis, double& position) {
  if (!m_isConnected) {
    return false;
  }

  int axisIndex = GetAxisIndex(axis);
  if (axisIndex < 0) {
    return false;
  }

  if (!acsc_GetFPosition(m_controllerId, axisIndex, &position, NULL)) {
    int error = acsc_GetLastError();
    // Only log in debug mode to reduce overhead
    if (m_enableDebug) {
      std::cout << "ACSController: Error getting position for axis " << axis << ": " << error << std::endl;
    }
    return false;
  }

  return true;
}



// In ACSController::GetPositions - modify to use batch queries
bool ACSController::GetPositions(std::map<std::string, double>& positions) {
  if (!m_isConnected || m_availableAxes.empty()) {
    return false;
  }

  // Since we now have only X, Y, Z axes, we can optimize this for exactly 3 axes
  // Create arrays for the axes we know we have
  int axisIndices[3] = {
      GetAxisIndex("X"), GetAxisIndex("Y"), GetAxisIndex("Z")
  };
  double posArray[3] = { 0.0 };

  // Query positions individually for each axis
  // This could be optimized with a batch call if the ACS API supports it
  bool success = true;
  for (int i = 0; i < 3; i++) {
    if (axisIndices[i] >= 0) {
      if (!acsc_GetFPosition(m_controllerId, axisIndices[i], &posArray[i], NULL)) {
        success = false;
      }
    }
  }

  if (success) {
    // Fill the map with results
    for (int i = 0; i < 3; i++) {
      if (axisIndices[i] >= 0) {
        positions[m_availableAxes[i]] = posArray[i];
      }
    }
  }

  return success;
}

bool ACSController::EnableServo(const std::string& axis, bool enable) {
  if (!m_isConnected) {
    m_logger->LogError("ACSController: Cannot change servo state - not connected");
    return false;
  }

  int axisIndex = GetAxisIndex(axis);
  if (axisIndex < 0) {
    return false;
  }

  m_logger->LogInfo("ACSController: Setting servo state for axis " + axis + " to " +
    (enable ? "enabled" : "disabled"));

  // Enable or disable the servo
  bool result = false;
  if (enable) {
    result = acsc_Enable(m_controllerId, axisIndex, NULL) != 0;
  }
  else {
    result = acsc_Disable(m_controllerId, axisIndex, NULL) != 0;
  }

  if (!result) {
    int error = acsc_GetLastError();
    m_logger->LogError("ACSController: Failed to set servo state. Error code: " + std::to_string(error));
  }

  return result;
}

bool ACSController::IsServoEnabled(const std::string& axis, bool& enabled) {
  if (!m_isConnected) {
    return false;
  }

  int axisIndex = GetAxisIndex(axis);
  if (axisIndex < 0) {
    return false;
  }

  // Get the motor state
  int state = 0;
  if (!acsc_GetMotorState(m_controllerId, axisIndex, &state, NULL)) {
    return false;
  }

  // Check if the axis is enabled based on state bits
  enabled = (state & ACSC_MST_ENABLE) != 0;
  return true;
}

bool ACSController::SetVelocity(const std::string& axis, double velocity) {
  if (!m_isConnected) {
    m_logger->LogError("ACSController: Cannot set velocity - not connected");
    return false;
  }

  int axisIndex = GetAxisIndex(axis);
  if (axisIndex < 0) {
    return false;
  }

  m_logger->LogInfo("ACSController: Setting velocity for axis " + axis + " to " + std::to_string(velocity));

  // Set the velocity
  if (!acsc_SetVelocity(m_controllerId, axisIndex, velocity, NULL)) {
    int error = acsc_GetLastError();
    m_logger->LogError("ACSController: Failed to set velocity. Error code: " + std::to_string(error));
    return false;
  }

  return true;
}

bool ACSController::GetVelocity(const std::string& axis, double& velocity) {
  if (!m_isConnected) {
    return false;
  }

  int axisIndex = GetAxisIndex(axis);
  if (axisIndex < 0) {
    return false;
  }

  // Get the velocity
  if (!acsc_GetVelocity(m_controllerId, axisIndex, &velocity, NULL)) {
    return false;
  }

  return true;
}

bool ACSController::WaitForMotionCompletion(const std::string& axis, double timeoutSeconds) {
  if (!m_isConnected) {
    m_logger->LogError("ACSController: Cannot wait for motion completion - not connected");
    return false;
  }

  int axisIndex = GetAxisIndex(axis);
  if (axisIndex < 0) {
    return false;
  }

  m_logger->LogInfo("ACSController: Waiting for motion completion on axis " + axis);

  // Use system clock for timeout
  auto startTime = std::chrono::steady_clock::now();
  int checkCount = 0;

  while (true) {
    checkCount++;
    bool stillMoving = IsMoving(axis);

    if (!stillMoving) {
      m_logger->LogInfo("ACSController: Motion completed on axis " + axis);
      return true;
    }

    // Check for timeout
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();

    if (elapsedSeconds > timeoutSeconds) {
      m_logger->LogWarning("ACSController: Timeout waiting for motion completion on axis " + axis);
      return false;
    }

    // Sleep to avoid CPU spikes
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

//
// Updated ConfigureFromDevice method for ACSController to handle space-separated InstalledAxes
//
bool ACSController::ConfigureFromDevice(const MotionDevice& device) {
  if (m_isConnected) {
    m_logger->LogWarning("ACSController: Cannot configure from device while connected");
    return false;
  }

  m_deviceName = device.Name;
  m_logger->LogInfo("ACSController: Configuring from device: " + device.Name);

  // Store the IP address and port from the device configuration
  m_ipAddress = device.IpAddress;
  m_port = device.Port;

  // Define available axes based on device configuration
  m_availableAxes.clear();

  // If InstalledAxes is specified, use it
  if (!device.InstalledAxes.empty()) {
    // Since InstalledAxes may be space-separated (e.g., "X Y Z"),
    // we need to parse it by splitting on spaces

    std::string axisStr = device.InstalledAxes;
    std::string delimiter = " ";
    size_t pos = 0;
    std::string token;

    // Parse the space-separated string
    while ((pos = axisStr.find(delimiter)) != std::string::npos) {
      token = axisStr.substr(0, pos);
      if (!token.empty()) {
        m_availableAxes.push_back(token);
      }
      axisStr.erase(0, pos + delimiter.length());
    }

    // Add the last token if there is one
    if (!axisStr.empty()) {
      m_availableAxes.push_back(axisStr);
    }

    // Log the configured axes
    std::string axesList;
    for (const auto& axis : m_availableAxes) {
      if (!axesList.empty()) axesList += " ";
      axesList += axis;
    }

    m_logger->LogInfo("ACSController: Configured with specified axes: " + axesList);
  }
  // Otherwise, use defaults based on device type (historically ACS controllers use X, Y, Z)
  else {
    m_availableAxes = { "X", "Y", "Z" };
    m_logger->LogInfo("ACSController: Configured with default gantry axes (X Y Z)");
  }

  return true;
}


bool ACSController::MoveToNamedPosition(const std::string& deviceName, const std::string& positionName) {
  m_logger->LogInfo("ACSController: Moving to named position " + positionName + " for device " + deviceName);

  // This method would typically lookup the position from a configuration source
  // For now, it's a placeholder that could be extended with your position data

  return true;
}

bool ACSController::StartMotion(const std::string& axis) {
  if (!m_isConnected) {
    m_logger->LogError("ACSController: Cannot start motion - not connected");
    return false;
  }

  int axisIndex = GetAxisIndex(axis);
  if (axisIndex < 0) {
    m_logger->LogError("ACSController: Invalid axis for starting motion: " + axis);
    return false;
  }

  m_logger->LogInfo("ACSController: Starting motion on axis " + axis);

  // Set up axis array for GoM (ending with -1)
  int axes[2] = { axisIndex, -1 };

  // Call acsc_GoM to start the motion
  if (!acsc_GoM(m_controllerId, axes, NULL)) {
    int error = acsc_GetLastError();
    m_logger->LogError("ACSController: Failed to start motion on axis " + axis +
      ". Error code: " + std::to_string(error));
    return false;
  }

  return true;
}


bool ACSController::MoveToPositionMultiAxis(const std::vector<std::string>& axes,
  const std::vector<double>& positions,
  bool blocking) {
  if (!m_isConnected) {
    m_logger->LogError("ACSController: Cannot move axes - not connected");
    return false;
  }

  if (axes.size() != positions.size() || axes.empty()) {
    m_logger->LogError("ACSController: Invalid axes/positions arrays for multi-axis move");
    return false;
  }

  // Log the motion command
  std::stringstream ss;
  ss << "ACSController: Moving multiple axes to positions: ";
  for (size_t i = 0; i < axes.size(); i++) {
    ss << axes[i] << "=" << positions[i] << " ";
  }
  m_logger->LogInfo(ss.str());

  // Convert string axes to ACS axis indices
  std::vector<int> axisIndices;
  for (const auto& axis : axes) {
    int axisIndex = GetAxisIndex(axis);
    if (axisIndex < 0) {
      m_logger->LogError("ACSController: Invalid axis: " + axis);
      return false;
    }
    axisIndices.push_back(axisIndex);
  }

  // Set up arrays for acsc_ToPointM
  std::vector<int> axesArray(axisIndices.size() + 1);  // +1 for the terminating -1
  for (size_t i = 0; i < axisIndices.size(); i++) {
    axesArray[i] = axisIndices[i];
  }
  axesArray[axisIndices.size()] = -1;  // Mark the end of the array with -1

  // Command the move using acsc_ToPointM
  if (!acsc_ToPointM(m_controllerId, ACSC_AMF_WAIT, axesArray.data(),
    const_cast<double*>(positions.data()), NULL)) {
    int error = acsc_GetLastError();
    m_logger->LogError("ACSController: Failed to move axes. Error code: " +
      std::to_string(error));
    return false;
  }

  // Start the motion - for multi-axis we'll need to use GoM instead of Go
  if (!acsc_GoM(m_controllerId, axesArray.data(), NULL)) {
    int error = acsc_GetLastError();
    m_logger->LogError("ACSController: Failed to start motion. Error code: " +
      std::to_string(error));
    return false;
  }

  // If blocking mode, wait for motion to complete on all axes
  if (blocking) {
    bool allCompleted = true;
    for (const auto& axis : axes) {
      if (!WaitForMotionCompletion(axis)) {
        m_logger->LogError("ACSController: Timeout waiting for motion completion on axis " + axis);
        allCompleted = false;
      }
    }
    return allCompleted;
  }

  return true;
}



void ACSController::RenderUI() {
  // Skip rendering if window is hidden
  if (!m_showWindow) {
    return;
  }

  // Use the unique window title
  if (!ImGui::Begin(m_windowTitle.c_str(), &m_showWindow, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::End();
    return;
  }

  // Connection status and controls
  ImGui::Text("Connection Status: %s", m_isConnected ? "Connected" : "Disconnected");

  if (!m_isConnected) {
    // Show connection controls
    char ipBuffer[64];
    strncpy(ipBuffer, m_ipAddress.empty() ? "192.168.0.50" : m_ipAddress.c_str(), sizeof(ipBuffer) - 1);
    ipBuffer[sizeof(ipBuffer) - 1] = '\0';

    int port = m_port == 0 ? ACSC_SOCKET_STREAM_PORT : m_port;

    ImGui::InputText("IP Address", ipBuffer, sizeof(ipBuffer));
    ImGui::InputInt("Port", &port);

    if (ImGui::Button("Connect")) {
      Connect(ipBuffer, port);
    }
  }
  else {
    // Show disconnection control
    if (ImGui::Button("Disconnect")) {
      Disconnect();
    }

    ImGui::Separator();

    // Motion controls
    ImGui::Text("Motion Controls");

    // Jog distance control
    float jogDistanceFloat = static_cast<float>(m_jogDistance);
    if (ImGui::SliderFloat("Jog Distance (mm)", &jogDistanceFloat, 0.01f, 10.0f, "%.3f")) {
      m_jogDistance = static_cast<double>(jogDistanceFloat);
    }

    // Add velocity control slider (integers from 1-100)
    static int velocityValue = 10; // Default velocity value
    ImGui::SliderInt("Velocity", &velocityValue, 1, 100);

    // Add velocity axis selector and set button
    static int selectedAxisIndex = 0;
    if (ImGui::BeginCombo("Velocity Axis", m_availableAxes[selectedAxisIndex].c_str())) {
      for (int i = 0; i < m_availableAxes.size(); i++) {
        bool isSelected = (selectedAxisIndex == i);
        if (ImGui::Selectable(m_availableAxes[i].c_str(), isSelected)) {
          selectedAxisIndex = i;
        }
        if (isSelected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }

    // Set velocity button
    if (ImGui::Button("Set Velocity")) {
      // Apply the velocity to the selected axis
      SetVelocity(m_availableAxes[selectedAxisIndex], static_cast<double>(velocityValue));
    }

    // Create static variables to persist previous values
    static std::map<std::string, double> lastPositions;
    static std::map<std::string, bool> lastMoving;
    static std::map<std::string, bool> lastServoEnabled;

    // Store axis positions in local variables to avoid locking the mutex multiple times
    std::map<std::string, double> positionsCopy;
    std::map<std::string, bool> movingCopy;
    std::map<std::string, bool> servoEnabledCopy;

    {
      // Single mutex lock to copy all cached data
      std::lock_guard<std::mutex> lock(m_mutex);
      positionsCopy = m_axisPositions;
      movingCopy = m_axisMoving;
      servoEnabledCopy = m_axisServoEnabled;
    }

    // Merge with last known values - keep previous values if not present in current data
    for (const auto& axis : m_availableAxes) {
      // If we don't have a position for this axis but we had one before, keep the old one
      if (positionsCopy.find(axis) == positionsCopy.end() && lastPositions.find(axis) != lastPositions.end()) {
        positionsCopy[axis] = lastPositions[axis];
      }

      // Same for servo enabled status
      if (servoEnabledCopy.find(axis) == servoEnabledCopy.end() && lastServoEnabled.find(axis) != lastServoEnabled.end()) {
        servoEnabledCopy[axis] = lastServoEnabled[axis];
      }

      // Same for moving status
      if (movingCopy.find(axis) == movingCopy.end() && lastMoving.find(axis) != lastMoving.end()) {
        movingCopy[axis] = lastMoving[axis];
      }
    }

    // Save current values for next frame
    lastPositions = positionsCopy;
    lastMoving = movingCopy;
    lastServoEnabled = servoEnabledCopy;

    // Display axis controls in a grid layout
    ImVec2 buttonSize(30, 25); // Standard size for all jog buttons
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 8)); // Tighter spacing

    // Create a table for better layout
    if (ImGui::BeginTable("AxisControlTable", 1, ImGuiTableFlags_Borders)) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();

      // Title for the axis controls
      ImGui::TextColored(ImVec4(0.2f, 0.5f, 0.8f, 1.0f), "XYZ Axis Controls");

      // Display each axis control in a separate row
      for (const auto& axis : m_availableAxes) {
        ImGui::PushID(axis.c_str());
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        // Get axis position from cached values
        auto posIt = positionsCopy.find(axis);
        double position = (posIt != positionsCopy.end()) ? posIt->second : 0.0;

        // Get servo status from cached values
        auto servoIt = servoEnabledCopy.find(axis);
        bool enabled = (servoIt != servoEnabledCopy.end()) ? servoIt->second : false;

        // Get motion status
        auto movingIt = movingCopy.find(axis);
        bool moving = (movingIt != movingCopy.end()) ? movingIt->second : false;

        // Add a colored indicator for moving status
        ImVec4 statusColor = moving ? ImVec4(1.0f, 0.5f, 0.0f, 1.0f) : ImVec4(0.0f, 0.8f, 0.0f, 1.0f);
        ImGui::TextColored(statusColor, "●");
        ImGui::SameLine();

        // Display axis info with proper label
        ImGui::Text("Axis %s: %.3f mm %s",
          axis.c_str(), position,
          enabled ? "(Enabled)" : "(Disabled)");

        // Create a line with jog controls
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20); // Indent the controls

        // Jog negative button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.3f, 0.6f, 1.0f)); // Deep blue for negative direction
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.4f, 0.7f, 1.0f)); // Slightly lighter blue on hover
        if (ImGui::Button(("-##" + axis).c_str(), buttonSize)) {
          if (enabled) {
            // Queue the motor command
            std::lock_guard<std::mutex> lock(m_commandMutex);
            m_commandQueue.push_back({ axis, -m_jogDistance, false });
            m_condVar.notify_one();
          }
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();

        // Jog positive button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.5f, 0.4f, 1.0f)); // Teal-like blue for positive direction
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.6f, 0.5f, 1.0f)); // Slightly lighter teal on hover
        if (ImGui::Button(("+##" + axis).c_str(), buttonSize)) {
          if (enabled) {
            // Queue the motor command
            std::lock_guard<std::mutex> lock(m_commandMutex);
            m_commandQueue.push_back({ axis, m_jogDistance, false });
            m_condVar.notify_one();
          }
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();

        // Home button
        if (ImGui::Button(("Home##" + axis).c_str(), ImVec2(60, 25))) {
          if (enabled) {
            HomeAxis(axis);
          }
        }
        ImGui::SameLine();

        // Stop button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f)); // Red for stop
        if (ImGui::Button(("Stop##" + axis).c_str(), ImVec2(60, 25))) {
          StopAxis(axis);
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();

        // Enable/Disable button
        std::string buttonLabel = enabled ? "Disable##" + axis : "Enable##" + axis;
        ImGui::PushStyleColor(ImGuiCol_Button, enabled ? ImVec4(0.2f, 0.6f, 0.2f, 1.0f) : ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button(buttonLabel.c_str(), ImVec2(70, 25))) {
          EnableServo(axis, !enabled);
        }
        ImGui::PopStyleColor();

        ImGui::PopID();

      }

      ImGui::EndTable();
    }
    ImGui::PopStyleVar(); // Pop the spacing style



    ImGui::Separator();

    // Add a motion status indicator - use cached values
    bool anyAxisMoving = false;
    for (const auto& [axis, moving] : movingCopy) {
      if (moving) {
        anyAxisMoving = true;
        break;
      }
    }

    // Display a status bar
    ImVec4 statusColor = anyAxisMoving ? ImVec4(1.0f, 0.5f, 0.0f, 1.0f) : ImVec4(0.0f, 0.8f, 0.0f, 1.0f);
    ImGui::TextColored(statusColor, "●");
    ImGui::SameLine();
    ImGui::Text("Motion Status: %s", anyAxisMoving ? "Moving" : "Idle");

    ImGui::Separator();
    

    // Position copy controls
    ImGui::TextColored(ImVec4(0.2f, 0.5f, 0.8f, 1.0f), "Position Data");

    // Button with consistent styling
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.6f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.7f, 1.0f));
    if (ImGui::Button("Copy Position as JSON", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 30))) {
      if (CopyPositionToClipboard()) {
        m_statusMessage = "Position copied to clipboard as JSON";
        m_statusMessageTime = 3.0f; // Show message for 3 seconds
      }
      else {
        m_statusMessage = "Failed to copy position";
        m_statusMessageTime = 3.0f;
      }
    }
    ImGui::PopStyleColor(2);

    // Show status message with animation effect
    if (!m_statusMessage.empty() && m_statusMessageTime > 0) {
      ImGui::SameLine();

      // Fade out the status message as it approaches expiration
      float alpha = (std::min)(1.0f, m_statusMessageTime / 0.5f);
      ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, alpha), "%s", m_statusMessage.c_str());

      // Update time
      float deltaTime = ImGui::GetIO().DeltaTime;
      m_statusMessageTime -= deltaTime;
      if (m_statusMessageTime <= 0) {
        m_statusMessage.clear();
      }
    }
    ImGui::Separator();
    // Stop all axes button with red theme
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1.0f)); // Red for emergency stop
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f)); // Lighter red on hover
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.0f, 0.0f, 1.0f)); // Darker red when active
    if (ImGui::Button("STOP ALL AXES", ImVec2(-1, 40))) { // Full width, 40px height
      StopAllAxes();
    }
    ImGui::PopStyleColor(3);
  }

  ImGui::End();
}

// Add this implementation to acs_controller.cpp
bool ACSController::CopyPositionToClipboard() {
  // Create a copy of current positions to avoid locking the mutex for too long
  std::map<std::string, double> positions;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    positions = m_axisPositions;
  }

  // Check if we have any positions to copy
  if (positions.empty()) {
    return false;
  }

  // Format as JSON with proper indentation
  std::stringstream jsonStr;

  // Start JSON object with device name
  jsonStr << "{" << std::endl;
  jsonStr << "  \"device\": \"" << m_deviceName << "\"," << std::endl;
  jsonStr << "  \"positions\": {" << std::endl;

  // Use iterator to handle the comma placement
  auto it = positions.begin();
  auto end = positions.end();

  while (it != end) {
    // Format with 6 decimal places precision
    jsonStr << "    \"" << it->first << "\": " << std::fixed << std::setprecision(6) << it->second;

    // Add comma if not the last element
    ++it;
    if (it != end) {
      jsonStr << ",";
    }
    jsonStr << std::endl;
  }

  jsonStr << "  }" << std::endl;
  jsonStr << "}";

  // Copy to clipboard using ImGui
  ImGui::SetClipboardText(jsonStr.str().c_str());

  return true;
}


bool ACSController::RunBuffer(int bufferNumber, const std::string& labelName) {
  if (!m_isConnected) {
    m_logger->LogError("ACSController: Cannot run buffer - not connected");
    return false;
  }

  // Validate buffer number (0-63 depending on controller)
  if (bufferNumber < 0 || bufferNumber > 63) {
    m_logger->LogError("ACSController: Invalid buffer number " + std::to_string(bufferNumber) +
      ". Must be between 0 and 63");
    return false;
  }

  // Prepare label parameter
  char* labelPtr = nullptr;
  char labelBuffer[256] = { 0 };

  if (!labelName.empty()) {
    // Validate label name (must start with underscore or A-Z)
    std::string upperLabel = labelName;
    std::transform(upperLabel.begin(), upperLabel.end(), upperLabel.begin(), ::toupper);

    if (upperLabel[0] != '_' && (upperLabel[0] < 'A' || upperLabel[0] > 'Z')) {
      m_logger->LogError("ACSController: Invalid label name '" + labelName +
        "'. Label must start with underscore or letter A-Z");
      return false;
    }

    // Copy to buffer for ACS API
    strncpy(labelBuffer, upperLabel.c_str(), sizeof(labelBuffer) - 1);
    labelBuffer[sizeof(labelBuffer) - 1] = '\0';
    labelPtr = labelBuffer;

    m_logger->LogInfo("ACSController: Running buffer " + std::to_string(bufferNumber) +
      " from label " + labelName);
  }
  else {
    m_logger->LogInfo("ACSController: Running buffer " + std::to_string(bufferNumber) +
      " from start");
  }

  // Call ACS API function
  if (!acsc_RunBuffer(m_controllerId, bufferNumber, labelPtr, ACSC_SYNCHRONOUS)) {
    int error = acsc_GetLastError();
    m_logger->LogError("ACSController: Failed to run buffer " + std::to_string(bufferNumber) +
      ". Error code: " + std::to_string(error));
    return false;
  }

  m_logger->LogInfo("ACSController: Successfully started buffer " + std::to_string(bufferNumber));
  return true;
}

bool ACSController::StopBuffer(int bufferNumber) {
  if (!m_isConnected) {
    m_logger->LogError("ACSController: Cannot stop buffer - not connected");
    return false;
  }

  // Validate buffer number (0-63 depending on controller)
  if (bufferNumber < 0 || bufferNumber > 63) {
    m_logger->LogError("ACSController: Invalid buffer number " + std::to_string(bufferNumber) +
      ". Must be between 0 and 63");
    return false;
  }

  m_logger->LogInfo("ACSController: Stopping buffer " + std::to_string(bufferNumber));

  // Call ACS API function
  if (!acsc_StopBuffer(m_controllerId, bufferNumber, ACSC_SYNCHRONOUS)) {
    int error = acsc_GetLastError();
    m_logger->LogError("ACSController: Failed to stop buffer " + std::to_string(bufferNumber) +
      ". Error code: " + std::to_string(error));
    return false;
  }

  m_logger->LogInfo("ACSController: Successfully stopped buffer " + std::to_string(bufferNumber));
  return true;
}

bool ACSController::StopAllBuffers() {
  if (!m_isConnected) {
    m_logger->LogError("ACSController: Cannot stop all buffers - not connected");
    return false;
  }

  m_logger->LogInfo("ACSController: Stopping all buffers");

  // Use ACSC_NONE to stop all buffers
  if (!acsc_StopBuffer(m_controllerId, ACSC_NONE, ACSC_SYNCHRONOUS)) {
    int error = acsc_GetLastError();
    m_logger->LogError("ACSController: Failed to stop all buffers. Error code: " +
      std::to_string(error));
    return false;
  }

  m_logger->LogInfo("ACSController: Successfully stopped all buffers");
  return true;
}