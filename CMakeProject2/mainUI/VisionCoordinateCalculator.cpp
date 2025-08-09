// VisionCoordinateCalculator.cpp - Implementation
#include "VisionCoordinateCalculator.h"
#include "machine_operations.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

VisionCoordinateCalculator::VisionCoordinateCalculator() {
  // Load default calibration on construction
  LoadCalibration();
}

VisionCoordinateCalculator::~VisionCoordinateCalculator() {
  // Cleanup if needed
}

VisionCoordinateCalculator::CoordinateSet VisionCoordinateCalculator::CalculateCoordinates(
  double pixelX, double pixelY, double pixelRadius, int imageWidth, int imageHeight) const {

  CoordinateSet coords;

  // Store input values
  coords.pixelX = pixelX;
  coords.pixelY = pixelY;
  coords.pixelRadius = pixelRadius;
  coords.imageWidth = imageWidth;
  coords.imageHeight = imageHeight;
  coords.imageCenterX = imageWidth / 2.0;
  coords.imageCenterY = imageHeight / 2.0;

  // Validate input
  if (!ValidatePixelCoordinates(pixelX, pixelY, imageWidth, imageHeight)) {
    return coords; // Return with default values
  }

  // Store calibration info
  coords.pixelToMmFactorX = m_calibration.pixelToMillimeterFactorX;
  coords.pixelToMmFactorY = m_calibration.pixelToMillimeterFactorY;
  coords.hasCalibration = m_calibration.isValid;

  // Calculate step by step
  CalculateOffsets(coords);
  ConvertToMillimeters(coords);

  // Get robot position
  coords.hasRobotPosition = GetCurrentRobotPosition(coords.robotX, coords.robotY, coords.robotZ);

  // Calculate target position
  CalculateTargetPosition(coords);

  return coords;
}

bool VisionCoordinateCalculator::LoadCalibration(const std::string& calibrationFilePath) {
  m_calibration = CalibrationData(); // Reset
  m_calibration.sourceFile = calibrationFilePath;

  return ParseCalibrationFile(calibrationFilePath);
}

bool VisionCoordinateCalculator::ParseCalibrationFile(const std::string& filePath) {
  try {
    std::ifstream file(filePath);
    if (!file.is_open()) {
      std::cout << "[VisionCoordinateCalculator] Warning: Could not open " << filePath
        << ", using default calibration values" << std::endl;
      return false;
    }

    std::string line;
    bool foundX = false, foundY = false;

    while (std::getline(file, line)) {
      // Simple JSON parsing for calibration values
      if (line.find("pixelToMillimeterFactorX") != std::string::npos) {
        size_t colonPos = line.find(":");
        if (colonPos != std::string::npos) {
          std::string valueStr = line.substr(colonPos + 1);
          // Remove spaces, commas, and other JSON characters
          valueStr.erase(std::remove_if(valueStr.begin(), valueStr.end(),
            [](char c) {
            return c == ' ' || c == ',' || c == '\r' || c == '\n' || c == '"';
          }), valueStr.end());

          try {
            m_calibration.pixelToMillimeterFactorX = std::stod(valueStr);
            foundX = true;
          }
          catch (const std::exception& e) {
            std::cout << "[VisionCoordinateCalculator] Error parsing X factor: " << e.what() << std::endl;
          }
        }
      }
      else if (line.find("pixelToMillimeterFactorY") != std::string::npos) {
        size_t colonPos = line.find(":");
        if (colonPos != std::string::npos) {
          std::string valueStr = line.substr(colonPos + 1);
          valueStr.erase(std::remove_if(valueStr.begin(), valueStr.end(),
            [](char c) {
            return c == ' ' || c == ',' || c == '\r' || c == '\n' || c == '"';
          }), valueStr.end());

          try {
            m_calibration.pixelToMillimeterFactorY = std::stod(valueStr);
            foundY = true;
          }
          catch (const std::exception& e) {
            std::cout << "[VisionCoordinateCalculator] Error parsing Y factor: " << e.what() << std::endl;
          }
        }
      }
    }

    m_calibration.isValid = foundX && foundY;

    if (m_calibration.isValid) {
      std::cout << "[VisionCoordinateCalculator] Loaded calibration: X="
        << m_calibration.pixelToMillimeterFactorX
        << ", Y=" << m_calibration.pixelToMillimeterFactorY << std::endl;
    }
    else {
      std::cout << "[VisionCoordinateCalculator] Failed to parse calibration file, using defaults" << std::endl;
    }

    return m_calibration.isValid;
  }
  catch (const std::exception& e) {
    std::cout << "[VisionCoordinateCalculator] Exception loading calibration: " << e.what() << std::endl;
    return false;
  }
}

