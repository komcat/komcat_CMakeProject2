// ModuleAlignment.h - Module alignment system for figuring out origin, axes, and center location
#pragma once

#include "include/machine_operations.h"
#include "include/halcon/VisionCircleDetection.h"
#include "VisionPresetManager.h"
#include "include/camera/CameraManager.h"
#include "include/camera/ICameraHardware.h"
#include "include/camera/CameraFrameData.h"
#include <sqlite3.h>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <map>
#include <nlohmann/json.hpp>

/**
 * @brief ModuleAlignment class uses composition with MachineOperations
 *
 * This class performs 3-point alignment calibration to determine:
 * - Origin position (center of 3 points)
 * - X-axis direction (node1 → node2)
 * - Y-axis direction (node1 → node3)
 * - Coordinate transformation matrix for machine ↔ vision conversion
 *
 * Process:
 * 1. Move gantry-main to node1 → detect feature → store dpos_node1
 * 2. Move gantry-main to node2 → detect feature → store dpos_node2
 * 3. Move gantry-main to node3 → detect feature → store dpos_node3
 * 4. Calculate center (ctr) = (dpos1 + dpos2 + dpos3) / 3
 * 5. Setup coordinate system with X-axis (node1→node2), Y-axis (node1→node3)
 * 6. Create and save transformation matrix for reuse
 */
class ModuleAlignment {
public:
  // =============================================================================
  // DATA STRUCTURES
  // =============================================================================

  /**
   * @brief Alignment point containing machine and detected vision coordinates
   */
  struct AlignmentPoint {
    std::string nodeName;              // Node identifier
    PositionStruct machinePosition;    // Machine coordinates when at node
    PositionStruct detectedPosition;   // Vision-detected feature coordinates
    double confidence = 0.0;           // Detection confidence (0.0-1.0)
    bool isValid = false;              // Whether detection was successful

    AlignmentPoint() = default;
    AlignmentPoint(const std::string& name) : nodeName(name) {}
  };

  /**
   * @brief 3D Vector for coordinate calculations
   */
  struct Vector3D {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Vector3D() = default;
    Vector3D(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
    Vector3D(const PositionStruct& pos) : x(pos.x), y(pos.y), z(pos.z) {}

    Vector3D operator+(const Vector3D& other) const {
      return Vector3D(x + other.x, y + other.y, z + other.z);
    }

    Vector3D operator-(const Vector3D& other) const {
      return Vector3D(x - other.x, y - other.y, z - other.z);
    }

    Vector3D operator*(double scalar) const {
      return Vector3D(x * scalar, y * scalar, z * scalar);
    }

    double magnitude() const {
      return sqrt(x * x + y * y + z * z);
    }

    Vector3D normalize() const {
      double mag = magnitude();
      if (mag > 1e-10) {
        return Vector3D(x / mag, y / mag, z / mag);
      }
      return Vector3D(1.0, 0.0, 0.0); // Default to X-axis if zero vector
    }

    // Cross product
    Vector3D cross(const Vector3D& other) const {
      return Vector3D(
        y * other.z - z * other.y,
        z * other.x - x * other.z,
        x * other.y - y * other.x
      );
    }

    // Dot product
    double dot(const Vector3D& other) const {
      return x * other.x + y * other.y + z * other.z;
    }
  };

  /**
   * @brief Alignment result summary
   */
  struct AlignmentResult {
    bool success = false;
    std::string errorMessage;

    PositionStruct centerPosition;     // Calculated center (ctr)
    Vector3D xAxisDirection;           // X-axis unit vector
    Vector3D yAxisDirection;           // Y-axis unit vector  
    Vector3D zAxisDirection;           // Z-axis unit vector (cross product)

    double xAxisLength = 0.0;          // Distance node1 → node2
    double yAxisLength = 0.0;          // Distance node1 → node3
    double axisAngle = 0.0;            // Angle between X and Y axes (degrees)

    std::vector<AlignmentPoint> points; // All detected points
    std::string alignmentName;         // Saved alignment identifier
    std::string timestamp;             // When alignment was performed
  };

public:
  // =============================================================================
  // CONSTRUCTION & LIFECYCLE
  // =============================================================================

  ModuleAlignment();
  virtual ~ModuleAlignment();

  // Disable copy/move to avoid issues with references
  ModuleAlignment(const ModuleAlignment&) = delete;
  ModuleAlignment& operator=(const ModuleAlignment&) = delete;
  ModuleAlignment(ModuleAlignment&&) = delete;
  ModuleAlignment& operator=(ModuleAlignment&&) = delete;

