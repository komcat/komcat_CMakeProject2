#include "GlobalMotionController.h"
#include "include/motions/pi_controller_manager.h"
#include "include/motions/acs_controller_manager.h"
#include "include/logger.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <cmath>
#include <iostream>

// ============================================================================
// Matrix3x3 Implementation
// ============================================================================

Matrix3x3::Matrix3x3() {
  SetIdentity();
}

void Matrix3x3::SetIdentity() {
  m[0][0] = 1; m[0][1] = 0; m[0][2] = 0;
  m[1][0] = 0; m[1][1] = 1; m[1][2] = 0;
  m[2][0] = 0; m[2][1] = 0; m[2][2] = 1;
}

void Matrix3x3::Transform(float& x, float& y, float& z) const {
  float newX = m[0][0] * x + m[0][1] * y + m[0][2] * z;
  float newY = m[1][0] * x + m[1][1] * y + m[1][2] * z;
  float newZ = m[2][0] * x + m[2][1] * y + m[2][2] * z;
  x = newX; y = newY; z = newZ;
}

Matrix3x3 Matrix3x3::GetInverse() const {
  Matrix3x3 inv;
  // For rotation matrices (orthogonal), inverse = transpose
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      inv.m[i][j] = m[j][i];
    }
  }
  return inv;
}

// ============================================================================
// GlobalPosition Implementation
// ============================================================================

GlobalPosition::GlobalPosition(float x, float y, float z, const std::string& device)
  : x(x), y(y), z(z), deviceId(device) {
}

// ============================================================================
// GlobalMotionController Implementation
// ============================================================================

GlobalMotionController::GlobalMotionController() {
  InitializeDefaultMatrices();
  LogStatus("GlobalMotionController initialized");
}

// Update destructor to clean up thread
GlobalMotionController::~GlobalMotionController() {
  if (m_moveThread.joinable()) {
    m_moveThread.join();
  }
}

void GlobalMotionController::InitializeDefaultMatrices() {
  // Based on your transformation_matrix.json file

  // hex-left: Z->X, Y->Y, X->Z
  Matrix3x3 hexLeft;
  hexLeft.m[0][0] = 0; hexLeft.m[0][1] = 0; hexLeft.m[0][2] = 1;
  hexLeft.m[1][0] = 0; hexLeft.m[1][1] = 1; hexLeft.m[1][2] = 0;
  hexLeft.m[2][0] = 1; hexLeft.m[2][1] = 0; hexLeft.m[2][2] = 0;
  m_transformMatrices["hex-left"] = hexLeft;

  // hex-bottom: -Y->X, X->Y, Z->Z (90° rotation in XY plane)
  Matrix3x3 hexBottom;
  hexBottom.m[0][0] = 0; hexBottom.m[0][1] = -1; hexBottom.m[0][2] = 0;
  hexBottom.m[1][0] = 1; hexBottom.m[1][1] = 0; hexBottom.m[1][2] = 0;
  hexBottom.m[2][0] = 0; hexBottom.m[2][1] = 0; hexBottom.m[2][2] = 1;
  m_transformMatrices["hex-bottom"] = hexBottom;

  // hex-right: Z->X, -Y->Y, -X->Z
  Matrix3x3 hexRight;
  hexRight.m[0][0] = 0; hexRight.m[0][1] = 0; hexRight.m[0][2] = 1;
  hexRight.m[1][0] = 0; hexRight.m[1][1] = -1; hexRight.m[1][2] = 0;
  hexRight.m[2][0] = -1; hexRight.m[2][1] = 0; hexRight.m[2][2] = 0;
  m_transformMatrices["hex-right"] = hexRight;

  // gantry-main: X->X, Y->Y, -Z->Z (inverts Z axis)
  Matrix3x3 gantryMain;
  gantryMain.m[0][0] = 1; gantryMain.m[0][1] = 0; gantryMain.m[0][2] = 0;
  gantryMain.m[1][0] = 0; gantryMain.m[1][1] = 1; gantryMain.m[1][2] = 0;
  gantryMain.m[2][0] = 0; gantryMain.m[2][1] = 0; gantryMain.m[2][2] = -1;
  m_transformMatrices["gantry-main"] = gantryMain;

  LogStatus("Initialized default transformation matrices for 4 devices");
}

