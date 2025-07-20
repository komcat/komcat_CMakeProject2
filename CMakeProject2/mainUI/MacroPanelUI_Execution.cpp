// MacroPanelUI_Execution.cpp - Execution methods and detailed rendering
#include "MacroPanelUI.h"
#include "MacroManager.h"
#include <algorithm>
#include <iostream>

// ============================================================================
// EXECUTION METHODS
// ============================================================================

// Fixed execution logic for MacroPanelUI_Execution.cpp

void MacroPanelUI::ExecuteSelectedPrograms() {
  if (m_currentMacroName.empty() || !m_macroManager) return;

  // Collect only selected programs in order
  std::vector<std::string> selectedPrograms;
  for (const auto& item : m_programItems) {
    if (item.selected) {
      selectedPrograms.push_back(item.name);
    }
  }

  if (selectedPrograms.empty()) {
    std::cout << "No programs selected for execution" << std::endl;
    return;
  }

  // Set execution state
  m_isExecuting = true;
  m_currentProgramIndex = 0;

  std::cout << "Starting selected program execution: " << selectedPrograms.size()
    << " programs from macro '" << m_currentMacroName << "'" << std::endl;

  // Store selected programs for sequential execution
  m_selectedProgramsQueue = selectedPrograms;
  m_currentExecutionIndex = 0;

  // Start executing the first program
  ExecuteNextSelectedProgram();
}

// Execute programs one by one with proper sequencing
void MacroPanelUI::ExecuteNextSelectedProgram() {
  if (m_currentExecutionIndex >= m_selectedProgramsQueue.size()) {
    // All programs completed
    m_isExecuting = false;
    m_currentProgramIndex = -1;
    m_selectedProgramsQueue.clear();
    std::cout << "All selected programs execution completed successfully" << std::endl;
    return;
  }

  std::string currentProgram = m_selectedProgramsQueue[m_currentExecutionIndex];

  // Update visual feedback - find the program index in the original list
  for (int i = 0; i < m_programItems.size(); i++) {
    if (m_programItems[i].name == currentProgram) {
      m_currentProgramIndex = i;
      break;
    }
  }

  std::cout << "Executing selected program " << (m_currentExecutionIndex + 1)
    << "/" << m_selectedProgramsQueue.size() << ": " << currentProgram << std::endl;

  if (m_macroManager) {
    m_macroManager->ExecuteSingleProgram(currentProgram);

    // Since ExecuteSingleProgram doesn't have callback, we need to implement
    // a polling mechanism or timer to check when to continue to next program
    // For now, we'll use a simple approach that works with the existing system

    m_currentExecutionIndex++;

    // Add a small delay and continue (you might want to implement proper completion checking)
    // This is a simplified approach - in a real implementation you'd want to check
    // program completion status before continuing
    ExecuteNextSelectedProgram();
  }
}

// Better approach: Execute selected programs without temporary macros
void MacroPanelUI::ExecuteSelectedProgramsClean() {
  if (m_currentMacroName.empty() || !m_macroManager) return;

  // Collect selected programs
  std::vector<std::string> selectedPrograms;
  for (const auto& item : m_programItems) {
    if (item.selected) {
      selectedPrograms.push_back(item.name);
    }
  }

  if (selectedPrograms.empty()) {
    std::cout << "No programs selected for execution" << std::endl;
    return;
  }

  std::cout << "Executing " << selectedPrograms.size() << " selected programs in sequence:" << std::endl;

  // Execute each selected program immediately (works if programs are fast)
  for (int i = 0; i < selectedPrograms.size(); i++) {
    // Update visual feedback
    for (int j = 0; j < m_programItems.size(); j++) {
      if (m_programItems[j].name == selectedPrograms[i]) {
        m_currentProgramIndex = j;
        break;
      }
    }

    std::cout << "  - Executing (" << (i + 1) << "/" << selectedPrograms.size() << "): " << selectedPrograms[i] << std::endl;
    m_macroManager->ExecuteSingleProgram(selectedPrograms[i]);
  }

  std::cout << "All selected programs have been executed" << std::endl;
  m_isExecuting = false;
  m_currentProgramIndex = -1;
}

void MacroPanelUI::ExecuteSingleProgram(const std::string& programName) {
  if (m_macroManager) {
    std::cout << "Executing single program: " << programName << std::endl;
    m_macroManager->ExecuteSingleProgram(programName);
  }
}

void MacroPanelUI::PlayExecution() {
  std::cout << "Play execution requested" << std::endl;
  ExecuteSelectedPrograms();
}

void MacroPanelUI::PauseExecution() {
  m_isPaused = !m_isPaused;
  std::cout << "Pause execution: " << (m_isPaused ? "Paused" : "Resumed") << std::endl;
  // TODO: Implement pause functionality in MacroManager
}

