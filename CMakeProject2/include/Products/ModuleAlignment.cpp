// ModuleAlignment.cpp - Implementation of 3-point module alignment system
#include "ModuleAlignment.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <thread>
#include <filesystem>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =============================================================================
// CONSTRUCTION & LIFECYCLE
// =============================================================================

ModuleAlignment::ModuleAlignment() {
  m_logger = Logger::GetInstance();
  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: Initializing module alignment system");
  }

  // Initialize transformation matrices to identity
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      m_transformationMatrix[i][j] = (i == j) ? 1.0 : 0.0;
      m_inverseTransformationMatrix[i][j] = (i == j) ? 1.0 : 0.0;
    }
  }

  // Initialize vision system
  if (!InitializeVisionSystem()) {
    SetError("Failed to initialize vision system");
    return;
  }

  // Initialize database
  if (!InitializeDatabase()) {
    SetError("Failed to initialize alignment database");
    return;
  }

  // Load node-preset mappings
  LoadNodePresetMappings();

  ClearError();
  m_logger->LogInfo("ModuleAlignment: Initialization complete");
}

ModuleAlignment::~ModuleAlignment() {
  CloseDatabase();
  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: Cleanup complete");
  }
}

// =============================================================================
// MAIN ALIGNMENT INTERFACE
// =============================================================================

ModuleAlignment::AlignmentResult ModuleAlignment::PerformThreePointAlignment(
  const std::string& node1Name,
  const std::string& node2Name,
  const std::string& node3Name,
  bool useRobotZ) {

  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: Starting 3-point alignment: " + node1Name +
      ", " + node2Name + ", " + node3Name);
  }

  // Reset state
  m_alignmentResult = AlignmentResult();
  m_hasValidAlignment = false;
  ClearError();

  // Check dependencies
  if (!IsReadyForAlignment()) {
    SetError("MachineOperations not set - call SetMachineOperations() first");
    m_alignmentResult.errorMessage = m_lastError;
    return m_alignmentResult;
  }

  // Validate input parameters
  if (node1Name.empty() || node2Name.empty() || node3Name.empty()) {
    SetError("Node names cannot be empty");
    m_alignmentResult.errorMessage = m_lastError;
    return m_alignmentResult;
  }

  if (node1Name == node2Name || node1Name == node3Name || node2Name == node3Name) {
    SetError("All three nodes must be different");
    m_alignmentResult.errorMessage = m_lastError;
    return m_alignmentResult;
  }

  // Prepare alignment points
  std::vector<AlignmentPoint> points(3);
  points[0].nodeName = node1Name;
  points[1].nodeName = node2Name;
  points[2].nodeName = node3Name;

  // Step 1: Move to node1 and detect
  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: Step 1 - Moving to node1: " + node1Name);
  }
  bool useMockDetection = (m_cameraManager == nullptr);
  if (!MoveToNodeAndDetect(node1Name, points[0], useMockDetection)) {
    SetError("Failed to detect feature at node1: " + node1Name);
    m_alignmentResult.errorMessage = m_lastError;
    return m_alignmentResult;
  }
  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: Node1 detection successful - X:" +
      std::to_string(points[0].detectedPosition.x) +
      " Y:" + std::to_string(points[0].detectedPosition.y));
  }

  // Step 2: Move to node2 and detect  
  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: Step 2 - Moving to node2: " + node2Name);
  }
  if (!MoveToNodeAndDetect(node2Name, points[1], useMockDetection)) {
    SetError("Failed to detect feature at node2: " + node2Name);
    m_alignmentResult.errorMessage = m_lastError;
    return m_alignmentResult;
  }
  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: Node2 detection successful - X:" +
      std::to_string(points[1].detectedPosition.x) +
      " Y:" + std::to_string(points[1].detectedPosition.y));
  }

  // Step 3: Move to node3 and detect
  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: Step 3 - Moving to node3: " + node3Name);
  }
  if (!MoveToNodeAndDetect(node3Name, points[2], useMockDetection)) {
    SetError("Failed to detect feature at node3: " + node3Name);
    m_alignmentResult.errorMessage = m_lastError;
    return m_alignmentResult;
  }
  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: Node3 detection successful - X:" +
      std::to_string(points[2].detectedPosition.x) +
      " Y:" + std::to_string(points[2].detectedPosition.y));
  }

  // Store points in result
  m_alignmentResult.points = points;

  // Handle Z coordinate preference
  if (useRobotZ) {
    for (auto& point : m_alignmentResult.points) {
      point.detectedPosition.z = point.machinePosition.z;
    }
    if (m_logger) {
      m_logger->LogInfo("ModuleAlignment: Using robot Z coordinates");
    }
  }
  else {
    if (m_logger) {
      m_logger->LogInfo("ModuleAlignment: Using detected Z coordinates");
    }
  }

  // Step 4: Calculate coordinate system
  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: Step 4 - Calculating coordinate system");
  }
  if (!CalculateCoordinateSystem()) {
    m_alignmentResult.errorMessage = m_lastError;
    return m_alignmentResult;
  }

  // Step 5: Validate geometry
  if (!ValidateAlignmentGeometry()) {
    m_alignmentResult.errorMessage = m_lastError;
    return m_alignmentResult;
  }

  // Success
  m_alignmentResult.success = true;
  m_alignmentResult.timestamp = GetCurrentTimestamp();
  m_hasValidAlignment = true;

  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: 3-point alignment completed successfully");
    m_logger->LogInfo("ModuleAlignment: Center position - X:" +
      std::to_string(m_alignmentResult.centerPosition.x) +
      " Y:" + std::to_string(m_alignmentResult.centerPosition.y) +
      " Z:" + std::to_string(m_alignmentResult.centerPosition.z));
    m_logger->LogInfo("ModuleAlignment: X-axis length: " + std::to_string(m_alignmentResult.xAxisLength) +
      " Y-axis length: " + std::to_string(m_alignmentResult.yAxisLength));
    m_logger->LogInfo("ModuleAlignment: Axis angle: " + std::to_string(m_alignmentResult.axisAngle) + " degrees");
  }

  return m_alignmentResult;
}

