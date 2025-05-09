// global_jog_panel.cpp
#include "include/motions/global_jog_panel.h"
#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <iostream>

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

  // Initialize key bindings
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

    if (deviceX != 0.0 || deviceY != 0.0 || deviceZ != 0.0) {
      m_logger->LogInfo("GlobalJogPanel: Transformed movement for " + deviceId +
        ": Global [" + std::to_string(globalX) + "," + std::to_string(globalY) + "," + std::to_string(globalZ) +
        "] -> Device [" + std::to_string(deviceX) + "," + std::to_string(deviceY) + "," + std::to_string(deviceZ) + "]");
    }
  }
  else {
    // No transformation found, use identity (direct mapping)
    deviceX = globalX;
    deviceY = globalY;
    deviceZ = globalZ;

    m_logger->LogWarning("GlobalJogPanel: No transformation found for device: " + deviceId);
  }
}

void GlobalJogPanel::MoveAxis(const std::string& axis) {
  if (m_selectedDevice.empty()) {
    m_logger->LogWarning("GlobalJogPanel: No device selected for movement");
    return;
  }

  // Get current jog step
  double stepSize = m_jogSteps[m_currentStepIndex];

  // Determine which global axis to move and the direction
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
  else {
    // ACS controller
    ACSController* controller = m_acsControllerManager.GetController(m_selectedDevice);
    if (controller && controller->IsConnected()) {
      // Move each axis if needed
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

void GlobalJogPanel::ProcessKeyInput(int keyCode, bool keyDown) {
  if (!m_keyBindingEnabled || !m_showWindow || m_selectedDevice.empty()) {
    return;
  }

  // Only process key down events
  if (!keyDown) {
    return;
  }

  // Check which key was pressed
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

ImVec4 GlobalJogPanel::GetButtonColor(const std::string& key) {
  // Default button color
  ImVec4 regularColor = ImVec4(0.5f, 0.5f, 1.0f, 0.8f);

  // When key binding is enabled, use a more alerting color
  ImVec4 alertColor = ImVec4(0.7f, 0.7f, 1.0f, 1.0f);

  return m_keyBindingEnabled ? alertColor : regularColor;
}

void GlobalJogPanel::RenderUI() {
  if (!m_showWindow) return;

  ImGui::Begin("Global Jog Control", &m_showWindow);

  // Device selection dropdown
  ImGui::Text("Device");
  ImGui::SameLine();

  if (ImGui::BeginCombo("##Device", m_selectedDevice.c_str())) {
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
          m_logger->LogInfo("GlobalJogPanel: Selected device: " + name);
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
  ImGui::Text("Step Size");
  ImGui::SameLine();

  if (ImGui::BeginCombo("##StepSize", std::to_string(m_jogSteps[m_currentStepIndex]).c_str())) {
    for (int i = 0; i < m_jogSteps.size(); i++) {
      bool isSelected = (m_currentStepIndex == i);
      std::string sizeLabel = std::to_string(m_jogSteps[i]);
      if (ImGui::Selectable(sizeLabel.c_str(), isSelected)) {
        m_currentStepIndex = i;
        m_logger->LogInfo("GlobalJogPanel: Set jog step to " + std::to_string(m_jogSteps[m_currentStepIndex]));
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  ImGui::SameLine();

  // Step increase/decrease buttons
  ImGui::PushStyleColor(ImGuiCol_Button, GetButtonColor("Q"));
  if (ImGui::Button("Q Step-")) {
    DecreaseStep();
  }
  ImGui::PopStyleColor();

  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, GetButtonColor("E"));
  if (ImGui::Button("E Step+")) {
    IncreaseStep();
  }
  ImGui::PopStyleColor();

  // Enable key binding checkbox
  if (ImGui::Checkbox("Enable Key Binding", &m_keyBindingEnabled)) {
    m_logger->LogInfo("GlobalJogPanel: Key binding " + std::string(m_keyBindingEnabled ? "enabled" : "disabled"));
  }

  // 2x4 grid of movement buttons with enhanced colors when key binding is enabled
  float buttonWidth = ImGui::GetContentRegionAvail().x / 4.0f;
  float buttonHeight = 50.0f;

  // Row 1
  ImGui::PushStyleColor(ImGuiCol_Button, GetButtonColor("Q"));
  if (ImGui::Button("Q\nDecr Step", ImVec2(buttonWidth, buttonHeight))) {
    DecreaseStep();
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, GetButtonColor("W"));
  if (ImGui::Button("W\nY-", ImVec2(buttonWidth, buttonHeight))) {
    MoveAxis("Y-");
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, GetButtonColor("E"));
  if (ImGui::Button("E\nIncr Step", ImVec2(buttonWidth, buttonHeight))) {
    IncreaseStep();
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, GetButtonColor("R"));
  if (ImGui::Button("R\nZ+", ImVec2(buttonWidth, buttonHeight))) {
    MoveAxis("Z+");
  }
  ImGui::PopStyleColor();

  // Row 2
  ImGui::PushStyleColor(ImGuiCol_Button, GetButtonColor("A"));
  if (ImGui::Button("A\nX-", ImVec2(buttonWidth, buttonHeight))) {
    MoveAxis("X-");
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, GetButtonColor("S"));
  if (ImGui::Button("S\nY+", ImVec2(buttonWidth, buttonHeight))) {
    MoveAxis("Y+");
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, GetButtonColor("D"));
  if (ImGui::Button("D\nX+", ImVec2(buttonWidth, buttonHeight))) {
    MoveAxis("X+");
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, GetButtonColor("F"));
  if (ImGui::Button("F\nZ-", ImVec2(buttonWidth, buttonHeight))) {
    MoveAxis("Z-");
  }
  ImGui::PopStyleColor();





  ImGui::Separator();

  //if (!m_selectedDevice.empty() && DeviceSupportsUVW(m_selectedDevice)) {
  //  m_logger->LogInfo("GlobalJogPanel: Rendering UVW controls for " + m_selectedDevice);
  //  RenderRotationControls();
  //}
  //else {
  //  m_logger->LogWarning("GlobalJogPanel: UVW controls not shown for " + m_selectedDevice +
  //    " - DeviceSupportsUVW: " + std::to_string(DeviceSupportsUVW(m_selectedDevice)));
  //}

  // Add rotation controls if device supports UVW
  if (!m_selectedDevice.empty() && DeviceSupportsUVW(m_selectedDevice)) {
    RenderRotationControls();
  }

  ImGui::Separator();
  // Key bindings section
  if (ImGui::CollapsingHeader("Key Bindings")) {
    if (m_keyBindingEnabled) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Key bindings are ACTIVE");
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Key bindings are INACTIVE");
    }

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



// Check if device supports UVW axes (typically PI hexapod controllers)
// Check if device supports UVW axes (typically PI hexapod controllers)
bool GlobalJogPanel::DeviceSupportsUVW(const std::string& deviceId) {
  if (deviceId.empty()) {
    return false;
  }

  auto deviceOpt = m_configManager.GetDevice(deviceId);
  if (!deviceOpt.has_value()) {
    return false;
  }

  const auto& device = deviceOpt.value().get();

  // Check if this is a PI controller (port 50000)
  if (device.Port != 50000) {
    return false;
  }

  // Get the controller
  PIController* controller = m_piControllerManager.GetController(deviceId);
  if (!controller || !controller->IsConnected()) {
    m_logger->LogWarning("GlobalJogPanel: Controller not connected for " + deviceId);
    return false;
  }

  // Get available axes from the controller
  const auto& availableAxes = controller->GetAvailableAxes();

  // Log the available axes
  if (debugverbose)m_logger->LogInfo("GlobalJogPanel: DeviceSupportsUVW - available axes for " + deviceId + ": " +std::to_string(availableAxes.size()) + " axes");

  // If axes are named as numbers 1-6 instead of letters X,Y,Z,U,V,W
  // Check if there are at least 6 axes (hexapod typically has 6 axes)
  if (availableAxes.size() >= 6) {
    // Check if the axes are numbered or lettered
    bool hasNumericAxes = false;

    // Check for numeric axes pattern (1, 2, 3, 4, 5, 6)
    if (availableAxes.size() >= 6) {
      // For hex devices that use axis numbers 1-6 instead of letters
      if (std::find(availableAxes.begin(), availableAxes.end(), "4") != availableAxes.end() ||
        std::find(availableAxes.begin(), availableAxes.end(), "5") != availableAxes.end() ||
        std::find(availableAxes.begin(), availableAxes.end(), "6") != availableAxes.end()) {
        if(debugverbose) m_logger->LogInfo("GlobalJogPanel: DeviceSupportsUVW - device has numeric axes (1-6)");
        return true;
      }
    }

    // Look for lettered axes (U, V, W)
    bool hasU = std::find(availableAxes.begin(), availableAxes.end(), "U") != availableAxes.end();
    bool hasV = std::find(availableAxes.begin(), availableAxes.end(), "V") != availableAxes.end();
    bool hasW = std::find(availableAxes.begin(), availableAxes.end(), "W") != availableAxes.end();

    if (debugverbose)m_logger->LogInfo("GlobalJogPanel: DeviceSupportsUVW - U:" + std::to_string(hasU) +
      " V:" + std::to_string(hasV) + " W:" + std::to_string(hasW));

    return hasU && hasV && hasW;
  }

  return false;
}

// Handle UVW rotation movement
// Handle UVW rotation movement with support for numeric axes
// Handle UVW rotation movement for hexapod devices
void GlobalJogPanel::MoveRotationAxis(const std::string& axis, double amount) {
  if (m_selectedDevice.empty()) {
    m_logger->LogWarning("GlobalJogPanel: No device selected for rotation");
    return;
  }

  // Verify device supports UVW
  if (!DeviceSupportsUVW(m_selectedDevice)) {
    m_logger->LogWarning("GlobalJogPanel: Selected device does not support rotation axes");
    return;
  }

  // Get the controller
  PIController* controller = m_piControllerManager.GetController(m_selectedDevice);
  if (!controller || !controller->IsConnected()) {
    m_logger->LogError("GlobalJogPanel: Controller not available for device: " + m_selectedDevice);
    return;
  }

  // For PI controllers, we always use the original axis letters (U, V, W)
  // The PIController implementation handles the mapping to numeric axes internally if needed

  // Execute the move - PI expects the actual letter in the API
  bool success = controller->MoveRelative(axis, amount, false);

  if (success) {
    m_logger->LogInfo("GlobalJogPanel: Moved rotation axis " + axis + " by " + std::to_string(amount) + " deg");
  }
  else {
    m_logger->LogWarning("GlobalJogPanel: Failed to move rotation axis " + axis);
  }
}




// Render rotation controls in the UI
// Render rotation controls in the UI with improved contrast
void GlobalJogPanel::RenderRotationControls() {
  // Skip rendering if device doesn't support UVW
  if (!DeviceSupportsUVW(m_selectedDevice)) {
    return;
  }

  ImGui::Separator();
  // Use a more visible blue color for the title
  ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "Rotation Controls (UVW)");

  // Current step for rotation (use smaller values for rotation)
  // Convert linear step to rotation step - use 10x scaling for rotations
  double rotStep = m_jogSteps[m_currentStepIndex] * 10.0; // Convert from mm to degrees with scaling

  ImGui::Text("Rotation Step: %.3f deg", rotStep);

  // Center everything
  float fullWidth = ImGui::GetContentRegionAvail().x;
  float controlWidth = 250.0f; // Total width of control group
  float startX = (fullWidth - controlWidth) * 0.5f;

  // Define consistent button sizes
  float buttonWidth = 60.0f;
  float buttonHeight = 30.0f;
  float arrowWidth = 70.0f;

  // Styling constants with improved contrast
  ImVec4 negColor = ImVec4(0.8f, 0.3f, 0.3f, 0.9f);   // Brighter red for negative
  ImVec4 posColor = ImVec4(0.3f, 0.8f, 0.3f, 0.9f);   // Brighter green for positive
  ImVec4 labelColor = ImVec4(1.0f, 0.85f, 0.0f, 1.0f); // Gold instead of yellow for labels
  ImVec4 textColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);   // Black text for buttons for contrast

  // Draw U, V, W axis controls in rows
  const char* axes[] = { "U", "V", "W" };
  const char* axisDescriptions[] = { "Roll", "Pitch", "Yaw" };

  for (int i = 0; i < 3; ++i) {
    // Create centered layout
    ImGui::SetCursorPosX(startX);

    // Axis label with fixed width - use larger, more visible font
    ImGui::PushStyleColor(ImGuiCol_Text, labelColor);
    // Draw a darker background behind the text for better contrast
    ImVec2 textSize = ImGui::CalcTextSize((std::string(axes[i]) + " (" + axisDescriptions[i] + "):").c_str());
    ImVec2 textPos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
      textPos,
      ImVec2(textPos.x + textSize.x, textPos.y + textSize.y),
      IM_COL32(40, 40, 40, 200)
    );
    ImGui::Text("%s (%s):", axes[i], axisDescriptions[i]);
    ImGui::PopStyleColor();

    // Negative button
    ImGui::SameLine(startX + 80.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, negColor);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor); // Black text for contrast
    if (ImGui::Button(("<##" + std::string(axes[i]) + "-").c_str(), ImVec2(buttonWidth, buttonHeight))) {
      MoveRotationAxis(axes[i], -rotStep);
    }
    ImGui::PopStyleColor(2);

    // Draw directional arrows in the middle
    ImGui::SameLine();
    float textWidth = ImGui::CalcTextSize("<-   ->").x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (arrowWidth - textWidth) * 0.5f);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "<-   ->"); // White for better visibility

    // Positive button
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, posColor);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor); // Black text for contrast
    if (ImGui::Button((">##" + std::string(axes[i]) + "+").c_str(), ImVec2(buttonWidth, buttonHeight))) {
      MoveRotationAxis(axes[i], rotStep);
    }
    ImGui::PopStyleColor(2);
  }
}