// machine_operations.cpp
#include "machine_operations.h"
#include "include/cld101x_operations.h"  // Include it here, not in the header
#include <sstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include "include/SMU/keithley2400_operations.h" // Include the SMU operations header
#include "external/sqlite/sqlite3.h"
// Add these includes at the top:
#include "include/data/DatabaseManager.h"
#include "include/data/OperationResultsManager.h"




// Updated constructor

MachineOperations::MachineOperations(
  MotionControlLayer& motionLayer,
  PIControllerManager& piControllerManager,
  EziIOManager& ioManager,
  PneumaticManager& pneumaticManager,
  CLD101xOperations* laserOps,
  PylonCameraTest* cameraTest,
  Keithley2400Operations* smuOps
) : m_motionLayer(motionLayer),
m_piControllerManager(piControllerManager),
m_ioManager(ioManager),
m_pneumaticManager(pneumaticManager),
m_laserOps(laserOps),
m_smuOps(smuOps),
m_cameraTest(cameraTest),
m_autoExposureEnabled(true)
{
  m_logger = Logger::GetInstance();

  // Initialize database and results managers
  try {
    m_dbManager = std::make_shared<DatabaseManager>();
    if (!m_dbManager->Initialize()) {
      m_logger->LogError("MachineOperations: Failed to initialize database: " + m_dbManager->GetLastError());
      m_dbManager.reset(); // Clear invalid manager
    }

    if (m_dbManager) {
      m_resultsManager = std::make_shared<OperationResultsManager>(m_dbManager);
      m_logger->LogInfo("MachineOperations: Initialized with result tracking");
    }
    else {
      m_logger->LogWarning("MachineOperations: Operating without result tracking due to database error");
    }

  }
  catch (const std::exception& e) {
    m_logger->LogError("MachineOperations: Exception initializing result managers: " + std::string(e.what()));
    m_dbManager.reset();
    m_resultsManager.reset();
  }

  // Initialize camera exposure manager
  if (m_cameraTest) {
    m_cameraExposureManager = std::make_unique<CameraExposureManager>("camera_exposure_config.json");
    m_logger->LogInfo("MachineOperations: Camera exposure manager initialized");
  }

  m_logger->LogInfo("MachineOperations: Initialized" +
    std::string(m_smuOps ? " with SMU support" : ""));
}


MachineOperations::~MachineOperations() {
  m_logger->LogInfo("MachineOperations: Shutting down");
}


// Helper method to store position data
void MachineOperations::StorePositionResult(const std::string& operationId,
  const std::string& prefix,
  const PositionStruct& position) {
  if (!m_resultsManager) return;

  m_resultsManager->StoreResult(operationId, prefix + "_x", std::to_string(position.x));
  m_resultsManager->StoreResult(operationId, prefix + "_y", std::to_string(position.y));
  m_resultsManager->StoreResult(operationId, prefix + "_z", std::to_string(position.z));

  // Include rotation if non-zero
  if (position.u != 0.0) m_resultsManager->StoreResult(operationId, prefix + "_u", std::to_string(position.u));
  if (position.v != 0.0) m_resultsManager->StoreResult(operationId, prefix + "_v", std::to_string(position.v));
  if (position.w != 0.0) m_resultsManager->StoreResult(operationId, prefix + "_w", std::to_string(position.w));
}

// Public query methods
std::vector<OperationResult> MachineOperations::GetRecentOperations(int limit) {
  if (!m_resultsManager) return {};
  return m_resultsManager->GetOperationHistory(limit);
}

std::map<std::string, std::string> MachineOperations::GetLastOperationResults(const std::string& methodName) {
  if (!m_resultsManager) return {};
  return m_resultsManager->GetLatestResults(methodName);
}

double MachineOperations::GetAverageOperationTime(const std::string& methodName) {
  if (!m_resultsManager) return 0.0;
  return m_resultsManager->GetAverageElapsedTime(methodName);
}

double MachineOperations::GetOperationSuccessRate(const std::string& methodName) {
  if (!m_resultsManager) return 0.0;
  return m_resultsManager->GetSuccessRate(methodName);
}

std::vector<OperationResult> MachineOperations::GetOperationsBySequence(const std::string& sequenceName, int limit) {
  if (!m_resultsManager) return {};
  return m_resultsManager->GetOperationsBySequence(sequenceName, limit);
}

double MachineOperations::GetSequenceSuccessRate(const std::string& sequenceName) {
  if (!m_resultsManager) return 0.0;
  return m_resultsManager->GetSequenceSuccessRate(sequenceName);
}




// machine_operations.cpp - REPLACE your MoveDeviceToNode method with this:

bool MachineOperations::MoveDeviceToNode(const std::string& deviceName,
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

  m_logger->LogInfo("MachineOperations: Moving device " + deviceName +
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

  // RELOAD EXPOSURE CONFIG EVERY TIME TO GUARANTEE FRESH VALUES
  if (m_cameraExposureManager) {
    m_logger->LogInfo("MachineOperations: Reloading camera exposure configuration to ensure fresh values");
    if (m_cameraExposureManager->LoadConfiguration("camera_exposure_config.json")) {
      m_logger->LogInfo("MachineOperations: Camera exposure configuration reloaded successfully");
    }
    else {
      m_logger->LogWarning("MachineOperations: Failed to reload camera exposure configuration, using existing values");
    }
  }

  // Get the current node for the device
  std::string currentNodeId;
  if (!m_motionLayer.GetDeviceCurrentNode(graphName, deviceName, currentNodeId)) {
    m_logger->LogError("MachineOperations: Failed to get current node for device " + deviceName);

    // If we can't determine the current node, try to get positions and compare
    PositionStruct currentPos;
    if (m_motionLayer.GetCurrentPosition(deviceName, currentPos)) {
      m_logger->LogInfo("MachineOperations: Current position: X=" + std::to_string(currentPos.x) +
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

              m_logger->LogInfo("MachineOperations: Target node position: X=" + std::to_string(targetPos.x) +
                " Y=" + std::to_string(targetPos.y) +
                " Z=" + std::to_string(targetPos.z));

              double distance = GetDistanceBetweenPositions(currentPos, targetPos, false);
              m_logger->LogInfo("MachineOperations: Distance to target: " + std::to_string(distance) + " mm");

              if (distance < 0.1) { // Within 0.1mm, consider we're already there
                m_logger->LogInfo("MachineOperations: Device appears to be at target node based on position proximity");

                // STILL APPLY CAMERA EXPOSURE EVEN IF ALREADY AT NODE
                if (deviceName == "gantry-main" && m_autoExposureEnabled) {
                  m_logger->LogInfo("MachineOperations: Device appears at " + targetNodeId +
                    ", applying camera exposure with fresh config");
                  ApplyCameraExposureForNode(targetNodeId);
                }

                // Store success result
                if (m_resultsManager && !opId.empty()) {
                  m_resultsManager->StoreResult(opId, "distance_moved", std::to_string(distance));
                  m_resultsManager->StoreResult(opId, "already_at_target", "true");
                  StorePositionResult(opId, "final", currentPos);
                  m_resultsManager->EndOperation(opId, "success");  //  FIXED: EndOperation called
                }

                return true;
              }
            }
          }
        }
      }
      catch (const std::exception& e) {
        m_logger->LogError("MachineOperations: Exception while checking node position: " + std::string(e.what()));
      }
    }

    // Store failure result
    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->EndOperation(opId, "failed", "Failed to get current node for device");  //  FIXED: EndOperation called
    }
    return false;
  }

  // Already at the target node
  if (currentNodeId == targetNodeId) {
    m_logger->LogInfo("MachineOperations: Device " + deviceName + " is already at node " + targetNodeId);

    // STILL APPLY CAMERA EXPOSURE EVEN IF ALREADY AT NODE
    if (deviceName == "gantry-main" && m_autoExposureEnabled) {
      m_logger->LogInfo("MachineOperations: Device already at " + targetNodeId +
        ", but applying camera exposure with fresh config");
      ApplyCameraExposureForNode(targetNodeId);
    }

    // Store success result
    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->StoreResult(opId, "distance_moved", "0.0");
      m_resultsManager->StoreResult(opId, "already_at_target", "true");
      PositionStruct currentPos;
      if (m_motionLayer.GetCurrentPosition(deviceName, currentPos)) {
        StorePositionResult(opId, "final", currentPos);
      }
      m_resultsManager->EndOperation(opId, "success");  //  FIXED: EndOperation called
    }

    return true;
  }

  // Plan and execute path
 // bool success = MovePathFromTo(deviceName, graphName, currentNodeId, targetNodeId, blocking);

  // When calling MovePathFromTo internally, pass through the caller context:
  bool success = MovePathFromTo(deviceName, graphName, currentNodeId, targetNodeId, blocking, callerContext);



  // Apply camera exposure settings if the gantry moved successfully
  if (success && deviceName == "gantry-main" && m_autoExposureEnabled) {
    m_logger->LogInfo("MachineOperations: Gantry moved to " + targetNodeId +
      ", applying camera exposure with fresh config");
    ApplyCameraExposureForNode(targetNodeId);
  }

  // Store final results -  FIXED: This section was missing/incomplete
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
      m_resultsManager->EndOperation(opId, "success");  //  FIXED: EndOperation called
    }
    else {
      m_resultsManager->EndOperation(opId, "failed", "Path execution failed");  //  FIXED: EndOperation called
    }
  }

  return success;
}


// 3. UPDATE MovePathFromTo method with database tracking:
bool MachineOperations::MovePathFromTo(const std::string& deviceName,
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

  m_logger->LogInfo("MachineOperations: Planning path for device " + deviceName +
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
    m_logger->LogError("MachineOperations: Failed to plan path from " +
      startNodeId + " to " + endNodeId);

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->EndOperation(opId, "failed", "Path planning failed");
    }
    return false;
  }

  // Execute the path
  m_logger->LogInfo("MachineOperations: Executing path");
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
    m_logger->LogInfo("MachineOperations: Path execution " +
      std::string(blocking ? "completed" : "started"));
  }
  else {
    m_logger->LogError("MachineOperations: Path execution failed");
  }

  return success;
}


// 1. UPDATE MoveToPointName method with database tracking:
bool MachineOperations::MoveToPointName(const std::string& deviceName,
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

  m_logger->LogInfo("MachineOperations: Moving device " + deviceName + " to named position " + positionName);

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
    m_logger->LogError("MachineOperations: Device not connected: " + deviceName);

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->EndOperation(opId, "failed", "Device not connected");
    }
    return false;
  }

  // Get the named position from the motion layer configuration
  auto posOpt = m_motionLayer.GetConfigManager().GetNamedPosition(deviceName, positionName);
  if (!posOpt.has_value()) {
    m_logger->LogError("MachineOperations: Position " + positionName + " not found for device " + deviceName);

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
  positionLog << "MachineOperations: Moving device " << deviceName
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
    m_logger->LogInfo("MachineOperations: Successfully moved device " + deviceName + " to position " + positionName);
  }
  else {
    m_logger->LogError("MachineOperations: Failed to move device " + deviceName + " to position " + positionName);
  }

  return success;
}