bool ModuleAlignment::MoveToNodeAndDetect(const std::string& nodeName, AlignmentPoint& result, bool use) {
  m_logger->LogInfo("ModuleAlignment: Moving to node and detecting: " + nodeName);

  if (!m_machineOperations) {
    SetError("MachineOperations not set");
    return false;
  }

  // Step 1: Move gantry-main to the specified node
  if (!m_machineOperations->MoveDeviceToNode("gantry-main", "Process_Flow", nodeName, true)) {
    SetError("Failed to move gantry-main to node: " + nodeName);
    return false;
  }

  // Step 2: Get current machine position
  if (!m_machineOperations->GetDeviceCurrentPosition("gantry-main", result.machinePosition)) {
    SetError("Failed to get current machine position at node: " + nodeName);
    return false;
  }

  // Step 3: Load vision preset for this node from database
  if (!LoadVisionPresetForNode(nodeName)) {
    m_logger->LogWarning("ModuleAlignment: No vision preset found for node " + nodeName +
      ", using current settings");
  }

  // Step 4: Apply camera exposure settings for this node (if auto-exposure enabled)
  if (m_machineOperations && m_machineOperations->IsAutoExposureEnabled()) {
    m_machineOperations->ApplyCameraExposureForNode(nodeName);
  }

  // Step 5: Small delay to ensure camera settings are applied
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // Step 6: Capture image and perform vision detection
  if (!m_visionDetector) {
    SetError("Vision detector not initialized");
    return false;
  }

  // Get image from camera and detect
  VisionCircleDetection::Result visionResult;

  // Try to get image from camera manager if available
  if (m_cameraManager && !m_cameraManager->GetCameraIds().empty()) {
    auto cameraIds = m_cameraManager->GetCameraIds();
    std::string cameraId = cameraIds[0]; // Use first available camera

    if (m_logger) {
      m_logger->LogInfo("ModuleAlignment: Using camera ID: " + cameraId);
    }

    // Get camera hardware (same pattern as UIVisionPanel)
    ICameraHardware* camera = m_cameraManager->GetCameraHardware(cameraId);
    if (!camera || !camera->IsConnected()) {
      SetError("Camera not connected: " + cameraId);
      return false;
    }

    // Capture frame (same pattern as UIVisionPanel)
    CameraFrameData frameData;
    if (!camera->CaptureFrame(frameData)) {
      SetError("Failed to capture frame from camera");
      return false;
    }

    // Validate frame data (same pattern as UIVisionPanel)
    if (!frameData.IsValid() || frameData.imageData.empty()) {
      SetError("Invalid frame data from camera");
      return false;
    }

    if (m_logger) {
      m_logger->LogInfo("ModuleAlignment: Captured frame " + std::to_string(frameData.width) +
        "x" + std::to_string(frameData.height) +
        " (" + std::to_string(frameData.channels) + " channels)");
    }

    // Perform vision detection on captured frame
    visionResult = m_visionDetector->DetectFromBuffer(
      frameData.imageData.data(),
      frameData.width,
      frameData.height,
      frameData.channels
    );

    if (m_logger) {
      if (visionResult.found) {
        m_logger->LogInfo("ModuleAlignment: Real camera detection successful at " + nodeName +
          " - Center: (" + std::to_string(visionResult.centerX) +
          ", " + std::to_string(visionResult.centerY) +
          ") Confidence: " + std::to_string(visionResult.confidence));
      }
      else {
        m_logger->LogWarning("ModuleAlignment: Real camera detection failed at " + nodeName +
          ": " + visionResult.errorMessage);
      }
    }
  }
  else {
    SetError("No camera manager or cameras available for detection");
    return false;
  }

  // Step 7: Check detection results
  if (!visionResult.found) {
    SetError("Vision detection failed at node " + nodeName + ": " + visionResult.errorMessage);
    return false;
  }

  // Step 8: Store detection results
  result.detectedPosition.x = visionResult.centerX;
  result.detectedPosition.y = visionResult.centerY;
  result.detectedPosition.z = result.machinePosition.z; // Will be overridden if useRobotZ is false
  result.confidence = visionResult.confidence;
  result.isValid = true;

  m_logger->LogInfo("ModuleAlignment: Detection successful at " + nodeName +
    " - Center: (" + std::to_string(visionResult.centerX) +
    ", " + std::to_string(visionResult.centerY) +
    ") Confidence: " + std::to_string(visionResult.confidence));

  return true;
}

bool ModuleAlignment::CalculateCoordinateSystem() {
  if (m_alignmentResult.points.size() != 3) {
    SetError("Need exactly 3 alignment points for coordinate system calculation");
    return false;
  }

  // Hardcoded pixel-to-millimeter conversion factors
  const double PIXEL_TO_MM_X = 0.00248;
  const double PIXEL_TO_MM_Y = 0.00248;

  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: Converting pixel coordinates to real-world coordinates");
    m_logger->LogInfo("ModuleAlignment: Pixel-to-MM factors: X=" + std::to_string(PIXEL_TO_MM_X) +
      ", Y=" + std::to_string(PIXEL_TO_MM_Y));
  }

  // Convert pixel coordinates to real-world coordinates for each point
  std::vector<Vector3D> realWorldPositions(3);

  for (size_t i = 0; i < 3; i++) {
    const auto& point = m_alignmentResult.points[i];

    // Step 1: Convert pixel coordinates to mm
    double detectedX_mm = point.detectedPosition.x * PIXEL_TO_MM_X;
    double detectedY_mm = point.detectedPosition.y * PIXEL_TO_MM_Y;

    if (m_logger) {
      m_logger->LogInfo("ModuleAlignment: Node " + point.nodeName +
        " - Pixel: (" + std::to_string(point.detectedPosition.x) +
        ", " + std::to_string(point.detectedPosition.y) +
        ") → MM: (" + std::to_string(detectedX_mm) +
        ", " + std::to_string(detectedY_mm) + ")");
    }

    // Step 2: Calculate offset from center of image (camera center)
    // Assuming image center is at (640, 512) for 1280x1024 image
    // TODO: Get actual image dimensions from camera
    const double IMAGE_CENTER_X = 640.0;  // pixels
    const double IMAGE_CENTER_Y = 512.0;  // pixels

    double deltaX_pixels = point.detectedPosition.x - IMAGE_CENTER_X;
    double deltaY_pixels = point.detectedPosition.y - IMAGE_CENTER_Y;

    // Convert delta to mm
    double deltaX_mm = deltaX_pixels * PIXEL_TO_MM_X;
    double deltaY_mm = deltaY_pixels * PIXEL_TO_MM_Y;

    if (m_logger) {
      m_logger->LogInfo("ModuleAlignment: Node " + point.nodeName +
        " - Delta from image center: (" + std::to_string(deltaX_mm) +
        ", " + std::to_string(deltaY_mm) + ") mm");
    }

    // Step 3: Calculate target position = robot position + delta
    // Note: Camera coordinate system may need Y-axis flip depending on camera orientation
    double targetX = point.machinePosition.x + deltaX_mm;
    double targetY = point.machinePosition.y - deltaY_mm;  // Flip Y if needed
    double targetZ = point.detectedPosition.z;  // Use detected or robot Z as configured

    realWorldPositions[i] = Vector3D(targetX, targetY, targetZ);

    if (m_logger) {
      m_logger->LogInfo("ModuleAlignment: Node " + point.nodeName +
        " - Robot pos: (" + std::to_string(point.machinePosition.x) +
        ", " + std::to_string(point.machinePosition.y) +
        ", " + std::to_string(point.machinePosition.z) +
        ") → Target: (" + std::to_string(targetX) +
        ", " + std::to_string(targetY) +
        ", " + std::to_string(targetZ) + ")");
    }
  }

  // Step 4: Calculate center of the triangle (center of rectangle/module)
  Vector3D pos1 = realWorldPositions[0];  // Node 1
  Vector3D pos2 = realWorldPositions[1];  // Node 2  
  Vector3D pos3 = realWorldPositions[2];  // Node 3

  // Calculate center position: ctr = (pos1 + pos2 + pos3) / 3
  Vector3D pos4 = pos2 + pos3 - pos1;  // Calculate 4th corner
  Vector3D center = (pos1 + pos4) * 0.5;  // Rectangle center


  m_alignmentResult.centerPosition.x = center.x;
  m_alignmentResult.centerPosition.y = center.y;
  m_alignmentResult.centerPosition.z = center.z;

  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: Triangle center (module center): (" +
      std::to_string(center.x) + ", " + std::to_string(center.y) +
      ", " + std::to_string(center.z) + ")");
  }

  // Step 5: Calculate local module alignment axes
  // X-axis direction: pos1 → pos2 (Node1 to Node2)
  Vector3D xAxisVector = pos2 - pos1;
  m_alignmentResult.xAxisLength = xAxisVector.magnitude();
  Vector3D xAxis = xAxisVector.normalize();
  m_alignmentResult.xAxisDirection = xAxis;

  // Y-axis direction: pos1 → pos3 (Node1 to Node3)  
  Vector3D yAxisVector = pos3 - pos1;
  m_alignmentResult.yAxisLength = yAxisVector.magnitude();

  // Orthogonalize Y-axis (Gram-Schmidt process to ensure 90 degrees)
  // y_orthogonal = y_raw - (y_raw · x_unit) * x_unit
  Vector3D yAxis = (yAxisVector - xAxis * yAxisVector.dot(xAxis)).normalize();
  m_alignmentResult.yAxisDirection = yAxis;

  // Z-axis direction: cross product of X and Y (right-hand rule)
  Vector3D zAxis = xAxis.cross(yAxis).normalize();
  m_alignmentResult.zAxisDirection = zAxis;

  // Step 6: Calculate angle between local X-axis (X') and global X-axis
  // Global X-axis is (1, 0, 0)
  Vector3D globalXAxis(1.0, 0.0, 0.0);
  double cosAngle = xAxis.dot(globalXAxis);
  cosAngle = (std::max)(-1.0, (std::min)(1.0, cosAngle)); // Clamp to [-1, 1]
  double angleToGlobalX = std::acos(cosAngle) * 180.0 / M_PI;

  // Determine sign of angle using cross product
  Vector3D crossProduct = globalXAxis.cross(xAxis);
  if (crossProduct.z < 0) {
    angleToGlobalX = -angleToGlobalX;
  }

  m_alignmentResult.axisAngle = angleToGlobalX;

  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: X-axis length: " + std::to_string(m_alignmentResult.xAxisLength) + " mm");
    m_logger->LogInfo("ModuleAlignment: Y-axis length: " + std::to_string(m_alignmentResult.yAxisLength) + " mm");
    m_logger->LogInfo("ModuleAlignment: Local X-axis direction: (" +
      std::to_string(xAxis.x) + ", " + std::to_string(xAxis.y) + ", " + std::to_string(xAxis.z) + ")");
    m_logger->LogInfo("ModuleAlignment: Local Y-axis direction: (" +
      std::to_string(yAxis.x) + ", " + std::to_string(yAxis.y) + ", " + std::to_string(yAxis.z) + ")");
    m_logger->LogInfo("ModuleAlignment: Angle of local X-axis to global X-axis: " +
      std::to_string(angleToGlobalX) + " degrees");
  }

  // Create transformation matrices
  CalculateTransformationMatrix();
  CalculateInverseTransformationMatrix();

  return true;
}

