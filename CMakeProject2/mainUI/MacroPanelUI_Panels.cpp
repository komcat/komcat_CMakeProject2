// MacroPanelUI_Panels.cpp - Panel rendering methods
#include "MacroPanelUI.h"
#include "MacroManager.h"
#include "MacroPanelCameraHandler.h"
#include <algorithm>
#include <iostream>

// ============================================================================
// LEFT PANEL - Macro List & Controls - RENAMED to avoid conflicts
// ============================================================================

void MacroPanelUI::RenderMacroLeftPanel() {
  ImGui::Text("Macro Management");
  ImGui::Separator();

  RenderMacroList();
  ImGui::Spacing();
  RenderNewMacroSection();
  ImGui::Spacing();
  RenderLoadSaveSection();

  // Move program selection from right panel to here
  ImGui::Spacing();
  ImGui::Separator();
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

void MacroPanelUI::RenderMacroList() {
  ImGui::Text("Select Macro:");

  if (!m_availableMacros.empty()) {
    std::vector<const char*> items;
    for (const auto& macro : m_availableMacros) {
      items.push_back(macro.c_str());
    }

    if (ImGui::Combo("##MacroSelect", &m_selectedMacroIndex, items.data(), items.size())) {
      if (m_selectedMacroIndex >= 0 && m_selectedMacroIndex < m_availableMacros.size()) {
        m_currentMacroName = m_availableMacros[m_selectedMacroIndex];
        RefreshProgramItems();
        std::cout << "Selected macro: " << m_currentMacroName << std::endl;
      }
    }
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No macros available");
  }

  if (ImGui::Button("Refresh", ImVec2(-1, 0))) {
    // Force scan for programs first
    if (m_macroManager) {
      m_macroManager->ScanForPrograms();
    }
    RefreshMacroList();
    RefreshAvailablePrograms();
    std::cout << "Refreshed macro and program lists" << std::endl;
  }
}

void MacroPanelUI::RenderNewMacroSection() {
  ImGui::Text("Create New:");

  ImGui::Text("Name:");
  ImGui::InputText("##MacroName", m_macroNameBuffer, sizeof(m_macroNameBuffer));

  ImGui::Text("Description:");
  ImGui::InputTextMultiline("##MacroDesc", m_macroDescBuffer, sizeof(m_macroDescBuffer), ImVec2(-1, 60));

  if (ImGui::Button("Create", ImVec2(-1, 0))) {
    if (strlen(m_macroNameBuffer) > 0 && m_macroManager) {
      // Create the macro
      if (m_macroManager->CreateMacro(m_macroNameBuffer, m_macroDescBuffer)) {
        // Auto-save the newly created macro
        std::string fileName = "macros/" + std::string(m_macroNameBuffer) + "_macro.json";
        m_macroManager->SaveMacro(m_macroNameBuffer, fileName);
        std::cout << "Created and saved macro: " << m_macroNameBuffer << std::endl;

        // Set as current macro and refresh lists
        m_currentMacroName = m_macroNameBuffer;
        RefreshMacroList();
        RefreshProgramItems();

        // Find the new macro in the list and select it
        for (int i = 0; i < m_availableMacros.size(); i++) {
          if (m_availableMacros[i] == m_currentMacroName) {
            m_selectedMacroIndex = i;
            break;
          }
        }
      }

      // Clear input buffers
      memset(m_macroNameBuffer, 0, sizeof(m_macroNameBuffer));
      memset(m_macroDescBuffer, 0, sizeof(m_macroDescBuffer));
    }
  }
}


void MacroPanelUI::RenderLoadSaveSection() {
  ImGui::Text("File Operations:");

  // Load button - opens file dialog
  if (ImGui::Button("Load", ImVec2(-1, 0))) {
    m_showLoadDialog = true;
    NavigateToDirectory("macros"); // Start in macros directory
    RefreshDirectoryListing();
    m_selectedFileIndex = -1;
  }

  // Save button - opens save dialog
  if (ImGui::Button("Save", ImVec2(-1, 0))) {
    if (!m_currentMacroName.empty()) {
      m_showSaveDialog = true;
      NavigateToDirectory("macros"); // Start in macros directory
      RefreshDirectoryListing();

      // Pre-fill with default name
      std::string defaultName = m_currentMacroName + "_macro.json";
      strncpy_s(m_saveFileName, sizeof(m_saveFileName), defaultName.c_str(), sizeof(m_saveFileName) - 1);
    }
    else {
      std::cout << "No macro selected to save" << std::endl;
    }
  }
}
// ============================================================================
// MIDDLE PANEL - Program Items Table - RENAMED to avoid conflicts
// ============================================================================

void MacroPanelUI::RenderMacroMiddlePanel() {
  ImGui::Text("Program Sequence");
  ImGui::Separator();

  // Show current macro name
  if (!m_currentMacroName.empty()) {
    ImGui::Text("Macro: %s", m_currentMacroName.c_str());
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No macro selected");
  }

  ImGui::Spacing();
  RenderExecutionControls();
  ImGui::Spacing();
  RenderProgramTable();
}

void MacroPanelUI::RenderExecutionControls() {
  // Play, Pause, Stop buttons
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
  if (ImGui::Button("Play", ImVec2(60, 30))) {
    PlayExecution();
  }
  ImGui::PopStyleColor(2);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Execute selected programs in sequence");
  }

  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.7f, 0.3f, 1.0f));
  if (ImGui::Button("Pause", ImVec2(60, 30))) {
    PauseExecution();
  }
  ImGui::PopStyleColor(2);

  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
  if (ImGui::Button("Stop", ImVec2(60, 30))) {
    StopExecution();
  }
  ImGui::PopStyleColor(2);

  // Enhanced execution status
  ImGui::SameLine();
  if (m_isExecuting) {
    if (m_currentProgramIndex >= 0 && m_currentProgramIndex < m_programItems.size()) {
      ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Running: %s (%d/%d)",
        m_programItems[m_currentProgramIndex].name.c_str(),
        m_currentProgramIndex + 1,
        (int)m_programItems.size());
    }
    else {
      ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Starting...");
    }
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Stopped");
  }
}

