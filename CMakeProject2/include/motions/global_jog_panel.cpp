// global_jog_panel.cpp
#include "include/motions/global_jog_panel.h"
#include "imgui.h"
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

bool GlobalJogPanel::LoadTransformations(const std::string& filePath) {
  try {
    // Open and read the file
    std::ifstream file(filePath);
    if (!file.is_open()) {
      m_logger->LogError("GlobalJogPanel: Could not open transformation file: " + filePath);
      return false;
    }

    // Parse JSON
    json transformJson;
    file >> transformJson;

    // Clear existing transformations
    m_deviceTransforms.clear();

    // Parse transformations
    for (const auto& item : transformJson) {
      DeviceTransform transform;
      transform.deviceId = item["DeviceId"];

      // Parse matrix elements
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
  // Find the transformation matrix for this device
  auto it = std::find_if(m_deviceTransforms.begin(), m_deviceTransforms.end(),
    [&deviceId](const DeviceTransform& transform) {
    return transform.deviceId == deviceId;
  });

  if (it != m_deviceTransforms.end()) {
    const TransformationMatrix& matrix = it->matrix;

    // Apply the transformation
    deviceX = matrix.M11 * globalX + matrix.M12 * globalY + matrix.M13 * globalZ;
    deviceY = matrix.M21 * globalX + matrix.M22 * globalY + matrix.M23 * globalZ;
    deviceZ = matrix.M31 * globalX + matrix.M32 * globalY + matrix.M33 * globalZ;

    m_logger->LogInfo("GlobalJogPanel: Transformed movement for " + deviceId +
      ": Global [" + std::to_string(globalX) + "," + std::to_string(globalY) + "," + std::to_string(globalZ) +
      "] -> Device [" + std::to_string(deviceX) + "," + std::to_string(deviceY) + "," + std::to_string(deviceZ) + "]");
  }
  else {
    // No transformation found, use identity (direct mapping)
    deviceX = globalX;
    deviceY = globalY;
    deviceZ = globalZ;

    m_logger->LogWarning("GlobalJogPanel: No transformation found for device: " + deviceId);
  }
}

void GlobalJogPanel::MoveAxis(const std::string& axis, double distance) {
  if (m_selectedDevice.empty()) {
    m_logger->LogWarning("GlobalJogPanel: No device selected for movement");
    return;
  }

  // Get current jog step
  double stepSize = m_jogSteps[m_currentStepIndex];
  double moveDistance = (axis.find("+") != std::string::npos) ? stepSize : -stepSize;

  // Determine which global axis to move
  double globalX = 0.0, globalY = 0.0, globalZ = 0.0;

  if (axis == "X+" || axis == "X-") {
    globalX = moveDistance;
  }
  else if (axis == "Y+" || axis == "Y-") {
    globalY = moveDistance;
  }
  else if (axis == "Z+" || axis == "Z-") {
    globalZ = moveDistance;
  }

  // Transform to device coordinates
  double deviceX = 0.0, deviceY = 0.0, deviceZ = 0.0;
  TransformMovement(m_selectedDevice, globalX, globalY, globalZ, deviceX, deviceY, deviceZ);

  // Check if we need to use PI or ACS controller
  auto deviceOpt = m_configManager.GetDevice(m_selectedDevice);
  if (!deviceOpt.has_value()) {
    m_logger->LogError("GlobalJogPanel: Device not found: " + m_selectedDevice);
    return;
  }

  const auto& device = deviceOpt.value().get();

  if (device.Port == 50000) {
    // PI controller
    PIController* controller = m_piControllerManager.GetController(m_selectedDevice);
    if (controller && controller->IsConnected()) {
      // Move each axis if needed
      if (deviceX != 0.0) {
        controller->MoveRelative("X", deviceX, false);
      }
      if (deviceY != 0.0) {
        controller->MoveRelative("Y", deviceY, false);
      }
      if (deviceZ != 0.0) {
        controller->MoveRelative("Z", deviceZ, false);
      }

      m_logger->LogInfo("GlobalJogPanel: Moved PI device " + m_selectedDevice);
    }
    else {
      m_logger->LogError("GlobalJogPanel: PI controller not available/connected for " + m_selectedDevice);
    }
  }
  else {
    // ACS controller
    ACSController* controller = m_acsControllerManager.GetController(m_selectedDevice);
    if (controller && controller->IsConnected()) {
      // Move each axis if needed
      if (deviceX != 0.0) {
        controller->MoveRelative("X", deviceX, false);
      }
      if (deviceY != 0.0) {
        controller->MoveRelative("Y", deviceY, false);
      }
      if (deviceZ != 0.0) {
        controller->MoveRelative("Z", deviceZ, false);
      }

      m_logger->LogInfo("GlobalJogPanel: Moved ACS device " + m_selectedDevice);
    }
    else {
      m_logger->LogError("GlobalJogPanel: ACS controller not available/connected for " + m_selectedDevice);
    }
  }
}

void GlobalJogPanel::IncreaseStep() {
  if (m_currentStepIndex < m_jogSteps.size() - 1) {
    m_currentStepIndex++;
    m_logger->LogInfo("GlobalJogPanel: Increased jog step to " + std::to_string(m_jogSteps[m_currentStepIndex]));
  }
}

void GlobalJogPanel::DecreaseStep() {
  if (m_currentStepIndex > 0) {
    m_currentStepIndex--;
    m_logger->LogInfo("GlobalJogPanel: Decreased jog step to " + std::to_string(m_jogSteps[m_currentStepIndex]));
  }
}

void GlobalJogPanel::HandleKeyInput() {
  // This is a mock function for keyboard input handling
  // In a real implementation, you would capture keyboard input from ImGui or SDL
  // and trigger the appropriate movement actions

  m_logger->LogInfo("GlobalJogPanel: Key input handling not implemented yet");

  // Example implementation would look like:
  /*
  if (ImGui::IsKeyPressed(ImGuiKey_A)) {
      MoveAxis("X-", m_jogSteps[m_currentStepIndex]);
  }
  else if (ImGui::IsKeyPressed(ImGuiKey_D)) {
      MoveAxis("X+", m_jogSteps[m_currentStepIndex]);
  }
  // ... and so on for other keys
  */
}

void GlobalJogPanel::RenderUI() {
  if (!m_showWindow) return;

  ImGui::Begin("Global Jog Control", &m_showWindow);

  // Device selection dropdown
  if (ImGui::BeginCombo("Device", m_selectedDevice.c_str())) {
    const auto& allDevices = m_configManager.GetAllDevices();
    for (const auto& [name, device] : allDevices) {
      bool isSelected = (m_selectedDevice == name);

      // Create a display string with the device port type
      std::string displayName = name;
      if (device.Port == 50000) {
        displayName += " (PI)";
      }
      else {
        displayName += " (ACS)";
      }

      // Only enabled devices
      if (device.IsEnabled) {
        if (ImGui::Selectable(displayName.c_str(), isSelected)) {
          m_selectedDevice = name;
        }
        if (isSelected) {
          ImGui::SetItemDefaultFocus();
        }
      }
    }
    ImGui::EndCombo();
  }

  ImGui::Separator();

  // Step size selection
  ImGui::Text("Jog Step Size: %.5f mm", m_jogSteps[m_currentStepIndex]);

  // Step size combo box
  if (ImGui::BeginCombo("Step Size", std::to_string(m_jogSteps[m_currentStepIndex]).c_str())) {
    for (int i = 0; i < m_jogSteps.size(); i++) {
      bool isSelected = (m_currentStepIndex == i);
      if (ImGui::Selectable(std::to_string(m_jogSteps[i]).c_str(), isSelected)) {
        m_currentStepIndex = i;
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  ImGui::SameLine();

  // Step increase/decrease buttons
  if (ImGui::Button("Q Step-")) {
    DecreaseStep();
  }
  ImGui::SameLine();
  if (ImGui::Button("E Step+")) {
    IncreaseStep();
  }

  ImGui::Separator();

  // 2x4 grid of movement buttons
  float buttonWidth = ImGui::GetContentRegionAvail().x / 4.0f;
  float buttonHeight = 50.0f;

  // Row 1
  if (ImGui::Button("Q\nDecr Step", ImVec2(buttonWidth, buttonHeight))) {
    DecreaseStep();
  }
  ImGui::SameLine();

  if (ImGui::Button("W\nY-", ImVec2(buttonWidth, buttonHeight))) {
    MoveAxis("Y-", m_jogSteps[m_currentStepIndex]);
  }
  ImGui::SameLine();

  if (ImGui::Button("E\nIncr Step", ImVec2(buttonWidth, buttonHeight))) {
    IncreaseStep();
  }
  ImGui::SameLine();

  if (ImGui::Button("R\nZ+", ImVec2(buttonWidth, buttonHeight))) {
    MoveAxis("Z+", m_jogSteps[m_currentStepIndex]);
  }

  // Row 2
  if (ImGui::Button("A\nX-", ImVec2(buttonWidth, buttonHeight))) {
    MoveAxis("X-", m_jogSteps[m_currentStepIndex]);
  }
  ImGui::SameLine();

  if (ImGui::Button("S\nY+", ImVec2(buttonWidth, buttonHeight))) {
    MoveAxis("Y+", m_jogSteps[m_currentStepIndex]);
  }
  ImGui::SameLine();

  if (ImGui::Button("D\nX+", ImVec2(buttonWidth, buttonHeight))) {
    MoveAxis("X+", m_jogSteps[m_currentStepIndex]);
  }
  ImGui::SameLine();

  if (ImGui::Button("F\nZ-", ImVec2(buttonWidth, buttonHeight))) {
    MoveAxis("Z-", m_jogSteps[m_currentStepIndex]);
  }

  ImGui::Separator();

  // Key bindings section
  if (ImGui::CollapsingHeader("Key Bindings")) {
    ImGui::Text("The following key bindings are available (not implemented yet):");

    if (ImGui::BeginTable("KeyBindings", 3, ImGuiTableFlags_Borders)) {
      ImGui::TableSetupColumn("Key");
      ImGui::TableSetupColumn("Action");
      ImGui::TableSetupColumn("Description");
      ImGui::TableHeadersRow();

      for (const auto& binding : m_keyBindings) {
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::Text("%s", binding.key.c_str());

        ImGui::TableNextColumn();
        ImGui::Text("%s", binding.action.c_str());

        ImGui::TableNextColumn();
        ImGui::Text("%s", binding.description.c_str());
      }

      ImGui::EndTable();
    }
  }

  ImGui::End();
}