// 2. UPDATE the SetOutput method (replace the existing one):
bool MachineOperations::SetOutput(const std::string& deviceName, int outputPin, bool state,
  const std::string& callerContext) {  // ADD this parameter

  // Start operation tracking with caller context
  std::string opId;
  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"output_pin", std::to_string(outputPin)},
        {"target_state", state ? "true" : "false"}
    };

    // Extract sequence name from caller context if possible
    std::string sequenceName = "";
    if (callerContext.find("Initialization") != std::string::npos) {
      sequenceName = "Initialization";
    }
    else if (callerContext.find("ProcessStep") != std::string::npos) {
      sequenceName = "Process";
    }

    opId = m_resultsManager->StartOperation("SetOutput", deviceName,
      callerContext, sequenceName, parameters);
  }

  m_logger->LogInfo("MachineOperations: Setting output pin " + std::to_string(outputPin) +
    " on device " + deviceName + " to " + (state ? "ON" : "OFF") +
    (callerContext.empty() ? "" : " (called by: " + callerContext + ")") +
    (opId.empty() ? "" : " [" + opId + "]"));

  bool success = false;
  std::string errorMessage = "";

  try {
    // Get device by name
    EziIODevice* device = m_ioManager.getDeviceByName(deviceName);
    if (!device) {
      errorMessage = "Device not found";
      if (m_resultsManager && !opId.empty()) {
        m_resultsManager->EndOperation(opId, "failed", errorMessage);
      }
      m_logger->LogError("MachineOperations: " + errorMessage);
      return false;
    }

    // Execute the operation
    success = device->setOutput(outputPin, state);

    if (success) {
      if (m_resultsManager && !opId.empty()) {
        m_resultsManager->StoreResult(opId, "final_state", state ? "true" : "false");
        m_resultsManager->EndOperation(opId, "success");
      }
    }
    else {
      errorMessage = "Failed to set output";
      if (m_resultsManager && !opId.empty()) {
        m_resultsManager->EndOperation(opId, "failed", errorMessage);
      }
      m_logger->LogError("MachineOperations: " + errorMessage);
    }

  }
  catch (const std::exception& e) {
    errorMessage = "Exception: " + std::string(e.what());
    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->EndOperation(opId, "failed", errorMessage);
    }
    m_logger->LogError("MachineOperations: " + errorMessage);
    success = false;
  }

  return success;
}

// Set an output state by device ID
bool MachineOperations::SetOutput(int deviceId, int outputPin, bool state) {
  m_logger->LogInfo("MachineOperations: Setting output pin " + std::to_string(outputPin) +
    " on device ID " + std::to_string(deviceId) + " to " + (state ? "ON" : "OFF"));

  return m_ioManager.setOutput(deviceId, outputPin, state);
}

// Read input state by device name
// UPDATED ReadInput method with tracking
bool MachineOperations::ReadInput(const std::string& deviceName, int inputPin, bool& state,
  const std::string& callerContext) {

  // 1. Start operation tracking
  std::string opId;
  auto startTime = std::chrono::steady_clock::now();

  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"device_name", deviceName},
        {"input_pin", std::to_string(inputPin)}
    };
    opId = m_resultsManager->StartOperation("ReadInput", deviceName, callerContext, "", parameters);
  }

  m_logger->LogInfo("MachineOperations: Reading input pin " + std::to_string(inputPin) +
    " on device " + deviceName +
    (callerContext.empty() ? "" : " (called by: " + callerContext + ")"));

  // 2. Get device by name
  EziIODevice* device = m_ioManager.getDeviceByName(deviceName);
  if (!device) {
    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
      m_resultsManager->EndOperation(opId, "failed", "Device not found: " + deviceName);
    }

    m_logger->LogError("MachineOperations: Device not found: " + deviceName);
    return false;
  }

  // 3. Read inputs
  uint32_t inputs = 0, latch = 0;
  if (!device->readInputs(inputs, latch)) {
    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
      m_resultsManager->EndOperation(opId, "failed", "Failed to read inputs from device " + deviceName);
    }

    m_logger->LogError("MachineOperations: Failed to read inputs from device " + deviceName);
    return false;
  }

  // 4. Check if the pin is within range
  if (inputPin >= device->getInputCount()) {
    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->StoreResult(opId, "input_count", std::to_string(device->getInputCount()));
      m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
      m_resultsManager->EndOperation(opId, "failed", "Invalid input pin " + std::to_string(inputPin));
    }

    m_logger->LogError("MachineOperations: Invalid input pin " + std::to_string(inputPin) +
      " for device " + deviceName);
    return false;
  }

  // 5. Check the pin state
  state = ConvertPinStateToBoolean(inputs, inputPin);

  // 6. Store results and end tracking
  auto endTime = std::chrono::steady_clock::now();
  auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

  if (m_resultsManager && !opId.empty()) {
    m_resultsManager->StoreResult(opId, "pin_state", state ? "HIGH" : "LOW");
    m_resultsManager->StoreResult(opId, "raw_inputs", "0x" + std::to_string(inputs));
    m_resultsManager->StoreResult(opId, "latch_value", "0x" + std::to_string(latch));
    m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
    m_resultsManager->EndOperation(opId, "success");
  }

  return true;
}

// Read input state by device ID
bool MachineOperations::ReadInput(int deviceId, int inputPin, bool& state) {
  m_logger->LogInfo("MachineOperations: Reading input pin " + std::to_string(inputPin) +
    " on device ID " + std::to_string(deviceId));

  // Read inputs
  uint32_t inputs = 0, latch = 0;
  if (!m_ioManager.readInputs(deviceId, inputs, latch)) {
    m_logger->LogError("MachineOperations: Failed to read inputs from device ID " +
      std::to_string(deviceId));
    return false;
  }

  // Check the pin state
  state = ConvertPinStateToBoolean(inputs, inputPin);
  return true;
}




// Clear latch by device name and pin

// UPDATED ClearLatch method with tracking
bool MachineOperations::ClearLatch(const std::string& deviceName, int inputPin,
  const std::string& callerContext) {

  // 1. Start operation tracking
  std::string opId;
  auto startTime = std::chrono::steady_clock::now();

  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"device_name", deviceName},
        {"input_pin", std::to_string(inputPin)}
    };
    opId = m_resultsManager->StartOperation("ClearLatch", deviceName, callerContext, "", parameters);
  }

  m_logger->LogInfo("MachineOperations: Clearing latch for input pin " +
    std::to_string(inputPin) + " on device " + deviceName +
    (callerContext.empty() ? "" : " (called by: " + callerContext + ")"));

  // 2. Get device by name
  EziIODevice* device = m_ioManager.getDeviceByName(deviceName);
  if (!device) {
    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
      m_resultsManager->EndOperation(opId, "failed", "Device not found: " + deviceName);
    }

    m_logger->LogError("MachineOperations: Device not found: " + deviceName);
    return false;
  }

  // 3. Create mask for this pin and clear latch
  uint32_t latchMask = 1 << inputPin;
  bool success = device->clearLatch(latchMask);

  // 4. Store results and end tracking
  auto endTime = std::chrono::steady_clock::now();
  auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

  if (m_resultsManager && !opId.empty()) {
    m_resultsManager->StoreResult(opId, "latch_mask", "0x" + std::to_string(latchMask));
    m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));

    if (success) {
      m_resultsManager->EndOperation(opId, "success");
    }
    else {
      m_resultsManager->EndOperation(opId, "failed", "Failed to clear latch for pin " + std::to_string(inputPin));
    }
  }

  return success;
}



// Clear latch by device ID and mask
bool MachineOperations::ClearLatch(int deviceId, uint32_t latchMask) {
  m_logger->LogInfo("MachineOperations: Clearing latch with mask 0x" +
    std::to_string(latchMask) + " on device ID " + std::to_string(deviceId));

  // Get device by ID
  EziIODevice* device = m_ioManager.getDevice(deviceId);
  if (!device) {
    m_logger->LogError("MachineOperations: Device not found with ID: " +
      std::to_string(deviceId));
    return false;
  }

  return device->clearLatch(latchMask);
}



bool MachineOperations::ClearOutput(const std::string& deviceName, int outputPin,
  const std::string& callerContext) {

  // Start operation tracking with caller context
  std::string opId;
  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"output_pin", std::to_string(outputPin)},
        {"action", "clear"}
    };

    // Extract sequence name from caller context if possible
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

    opId = m_resultsManager->StartOperation("ClearOutput", deviceName,
      callerContext, sequenceName, parameters);
  }

  m_logger->LogInfo("MachineOperations: Clearing output pin " + std::to_string(outputPin) +
    " on device " + deviceName +
    (callerContext.empty() ? "" : " (called by: " + callerContext + ")") +
    (opId.empty() ? "" : " [" + opId + "]"));

  bool success = false;
  std::string errorMessage = "";

  try {
    // Get device by name
    EziIODevice* device = m_ioManager.getDeviceByName(deviceName);
    if (!device) {
      errorMessage = "Device not found";
      if (m_resultsManager && !opId.empty()) {
        m_resultsManager->EndOperation(opId, "failed", errorMessage);
      }
      m_logger->LogError("MachineOperations: " + errorMessage);
      return false;
    }

    // Store previous state if possible (optional enhancement)
    bool previousState = false;
    // You could read current state here if your hardware supports it
    // device->getOutput(outputPin, previousState); // If this method exists

    // Execute the clear operation (set to false)
    success = device->setOutput(outputPin, false);

    if (success) {
      if (m_resultsManager && !opId.empty()) {
        m_resultsManager->StoreResult(opId, "previous_state", "unknown"); // or actual previous state
        m_resultsManager->StoreResult(opId, "final_state", "false");
        m_resultsManager->StoreResult(opId, "action_performed", "clear");
        m_resultsManager->EndOperation(opId, "success");
      }
      m_logger->LogInfo("MachineOperations: Successfully cleared output pin " +
        std::to_string(outputPin) + " on device " + deviceName);
    }
    else {
      errorMessage = "Failed to clear output";
      if (m_resultsManager && !opId.empty()) {
        m_resultsManager->EndOperation(opId, "failed", errorMessage);
      }
      m_logger->LogError("MachineOperations: " + errorMessage);
    }

  }
  catch (const std::exception& e) {
    errorMessage = "Exception: " + std::string(e.what());
    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->EndOperation(opId, "failed", errorMessage);
    }
    m_logger->LogError("MachineOperations: " + errorMessage);
    success = false;
  }

  return success;
}


// Extend a pneumatic slide
bool MachineOperations::ExtendSlide(const std::string& slideName, bool waitForCompletion,
  int timeoutMs, const std::string& callerContext) {

  // 1. Start operation tracking
  std::string opId;
  auto startTime = std::chrono::steady_clock::now();

  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"slideName", slideName},
        {"waitForCompletion", waitForCompletion ? "true" : "false"},
        {"timeoutMs", std::to_string(timeoutMs)}
    };
    opId = m_resultsManager->StartOperation("ExtendSlide", slideName, callerContext, "", parameters);
  }

  m_logger->LogInfo("MachineOperations: Extending slide " + slideName);

  // 2. Store initial state
  SlideState initialState = m_pneumaticManager.getSlideState(slideName);

  // 3. Execute extend operation
  bool success = m_pneumaticManager.extendSlide(slideName);
  if (!success) {
    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->StoreResult(opId, "initial_state", std::to_string(static_cast<int>(initialState)));
      m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
      m_resultsManager->EndOperation(opId, "failed", "Failed to extend slide " + slideName);
    }

    m_logger->LogError("MachineOperations: Failed to extend slide " + slideName);
    return false;
  }

  // 4. Wait for completion if requested
  bool finalSuccess = true;
  if (waitForCompletion) {
    finalSuccess = WaitForSlideState(slideName, SlideState::EXTENDED, timeoutMs, callerContext);
  }

  // 5. Store results and end tracking
  auto endTime = std::chrono::steady_clock::now();
  auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

  if (m_resultsManager && !opId.empty()) {
    SlideState finalState = m_pneumaticManager.getSlideState(slideName);

    m_resultsManager->StoreResult(opId, "initial_state", std::to_string(static_cast<int>(initialState)));
    m_resultsManager->StoreResult(opId, "final_state", std::to_string(static_cast<int>(finalState)));
    m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
    m_resultsManager->StoreResult(opId, "wait_for_completion", waitForCompletion ? "true" : "false");

    if (finalSuccess) {
      m_resultsManager->EndOperation(opId, "success");
    }
    else {
      m_resultsManager->EndOperation(opId, "failed", "Slide extend operation failed or timed out");
    }
  }

  return finalSuccess;
}

