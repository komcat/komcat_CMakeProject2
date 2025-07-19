// motion_ops_movement.cpp
#include "motion_ops.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

// Core movement methods implementation

bool MotionOps::MoveDeviceToNode(const std::string& deviceName,
  const std::string& graphName,
  const std::string& targetNodeId,
  bool blocking,
  const std::string& callerContext) {

  // Start operation tracking
  std::string opId;
  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"graph_name", graphName},
        {"target_node", targetNodeId},
        {"blocking", blocking ? "true" : "false"}
    };

    // Extract sequence name from caller context
    std::string sequenceName = "";
    if (callerContext.find("Initialization") != std::string::npos) {
      sequenceName = "Initialization";
    }
    else if (callerContext.find("ProcessStep") != std::string::npos) {
      sequenceName = "Process";
    }
    else if (callerContext.find("Cleanup") != std::string::npos) {
      sequenceName = "Cleanup";
    }

    opId = m_resultsManager->StartOperation("MoveDeviceToNode", deviceName,
      callerContext, sequenceName, parameters);
  }

  m_logger->LogInfo("MotionOps: Moving device " + deviceName +
    " to node " + targetNodeId + " in graph " + graphName +
    (callerContext.empty() ? "" : " (called by: " + callerContext + ")") +
    (opId.empty() ? "" : " [" + opId + "]"));

  // Get start position for result tracking
  PositionStruct startPos;
  bool hasStartPos = false;
  if (m_resultsManager && !opId.empty()) {
    hasStartPos = m_motionLayer.GetCurrentPosition(deviceName, startPos);
    if (hasStartPos) {
      StorePositionResult(opId, "start", startPos);
    }
  }

  // Get the current node for the device
  std::string currentNodeId;
  if (!m_motionLayer.GetDeviceCurrentNode(graphName, deviceName, currentNodeId)) {
    m_logger->LogError("MotionOps: Failed to get current node for device " + deviceName);

    // If we can't determine the current node, try to get positions and compare
    PositionStruct currentPos;
    if (m_motionLayer.GetCurrentPosition(deviceName, currentPos)) {
      m_logger->LogInfo("MotionOps: Current position: X=" + std::to_string(currentPos.x) +
        " Y=" + std::to_string(currentPos.y) +
        " Z=" + std::to_string(currentPos.z));

      // Try to get the target node's position
      try {
        auto graphOpt = m_motionLayer.GetConfigManager().GetGraph(graphName);
        if (graphOpt.has_value()) {
          const auto& graph = graphOpt.value().get();

          const Node* targetNode = nullptr;
          for (const auto& node : graph.Nodes) {
            if (node.Id == targetNodeId && node.Device == deviceName) {
              targetNode = &node;
              break;
            }
          }

          if (targetNode && !targetNode->Position.empty()) {
            auto posOpt = m_motionLayer.GetConfigManager().GetNamedPosition(deviceName, targetNode->Position);
            if (posOpt.has_value()) {
              const auto& targetPos = posOpt.value().get();

              m_logger->LogInfo("MotionOps: Target node position: X=" + std::to_string(targetPos.x) +
                " Y=" + std::to_string(targetPos.y) +
                " Z=" + std::to_string(targetPos.z));

              double distance = GetDistanceBetweenPositions(currentPos, targetPos, false);
              m_logger->LogInfo("MotionOps: Distance to target: " + std::to_string(distance) + " mm");

              if (distance < 0.1) { // Within 0.1mm, consider we're already there
                m_logger->LogInfo("MotionOps: Device appears to be at target node based on position proximity");

                // Store success result
                if (m_resultsManager && !opId.empty()) {
                  m_resultsManager->StoreResult(opId, "distance_moved", std::to_string(distance));
                  m_resultsManager->StoreResult(opId, "already_at_target", "true");
                  StorePositionResult(opId, "final", currentPos);
                  m_resultsManager->EndOperation(opId, "success");
                }

                return true;
              }
            }
          }
        }
      }
      catch (const std::exception& e) {
        m_logger->LogError("MotionOps: Exception while checking node position: " + std::string(e.what()));
      }
    }

    // Store failure result
    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->EndOperation(opId, "failed", "Failed to get current node for device");
    }
    return false;
  }

  // Already at the target node
  if (currentNodeId == targetNodeId) {
    m_logger->LogInfo("MotionOps: Device " + deviceName + " is already at node " + targetNodeId);

    // Store success result
    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->StoreResult(opId, "distance_moved", "0.0");
      m_resultsManager->StoreResult(opId, "already_at_target", "true");
      PositionStruct currentPos;
      if (m_motionLayer.GetCurrentPosition(deviceName, currentPos)) {
        StorePositionResult(opId, "final", currentPos);
      }
      m_resultsManager->EndOperation(opId, "success");
    }

    return true;
  }

  // Plan and execute path
  bool success = MovePathFromTo(deviceName, graphName, currentNodeId, targetNodeId, blocking, callerContext);

  // Store final results
  if (m_resultsManager && !opId.empty()) {
    if (success) {
      // Get final position and calculate distance
      PositionStruct finalPos;
      if (m_motionLayer.GetCurrentPosition(deviceName, finalPos)) {
        StorePositionResult(opId, "final", finalPos);

        if (hasStartPos) {
          double distance = GetDistanceBetweenPositions(startPos, finalPos);
          m_resultsManager->StoreResult(opId, "distance_moved", std::to_string(distance));
        }
      }
      m_resultsManager->StoreResult(opId, "current_node", currentNodeId);
      m_resultsManager->StoreResult(opId, "target_node", targetNodeId);
      m_resultsManager->EndOperation(opId, "success");
    }
    else {
      m_resultsManager->EndOperation(opId, "failed", "Path execution failed");
    }
  }

  return success;
}

