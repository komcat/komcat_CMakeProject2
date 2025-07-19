// motion_ops_pi_specific.cpp
#include "include/ops/motion_ops.h"
#include <algorithm>

// PI-specific device status and movement methods implementation

bool MotionOps::IsDeviceConnected(const std::string& deviceName) {
  // Check if it's a PI controller
  PIController* piController = m_piControllerManager.GetController(deviceName);
  if (piController) {
    return piController->IsConnected();
  }

  // If not a PI controller, check if it's an ACS controller
  auto deviceOpt = m_motionLayer.GetConfigManager().GetDevice(deviceName);
  if (!deviceOpt.has_value()) {
    m_logger->LogWarning("Device " + deviceName + " not found in configuration");
    return false;
  }

  const auto& device = deviceOpt.value().get();
  if (device.Port == 701) { // ACS controller typically uses port 701
    // Access the ACS controller manager through motion layer
    ACSController* acsController = m_motionLayer.GetACSControllerManager().GetController(deviceName);
    if (acsController) {
      return acsController->IsConnected();
    }
  }

  // Device not found in any controller manager
  m_logger->LogWarning("Device " + deviceName + " not found in any controller manager");
  return false;
}

bool MotionOps::IsDevicePIController(const std::string& deviceName) const {
  // Get the device info from configuration (using motion layer, which has access to config)
  auto deviceOpt = m_motionLayer.GetConfigManager().GetDevice(deviceName);
  if (!deviceOpt.has_value()) {
    m_logger->LogError("MotionOps: Device " + deviceName + " not found in configuration");
    return false;
  }

  const auto& device = deviceOpt.value().get();

  // PI controllers use port 50000, ACS uses different ports (like 701)
  return (device.Port == 50000);
}

bool MotionOps::IsDeviceMoving(const std::string& deviceName) {
  // Determine which controller manager to use
  if (IsDevicePIController(deviceName)) {
    PIController* controller = m_piControllerManager.GetController(deviceName);
    if (!controller || !controller->IsConnected()) {
      m_logger->LogError("MotionOps: No connected PI controller for device " + deviceName);
      return false;
    }

    // Check all axes for motion using the existing IsMoving method
    for (const auto& axis : { "X", "Y", "Z", "U", "V", "W" }) {
      if (controller->IsMoving(axis)) {
        return true;
      }
    }

    return false;
  }
  else {
    // For ACS controllers or other devices
    // Fall back to position change detection if we can't check directly

    PositionStruct currentPos;
    if (!m_motionLayer.GetCurrentPosition(deviceName, currentPos)) {
      // Can't determine position
      return false;
    }

    // Store last positions and check times
    static std::map<std::string, PositionStruct> lastPositions;
    static std::map<std::string, std::chrono::steady_clock::time_point> lastCheckTimes;

    auto now = std::chrono::steady_clock::now();

    // First time checking this device
    if (lastPositions.find(deviceName) == lastPositions.end()) {
      lastPositions[deviceName] = currentPos;
      lastCheckTimes[deviceName] = now;
      return false; // Assume not moving on first check
    }

    // Get time since last check
    auto lastTime = lastCheckTimes[deviceName];
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime).count();

    // Only check if enough time has passed
    if (elapsed < 100) { // Less than 100ms since last check
      return false;
    }

    // Compare positions
    const auto& lastPos = lastPositions[deviceName];
    double tolerance = 0.0001; // 0.1 micron position change detection threshold

    bool posChanged =
      std::abs(currentPos.x - lastPos.x) > tolerance ||
      std::abs(currentPos.y - lastPos.y) > tolerance ||
      std::abs(currentPos.z - lastPos.z) > tolerance ||
      std::abs(currentPos.u - lastPos.u) > tolerance ||
      std::abs(currentPos.v - lastPos.v) > tolerance ||
      std::abs(currentPos.w - lastPos.w) > tolerance;

    // Update stored values
    lastPositions[deviceName] = currentPos;
    lastCheckTimes[deviceName] = now;

    return posChanged;
  }
}