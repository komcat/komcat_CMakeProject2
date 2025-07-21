// MacroPanelUI_Execution.cpp - Cleaned and optimized
#include "MacroPanelUI.h"
#include "MacroManager.h"
#include <algorithm>
#include <iostream>

// ============================================================================

// ============================================================================
// FIXED EXECUTION METHODS - SYNC WITH MACROMANAGER STATE
// ============================================================================

void MacroPanelUI::ExecuteSelectedPrograms() {
  if (m_currentMacroName.empty() || !m_macroManager) {
    std::cout << "Error: No macro selected or MacroManager not available" << std::endl;
    return;
  }

  // Collect indices of selected programs
  std::vector<int> selectedIndices;
  for (int i = 0; i < m_programItems.size(); i++) {
    if (m_programItems[i].selected) {
      selectedIndices.push_back(i);
    }
  }

  if (selectedIndices.empty()) {
    std::cout << "No programs selected for execution" << std::endl;
    return;
  }

  std::cout << "Starting execution of " << selectedIndices.size()
    << " selected programs from macro '" << m_currentMacroName << "'" << std::endl;

  // Set UI execution state
  m_isExecuting = true;
  m_currentProgramIndex = 0;

  // Use MacroManager's built-in sequential execution
  m_macroManager->ExecuteMacroWithIndices(m_currentMacroName, selectedIndices);
}


void MacroPanelUI::ExecuteSingleProgram(const std::string& programName) {
  if (m_macroManager) {
    std::cout << "Executing single program: " << programName << std::endl;
    m_macroManager->ExecuteSingleProgram(programName);
  }
  else {
    std::cout << "Error: MacroManager not available" << std::endl;
  }
}

void MacroPanelUI::PlayExecution() {
  std::cout << "Play execution requested" << std::endl;
  ExecuteSelectedPrograms();
}

void MacroPanelUI::PauseExecution() {
  m_isPaused = !m_isPaused;
  std::cout << "Execution " << (m_isPaused ? "paused" : "resumed") << std::endl;
  // Note: Pause functionality would need to be implemented in MacroManager
}

void MacroPanelUI::StopExecution() {
  std::cout << "Stop execution requested" << std::endl;
  if (m_macroManager) {
    m_macroManager->StopExecution();
  }
  // Don't reset state here - let the sync method handle it
}

// ============================================================================
// RENDERING METHODS
// ============================================================================


// ============================================================================
// UPDATED EXECUTION CONTROLS WITH BETTER STATE MANAGEMENT
// ============================================================================

void MacroPanelUI::RenderExecutionControls() {
  // Sync state first
  SyncExecutionState();

  bool macroManagerExecuting = m_macroManager ? m_macroManager->IsExecuting() : false;

  // Play Button - Green (disabled during execution)
  if (macroManagerExecuting) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::Button("Playing...", ImVec2(60, 30));
    ImGui::PopStyleColor();
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    if (ImGui::Button("Play", ImVec2(60, 30))) {
      PlayExecution();
    }
    ImGui::PopStyleColor(2);
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Execute selected programs in sequence");
  }

  ImGui::SameLine();

  // Stop Button - Red (only enabled during execution)
  if (macroManagerExecuting) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button("Stop", ImVec2(60, 30))) {
      StopExecution();
    }
    ImGui::PopStyleColor(2);
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    ImGui::Button("Stop", ImVec2(60, 30));
    ImGui::PopStyleColor();
  }

  // Execution status text
  ImGui::SameLine();
  if (macroManagerExecuting) {
    if (m_currentProgramIndex >= 0 && m_currentProgramIndex < m_programItems.size()) {
      ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
        "Executing: %s (%d/%d)",
        m_programItems[m_currentProgramIndex].name.c_str(),
        m_currentProgramIndex + 1,
        (int)m_programItems.size());
    }
    else {
      ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Executing...");
    }
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Ready");
  }
}


void MacroPanelUI::RenderTableHeader() {
  ImGui::Columns(4, "ProgramColumns", true);
  ImGui::SetColumnWidth(0, 30);   // ID
  ImGui::SetColumnWidth(1, 30);   // Checkbox
  ImGui::SetColumnWidth(2, 150);  // Name
  ImGui::SetColumnWidth(3, 60);   // Actions

  ImGui::Text("ID");
  ImGui::NextColumn();
  ImGui::Text("✓");
  ImGui::NextColumn();
  ImGui::Text("Program Name");
  ImGui::NextColumn();
  ImGui::Text("Actions");
  ImGui::NextColumn();
  ImGui::Separator();
}