void MacroPanelUI::StopExecution() {
  std::cout << "Stop execution requested" << std::endl;
  if (m_macroManager) {
    m_macroManager->StopExecution();
  }
  m_isExecuting = false;
  m_currentProgramIndex = -1;
}

// ============================================================================
// DETAILED RENDERING METHODS
// ============================================================================

void MacroPanelUI::RenderTableHeader() {
  ImGui::Columns(4, "ProgramColumns", true);
  ImGui::SetColumnWidth(0, 30);  // ID
  ImGui::SetColumnWidth(1, 30);  // Checkbox
  ImGui::SetColumnWidth(2, 150); // Name
  ImGui::SetColumnWidth(3, 60);  // Run button

  ImGui::Text("ID");
  ImGui::NextColumn();
  ImGui::Text("✓");
  ImGui::NextColumn();
  ImGui::Text("Program Name");
  ImGui::NextColumn();
  ImGui::Text("Run");
  ImGui::NextColumn();
  ImGui::Separator();
}

void MacroPanelUI::RenderProgramRow(int rowIndex, MacroProgramItem& item) {
  ImGui::PushID(rowIndex);

  // Highlight current executing row with background color
  if (m_isExecuting && rowIndex == m_currentProgramIndex) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f)); // Bright green text
    // Add background highlight
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, 25);
    ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
      IM_COL32(0, 100, 0, 80)); // Green background
  }

  // Create a single row with all elements using better spacing
  ImGui::Text("%d", rowIndex + 1);
  ImGui::SameLine(40);  // Position for checkbox

  ImGui::Checkbox("##selected", &item.selected);
  ImGui::SameLine(80);  // Position for program name

  // Show execution indicator next to program name
  if (m_isExecuting && rowIndex == m_currentProgramIndex) {
    ImGui::Text("▶ %s", item.name.c_str()); // Arrow indicator for currently running
  }
  else {
    ImGui::Text("%s", item.name.c_str());
  }

  // Calculate positions from the right side of the available space
  float contentWidth = ImGui::GetContentRegionAvail().x;
  float buttonStartPos = ImGui::GetCursorPosX() + contentWidth - 80; // 80 pixels for both buttons

  ImGui::SameLine(buttonStartPos); // Position for Run button

  // Run Button (>>) - Blue
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
  if (ImGui::Button((">>##run" + std::to_string(rowIndex)).c_str(), ImVec2(35, 22))) {
    ExecuteSingleProgram(item.name);
  }
  ImGui::PopStyleColor(2);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Run this program only");
  }

  ImGui::SameLine(); // Position for Remove button right next to Run button

  // Remove Button (X) - Red
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
  if (ImGui::Button(("X##remove" + std::to_string(rowIndex)).c_str(), ImVec2(30, 22))) {
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

  if (m_isExecuting && rowIndex == m_currentProgramIndex) {
    ImGui::PopStyleColor(); // Pop the text color
  }

  ImGui::PopID();
}

// New column-based row rendering method
void MacroPanelUI::RenderProgramRowColumns(int rowIndex, MacroProgramItem& item) {
  ImGui::PushID(rowIndex);

  // Highlight current executing row
  if (m_isExecuting && rowIndex == m_currentProgramIndex) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
  }

  // ID Column
  ImGui::Text("%d", rowIndex + 1);
  ImGui::NextColumn();

  // Checkbox Column
  ImGui::Checkbox("##selected", &item.selected);
  ImGui::NextColumn();

  // Program Name Column (truncated to fit)
  std::string displayName = item.name;
  if (displayName.length() > 8) {
    displayName = displayName.substr(0, 6) + "..";
  }

  if (m_isExecuting && rowIndex == m_currentProgramIndex) {
    ImGui::Text("▶%s", displayName.c_str());
  }
  else {
    ImGui::Text("%s", displayName.c_str());
  }
  ImGui::NextColumn();

  // Actions Column (Run and Remove buttons)
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
  if (ImGui::Button((">>##run" + std::to_string(rowIndex)).c_str(), ImVec2(25, 18))) {
    ExecuteSingleProgram(item.name);
  }
  ImGui::PopStyleColor();

  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
  if (ImGui::Button(("X##remove" + std::to_string(rowIndex)).c_str(), ImVec2(20, 18))) {
    if (m_macroManager && !m_currentMacroName.empty()) {
      m_macroManager->RemoveProgramFromMacro(m_currentMacroName, rowIndex);
      RefreshProgramItems();
      std::cout << "Removed program '" << item.name << "' from macro" << std::endl;
    }
  }
  ImGui::PopStyleColor();

  ImGui::NextColumn();

  if (m_isExecuting && rowIndex == m_currentProgramIndex) {
    ImGui::PopStyleColor();
  }

  ImGui::PopID();
}