bool GlobalMotionController::LoadTransformationMatrices(const std::string& jsonFile) {
  try {
    std::ifstream file(jsonFile);
    if (!file.is_open()) {
      LogStatus("Failed to open transformation matrix file: " + jsonFile);
      return false;
    }

    nlohmann::json j;
    file >> j;

    for (const auto& item : j) {
      std::string deviceId = item["DeviceId"];
      Matrix3x3 matrix;

      auto m = item["Matrix"];
      matrix.m[0][0] = m["M11"]; matrix.m[0][1] = m["M12"]; matrix.m[0][2] = m["M13"];
      matrix.m[1][0] = m["M21"]; matrix.m[1][1] = m["M22"]; matrix.m[1][2] = m["M23"];
      matrix.m[2][0] = m["M31"]; matrix.m[2][1] = m["M32"]; matrix.m[2][2] = m["M33"];

      m_transformMatrices[deviceId] = matrix;
      LogStatus("Loaded transformation matrix for " + deviceId);
    }

    return true;
  }
  catch (const std::exception& e) {
    LogStatus("Failed to parse transformation matrix file: " + std::string(e.what()));
    return false;
  }
}

void GlobalMotionController::SetTransformationMatrix(const std::string& deviceId, const Matrix3x3& matrix) {
  m_transformMatrices[deviceId] = matrix;
  LogStatus("Set transformation matrix for " + deviceId);
}

bool GlobalMotionController::MoveToGlobal(const std::string& deviceId,
  float globalX, float globalY, float globalZ,
  float velocity) {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Check if device exists
  if (!IsDeviceAvailable(deviceId)) {
    LogStatus("Device not available: " + deviceId);
    return false;
  }

  // Check global limits
  if (!IsWithinGlobalLimits(globalX, globalY, globalZ)) {
    LogStatus("Target position outside global limits for " + deviceId);
    return false;
  }

  // Convert to device coordinates
  float deviceX, deviceY, deviceZ;
  GlobalToDevice(deviceId, globalX, globalY, globalZ, deviceX, deviceY, deviceZ);

  LogStatus(deviceId + " moving to global: (" +
    std::to_string(globalX) + ", " + std::to_string(globalY) + ", " +
    std::to_string(globalZ) + ") -> device: (" +
    std::to_string(deviceX) + ", " + std::to_string(deviceY) + ", " +
    std::to_string(deviceZ) + ")");

  // Execute movement based on device type
  bool result = false;

  if (IsPIDevice(deviceId)) {
    if (m_piController) {
      // Get the specific PI controller for this device
      PIController* controller = m_piController->GetController(deviceId);
      if (controller && controller->IsConnected()) {
        // Move to absolute position
        // You'll need to adapt this to your PI controller API
        result = controller->MoveToPosition("X", deviceX) &&
          controller->MoveToPosition("Y", deviceY) &&
          controller->MoveToPosition("Z", deviceZ);

        if (result) {
          LogStatus("PI movement initiated for " + deviceId);
        }
      }
      else {
        LogStatus("PI controller not connected for " + deviceId);
      }
    }
    else {
      LogStatus("PI controller manager not available");
    }
  }
  else if (IsACSDevice(deviceId)) {
    if (m_acsController) {
      // Get the ACS controller for this device
      ACSController* controller = m_acsController->GetController(deviceId);
      if (controller && controller->IsConnected()) {
        // Move to absolute position using ACS API
        // Adapt to your ACS controller API
        result = controller->MoveToAbsolutePosition(deviceX, deviceY, deviceZ, velocity);

        if (result) {
          LogStatus("ACS movement initiated for " + deviceId);
        }
      }
      else {
        LogStatus("ACS controller not connected for " + deviceId);
      }
    }
    else {
      LogStatus("ACS controller manager not available");
    }
  }
  else {
    LogStatus("Unknown device type: " + deviceId);
  }

  return result;
}

bool GlobalMotionController::MoveRelativeGlobal(const std::string& deviceId,
  float deltaX, float deltaY, float deltaZ,
  float velocity) {
  // Get current position
  GlobalPosition currentPos = GetGlobalPosition(deviceId);

  // Calculate target
  float targetX = currentPos.x + deltaX;
  float targetY = currentPos.y + deltaY;
  float targetZ = currentPos.z + deltaZ;

  return MoveToGlobal(deviceId, targetX, targetY, targetZ, velocity);
}

bool GlobalMotionController::JogGlobal(const std::string& deviceId,
  int globalAxis, float distance,
  float velocity) {
  float deltaX = 0, deltaY = 0, deltaZ = 0;

  switch (globalAxis) {
  case 0: deltaX = distance; break;
  case 1: deltaY = distance; break;
  case 2: deltaZ = distance; break;
  default:
    LogStatus("Invalid axis index: " + std::to_string(globalAxis));
    return false;
  }

  return MoveRelativeGlobal(deviceId, deltaX, deltaY, deltaZ, velocity);
}

