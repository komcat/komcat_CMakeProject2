// CameraHardwareFactory.cpp - Complete implementation
#include "CameraHardwareFactory.h"
#include "PylonCameraAdapter.h"
#include "IDSCameraAdapter.h"
#include "include/logger.h"
#include <algorithm>
#include <cctype>

std::unique_ptr<ICameraHardware> CameraHardwareFactory::CreateCamera(
  ICameraHardware::CameraType type,
  const std::string& cameraId,
  const std::string& deviceInfo) {

  Logger* logger = Logger::GetInstance();

  try {
    switch (type) {
    case ICameraHardware::CameraType::PYLON: {
      if (logger) {
        logger->LogInfo("Creating Pylon camera adapter for: " + cameraId);
      }
      auto camera = std::make_unique<PylonCameraAdapter>(cameraId);
      if (camera) {
        logger->LogInfo("Pylon camera adapter created successfully");
      }
      return camera;
    }

    case ICameraHardware::CameraType::IDS: {
      if (logger) {
        logger->LogInfo("Creating IDS camera adapter for: " + cameraId);
      }

      // Extract device ID from deviceInfo if available
      int deviceId = 0;
      if (!deviceInfo.empty()) {
        try {
          deviceId = std::stoi(deviceInfo);
        }
        catch (...) {
          if (logger) {
            logger->LogWarning("Failed to parse device ID from: " + deviceInfo + ", using 0");
          }
          deviceId = 0;
        }
      }

      auto camera = std::make_unique<IDSCameraAdapter>(cameraId, deviceId);
      if (camera) {
        logger->LogInfo("IDS camera adapter created successfully (Device ID: " + std::to_string(deviceId) + ")");
      }
      return camera;
    }

    case ICameraHardware::CameraType::UNKNOWN:
    default: {
      if (logger) {
        logger->LogError("Unknown camera type requested for: " + cameraId);
      }
      return nullptr;
    }
    }
  }
  catch (const std::exception& e) {
    if (logger) {
      logger->LogError("Exception creating camera [" + cameraId + "]: " + std::string(e.what()));
    }
    return nullptr;
  }
}

ICameraHardware::CameraType CameraHardwareFactory::DetectCameraType(const std::string& deviceInfo) {
  if (deviceInfo.empty()) {
    return ICameraHardware::CameraType::UNKNOWN;
  }

  // Convert to lowercase for case-insensitive comparison
  std::string lowerDeviceInfo = deviceInfo;
  std::transform(lowerDeviceInfo.begin(), lowerDeviceInfo.end(), lowerDeviceInfo.begin(), ::tolower);

  // Check for Pylon/Basler indicators
  if (lowerDeviceInfo.find("basler") != std::string::npos ||
    lowerDeviceInfo.find("pylon") != std::string::npos ||
    lowerDeviceInfo.find("ace") != std::string::npos ||
    lowerDeviceInfo.find("dart") != std::string::npos ||
    lowerDeviceInfo.find("pulse") != std::string::npos ||
    lowerDeviceInfo.find("gige") != std::string::npos) {

    Logger* logger = Logger::GetInstance();
    if (logger) {
      logger->LogInfo("Detected Pylon camera from device info: " + deviceInfo);
    }
    return ICameraHardware::CameraType::PYLON;
  }

  // Check for IDS indicators
  if (lowerDeviceInfo.find("ids") != std::string::npos ||
    lowerDeviceInfo.find("ueye") != std::string::npos ||
    lowerDeviceInfo.find("imaging development systems") != std::string::npos ||
    lowerDeviceInfo.find("ui-") != std::string::npos ||
    lowerDeviceInfo.find("le") != std::string::npos ||
    lowerDeviceInfo.find("cp") != std::string::npos ||
    lowerDeviceInfo.find("ml") != std::string::npos) {

    Logger* logger = Logger::GetInstance();
    if (logger) {
      logger->LogInfo("Detected IDS camera from device info: " + deviceInfo);
    }
    return ICameraHardware::CameraType::IDS;
  }

  // Default to unknown if no match found
  Logger* logger = Logger::GetInstance();
  if (logger) {
    logger->LogWarning("Could not detect camera type from device info: " + deviceInfo);
  }

  return ICameraHardware::CameraType::UNKNOWN;
}

std::unique_ptr<ICameraHardware> CameraHardwareFactory::CreateCameraAutoDetect(
  const std::string& cameraId,
  const std::string& deviceInfo) {

  ICameraHardware::CameraType detectedType = DetectCameraType(deviceInfo);

  if (detectedType == ICameraHardware::CameraType::UNKNOWN) {
    Logger* logger = Logger::GetInstance();
    if (logger) {
      logger->LogWarning("Auto-detection failed for camera: " + cameraId + ", defaulting to Pylon");
    }
    detectedType = ICameraHardware::CameraType::PYLON; // Default fallback
  }

  return CreateCamera(detectedType, cameraId, deviceInfo);
}

std::string CameraHardwareFactory::GetCameraTypeName(ICameraHardware::CameraType type) {
  switch (type) {
  case ICameraHardware::CameraType::PYLON:
    return "Pylon/Basler";
  case ICameraHardware::CameraType::IDS:
    return "IDS uEye";
  case ICameraHardware::CameraType::UNKNOWN:
  default:
    return "Unknown";
  }
}

bool CameraHardwareFactory::IsCameraTypeSupported(ICameraHardware::CameraType type) {
  switch (type) {
  case ICameraHardware::CameraType::PYLON:
  case ICameraHardware::CameraType::IDS:
    return true;
  case ICameraHardware::CameraType::UNKNOWN:
  default:
    return false;
  }
}