  // =============================================================================
  // SYSTEM INTEGRATION
  // =============================================================================

  /**
   * @brief Set machine operations reference for movement and control
   * @param machineOps Pointer to MachineOperations instance
   */
  void SetMachineOperations(MachineOperations* machineOps) { m_machineOperations = machineOps; }

  /**
   * @brief Set camera manager for image capture
   * @param cameraManager Pointer to CameraManager instance
   */
  void SetCameraManager(CameraManager* cameraManager) { m_cameraManager = cameraManager; }

  /**
   * @brief Set logger instance (optional - will use default if not set)
   * @param logger Pointer to Logger instance
   */
  void SetLogger(Logger* logger) { m_logger = logger; }

  /**
   * @brief Check if all required dependencies are set
   * @return True if MachineOperations is set (CameraManager is optional)
   */
  bool IsReadyForAlignment() const { return m_machineOperations != nullptr; }

  // =============================================================================
  // MAIN ALIGNMENT INTERFACE
  // =============================================================================

  /**
   * @brief Perform 3-point module alignment calibration
   * @param node1Name First calibration node (origin reference)
   * @param node2Name Second calibration node (X-axis direction)
   * @param node3Name Third calibration node (Y-axis direction)
   * @param useRobotZ If true, use robot Z coordinate; if false, use detected Z
   * @return Alignment result with success status and calculated data
   */
  AlignmentResult PerformThreePointAlignment(
    const std::string& node1Name,
    const std::string& node2Name,
    const std::string& node3Name,
    bool useRobotZ = false
  );

  /**
   * @brief Move to specific node and perform vision detection
   * @param nodeName Target node identifier
   * @param result Output alignment point with detected coordinates
   * @param useMockDetection If true, uses mock detection for testing (default: false)
   * @return True if movement and detection successful
   */
  bool MoveToNodeAndDetect(const std::string& nodeName, AlignmentPoint& result, bool useMockDetection = false);

  /**
   * @brief Calculate coordinate system from 3 alignment points
   * @return True if coordinate system calculated successfully
   */
  bool CalculateCoordinateSystem();

  // =============================================================================
  // DATA PERSISTENCE
  // =============================================================================

  /**
   * @brief Save alignment calibration to database for reuse
   * @param alignmentName Unique identifier for this alignment
   * @return True if saved successfully
   */
  bool SaveAlignmentData(const std::string& alignmentName);

  /**
   * @brief Load previously saved alignment calibration
   * @param alignmentName Alignment identifier to load
   * @return True if loaded successfully
   */
  bool LoadAlignmentData(const std::string& alignmentName);

  /**
   * @brief Get list of all saved alignments
   * @return Vector of alignment names
   */
  std::vector<std::string> GetSavedAlignments() const;

  /**
   * @brief Delete saved alignment from database
   * @param alignmentName Alignment to delete
   * @return True if deleted successfully
   */
  bool DeleteAlignmentData(const std::string& alignmentName);

  // =============================================================================
  // COORDINATE TRANSFORMATION
  // =============================================================================

  /**
   * @brief Transform point from machine coordinates to alignment coordinates
   * @param machinePos Machine coordinate position
   * @param alignmentPos Output alignment coordinate position
   * @return True if transformation successful
   */
  bool TransformMachineToAlignment(const PositionStruct& machinePos, PositionStruct& alignmentPos) const;

  /**
   * @brief Transform point from alignment coordinates to machine coordinates
   * @param alignmentPos Alignment coordinate position
   * @param machinePos Output machine coordinate position
   * @return True if transformation successful
   */
  bool TransformAlignmentToMachine(const PositionStruct& alignmentPos, PositionStruct& machinePos) const;

  // =============================================================================
  // STATUS & INFORMATION
  // =============================================================================

  /**
   * @brief Check if valid alignment data is available
   * @return True if alignment has been performed and is valid
   */
  bool HasValidAlignment() const { return m_hasValidAlignment; }

  /**
   * @brief Get current alignment result
   * @return Current alignment result structure
   */
  const AlignmentResult& GetAlignmentResult() const { return m_alignmentResult; }

  /**
   * @brief Get last error message
   * @return Error message string
   */
  const std::string& GetLastError() const { return m_lastError; }

  /**
   * @brief Get alignment points (node1, node2, node3)
   * @return Vector of alignment points
   */
  const std::vector<AlignmentPoint>& GetAlignmentPoints() const { return m_alignmentResult.points; }

