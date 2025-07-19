// motion_ops_position_related.cpp
#include "motion_ops.h"
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>

// Position-related methods implementation

std::string MotionOps::GetDeviceCurrentNode(const std::string& deviceName, const std::string& graphName) {
  if (m_enableDebug) m_logger->LogInfo("MotionOps: Getting current node for device " + deviceName +
    " in graph " + graphName);

  std::string currentNodeId;
  if (!m_motionLayer.GetDeviceCurrentNode(graphName, deviceName, currentNodeId)) {
    if (m_enableDebug) m_logger->LogError("MotionOps: Failed to get current node for device " + deviceName);
    return "";
  }

  return currentNodeId;
}

std::string MotionOps::GetDeviceCurrentPositionName(const std::string& deviceName) {
  if (m_enableDebug) m_logger->LogInfo("MotionOps: Getting current named position for device " + deviceName);

  // Get current position
  PositionStruct currentPosition;
  if (!GetDeviceCurrentPosition(deviceName, currentPosition)) {
    if (m_enableDebug) m_logger->LogError("MotionOps: Failed to get current position for device " + deviceName);
    return "";
  }

  // Get all named positions for the device
  auto& configManager = m_motionLayer.GetConfigManager();
  auto namedPositionsOpt = configManager.GetNamedPositions(deviceName);
  if (!namedPositionsOpt.has_value()) {
    m_logger->LogWarning("MotionOps: No named positions found for device " + deviceName);
    return "";
  }

  const auto& namedPositions = namedPositionsOpt.value().get();
  if (namedPositions.empty()) {
    return "";
  }

  // Find the closest named position
  std::string closestPosName;
  double minDistance = (std::numeric_limits<double>::max)();

  for (const auto& posEntry : namedPositions) {
    const auto& posName = posEntry.first;
    const auto& pos = posEntry.second;

    // Calculate distance (without considering rotation)
    double distance = GetDistanceBetweenPositions(currentPosition, pos, false);

    if (distance < minDistance) {
      minDistance = distance;
      closestPosName = posName;
    }
  }

  // If we're very close to a named position (within 0.1mm), consider we're at that position
  if (minDistance <= 0.1) {
    if (m_enableDebug) m_logger->LogInfo("MotionOps: Device " + deviceName +
      " is at named position " + closestPosName);
    return closestPosName;
  }

  // If no position is close enough, return empty string
  m_logger->LogInfo("MotionOps: Device " + deviceName +
    " is not at any named position (closest: " + closestPosName +
    ", distance: " + std::to_string(minDistance) + " mm)");
  return "";
}

bool MotionOps::GetDeviceCurrentPosition(const std::string& deviceName, PositionStruct& position) {
  if (m_enableDebug) m_logger->LogInfo("MotionOps: Getting current position for device " + deviceName);

  // Use the motion layer to get the current position
  if (!m_motionLayer.GetCurrentPosition(deviceName, position)) {
    m_logger->LogError("MotionOps: Failed to get current position for device " + deviceName);
    return false;
  }

  // Log position details
  std::stringstream posStr;
  posStr << "Current position - X:" << std::fixed << std::setprecision(6) << position.x
    << " Y:" << std::setprecision(6) << position.y
    << " Z:" << std::setprecision(6) << position.z;

  // Include rotation values if any are non-zero
  if (position.u != 0.0 || position.v != 0.0 || position.w != 0.0) {
    posStr << " U:" << std::setprecision(6) << position.u
      << " V:" << std::setprecision(6) << position.v
      << " W:" << std::setprecision(6) << position.w;
  }

  if (m_enableDebug) m_logger->LogInfo("MotionOps: " + posStr.str());
  return true;
}

double MotionOps::GetDistanceBetweenPositions(const PositionStruct& pos1,
  const PositionStruct& pos2,
  bool includeRotation) const {
  // Calculate Euclidean distance for XYZ
  double dx = pos1.x - pos2.x;
  double dy = pos1.y - pos2.y;
  double dz = pos1.z - pos2.z;

  double distance = std::sqrt(dx * dx + dy * dy + dz * dz);

  // If rotation should be included
  if (includeRotation) {
    double du = pos1.u - pos2.u;
    double dv = pos1.v - pos2.v;
    double dw = pos1.w - pos2.w;

    // Add weighted rotation component to distance
    // Weight rotation less than translation (scale factor 0.1)
    double rotationFactor = 0.1;
    double rotDistance = std::sqrt(du * du + dv * dv + dw * dw) * rotationFactor;

    // Combine the distances
    distance = std::sqrt(distance * distance + rotDistance * rotDistance);
  }

  return distance;
}

