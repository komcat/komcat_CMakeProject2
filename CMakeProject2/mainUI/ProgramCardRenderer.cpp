
// ProgramCardRenderer.cpp
#include "ProgramCardRenderer.h"
#include "MacroPanelUI.h" // For MacroProgramItem

ProgramCardRenderer::ProgramCardRenderer() {
  // Constructor - initialize any default values if needed
}

void ProgramCardRenderer::RenderProgramCards(
  const std::vector<MacroProgramItem>& programItems,
  int currentExecutingIndex,
  bool isExecuting,
  std::function<void(const std::string&)> onRunProgram,
  std::function<void(int)> onRemoveProgram) {

  if (programItems.empty()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No programs in this macro");
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Add programs from the left panel");
    return;
  }

  ImGui::Text("Programs (%zu):", programItems.size());
  ImGui::Separator();
  ImGui::Spacing();

  // Render each program card
  for (int i = 0; i < programItems.size(); i++) {
    bool isCurrentlyExecuting = isExecuting && (i == currentExecutingIndex);

    RenderSingleCard(
      programItems[i],
      i,
      isCurrentlyExecuting,
      onRunProgram,
      onRemoveProgram
    );

    // Add connector to next card (except for last card)
    if (m_showConnectors && i < programItems.size() - 1) {
      ImVec2 connectorStart = ImGui::GetCursorScreenPos();
      connectorStart.x += ImGui::GetContentRegionAvail().x * 0.5f;
      connectorStart.y -= 5;

      ImVec2 connectorEnd = connectorStart;
      connectorEnd.y += m_cardSpacing;

      DrawConnector(connectorStart, connectorEnd);
    }

    // Add spacing between cards
    if (i < programItems.size() - 1) {
      ImGui::Dummy(ImVec2(0, m_cardSpacing));
    }
  }
}

void ProgramCardRenderer::RenderSingleCard(
  const MacroProgramItem& item,
  int index,
  bool isCurrentlyExecuting,
  std::function<void(const std::string&)> onRunProgram,
  std::function<void(int)> onRemoveProgram) {

  ImGui::PushID(index);

  // Calculate card dimensions
  ImVec2 cardSize = ImVec2(ImGui::GetContentRegionAvail().x, m_cardHeight);
  ImVec2 cardPos = ImGui::GetCursorScreenPos();

  // Determine colors based on state
  ImU32 cardColor = GetCardColor(item.selected, isCurrentlyExecuting);
  ImU32 borderColor = GetBorderColor(item.selected, isCurrentlyExecuting);

  // Draw card background and border
  DrawCardBackground(cardPos, cardSize, cardColor, borderColor);

  // Draw status indicator (left border)
  DrawStatusIndicator(cardPos, cardSize, borderColor);

  // Create child window for card content
  ImGui::BeginChild(("Card" + std::to_string(index)).c_str(), cardSize, false, ImGuiWindowFlags_NoScrollbar);

  // Card content layout
  ImGui::SetCursorPosX(15); // Indent from status border
  ImGui::SetCursorPosY(8);

  // Top row: Checkbox, Icon, Program name, Action buttons
  ImGui::Checkbox("##select", const_cast<bool*>(&item.selected));
  ImGui::SameLine();

  // Program icon
  if (m_showIcons) {
    const char* icon = GetProgramIcon(item.name);
    ImGui::Text("%s", icon);
    ImGui::SameLine();
  }

  // Program name with execution indicator
  if (isCurrentlyExecuting) {
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "▶ %s", item.name.c_str());
  }
  else {
    ImGui::Text("%s", item.name.c_str());
  }

  // Action buttons (top right)
  ImGui::SameLine();
  float buttonAreaWidth = 60;
  ImGui::SetCursorPosX(cardSize.x - buttonAreaWidth - 10);

  // Run button
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 0.8f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
  if (ImGui::Button(("▶##run" + std::to_string(index)).c_str(), ImVec2(25, 25))) {
    onRunProgram(item.name);
  }
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(2);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Run this program");
  }

  ImGui::SameLine();

  // Remove button
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.3f, 0.3f, 0.8f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
  if (ImGui::Button(("✕##remove" + std::to_string(index)).c_str(), ImVec2(25, 25))) {
    onRemoveProgram(index);
  }
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(2);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Remove from macro");
  }

  // Status/progress row
  ImGui::SetCursorPosY(45);
  ImGui::SetCursorPosX(15);

  if (isCurrentlyExecuting) {
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 1.0f, 0.2f, 0.8f));
    ImGui::ProgressBar(0.6f, ImVec2(cardSize.x - 85, 8), "");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::SetCursorPosY(42);
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Executing...");
  }
  else if (item.selected) {
    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Ready to execute");
  }
  else {
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Not selected");
  }

  ImGui::EndChild();
  ImGui::PopID();
}

