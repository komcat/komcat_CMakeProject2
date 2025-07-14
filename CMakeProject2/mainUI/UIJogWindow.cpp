// UIJogWindow.cpp - Updated to wrap GlobalJogPanel
#include "UIJogWindow.h"
#include "imgui.h"
#include <vector>
#include <string>

// Constructor for testing/mock mode
UIJogWindow::UIJogWindow(MotionConfigManager& configManager)
  : m_configManager(configManager) {
  // Start in mock mode - controllers will be set later
  m_mockWindowVisible = false; // Start hidden
}

// Constructor with controllers (for immediate real operation)
UIJogWindow::UIJogWindow(MotionConfigManager& configManager,
  PIControllerManager& piControllerManager,
  ACSControllerManager& acsControllerManager)
  : m_configManager(configManager),
  m_piControllerManager(&piControllerManager),
  m_acsControllerManager(&acsControllerManager) {

  // Create GlobalJogPanel immediately since we have controllers
  CreateGlobalJogPanel();
}

UIJogWindow::~UIJogWindow() = default;

void UIJogWindow::CreateGlobalJogPanel() {
  // Only create if we have both managers
  if (m_piControllerManager && m_acsControllerManager) {
    m_globalJogPanel = std::make_unique<GlobalJogPanel>(
      m_configManager,
      *m_piControllerManager,
      *m_acsControllerManager
    );
  }
}

void UIJogWindow::SetPIControllerManager(PIControllerManager* piManager) {
  m_piControllerManager = piManager;

  // Try to create GlobalJogPanel if we now have both managers
  if (m_piControllerManager && m_acsControllerManager && !m_globalJogPanel) {
    CreateGlobalJogPanel();
  }
}

void UIJogWindow::SetACSControllerManager(ACSControllerManager* acsManager) {
  m_acsControllerManager = acsManager;

  // Try to create GlobalJogPanel if we now have both managers
  if (m_piControllerManager && m_acsControllerManager && !m_globalJogPanel) {
    CreateGlobalJogPanel();
  }
}

void UIJogWindow::ToggleWindow() {
  if (m_globalJogPanel) {
    m_globalJogPanel->ToggleWindow();
  }
  else {
    // Fall back to mock mode
    m_mockWindowVisible = !m_mockWindowVisible;
  }
}

bool UIJogWindow::IsVisible() const {
  if (m_globalJogPanel) {
    return m_globalJogPanel->IsVisible();
  }
  else {
    // Fall back to mock mode
    return m_mockWindowVisible;
  }
}

void UIJogWindow::SetVisible(bool visible) {
  if (m_globalJogPanel) {
    if (visible != m_globalJogPanel->IsVisible()) {
      m_globalJogPanel->ToggleWindow();
    }
  }
  else {
    // Fall back to mock mode
    m_mockWindowVisible = visible;
  }
}

void UIJogWindow::RenderUI() {
  if (m_globalJogPanel) {
    // Set predetermined position on first render
    if (m_firstRender) {
      SetPredeterminedPosition();
      m_firstRender = false;
    }

    // Use real GlobalJogPanel
    m_globalJogPanel->RenderUI();
  }
  else {
    // Fall back to mock mode
    RenderMockJogWindow();
  }
}

void UIJogWindow::ProcessKeyInput(int keyCode, bool keyDown) {
  if (m_globalJogPanel) {
    // Forward to real GlobalJogPanel
    m_globalJogPanel->ProcessKeyInput(keyCode, keyDown);
  }
  // Mock mode doesn't handle key input
}

void UIJogWindow::SetPredeterminedPosition() {
  // Set the next window position for the jog window
  ImVec2 displaySize = ImGui::GetIO().DisplaySize;

  // Calculate 15% of screen width
  float windowWidth = displaySize.x * 0.15f;

  // Position at rightmost edge, 200px down from top
  ImVec2 windowPos = ImVec2(displaySize.x - windowWidth, 200.0f);

  ImGui::SetNextWindowPos(windowPos, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(windowWidth, 600.0f), ImGuiCond_FirstUseEver);
}