// Position storage methods
bool MotionOps::CaptureCurrentPosition(const std::string& deviceName, const std::string& label) {
  if (label.empty()) {
    m_logger->LogError("MotionOps: Cannot capture position with empty label");
    return false;
  }

  m_logger->LogInfo("MotionOps: Capturing current position for device " + deviceName +
    " with label '" + label + "'");

  // Get current position
  PositionStruct currentPosition;
  if (!GetDeviceCurrentPosition(deviceName, currentPosition)) {
    m_logger->LogError("MotionOps: Failed to get current position for device " + deviceName);
    return false;
  }

  // Store the position
  {
    std::lock_guard<std::mutex> lock(m_positionStorageMutex);
    m_storedPositions[label] = StoredPositionInfo(deviceName, currentPosition);
  }

  m_logger->LogInfo("MotionOps: Successfully stored position '" + label + "' for device " +
    deviceName + " at coordinates: X=" + std::to_string(currentPosition.x) +
    " Y=" + std::to_string(currentPosition.y) +
    " Z=" + std::to_string(currentPosition.z));

  return true;
}

bool MotionOps::GetStoredPosition(const std::string& label, PositionStruct& position) const {
  std::lock_guard<std::mutex> lock(m_positionStorageMutex);

  auto it = m_storedPositions.find(label);
  if (it == m_storedPositions.end()) {
    m_logger->LogWarning("MotionOps: Stored position '" + label + "' not found");
    return false;
  }

  position = it->second.position;
  return true;
}

std::vector<std::string> MotionOps::GetStoredPositionLabels(const std::string& deviceNameFilter) const {
  std::vector<std::string> labels;
  std::lock_guard<std::mutex> lock(m_positionStorageMutex);

  for (const auto& [label, info] : m_storedPositions) {
    if (deviceNameFilter.empty() || info.deviceName == deviceNameFilter) {
      labels.push_back(label);
    }
  }

  return labels;
}

double MotionOps::CalculateDistanceFromStored(const std::string& deviceName, const std::string& storedLabel) {
  // Get stored position
  PositionStruct storedPosition;
  if (!GetStoredPosition(storedLabel, storedPosition)) {
    m_logger->LogError("MotionOps: Cannot calculate distance - stored position '" +
      storedLabel + "' not found");
    return -1.0;
  }

  // Verify the stored position is for the correct device
  {
    std::lock_guard<std::mutex> lock(m_positionStorageMutex);
    auto it = m_storedPositions.find(storedLabel);
    if (it != m_storedPositions.end() && it->second.deviceName != deviceName) {
      m_logger->LogWarning("MotionOps: Stored position '" + storedLabel +
        "' is for device '" + it->second.deviceName +
        "', not '" + deviceName + "'");
    }
  }

  // Get current position
  PositionStruct currentPosition;
  if (!GetDeviceCurrentPosition(deviceName, currentPosition)) {
    m_logger->LogError("MotionOps: Cannot get current position for device " + deviceName);
    return -1.0;
  }

  // Calculate distance using the const method
  double distance = GetDistanceBetweenPositions(currentPosition, storedPosition, false);

  m_logger->LogInfo("MotionOps: Distance from stored position '" + storedLabel +
    "' to current position of " + deviceName + ": " +
    std::to_string(distance) + " mm");

  return distance;
}

bool MotionOps::HasMovedFromStored(const std::string& deviceName,
  const std::string& storedLabel,
  double tolerance) {
  double distance = CalculateDistanceFromStored(deviceName, storedLabel);

  if (distance < 0) {
    // Error occurred in distance calculation
    return false;
  }

  bool hasMoved = distance > tolerance;

  if (hasMoved) {
    m_logger->LogInfo("MotionOps: Device " + deviceName + " has moved " +
      std::to_string(distance) + " mm from stored position '" +
      storedLabel + "' (tolerance: " + std::to_string(tolerance) + " mm)");
  }

  return hasMoved;
}

void MotionOps::ClearStoredPositions(const std::string& deviceNameFilter) {
  std::lock_guard<std::mutex> lock(m_positionStorageMutex);

  if (deviceNameFilter.empty()) {
    // Clear all stored positions
    size_t clearedCount = m_storedPositions.size();
    m_storedPositions.clear();
    m_logger->LogInfo("MotionOps: Cleared all " + std::to_string(clearedCount) +
      " stored positions");
  }
  else {
    // Clear positions for specific device
    size_t clearedCount = 0;
    auto it = m_storedPositions.begin();
    while (it != m_storedPositions.end()) {
      if (it->second.deviceName == deviceNameFilter) {
        it = m_storedPositions.erase(it);
        clearedCount++;
      }
      else {
        ++it;
      }
    }
    m_logger->LogInfo("MotionOps: Cleared " + std::to_string(clearedCount) +
      " stored positions for device '" + deviceNameFilter + "'");
  }
}