// =============================================================================
// PRIVATE IMPLEMENTATION METHODS
// =============================================================================

bool ModuleAlignment::InitializeVisionSystem() {
  try {
    // Initialize vision circle detection
    m_visionDetector = std::make_unique<VisionCircleDetection>();

    // Load default parameters
    std::string paramFile = "vision_circle_params.json";
    if (!m_visionDetector->LoadParameters(paramFile)) {
      m_logger->LogWarning("ModuleAlignment: Creating default vision parameters");
      if (VisionCircleDetection::CreateDefaultParameterFile(paramFile)) {
        m_visionDetector->LoadParameters(paramFile);
      }
    }

    // Initialize preset manager
    m_presetManager = std::make_unique<VisionPresetManager>();
    if (!m_presetManager->Initialize()) {
      m_logger->LogWarning("ModuleAlignment: Vision preset manager initialization failed: " +
        m_presetManager->GetLastError());
      // Continue without preset manager
    }

    m_logger->LogInfo("ModuleAlignment: Vision system initialized successfully");
    return true;

  }
  catch (const std::exception& e) {
    SetError("Exception initializing vision system: " + std::string(e.what()));
    return false;
  }
}

bool ModuleAlignment::InitializeDatabase() {
  int result = sqlite3_open(m_dbPath.c_str(), &m_alignmentDB);
  if (result != SQLITE_OK) {
    SetError("Cannot open alignment database: " + std::string(sqlite3_errmsg(m_alignmentDB)));
    if (m_alignmentDB) {
      sqlite3_close(m_alignmentDB);
      m_alignmentDB = nullptr;
    }
    return false;
  }

  if (!CreateAlignmentTables()) {
    CloseDatabase();
    return false;
  }

  m_logger->LogInfo("ModuleAlignment: Database initialized successfully");
  return true;
}

void ModuleAlignment::CloseDatabase() {
  if (m_alignmentDB) {
    sqlite3_close(m_alignmentDB);
    m_alignmentDB = nullptr;
  }
}

bool ModuleAlignment::CreateAlignmentTables() {
  const char* createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS module_alignments (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            alignment_name TEXT NOT NULL UNIQUE,
            node1_name TEXT NOT NULL,
            node2_name TEXT NOT NULL,
            node3_name TEXT NOT NULL,
            
            center_x REAL NOT NULL,
            center_y REAL NOT NULL,
            center_z REAL NOT NULL,
            
            x_axis_x REAL NOT NULL,
            x_axis_y REAL NOT NULL,
            x_axis_z REAL NOT NULL,
            
            y_axis_x REAL NOT NULL,
            y_axis_y REAL NOT NULL,
            y_axis_z REAL NOT NULL,
            
            z_axis_x REAL NOT NULL,
            z_axis_y REAL NOT NULL,
            z_axis_z REAL NOT NULL,
            
            x_axis_length REAL NOT NULL,
            y_axis_length REAL NOT NULL,
            axis_angle REAL NOT NULL,
            
            transformation_matrix TEXT NOT NULL,
            inverse_matrix TEXT NOT NULL,
            
            use_robot_z INTEGER DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
        
        CREATE TABLE IF NOT EXISTS alignment_points (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            alignment_id INTEGER NOT NULL,
            node_name TEXT NOT NULL,
            point_index INTEGER NOT NULL,
            
            machine_x REAL NOT NULL,
            machine_y REAL NOT NULL,
            machine_z REAL NOT NULL,
            
            detected_x REAL NOT NULL,
            detected_y REAL NOT NULL,
            detected_z REAL NOT NULL,
            
            confidence REAL NOT NULL,
            
            FOREIGN KEY (alignment_id) REFERENCES module_alignments (id) ON DELETE CASCADE
        );
    )";

  char* errorMessage = nullptr;
  int result = sqlite3_exec(m_alignmentDB, createTableSQL, nullptr, nullptr, &errorMessage);

  if (result != SQLITE_OK) {
    SetError("Failed to create alignment tables: " +
      std::string(errorMessage ? errorMessage : "Unknown error"));
    if (errorMessage) {
      sqlite3_free(errorMessage);
    }
    return false;
  }

  return true;
}