GlobalPosition GlobalMotionController::GetGlobalPosition(const std::string& deviceId) {
  float deviceX = 0, deviceY = 0, deviceZ = 0;

  // Get device position based on device type
  if (IsPIDevice(deviceId)) {
    if (m_piController) {
      PIController* controller = m_piController->GetController(deviceId);
      if (controller && controller->IsConnected()) {
        // Get position from PI controller
        // Adapt to your PI API
        deviceX = controller->GetAxisPositionFloat("X");
        deviceY = controller->GetAxisPositionFloat("Y");
        deviceZ = controller->GetAxisPositionFloat("Z");
      }
    }
  }
  else if (IsACSDevice(deviceId)) {
    if (m_acsController) {
      ACSController* controller = m_acsController->GetController(deviceId);
      if (controller && controller->IsConnected()) {
        // Get position from ACS controller
        // Adapt to your ACS API
        PositionStruct pos = controller->GetCurrentPosition();
        deviceX = pos.x;
        deviceY = pos.y;
        deviceZ = pos.z;
      }
    }
  }

  // Convert to global coordinates
  float globalX, globalY, globalZ;
  DeviceToGlobal(deviceId, deviceX, deviceY, deviceZ, globalX, globalY, globalZ);

  return GlobalPosition(globalX, globalY, globalZ, deviceId);
}

void GlobalMotionController::GlobalToDevice(const std::string& deviceId,
  float globalX, float globalY, float globalZ,
  float& deviceX, float& deviceY, float& deviceZ) const {
  auto it = m_transformMatrices.find(deviceId);
  if (it != m_transformMatrices.end()) {
    // Apply inverse transformation (transpose for rotation matrices)
    Matrix3x3 inverse = it->second.GetInverse();
    deviceX = globalX; deviceY = globalY; deviceZ = globalZ;
    inverse.Transform(deviceX, deviceY, deviceZ);
  }
  else {
    // No transformation, use identity
    deviceX = globalX;
    deviceY = globalY;
    deviceZ = globalZ;
  }
}

void GlobalMotionController::DeviceToGlobal(const std::string& deviceId,
  float deviceX, float deviceY, float deviceZ,
  float& globalX, float& globalY, float& globalZ) const {
  auto it = m_transformMatrices.find(deviceId);
  if (it != m_transformMatrices.end()) {
    // Apply forward transformation
    globalX = deviceX; globalY = deviceY; globalZ = deviceZ;
    it->second.Transform(globalX, globalY, globalZ);
  }
  else {
    // No transformation, use identity
    globalX = deviceX;
    globalY = deviceY;
    globalZ = deviceZ;
  }
}

bool GlobalMotionController::IsWithinGlobalLimits(float globalX, float globalY, float globalZ) const {
  return globalX >= m_globalLimits.minX && globalX <= m_globalLimits.maxX &&
    globalY >= m_globalLimits.minY && globalY <= m_globalLimits.maxY &&
    globalZ >= m_globalLimits.minZ && globalZ <= m_globalLimits.maxZ;
}

void GlobalMotionController::EmergencyStopGlobal() {
  std::lock_guard<std::mutex> lock(m_mutex);

  LogStatus("EMERGENCY STOP - All motion halted");

  // Stop all PI controllers
  if (m_piController) {
    // Stop all PI devices
    for (const auto& deviceId : GetAvailableDevices()) {
      if (IsPIDevice(deviceId)) {
        PIController* controller = m_piController->GetController(deviceId);
        if (controller && controller->IsConnected()) {
          controller->StopAllAxes();
        }
      }
    }
  }

  // Stop all ACS controllers
  if (m_acsController) {
    // Stop all ACS devices
    for (const auto& deviceId : GetAvailableDevices()) {
      if (IsACSDevice(deviceId)) {
        ACSController* controller = m_acsController->GetController(deviceId);
        if (controller && controller->IsConnected()) {
          controller->StopAllMotion();
        }
      }
    }
  }
}

std::vector<std::string> GlobalMotionController::GetAvailableDevices() const {
  std::vector<std::string> devices;
  for (const auto& [deviceId, matrix] : m_transformMatrices) {
    devices.push_back(deviceId);
  }
  return devices;
}

bool GlobalMotionController::IsDeviceAvailable(const std::string& deviceId) const {
  return m_transformMatrices.find(deviceId) != m_transformMatrices.end();
}

bool GlobalMotionController::IsPIDevice(const std::string& deviceId) const {
  return deviceId.find("hex") != std::string::npos;
}

bool GlobalMotionController::IsACSDevice(const std::string& deviceId) const {
  return deviceId.find("gantry") != std::string::npos;
}

void GlobalMotionController::LogStatus(const std::string& message) const {
  if (m_statusCallback) {
    m_statusCallback(message);
  }
  else {
    // Fallback to console output
    std::cout << "[GlobalMotion] " << message << std::endl;
  }

  // Also log to system logger if available
  auto* logger = Logger::GetInstance();
  if (logger) {
    logger->LogInfo("GlobalMotion: " + message);
  }
}

