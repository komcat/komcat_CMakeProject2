// MacroPanelUI_Panels.cpp - Cleaned panel rendering methods
#include "MacroPanelUI.h"
#include "MacroManager.h"
#include "MacroPanelCameraHandler.h"
#include <algorithm>
#include <iostream>

// ============================================================================
// LEFT PANEL - Macro Management & Program Addition
// ============================================================================

void MacroPanelUI::RenderMacroLeftPanel() {
  ImGui::Text("Macro Management");
  ImGui::Separator();

  RenderMacroList();
  ImGui::Spacing();
  RenderNewMacroSection();
  ImGui::Spacing();
  RenderLoadSaveSection();
  ImGui::Spacing();
  ImGui::Separator();
  RenderAvailablePrograms();
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
      if (m_macroManager->CreateMacro(m_macroNameBuffer, m_macroDescBuffer)) {
        // Auto-save the newly created macro
        std::string fileName = "macros/" + std::string(m_macroNameBuffer) + "_macro.json";
        m_macroManager->SaveMacro(m_macroNameBuffer, fileName);
        std::cout << "Created and saved macro: " << m_macroNameBuffer << std::endl;

        // Set as current macro and refresh lists
        m_currentMacroName = m_macroNameBuffer;
        RefreshMacroList();
        RefreshProgramItems();

        // Select the new macro in the dropdown
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

  // Load button
  if (ImGui::Button("Load", ImVec2(-1, 0))) {
    m_showLoadDialog = true;
    NavigateToDirectory("macros");
    RefreshDirectoryListing();
    m_selectedFileIndex = -1;
  }

  // Save button
  if (ImGui::Button("Save", ImVec2(-1, 0))) {
    if (!m_currentMacroName.empty()) {
      m_showSaveDialog = true;
      NavigateToDirectory("macros");
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
// MIDDLE PANEL - Program Sequence & Execution Controls
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

void MacroPanelUI::RenderProgramTable() {
  if (m_programItems.empty()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No programs in this macro");
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Add programs from the left panel");
    return;
  }

  ImGui::Text("Programs (%zu):", m_programItems.size());
  ImGui::Separator();

  // Render each program row
  for (int i = 0; i < m_programItems.size(); i++) {
    RenderProgramRow(i, m_programItems[i]);
    if (i < m_programItems.size() - 1) {
      ImGui::Spacing();
    }
  }
}

// ============================================================================
// RIGHT PANEL - Properties & Status
// ============================================================================

void MacroPanelUI::RenderMacroRightPanel() {
  // Camera feed display - MOVED TO TOP
  if (m_cameraHandler) {
    m_cameraHandler->RenderCameraCanvas();
  }

  ImGui::Spacing();
  ImGui::Separator();

  ImGui::Text("Properties");
  ImGui::Separator();

  RenderMacroProperties();
  ImGui::Spacing();
  RenderExecutionStatus();
}


void MacroPanelUI::RenderMacroProperties() {
  if (m_currentMacroName.empty()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No macro selected");
    return;
  }

  ImGui::Text("Macro: %s", m_currentMacroName.c_str());
  ImGui::Text("Programs: %zu", m_programItems.size());

  // Count selected programs
  int selectedCount = 0;
  for (const auto& item : m_programItems) {
    if (item.selected) selectedCount++;
  }
  ImGui::Text("Selected: %d", selectedCount);

  // Show execution preview if programs are selected
  if (selectedCount > 0) {
    ImGui::Spacing();
    ImGui::Text("Will Execute:");
    ImGui::Indent();
    for (int i = 0; i < m_programItems.size(); i++) {
      if (m_programItems[i].selected) {
        ImGui::BulletText("%s", m_programItems[i].name.c_str());
      }
    }
    ImGui::Unindent();
  }
}


// ============================================================================
// UPDATED EXECUTION STATUS RENDERING WITH PROPER PROGRESS
// ============================================================================

void MacroPanelUI::RenderExecutionStatus() {
  // CRITICAL: Sync state with MacroManager first
  SyncExecutionState();

  ImGui::Text("Execution Status:");

  if (m_macroManager && m_macroManager->IsExecuting()) {
    // Use MacroManager's state as authoritative
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Running");

    std::string currentMacro = m_macroManager->GetCurrentMacro();
    if (!currentMacro.empty()) {
      ImGui::Text("Macro: %s", currentMacro.c_str());
    }

    // Calculate proper progress based on SELECTED programs, not total programs
    int selectedCount = 0;
    int selectedCompleted = 0;

    for (int i = 0; i < m_programItems.size(); i++) {
      if (m_programItems[i].selected) {
        selectedCount++;
        if (i < m_currentProgramIndex) {
          selectedCompleted++;
        }
      }
    }

    if (selectedCount > 0) {
      // Show current program
      if (m_currentProgramIndex >= 0 && m_currentProgramIndex < m_programItems.size()) {
        ImGui::Text("Current: %s", m_programItems[m_currentProgramIndex].name.c_str());
      }

      // Calculate correct progress: completed programs / total selected programs
      float progress = static_cast<float>(selectedCompleted) / static_cast<float>(selectedCount);

      // If we're currently executing a program, add partial progress
      if (m_currentProgramIndex >= 0 && m_programItems[m_currentProgramIndex].selected) {
        progress = static_cast<float>(selectedCompleted + 1) / static_cast<float>(selectedCount);
      }

      ImGui::Text("Progress: %d/%d selected programs", selectedCompleted, selectedCount);
      ImGui::ProgressBar(progress, ImVec2(-1, 0));

      // Show percentage
      ImGui::SameLine();
      ImGui::Text("%.0f%%", progress * 100.0f);
    }

  }
  else if (m_isPaused) {
    ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.2f, 1.0f), "Paused");
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Ready");

    // Show what will be executed when ready
    int selectedCount = 0;
    for (const auto& item : m_programItems) {
      if (item.selected) selectedCount++;
    }

    if (selectedCount > 0) {
      ImGui::Text("Ready to execute %d programs", selectedCount);
    }
  }
}