bool ModuleAlignment::LoadVisionPresetForNode(const std::string& nodeName) {
  if (!m_presetManager) {
    return false; // No preset manager available
  }

  auto it = m_nodeToPresetMap.find(nodeName);
  if (it == m_nodeToPresetMap.end()) {
    return false; // No preset mapped to this node
  }

  int presetId = it->second;

  // Load preset parameters using the correct signature
  nlohmann::json presetParams;
  if (m_presetManager->LoadPreset(presetId, presetParams)) {
    // Convert JSON to VisionCircleDetection::Parameters
    VisionCircleDetection::Parameters visionParams;

    try {
      // Parse ROI parameters
      if (presetParams.contains("roi")) {
        visionParams.roiSize = presetParams["roi"].value("size", visionParams.roiSize);
        visionParams.roiOffsetX = presetParams["roi"].value("offsetX", visionParams.roiOffsetX);
        visionParams.roiOffsetY = presetParams["roi"].value("offsetY", visionParams.roiOffsetY);
      }

      // Parse threshold parameters
      if (presetParams.contains("threshold")) {
        visionParams.thresholdLow = presetParams["threshold"].value("low", visionParams.thresholdLow);
        visionParams.thresholdHigh = presetParams["threshold"].value("high", visionParams.thresholdHigh);
        visionParams.invertImage = presetParams["threshold"].value("invertImage", visionParams.invertImage);
      }

      // Parse filter parameters
      if (presetParams.contains("filter")) {
        visionParams.minArea = presetParams["filter"].value("minArea", visionParams.minArea);
        visionParams.maxArea = presetParams["filter"].value("maxArea", visionParams.maxArea);
        visionParams.minCircularity = presetParams["filter"].value("minCircularity", visionParams.minCircularity);
        visionParams.maxCircularity = presetParams["filter"].value("maxCircularity", visionParams.maxCircularity);
        visionParams.minRadius = presetParams["filter"].value("minRadius", visionParams.minRadius);
        visionParams.maxRadius = presetParams["filter"].value("maxRadius", visionParams.maxRadius);
        visionParams.targetRadius = presetParams["filter"].value("targetRadius", visionParams.targetRadius);
      }

      // Parse advanced parameters
      if (presetParams.contains("advanced")) {
        visionParams.useCompactnessFilter = presetParams["advanced"].value("useCompactnessFilter", visionParams.useCompactnessFilter);
        visionParams.minCompactness = presetParams["advanced"].value("minCompactness", visionParams.minCompactness);
        visionParams.maxCompactness = presetParams["advanced"].value("maxCompactness", visionParams.maxCompactness);
        visionParams.useNoiseReduction = presetParams["advanced"].value("useNoiseReduction", visionParams.useNoiseReduction);
        visionParams.medianKernelSize = presetParams["advanced"].value("medianKernelSize", visionParams.medianKernelSize);
        visionParams.enableFallback = presetParams["advanced"].value("enableFallback", visionParams.enableFallback);
        visionParams.fallbackMinRadius = presetParams["advanced"].value("fallbackMinRadius", visionParams.fallbackMinRadius);
        visionParams.fallbackMaxRadius = presetParams["advanced"].value("fallbackMaxRadius", visionParams.fallbackMaxRadius);
      }

      // Apply the parameters to the vision detector
      m_visionDetector->SetParameters(visionParams);

      m_logger->LogInfo("ModuleAlignment: Loaded vision preset " + std::to_string(presetId) +
        " for node " + nodeName);
      return true;

    }
    catch (const std::exception& e) {
      SetError("Failed to parse vision preset parameters: " + std::string(e.what()));
      return false;
    }
  }

  return false;
}

int ModuleAlignment::GetPresetIdForNode(const std::string& nodeName) {
  auto it = m_nodeToPresetMap.find(nodeName);
  return (it != m_nodeToPresetMap.end()) ? it->second : -1;
}

void ModuleAlignment::LoadNodePresetMappings() {
  m_nodeToPresetMap.clear();

  // Load from vision_presets.db (same database used by UIVisionPanel)
  sqlite3* visionDB = nullptr;
  int result = sqlite3_open("vision_presets.db", &visionDB);

  if (result != SQLITE_OK) {
    m_logger->LogWarning("ModuleAlignment: Cannot open vision presets database for node mappings");
    return;
  }

  const char* selectSQL = R"(
        SELECT node_id, preset_id 
        FROM node_preset_mappings 
        WHERE auto_load = 1;
    )";

  sqlite3_stmt* stmt;
  result = sqlite3_prepare_v2(visionDB, selectSQL, -1, &stmt, nullptr);

  if (result == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      std::string nodeId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
      int presetId = sqlite3_column_int(stmt, 1);
      m_nodeToPresetMap[nodeId] = presetId;
    }
    sqlite3_finalize(stmt);
  }

  sqlite3_close(visionDB);

  m_logger->LogInfo("ModuleAlignment: Loaded " + std::to_string(m_nodeToPresetMap.size()) +
    " node-preset mappings");
}

bool ModuleAlignment::ValidateAlignmentGeometry() {
  if (m_alignmentResult.points.size() != 3) {
    SetError("Invalid number of alignment points");
    return false;
  }

  // Check minimum distances between points
  const double MIN_DISTANCE = 1.0; // mm

  Vector3D pos1(m_alignmentResult.points[0].detectedPosition);
  Vector3D pos2(m_alignmentResult.points[1].detectedPosition);
  Vector3D pos3(m_alignmentResult.points[2].detectedPosition);

  double dist12 = (pos2 - pos1).magnitude();
  double dist13 = (pos3 - pos1).magnitude();
  double dist23 = (pos3 - pos2).magnitude();

  if (dist12 < MIN_DISTANCE || dist13 < MIN_DISTANCE || dist23 < MIN_DISTANCE) {
    SetError("Alignment points are too close together (minimum " +
      std::to_string(MIN_DISTANCE) + "mm required)");
    return false;
  }

  // FIXED: Check angle between X-axis and Y-axis (should be close to 90°)
  // Instead of checking rotation relative to global coordinates
  const double MIN_AXIS_ANGLE = 10.0;  // degrees - minimum angle between X and Y axes
  const double MAX_AXIS_ANGLE = 170.0; // degrees - maximum angle between X and Y axes

  // Calculate angle between local X-axis and Y-axis directions
  double axisAngleBetween = CalculateAngleBetweenVectors(
    m_alignmentResult.xAxisDirection,
    m_alignmentResult.yAxisDirection
  );

  if (axisAngleBetween < MIN_AXIS_ANGLE || axisAngleBetween > MAX_AXIS_ANGLE) {
    SetError("X and Y axes are too close to parallel (angle between axes: " +
      std::to_string(axisAngleBetween) + "°, need " +
      std::to_string(MIN_AXIS_ANGLE) + "° to " + std::to_string(MAX_AXIS_ANGLE) + "°)");
    return false;
  }

  // Log the corrected validation
  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: Angle between X and Y axes: " +
      std::to_string(axisAngleBetween) + "° (valid)");
    m_logger->LogInfo("ModuleAlignment: Rotation relative to global X-axis: " +
      std::to_string(m_alignmentResult.axisAngle) + "° (informational)");
  }

  // Check detection confidence
  const double MIN_CONFIDENCE = 0.3;
  for (const auto& point : m_alignmentResult.points) {
    if (point.confidence < MIN_CONFIDENCE) {
      SetError("Low detection confidence at node " + point.nodeName +
        " (" + std::to_string(point.confidence) + ", minimum " +
        std::to_string(MIN_CONFIDENCE) + " required)");
      return false;
    }
  }

  return true;
}