void ProgramCardRenderer::RenderEnhancedExecutionControls(
  bool isExecuting,
  bool isPaused,
  std::function<void()> onPlay,
  std::function<void()> onStop,
  std::function<void()> onPause) {

  ImVec2 regionAvail = ImGui::GetContentRegionAvail();
  float buttonWidth = (regionAvail.x - 20) / 3; // 3 buttons with spacing
  float buttonHeight = 40;

  // Play/Pause button
  if (isExecuting && !isPaused) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.2f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.7f, 0.3f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    if (ImGui::Button("⏸ Pause", ImVec2(buttonWidth, buttonHeight))) {
      onPause();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    if (ImGui::Button("▶ Play", ImVec2(buttonWidth, buttonHeight))) {
      onPlay();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Execute selected programs in sequence");
  }

  ImGui::SameLine();

  // Stop button
  bool stopEnabled = isExecuting;
  ImGui::PushStyleColor(ImGuiCol_Button, stopEnabled ?
    ImVec4(0.8f, 0.2f, 0.2f, 0.9f) : ImVec4(0.4f, 0.4f, 0.4f, 0.5f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, stopEnabled ?
    ImVec4(0.9f, 0.3f, 0.3f, 1.0f) : ImVec4(0.4f, 0.4f, 0.4f, 0.5f));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

  bool stopClicked = ImGui::Button("⏹ Stop", ImVec2(buttonWidth, buttonHeight));
  if (stopClicked && stopEnabled) {
    onStop();
  }

  ImGui::PopStyleVar();
  ImGui::PopStyleColor(2);

  if (ImGui::IsItemHovered() && stopEnabled) {
    ImGui::SetTooltip("Stop execution");
  }

  ImGui::SameLine();

  // Info/Settings button
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.7f, 0.8f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.8f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
  if (ImGui::Button("ℹ Info", ImVec2(buttonWidth, buttonHeight))) {
    // Could open a modal with execution details
  }
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(2);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Execution information and settings");
  }
}

// Helper methods
void ProgramCardRenderer::DrawCardBackground(ImVec2 cardPos, ImVec2 cardSize, ImU32 cardColor, ImU32 borderColor) {
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  ImVec2 cardMax = ImVec2(cardPos.x + cardSize.x, cardPos.y + cardSize.y);

  // Draw background
  drawList->AddRectFilled(cardPos, cardMax, cardColor, m_cornerRadius);

  // Draw border
  drawList->AddRect(cardPos, cardMax, borderColor, m_cornerRadius, 0, m_borderWidth);
}

void ProgramCardRenderer::DrawStatusIndicator(ImVec2 cardPos, ImVec2 cardSize, ImU32 borderColor) {
  ImDrawList* drawList = ImGui::GetWindowDrawList();

  // Left border status indicator
  ImVec2 indicatorMax = ImVec2(cardPos.x + 4, cardPos.y + cardSize.y);
  drawList->AddRectFilled(
    cardPos,
    indicatorMax,
    borderColor,
    m_cornerRadius,
    ImDrawFlags_RoundCornersLeft
  );
}

void ProgramCardRenderer::DrawConnector(ImVec2 startPos, ImVec2 endPos) {
  ImDrawList* drawList = ImGui::GetWindowDrawList();

  // Main line
  drawList->AddLine(startPos, endPos, IM_COL32(100, 100, 100, 255), 2.0f);

  // Arrow head
  ImVec2 arrowTip = endPos;
  ImVec2 arrowLeft = ImVec2(arrowTip.x - 4, arrowTip.y - 4);
  ImVec2 arrowRight = ImVec2(arrowTip.x + 4, arrowTip.y - 4);

  drawList->AddTriangleFilled(arrowTip, arrowLeft, arrowRight, IM_COL32(100, 100, 100, 255));
}

const char* ProgramCardRenderer::GetProgramIcon(const std::string& programName) {
  // Simple icon mapping based on program name patterns
  std::string lowerName = programName;
  std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

  if (lowerName.find("camera") != std::string::npos ||
    lowerName.find("pic") != std::string::npos) return "📷";
  if (lowerName.find("move") != std::string::npos ||
    lowerName.find("gantry") != std::string::npos) return "🏗️";
  if (lowerName.find("test") != std::string::npos ||
    lowerName.find("prompt") != std::string::npos) return "📝";
  if (lowerName.find("home") != std::string::npos) return "🏠";
  if (lowerName.find("probe") != std::string::npos ||
    lowerName.find("measure") != std::string::npos) return "🔍";
  if (lowerName.find("sled") != std::string::npos) return "🛷";

  return "⚙️"; // Default gear icon
}

ImU32 ProgramCardRenderer::GetCardColor(bool isSelected, bool isExecuting) {
  if (isExecuting) return m_executingCardColor;
  if (isSelected) return m_selectedCardColor;
  return m_defaultCardColor;
}

ImU32 ProgramCardRenderer::GetBorderColor(bool isSelected, bool isExecuting) {
  if (isExecuting) return m_executingBorderColor;
  if (isSelected) return m_selectedBorderColor;
  return m_defaultBorderColor;
}