void MotionOps::ClearOldStoredPositions(int maxAgeMinutes) {
  std::lock_guard<std::mutex> lock(m_positionStorageMutex);

  auto cutoffTime = std::chrono::steady_clock::now() - std::chrono::minutes(maxAgeMinutes);
  size_t clearedCount = 0;

  auto it = m_storedPositions.begin();
  while (it != m_storedPositions.end()) {
    if (it->second.timestamp < cutoffTime) {
      m_logger->LogInfo("MotionOps: Removing old stored position '" + it->first +
        "' for device '" + it->second.deviceName + "'");
      it = m_storedPositions.erase(it);
      clearedCount++;
    }
    else {
      ++it;
    }
  }

  if (clearedCount > 0) {
    m_logger->LogInfo("MotionOps: Cleared " + std::to_string(clearedCount) +
      " stored positions older than " + std::to_string(maxAgeMinutes) + " minutes");
  }
}

bool MotionOps::GetStoredPositionInfo(const std::string& label,
  std::string& deviceName,
  std::chrono::steady_clock::time_point& timestamp) const {
  std::lock_guard<std::mutex> lock(m_positionStorageMutex);

  auto it = m_storedPositions.find(label);
  if (it == m_storedPositions.end()) {
    return false;
  }

  deviceName = it->second.deviceName;
  timestamp = it->second.timestamp;
  return true;
}

// Position monitoring and cache methods
std::map<std::string, PositionStruct> MotionOps::GetCurrentPositions() {
  if (m_enableDebug) m_logger->LogInfo("MotionOps: Getting current positions for all controllers");

  std::lock_guard<std::mutex> lock(m_currentPositionsMutex);

  // Check if cache is still valid (within timeout)
  auto now = std::chrono::steady_clock::now();
  if (now - m_lastPositionUpdate < POSITION_CACHE_TIMEOUT && !m_currentPositions.empty()) {
    if (m_enableDebug) m_logger->LogInfo("MotionOps: Returning cached positions (" +
      std::to_string(m_currentPositions.size()) + " devices)");
    return m_currentPositions;
  }

  // Clear existing positions
  m_currentPositions.clear();

  // Get all available devices from the motion layer
  std::vector<std::string> deviceList = m_motionLayer.GetAvailableDevices();

  if (deviceList.empty()) {
    m_logger->LogWarning("MotionOps: No devices available for position reading");
    return m_currentPositions;
  }

  // Get current position for each connected device
  int successCount = 0;
  for (const auto& deviceName : deviceList) {
    // Check if device is connected before trying to get position
    if (!IsDeviceConnected(deviceName)) {
      if (m_enableDebug) m_logger->LogWarning("MotionOps: Device " + deviceName + " is not connected, skipping");
      continue;
    }

    PositionStruct currentPosition;
    if (GetDeviceCurrentPosition(deviceName, currentPosition)) {
      m_currentPositions[deviceName] = currentPosition;
      successCount++;

      if (m_enableDebug) m_logger->LogInfo("MotionOps: Got position for " + deviceName +
        " - X:" + std::to_string(currentPosition.x) +
        " Y:" + std::to_string(currentPosition.y) +
        " Z:" + std::to_string(currentPosition.z));
    }
    else {
      m_logger->LogError("MotionOps: Failed to get current position for device " + deviceName);
    }
  }

  // Update cache timestamp
  m_lastPositionUpdate = now;

  if (m_enableDebug) m_logger->LogInfo("MotionOps: Successfully retrieved positions for " +
    std::to_string(successCount) + " out of " +
    std::to_string(deviceList.size()) + " devices");

  return m_currentPositions;
}