void ModuleAlignment::CalculateTransformationMatrix() {
  // Create transformation matrix from alignment coordinate system to machine coordinates
  // T = [Rx Ry Rz T]
  //     [0  0  0  1]
  // Where Rx, Ry, Rz are the axis vectors and T is the translation

  const auto& xAxis = m_alignmentResult.xAxisDirection;
  const auto& yAxis = m_alignmentResult.yAxisDirection;
  const auto& zAxis = m_alignmentResult.zAxisDirection;
  const auto& center = m_alignmentResult.centerPosition;

  // Initialize to identity
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      m_transformationMatrix[i][j] = 0.0;
    }
  }

  // Set rotation matrix (axis vectors as columns)
  m_transformationMatrix[0][0] = xAxis.x;  m_transformationMatrix[0][1] = yAxis.x;  m_transformationMatrix[0][2] = zAxis.x;
  m_transformationMatrix[1][0] = xAxis.y;  m_transformationMatrix[1][1] = yAxis.y;  m_transformationMatrix[1][2] = zAxis.y;
  m_transformationMatrix[2][0] = xAxis.z;  m_transformationMatrix[2][1] = yAxis.z;  m_transformationMatrix[2][2] = zAxis.z;

  // Set translation (center position)
  m_transformationMatrix[0][3] = center.x;
  m_transformationMatrix[1][3] = center.y;
  m_transformationMatrix[2][3] = center.z;

  // Set homogeneous coordinate
  m_transformationMatrix[3][3] = 1.0;
}

void ModuleAlignment::CalculateInverseTransformationMatrix() {
  // For our transformation matrix, the inverse is:
  // T^-1 = [R^T  -R^T*t]
  //        [0    1     ]
  // Where R^T is the transpose of the rotation matrix and t is the translation

  // Initialize to identity
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      m_inverseTransformationMatrix[i][j] = 0.0;
    }
  }

  // Transpose of rotation matrix
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      m_inverseTransformationMatrix[i][j] = m_transformationMatrix[j][i];
    }
  }

  // Calculate -R^T * t
  const auto& center = m_alignmentResult.centerPosition;
  m_inverseTransformationMatrix[0][3] = -(m_inverseTransformationMatrix[0][0] * center.x +
    m_inverseTransformationMatrix[0][1] * center.y +
    m_inverseTransformationMatrix[0][2] * center.z);
  m_inverseTransformationMatrix[1][3] = -(m_inverseTransformationMatrix[1][0] * center.x +
    m_inverseTransformationMatrix[1][1] * center.y +
    m_inverseTransformationMatrix[1][2] * center.z);
  m_inverseTransformationMatrix[2][3] = -(m_inverseTransformationMatrix[2][0] * center.x +
    m_inverseTransformationMatrix[2][1] * center.y +
    m_inverseTransformationMatrix[2][2] * center.z);

  // Set homogeneous coordinate
  m_inverseTransformationMatrix[3][3] = 1.0;
}

double ModuleAlignment::CalculateAngleBetweenVectors(const Vector3D& v1, const Vector3D& v2) const {
  double dot = v1.dot(v2);
  double mag1 = v1.magnitude();
  double mag2 = v2.magnitude();

  if (mag1 < 1e-10 || mag2 < 1e-10) {
    return 0.0;
  }

  double cosAngle = dot / (mag1 * mag2);
  cosAngle = (std::max)(-1.0, (std::min)(1.0, cosAngle)); // Clamp to [-1, 1]

  return std::acos(cosAngle) * 180.0 / M_PI;
}

// =============================================================================
// DATA PERSISTENCE
// =============================================================================

