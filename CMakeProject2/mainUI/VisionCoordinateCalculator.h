// VisionCoordinateCalculator.h - Real-World Coordinate Calculation Utility
#pragma once

#include <string>
#include <optional>

// Forward declarations
class MachineOperations;

/**
 * @brief Utility class for converting vision detection coordinates to real-world robot coordinates
 */
class VisionCoordinateCalculator {
public:
  // Coordinate structure for all coordinate types
  struct CoordinateSet {
    // Original pixel coordinates
    double pixelX = 0.0;
    double pixelY = 0.0;
    double pixelRadius = 0.0;

    // Image information
    int imageWidth = 0;
    int imageHeight = 0;
    double imageCenterX = 0.0;
    double imageCenterY = 0.0;

    // Offset from image center (pixels)
    double offsetPixelX = 0.0;
    double offsetPixelY = 0.0;

    // Offset in millimeters
    double offsetMmX = 0.0;
    double offsetMmY = 0.0;

    // Robot position at time of detection
    double robotX = 0.0;
    double robotY = 0.0;
    double robotZ = 0.0;
    bool hasRobotPosition = false;

    // Target coordinates (robot + offset)
    double targetX = 0.0;
    double targetY = 0.0;
    double targetZ = 0.0;

    // Calibration factors used
    double pixelToMmFactorX = 0.0;
    double pixelToMmFactorY = 0.0;
    bool hasCalibration = false;
  };

  // Calibration data structure
  struct CalibrationData {
    double pixelToMillimeterFactorX = 0.00248;  // Default from your system
    double pixelToMillimeterFactorY = 0.00252;  // Default from your system
    bool isValid = false;
    std::string sourceFile = "";
    std::string lastModified = "";
  };

public:
  VisionCoordinateCalculator();
  ~VisionCoordinateCalculator();

  // Main calculation method
  CoordinateSet CalculateCoordinates(double pixelX, double pixelY, double pixelRadius,
    int imageWidth, int imageHeight) const;

  // Calibration management
  bool LoadCalibration(const std::string& calibrationFilePath = "camera_calibration.json");
  const CalibrationData& GetCalibrationData() const { return m_calibration; }
  bool IsCalibrationValid() const { return m_calibration.isValid; }

  // Robot integration
  void SetMachineOperations(MachineOperations* machineOps) { m_machineOperations = machineOps; }
  bool GetCurrentRobotPosition(double& x, double& y, double& z) const;

  // Utility methods
  static double CalculateDistance2D(double x1, double y1, double x2, double y2);
  static std::string FormatCoordinate(double value, int precision = 3);
  static std::string FormatCoordinateSet(const CoordinateSet& coords);

  // Validation
  bool ValidatePixelCoordinates(double pixelX, double pixelY, int imageWidth, int imageHeight) const;
  std::string GetValidationMessage() const { return m_lastValidationMessage; }

private:
  CalibrationData m_calibration;
  MachineOperations* m_machineOperations = nullptr;
  mutable std::string m_lastValidationMessage = "";

  // Internal helper methods
  bool ParseCalibrationFile(const std::string& filePath);
  void CalculateOffsets(CoordinateSet& coords) const;
  void ConvertToMillimeters(CoordinateSet& coords) const;
  void CalculateTargetPosition(CoordinateSet& coords) const;
};