void MacroPanelUI::RenderAvailablePrograms() {
  ImGui::Text("Add Program:");

  if (!m_availablePrograms.empty()) {
    std::vector<const char*> items;
    for (const auto& program : m_availablePrograms) {
      items.push_back(program.c_str());
    }

    ImGui::PushItemWidth(-1);
    ImGui::Combo("##ProgramSelect", &m_selectedProgramIndex, items.data(), items.size());
    ImGui::PopItemWidth();

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
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No programs available");
    if (ImGui::Button("Scan", ImVec2(-1, 0))) {
      if (m_macroManager) {
        m_macroManager->ScanForPrograms();
        RefreshAvailablePrograms();
        std::cout << "Rescanned for available programs" << std::endl;
      }
    }
  }
}

// ============================================================================
// MACRO VISUALIZATION METHOD
// ============================================================================

void MacroPanelUI::RenderMacroVisualization(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize) {
  if (m_programItems.empty()) return;

  float boxWidth = 80.0f;
  float boxHeight = 30.0f;
  float spacing = 20.0f;
  float startY = canvasPos.y + 20.0f;

  // Calculate total width needed and starting X position for centering
  float totalWidth = (boxWidth * m_programItems.size()) + (spacing * ((std::max)(0, (int)m_programItems.size() - 1)));
  float startX = canvasPos.x + (canvasSize.x - totalWidth) * 0.5f;

  for (int i = 0; i < m_programItems.size(); i++) {
    float x = startX + i * (boxWidth + spacing);
    float y = startY;

    // Determine box color based on state
    ImU32 boxColor;
    ImU32 textColor = IM_COL32(255, 255, 255, 255);

    if (m_isExecuting && i == m_currentProgramIndex) {
      boxColor = IM_COL32(0, 150, 0, 255); // Green for currently executing
    }
    else if (m_programItems[i].selected) {
      boxColor = IM_COL32(0, 100, 200, 255); // Blue for selected
    }
    else {
      boxColor = IM_COL32(80, 80, 80, 255); // Gray for unselected
    }

    // Draw program box
    ImVec2 boxMin = ImVec2(x, y);
    ImVec2 boxMax = ImVec2(x + boxWidth, y + boxHeight);

    drawList->AddRectFilled(boxMin, boxMax, boxColor, 5.0f);
    drawList->AddRect(boxMin, boxMax, IM_COL32(200, 200, 200, 255), 5.0f, 0, 1.5f);

    // Draw program name (truncated if too long)
    std::string displayName = m_programItems[i].name;
    if (displayName.length() > 10) {
      displayName = displayName.substr(0, 8) + "..";
    }

    ImVec2 textSize = ImGui::CalcTextSize(displayName.c_str());
    ImVec2 textPos = ImVec2(x + (boxWidth - textSize.x) * 0.5f, y + (boxHeight - textSize.y) * 0.5f);
    drawList->AddText(textPos, textColor, displayName.c_str());

    // Draw arrow to next program
    if (i < m_programItems.size() - 1) {
      float arrowStartX = x + boxWidth + 5;
      float arrowEndX = x + boxWidth + spacing - 5;
      float arrowY = y + boxHeight * 0.5f;

      drawList->AddLine(ImVec2(arrowStartX, arrowY), ImVec2(arrowEndX, arrowY),
        IM_COL32(150, 150, 150, 255), 2.0f);

      // Arrow head
      drawList->AddLine(ImVec2(arrowEndX - 5, arrowY - 3), ImVec2(arrowEndX, arrowY),
        IM_COL32(150, 150, 150, 255), 2.0f);
      drawList->AddLine(ImVec2(arrowEndX - 5, arrowY + 3), ImVec2(arrowEndX, arrowY),
        IM_COL32(150, 150, 150, 255), 2.0f);
    }
  }

  // Add legend at the bottom
  float legendY = startY + boxHeight + 40.0f;
  ImVec2 legendPos = ImVec2(canvasPos.x + 10, legendY);

  // Legend items
  drawList->AddRectFilled(ImVec2(legendPos.x, legendPos.y), ImVec2(legendPos.x + 15, legendPos.y + 15),
    IM_COL32(0, 150, 0, 255), 2.0f);
  drawList->AddText(ImVec2(legendPos.x + 20, legendPos.y), IM_COL32(200, 200, 200, 255), "Running");

  drawList->AddRectFilled(ImVec2(legendPos.x + 80, legendPos.y), ImVec2(legendPos.x + 95, legendPos.y + 15),
    IM_COL32(0, 100, 200, 255), 2.0f);
  drawList->AddText(ImVec2(legendPos.x + 100, legendPos.y), IM_COL32(200, 200, 200, 255), "Selected");

  drawList->AddRectFilled(ImVec2(legendPos.x + 170, legendPos.y), ImVec2(legendPos.x + 185, legendPos.y + 15),
    IM_COL32(80, 80, 80, 255), 2.0f);
  drawList->AddText(ImVec2(legendPos.x + 190, legendPos.y), IM_COL32(200, 200, 200, 255), "Unselected");
}