bool ModuleAlignment::SaveAlignmentData(const std::string& alignmentName) {
  if (!m_alignmentDB) {
    SetError("Database not initialized");
    return false;
  }

  if (!m_hasValidAlignment) {
    SetError("No valid alignment data to save");
    return false;
  }

  if (alignmentName.empty()) {
    SetError("Alignment name cannot be empty");
    return false;
  }

  // Serialize transformation matrices to JSON
  std::ostringstream transformJson, inverseJson;
  transformJson << "[";
  inverseJson << "[";

  for (int i = 0; i < 4; i++) {
    if (i > 0) {
      transformJson << ",";
      inverseJson << ",";
    }
    transformJson << "[";
    inverseJson << "[";
    for (int j = 0; j < 4; j++) {
      if (j > 0) {
        transformJson << ",";
        inverseJson << ",";
      }
      transformJson << std::fixed << std::setprecision(10) << m_transformationMatrix[i][j];
      inverseJson << std::fixed << std::setprecision(10) << m_inverseTransformationMatrix[i][j];
    }
    transformJson << "]";
    inverseJson << "]";
  }
  transformJson << "]";
  inverseJson << "]";

  // Begin transaction
  sqlite3_exec(m_alignmentDB, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

  try {
    // Insert main alignment record
    const char* insertAlignmentSQL = R"(
            INSERT OR REPLACE INTO module_alignments 
            (alignment_name, node1_name, node2_name, node3_name,
             center_x, center_y, center_z,
             x_axis_x, x_axis_y, x_axis_z,
             y_axis_x, y_axis_y, y_axis_z,
             z_axis_x, z_axis_y, z_axis_z,
             x_axis_length, y_axis_length, axis_angle,
             transformation_matrix, inverse_matrix,
             use_robot_z, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP);
        )";

    sqlite3_stmt* stmt;
    int result = sqlite3_prepare_v2(m_alignmentDB, insertAlignmentSQL, -1, &stmt, nullptr);

    if (result != SQLITE_OK) {
      throw std::runtime_error("Failed to prepare alignment insert statement");
    }

    // Bind values
    int idx = 1;
    sqlite3_bind_text(stmt, idx++, alignmentName.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, idx++, m_alignmentResult.points[0].nodeName.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, idx++, m_alignmentResult.points[1].nodeName.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, idx++, m_alignmentResult.points[2].nodeName.c_str(), -1, SQLITE_STATIC);

    sqlite3_bind_double(stmt, idx++, m_alignmentResult.centerPosition.x);
    sqlite3_bind_double(stmt, idx++, m_alignmentResult.centerPosition.y);
    sqlite3_bind_double(stmt, idx++, m_alignmentResult.centerPosition.z);

    sqlite3_bind_double(stmt, idx++, m_alignmentResult.xAxisDirection.x);
    sqlite3_bind_double(stmt, idx++, m_alignmentResult.xAxisDirection.y);
    sqlite3_bind_double(stmt, idx++, m_alignmentResult.xAxisDirection.z);

    sqlite3_bind_double(stmt, idx++, m_alignmentResult.yAxisDirection.x);
    sqlite3_bind_double(stmt, idx++, m_alignmentResult.yAxisDirection.y);
    sqlite3_bind_double(stmt, idx++, m_alignmentResult.yAxisDirection.z);

    sqlite3_bind_double(stmt, idx++, m_alignmentResult.zAxisDirection.x);
    sqlite3_bind_double(stmt, idx++, m_alignmentResult.zAxisDirection.y);
    sqlite3_bind_double(stmt, idx++, m_alignmentResult.zAxisDirection.z);

    sqlite3_bind_double(stmt, idx++, m_alignmentResult.xAxisLength);
    sqlite3_bind_double(stmt, idx++, m_alignmentResult.yAxisLength);
    sqlite3_bind_double(stmt, idx++, m_alignmentResult.axisAngle);

    std::string transformStr = transformJson.str();
    std::string inverseStr = inverseJson.str();
    sqlite3_bind_text(stmt, idx++, transformStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, inverseStr.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_bind_int(stmt, idx++, 0); // use_robot_z placeholder

    result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
      throw std::runtime_error("Failed to insert alignment record");
    }

    // Get the alignment ID
    int alignmentId = static_cast<int>(sqlite3_last_insert_rowid(m_alignmentDB));

    // Delete old points for this alignment
    const char* deletePointsSQL = "DELETE FROM alignment_points WHERE alignment_id = ?;";
    result = sqlite3_prepare_v2(m_alignmentDB, deletePointsSQL, -1, &stmt, nullptr);
    if (result == SQLITE_OK) {
      sqlite3_bind_int(stmt, 1, alignmentId);
      sqlite3_step(stmt);
      sqlite3_finalize(stmt);
    }

    // Insert alignment points
    const char* insertPointSQL = R"(
            INSERT INTO alignment_points 
            (alignment_id, node_name, point_index, 
             machine_x, machine_y, machine_z,
             detected_x, detected_y, detected_z, confidence)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
        )";

    for (size_t i = 0; i < m_alignmentResult.points.size(); i++) {
      const auto& point = m_alignmentResult.points[i];

      result = sqlite3_prepare_v2(m_alignmentDB, insertPointSQL, -1, &stmt, nullptr);
      if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare point insert statement");
      }

      sqlite3_bind_int(stmt, 1, alignmentId);
      sqlite3_bind_text(stmt, 2, point.nodeName.c_str(), -1, SQLITE_STATIC);
      sqlite3_bind_int(stmt, 3, static_cast<int>(i));

      sqlite3_bind_double(stmt, 4, point.machinePosition.x);
      sqlite3_bind_double(stmt, 5, point.machinePosition.y);
      sqlite3_bind_double(stmt, 6, point.machinePosition.z);

      sqlite3_bind_double(stmt, 7, point.detectedPosition.x);
      sqlite3_bind_double(stmt, 8, point.detectedPosition.y);
      sqlite3_bind_double(stmt, 9, point.detectedPosition.z);

      sqlite3_bind_double(stmt, 10, point.confidence);

      result = sqlite3_step(stmt);
      sqlite3_finalize(stmt);

      if (result != SQLITE_DONE) {
        throw std::runtime_error("Failed to insert point " + std::to_string(i));
      }
    }

    // Commit transaction
    sqlite3_exec(m_alignmentDB, "COMMIT;", nullptr, nullptr, nullptr);

    m_alignmentResult.alignmentName = alignmentName;
    m_logger->LogInfo("ModuleAlignment: Successfully saved alignment '" + alignmentName + "'");

    return true;

  }
  catch (const std::exception& e) {
    sqlite3_exec(m_alignmentDB, "ROLLBACK;", nullptr, nullptr, nullptr);
    SetError("Failed to save alignment data: " + std::string(e.what()));
    return false;
  }
}

bool ModuleAlignment::LoadAlignmentData(const std::string& alignmentName) {
  if (!m_alignmentDB) {
    SetError("Database not initialized");
    return false;
  }

  if (alignmentName.empty()) {
    SetError("Alignment name cannot be empty");
    return false;
  }

  // Load main alignment record
  const char* selectAlignmentSQL = R"(
        SELECT node1_name, node2_name, node3_name,
               center_x, center_y, center_z,
               x_axis_x, x_axis_y, x_axis_z,
               y_axis_x, y_axis_y, y_axis_z,
               z_axis_x, z_axis_y, z_axis_z,
               x_axis_length, y_axis_length, axis_angle,
               transformation_matrix, inverse_matrix,
               created_at
        FROM module_alignments 
        WHERE alignment_name = ?;
    )";

  sqlite3_stmt* stmt;
  int result = sqlite3_prepare_v2(m_alignmentDB, selectAlignmentSQL, -1, &stmt, nullptr);

  if (result != SQLITE_OK) {
    SetError("Failed to prepare alignment select statement");
    return false;
  }

  sqlite3_bind_text(stmt, 1, alignmentName.c_str(), -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    SetError("Alignment '" + alignmentName + "' not found");
    return false;
  }

  // Reset alignment result
  m_alignmentResult = AlignmentResult();

  // Load alignment data
  int idx = 0;
  m_alignmentResult.points.resize(3);
  m_alignmentResult.points[0].nodeName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx++));
  m_alignmentResult.points[1].nodeName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx++));
  m_alignmentResult.points[2].nodeName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx++));

  m_alignmentResult.centerPosition.x = sqlite3_column_double(stmt, idx++);
  m_alignmentResult.centerPosition.y = sqlite3_column_double(stmt, idx++);
  m_alignmentResult.centerPosition.z = sqlite3_column_double(stmt, idx++);

  m_alignmentResult.xAxisDirection.x = sqlite3_column_double(stmt, idx++);
  m_alignmentResult.xAxisDirection.y = sqlite3_column_double(stmt, idx++);
  m_alignmentResult.xAxisDirection.z = sqlite3_column_double(stmt, idx++);

  m_alignmentResult.yAxisDirection.x = sqlite3_column_double(stmt, idx++);
  m_alignmentResult.yAxisDirection.y = sqlite3_column_double(stmt, idx++);
  m_alignmentResult.yAxisDirection.z = sqlite3_column_double(stmt, idx++);

  m_alignmentResult.zAxisDirection.x = sqlite3_column_double(stmt, idx++);
  m_alignmentResult.zAxisDirection.y = sqlite3_column_double(stmt, idx++);
  m_alignmentResult.zAxisDirection.z = sqlite3_column_double(stmt, idx++);

  m_alignmentResult.xAxisLength = sqlite3_column_double(stmt, idx++);
  m_alignmentResult.yAxisLength = sqlite3_column_double(stmt, idx++);
  m_alignmentResult.axisAngle = sqlite3_column_double(stmt, idx++);

  // Parse transformation matrices (simplified - would need proper JSON parsing)
  // For now, we'll recalculate them

  const char* timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx + 2));
  m_alignmentResult.timestamp = timestamp ? timestamp : "";

  sqlite3_finalize(stmt);

  // TODO: Load alignment points from alignment_points table
  // For now, we'll mark the points as loaded but without detailed data
  for (auto& point : m_alignmentResult.points) {
    point.isValid = true;
  }

  // Recalculate transformation matrices
  CalculateTransformationMatrix();
  CalculateInverseTransformationMatrix();

  m_alignmentResult.success = true;
  m_alignmentResult.alignmentName = alignmentName;
  m_hasValidAlignment = true;

  m_logger->LogInfo("ModuleAlignment: Successfully loaded alignment '" + alignmentName + "'");
  return true;
}