bool GlobalMotionController::IsDeviceMoving(const std::string& deviceId) const {
  // Check if device exists
  if (!IsDeviceAvailable(deviceId)) {
    LogStatus("Device not available for movement check: " + deviceId);
    return false;
  }

  bool isMoving = false;

  if (IsPIDevice(deviceId)) {
    if (m_piController) {
      PIController* controller = m_piController->GetController(deviceId);
      if (controller && controller->IsConnected()) {
        // Check all axes for motion
        isMoving = controller->IsMoving("X") ||
          controller->IsMoving("Y") ||
          controller->IsMoving("Z");
      }
    }
  }
  else if (IsACSDevice(deviceId)) {
    if (m_acsController) {
      ACSController* controller = m_acsController->GetController(deviceId);
      if (controller && controller->IsConnected()) {
        // Check ACS motion status
        // Adapt to your ACS API - this might be something like:
        isMoving = controller->IsMoving("X") ||
          controller->IsMoving("Y")||
          controller->IsMoving("Z");
        // or
        // isMoving = controller->GetMotionStatus() != 0;
      }
    }
  }

  return isMoving;
}

bool GlobalMotionController::IsAnyDeviceMoving() const {
  for (const auto& deviceId : GetAvailableDevices()) {
    if (IsDeviceMoving(deviceId)) {
      return true;
    }
  }
  return false;
}

bool GlobalMotionController::MoveToGlobalAsync(const std::string& deviceId,
  float globalX, float globalY, float globalZ,
  float velocity) {
  // Check if already moving
  if (m_movementPending) {
    LogStatus("Movement already in progress");
    return false;
  }

  // Validate
  if (!IsDeviceAvailable(deviceId)) {
    LogStatus("Device not available: " + deviceId);
    return false;
  }

  if (!IsWithinGlobalLimits(globalX, globalY, globalZ)) {
    LogStatus("Target position outside global limits");
    return false;
  }

  // Convert to device coordinates
  float deviceX, deviceY, deviceZ;
  GlobalToDevice(deviceId, globalX, globalY, globalZ, deviceX, deviceY, deviceZ);

  // Launch async movement
  m_movementPending = true;

  // If previous thread exists, wait for it
  if (m_moveThread.joinable()) {
    m_moveThread.join();
  }

  // Start new movement thread
  m_moveThread = std::thread(&GlobalMotionController::ExecuteMovementThread, this,
    deviceId, deviceX, deviceY, deviceZ, velocity);

  LogStatus("Async movement initiated for " + deviceId);
  return true;
}

void GlobalMotionController::ExecuteMovementThread(const std::string& deviceId,
  float deviceX, float deviceY, float deviceZ,
  float velocity) {
  bool result = false;

  try {
    if (IsPIDevice(deviceId)) {
      if (m_piController) {
        PIController* controller = m_piController->GetController(deviceId);
        if (controller && controller->IsConnected()) {
          // Set velocity first

          controller->SetSystemVelocity(velocity);
          //controller->SetVelocity("X", velocity);
          //controller->SetVelocity("Y", velocity);
          //controller->SetVelocity("Z", velocity);

          // Move to position
          result = controller->MoveToPosition("X", deviceX) &&
            controller->MoveToPosition("Y", deviceY) &&
            controller->MoveToPosition("Z", deviceZ);

          // Wait for motion to complete
          while (controller->IsMoving("X") ||
            controller->IsMoving("Y") ||
            controller->IsMoving("Z")) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
          }
        }
      }
    }
    else if (IsACSDevice(deviceId)) {
      if (m_acsController) {
        ACSController* controller = m_acsController->GetController(deviceId);
        if (controller && controller->IsConnected()) {
          result = controller->MoveToAbsolutePosition(deviceX, deviceY, deviceZ, velocity);

          // Wait for motion to complete
          while (controller->IsInMotion()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
          }
        }
      }
    }

    LogStatus(result ? "Movement completed" : "Movement failed");
  }
  catch (const std::exception& e) {
    LogStatus("Movement error: " + std::string(e.what()));
  }

  m_movementPending = false;
}

bool GlobalMotionController::JogGlobalAsync(const std::string& deviceId,
  int globalAxis, float distance,
  float velocity) {
  // Get current position
  GlobalPosition currentPos = GetGlobalPosition(deviceId);

  // Calculate target
  float targetX = currentPos.x;
  float targetY = currentPos.y;
  float targetZ = currentPos.z;

  switch (globalAxis) {
  case 0: targetX += distance; break;
  case 1: targetY += distance; break;
  case 2: targetZ += distance; break;
  default:
    LogStatus("Invalid axis index: " + std::to_string(globalAxis));
    return false;
  }

  return MoveToGlobalAsync(deviceId, targetX, targetY, targetZ, velocity);
}

