// global_jog_panel_ui.cpp - UI rendering functionality
#include "include/motions/global_jog_panel.h"

void GlobalJogPanel::RenderUI() {
  if (!m_showWindow) return;

  ImGui::Begin("Global Jog Control", &m_showWindow);

  RenderDeviceButtons();

  ImGui::SameLine();
  if (ImGui::Button(m_showPositions ? "Hide Positions" : "Show Positions")) {
    m_showPositions = !m_showPositions;
    m_logger->LogInfo("GlobalJogPanel: " + std::string(m_showPositions ? "Showing" : "Hiding") + " positions");
  }

  ImGui::Separator();
  RenderStepSizeControls();

  if (ImGui::Checkbox("Enable Key Binding", &m_keyBindingEnabled)) {
    m_logger->LogInfo("GlobalJogPanel: Key binding " + std::string(m_keyBindingEnabled ? "enabled" : "disabled"));
  }

  // Movement buttons
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

  RenderPositionDisplay();

  ImGui::Separator();

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

std::string GlobalJogPanel::FormatStepSize(double stepSize) const {
  if (stepSize < 1.0) {
    double microns = stepSize * 1000.0;
    if (microns < 1.0) {
      return std::to_string(microns) + " micron";
    }
    else {
      return std::to_string((int)microns) + " micron" + (microns != 1.0 ? "s" : "");
    }
  }
  else {
    return std::to_string((int)stepSize) + " mm";
  }
}

ImVec4 GlobalJogPanel::GetButtonColor(const std::string& key) {
  ImVec4 regularColor = ImVec4(0.5f, 0.5f, 1.0f, 0.8f);
  ImVec4 alertColor = ImVec4(0.7f, 0.7f, 1.0f, 1.0f);
  return m_keyBindingEnabled ? alertColor : regularColor;
}

void GlobalJogPanel::RenderDeviceButtons() {
  ImGui::Text("Device Selection:");

  const auto& allDevices = m_configManager.GetAllDevices();

  float buttonWidth = 120.0f;
  float buttonHeight = 35.0f;
  float spacing = 5.0f;

  int buttonsPerRow = (int)(ImGui::GetContentRegionAvail().x / (buttonWidth + spacing));
  if (buttonsPerRow < 1) buttonsPerRow = 1;

  int buttonCount = 0;

  for (const auto& [name, device] : allDevices) {
    if (!device.IsEnabled) continue;

    ImVec4 buttonColor;
    ImVec4 textColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

    bool isSelected = (m_selectedDevice == name);
    bool isConnected = false;

    // Check connection status
    if (device.TypeController == "PI") {
      PIController* controller = m_piControllerManager.GetController(name);
      isConnected = controller && controller->IsConnected();
    }
    else if (device.TypeController == "ACS") {
      ACSController* controller = m_acsControllerManager.GetController(name);
      isConnected = controller && controller->IsConnected();
    }
    else {
      // Fallback
      if (device.Port == 50000) {
        PIController* controller = m_piControllerManager.GetController(name);
        isConnected = controller && controller->IsConnected();
      }
      else {
        ACSController* controller = m_acsControllerManager.GetController(name);
        isConnected = controller && controller->IsConnected();
      }
    }

    // Set colors based on state
    if (isSelected && isConnected) {
      buttonColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
    }
    else if (isSelected && !isConnected) {
      buttonColor = ImVec4(1.0f, 0.5f, 0.0f, 1.0f); // Orange
    }
    else if (!isSelected && isConnected) {
      buttonColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Light gray
    }
    else {
      buttonColor = ImVec4(0.4f, 0.4f, 0.4f, 1.0f); // Dark gray
      textColor = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    }

    // Create display name
    std::string displayName = name;
    std::string controllerType = "Unknown";

    if (device.TypeController == "PI") {
      controllerType = "PI";
      displayName += "\n(PI)";
    }
    else if (device.TypeController == "ACS") {
      controllerType = "ACS";
      displayName += "\n(ACS)";
    }
    else {
      if (device.Port == 50000) {
        controllerType = "PI";
        displayName += "\n(PI)";
      }
      else {
        controllerType = "ACS";
        displayName += "\n(ACS)";
      }
    }

    // Layout logic
    if (buttonCount > 0 && buttonCount % buttonsPerRow != 0) {
      ImGui::SameLine();
    }

    ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);

    if (ImGui::Button(displayName.c_str(), ImVec2(buttonWidth, buttonHeight))) {
      m_selectedDevice = name;
      m_logger->LogInfo("GlobalJogPanel: Selected device: " + name);
    }

    ImGui::PopStyleColor(2);

    // Tooltip
    if (ImGui::IsItemHovered()) {
      std::string tooltip = name + "\nType: " + controllerType + " Controller";
      tooltip += "\nStatus: " + std::string(isConnected ? "Connected" : "Disconnected");
      if (isSelected) {
        tooltip += "\n(Currently Selected)";
      }
      ImGui::SetTooltip("%s", tooltip.c_str());
    }

    buttonCount++;
  }

  if (!m_selectedDevice.empty()) {
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Active Device: %s", m_selectedDevice.c_str());
  }
}