  /**
   * @brief Get calculated center position
   * @return Center position coordinates
   */
  const PositionStruct& GetCenterPosition() const { return m_alignmentResult.centerPosition; }



  // =============================================================================
  // LOCAL COORDINATE MOVEMENT
  // =============================================================================

  /**
   * @brief Move device to specified local coordinate position
   * @param localPosition Local coordinate position (relative to alignment center)
   * @param deviceName Name of device to move (e.g., "gantry-main")
   * @param waitForCompletion If true, waits for movement to complete
   * @return True if movement command successful
   */
  bool MoveToLocalCoordinate(const PositionStruct& localPosition,
    const std::string& deviceName = "gantry-main",
    bool waitForCompletion = true);

  /**
   * @brief Move device to specified local coordinate position (convenience overload)
   * @param x Local X coordinate (mm)
   * @param y Local Y coordinate (mm)
   * @param z Local Z coordinate (mm)
   * @param deviceName Name of device to move (e.g., "gantry-main")
   * @param waitForCompletion If true, waits for movement to complete
   * @return True if movement command successful
   */
  bool MoveToLocalCoordinate(double x, double y, double z,
    const std::string& deviceName = "gantry-main",
    bool waitForCompletion = true);

  /**
   * @brief Get current device position in local coordinates
   * @param deviceName Name of device to query
   * @param localPosition Output local coordinate position
   * @return True if position retrieved successfully
   */
  bool GetCurrentLocalPosition(const std::string& deviceName,
    PositionStruct& localPosition);

  /**
   * @brief Move device relative to current position in local coordinates
   * @param deltaX Change in local X coordinate (mm)
   * @param deltaY Change in local Y coordinate (mm)
   * @param deltaZ Change in local Z coordinate (mm)
   * @param deviceName Name of device to move
   * @param waitForCompletion If true, waits for movement to complete
   * @return True if movement command successful
   */
  bool MoveToLocalCoordinateRelative(double deltaX, double deltaY, double deltaZ,
    const std::string& deviceName = "gantry-main",
    bool waitForCompletion = true);

private:
  // =============================================================================
  // MEMBER VARIABLES
  // =============================================================================

  // System references
  MachineOperations* m_machineOperations = nullptr;
  CameraManager* m_cameraManager = nullptr;
  Logger* m_logger = nullptr;

  // Vision system
  std::unique_ptr<VisionCircleDetection> m_visionDetector;
  std::unique_ptr<VisionPresetManager> m_presetManager;

  // Alignment state
  AlignmentResult m_alignmentResult;
  bool m_hasValidAlignment = false;
  std::string m_lastError;

  // Coordinate transformation matrix (4x4 homogeneous)
  std::array<std::array<double, 4>, 4> m_transformationMatrix;
  std::array<std::array<double, 4>, 4> m_inverseTransformationMatrix;

  // Database connection
  sqlite3* m_alignmentDB = nullptr;
  std::string m_dbPath = "module_alignment.db";

  // Vision preset integration
  std::map<std::string, int> m_nodeToPresetMap;  // Node → Preset ID mapping

  // =============================================================================
  // PRIVATE METHODS
  // =============================================================================

  // Database operations
  bool InitializeDatabase();
  void CloseDatabase();
  bool CreateAlignmentTables();

  // Vision integration
  bool InitializeVisionSystem();
  bool LoadVisionPresetForNode(const std::string& nodeName);
  int GetPresetIdForNode(const std::string& nodeName);
  void LoadNodePresetMappings();

  // Camera integration helpers
  bool CaptureImageFromCamera(const std::string& nodeName, VisionCircleDetection::Result& result);
  bool PerformMockDetection(const std::string& nodeName, VisionCircleDetection::Result& result);

  // Mathematical calculations
  bool ValidateAlignmentGeometry();
  void CalculateTransformationMatrix();
  void CalculateInverseTransformationMatrix();
  double CalculateAngleBetweenVectors(const Vector3D& v1, const Vector3D& v2) const;

  // Utility methods
  void SetError(const std::string& error);
  void ClearError();
  std::string GetCurrentTimestamp() const;
  bool IsValidNodeName(const std::string& nodeName) const;

  // Matrix operations
  void MultiplyMatrix4x4(const std::array<std::array<double, 4>, 4>& a,
    const std::array<std::array<double, 4>, 4>& b,
    std::array<std::array<double, 4>, 4>& result) const;
  bool InvertMatrix4x4(const std::array<std::array<double, 4>, 4>& matrix,
    std::array<std::array<double, 4>, 4>& inverse) const;
};