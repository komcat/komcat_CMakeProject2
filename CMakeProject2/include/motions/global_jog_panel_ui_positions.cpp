// global_jog_panel_ui_positions.cpp - Position display and rotation controls
#include "include/motions/global_jog_panel.h"

void GlobalJogPanel::RenderPositionDisplay() {
  if (!m_showPositions || m_selectedDevice.empty()) {
    return;
  }

  ImGui::Separator();
  ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Current Positions for %s", m_selectedDevice.c_str());

  if (!IsDeviceConnected()) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Device not connected");
    return;
  }

  std::map<std::string, double> positions = GetCurrentPositions();

  if (positions.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Unable to read positions");
    return;
  }

  if (ImGui::Button("Refresh Positions")) {
    positions = GetCurrentPositions();
  }

  ImGui::SameLine();

  if (ImGui::Button("Copy to Clipboard")) {
    std::string jsonStr = "{\n";
    jsonStr += "  \"device\": \"" + m_selectedDevice + "\",\n";
    jsonStr += "  \"positions\": {\n";

    auto it = positions.begin();
    while (it != positions.end()) {
      char positionBuffer[64];
      snprintf(positionBuffer, sizeof(positionBuffer), "%.6f", it->second);

      jsonStr += "    \"" + it->first + "\": " + std::string(positionBuffer);
      ++it;
      if (it != positions.end()) {
        jsonStr += ",";
      }
      jsonStr += "\n";
    }

    jsonStr += "  }\n";
    jsonStr += "}";

    ImGui::SetClipboardText(jsonStr.c_str());
    m_logger->LogInfo("GlobalJogPanel: Copied positions to clipboard for " + m_selectedDevice);
  }

  if (ImGui::BeginTable("PositionsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Axis", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableSetupColumn("Position (mm/deg)", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    std::vector<std::pair<std::string, std::string>> axisInfo = {
      {"X", "X (Linear)"},
      {"Y", "Y (Linear)"},
      {"Z", "Z (Linear)"},
      {"U", "U (Roll)"},
      {"V", "V (Pitch)"},
      {"W", "W (Yaw)"}
    };

    for (const auto& [axis, label] : axisInfo) {
      auto posIt = positions.find(axis);
      if (posIt != positions.end()) {
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "%s", label.c_str());

        ImGui::TableNextColumn();
        if (axis == "U" || axis == "V" || axis == "W") {
          ImGui::Text("%.4f°", posIt->second);
        }
        else {
          ImGui::Text("%.6f mm", posIt->second);
        }
      }
    }

    ImGui::EndTable();
  }

  ImGui::Separator();
  ImGui::Text("Motion Status:");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected & Ready");
}

void GlobalJogPanel::RenderRotationControls() {
  if (!DeviceSupportsUVW(m_selectedDevice)) {
    return;
  }

  ImGui::Separator();
  ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "Rotation Controls (UVW)");

  double rotStep = m_jogSteps[m_currentStepIndex] * 10.0;
  ImGui::Text("Rotation Step: %.3f deg", rotStep);

  float fullWidth = ImGui::GetContentRegionAvail().x;
  float controlWidth = 250.0f;
  float startX = (fullWidth - controlWidth) * 0.5f;

  float buttonWidth = 60.0f;
  float buttonHeight = 30.0f;
  float arrowWidth = 70.0f;

  ImVec4 negColor = ImVec4(0.8f, 0.3f, 0.3f, 0.9f);
  ImVec4 posColor = ImVec4(0.3f, 0.8f, 0.3f, 0.9f);
  ImVec4 labelColor = ImVec4(1.0f, 0.85f, 0.0f, 1.0f);
  ImVec4 textColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

  const char* axes[] = { "U", "V", "W" };
  const char* axisDescriptions[] = { "Roll", "Pitch", "Yaw" };

  for (int i = 0; i < 3; ++i) {
    ImGui::SetCursorPosX(startX);

    ImGui::PushStyleColor(ImGuiCol_Text, labelColor);
    ImVec2 textSize = ImGui::CalcTextSize((std::string(axes[i]) + " (" + axisDescriptions[i] + "):").c_str());
    ImVec2 textPos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
      textPos,
      ImVec2(textPos.x + textSize.x, textPos.y + textSize.y),
      IM_COL32(40, 40, 40, 200)
    );
    ImGui::Text("%s (%s):", axes[i], axisDescriptions[i]);
    ImGui::PopStyleColor();

    ImGui::SameLine(startX + 80.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, negColor);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    if (ImGui::Button(("<##" + std::string(axes[i]) + "-").c_str(), ImVec2(buttonWidth, buttonHeight))) {
      MoveRotationAxis(axes[i], -rotStep);
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    float textWidth = ImGui::CalcTextSize("<-   ->").x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (arrowWidth - textWidth) * 0.5f);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "<-   ->");

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, posColor);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    if (ImGui::Button((">##" + std::string(axes[i]) + "+").c_str(), ImVec2(buttonWidth, buttonHeight))) {
      MoveRotationAxis(axes[i], rotStep);
    }
    ImGui::PopStyleColor(2);
  }
}