void MacroPanelUI::RenderProgramRow(int rowIndex, MacroProgramItem& item) {
  ImGui::PushID(rowIndex);

  // Highlight currently executing row
  if (m_isExecuting && rowIndex == m_currentProgramIndex) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f)); // Bright green

    // Add background highlight
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, 25);
    ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
      IM_COL32(0, 100, 0, 80)); // Green background
  }

  // ID Column
  ImGui::Text("%d", rowIndex + 1);
  ImGui::NextColumn();

  // Checkbox Column
  ImGui::Checkbox("##selected", &item.selected);
  ImGui::NextColumn();

  // Program Name Column
  if (m_isExecuting && rowIndex == m_currentProgramIndex) {
    ImGui::Text("▶ %s", item.name.c_str()); // Arrow indicator for currently running
  }
  else {
    ImGui::Text("%s", item.name.c_str());
  }
  ImGui::NextColumn();

  // Actions Column
  // Run Button - Blue
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
  if (ImGui::Button((">>##run" + std::to_string(rowIndex)).c_str(), ImVec2(25, 18))) {
    ExecuteSingleProgram(item.name);
  }
  ImGui::PopStyleColor(2);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Run this program only");
  }

  ImGui::SameLine();

  // Remove Button - Red
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
  if (ImGui::Button(("X##remove" + std::to_string(rowIndex)).c_str(), ImVec2(20, 18))) {
    if (m_macroManager && !m_currentMacroName.empty()) {
      m_macroManager->RemoveProgramFromMacro(m_currentMacroName, rowIndex);
      RefreshProgramItems();
      std::cout << "Removed program '" << item.name << "' from macro" << std::endl;
    }
  }
  ImGui::PopStyleColor(2);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Remove from macro");
  }

  ImGui::NextColumn();

  // Clean up style changes
  if (m_isExecuting && rowIndex == m_currentProgramIndex) {
    ImGui::PopStyleColor(); // Pop the text color
  }

  ImGui::PopID();
}

void MacroPanelUI::RenderAvailablePrograms() {
  ImGui::Text("Add Program:");

  if (!m_availablePrograms.empty()) {
    // Program selection dropdown
    std::vector<const char*> items;
    for (const auto& program : m_availablePrograms) {
      items.push_back(program.c_str());
    }

    ImGui::PushItemWidth(-1);
    ImGui::Combo("##ProgramSelect", &m_selectedProgramIndex, items.data(), items.size());
    ImGui::PopItemWidth();

    // Add button
    if (ImGui::Button("Add", ImVec2(-1, 0))) {
      if (m_selectedProgramIndex >= 0 && m_selectedProgramIndex < m_availablePrograms.size() &&
        !m_currentMacroName.empty() && m_macroManager) {

        std::string selectedProgram = m_availablePrograms[m_selectedProgramIndex];

        // Check if program is already in the macro
        bool alreadyExists = false;
        for (const auto& item : m_programItems) {
          if (item.name == selectedProgram) {
            alreadyExists = true;
            break;
          }
        }

        if (!alreadyExists) {
          m_macroManager->AddProgramToMacro(m_currentMacroName, selectedProgram);
          RefreshProgramItems();
          std::cout << "Added program '" << selectedProgram << "' to macro '" << m_currentMacroName << "'" << std::endl;
        }
        else {
          std::cout << "Program '" << selectedProgram << "' already exists in macro" << std::endl;
        }
      }
    }
  }
  else {
    // No programs available
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No programs available");
    if (ImGui::Button("Scan for Programs", ImVec2(-1, 0))) {
      if (m_macroManager) {
        m_macroManager->ScanForPrograms();
        RefreshAvailablePrograms();
        std::cout << "Rescanned for available programs" << std::endl;
      }
    }
  }
}

// ============================================================================
// MACRO VISUALIZATION
// ============================================================================

