// global_jog_panel_core.cpp - Core functionality
#include "include/motions/global_jog_panel.h"
#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

GlobalJogPanel::GlobalJogPanel(MotionConfigManager& configManager,
  PIControllerManager& piControllerManager,
  ACSControllerManager& acsControllerManager)
  : m_configManager(configManager),
  m_piControllerManager(piControllerManager),
  m_acsControllerManager(acsControllerManager)
{
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("GlobalJogPanel: Initializing");

  InitializeStepSizes();
  InitializeKeyBindings();

  // Load transformation matrices
  if (LoadTransformations("transformation_matrix.json")) {
    m_logger->LogInfo("GlobalJogPanel: Transformation matrices loaded successfully");
  }
  else {
    m_logger->LogError("GlobalJogPanel: Failed to load transformation matrices");
  }
}

GlobalJogPanel::~GlobalJogPanel() {
  m_logger->LogInfo("GlobalJogPanel: Shutting down");
}

void GlobalJogPanel::InitializeStepSizes() {
  m_jogSteps = {
      0.0001,  // 0.1 micron
      0.0002,  // 0.2 micron
      0.0005,  // 0.5 micron
      0.001,   // 1 micron
      0.002,   // 2 micron
      0.005,   // 5 micron
      0.01,    // 10 micron
      0.02,    // 20 micron
      0.05,    // 50 micron
      0.1,     // 100 micron
      0.2,     // 200 micron
      0.5,     // 500 micron
      1.0,     // 1 mm
      2.0,     // 2 mm
      5.0      // 5 mm
  };
}

void GlobalJogPanel::InitializeKeyBindings() {
  m_keyBindings = {
      {"A", 'a', "X-", "Move X axis negative"},
      {"D", 'd', "X+", "Move X axis positive"},
      {"W", 'w', "Y-", "Move Y axis negative"},
      {"S", 's', "Y+", "Move Y axis positive"},
      {"R", 'r', "Z+", "Move Z axis positive"},
      {"F", 'f', "Z-", "Move Z axis negative"},
      {"Q", 'q', "Step-", "Decrease jog step"},
      {"E", 'e', "Step+", "Increase jog step"}
  };
}

bool GlobalJogPanel::LoadTransformations(const std::string& filePath) {
  try {
    std::ifstream file(filePath);
    if (!file.is_open()) {
      m_logger->LogError("GlobalJogPanel: Could not open transformation file: " + filePath);
      return false;
    }

    json transformJson;
    file >> transformJson;
    m_deviceTransforms.clear();

    for (const auto& item : transformJson) {
      DeviceTransform transform;
      transform.deviceId = item["DeviceId"];

      const auto& matrix = item["Matrix"];
      transform.matrix.M11 = matrix["M11"];
      transform.matrix.M12 = matrix["M12"];
      transform.matrix.M13 = matrix["M13"];
      transform.matrix.M21 = matrix["M21"];
      transform.matrix.M22 = matrix["M22"];
      transform.matrix.M23 = matrix["M23"];
      transform.matrix.M31 = matrix["M31"];
      transform.matrix.M32 = matrix["M32"];
      transform.matrix.M33 = matrix["M33"];

      m_deviceTransforms.push_back(transform);
      m_logger->LogInfo("GlobalJogPanel: Loaded transformation for device: " + transform.deviceId);
    }

    return !m_deviceTransforms.empty();
  }
  catch (const std::exception& e) {
    m_logger->LogError("GlobalJogPanel: Error loading transformations: " + std::string(e.what()));
    return false;
  }
}

void GlobalJogPanel::TransformMovement(const std::string& deviceId,
  double globalX, double globalY, double globalZ,
  double& deviceX, double& deviceY, double& deviceZ) {

  auto it = std::find_if(m_deviceTransforms.begin(), m_deviceTransforms.end(),
    [&deviceId](const DeviceTransform& transform) {
    return transform.deviceId == deviceId;
  });

  if (it != m_deviceTransforms.end()) {
    const TransformationMatrix& matrix = it->matrix;
    deviceX = matrix.M11 * globalX + matrix.M12 * globalY + matrix.M13 * globalZ;
    deviceY = matrix.M21 * globalX + matrix.M22 * globalY + matrix.M23 * globalZ;
    deviceZ = matrix.M31 * globalX + matrix.M32 * globalY + matrix.M33 * globalZ;

    if (deviceX != 0.0 || deviceY != 0.0 || deviceZ != 0.0) {
      m_logger->LogInfo("GlobalJogPanel: Transformed movement for " + deviceId +
        ": Global [" + std::to_string(globalX) + "," + std::to_string(globalY) + "," + std::to_string(globalZ) +
        "] -> Device [" + std::to_string(deviceX) + "," + std::to_string(deviceY) + "," + std::to_string(deviceZ) + "]");
    }
  }
  else {
    deviceX = globalX;
    deviceY = globalY;
    deviceZ = globalZ;
    m_logger->LogWarning("GlobalJogPanel: No transformation found for device: " + deviceId);
  }
}

void GlobalJogPanel::ProcessKeyInput(int keyCode, bool keyDown) {
  if (!m_keyBindingEnabled || !m_showWindow || m_selectedDevice.empty()) {
    return;
  }

  if (!keyDown) {
    return;
  }

  for (const auto& binding : m_keyBindings) {
    if (binding.keyCode == keyCode) {
      m_logger->LogInfo("GlobalJogPanel: Key pressed: " + binding.key + " for action: " + binding.action);

      if (binding.action == "X+") {
        MoveAxis("X+");
      }
      else if (binding.action == "X-") {
        MoveAxis("X-");
      }
      else if (binding.action == "Y+") {
        MoveAxis("Y+");
      }
      else if (binding.action == "Y-") {
        MoveAxis("Y-");
      }
      else if (binding.action == "Z+") {
        MoveAxis("Z+");
      }
      else if (binding.action == "Z-") {
        MoveAxis("Z-");
      }
      else if (binding.action == "Step+") {
        IncreaseStep();
      }
      else if (binding.action == "Step-") {
        DecreaseStep();
      }
      break;
    }
  }
}