bool MotionOps::UpdateAllCurrentPositions() {
  if (m_enableDebug) m_logger->LogInfo("MotionOps: Updating all current positions (forced refresh)");

  std::lock_guard<std::mutex> lock(m_currentPositionsMutex);

  // Force cache invalidation by setting old timestamp
  m_lastPositionUpdate = std::chrono::steady_clock::time_point{};

  // Clear current cache
  m_currentPositions.clear();

  // Get all available devices
  std::vector<std::string> deviceList = m_motionLayer.GetAvailableDevices();

  if (deviceList.empty()) {
    m_logger->LogWarning("MotionOps: No devices available for position update");
    return false;
  }

  // Update positions for all connected devices
  int successCount = 0;
  for (const auto& deviceName : deviceList) {
    if (!IsDeviceConnected(deviceName)) {
      continue;
    }

    PositionStruct currentPosition;
    if (GetDeviceCurrentPosition(deviceName, currentPosition)) {
      m_currentPositions[deviceName] = currentPosition;
      successCount++;
    }
  }

  // Update timestamp
  m_lastPositionUpdate = std::chrono::steady_clock::now();

  bool allSuccess = (successCount == static_cast<int>(deviceList.size()));
  if (allSuccess) {
    if (m_enableDebug) m_logger->LogInfo("MotionOps: Successfully updated all " +
      std::to_string(successCount) + " device positions");
  }
  else {
    m_logger->LogWarning("MotionOps: Updated " + std::to_string(successCount) +
      " out of " + std::to_string(deviceList.size()) + " device positions");
  }

  return allSuccess;
}

const std::map<std::string, PositionStruct>& MotionOps::GetCachedPositions() const {
  std::lock_guard<std::mutex> lock(m_currentPositionsMutex);
  return m_currentPositions;
}

void MotionOps::RefreshPositionCache() {
  m_logger->LogInfo("MotionOps: Refreshing position cache");

  // Call GetCurrentPositions with forced refresh
  std::lock_guard<std::mutex> lock(m_currentPositionsMutex);
  m_lastPositionUpdate = std::chrono::steady_clock::time_point{}; // Force refresh

  // Release lock and call GetCurrentPositions (which will reacquire lock)
  {
    std::unique_lock<std::mutex> tempLock(m_currentPositionsMutex);
    tempLock.unlock();
    GetCurrentPositions();
  }
}

// Configuration management methods
bool MotionOps::SaveCurrentPositionToConfig(const std::string& deviceName, const std::string& positionName) {
  m_logger->LogInfo("MotionOps: Delegating position save to MotionControlLayer");

  // Validate device name and position name
  if (deviceName.empty() || positionName.empty()) {
    m_logger->LogError("MotionOps: Device name and position name cannot be empty");
    return false;
  }

  // Check if device is connected before attempting to save
  if (!IsDeviceConnected(deviceName)) {
    m_logger->LogError("MotionOps: Device " + deviceName + " is not connected");
    return false;
  }

  // Check if device is moving (optional safety check)
  if (IsDeviceMoving(deviceName)) {
    m_logger->LogWarning("MotionOps: Device " + deviceName + " is currently moving, position may not be stable");
  }

  // Delegate to MotionControlLayer which has proper access to MotionConfigManager
  bool success = m_motionLayer.SaveCurrentPositionToConfig(deviceName, positionName);

  if (success) {
    m_logger->LogInfo("MotionOps: Successfully saved position '" + positionName +
      "' for device " + deviceName + " to motion_config.json");
  }
  else {
    m_logger->LogError("MotionOps: Failed to save position '" + positionName +
      "' for device " + deviceName);
  }

  return success;
}

bool MotionOps::UpdateNamedPositionInConfig(const std::string& deviceName, const std::string& positionName) {
  m_logger->LogInfo("MotionOps: Delegating position update to MotionControlLayer");

  // Validate inputs
  if (deviceName.empty() || positionName.empty()) {
    m_logger->LogError("MotionOps: Device name and position name cannot be empty");
    return false;
  }

  // Check if device is connected
  if (!IsDeviceConnected(deviceName)) {
    m_logger->LogError("MotionOps: Device " + deviceName + " is not connected");
    return false;
  }

  // Delegate to MotionControlLayer
  bool success = m_motionLayer.UpdateNamedPositionInConfig(deviceName, positionName);

  if (success) {
    m_logger->LogInfo("MotionOps: Successfully updated position '" + positionName +
      "' for device " + deviceName);
  }
  else {
    m_logger->LogError("MotionOps: Failed to update position '" + positionName +
      "' for device " + deviceName);
  }

  return success;
}

bool MotionOps::SaveAllCurrentPositionsToConfig(const std::string& prefix) {
  m_logger->LogInfo("MotionOps: Delegating bulk position save to MotionControlLayer");

  // Get current positions to check if any devices are available
  auto allPositions = GetCurrentPositions();

  if (allPositions.empty()) {
    m_logger->LogWarning("MotionOps: No controller positions available to save");
    return false;
  }

  // Log what we're about to save
  m_logger->LogInfo("MotionOps: Saving positions for " +
    std::to_string(allPositions.size()) + " devices");

  // Delegate to MotionControlLayer
  bool success = m_motionLayer.SaveAllCurrentPositionsToConfig(prefix);

  if (success) {
    m_logger->LogInfo("MotionOps: Successfully saved all current positions to configuration");
  }
  else {
    m_logger->LogError("MotionOps: Failed to save some current positions to configuration");
  }

  return success;
}