bool VisionCoordinateCalculator::GetCurrentRobotPosition(double& x, double& y, double& z) const {
  if (!m_machineOperations) {
    m_lastValidationMessage = "MachineOperations not available";
    return false;
  }

  try {
    // Use the GetDeviceCurrentPosition method from MachineOperations
    PositionStruct currentPos;
    if (m_machineOperations->GetDeviceCurrentPosition("gantry-main", currentPos)) {
      x = currentPos.x;
      y = currentPos.y;
      z = currentPos.z;
      return true;
    }
    else {
      m_lastValidationMessage = "Failed to get gantry-main position";
      return false;
    }
  }
  catch (const std::exception& e) {
    m_lastValidationMessage = "Exception getting robot position: " + std::string(e.what());
    return false;
  }
}

void VisionCoordinateCalculator::CalculateOffsets(CoordinateSet& coords) const {
  // Calculate pixel offset from image center
  coords.offsetPixelX = coords.pixelX - coords.imageCenterX;
  coords.offsetPixelY = coords.pixelY - coords.imageCenterY;
}

void VisionCoordinateCalculator::ConvertToMillimeters(CoordinateSet& coords) const {
  // Convert pixel offsets to millimeters using calibration factors
  coords.offsetMmX = coords.offsetPixelX * coords.pixelToMmFactorX;
  coords.offsetMmY = coords.offsetPixelY * coords.pixelToMmFactorY;
}

void VisionCoordinateCalculator::CalculateTargetPosition(CoordinateSet& coords) const {
  if (coords.hasRobotPosition) {
    // Target position = robot position + offset
    coords.targetX = coords.robotX + coords.offsetMmX;
    coords.targetY = coords.robotY + coords.offsetMmY;
    coords.targetZ = coords.robotZ; // Z typically doesn't change for 2D detection
  }
  else {
    // If no robot position, target equals offset from origin
    coords.targetX = coords.offsetMmX;
    coords.targetY = coords.offsetMmY;
    coords.targetZ = 0.0;
  }
}

bool VisionCoordinateCalculator::ValidatePixelCoordinates(double pixelX, double pixelY,
  int imageWidth, int imageHeight) const {
  if (imageWidth <= 0 || imageHeight <= 0) {
    m_lastValidationMessage = "Invalid image dimensions";
    return false;
  }

  if (pixelX < 0 || pixelX >= imageWidth) {
    m_lastValidationMessage = "Pixel X coordinate out of image bounds";
    return false;
  }

  if (pixelY < 0 || pixelY >= imageHeight) {
    m_lastValidationMessage = "Pixel Y coordinate out of image bounds";
    return false;
  }

  m_lastValidationMessage = "";
  return true;
}

// Static utility methods
double VisionCoordinateCalculator::CalculateDistance2D(double x1, double y1, double x2, double y2) {
  double dx = x2 - x1;
  double dy = y2 - y1;
  return std::sqrt(dx * dx + dy * dy);
}

std::string VisionCoordinateCalculator::FormatCoordinate(double value, int precision) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(precision) << value;
  return oss.str();
}

std::string VisionCoordinateCalculator::FormatCoordinateSet(const CoordinateSet& coords) {
  std::ostringstream oss;
  oss << "Target: ("
    << FormatCoordinate(coords.targetX, 3) << ", "
    << FormatCoordinate(coords.targetY, 3) << ", "
    << FormatCoordinate(coords.targetZ, 3) << ") mm";
  return oss.str();
}