// Retract a pneumatic slide
bool MachineOperations::RetractSlide(const std::string& slideName, bool waitForCompletion,
  int timeoutMs, const std::string& callerContext) {

  // 1. Start operation tracking
  std::string opId;
  auto startTime = std::chrono::steady_clock::now();

  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"slideName", slideName},
        {"waitForCompletion", waitForCompletion ? "true" : "false"},
        {"timeoutMs", std::to_string(timeoutMs)}
    };
    opId = m_resultsManager->StartOperation("RetractSlide", slideName, callerContext, "", parameters);
  }

  m_logger->LogInfo("MachineOperations: Retracting slide " + slideName);

  // 2. Store initial state
  SlideState initialState = m_pneumaticManager.getSlideState(slideName);

  // 3. Execute retract operation
  bool success = m_pneumaticManager.retractSlide(slideName);
  if (!success) {
    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->StoreResult(opId, "initial_state", std::to_string(static_cast<int>(initialState)));
      m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
      m_resultsManager->EndOperation(opId, "failed", "Failed to retract slide " + slideName);
    }

    m_logger->LogError("MachineOperations: Failed to retract slide " + slideName);
    return false;
  }

  // 4. Wait for completion if requested
  bool finalSuccess = true;
  if (waitForCompletion) {
    finalSuccess = WaitForSlideState(slideName, SlideState::RETRACTED, timeoutMs, callerContext);
  }

  // 5. Store results and end tracking
  auto endTime = std::chrono::steady_clock::now();
  auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

  if (m_resultsManager && !opId.empty()) {
    SlideState finalState = m_pneumaticManager.getSlideState(slideName);

    m_resultsManager->StoreResult(opId, "initial_state", std::to_string(static_cast<int>(initialState)));
    m_resultsManager->StoreResult(opId, "final_state", std::to_string(static_cast<int>(finalState)));
    m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
    m_resultsManager->StoreResult(opId, "wait_for_completion", waitForCompletion ? "true" : "false");

    if (finalSuccess) {
      m_resultsManager->EndOperation(opId, "success");
    }
    else {
      m_resultsManager->EndOperation(opId, "failed", "Slide retract operation failed or timed out");
    }
  }

  return finalSuccess;
}
// Get the current state of a pneumatic slide
SlideState MachineOperations::GetSlideState(const std::string& slideName) {
  return m_pneumaticManager.getSlideState(slideName);
}

// Wait for a pneumatic slide to reach a specific state
bool MachineOperations::WaitForSlideState(const std::string& slideName, SlideState targetState,
  int timeoutMs, const std::string& callerContext) {

  // 1. Start operation tracking
  std::string opId;
  auto startTime = std::chrono::steady_clock::now();

  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"slideName", slideName},
        {"targetState", std::to_string(static_cast<int>(targetState))},
        {"timeoutMs", std::to_string(timeoutMs)}
    };
    opId = m_resultsManager->StartOperation("WaitForSlideState", slideName, callerContext, "", parameters);
  }

  m_logger->LogInfo("MachineOperations: Waiting for slide " + slideName +
    " to reach state: " + std::to_string(static_cast<int>(targetState)));

  // 2. Store initial state
  SlideState initialState = m_pneumaticManager.getSlideState(slideName);

  auto endTime = startTime + std::chrono::milliseconds(timeoutMs);
  bool success = false;
  SlideState finalState = initialState;

  // 3. Wait loop
  while (std::chrono::steady_clock::now() < endTime) {
    SlideState currentState = m_pneumaticManager.getSlideState(slideName);
    finalState = currentState;

    if (currentState == targetState) {
      success = true;
      m_logger->LogInfo("MachineOperations: Slide " + slideName + " reached target state");
      break;
    }

    // Check for error state
    if (currentState == SlideState::P_ERROR) {
      m_logger->LogError("MachineOperations: Slide " + slideName + " is in ERROR state");
      break;
    }

    // Sleep to avoid CPU spinning
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  // 4. Handle timeout
  if (!success && finalState != SlideState::P_ERROR) {
    m_logger->LogError("MachineOperations: Timeout waiting for slide " + slideName +
      " to reach target state");
  }

  // 5. Store results and end tracking
  auto actualEndTime = std::chrono::steady_clock::now();
  auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(actualEndTime - startTime).count();

  if (m_resultsManager && !opId.empty()) {
    m_resultsManager->StoreResult(opId, "initial_state", std::to_string(static_cast<int>(initialState)));
    m_resultsManager->StoreResult(opId, "final_state", std::to_string(static_cast<int>(finalState)));
    m_resultsManager->StoreResult(opId, "target_state", std::to_string(static_cast<int>(targetState)));
    m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));

    if (success) {
      m_resultsManager->EndOperation(opId, "success");
    }
    else if (finalState == SlideState::P_ERROR) {
      m_resultsManager->EndOperation(opId, "failed", "Slide entered ERROR state");
    }
    else {
      m_resultsManager->EndOperation(opId, "failed", "Timeout waiting for target state");
    }
  }

  return success;
}



// Wait for a specified time
void MachineOperations::Wait(int milliseconds) {
  if (milliseconds <= 0) return;

  m_logger->LogInfo("MachineOperations: Waiting for " + std::to_string(milliseconds) + " ms");
  std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

// Read a value from the global data store
float MachineOperations::ReadDataValue(const std::string& dataId, float defaultValue) {
  float value = GlobalDataStore::GetInstance()->GetValue(dataId, defaultValue);
  m_logger->LogInfo("MachineOperations: Read value from " + dataId + ": " + std::to_string(value));
  return value;
}

// Check if a data value exists
bool MachineOperations::HasDataValue(const std::string& dataId) {
  bool hasValue = GlobalDataStore::GetInstance()->HasValue(dataId);
  m_logger->LogInfo("MachineOperations: Checked if data exists for " + dataId + ": " +
    (hasValue ? "yes" : "no"));
  return hasValue;
}


// CORRECTED PerformScan method with tracking
bool MachineOperations::PerformScan(const std::string& deviceName, const std::string& dataChannel,
  const std::vector<double>& stepSizes, int settlingTimeMs,
  const std::vector<std::string>& axesToScan, const std::string& callerContext) {

  // 1. Start operation tracking
  std::string opId;
  auto startTime = std::chrono::steady_clock::now();

  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"device_name", deviceName},
        {"data_channel", dataChannel},
        {"settling_time_ms", std::to_string(settlingTimeMs)},
        {"axes_count", std::to_string(axesToScan.size())},
        {"steps_count", std::to_string(stepSizes.size())}
    };

    // Add step sizes and axes to parameters
    for (size_t i = 0; i < stepSizes.size() && i < 3; ++i) {
      parameters["step_size_" + std::to_string(i)] = std::to_string(stepSizes[i]);
    }
    for (size_t i = 0; i < axesToScan.size() && i < 3; ++i) {
      parameters["axis_" + std::to_string(i)] = axesToScan[i];
    }

    opId = m_resultsManager->StartOperation("PerformScan", deviceName, callerContext, "", parameters);
  }

  m_logger->LogInfo("MachineOperations: Starting scan for device " + deviceName +
    " using data channel " + dataChannel +
    (callerContext.empty() ? "" : " (called by: " + callerContext + ")") +
    (opId.empty() ? "" : " [" + opId + "]"));  // ADD: operation ID to log

  // 2. Get the PI controller for the device
  PIController* controller = m_piControllerManager.GetController(deviceName);
  if (!controller || !controller->IsConnected()) {
    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
      m_resultsManager->EndOperation(opId, "failed", "No connected PI controller for device " + deviceName);
    }

    m_logger->LogError("MachineOperations: No connected PI controller for device " + deviceName);
    return false;
  }

  // 3. Setup scanning parameters
  ScanningParameters params = ScanningParameters::CreateDefault();
  params.axesToScan = axesToScan;
  params.stepSizes = stepSizes;
  params.motionSettleTimeMs = settlingTimeMs;

  bool scanSuccess = false;
  std::string errorMessage;

  try {
    // 4. Validate parameters
    params.Validate();

    // 5. Create scanning algorithm
    GlobalDataStore* dataStore = GlobalDataStore::GetInstance();
    ScanningAlgorithm scanner(*controller, *dataStore, deviceName, dataChannel, params);

    // 6. Start the scan (blocking)
    m_logger->LogInfo("MachineOperations: Executing scan");
    bool success = scanner.StartScan();

    if (success) {
      m_logger->LogInfo("MachineOperations: Scan started for device " + deviceName);

      // 7. Wait for scan to complete
      while (scanner.IsScanningActive()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      m_logger->LogInfo("MachineOperations: Scan completed for device " + deviceName);
      scanSuccess = true;
    }
    else {
      errorMessage = "Failed to start scan for device " + deviceName;
      m_logger->LogError("MachineOperations: " + errorMessage);
      scanSuccess = false;
    }
  }
  catch (const std::exception& e) {
    errorMessage = "Exception during scan: " + std::string(e.what());
    m_logger->LogError("MachineOperations: " + errorMessage);
    scanSuccess = false;
  }

  // 8. Store results and end tracking
  auto endTime = std::chrono::steady_clock::now();
  auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

  if (m_resultsManager && !opId.empty()) {
    m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
    m_resultsManager->StoreResult(opId, "scan_type", "blocking_perform_scan");

    if (scanSuccess) {
      m_resultsManager->EndOperation(opId, "success");
    }
    else {
      m_resultsManager->EndOperation(opId, "failed", errorMessage.empty() ? "PerformScan operation failed" : errorMessage);  // IMPROVE: fallback error message
    }
  }

  return scanSuccess;
}







// Keep existing PerformScan method without callerContext (for backward compatibility)
bool MachineOperations::PerformScan(const std::string& deviceName, const std::string& dataChannel,
  const std::vector<double>& stepSizes, int settlingTimeMs,
  const std::vector<std::string>& axesToScan) {
  return PerformScan(deviceName, dataChannel, stepSizes, settlingTimeMs, axesToScan, "");
}





// Check if device is connected
bool MachineOperations::IsDeviceConnected(const std::string& deviceName) {
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

  // Check if it's an EziIO device as a fallback
  EziIODevice* eziioDevice = m_ioManager.getDeviceByName(deviceName);
  if (eziioDevice) {
    return eziioDevice->isConnected();
  }

  // Device not found in any controller manager
  m_logger->LogWarning("Device " + deviceName + " not found in any controller manager");
  return false;
}



// Check if slide is extended
bool MachineOperations::IsSlideExtended(const std::string& slideName) {
  SlideState state = m_pneumaticManager.getSlideState(slideName);
  return state == SlideState::EXTENDED;
}

// Check if slide is retracted
bool MachineOperations::IsSlideRetracted(const std::string& slideName) {
  SlideState state = m_pneumaticManager.getSlideState(slideName);
  return state == SlideState::RETRACTED;
}

// Check if slide is moving
bool MachineOperations::IsSlideMoving(const std::string& slideName) {
  SlideState state = m_pneumaticManager.getSlideState(slideName);
  return state == SlideState::MOVING;
}

// Check if slide is in error state
bool MachineOperations::IsSlideInError(const std::string& slideName) {
  SlideState state = m_pneumaticManager.getSlideState(slideName);
  return state == SlideState::P_ERROR;
}

// Get EziIO device ID from name
int MachineOperations::GetDeviceId(const std::string& deviceName) {
  EziIODevice* device = m_ioManager.getDeviceByName(deviceName);
  if (!device) {
    m_logger->LogError("MachineOperations: Device not found: " + deviceName);
    return -1;
  }
  return device->getDeviceId();
}

// Helper to convert pin state to boolean
bool MachineOperations::ConvertPinStateToBoolean(uint32_t inputs, int pin) {
  // Check if the specific pin is high
  return (inputs & (1 << pin)) != 0;
}

// Add implementations for the new methods:
bool MachineOperations::LaserOn(const std::string& laserName) {
  if (!m_laserOps) {
    m_logger->LogError("MachineOperations: No laser operations module available");
    return false;
  }
  return m_laserOps->LaserOn(laserName);
}

bool MachineOperations::LaserOff(const std::string& laserName) {
  if (!m_laserOps) {
    m_logger->LogError("MachineOperations: No laser operations module available");
    return false;
  }
  return m_laserOps->LaserOff(laserName);
}

bool MachineOperations::TECOn(const std::string& laserName) {
  if (!m_laserOps) {
    m_logger->LogError("MachineOperations: No laser operations module available");
    return false;
  }
  return m_laserOps->TECOn(laserName);
}

bool MachineOperations::TECOff(const std::string& laserName) {
  if (!m_laserOps) {
    m_logger->LogError("MachineOperations: No laser operations module available");
    return false;
  }
  return m_laserOps->TECOff(laserName);
}

