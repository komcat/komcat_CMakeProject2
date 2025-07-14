// global_jog_panel_movement.cpp - Movement functionality
#include "include/motions/global_jog_panel.h"

void GlobalJogPanel::MoveAxis(const std::string& axis) {
  if (m_selectedDevice.empty()) {
    m_logger->LogWarning("GlobalJogPanel: No device selected for movement");
    return;
  }

  double stepSize = m_jogSteps[m_currentStepIndex];
  double globalX = 0.0, globalY = 0.0, globalZ = 0.0;

  if (axis == "X+") {
    globalX = stepSize;
  }
  else if (axis == "X-") {
    globalX = -stepSize;
  }
  else if (axis == "Y+") {
    globalY = stepSize;
  }
  else if (axis == "Y-") {
    globalY = -stepSize;
  }
  else if (axis == "Z+") {
    globalZ = stepSize;
  }
  else if (axis == "Z-") {
    globalZ = -stepSize;
  }
  else {
    m_logger->LogError("GlobalJogPanel: Unknown axis: " + axis);
    return;
  }

  double deviceX = 0.0, deviceY = 0.0, deviceZ = 0.0;
  TransformMovement(m_selectedDevice, globalX, globalY, globalZ, deviceX, deviceY, deviceZ);

  auto deviceOpt = m_configManager.GetDevice(m_selectedDevice);
  if (!deviceOpt.has_value()) {
    m_logger->LogError("GlobalJogPanel: Device not found: " + m_selectedDevice);
    return;
  }

  const auto& device = deviceOpt.value().get();

  if (device.TypeController == "PI") {
    PIController* controller = m_piControllerManager.GetController(m_selectedDevice);
    if (controller && controller->IsConnected()) {
      bool moved = false;
      if (deviceX != 0.0) {
        controller->MoveRelative("X", deviceX, false);
        moved = true;
      }
      if (deviceY != 0.0) {
        controller->MoveRelative("Y", deviceY, false);
        moved = true;
      }
      if (deviceZ != 0.0) {
        controller->MoveRelative("Z", deviceZ, false);
        moved = true;
      }
      if (moved) {
        m_logger->LogInfo("GlobalJogPanel: Moved PI device " + m_selectedDevice + " on " + axis);
      }
    }
    else {
      m_logger->LogError("GlobalJogPanel: PI controller not available/connected for " + m_selectedDevice);
    }
  }
  else if (device.TypeController == "ACS") {
    ACSController* controller = m_acsControllerManager.GetController(m_selectedDevice);
    if (controller && controller->IsConnected()) {
      bool moved = false;
      if (deviceX != 0.0) {
        controller->MoveRelative("X", deviceX, false);
        moved = true;
      }
      if (deviceY != 0.0) {
        controller->MoveRelative("Y", deviceY, false);
        moved = true;
      }
      if (deviceZ != 0.0) {
        controller->MoveRelative("Z", deviceZ, false);
        moved = true;
      }
      if (moved) {
        m_logger->LogInfo("GlobalJogPanel: Moved ACS device " + m_selectedDevice + " on " + axis);
      }
    }
    else {
      m_logger->LogError("GlobalJogPanel: ACS controller not available/connected for " + m_selectedDevice);
    }
  }
  else {
    // Fallback to port-based detection
    if (device.Port == 50000) {
      PIController* controller = m_piControllerManager.GetController(m_selectedDevice);
      if (controller && controller->IsConnected()) {
        bool moved = false;
        if (deviceX != 0.0) {
          controller->MoveRelative("X", deviceX, false);
          moved = true;
        }
        if (deviceY != 0.0) {
          controller->MoveRelative("Y", deviceY, false);
          moved = true;
        }
        if (deviceZ != 0.0) {
          controller->MoveRelative("Z", deviceZ, false);
          moved = true;
        }
        if (moved) {
          m_logger->LogInfo("GlobalJogPanel: Moved PI device " + m_selectedDevice + " on " + axis);
        }
      }
    }
    else {
      ACSController* controller = m_acsControllerManager.GetController(m_selectedDevice);
      if (controller && controller->IsConnected()) {
        bool moved = false;
        if (deviceX != 0.0) {
          controller->MoveRelative("X", deviceX, false);
          moved = true;
        }
        if (deviceY != 0.0) {
          controller->MoveRelative("Y", deviceY, false);
          moved = true;
        }
        if (deviceZ != 0.0) {
          controller->MoveRelative("Z", deviceZ, false);
          moved = true;
        }
        if (moved) {
          m_logger->LogInfo("GlobalJogPanel: Moved ACS device " + m_selectedDevice + " on " + axis);
        }
      }
    }
  }
}

void GlobalJogPanel::IncreaseStep() {
  if (m_currentStepIndex < m_jogSteps.size() - 1) {
    m_currentStepIndex++;
    m_logger->LogInfo("GlobalJogPanel: Increased jog step to " + FormatStepSize(m_jogSteps[m_currentStepIndex]));
  }
}

void GlobalJogPanel::DecreaseStep() {
  if (m_currentStepIndex > 0) {
    m_currentStepIndex--;
    m_logger->LogInfo("GlobalJogPanel: Decreased jog step to " + FormatStepSize(m_jogSteps[m_currentStepIndex]));
  }
}

void GlobalJogPanel::MoveRotationAxis(const std::string& axis, double amount) {
  if (m_selectedDevice.empty()) {
    m_logger->LogWarning("GlobalJogPanel: No device selected for rotation");
    return;
  }

  if (!DeviceSupportsUVW(m_selectedDevice)) {
    m_logger->LogWarning("GlobalJogPanel: Selected device does not support rotation axes");
    return;
  }

  PIController* controller = m_piControllerManager.GetController(m_selectedDevice);
  if (!controller || !controller->IsConnected()) {
    m_logger->LogError("GlobalJogPanel: Controller not available for device: " + m_selectedDevice);
    return;
  }

  bool success = controller->MoveRelative(axis, amount, false);

  if (success) {
    m_logger->LogInfo("GlobalJogPanel: Moved rotation axis " + axis + " by " + std::to_string(amount) + " deg");
  }
  else {
    m_logger->LogWarning("GlobalJogPanel: Failed to move rotation axis " + axis);
  }
}