std::vector<std::string> ModuleAlignment::GetSavedAlignments() const {
  std::vector<std::string> alignments;

  if (!m_alignmentDB) {
    return alignments;
  }

  const char* selectSQL = "SELECT alignment_name FROM module_alignments ORDER BY updated_at DESC;";

  sqlite3_stmt* stmt;
  int result = sqlite3_prepare_v2(m_alignmentDB, selectSQL, -1, &stmt, nullptr);

  if (result == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
      if (name) {
        alignments.push_back(name);
      }
    }
    sqlite3_finalize(stmt);
  }

  return alignments;
}

bool ModuleAlignment::DeleteAlignmentData(const std::string& alignmentName) {
  if (!m_alignmentDB) {
    SetError("Database not initialized");
    return false;
  }

  const char* deleteSQL = "DELETE FROM module_alignments WHERE alignment_name = ?;";

  sqlite3_stmt* stmt;
  int result = sqlite3_prepare_v2(m_alignmentDB, deleteSQL, -1, &stmt, nullptr);

  if (result != SQLITE_OK) {
    SetError("Failed to prepare delete statement");
    return false;
  }

  sqlite3_bind_text(stmt, 1, alignmentName.c_str(), -1, SQLITE_STATIC);
  result = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (result == SQLITE_DONE) {
    m_logger->LogInfo("ModuleAlignment: Deleted alignment '" + alignmentName + "'");
    return true;
  }
  else {
    SetError("Failed to delete alignment '" + alignmentName + "'");
    return false;
  }
}

// =============================================================================
// COORDINATE TRANSFORMATION
// =============================================================================

bool ModuleAlignment::TransformMachineToAlignment(const PositionStruct& machinePos, PositionStruct& alignmentPos) const {
  if (!m_hasValidAlignment) {
    return false;
  }

  // Apply inverse transformation: alignment = T^-1 * machine
  double machine[4] = { machinePos.x, machinePos.y, machinePos.z, 1.0 };
  double alignment[4] = { 0.0, 0.0, 0.0, 0.0 };

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      alignment[i] += m_inverseTransformationMatrix[i][j] * machine[j];
    }
  }

  alignmentPos.x = alignment[0];
  alignmentPos.y = alignment[1];
  alignmentPos.z = alignment[2];
  alignmentPos.u = machinePos.u; // Pass through rotational axes
  alignmentPos.v = machinePos.v;
  alignmentPos.w = machinePos.w;

  return true;
}

bool ModuleAlignment::TransformAlignmentToMachine(const PositionStruct& alignmentPos, PositionStruct& machinePos) const {
  if (!m_hasValidAlignment) {
    return false;
  }

  // Apply transformation: machine = T * alignment
  double alignment[4] = { alignmentPos.x, alignmentPos.y, alignmentPos.z, 1.0 };
  double machine[4] = { 0.0, 0.0, 0.0, 0.0 };

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      machine[i] += m_transformationMatrix[i][j] * alignment[j];
    }
  }

  machinePos.x = machine[0];
  machinePos.y = machine[1];
  machinePos.z = machine[2];
  machinePos.u = alignmentPos.u; // Pass through rotational axes
  machinePos.v = alignmentPos.v;
  machinePos.w = alignmentPos.w;

  return true;
}

// =============================================================================
// UTILITY METHODS
// =============================================================================

void ModuleAlignment::SetError(const std::string& error) {
  m_lastError = error;
  if (m_logger) {
    m_logger->LogError("ModuleAlignment: " + error);
  }
  else {
    std::cerr << "ModuleAlignment Error: " << error << std::endl;
  }
}

void ModuleAlignment::ClearError() {
  m_lastError.clear();
}

std::string ModuleAlignment::GetCurrentTimestamp() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

	std::tm timeinfo;
	localtime_s(&timeinfo, &time_t);


  std::ostringstream oss;
  oss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
  return oss.str();
}

bool ModuleAlignment::IsValidNodeName(const std::string& nodeName) const {
  // Basic validation - could be enhanced to check against actual node configuration
  return !nodeName.empty() && nodeName.length() < 100;
}

// =============================================================================
// CAMERA INTEGRATION HELPERS
// =============================================================================

bool ModuleAlignment::CaptureImageFromCamera(const std::string& nodeName, VisionCircleDetection::Result& result) {
  if (!m_cameraManager) {
    SetError("CameraManager not set - use SetCameraManager() or enable mock detection");
    return false;
  }

  try {
    // Get available cameras (same pattern as UIVisionPanel)
    auto cameraIds = m_cameraManager->GetCameraIds();
    if (cameraIds.empty()) {
      SetError("No cameras available from CameraManager");
      return false;
    }

    // Use first available camera
    std::string cameraId = cameraIds[0];
    if (m_logger) {
      m_logger->LogInfo("ModuleAlignment: Using camera ID: " + cameraId);
    }

    // Get camera hardware (same pattern as UIVisionPanel)
    ICameraHardware* camera = m_cameraManager->GetCameraHardware(cameraId);
    if (!camera || !camera->IsConnected()) {
      SetError("Camera not connected: " + cameraId);
      return false;
    }

    // Capture frame (same pattern as UIVisionPanel)
    CameraFrameData frameData;
    if (!camera->CaptureFrame(frameData)) {
      SetError("Failed to capture frame from camera");
      return false;
    }

    // Validate frame data (same pattern as UIVisionPanel)
    if (!frameData.IsValid() || frameData.imageData.empty()) {
      SetError("Invalid frame data from camera");
      return false;
    }

    if (m_logger) {
      m_logger->LogInfo("ModuleAlignment: Captured frame " + std::to_string(frameData.width) +
        "x" + std::to_string(frameData.height) +
        " (" + std::to_string(frameData.channels) + " channels)");
    }

    // Perform vision detection on captured frame
    result = m_visionDetector->DetectFromBuffer(
      frameData.imageData.data(),
      frameData.width,
      frameData.height,
      frameData.channels
    );

    if (m_logger) {
      if (result.found) {
        m_logger->LogInfo("ModuleAlignment: Real camera detection successful at " + nodeName +
          " - Center: (" + std::to_string(result.centerX) +
          ", " + std::to_string(result.centerY) +
          ") Confidence: " + std::to_string(result.confidence));
      }
      else {
        m_logger->LogWarning("ModuleAlignment: Real camera detection failed at " + nodeName +
          ": " + result.errorMessage);
      }
    }

    return result.found;

  }
  catch (const std::exception& e) {
    SetError("Exception during camera capture: " + std::string(e.what()));

    // Fall back to mock detection if camera fails
    if (m_logger) {
      m_logger->LogWarning("ModuleAlignment: Camera exception, falling back to mock detection");
    }
    return PerformMockDetection(nodeName, result);
  }
}