bool MachineOperations::SetLaserCurrent(float current, const std::string& laserName) {
  if (!m_laserOps) {
    m_logger->LogError("MachineOperations: No laser operations module available");
    return false;
  }
  return m_laserOps->SetLaserCurrent(current, laserName);
}

bool MachineOperations::SetTECTemperature(float temperature, const std::string& laserName) {
  if (!m_laserOps) {
    m_logger->LogError("MachineOperations: No laser operations module available");
    return false;
  }
  return m_laserOps->SetTECTemperature(temperature, laserName);
}

float MachineOperations::GetLaserTemperature(const std::string& laserName) {
  if (!m_laserOps) {
    m_logger->LogError("MachineOperations: No laser operations module available");
    return 0.0f;
  }
  return m_laserOps->GetTemperature(laserName);
}

float MachineOperations::GetLaserCurrent(const std::string& laserName) {
  if (!m_laserOps) {
    m_logger->LogError("MachineOperations: No laser operations module available");
    return 0.0f;
  }
  return m_laserOps->GetLaserCurrent(laserName);
}

bool MachineOperations::WaitForLaserTemperature(float targetTemp, float tolerance,
  int timeoutMs, const std::string& laserName) {
  if (!m_laserOps) {
    m_logger->LogError("MachineOperations: No laser operations module available");
    return false;
  }
  return m_laserOps->WaitForTemperatureStabilization(targetTemp, tolerance, timeoutMs, laserName);
}




// CORRECTED StartScan method with proper tracking
bool MachineOperations::StartScan(const std::string& deviceName, const std::string& dataChannel,
  const std::vector<double>& stepSizes, int settlingTimeMs,
  const std::vector<std::string>& axesToScan, const std::string& callerContext) {

  // 1. Start operation tracking
  std::string opId;
  auto startTime = std::chrono::steady_clock::now();

  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"device_name", deviceName},
        {"data_channel", dataChannel},
        {"settling_time_ms", std::to_string(settlingTimeMs)},
        {"axes_count", std::to_string(axesToScan.size())},
        {"steps_count", std::to_string(stepSizes.size())}
    };

    // Add step sizes and axes to parameters
    for (size_t i = 0; i < stepSizes.size() && i < 3; ++i) {
      parameters["step_size_" + std::to_string(i)] = std::to_string(stepSizes[i]);
    }
    for (size_t i = 0; i < axesToScan.size() && i < 3; ++i) {
      parameters["axis_" + std::to_string(i)] = axesToScan[i];
    }

    opId = m_resultsManager->StartOperation("StartScan", deviceName, callerContext, "", parameters);
  }

  m_logger->LogInfo("MachineOperations: Starting asynchronous scan for device " + deviceName +
    " using data channel " + dataChannel +
    (callerContext.empty() ? "" : " (called by: " + callerContext + ")") +
    (opId.empty() ? "" : " [" + opId + "]"));

  // 2. Check if a scan is already active for this device
  bool needsReset = false;
  {
    std::lock_guard<std::mutex> lock(m_scanMutex);
    if (m_activeScans.find(deviceName) != m_activeScans.end()) {
      // Check if the scan is truly active or just stalled
      auto& scanner = m_activeScans[deviceName];
      if (scanner && scanner->IsScanningActive()) {
        auto endTime = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        if (m_resultsManager && !opId.empty()) {
          m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
          m_resultsManager->EndOperation(opId, "failed", "Scan already in progress for device " + deviceName);
        }

        m_logger->LogWarning("MachineOperations: Scan already in progress for device " + deviceName);
        return false;
      }
      else {
        // The scanner exists but is not active - this is a stalled state
        needsReset = true;
        m_logger->LogWarning("MachineOperations: Found stalled scanner for device " + deviceName + ", will reset");
      }
    }

    // Also check the scan info status
    auto infoIt = m_scanInfo.find(deviceName);
    if (infoIt != m_scanInfo.end() && infoIt->second.isActive.load()) {
      // The scan info shows active but we didn't find an active scanner
      // This is definitely a desynchronized state
      needsReset = true;
      m_logger->LogWarning("MachineOperations: Scan info shows active but no active scanner for " + deviceName + ", will reset");
    }
  }

  // Reset state if needed
  if (needsReset) {
    ResetScanState(deviceName);
  }

  // 3. Get the PI controller for the device
  PIController* controller = m_piControllerManager.GetController(deviceName);
  if (!controller || !controller->IsConnected()) {
    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
      m_resultsManager->EndOperation(opId, "failed", "No connected PI controller for device " + deviceName);
    }

    m_logger->LogError("MachineOperations: No connected PI controller for device " + deviceName);
    return false;
  }

  // 4. Setup scanning parameters
  ScanningParameters params = ScanningParameters::CreateDefault();
  params.axesToScan = axesToScan;
  params.stepSizes = stepSizes;
  params.motionSettleTimeMs = settlingTimeMs;

  bool scanStartSuccess = false;
  std::string errorMessage;

  try {
    // 5. Validate parameters
    params.Validate();

    // 6. Create and configure the scanning algorithm
    auto scanner = std::make_unique<ScanningAlgorithm>(
      *controller,
      *GlobalDataStore::GetInstance(), // Use global data store
      deviceName,
      dataChannel,
      params
    );

    // 7. Initialize scan info - create it in the map if it doesn't exist yet
    {
      std::lock_guard<std::mutex> lock(m_scanMutex);

      // Create the entry if it doesn't exist
      if (m_scanInfo.find(deviceName) == m_scanInfo.end()) {
        m_scanInfo.emplace(std::piecewise_construct,
          std::forward_as_tuple(deviceName),
          std::forward_as_tuple());
      }

      // Initialize the values directly
      m_scanInfo[deviceName].isActive.store(true);
      m_scanInfo[deviceName].progress.store(0.0);

      // Set the status with proper locking
      {
        std::lock_guard<std::mutex> statusLock(m_scanInfo[deviceName].statusMutex);
        m_scanInfo[deviceName].status = "Starting scan...";
      }
    }

    // 8. Set callbacks to update status
    scanner->SetProgressCallback([this, deviceName](const ScanProgressEventArgs& args) {
      this->m_scanInfo[deviceName].progress.store(args.GetProgress());
      std::lock_guard<std::mutex> lock(this->m_scanInfo[deviceName].statusMutex);
      this->m_scanInfo[deviceName].status = args.GetStatus();
    });

    scanner->SetPeakUpdateCallback([this, deviceName](double value, const PositionStruct& position, const std::string& context) {
      std::lock_guard<std::mutex> lock(this->m_scanInfo[deviceName].peakMutex);
      this->m_scanInfo[deviceName].peakValue = value;
      this->m_scanInfo[deviceName].peakPosition = position;
    });

    scanner->SetCompletionCallback([this, deviceName](const ScanCompletedEventArgs& args) {
      // Update status, but don't remove scanner here
      this->m_scanInfo[deviceName].isActive.store(false);
      this->m_scanInfo[deviceName].progress.store(1.0);
      std::lock_guard<std::mutex> lock(this->m_scanInfo[deviceName].statusMutex);
      this->m_scanInfo[deviceName].status = "Scan completed";

      // Schedule cleanup to happen later from the main thread
      // We can't erase from m_activeScans here since we're running in the scanner's thread
      // The scanner will be cleaned up when another scan is started or the program ends
    });

    scanner->SetErrorCallback([this, deviceName](const ScanErrorEventArgs& args) {
      // Update status, but don't remove scanner here
      this->m_scanInfo[deviceName].isActive.store(false);
      std::lock_guard<std::mutex> lock(this->m_scanInfo[deviceName].statusMutex);
      this->m_scanInfo[deviceName].status = "Error: " + args.GetError();

      // Same as above - don't erase from m_activeScans here
    });

    // 9. Start the scan
    if (!scanner->StartScan()) {
      errorMessage = "Failed to start scan for device " + deviceName;
      m_logger->LogError("MachineOperations: " + errorMessage);
      scanStartSuccess = false;
    }
    else {
      // Store the scanner
      {
        std::lock_guard<std::mutex> lock(m_scanMutex);
        m_activeScans[deviceName] = std::move(scanner);
      }

      m_logger->LogInfo("MachineOperations: Scan started for device " + deviceName);
      scanStartSuccess = true;
    }
  }
  catch (const std::exception& e) {
    errorMessage = "Exception during scan setup: " + std::string(e.what());
    m_logger->LogError("MachineOperations: " + errorMessage);
    scanStartSuccess = false;
  }

  // 10. Store results and end tracking
  auto endTime = std::chrono::steady_clock::now();
  auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

  if (m_resultsManager && !opId.empty()) {
    m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
    m_resultsManager->StoreResult(opId, "scan_type", "asynchronous_start_scan");

    if (scanStartSuccess) {
      m_resultsManager->EndOperation(opId, "success");
    }
    else {
      m_resultsManager->EndOperation(opId, "failed", errorMessage.empty() ? "StartScan operation failed" : errorMessage);
    }
  }

  return scanStartSuccess;
}






// Keep existing StartScan method without callerContext (for backward compatibility)
bool MachineOperations::StartScan(const std::string& deviceName, const std::string& dataChannel,
  const std::vector<double>& stepSizes, int settlingTimeMs,
  const std::vector<std::string>& axesToScan) {
  return StartScan(deviceName, dataChannel, stepSizes, settlingTimeMs, axesToScan, "");
}


// StopScan method with tracking - keeping existing complex logic
bool MachineOperations::StopScan(const std::string& deviceName, const std::string& callerContext) {

  // 1. Start operation tracking
  std::string opId;
  auto startTime = std::chrono::steady_clock::now();

  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"device_name", deviceName}
    };
    opId = m_resultsManager->StartOperation("StopScan", deviceName, callerContext, "", parameters);
  }

  m_logger->LogInfo("MachineOperations: Stopping scan for device " + deviceName +
    (callerContext.empty() ? "" : " (called by: " + callerContext + ")") +
    (opId.empty() ? "" : " [" + opId + "]"));

  // 2. EXISTING COMPLEX LOGIC (unchanged)
  std::unique_ptr<ScanningAlgorithm> scanner;
  bool success = false;

  // Get the scanner and remove it from active scans
  {
    std::lock_guard<std::mutex> lock(m_scanMutex);
    auto it = m_activeScans.find(deviceName);
    if (it == m_activeScans.end()) {
      // Even if the scanner isn't found, reset the scan info data
      // This is critical for recovery from abnormal states
      auto infoIt = m_scanInfo.find(deviceName);
      if (infoIt != m_scanInfo.end()) {
        infoIt->second.isActive.store(false);
        std::lock_guard<std::mutex> statusLock(infoIt->second.statusMutex);
        infoIt->second.status = "No active scan";
      }
      m_logger->LogWarning("MachineOperations: No active scan found for device " + deviceName + ", but reset status anyway");
      success = true; // Return success since the end goal is achieved (no active scan)
    }
    else {
      scanner = std::move(it->second);
      m_activeScans.erase(it);
    }
  }

  // Stop the scan
  if (scanner) {
    scanner->HaltScan();
    m_logger->LogInfo("MachineOperations: Scan stopped for device " + deviceName);
    // Update scan status
    auto it = m_scanInfo.find(deviceName);
    if (it != m_scanInfo.end()) {
      it->second.isActive.store(false);
      std::lock_guard<std::mutex> lock(it->second.statusMutex);
      it->second.status = "Scan stopped by user";
    }
    // Use our safe cleanup method
    success = SafelyCleanupScanner(deviceName);
  }
  else if (!success) {
    success = false;
  }

  // 3. Store results and end tracking
  auto endTime = std::chrono::steady_clock::now();
  auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

  if (m_resultsManager && !opId.empty()) {
    m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));

    if (success) {
      m_resultsManager->EndOperation(opId, "success");
    }
    else {
      m_resultsManager->EndOperation(opId, "failed", "StopScan operation failed");
    }
  }

  return success;
}




// Keep existing StopScan method without callerContext (for backward compatibility)
bool MachineOperations::StopScan(const std::string& deviceName) {
  return StopScan(deviceName, "");
}