// COMPLETE REPLACEMENT for RenderProgramTable() - No columns, no crashes
void MacroPanelUI::RenderProgramTable() {
  if (m_programItems.empty()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No programs in this macro");
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Add programs from the left panel");
    return;
  }

  // Simple header without columns
  ImGui::Text("Programs in Macro:");
  ImGui::Separator();

  // Render each program as a simple row without columns
  for (int i = 0; i < m_programItems.size(); i++) {
    RenderProgramRowSimple(i, m_programItems[i]);
    if (i < m_programItems.size() - 1) {
      ImGui::Spacing();
    }
  }
}

// Safe row rendering without columns or complex layouts
void MacroPanelUI::RenderProgramRowSimple(int rowIndex, MacroProgramItem& item) {
  ImGui::PushID(rowIndex);

  // Create a horizontal layout manually without columns
  float windowWidth = ImGui::GetContentRegionAvail().x;
  float buttonWidth = 30.0f;
  float checkboxWidth = 20.0f;
  float idWidth = 25.0f;
  float spacingWidth = 10.0f;

  // Background highlight for currently executing program
  if (m_isExecuting && rowIndex == m_currentProgramIndex) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size = ImVec2(windowWidth, 25.0f);
    ImGui::GetWindowDrawList()->AddRectFilled(pos,
      ImVec2(pos.x + size.x, pos.y + size.y),
      IM_COL32(0, 100, 0, 80)); // Green background
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
  }

  // Row ID
  ImGui::Text("%d.", rowIndex + 1);

  // Checkbox (same line)
  ImGui::SameLine(idWidth);
  ImGui::Checkbox("##selected", &item.selected);

  // Program name (same line)  
  ImGui::SameLine(idWidth + checkboxWidth + spacingWidth);
  std::string displayName = item.name;
  if (m_isExecuting && rowIndex == m_currentProgramIndex) {
    displayName = "▶ " + displayName;
  }

  // Truncate if too long
  float remainingWidth = windowWidth - (idWidth + checkboxWidth + spacingWidth + 2 * buttonWidth + 2 * spacingWidth);
  ImVec2 textSize = ImGui::CalcTextSize(displayName.c_str());
  if (textSize.x > remainingWidth) {
    // Truncate the display name
    while (displayName.length() > 3 && ImGui::CalcTextSize((displayName + "..").c_str()).x > remainingWidth) {
      displayName.pop_back();
    }
    displayName += "..";
  }

  ImGui::Text("%s", displayName.c_str());

  // Run button (same line, positioned from right)
  float runButtonPos = windowWidth - (2 * buttonWidth + spacingWidth);
  ImGui::SameLine(runButtonPos);

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
  if (ImGui::Button((">>##run" + std::to_string(rowIndex)).c_str(), ImVec2(buttonWidth, 20))) {
    ExecuteSingleProgram(item.name);
  }
  ImGui::PopStyleColor(2);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Run this program only");
  }

  // Remove button (same line)
  ImGui::SameLine(windowWidth - buttonWidth);

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
  if (ImGui::Button(("X##remove" + std::to_string(rowIndex)).c_str(), ImVec2(buttonWidth, 20))) {
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

  // Pop the highlighting color if it was set
  if (m_isExecuting && rowIndex == m_currentProgramIndex) {
    ImGui::PopStyleColor();
  }

  ImGui::PopID();
}