void GlobalJogPanel::RenderStepSizeControls() {
  std::string currentStepText = FormatStepSize(m_jogSteps[m_currentStepIndex]);
  ImGui::Text("Jog Step Size: %s", currentStepText.c_str());

  ImGui::Text("Quick Steps:");

  float buttonWidth = (ImGui::GetContentRegionAvail().x - 3 * 5.0f) / 4.0f;
  float buttonHeight = 30.0f;

  // 0.5 micron button
  bool is0_5Selected = (m_currentStepIndex == QUICK_STEP_0_5_MICRON);
  if (is0_5Selected) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
  }
  if (ImGui::Button("0.5um", ImVec2(buttonWidth, buttonHeight))) {
    m_currentStepIndex = QUICK_STEP_0_5_MICRON;
    m_logger->LogInfo("GlobalJogPanel: Quick set step to 0.5 micron");
  }
  if (is0_5Selected) {
    ImGui::PopStyleColor();
  }

  ImGui::SameLine();

  // 1 micron button
  bool is1Selected = (m_currentStepIndex == QUICK_STEP_1_MICRON);
  if (is1Selected) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
  }
  if (ImGui::Button("1um", ImVec2(buttonWidth, buttonHeight))) {
    m_currentStepIndex = QUICK_STEP_1_MICRON;
    m_logger->LogInfo("GlobalJogPanel: Quick set step to 1 micron");
  }
  if (is1Selected) {
    ImGui::PopStyleColor();
  }

  ImGui::SameLine();

  // 5 micron button
  bool is5Selected = (m_currentStepIndex == QUICK_STEP_5_MICRON);
  if (is5Selected) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
  }
  if (ImGui::Button("5um", ImVec2(buttonWidth, buttonHeight))) {
    m_currentStepIndex = QUICK_STEP_5_MICRON;
    m_logger->LogInfo("GlobalJogPanel: Quick set step to 5 microns");
  }
  if (is5Selected) {
    ImGui::PopStyleColor();
  }

  ImGui::SameLine();

  // 10 micron button  
  bool is10Selected = (m_currentStepIndex == QUICK_STEP_10_MICRON);
  if (is10Selected) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
  }
  if (ImGui::Button("10um", ImVec2(buttonWidth, buttonHeight))) {
    m_currentStepIndex = QUICK_STEP_10_MICRON;
    m_logger->LogInfo("GlobalJogPanel: Quick set step to 10 microns");
  }
  if (is10Selected) {
    ImGui::PopStyleColor();
  }

  // Full dropdown
  ImGui::Text("All Step Sizes:");
  ImGui::SameLine();

  if (ImGui::BeginCombo("##StepSize", currentStepText.c_str())) {
    for (int i = 0; i < m_jogSteps.size(); i++) {
      bool isSelected = (m_currentStepIndex == i);
      std::string sizeLabel = FormatStepSize(m_jogSteps[i]);

      if (ImGui::Selectable(sizeLabel.c_str(), isSelected)) {
        m_currentStepIndex = i;
        m_logger->LogInfo("GlobalJogPanel: Set jog step to " + sizeLabel);
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  ImGui::SameLine();

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
}