bool MotionOps::MovePathFromTo(const std::string& deviceName,
  const std::string& graphName,
  const std::string& startNodeId,
  const std::string& endNodeId,
  bool blocking,
  const std::string& callerContext) {

  // Start operation tracking
  std::string opId;
  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"graph_name", graphName},
        {"start_node", startNodeId},
        {"end_node", endNodeId},
        {"blocking", blocking ? "true" : "false"}
    };
    opId = m_resultsManager->StartOperation("MovePathFromTo", deviceName, callerContext, "", parameters);
  }

  m_logger->LogInfo("MotionOps: Planning path for device " + deviceName +
    " from " + startNodeId + " to " + endNodeId + " in graph " + graphName);

  // Store start position if tracking enabled
  PositionStruct startPos;
  bool hasStartPos = false;
  if (m_resultsManager && !opId.empty()) {
    if (m_motionLayer.GetCurrentPosition(deviceName, startPos)) {
      StorePositionResult(opId, "start", startPos);
      hasStartPos = true;
    }
  }

  // Plan the path
  if (!m_motionLayer.PlanPath(graphName, startNodeId, endNodeId)) {
    m_logger->LogError("MotionOps: Failed to plan path from " +
      startNodeId + " to " + endNodeId);

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->EndOperation(opId, "failed", "Path planning failed");
    }
    return false;
  }

  // Execute the path
  m_logger->LogInfo("MotionOps: Executing path");
  bool success = m_motionLayer.ExecutePath(blocking);

  // Store final results
  if (m_resultsManager && !opId.empty()) {
    if (success) {
      // Get final position and calculate path distance
      PositionStruct finalPos;
      if (m_motionLayer.GetCurrentPosition(deviceName, finalPos)) {
        StorePositionResult(opId, "final", finalPos);

        if (hasStartPos) {
          double pathDistance = GetDistanceBetweenPositions(startPos, finalPos);
          m_resultsManager->StoreResult(opId, "path_distance", std::to_string(pathDistance));
        }
      }

      m_resultsManager->StoreResult(opId, "start_node", startNodeId);
      m_resultsManager->StoreResult(opId, "end_node", endNodeId);
      m_resultsManager->EndOperation(opId, "success");
    }
    else {
      m_resultsManager->EndOperation(opId, "failed", "Path execution failed");
    }
  }

  if (success) {
    m_logger->LogInfo("MotionOps: Path execution " +
      std::string(blocking ? "completed" : "started"));
  }
  else {
    m_logger->LogError("MotionOps: Path execution failed");
  }

  return success;
}