void MacroPanelUI::RenderMacroVisualization(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize) {
  if (m_programItems.empty()) return;

  const float boxWidth = 80.0f;
  const float boxHeight = 30.0f;
  const float spacing = 20.0f;
  const float startY = canvasPos.y + 20.0f;

  // Calculate total width and center the visualization
  float totalWidth = (boxWidth * m_programItems.size()) + (spacing * std::max(0, (int)m_programItems.size() - 1));
  float startX = canvasPos.x + (canvasSize.x - totalWidth) * 0.5f;

  // Draw program boxes
  for (int i = 0; i < m_programItems.size(); i++) {
    float x = startX + i * (boxWidth + spacing);
    float y = startY;

    // Determine colors based on state
    ImU32 boxColor;
    if (m_isExecuting && i == m_currentProgramIndex) {
      boxColor = IM_COL32(0, 150, 0, 255); // Green for currently executing
    }
    else if (m_programItems[i].selected) {
      boxColor = IM_COL32(0, 100, 200, 255); // Blue for selected
    }
    else {
      boxColor = IM_COL32(80, 80, 80, 255); // Gray for unselected
    }

    // Draw box
    ImVec2 boxMin = ImVec2(x, y);
    ImVec2 boxMax = ImVec2(x + boxWidth, y + boxHeight);
    drawList->AddRectFilled(boxMin, boxMax, boxColor, 5.0f);
    drawList->AddRect(boxMin, boxMax, IM_COL32(200, 200, 200, 255), 5.0f, 0, 1.5f);

    // Draw program name (truncated if needed)
    std::string displayName = m_programItems[i].name;
    if (displayName.length() > 10) {
      displayName = displayName.substr(0, 8) + "..";
    }

    ImVec2 textSize = ImGui::CalcTextSize(displayName.c_str());
    ImVec2 textPos = ImVec2(x + (boxWidth - textSize.x) * 0.5f, y + (boxHeight - textSize.y) * 0.5f);
    drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), displayName.c_str());

    // Draw arrow to next program
    if (i < m_programItems.size() - 1) {
      float arrowStartX = x + boxWidth + 5;
      float arrowEndX = x + boxWidth + spacing - 5;
      float arrowY = y + boxHeight * 0.5f;

      // Arrow line
      drawList->AddLine(ImVec2(arrowStartX, arrowY), ImVec2(arrowEndX, arrowY),
        IM_COL32(150, 150, 150, 255), 2.0f);

      // Arrow head
      drawList->AddLine(ImVec2(arrowEndX - 5, arrowY - 3), ImVec2(arrowEndX, arrowY),
        IM_COL32(150, 150, 150, 255), 2.0f);
      drawList->AddLine(ImVec2(arrowEndX - 5, arrowY + 3), ImVec2(arrowEndX, arrowY),
        IM_COL32(150, 150, 150, 255), 2.0f);
    }
  }

  // Draw legend
  float legendY = startY + boxHeight + 40.0f;
  ImVec2 legendPos = ImVec2(canvasPos.x + 10, legendY);

  // Running indicator
  drawList->AddRectFilled(ImVec2(legendPos.x, legendPos.y), ImVec2(legendPos.x + 15, legendPos.y + 15),
    IM_COL32(0, 150, 0, 255), 2.0f);
  drawList->AddText(ImVec2(legendPos.x + 20, legendPos.y), IM_COL32(200, 200, 200, 255), "Running");

  // Selected indicator
  drawList->AddRectFilled(ImVec2(legendPos.x + 80, legendPos.y), ImVec2(legendPos.x + 95, legendPos.y + 15),
    IM_COL32(0, 100, 200, 255), 2.0f);
  drawList->AddText(ImVec2(legendPos.x + 100, legendPos.y), IM_COL32(200, 200, 200, 255), "Selected");

  // Unselected indicator
  drawList->AddRectFilled(ImVec2(legendPos.x + 170, legendPos.y), ImVec2(legendPos.x + 185, legendPos.y + 15),
    IM_COL32(80, 80, 80, 255), 2.0f);
  drawList->AddText(ImVec2(legendPos.x + 190, legendPos.y), IM_COL32(200, 200, 200, 255), "Unselected");
}


// ============================================================================
// NEW METHOD: SYNC UI STATE WITH MACROMANAGER
// ============================================================================

void MacroPanelUI::SyncExecutionState() {
  if (!m_macroManager) return;

  bool macroManagerExecuting = m_macroManager->IsExecuting();
  std::string currentMacro = m_macroManager->GetCurrentMacro();

  // Check if MacroManager finished execution
  if (m_isExecuting && !macroManagerExecuting) {
    // Execution completed - reset UI state
    m_isExecuting = false;
    m_currentProgramIndex = -1;
    std::cout << "Execution completed - UI state synchronized" << std::endl;
  }

  // Update current program index if we're executing the same macro
  if (macroManagerExecuting && currentMacro == m_currentMacroName) {
    m_isExecuting = true;
    // You could get the current program index from MacroManager if needed
    // For now, we'll rely on the existing logic
  }
}