bool MotionOps::ReloadMotionConfig() {
  m_logger->LogInfo("MotionOps: Forcing motion configuration reload");

  bool success = m_motionLayer.ReloadMotionConfig();

  if (success) {
    m_logger->LogInfo("MotionOps: Motion configuration reloaded successfully");
  }
  else {
    m_logger->LogError("MotionOps: Failed to reload motion configuration");
  }

  return success;
}

bool MotionOps::BackupMotionConfig(const std::string& backupSuffix) {
  m_logger->LogInfo("MotionOps: Delegating config backup to MotionControlLayer");

  bool success = m_motionLayer.BackupMotionConfig(backupSuffix);

  if (success) {
    m_logger->LogInfo("MotionOps: Successfully created configuration backup");
  }
  else {
    m_logger->LogError("MotionOps: Failed to create configuration backup");
  }

  return success;
}

bool MotionOps::RestoreMotionConfigFromBackup(const std::string& backupSuffix) {
  m_logger->LogInfo("MotionOps: Restoring configuration from backup");

  try {
    std::string configPath = "motion_config.json";
    std::string backupPath = "motion_config_backup_" + backupSuffix + ".json";

    // Check if backup file exists
    if (!std::filesystem::exists(backupPath)) {
      m_logger->LogError("MotionOps: Backup file not found: " + backupPath);
      return false;
    }

    // Create backup of current config before restore
    if (!m_motionLayer.BackupMotionConfig("before_restore")) {
      m_logger->LogWarning("MotionOps: Failed to backup current config before restore");
    }

    // Copy backup file over current config
    std::filesystem::copy_file(backupPath, configPath, std::filesystem::copy_options::overwrite_existing);

    m_logger->LogInfo("MotionOps: Restored config from backup: " + backupPath);

    // Notify about configuration reload requirement
    m_logger->LogInfo("MotionOps: Configuration file restored successfully");
    m_logger->LogWarning("MotionOps: Please restart application to use restored configuration");

    return true;

  }
  catch (const std::exception& e) {
    m_logger->LogError("MotionOps: Failed to restore from backup: " + std::string(e.what()));
    return false;
  }
}

bool MotionOps::SaveCurrentPositionForNode(const std::string& deviceName, const std::string& graphName, const std::string& nodeId) {
  m_logger->LogInfo("MotionOps: Saving current position for node " + nodeId +
    " in graph " + graphName + " for device " + deviceName);

  // Validate inputs
  if (deviceName.empty() || graphName.empty() || nodeId.empty()) {
    m_logger->LogError("MotionOps: Device name, graph name, and node ID cannot be empty");
    return false;
  }

  // Check if device is connected
  if (!IsDeviceConnected(deviceName)) {
    m_logger->LogError("MotionOps: Device " + deviceName + " is not connected");
    return false;
  }

  try {
    // Get the graph from config manager
    auto graphOpt = m_motionLayer.GetMotionConfigManager().GetGraph(graphName);
    if (!graphOpt.has_value()) {
      m_logger->LogError("MotionOps: Graph not found: " + graphName);
      return false;
    }

    const auto& graph = graphOpt.value().get();

    // Find the node in the graph
    const Node* targetNode = nullptr;
    for (const auto& node : graph.Nodes) {
      if (node.Id == nodeId && node.Device == deviceName) {
        targetNode = &node;
        break;
      }
    }

    if (!targetNode) {
      m_logger->LogError("MotionOps: Node " + nodeId + " not found for device " + deviceName + " in graph " + graphName);
      return false;
    }

    // Get the actual position name from the node
    std::string actualPositionName = targetNode->Position;

    if (actualPositionName.empty()) {
      m_logger->LogError("MotionOps: Node " + nodeId + " has no position name defined");
      return false;
    }

    m_logger->LogInfo("MotionOps: Node " + nodeId + " refers to position '" + actualPositionName + "'");

    // Now save the current position using the correct position name
    bool success = m_motionLayer.SaveCurrentPositionToConfig(deviceName, actualPositionName);

    if (success) {
      m_logger->LogInfo("MotionOps: Successfully saved current position of " + deviceName +
        " to position '" + actualPositionName + "' (referenced by node " + nodeId + ")");
    }

    return success;
  }
  catch (const std::exception& e) {
    m_logger->LogError("MotionOps: Exception while saving position for node: " + std::string(e.what()));
    return false;
  }
}