bool MotionOps::MoveToPointName(const std::string& deviceName,
  const std::string& positionName,
  bool blocking,
  const std::string& callerContext) {

  // Start operation tracking
  std::string opId;
  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"position_name", positionName},
        {"blocking", blocking ? "true" : "false"}
    };
    opId = m_resultsManager->StartOperation("MoveToPointName", deviceName, callerContext, "", parameters);
  }

  m_logger->LogInfo("MotionOps: Moving device " + deviceName + " to named position " + positionName);

  // Store start position if tracking enabled
  PositionStruct startPos;
  bool hasStartPos = false;
  if (m_resultsManager && !opId.empty()) {
    if (m_motionLayer.GetCurrentPosition(deviceName, startPos)) {
      StorePositionResult(opId, "start", startPos);
      hasStartPos = true;
    }
  }

  // Check if the device is connected
  if (!IsDeviceConnected(deviceName)) {
    m_logger->LogError("MotionOps: Device not connected: " + deviceName);

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->EndOperation(opId, "failed", "Device not connected");
    }
    return false;
  }

  // Get the named position from the motion layer configuration
  auto posOpt = m_motionLayer.GetConfigManager().GetNamedPosition(deviceName, positionName);
  if (!posOpt.has_value()) {
    m_logger->LogError("MotionOps: Position " + positionName + " not found for device " + deviceName);

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->EndOperation(opId, "failed", "Position not found");
    }
    return false;
  }

  const auto& targetPosition = posOpt.value().get();

  // Store target position info
  if (m_resultsManager && !opId.empty()) {
    StorePositionResult(opId, "target", targetPosition);
  }

  // Log detailed position information
  std::stringstream positionLog;
  positionLog << "MotionOps: Moving device " << deviceName
    << " to position " << positionName
    << " - Coordinates: "
    << "X:" << targetPosition.x << ", "
    << "Y:" << targetPosition.y << ", "
    << "Z:" << targetPosition.z;

  // Include rotation values if any are non-zero
  if (targetPosition.u != 0.0 || targetPosition.v != 0.0 || targetPosition.w != 0.0) {
    positionLog << ", U:" << targetPosition.u << ", "
      << "V:" << targetPosition.v << ", "
      << "W:" << targetPosition.w;
  }

  m_logger->LogInfo(positionLog.str());

  // Use the motion layer to perform the movement
  bool success = false;
  if (blocking) {
    success = m_motionLayer.MoveToPosition(deviceName, targetPosition, true);
  }
  else {
    success = m_motionLayer.MoveToPosition(deviceName, targetPosition, false);
  }

  // Store final results
  if (m_resultsManager && !opId.empty()) {
    if (success) {
      // Get final position and calculate distance if we had a start position
      PositionStruct finalPos;
      if (m_motionLayer.GetCurrentPosition(deviceName, finalPos)) {
        StorePositionResult(opId, "final", finalPos);

        if (hasStartPos) {
          double distance = GetDistanceBetweenPositions(startPos, finalPos);
          m_resultsManager->StoreResult(opId, "distance_moved", std::to_string(distance));
        }
      }
      m_resultsManager->EndOperation(opId, "success");
    }
    else {
      m_resultsManager->EndOperation(opId, "failed", "Move operation failed");
    }
  }

  if (success) {
    m_logger->LogInfo("MotionOps: Successfully moved device " + deviceName + " to position " + positionName);
  }
  else {
    m_logger->LogError("MotionOps: Failed to move device " + deviceName + " to position " + positionName);
  }

  return success;
}

bool MotionOps::MoveRelative(const std::string& deviceName,
  const std::string& axis,
  double distance,
  bool blocking,
  const std::string& callerContext) {

  // Start operation tracking
  std::string opId;
  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"axis", axis},
        {"distance", std::to_string(distance)},
        {"blocking", blocking ? "true" : "false"}
    };
    opId = m_resultsManager->StartOperation("MoveRelative", deviceName, callerContext, "", parameters);
  }

  m_logger->LogInfo("MotionOps: Moving device " + deviceName +
    " relative on axis " + axis + " by " + std::to_string(distance));

  // Store start position if tracking enabled
  PositionStruct startPos;
  bool hasStartPos = false;
  if (m_resultsManager && !opId.empty()) {
    if (m_motionLayer.GetCurrentPosition(deviceName, startPos)) {
      StorePositionResult(opId, "start", startPos);
      hasStartPos = true;
    }
  }

  // Check if the device is connected
  if (!IsDeviceConnected(deviceName)) {
    m_logger->LogError("MotionOps: Device not connected: " + deviceName);

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->EndOperation(opId, "failed", "Device not connected");
    }
    return false;
  }

  // Perform the relative move using motion layer
  bool success = m_motionLayer.MoveRelative(deviceName, axis, distance, blocking);

  // Store final results
  if (m_resultsManager && !opId.empty()) {
    if (success) {
      // Get final position and calculate actual distance moved
      PositionStruct finalPos;
      if (m_motionLayer.GetCurrentPosition(deviceName, finalPos)) {
        StorePositionResult(opId, "final", finalPos);

        if (hasStartPos) {
          double actualDistance = GetDistanceBetweenPositions(startPos, finalPos);
          m_resultsManager->StoreResult(opId, "actual_distance_moved", std::to_string(actualDistance));
          m_resultsManager->StoreResult(opId, "command_distance", std::to_string(distance));
        }
      }
      m_resultsManager->EndOperation(opId, "success");
    }
    else {
      m_resultsManager->EndOperation(opId, "failed", "Relative move failed");
    }
  }

  if (success) {
    m_logger->LogInfo("MotionOps: Successfully initiated relative move for device " +
      deviceName + " on axis " + axis);
  }
  else {
    m_logger->LogError("MotionOps: Failed to move device " + deviceName +
      " relative on axis " + axis);
  }

  return success;
}