void UIJogWindow::RenderMockJogWindow() {
  if (!m_mockWindowVisible) return;

  // Set predetermined position
  if (m_firstRender) {
    SetPredeterminedPosition();
    m_firstRender = false;
  }

  // Create the mock jog window
  ImGui::Begin("Global Jog Control##Mock", &m_mockWindowVisible,
    ImGuiWindowFlags_NoCollapse);

  // Mock device selection
  ImGui::Text("Device");
  ImGui::SameLine();

  static int selectedDevice = 0;
  const char* mockDevices[] = { "hex-left (Mock)", "hex-right (Mock)", "gantry-main (Mock)" };
  ImGui::Combo("##Device", &selectedDevice, mockDevices, IM_ARRAYSIZE(mockDevices));

  static bool showPositions = false;
  if (ImGui::Button(showPositions ? "Hide Positions" : "Show Positions")) {
    showPositions = !showPositions;
  }

  ImGui::Separator();

  // Mock step size
  static int currentStepIndex = 6; // Default 0.01mm
  static std::vector<double> jogSteps = {
      0.0001, 0.0002, 0.0005,
      0.001, 0.002, 0.005,
      0.01, 0.02, 0.05,
      0.1, 0.2, 0.5,
      1.0, 2.0, 5.0
  };

  ImGui::Text("Jog Step Size: %.5f mm", jogSteps[currentStepIndex]);

  ImGui::Text("Step Size");
  ImGui::SameLine();

  if (ImGui::BeginCombo("##StepSize", std::to_string(jogSteps[currentStepIndex]).c_str())) {
    for (int i = 0; i < jogSteps.size(); i++) {
      bool isSelected = (currentStepIndex == i);
      std::string sizeLabel = std::to_string(jogSteps[i]);
      if (ImGui::Selectable(sizeLabel.c_str(), isSelected)) {
        currentStepIndex = i;
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  ImGui::SameLine();

  // Mock key binding checkbox
  static bool keyBindingEnabled = false;
  if (ImGui::Checkbox("Enable Key Binding", &keyBindingEnabled)) {
    // Mock toggle
  }

  // Mock movement buttons with enhanced colors when key binding is enabled
  float buttonWidth = ImGui::GetContentRegionAvail().x / 4.0f;
  float buttonHeight = 50.0f;

  // Define button color based on key binding state
  ImVec4 buttonColor = keyBindingEnabled ?
    ImVec4(0.7f, 0.7f, 1.0f, 1.0f) : ImVec4(0.5f, 0.5f, 1.0f, 0.8f);

  // Row 1
  ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
  if (ImGui::Button("Q\nDecr Step", ImVec2(buttonWidth, buttonHeight))) {
    if (currentStepIndex > 0) currentStepIndex--;
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
  if (ImGui::Button("W\nY-", ImVec2(buttonWidth, buttonHeight))) {
    // Mock movement
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
  if (ImGui::Button("E\nIncr Step", ImVec2(buttonWidth, buttonHeight))) {
    if (currentStepIndex < jogSteps.size() - 1) currentStepIndex++;
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
  if (ImGui::Button("R\nZ+", ImVec2(buttonWidth, buttonHeight))) {
    // Mock movement
  }
  ImGui::PopStyleColor();

  // Row 2
  ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
  if (ImGui::Button("A\nX-", ImVec2(buttonWidth, buttonHeight))) {
    // Mock movement
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
  if (ImGui::Button("S\nY+", ImVec2(buttonWidth, buttonHeight))) {
    // Mock movement
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
  if (ImGui::Button("D\nX+", ImVec2(buttonWidth, buttonHeight))) {
    // Mock movement
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
  if (ImGui::Button("F\nZ-", ImVec2(buttonWidth, buttonHeight))) {
    // Mock movement
  }
  ImGui::PopStyleColor();

  // Mock position display
  if (showPositions) {
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Current Positions for %s", mockDevices[selectedDevice]);
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Mock Mode - No Real Position Data");

    if (ImGui::Button("Refresh Positions")) {
      // Mock refresh
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy to Clipboard")) {
      // Mock copy
    }

    // Mock position table
    if (ImGui::BeginTable("PositionsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Axis", ImGuiTableColumnFlags_WidthFixed, 60.0f);
      ImGui::TableSetupColumn("Position (mm/deg)", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      const char* axes[] = { "X", "Y", "Z", "U", "V", "W" };
      const char* labels[] = { "X (Linear)", "Y (Linear)", "Z (Linear)", "U (Roll)", "V (Pitch)", "W (Yaw)" };
      double mockPositions[] = { 12.345678, 23.456789, 34.567890, 1.2345, 2.3456, 3.4567 };

      for (int i = 0; i < 6; i++) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "%s", labels[i]);
        ImGui::TableNextColumn();
        if (i >= 3) { // Rotation axes
          ImGui::Text("%.4f°", mockPositions[i]);
        }
        else { // Linear axes
          ImGui::Text("%.6f mm", mockPositions[i]);
        }
      }
      ImGui::EndTable();
    }
  }

  // Mock rotation controls for hexapod
  if (selectedDevice < 2) { // hex-left or hex-right
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "Rotation Controls (UVW)");

    double rotStep = jogSteps[currentStepIndex] * 10.0;
    ImGui::Text("Rotation Step: %.3f deg", rotStep);

    // Mock UVW controls
    const char* rotAxes[] = { "U", "V", "W" };
    const char* rotDescriptions[] = { "Roll", "Pitch", "Yaw" };

    ImVec4 negColor = ImVec4(0.8f, 0.3f, 0.3f, 0.9f);
    ImVec4 posColor = ImVec4(0.3f, 0.8f, 0.3f, 0.9f);
    ImVec4 labelColor = ImVec4(1.0f, 0.85f, 0.0f, 1.0f);

    for (int i = 0; i < 3; i++) {
      ImGui::TextColored(labelColor, "%s (%s):", rotAxes[i], rotDescriptions[i]);
      ImGui::SameLine();

      ImGui::PushStyleColor(ImGuiCol_Button, negColor);
      if (ImGui::Button(("<##" + std::string(rotAxes[i]) + "-").c_str(), ImVec2(60, 30))) {
        // Mock rotation -
      }
      ImGui::PopStyleColor();

      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "<-   ->");
      ImGui::SameLine();

      ImGui::PushStyleColor(ImGuiCol_Button, posColor);
      if (ImGui::Button((">##" + std::string(rotAxes[i]) + "+").c_str(), ImVec2(60, 30))) {
        // Mock rotation +
      }
      ImGui::PopStyleColor();
    }
  }

  ImGui::Separator();

  // Mock key bindings section
  if (ImGui::CollapsingHeader("Key Bindings")) {
    if (keyBindingEnabled) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Key bindings are ACTIVE (Mock Mode)");
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Key bindings are INACTIVE");
    }

    if (ImGui::BeginTable("KeyBindings", 3, ImGuiTableFlags_Borders)) {
      ImGui::TableSetupColumn("Key");
      ImGui::TableSetupColumn("Action");
      ImGui::TableSetupColumn("Description");
      ImGui::TableHeadersRow();

      // Mock key bindings
      const char* keys[] = { "A", "D", "W", "S", "R", "F", "Q", "E" };
      const char* actions[] = { "X-", "X+", "Y-", "Y+", "Z+", "Z-", "Step-", "Step+" };
      const char* descriptions[] = {
        "Move X axis negative", "Move X axis positive",
        "Move Y axis negative", "Move Y axis positive",
        "Move Z axis positive", "Move Z axis negative",
        "Decrease jog step", "Increase jog step"
      };

      for (int i = 0; i < 8; i++) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("%s", keys[i]);
        ImGui::TableNextColumn(); ImGui::Text("%s", actions[i]);
        ImGui::TableNextColumn(); ImGui::Text("%s", descriptions[i]);
      }

      ImGui::EndTable();
    }
  }

  ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Status: Mock Mode - Motion controllers not available");

  ImGui::End();
}