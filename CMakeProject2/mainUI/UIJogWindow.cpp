// UIJogWindow.cpp - Simplified version for testing without controllers
#include "UIJogWindow.h"
#include "imgui.h"
#include <vector>
#include <string>

// Constructor for testing/mock mode
UIJogWindow::UIJogWindow(MotionConfigManager& configManager) {
  // Pure mock mode - no GlobalJogPanel dependency
  m_mockWindowVisible = false; // Start hidden
}

// This constructor for when we have controllers (future use)
UIJogWindow::UIJogWindow(MotionConfigManager& configManager,
  PIControllerManager& piControllerManager,
  ACSControllerManager& acsControllerManager) {
  // For now, just use mock mode even with controllers
  // TODO: Implement real GlobalJogPanel when controllers are ready
  m_mockWindowVisible = false;
}

UIJogWindow::~UIJogWindow() = default;

void UIJogWindow::ToggleWindow() {
  // Pure mock implementation
  m_mockWindowVisible = !m_mockWindowVisible;
}

bool UIJogWindow::IsVisible() const {
  // Pure mock implementation
  return m_mockWindowVisible;
}

void UIJogWindow::SetVisible(bool visible) {
  // Pure mock implementation
  m_mockWindowVisible = visible;
}

void UIJogWindow::RenderUI() {
  // Pure mock implementation
  RenderMockJogWindow();
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

  //ImGui::SameLine();
  static bool showPositions = false;
  if (ImGui::Button(showPositions ? "Hide Positions" : "Show Positions")) {
    showPositions = !showPositions;
  }

  ImGui::Separator();

  // Mock step size controls - hard-coded with proper labels
  static int currentStepIndex = 6;
  static std::vector<double> jogSteps = {
    0.0001, 0.0002, 0.0005,  // 0.1, 0.2, 0.5 micron
    0.001, 0.002, 0.005,     // 1, 2, 5 micron  
    0.01, 0.02, 0.05,        // 10, 20, 50 micron
    0.1, 0.2, 0.5,           // 100, 200, 500 micron
    1.0, 2.0, 5.0, 10.0      // 1, 2, 5, 10 mm
  };

  static std::vector<std::string> jogStepLabels = {
    "0.1 micron", "0.2 micron", "0.5 micron",
    "1 micron", "2 micron", "5 micron",
    "10 micron", "20 micron", "50 micron",
    "100 micron", "200 micron", "500 micron",
    "1 mm", "2 mm", "5 mm", "10 mm"
  };

  // Display current step size with hard-coded labels
  ImGui::Text("Jog Step Size: %s", jogStepLabels[currentStepIndex].c_str());

  ImGui::Text("Step Size");
  ImGui::SameLine();

  // Combo box with hard-coded formatted labels
  if (ImGui::BeginCombo("##StepSize", jogStepLabels[currentStepIndex].c_str())) {
    for (int i = 0; i < jogSteps.size(); i++) {
      bool isSelected = (currentStepIndex == i);

      if (ImGui::Selectable(jogStepLabels[i].c_str(), isSelected)) {
        currentStepIndex = i;
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  //ImGui::SameLine();

  // Step controls
  if (ImGui::Button("Q Step-")) {
    if (currentStepIndex > 0) currentStepIndex--;
  }

  ImGui::SameLine();

  if (ImGui::Button("E Step+")) {
    if (currentStepIndex < jogSteps.size() - 1) currentStepIndex++;
  }

  // Enable key binding checkbox
  static bool keyBindingEnabled = false;
  if (ImGui::Checkbox("Enable Key Binding", &keyBindingEnabled)) {
    // Mock key binding toggle
  }

  // 2x4 grid of movement buttons
  float buttonWidth = ImGui::GetContentRegionAvail().x / 4.0f;
  float buttonHeight = 50.0f;

  // Row 1
  ImVec4 buttonColor = keyBindingEnabled ? ImVec4(0.7f, 0.7f, 1.0f, 1.0f) : ImVec4(0.5f, 0.5f, 1.0f, 0.8f);

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

    if (ImGui::Button("Refresh Positions")) {
      // Mock refresh
    }

    ImGui::SameLine();

    if (ImGui::Button("Copy to Clipboard")) {
      // Mock copy to clipboard
      ImGui::SetClipboardText("{\n  \"device\": \"mock-device\",\n  \"positions\": {\n    \"X\": 0.000000,\n    \"Y\": 0.000000,\n    \"Z\": 0.000000\n  }\n}");
    }

    // Mock position table
    if (ImGui::BeginTable("PositionsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Axis", ImGuiTableColumnFlags_WidthFixed, 60.0f);
      ImGui::TableSetupColumn("Position (mm/deg)", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      // Mock position values
      const char* axes[] = { "X (Linear)", "Y (Linear)", "Z (Linear)", "U (Roll)", "V (Pitch)", "W (Yaw)" };
      static float mockPositions[] = { 10.123456f, -5.654321f, 8.244764f, 0.1234f, -0.5678f, 0.9012f };

      for (int i = 0; i < 6; i++) {
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "%s", axes[i]);

        ImGui::TableNextColumn();
        if (i >= 3) {
          ImGui::Text("%.4f°", mockPositions[i]);
        }
        else {
          ImGui::Text("%.6f mm", mockPositions[i]);
        }
      }

      ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::Text("Motion Status:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Mock Mode - Controllers Not Connected");
  }

  // Mock rotation controls for hexapod
  if (selectedDevice < 2) { // hex-left or hex-right
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "Rotation Controls (UVW)");

    double rotStep = jogSteps[currentStepIndex] * 10.0;
    ImGui::Text("Rotation Step: %s (x10)", jogStepLabels[currentStepIndex].c_str());

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

  ImGui::Text("Status: Mock Mode - UI Testing Only");

  ImGui::End();
}

void UIJogWindow::SetPredeterminedPosition() {
  // Set the next window position for the mock jog window
  ImVec2 displaySize = ImGui::GetIO().DisplaySize;

  // Calculate 15% of screen width
  float windowWidth = displaySize.x * 0.15f;

  // Position at rightmost edge, 200px down from top
  ImVec2 windowPos = ImVec2(displaySize.x - windowWidth, 200.0f);

  ImGui::SetNextWindowPos(windowPos, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(windowWidth, 600.0f), ImGuiCond_FirstUseEver);
}

void UIJogWindow::ProcessKeyInput(int keyCode, bool keyDown) {
  // For mock mode, we could add key handling here if needed
  // TODO: Implement when real controllers are available
}