// Add a new method to machine_operations.cpp to reset scan state if needed
bool MachineOperations::ResetScanState(const std::string& deviceName) {
  std::lock_guard<std::mutex> lock(m_scanMutex);

  // Reset active scans tracking
  auto scanIt = m_activeScans.find(deviceName);
  if (scanIt != m_activeScans.end()) {
    // Try to halt the scan if it's still active
    if (scanIt->second && scanIt->second->IsScanningActive()) {
      scanIt->second->HaltScan();

      // Wait briefly for it to stop
      for (int i = 0; i < 10; i++) {
        if (!scanIt->second->IsScanningActive()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
    }

    // Remove from active scans
    m_activeScans.erase(scanIt);
    m_logger->LogInfo("MachineOperations: Removed stalled scan for " + deviceName);
  }

  // Reset scan info state
  auto infoIt = m_scanInfo.find(deviceName);
  if (infoIt != m_scanInfo.end()) {
    infoIt->second.isActive.store(false);
    std::lock_guard<std::mutex> statusLock(infoIt->second.statusMutex);
    infoIt->second.status = "Ready";
  }

  return true;
}

// Also add a cleanup method to be called before starting a new sequence
// This should be called at the start of your process sequence
bool MachineOperations::CleanupAllScanners() {
  std::lock_guard<std::mutex> lock(m_scanMutex);

  bool success = true;
  // First, try to stop all active scanners
  for (auto& [deviceName, scanner] : m_activeScans) {
    if (scanner && scanner->IsScanningActive()) {
      m_logger->LogInfo("MachineOperations: Halting lingering scan for " + deviceName);
      scanner->HaltScan();

      // Wait briefly for it to stop
      for (int i = 0; i < 10; i++) {
        if (!scanner->IsScanningActive()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }

      if (scanner->IsScanningActive()) {
        m_logger->LogWarning("MachineOperations: Failed to halt scan for " + deviceName);
        success = false;
      }
    }
  }

  // Clear all active scans regardless
  m_activeScans.clear();

  // Reset all scan info states
  for (auto& [deviceName, info] : m_scanInfo) {
    info.isActive.store(false);
    std::lock_guard<std::mutex> statusLock(info.statusMutex);
    info.status = "Ready";
  }

  return success;
}


bool MachineOperations::IsScanActive(const std::string& deviceName) const {
  auto it = m_scanInfo.find(deviceName);
  if (it != m_scanInfo.end()) {
    return it->second.isActive;
  }
  return false;
}

double MachineOperations::GetScanProgress(const std::string& deviceName) const {
  auto it = m_scanInfo.find(deviceName);
  if (it != m_scanInfo.end()) {
    return it->second.progress;
  }
  return 0.0;
}

std::string MachineOperations::GetScanStatus(const std::string& deviceName) const {
  auto it = m_scanInfo.find(deviceName);
  if (it != m_scanInfo.end()) {
    std::lock_guard<std::mutex> lock(it->second.statusMutex);
    return it->second.status;
  }
  return "No scan information available";
}

bool MachineOperations::GetScanPeak(const std::string& deviceName, double& value, PositionStruct& position) const {
  auto it = m_scanInfo.find(deviceName);
  if (it != m_scanInfo.end()) {
    std::lock_guard<std::mutex> lock(it->second.peakMutex);
    value = it->second.peakValue;
    position = it->second.peakPosition;
    return (value > 0.0); // Return true if a valid peak was found
  }
  return false;
}

// Safe cleanup method to call before destructing the scanner
bool MachineOperations::SafelyCleanupScanner(const std::string& deviceName) {
  std::unique_ptr<ScanningAlgorithm> scanner;

  // Get the scanner and remove it from active scans
  {
    std::lock_guard<std::mutex> lock(m_scanMutex);
    auto it = m_activeScans.find(deviceName);
    if (it == m_activeScans.end()) {
      return false;  // Nothing to clean up
    }

    scanner = std::move(it->second);
    m_activeScans.erase(it);
  }

  // Now safely stop the scan if it's running
  if (scanner) {
    if (scanner->IsScanningActive()) {
      scanner->HaltScan();

      // Wait a brief period to allow thread to exit
      for (int i = 0; i < 50; i++) {  // Wait up to 5 seconds
        if (!scanner->IsScanningActive()) {
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }

    // The scanner will be destructed when this method returns
    return true;
  }

  return false;
}


bool MachineOperations::IsDevicePIController(const std::string& deviceName) const {
  // Get the device info from configuration (using motion layer, which has access to config)
  auto deviceOpt = m_motionLayer.GetConfigManager().GetDevice(deviceName);
  if (!deviceOpt.has_value()) {
    m_logger->LogError("MachineOperations: Device " + deviceName + " not found in configuration");
    return false;
  }

  const auto& device = deviceOpt.value().get();

  // PI controllers use port 50000, ACS uses different ports (like 701)
  return (device.Port == 50000);
}

bool MachineOperations::IsDeviceMoving(const std::string& deviceName) {
  // Determine which controller manager to use
  if (IsDevicePIController(deviceName)) {
    PIController* controller = m_piControllerManager.GetController(deviceName);
    if (!controller || !controller->IsConnected()) {
      m_logger->LogError("MachineOperations: No connected PI controller for device " + deviceName);
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


bool MachineOperations::WaitForDeviceMotionCompletion(const std::string& deviceName, int timeoutMs) {
  m_logger->LogInfo("MachineOperations: Waiting for device " + deviceName + " motion to complete");

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
        m_logger->LogInfo("MachineOperations: Motion completed for device " + deviceName);
        return true;
      }
    }
    else {
      // Never detected any movement
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();

      // If we've waited reasonably long and never saw movement, assume it was quick or unnecessary
      if (elapsed > 1000) {
        m_logger->LogInfo("MachineOperations: No motion detected for device " + deviceName);
        return true;
      }
    }

    // Check for timeout
    if (std::chrono::steady_clock::now() > endTime) {
      m_logger->LogError("MachineOperations: Timeout waiting for motion completion of device " + deviceName);
      return false;
    }

    // Sleep to avoid CPU spinning
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

#pragma region Pylon Camera Test



// Camera control methods implementation
bool MachineOperations::InitializeCamera() {
  if (!m_cameraTest) {
    m_logger->LogError("MachineOperations: Camera not available");
    return false;
  }

  m_logger->LogInfo("MachineOperations: Initializing camera");
  bool success = m_cameraTest->GetCamera().Initialize();

  if (success) {
    m_logger->LogInfo("MachineOperations: Camera initialized successfully");
  }
  else {
    m_logger->LogError("MachineOperations: Failed to initialize camera");
  }

  return success;
}

bool MachineOperations::ConnectCamera() {
  if (!m_cameraTest) {
    m_logger->LogError("MachineOperations: Camera not available");
    return false;
  }

  if (!m_cameraTest->GetCamera().IsConnected()) {
    m_logger->LogInfo("MachineOperations: Connecting to camera");
    bool success = m_cameraTest->GetCamera().Connect();

    if (success) {
      m_logger->LogInfo("MachineOperations: Connected to camera successfully");
    }
    else {
      m_logger->LogError("MachineOperations: Failed to connect to camera");
    }

    return success;
  }

  m_logger->LogInfo("MachineOperations: Camera already connected");
  return true;
}

bool MachineOperations::DisconnectCamera() {
  if (!m_cameraTest) {
    m_logger->LogError("MachineOperations: Camera not available");
    return false;
  }

  if (m_cameraTest->GetCamera().IsConnected()) {
    m_logger->LogInfo("MachineOperations: Disconnecting camera");
    m_cameraTest->GetCamera().Disconnect();
    m_logger->LogInfo("MachineOperations: Camera disconnected");
    return true;
  }

  m_logger->LogInfo("MachineOperations: Camera not connected");
  return true;
}

bool MachineOperations::StartCameraGrabbing() {
  if (!m_cameraTest) {
    m_logger->LogError("MachineOperations: Camera not available");
    return false;
  }

  if (!m_cameraTest->GetCamera().IsConnected()) {
    m_logger->LogWarning("MachineOperations: Camera not connected, attempting to connect");
    if (!ConnectCamera()) {
      return false;
    }
  }

  if (!m_cameraTest->GetCamera().IsGrabbing()) {
    m_logger->LogInfo("MachineOperations: Starting camera grabbing");
    bool success = m_cameraTest->GetCamera().StartGrabbing();

    if (success) {
      m_logger->LogInfo("MachineOperations: Camera grabbing started");
    }
    else {
      m_logger->LogError("MachineOperations: Failed to start camera grabbing");
    }

    return success;
  }

  m_logger->LogInfo("MachineOperations: Camera already grabbing");
  return true;
}

bool MachineOperations::StopCameraGrabbing() {
  if (!m_cameraTest) {
    m_logger->LogError("MachineOperations: Camera not available");
    return false;
  }

  if (m_cameraTest->GetCamera().IsGrabbing()) {
    m_logger->LogInfo("MachineOperations: Stopping camera grabbing");
    m_cameraTest->GetCamera().StopGrabbing();
    m_logger->LogInfo("MachineOperations: Camera grabbing stopped");
    return true;
  }

  m_logger->LogInfo("MachineOperations: Camera not grabbing");
  return true;
}

bool MachineOperations::IsCameraInitialized() const {
  if (!m_cameraTest) {
    return false;
  }

  // The PylonCamera class doesn't directly expose an IsInitialized method,
  // so we'll assume it's initialized if it's connected or if it has a model name
  return m_cameraTest->GetCamera().IsConnected() ||
    !m_cameraTest->GetCamera().GetDeviceInfo().empty();
}

bool MachineOperations::IsCameraConnected() const {
  if (!m_cameraTest) {
    return false;
  }

  return m_cameraTest->GetCamera().IsConnected();
}

bool MachineOperations::IsCameraGrabbing() const {
  if (!m_cameraTest) {
    return false;
  }

  return m_cameraTest->GetCamera().IsGrabbing();
}

bool MachineOperations::CaptureImageToFile(const std::string& filename) {
  if (!m_cameraTest) {
    m_logger->LogError("MachineOperations: Camera not available");
    return false;
  }

  if (!m_cameraTest->GetCamera().IsConnected()) {
    m_logger->LogError("MachineOperations: Camera not connected");
    return false;
  }

  // Create a directory for image captures if it doesn't exist
  std::filesystem::path imgDir = "captures";
  if (!std::filesystem::exists(imgDir)) {
    try {
      m_logger->LogInfo("MachineOperations: Creating image capture directory: " + imgDir.string());
      std::filesystem::create_directory(imgDir);
    }
    catch (const std::filesystem::filesystem_error& e) {
      m_logger->LogError("MachineOperations: Failed to create directory: " + std::string(e.what()));
      // If we can't create the directory, continue with the current working directory
    }
  }

  // Generate a filename if not provided
  std::string actualFilename = filename;
  if (actualFilename.empty()) {
    // Generate default filename using timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "capture_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S") << ".png";
    actualFilename = ss.str();
  }

  // Build full path
  std::filesystem::path baseName = std::filesystem::path(actualFilename).filename(); // Extract just the filename
  std::filesystem::path fullPath = imgDir / baseName;
  std::string fullPathStr = fullPath.string();

  m_logger->LogInfo("MachineOperations: Capturing image to file: " + fullPathStr);

  // If the camera is not grabbing, we need to grab a single frame first
  if (!m_cameraTest->GetCamera().IsGrabbing()) {
    m_logger->LogInfo("MachineOperations: Starting camera grabbing for single capture");
    if (!m_cameraTest->GrabSingleFrame()) {
      m_logger->LogError("MachineOperations: Failed to grab single frame");
      return false;
    }
  }

  // We need to modify the default capture behavior to specify our filename
  // Since pylonCameraTest.CaptureImage() generates its own filename,
  // we can use a temporary approach to save to our desired path
  try {
    std::lock_guard<std::mutex> lock(m_cameraTest->m_imageMutex); // Access protected member

    // Check if we have a valid grab result
    if (!m_cameraTest->m_ptrGrabResult || !m_cameraTest->m_ptrGrabResult->GrabSucceeded() ||
      !m_cameraTest->m_pylonImage.IsValid()) {
      m_logger->LogError("MachineOperations: No valid frame available to capture");
      return false;
    }

    // Save the image directly using our path
    Pylon::CImagePersistence::Save(Pylon::ImageFileFormat_Png, fullPathStr.c_str(), m_cameraTest->m_pylonImage);

    // Update the camera test's status
    m_cameraTest->m_imageCaptured = true;
    m_cameraTest->m_lastSavedPath = fullPathStr;

    m_logger->LogInfo("MachineOperations: Image captured successfully to " + fullPathStr);
    return true;
  }
  catch (const Pylon::GenericException& e) {
    m_logger->LogError("MachineOperations: Error saving image: " + std::string(e.GetDescription()));
    return false;
  }
  catch (const std::exception& e) {
    m_logger->LogError("MachineOperations: Error during image capture: " + std::string(e.what()));
    return false;
  }
}



bool MachineOperations::UpdateCameraDisplay() {
  if (!m_cameraTest) {
    return false;
  }

  // Check if camera is grabbing frames
  if (!m_cameraTest->GetCamera().IsGrabbing()) {
    return false;
  }

  // The camera display will be updated through the RenderUI method in the main loop
  return true;
}
#pragma endregion


// 2. UPDATE MoveRelative method with database tracking:
bool MachineOperations::MoveRelative(const std::string& deviceName,
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

  m_logger->LogInfo("MachineOperations: Moving device " + deviceName +
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
    m_logger->LogError("MachineOperations: Device not connected: " + deviceName);

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
    m_logger->LogInfo("MachineOperations: Successfully initiated relative move for device " +
      deviceName + " on axis " + axis);
  }
  else {
    m_logger->LogError("MachineOperations: Failed to move device " + deviceName +
      " relative on axis " + axis);
  }

  return success;
}


// Add this new method to integrate camera control with motion control
bool MachineOperations::IntegrateCameraWithMotion(PylonCameraTest* cameraTest) {
  if (!cameraTest) {
    m_logger->LogError("MachineOperations: Cannot integrate camera - camera test is null");
    return false;
  }

  // Set default calibration factors - you may need to adjust these based on your camera and lens
  // This is the conversion factor from pixels to mm
  cameraTest->SetPixelToMMFactors(0.00248f, 0.00248f);  // 0.01mm per pixel for both X and Y

  // Render the UI with machine operations integration
  cameraTest->RenderUIWithMachineOps(this);

  return true;
}

// Get the current node for a device in a motion graph
std::string MachineOperations::GetDeviceCurrentNode(const std::string& deviceName, const std::string& graphName) {
  m_logger->LogInfo("MachineOperations: Getting current node for device " + deviceName +
    " in graph " + graphName);

  std::string currentNodeId;
  if (!m_motionLayer.GetDeviceCurrentNode(graphName, deviceName, currentNodeId)) {
    m_logger->LogError("MachineOperations: Failed to get current node for device " + deviceName);
    return "";
  }

  return currentNodeId;
}

std::string MachineOperations::GetDeviceCurrentPositionName(const std::string& deviceName) {
  m_logger->LogInfo("MachineOperations: Getting current named position for device " + deviceName);

  // Get current position
  PositionStruct currentPosition;
  if (!GetDeviceCurrentPosition(deviceName, currentPosition)) {
    m_logger->LogError("MachineOperations: Failed to get current position for device " + deviceName);
    return "";
  }

  // Get all named positions for the device
  auto& configManager = m_motionLayer.GetConfigManager();
  auto namedPositionsOpt = configManager.GetNamedPositions(deviceName);
  if (!namedPositionsOpt.has_value()) {
    m_logger->LogWarning("MachineOperations: No named positions found for device " + deviceName);
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
    m_logger->LogInfo("MachineOperations: Device " + deviceName +
      " is at named position " + closestPosName);
    return closestPosName;
  }

  // If no position is close enough, return empty string
  m_logger->LogInfo("MachineOperations: Device " + deviceName +
    " is not at any named position (closest: " + closestPosName +
    ", distance: " + std::to_string(minDistance) + " mm)");
  return "";
}
// Get the current position for a device
bool MachineOperations::GetDeviceCurrentPosition(const std::string& deviceName, PositionStruct& position) {
  m_logger->LogInfo("MachineOperations: Getting current position for device " + deviceName);

  // Use the motion layer to get the current position
  if (!m_motionLayer.GetCurrentPosition(deviceName, position)) {
    m_logger->LogError("MachineOperations: Failed to get current position for device " + deviceName);
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

  m_logger->LogInfo("MachineOperations: " + posStr.str());
  return true;
}

//// Calculate the distance between two positions
//double MachineOperations::GetDistanceBetweenPositions(const PositionStruct& pos1,
//  const PositionStruct& pos2,
//  bool includeRotation) {
//  // Calculate Euclidean distance for XYZ
//  double dx = pos1.x - pos2.x;
//  double dy = pos1.y - pos2.y;
//  double dz = pos1.z - pos2.z;
//
//  double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
//
//  // If rotation should be included
//  if (includeRotation) {
//    double du = pos1.u - pos2.u;
//    double dv = pos1.v - pos2.v;
//    double dw = pos1.w - pos2.w;
//
//    // Add weighted rotation component to distance
//    // Weight rotation less than translation (scale factor 0.1)
//    double rotationFactor = 0.1;
//    double rotDistance = std::sqrt(du * du + dv * dv + dw * dw) * rotationFactor;
//
//    // Combine the distances
//    distance = std::sqrt(distance * distance + rotDistance * rotDistance);
//  }
//
//  return distance;
//}


// ADD these new methods at the end of machine_operations.cpp:


// 5. ADD these new methods to machine_operations.cpp:
bool MachineOperations::ApplyCameraExposureForNode(const std::string& nodeId) {
  if (!m_cameraTest || !m_cameraExposureManager) {
    m_logger->LogWarning("MachineOperations: Camera or exposure manager not available");
    return false;
  }

  if (!m_cameraTest->GetCamera().IsConnected()) {
    m_logger->LogWarning("MachineOperations: Camera not connected, cannot apply exposure settings");
    return false;
  }

  m_logger->LogInfo("MachineOperations: Applying camera exposure settings for node " + nodeId);

  // Small delay to ensure gantry has settled at the new position
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  bool success = m_cameraExposureManager->ApplySettingsForNode(m_cameraTest->GetCamera(), nodeId);

  if (success) {
    m_logger->LogInfo("MachineOperations: Successfully applied camera exposure for node " + nodeId);
  }
  else {
    m_logger->LogWarning("MachineOperations: Failed to apply specific exposure for node " + nodeId + ", trying default");
    success = ApplyDefaultCameraExposure();
  }

  return success;
}

bool MachineOperations::ApplyDefaultCameraExposure() {
  if (!m_cameraTest || !m_cameraExposureManager) {
    m_logger->LogWarning("MachineOperations: Camera or exposure manager not available");
    return false;
  }

  if (!m_cameraTest->GetCamera().IsConnected()) {
    m_logger->LogWarning("MachineOperations: Camera not connected, cannot apply default exposure");
    return false;
  }

  m_logger->LogInfo("MachineOperations: Applying default camera exposure settings");

  bool success = m_cameraExposureManager->ApplyDefaultSettings(m_cameraTest->GetCamera());

  if (success) {
    m_logger->LogInfo("MachineOperations: Successfully applied default camera exposure");
  }
  else {
    m_logger->LogError("MachineOperations: Failed to apply default camera exposure");
  }

  return success;
}

// machine_operations.cpp - FIXED implementation for the new position storage methods
// Add these methods to your existing machine_operations.cpp file
// REMOVE the duplicate CalculateDistanceFromStored method around line 1100

// NEW: Temporary Position Storage Implementation
// ==============================================

bool MachineOperations::CaptureCurrentPosition(const std::string& deviceName, const std::string& label) {
  if (label.empty()) {
    m_logger->LogError("MachineOperations: Cannot capture position with empty label");
    return false;
  }

  m_logger->LogInfo("MachineOperations: Capturing current position for device " + deviceName +
    " with label '" + label + "'");

  // Get current position
  PositionStruct currentPosition;
  if (!GetDeviceCurrentPosition(deviceName, currentPosition)) {
    m_logger->LogError("MachineOperations: Failed to get current position for device " + deviceName);
    return false;
  }

  // Store the position
  {
    std::lock_guard<std::mutex> lock(m_positionStorageMutex);
    m_storedPositions[label] = StoredPositionInfo(deviceName, currentPosition);
  }

  m_logger->LogInfo("MachineOperations: Successfully stored position '" + label + "' for device " +
    deviceName + " at coordinates: X=" + std::to_string(currentPosition.x) +
    " Y=" + std::to_string(currentPosition.y) +
    " Z=" + std::to_string(currentPosition.z));

  return true;
}

bool MachineOperations::GetStoredPosition(const std::string& label, PositionStruct& position) const {
  std::lock_guard<std::mutex> lock(m_positionStorageMutex);

  auto it = m_storedPositions.find(label);
  if (it == m_storedPositions.end()) {
    m_logger->LogWarning("MachineOperations: Stored position '" + label + "' not found");
    return false;
  }

  position = it->second.position;
  return true;
}

std::vector<std::string> MachineOperations::GetStoredPositionLabels(const std::string& deviceNameFilter) const {
  std::vector<std::string> labels;
  std::lock_guard<std::mutex> lock(m_positionStorageMutex);

  for (const auto& [label, info] : m_storedPositions) {
    if (deviceNameFilter.empty() || info.deviceName == deviceNameFilter) {
      labels.push_back(label);
    }
  }

  return labels;
}

// FIXED: This method is NOT const because it calls GetDeviceCurrentPosition which is not const
double MachineOperations::CalculateDistanceFromStored(const std::string& deviceName,
  const std::string& storedLabel) {
  // Get stored position
  PositionStruct storedPosition;
  if (!GetStoredPosition(storedLabel, storedPosition)) {
    m_logger->LogError("MachineOperations: Cannot calculate distance - stored position '" +
      storedLabel + "' not found");
    return -1.0;
  }

  // Verify the stored position is for the correct device
  {
    std::lock_guard<std::mutex> lock(m_positionStorageMutex);
    auto it = m_storedPositions.find(storedLabel);
    if (it != m_storedPositions.end() && it->second.deviceName != deviceName) {
      m_logger->LogWarning("MachineOperations: Stored position '" + storedLabel +
        "' is for device '" + it->second.deviceName +
        "', not '" + deviceName + "'");
    }
  }

  // Get current position (this method is NOT const, so this method can't be const either)
  PositionStruct currentPosition;
  if (!GetDeviceCurrentPosition(deviceName, currentPosition)) {
    m_logger->LogError("MachineOperations: Cannot get current position for device " + deviceName);
    return -1.0;
  }

  // Calculate distance using the const method
  double distance = GetDistanceBetweenPositions(currentPosition, storedPosition, false);

  m_logger->LogInfo("MachineOperations: Distance from stored position '" + storedLabel +
    "' to current position of " + deviceName + ": " +
    std::to_string(distance) + " mm");

  return distance;
}

// FIXED: This method is NOT const because it calls CalculateDistanceFromStored which is not const
bool MachineOperations::HasMovedFromStored(const std::string& deviceName,
  const std::string& storedLabel,
  double tolerance) {
  double distance = CalculateDistanceFromStored(deviceName, storedLabel);

  if (distance < 0) {
    // Error occurred in distance calculation
    return false;
  }

  bool hasMoved = distance > tolerance;

  if (hasMoved) {
    m_logger->LogInfo("MachineOperations: Device " + deviceName + " has moved " +
      std::to_string(distance) + " mm from stored position '" +
      storedLabel + "' (tolerance: " + std::to_string(tolerance) + " mm)");
  }

  return hasMoved;
}

void MachineOperations::ClearStoredPositions(const std::string& deviceNameFilter) {
  std::lock_guard<std::mutex> lock(m_positionStorageMutex);

  if (deviceNameFilter.empty()) {
    // Clear all stored positions
    size_t clearedCount = m_storedPositions.size();
    m_storedPositions.clear();
    m_logger->LogInfo("MachineOperations: Cleared all " + std::to_string(clearedCount) +
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
    m_logger->LogInfo("MachineOperations: Cleared " + std::to_string(clearedCount) +
      " stored positions for device '" + deviceNameFilter + "'");
  }
}

void MachineOperations::ClearOldStoredPositions(int maxAgeMinutes) {
  std::lock_guard<std::mutex> lock(m_positionStorageMutex);

  auto cutoffTime = std::chrono::steady_clock::now() - std::chrono::minutes(maxAgeMinutes);
  size_t clearedCount = 0;

  auto it = m_storedPositions.begin();
  while (it != m_storedPositions.end()) {
    if (it->second.timestamp < cutoffTime) {
      m_logger->LogInfo("MachineOperations: Removing old stored position '" + it->first +
        "' for device '" + it->second.deviceName + "'");
      it = m_storedPositions.erase(it);
      clearedCount++;
    }
    else {
      ++it;
    }
  }

  if (clearedCount > 0) {
    m_logger->LogInfo("MachineOperations: Cleared " + std::to_string(clearedCount) +
      " stored positions older than " + std::to_string(maxAgeMinutes) + " minutes");
  }
}

bool MachineOperations::GetStoredPositionInfo(const std::string& label,
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

// FIXED: Make sure this implementation is marked as const (should already be in your file)
double MachineOperations::GetDistanceBetweenPositions(const PositionStruct& pos1,
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

// Updated machine_operations.cpp - Now using MotionControlLayer methods instead of direct config access

// Save current position of a specific controller to motion_config.json as a named position
bool MachineOperations::SaveCurrentPositionToConfig(const std::string& deviceName, const std::string& positionName) {
  m_logger->LogInfo("MachineOperations: Delegating position save to MotionControlLayer");

  // Validate device name and position name
  if (deviceName.empty() || positionName.empty()) {
    m_logger->LogError("MachineOperations: Device name and position name cannot be empty");
    return false;
  }

  // Check if device is connected before attempting to save
  if (!IsDeviceConnected(deviceName)) {
    m_logger->LogError("MachineOperations: Device " + deviceName + " is not connected");
    return false;
  }

  // Check if device is moving (optional safety check)
  if (IsDeviceMoving(deviceName)) {
    m_logger->LogWarning("MachineOperations: Device " + deviceName + " is currently moving, position may not be stable");
  }

  // Delegate to MotionControlLayer which has proper access to MotionConfigManager
  bool success = m_motionLayer.SaveCurrentPositionToConfig(deviceName, positionName);

  if (success) {
    m_logger->LogInfo("MachineOperations: Successfully saved position '" + positionName +
      "' for device " + deviceName + " to motion_config.json");
  }
  else {
    m_logger->LogError("MachineOperations: Failed to save position '" + positionName +
      "' for device " + deviceName);
  }

  return success;
}

// Update an existing named position with current position
bool MachineOperations::UpdateNamedPositionInConfig(const std::string& deviceName, const std::string& positionName) {
  m_logger->LogInfo("MachineOperations: Delegating position update to MotionControlLayer");

  // Validate inputs
  if (deviceName.empty() || positionName.empty()) {
    m_logger->LogError("MachineOperations: Device name and position name cannot be empty");
    return false;
  }

  // Check if device is connected
  if (!IsDeviceConnected(deviceName)) {
    m_logger->LogError("MachineOperations: Device " + deviceName + " is not connected");
    return false;
  }

  // Delegate to MotionControlLayer
  bool success = m_motionLayer.UpdateNamedPositionInConfig(deviceName, positionName);

  if (success) {
    m_logger->LogInfo("MachineOperations: Successfully updated position '" + positionName +
      "' for device " + deviceName);
  }
  else {
    m_logger->LogError("MachineOperations: Failed to update position '" + positionName +
      "' for device " + deviceName);
  }

  return success;
}

// Save current positions of all connected controllers to config
bool MachineOperations::SaveAllCurrentPositionsToConfig(const std::string& prefix) {
  m_logger->LogInfo("MachineOperations: Delegating bulk position save to MotionControlLayer");

  // Get current positions to check if any devices are available
  auto allPositions = GetCurrentPositions();

  if (allPositions.empty()) {
    m_logger->LogWarning("MachineOperations: No controller positions available to save");
    return false;
  }

  // Log what we're about to save
  m_logger->LogInfo("MachineOperations: Saving positions for " +
    std::to_string(allPositions.size()) + " devices");

  // Delegate to MotionControlLayer
  bool success = m_motionLayer.SaveAllCurrentPositionsToConfig(prefix);

  if (success) {
    m_logger->LogInfo("MachineOperations: Successfully saved all current positions to configuration");
  }
  else {
    m_logger->LogError("MachineOperations: Failed to save some current positions to configuration");
  }

  return success;
}

// Create backup of motion config file
bool MachineOperations::BackupMotionConfig(const std::string& backupSuffix) {
  m_logger->LogInfo("MachineOperations: Delegating config backup to MotionControlLayer");

  bool success = m_motionLayer.BackupMotionConfig(backupSuffix);

  if (success) {
    m_logger->LogInfo("MachineOperations: Successfully created configuration backup");
  }
  else {
    m_logger->LogError("MachineOperations: Failed to create configuration backup");
  }

  return success;
}

// Restore motion config from backup
bool MachineOperations::RestoreMotionConfigFromBackup(const std::string& backupSuffix) {
  m_logger->LogInfo("MachineOperations: Restoring configuration from backup");

  try {
    std::string configPath = "motion_config.json";
    std::string backupPath = "motion_config_backup_" + backupSuffix + ".json";

    // Check if backup file exists
    if (!std::filesystem::exists(backupPath)) {
      m_logger->LogError("MachineOperations: Backup file not found: " + backupPath);
      return false;
    }

    // Create backup of current config before restore
    if (!m_motionLayer.BackupMotionConfig("before_restore")) {
      m_logger->LogWarning("MachineOperations: Failed to backup current config before restore");
    }

    // Copy backup file over current config
    std::filesystem::copy_file(backupPath, configPath, std::filesystem::copy_options::overwrite_existing);

    m_logger->LogInfo("MachineOperations: Restored config from backup: " + backupPath);

    // Notify about configuration reload requirement
    m_logger->LogInfo("MachineOperations: Configuration file restored successfully");
    m_logger->LogWarning("MachineOperations: Please restart application to use restored configuration");

    return true;

  }
  catch (const std::exception& e) {
    m_logger->LogError("MachineOperations: Failed to restore from backup: " + std::string(e.what()));
    return false;
  }
}



// Add these methods to machine_operations.cpp

// Get current positions of all controllers and store in variable for other classes to use
std::map<std::string, PositionStruct> MachineOperations::GetCurrentPositions() {
  m_logger->LogInfo("MachineOperations: Getting current positions for all controllers");

  std::lock_guard<std::mutex> lock(m_currentPositionsMutex);

  // Check if cache is still valid (within timeout)
  auto now = std::chrono::steady_clock::now();
  if (now - m_lastPositionUpdate < POSITION_CACHE_TIMEOUT && !m_currentPositions.empty()) {
    m_logger->LogInfo("MachineOperations: Returning cached positions (" +
      std::to_string(m_currentPositions.size()) + " devices)");
    return m_currentPositions;
  }

  // Clear existing positions
  m_currentPositions.clear();

  // Get all available devices from the motion layer
  std::vector<std::string> deviceList = m_motionLayer.GetAvailableDevices();

  if (deviceList.empty()) {
    m_logger->LogWarning("MachineOperations: No devices available for position reading");
    return m_currentPositions;
  }

  // Get current position for each connected device
  int successCount = 0;
  for (const auto& deviceName : deviceList) {
    // Check if device is connected before trying to get position
    if (!IsDeviceConnected(deviceName)) {
      m_logger->LogWarning("MachineOperations: Device " + deviceName + " is not connected, skipping");
      continue;
    }

    PositionStruct currentPosition;
    if (GetDeviceCurrentPosition(deviceName, currentPosition)) {
      m_currentPositions[deviceName] = currentPosition;
      successCount++;

      m_logger->LogInfo("MachineOperations: Got position for " + deviceName +
        " - X:" + std::to_string(currentPosition.x) +
        " Y:" + std::to_string(currentPosition.y) +
        " Z:" + std::to_string(currentPosition.z));
    }
    else {
      m_logger->LogError("MachineOperations: Failed to get current position for device " + deviceName);
    }
  }

  // Update cache timestamp
  m_lastPositionUpdate = now;

  m_logger->LogInfo("MachineOperations: Successfully retrieved positions for " +
    std::to_string(successCount) + " out of " +
    std::to_string(deviceList.size()) + " devices");

  return m_currentPositions;
}

// Update all current positions (refresh cache)
bool MachineOperations::UpdateAllCurrentPositions() {
  m_logger->LogInfo("MachineOperations: Updating all current positions (forced refresh)");

  std::lock_guard<std::mutex> lock(m_currentPositionsMutex);

  // Force cache invalidation by setting old timestamp
  m_lastPositionUpdate = std::chrono::steady_clock::time_point{};

  // Clear current cache
  m_currentPositions.clear();

  // Get all available devices
  std::vector<std::string> deviceList = m_motionLayer.GetAvailableDevices();

  if (deviceList.empty()) {
    m_logger->LogWarning("MachineOperations: No devices available for position update");
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
    m_logger->LogInfo("MachineOperations: Successfully updated all " +
      std::to_string(successCount) + " device positions");
  }
  else {
    m_logger->LogWarning("MachineOperations: Updated " + std::to_string(successCount) +
      " out of " + std::to_string(deviceList.size()) + " device positions");
  }

  return allSuccess;
}

// Get cached positions without updating (const method for other classes to use)
const std::map<std::string, PositionStruct>& MachineOperations::GetCachedPositions() const {
  std::lock_guard<std::mutex> lock(m_currentPositionsMutex);
  return m_currentPositions;
}

// Force refresh position cache
void MachineOperations::RefreshPositionCache() {
  m_logger->LogInfo("MachineOperations: Refreshing position cache");

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

bool MachineOperations::ReloadMotionConfig() {
  m_logger->LogInfo("MachineOperations: Forcing motion configuration reload");

  bool success = m_motionLayer.ReloadMotionConfig();

  if (success) {
    m_logger->LogInfo("MachineOperations: Motion configuration reloaded successfully");
  }
  else {
    m_logger->LogError("MachineOperations: Failed to reload motion configuration");
  }

  return success;
}

// 3. machine_operations.cpp - Implementation:
bool MachineOperations::SaveCurrentPositionForNode(const std::string& deviceName, const std::string& graphName, const std::string& nodeId) {
  m_logger->LogInfo("MachineOperations: Saving current position for node " + nodeId +
    " in graph " + graphName + " for device " + deviceName);

  // Validate inputs
  if (deviceName.empty() || graphName.empty() || nodeId.empty()) {
    m_logger->LogError("MachineOperations: Device name, graph name, and node ID cannot be empty");
    return false;
  }

  // Check if device is connected
  if (!IsDeviceConnected(deviceName)) {
    m_logger->LogError("MachineOperations: Device " + deviceName + " is not connected");
    return false;
  }

  try {
    // Get the graph from config manager
    auto graphOpt = m_motionLayer.GetMotionConfigManager().GetGraph(graphName);
    if (!graphOpt.has_value()) {
      m_logger->LogError("MachineOperations: Graph not found: " + graphName);
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
      m_logger->LogError("MachineOperations: Node " + nodeId + " not found for device " + deviceName + " in graph " + graphName);
      return false;
    }

    // Get the actual position name from the node
    std::string actualPositionName = targetNode->Position;

    if (actualPositionName.empty()) {
      m_logger->LogError("MachineOperations: Node " + nodeId + " has no position name defined");
      return false;
    }

    m_logger->LogInfo("MachineOperations: Node " + nodeId + " refers to position '" + actualPositionName + "'");

    // Now save the current position using the correct position name
    bool success = m_motionLayer.SaveCurrentPositionToConfig(deviceName, actualPositionName);

    if (success) {
      m_logger->LogInfo("MachineOperations: Successfully saved current position of " + deviceName +
        " to position '" + actualPositionName + "' (referenced by node " + nodeId + ")");
    }

    return success;
  }
  catch (const std::exception& e) {
    m_logger->LogError("MachineOperations: Exception while saving position for node: " + std::string(e.what()));
    return false;
  }
}


// SMU Control Methods Implementation

bool MachineOperations::SMU_ResetInstrument(const std::string& clientName) {
  if (!m_smuOps) {
    m_logger->LogError("MachineOperations: SMU operations not available");
    return false;
  }

  m_logger->LogInfo("MachineOperations: Resetting SMU instrument" +
    (clientName.empty() ? "" : " (" + clientName + ")"));

  return m_smuOps->ResetInstrument(clientName);
}

bool MachineOperations::SMU_SetOutput(bool enable, const std::string& clientName) {
  if (!m_smuOps) {
    m_logger->LogError("MachineOperations: SMU operations not available");
    return false;
  }

  m_logger->LogInfo("MachineOperations: " + std::string(enable ? "Enabling" : "Disabling") +
    " SMU output" + (clientName.empty() ? "" : " (" + clientName + ")"));

  return m_smuOps->SetOutput(enable, clientName);
}

bool MachineOperations::SMU_SetupVoltageSource(double voltage, double compliance,
  const std::string& range, const std::string& clientName) {
  if (!m_smuOps) {
    m_logger->LogError("MachineOperations: SMU operations not available");
    return false;
  }

  m_logger->LogInfo("MachineOperations: Setting up SMU voltage source - " +
    std::to_string(voltage) + "V, compliance " + std::to_string(compliance) + "A" +
    (clientName.empty() ? "" : " (" + clientName + ")"));

  return m_smuOps->SetupVoltageSource(voltage, compliance, range, clientName);
}

bool MachineOperations::SMU_SetupCurrentSource(double current, double compliance,
  const std::string& range, const std::string& clientName) {
  if (!m_smuOps) {
    m_logger->LogError("MachineOperations: SMU operations not available");
    return false;
  }

  m_logger->LogInfo("MachineOperations: Setting up SMU current source - " +
    std::to_string(current) + "A, compliance " + std::to_string(compliance) + "V" +
    (clientName.empty() ? "" : " (" + clientName + ")"));

  return m_smuOps->SetupCurrentSource(current, compliance, range, clientName);
}

bool MachineOperations::SMU_ReadVoltage(double& voltage, const std::string& clientName) {
  if (!m_smuOps) {
    m_logger->LogError("MachineOperations: SMU operations not available");
    return false;
  }

  bool result = m_smuOps->ReadVoltage(voltage, clientName);
  if (result) {
    m_logger->LogInfo("MachineOperations: SMU voltage reading: " + std::to_string(voltage) + "V" +
      (clientName.empty() ? "" : " (" + clientName + ")"));
  }
  return result;
}

bool MachineOperations::SMU_ReadCurrent(double& current, const std::string& clientName) {
  if (!m_smuOps) {
    m_logger->LogError("MachineOperations: SMU operations not available");
    return false;
  }

  bool result = m_smuOps->ReadCurrent(current, clientName);
  if (result) {
    m_logger->LogInfo("MachineOperations: SMU current reading: " + std::to_string(current) + "A" +
      (clientName.empty() ? "" : " (" + clientName + ")"));
  }
  return result;
}

bool MachineOperations::SMU_ReadResistance(double& resistance, const std::string& clientName) {
  if (!m_smuOps) {
    m_logger->LogError("MachineOperations: SMU operations not available");
    return false;
  }

  bool result = m_smuOps->ReadResistance(resistance, clientName);
  if (result) {
    m_logger->LogInfo("MachineOperations: SMU resistance reading: " + std::to_string(resistance) + "Ω" +
      (clientName.empty() ? "" : " (" + clientName + ")"));
  }
  return result;
}

bool MachineOperations::SMU_ReadPower(double& power, const std::string& clientName) {
  if (!m_smuOps) {
    m_logger->LogError("MachineOperations: SMU operations not available");
    return false;
  }

  bool result = m_smuOps->ReadPower(power, clientName);
  if (result) {
    m_logger->LogInfo("MachineOperations: SMU power reading: " + std::to_string(power) + "W" +
      (clientName.empty() ? "" : " (" + clientName + ")"));
  }
  return result;
}

bool MachineOperations::SMU_SendCommand(const std::string& command, const std::string& clientName) {
  if (!m_smuOps) {
    m_logger->LogError("MachineOperations: SMU operations not available");
    return false;
  }

  m_logger->LogInfo("MachineOperations: Sending SMU command: " + command +
    (clientName.empty() ? "" : " (" + clientName + ")"));

  return m_smuOps->SendWriteCommand(command, clientName);
}

bool MachineOperations::SMU_QueryCommand(const std::string& command, std::string& response,
  const std::string& clientName) {
  if (!m_smuOps) {
    m_logger->LogError("MachineOperations: SMU operations not available");
    return false;
  }

  m_logger->LogInfo("MachineOperations: Sending SMU query: " + command +
    (clientName.empty() ? "" : " (" + clientName + ")"));

  bool result = m_smuOps->SendQueryCommand(command, response, clientName);
  if (result) {
    m_logger->LogInfo("MachineOperations: SMU query response: " + response);
  }
  return result;
}

bool MachineOperations::SetDeviceSpeed(const std::string& deviceName, double velocity,
  const std::string& callerContext) {

  // 1. Start operation tracking
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

  m_logger->LogInfo("MachineOperations: Setting speed for " + controllerType + " device " + deviceName +
    " to " + std::to_string(velocity) + " mm/s" +
    (callerContext.empty() ? "" : " (called by: " + callerContext + ")") +
    (opId.empty() ? "" : " [" + opId + "]"));

  // 2. Store previous velocity if possible
  double previousVelocity = 0.0;
  bool hasPreviousVelocity = false;
  // Note: Could get current velocity here if needed for tracking

  // 3. Execute speed setting through motion layer (includes validation)
  bool success = m_motionLayer.SetDeviceVelocity(deviceName, velocity);

  // 4. Store results and end tracking
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
    m_logger->LogInfo("MachineOperations: Successfully set speed for " + controllerType + " device " + deviceName);
  }
  else {
    m_logger->LogError("MachineOperations: Failed to set speed for " + controllerType + " device " + deviceName +
      " - velocity may be outside limits (PI: 0.1-20 mm/s, ACS: 0.1-80 mm/s)");
  }

  return success;
}

// Add to machine_operations.cpp:
bool MachineOperations::GetDeviceSpeed(const std::string& deviceName, double& speed,
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



// Add these implementations to machine_operations.cpp:

bool MachineOperations::acsc_RunBuffer(const std::string& deviceName, int bufferNumber,
  const std::string& labelName, const std::string& callerContext) {
  // Start operation tracking
  std::string opId;
  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"buffer_number", std::to_string(bufferNumber)},
        {"label_name", labelName},
        {"device_name", deviceName}
    };

    // Extract sequence name from caller context
    std::string sequenceName = "";
    if (callerContext.find("BufferSequence") != std::string::npos) {
      sequenceName = "BufferSequence";
    }

    opId = m_resultsManager->StartOperation("acsc_RunBuffer", deviceName,
      callerContext, sequenceName, parameters);
  }

  m_logger->LogInfo("MachineOperations: Running ACS buffer " + std::to_string(bufferNumber) +
    " on device " + deviceName +
    (labelName.empty() ? "" : " from label " + labelName) +
    (callerContext.empty() ? "" : " (called by: " + callerContext + ")") +
    (opId.empty() ? "" : " [" + opId + "]"));

  // Check if device is connected
  if (!IsDeviceConnected(deviceName)) {
    m_logger->LogError("MachineOperations: Device not connected: " + deviceName);
    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->EndOperation(opId, "failed", "Device not connected");
    }
    return false;
  }

  // Call motion layer
  bool success = m_motionLayer.acsc_RunBuffer(deviceName, bufferNumber, labelName);

  // End operation tracking
  if (m_resultsManager && !opId.empty()) {
    m_resultsManager->EndOperation(opId, success ? "completed" : "failed",
      success ? "Buffer started successfully" : "Failed to start buffer");
  }

  if (success) {
    m_logger->LogInfo("MachineOperations: Successfully started ACS buffer " + std::to_string(bufferNumber) +
      " on device " + deviceName);
  }
  else {
    m_logger->LogError("MachineOperations: Failed to start ACS buffer " + std::to_string(bufferNumber) +
      " on device " + deviceName);
  }

  return success;
}

bool MachineOperations::acsc_StopBuffer(const std::string& deviceName, int bufferNumber,
  const std::string& callerContext) {
  // Start operation tracking
  std::string opId;
  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"buffer_number", std::to_string(bufferNumber)},
        {"device_name", deviceName}
    };

    // Extract sequence name from caller context
    std::string sequenceName = "";
    if (callerContext.find("BufferSequence") != std::string::npos) {
      sequenceName = "BufferSequence";
    }

    opId = m_resultsManager->StartOperation("acsc_StopBuffer", deviceName,
      callerContext, sequenceName, parameters);
  }

  m_logger->LogInfo("MachineOperations: Stopping ACS buffer " + std::to_string(bufferNumber) +
    " on device " + deviceName +
    (callerContext.empty() ? "" : " (called by: " + callerContext + ")") +
    (opId.empty() ? "" : " [" + opId + "]"));

  // Check if device is connected
  if (!IsDeviceConnected(deviceName)) {
    m_logger->LogError("MachineOperations: Device not connected: " + deviceName);
    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->EndOperation(opId, "failed", "Device not connected");
    }
    return false;
  }

  // Call motion layer
  bool success = m_motionLayer.acsc_StopBuffer(deviceName, bufferNumber);

  // End operation tracking
  if (m_resultsManager && !opId.empty()) {
    m_resultsManager->EndOperation(opId, success ? "completed" : "failed",
      success ? "Buffer stopped successfully" : "Failed to stop buffer");
  }

  if (success) {
    m_logger->LogInfo("MachineOperations: Successfully stopped ACS buffer " + std::to_string(bufferNumber) +
      " on device " + deviceName);
  }
  else {
    m_logger->LogError("MachineOperations: Failed to stop ACS buffer " + std::to_string(bufferNumber) +
      " on device " + deviceName);
  }

  return success;
}

bool MachineOperations::acsc_StopAllBuffers(const std::string& deviceName, const std::string& callerContext) {
  // Start operation tracking
  std::string opId;
  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"device_name", deviceName}
    };

    // Extract sequence name from caller context
    std::string sequenceName = "";
    if (callerContext.find("BufferSequence") != std::string::npos) {
      sequenceName = "BufferSequence";
    }

    opId = m_resultsManager->StartOperation("acsc_StopAllBuffers", deviceName,
      callerContext, sequenceName, parameters);
  }

  m_logger->LogInfo("MachineOperations: Stopping all ACS buffers on device " + deviceName +
    (callerContext.empty() ? "" : " (called by: " + callerContext + ")") +
    (opId.empty() ? "" : " [" + opId + "]"));

  // Check if device is connected
  if (!IsDeviceConnected(deviceName)) {
    m_logger->LogError("MachineOperations: Device not connected: " + deviceName);
    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->EndOperation(opId, "failed", "Device not connected");
    }
    return false;
  }

  // Call motion layer
  bool success = m_motionLayer.acsc_StopAllBuffers(deviceName);

  // End operation tracking
  if (m_resultsManager && !opId.empty()) {
    m_resultsManager->EndOperation(opId, success ? "completed" : "failed",
      success ? "All buffers stopped successfully" : "Failed to stop all buffers");
  }

  if (success) {
    m_logger->LogInfo("MachineOperations: Successfully stopped all ACS buffers on device " + deviceName);
  }
  else {
    m_logger->LogError("MachineOperations: Failed to stop all ACS buffers on device " + deviceName);
  }

  return success;
}

bool MachineOperations::acsc_IsBufferRunning(const std::string& deviceName, int bufferNumber) {
  // This is a query method, no need for operation tracking
  return m_motionLayer.acsc_IsBufferRunning(deviceName, bufferNumber);
}