bool ModuleAlignment::PerformMockDetection(const std::string& nodeName, VisionCircleDetection::Result& result) {
  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: Using mock detection for node: " + nodeName);
  }

  // Generate mock detection results for testing
  result.found = true;
  result.confidence = 0.95;
  result.errorMessage = "";

  // Generate different mock positions for each node to create a valid triangle
  if (nodeName.find("1") != std::string::npos || nodeName.find("node1") != std::string::npos) {
    result.centerX = 100.0;  // Node 1 at (100, 100)
    result.centerY = 100.0;
  }
  else if (nodeName.find("2") != std::string::npos || nodeName.find("node2") != std::string::npos) {
    result.centerX = 200.0;  // Node 2 at (200, 100) - X-axis direction
    result.centerY = 100.0;
  }
  else if (nodeName.find("3") != std::string::npos || nodeName.find("node3") != std::string::npos) {
    result.centerX = 100.0;  // Node 3 at (100, 200) - Y-axis direction
    result.centerY = 200.0;
  }
  else {
    // Default position for unknown nodes
    result.centerX = 150.0 + (nodeName.length() % 50);  // Add some variation
    result.centerY = 150.0 + (nodeName.length() % 30);
  }

  result.radius = 25.0;
  result.circularity = 0.95;
  result.area = 1963.5; // π * r²
  result.numCandidates = 1;

  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: Mock detection result - Center: (" +
      std::to_string(result.centerX) + ", " + std::to_string(result.centerY) + ")");
  }

  return true;
}

// Add this method to ModuleAlignment.cpp

bool ModuleAlignment::MoveToLocalCoordinate(const PositionStruct& localPosition,
  const std::string& deviceName,
  bool waitForCompletion) {
  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: Moving to local coordinate (" +
      std::to_string(localPosition.x) + ", " +
      std::to_string(localPosition.y) + ", " +
      std::to_string(localPosition.z) + ")");
  }

  // Check if we have a valid alignment
  if (!m_hasValidAlignment) {
    SetError("No valid alignment available - perform alignment first");
    return false;
  }

  // Check if MachineOperations is available
  if (!m_machineOperations) {
    SetError("MachineOperations not set - call SetMachineOperations() first");
    return false;
  }

  // Transform local coordinates to machine coordinates
  PositionStruct machinePosition;
  if (!TransformAlignmentToMachine(localPosition, machinePosition)) {
    SetError("Failed to transform local coordinates to machine coordinates");
    return false;
  }

  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: Transformed local (" +
      std::to_string(localPosition.x) + ", " +
      std::to_string(localPosition.y) + ", " +
      std::to_string(localPosition.z) + ") to machine (" +
      std::to_string(machinePosition.x) + ", " +
      std::to_string(machinePosition.y) + ", " +
      std::to_string(machinePosition.z) + ")");
  }

  // Move the device to the calculated machine position
  bool moveSuccess = m_machineOperations->MoveDeviceToPosition(deviceName, machinePosition, waitForCompletion);

  if (!moveSuccess) {
    SetError("Failed to move device '" + deviceName + "' to calculated machine position");
    return false;
  }

  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: Successfully moved " + deviceName +
      " to local coordinate (" + std::to_string(localPosition.x) + ", " +
      std::to_string(localPosition.y) + ", " + std::to_string(localPosition.z) + ")");
  }

  return true;
}

bool ModuleAlignment::MoveToLocalCoordinate(double x, double y, double z,
  const std::string& deviceName,
  bool waitForCompletion) {
  PositionStruct localPos = { x, y, z, 0, 0, 0 };
  return MoveToLocalCoordinate(localPos, deviceName, waitForCompletion);
}

bool ModuleAlignment::GetCurrentLocalPosition(const std::string& deviceName,
  PositionStruct& localPosition) {
  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: Getting current local position for " + deviceName);
  }

  // Check if we have a valid alignment
  if (!m_hasValidAlignment) {
    SetError("No valid alignment available - perform alignment first");
    return false;
  }

  // Check if MachineOperations is available
  if (!m_machineOperations) {
    SetError("MachineOperations not set - call SetMachineOperations() first");
    return false;
  }

  // Get current machine position
  PositionStruct machinePosition;
  if (!m_machineOperations->GetDeviceCurrentPosition(deviceName, machinePosition)) {
    SetError("Failed to get current machine position for device: " + deviceName);
    return false;
  }

  // Transform machine coordinates to local coordinates
  if (!TransformMachineToAlignment(machinePosition, localPosition)) {
    SetError("Failed to transform machine coordinates to local coordinates");
    return false;
  }

  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: Current local position for " + deviceName + ": (" +
      std::to_string(localPosition.x) + ", " +
      std::to_string(localPosition.y) + ", " +
      std::to_string(localPosition.z) + ")");
  }

  return true;
}

bool ModuleAlignment::MoveToLocalCoordinateRelative(double deltaX, double deltaY, double deltaZ,
  const std::string& deviceName,
  bool waitForCompletion) {
  if (m_logger) {
    m_logger->LogInfo("ModuleAlignment: Moving relative by local delta (" +
      std::to_string(deltaX) + ", " + std::to_string(deltaY) + ", " + std::to_string(deltaZ) + ")");
  }

  // Get current local position
  PositionStruct currentLocalPos;
  if (!GetCurrentLocalPosition(deviceName, currentLocalPos)) {
    return false; // Error already set
  }

  // Calculate target local position
  PositionStruct targetLocalPos;
  targetLocalPos.x = currentLocalPos.x + deltaX;
  targetLocalPos.y = currentLocalPos.y + deltaY;
  targetLocalPos.z = currentLocalPos.z + deltaZ;
  targetLocalPos.u = currentLocalPos.u; // Keep rotational axes unchanged
  targetLocalPos.v = currentLocalPos.v;
  targetLocalPos.w = currentLocalPos.w;

  // Move to the target position
  return MoveToLocalCoordinate(targetLocalPos, deviceName, waitForCompletion);
}