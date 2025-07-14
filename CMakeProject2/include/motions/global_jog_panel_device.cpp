// global_jog_panel_device.cpp - Device-related functionality
#include "include/motions/global_jog_panel.h"
#include <algorithm>

std::map<std::string, double> GlobalJogPanel::GetCurrentPositions() {
  std::map<std::string, double> positions;

  if (m_selectedDevice.empty()) {
    return positions;
  }

  auto deviceOpt = m_configManager.GetDevice(m_selectedDevice);
  if (!deviceOpt.has_value()) {
    return positions;
  }

  const auto& device = deviceOpt.value().get();

  if (device.TypeController == "PI") {
    PIController* controller = m_piControllerManager.GetController(m_selectedDevice);
    if (controller && controller->IsConnected()) {
      controller->GetPositions(positions);
    }
  }
  else if (device.TypeController == "ACS") {
    ACSController* controller = m_acsControllerManager.GetController(m_selectedDevice);
    if (controller && controller->IsConnected()) {
      controller->GetPositions(positions);
    }
  }
  else {
    // Fallback to port-based detection
    if (device.Port == 50000) {
      PIController* controller = m_piControllerManager.GetController(m_selectedDevice);
      if (controller && controller->IsConnected()) {
        controller->GetPositions(positions);
      }
    }
    else {
      ACSController* controller = m_acsControllerManager.GetController(m_selectedDevice);
      if (controller && controller->IsConnected()) {
        controller->GetPositions(positions);
      }
    }
  }

  return positions;
}

bool GlobalJogPanel::IsDeviceConnected() const {
  if (m_selectedDevice.empty()) {
    return false;
  }

  auto deviceOpt = m_configManager.GetDevice(m_selectedDevice);
  if (!deviceOpt.has_value()) {
    return false;
  }

  const auto& device = deviceOpt.value().get();

  if (device.TypeController == "PI") {
    PIController* controller = m_piControllerManager.GetController(m_selectedDevice);
    return controller && controller->IsConnected();
  }
  else if (device.TypeController == "ACS") {
    ACSController* controller = m_acsControllerManager.GetController(m_selectedDevice);
    return controller && controller->IsConnected();
  }
  else {
    // Fallback to port-based detection
    if (device.Port == 50000) {
      PIController* controller = m_piControllerManager.GetController(m_selectedDevice);
      return controller && controller->IsConnected();
    }
    else {
      ACSController* controller = m_acsControllerManager.GetController(m_selectedDevice);
      return controller && controller->IsConnected();
    }
  }
}

bool GlobalJogPanel::DeviceSupportsUVW(const std::string& deviceId) {
  if (deviceId.empty()) {
    return false;
  }

  auto deviceOpt = m_configManager.GetDevice(deviceId);
  if (!deviceOpt.has_value()) {
    return false;
  }

  const auto& device = deviceOpt.value().get();

  // Check if this is a PI controller
  bool isPIController = false;
  if (device.TypeController == "PI") {
    isPIController = true;
  }
  else if (device.TypeController.empty() && device.Port == 50000) {
    isPIController = true;
  }

  if (!isPIController) {
    return false;
  }

  PIController* controller = m_piControllerManager.GetController(deviceId);
  if (!controller || !controller->IsConnected()) {
    m_logger->LogWarning("GlobalJogPanel: Controller not connected for " + deviceId);
    return false;
  }

  const auto& availableAxes = controller->GetAvailableAxes();

  if (debugverbose) {
    m_logger->LogInfo("GlobalJogPanel: DeviceSupportsUVW - available axes for " + deviceId + ": " + std::to_string(availableAxes.size()) + " axes");
  }

  if (availableAxes.size() >= 6) {
    // Check for numeric axes pattern (1, 2, 3, 4, 5, 6)
    if (std::find(availableAxes.begin(), availableAxes.end(), "4") != availableAxes.end() ||
      std::find(availableAxes.begin(), availableAxes.end(), "5") != availableAxes.end() ||
      std::find(availableAxes.begin(), availableAxes.end(), "6") != availableAxes.end()) {
      if (debugverbose) {
        m_logger->LogInfo("GlobalJogPanel: DeviceSupportsUVW - device has numeric axes (1-6)");
      }
      return true;
    }

    // Look for lettered axes (U, V, W)
    bool hasU = std::find(availableAxes.begin(), availableAxes.end(), "U") != availableAxes.end();
    bool hasV = std::find(availableAxes.begin(), availableAxes.end(), "V") != availableAxes.end();
    bool hasW = std::find(availableAxes.begin(), availableAxes.end(), "W") != availableAxes.end();

    if (debugverbose) {
      m_logger->LogInfo("GlobalJogPanel: DeviceSupportsUVW - U:" + std::to_string(hasU) +
        " V:" + std::to_string(hasV) + " W:" + std::to_string(hasW));
    }

    return hasU && hasV && hasW;
  }

  return false;
}