// Alternative: Ultra-simple vertical layout (safest option)
void MacroPanelUI::RenderProgramTableVertical() {
  if (m_programItems.empty()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No programs in this macro");
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Add programs from the left panel");
    return;
  }

  ImGui::Text("Programs (%zu):", m_programItems.size());
  ImGui::Separator();

  for (int i = 0; i < m_programItems.size(); i++) {
    ImGui::PushID(i);

    // Simple vertical layout for each program
    if (m_isExecuting && i == m_currentProgramIndex) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
      ImGui::Text("▶ %d. %s (RUNNING)", i + 1, m_programItems[i].name.c_str());
      ImGui::PopStyleColor();
    }
    else {
      ImGui::Text("%d. %s", i + 1, m_programItems[i].name.c_str());
    }

    ImGui::SameLine();
    ImGui::Checkbox("##selected", &m_programItems[i].selected);

    ImGui::SameLine();
    if (ImGui::Button("Run")) {
      ExecuteSingleProgram(m_programItems[i].name);
    }

    ImGui::SameLine();
    if (ImGui::Button("Remove")) {
      if (m_macroManager && !m_currentMacroName.empty()) {
        m_macroManager->RemoveProgramFromMacro(m_currentMacroName, i);
        RefreshProgramItems();
        std::cout << "Removed program '" << m_programItems[i].name << "' from macro" << std::endl;
        ImGui::PopID();
        break; // Exit loop since we modified the vector
      }
    }

    if (i < m_programItems.size() - 1) {
      ImGui::Separator();
    }

    ImGui::PopID();
  }
}

// Safer version of the column-based row rendering
void MacroPanelUI::RenderProgramRowColumnsSafe(int rowIndex, MacroProgramItem& item) {
  ImGui::PushID(rowIndex);

  // Highlight current executing row
  if (m_isExecuting && rowIndex == m_currentProgramIndex) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
  }

  // ID Column - with safety check
  ImGui::Text("%d", rowIndex + 1);
  if (ImGui::GetColumnsCount() > 1) ImGui::NextColumn();

  // Checkbox Column
  ImGui::Checkbox("##selected", &item.selected);
  if (ImGui::GetColumnsCount() > 1) ImGui::NextColumn();

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
  if (ImGui::GetColumnsCount() > 1) ImGui::NextColumn();

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

  if (ImGui::GetColumnsCount() > 1) ImGui::NextColumn();

  if (m_isExecuting && rowIndex == m_currentProgramIndex) {
    ImGui::PopStyleColor();
  }

  ImGui::PopID();
}
// ============================================================================
// RIGHT PANEL - Properties & Actions - RENAMED to avoid conflicts
// ============================================================================

void MacroPanelUI::RenderMacroRightPanel() {
  ImGui::Text("Properties");
  ImGui::Separator();

  RenderMacroProperties();
  ImGui::Spacing();
  RenderExecutionStatus();

  // Camera feed display
  ImGui::Spacing();
  ImGui::Separator();
  if (m_cameraHandler) {
    m_cameraHandler->RenderCameraCanvas();
  }
}

void MacroPanelUI::RenderMacroProperties() {
  if (m_currentMacroName.empty()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No macro selected");
    return;
  }

  ImGui::Text("Macro: %s", m_currentMacroName.c_str());
  ImGui::Text("Programs: %zu", m_programItems.size());

  int selectedCount = 0;
  for (const auto& item : m_programItems) {
    if (item.selected) selectedCount++;
  }
  ImGui::Text("Selected: %d", selectedCount);
}

void MacroPanelUI::RenderExecutionStatus() {
  ImGui::Text("Status:");
  if (m_isExecuting) {
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Executing");
    if (m_currentProgramIndex >= 0) {
      ImGui::Text("Step: %d/%zu", m_currentProgramIndex + 1, m_programItems.size());
    }
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Ready");
  }
}


//void MacroPanelUI::RenderLoadSaveSection() {
//  ImGui::Text("File Operations:");
//
//  // Load button (full width) - NOW OPENS DIALOG
//  if (ImGui::Button("Load", ImVec2(-1, 0))) {
//    m_showLoadDialog = true;
//    RefreshMacroFiles();  // Refresh file list when dialog opens
//    std::cout << "Opening load dialog..." << std::endl;
//  }
//
//  // Save button (full width, UNDER Load button)
//  if (ImGui::Button("Save", ImVec2(-1, 0))) {
//    if (!m_currentMacroName.empty() && m_macroManager) {
//      std::string fileName = "macros/" + m_currentMacroName + "_macro.json";
//      m_macroManager->SaveMacro(m_currentMacroName, fileName);
//      std::cout << "Saved macro: " << m_currentMacroName << " to " << fileName << std::endl;
//    }
//    else {
//      std::cout << "No macro selected to save" << std::endl;
//    }
//  }
//}