bool MotionOps::SetDeviceSpeed(const std::string& deviceName, double velocity,
  const std::string& callerContext) {

  // Start operation tracking
  std::string opId;
  auto startTime = std::chrono::steady_clock::now();

  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"device_name", deviceName},
        {"velocity", std::to_string(velocity)}
    };

    // Extract sequence name from caller context if possible
    std::string sequenceName = "";
    if (callerContext.find("Initialization") != std::string::npos) {
      sequenceName = "Initialization";
    }
    else if (callerContext.find("ProcessStep") != std::string::npos) {
      sequenceName = "Process";
    }

    opId = m_resultsManager->StartOperation("SetDeviceSpeed", deviceName,
      callerContext, sequenceName, parameters);
  }

  // Get controller type for logging
  std::string controllerType = "Unknown";
  if (m_motionLayer.IsDevicePIController(deviceName)) {
    controllerType = "PI";
  }
  else {
    controllerType = "ACS";
  }

  m_logger->LogInfo("MotionOps: Setting speed for " + controllerType + " device " + deviceName +
    " to " + std::to_string(velocity) + " mm/s" +
    (callerContext.empty() ? "" : " (called by: " + callerContext + ")") +
    (opId.empty() ? "" : " [" + opId + "]"));

  // Execute speed setting through motion layer (includes validation)
  bool success = m_motionLayer.SetDeviceVelocity(deviceName, velocity);

  // Store results and end tracking
  auto endTime = std::chrono::steady_clock::now();
  auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

  if (m_resultsManager && !opId.empty()) {
    m_resultsManager->StoreResult(opId, "controller_type", controllerType);
    m_resultsManager->StoreResult(opId, "target_velocity", std::to_string(velocity));
    m_resultsManager->StoreResult(opId, "velocity_units", "mm/s");
    m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));

    if (success) {
      m_resultsManager->EndOperation(opId, "success");
    }
    else {
      m_resultsManager->EndOperation(opId, "failed", "Failed to set device speed - check velocity limits");
    }
  }

  if (success) {
    m_logger->LogInfo("MotionOps: Successfully set speed for " + controllerType + " device " + deviceName);
  }
  else {
    m_logger->LogError("MotionOps: Failed to set speed for " + controllerType + " device " + deviceName +
      " - velocity may be outside limits (PI: 0.1-20 mm/s, ACS: 0.1-80 mm/s)");
  }

  return success;
}

bool MotionOps::GetDeviceSpeed(const std::string& deviceName, double& speed,
  const std::string& callerContext) {
  std::string opId;
  auto startTime = std::chrono::steady_clock::now();

  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"device_name", deviceName}
    };
    opId = m_resultsManager->StartOperation("GetDeviceSpeed", deviceName,
      callerContext, "", parameters);
  }

  // Get speed through motion layer
  bool success = m_motionLayer.GetDeviceVelocity(deviceName, speed);

  auto endTime = std::chrono::steady_clock::now();
  auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

  if (m_resultsManager && !opId.empty()) {
    if (success) {
      m_resultsManager->StoreResult(opId, "current_speed", std::to_string(speed));
    }
    m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
    m_resultsManager->EndOperation(opId, success ? "success" : "failed");
  }

  return success;
}

bool MotionOps::WaitForDeviceMotionCompletion(const std::string& deviceName, int timeoutMs) {
  m_logger->LogInfo("MotionOps: Waiting for device " + deviceName + " motion to complete");

  auto startTime = std::chrono::steady_clock::now();
  auto endTime = startTime + std::chrono::milliseconds(timeoutMs);

  // First, wait a small amount of time to ensure motion has actually started
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Keep track of position stability
  bool wasMoving = false;
  int stableCount = 0;

  while (true) {
    bool isMoving = IsDeviceMoving(deviceName);

    if (isMoving) {
      wasMoving = true;
      stableCount = 0; // Reset stability counter
    }
    else if (wasMoving) {
      // Device has stopped moving, increment stability counter
      stableCount++;

      // If stable for 5 consecutive checks (about 250ms with 50ms sleep)
      if (stableCount >= 5) {
        m_logger->LogInfo("MotionOps: Motion completed for device " + deviceName);
        return true;
      }
    }
    else {
      // Never detected any movement
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();

      // If we've waited reasonably long and never saw movement, assume it was quick or unnecessary
      if (elapsed > 1000) {
        m_logger->LogInfo("MotionOps: No motion detected for device " + deviceName);
        return true;
      }
    }

    // Check for timeout
    if (std::chrono::steady_clock::now() > endTime) {
      m_logger->LogError("MotionOps: Timeout waiting for motion completion of device " + deviceName);
      return false;
    }

    // Sleep to avoid CPU spinning
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}