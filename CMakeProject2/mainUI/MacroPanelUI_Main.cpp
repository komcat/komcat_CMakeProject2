// MacroPanelUI_Main.cpp - Core functionality and constructor
#include "MacroPanelUI.h"
#include "MacroManager.h"
#include "MachineBlockUI.h"
#include "MacroPanelCameraHandler.h"
#include "include/camera/CameraManager.h"
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <cctype>

// ============================================================================
// CONSTRUCTOR & CORE METHODS
// ============================================================================

MacroPanelUI::MacroPanelUI(CameraManager* cameraManager) {
  RefreshMacroList();
  RefreshAvailablePrograms();

  // Initialize camera handler
  m_cameraHandler = std::make_unique<MacroPanelCameraHandler>(cameraManager);

  // NEW: Initialize card renderer
  m_cardRenderer = std::make_unique<ProgramCardRenderer>();

  // Configure card renderer
  m_cardRenderer->SetCardHeight(85.0f);
  m_cardRenderer->SetCardSpacing(12.0f);
  m_cardRenderer->SetShowIcons(true);
  m_cardRenderer->SetShowConnectors(true);
}

// Add explicit destructor for std::unique_ptr with forward declaration
MacroPanelUI::~MacroPanelUI() = default;

void MacroPanelUI::SetMacroManager(MacroManager* macroManager) {
  m_macroManager = macroManager;
  RefreshMacroList();
}

void MacroPanelUI::SetMachineBlockUI(MachineBlockUI* blockUI) {
  m_blockUI = blockUI;
}

void MacroPanelUI::SetCameraManager(CameraManager* cameraManager) {
  if (m_cameraHandler) {
    m_cameraHandler->SetCameraManager(cameraManager);
  }
}

// ============================================================================
// MAIN RENDER METHOD - RENAMED to avoid conflicts
// ============================================================================

void MacroPanelUI::RenderMacroPanel() {
  if (!m_showWindow) return;

  // Get content region for 3-column layout with new proportions
  ImVec2 contentRegion = ImGui::GetContentRegionAvail();
  float leftWidth = contentRegion.x * 0.15f;   // 33% for left panel (was 25%)
  float middleWidth = contentRegion.x * 0.25f; // 25% for middle panel (was 50%) 
  float rightWidth = contentRegion.x * 0.55f;  // 42% for right panel (was 25%)

  // === LEFT PANEL ===
  ImGui::BeginChild("MacroLeftPanel", ImVec2(leftWidth, 0), true);
  RenderMacroLeftPanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // === MIDDLE PANEL ===
  ImGui::BeginChild("MacroMiddlePanel", ImVec2(middleWidth, 0), true);
  RenderMacroMiddlePanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // === RIGHT PANEL  ===
  ImGui::BeginChild("MacroRightPanel", ImVec2(rightWidth, 0), true);
  RenderMacroRightPanel();
  ImGui::EndChild();


  // === RENDER FILE DIALOGS (Modal windows) ===
  RenderFileDialog();

  // QUICK TEST: Render prompts through MachineBlockUI
  if (m_blockUI) {
    m_blockUI->RenderFeedbackAndPrompts();
  }
}

// ============================================================================
// HELPER METHODS
// ============================================================================

void MacroPanelUI::RefreshMacroList() {
  m_availableMacros = GetAvailableMacros();
}

void MacroPanelUI::RefreshProgramItems() {
  m_programItems.clear();

  if (m_currentMacroName.empty() || !m_macroManager) return;

  auto* macro = m_macroManager->GetMacro(m_currentMacroName);
  if (!macro) return;

  for (int i = 0; i < macro->programs.size(); i++) {
    MacroProgramItem item;
    item.id = i;
    item.name = macro->programs[i].name;
    item.filePath = macro->programs[i].filePath;
    item.selected = false;
    m_programItems.push_back(item);
  }
}

void MacroPanelUI::RefreshAvailablePrograms() {
  m_availablePrograms = GetAvailablePrograms();
}

std::vector<std::string> MacroPanelUI::GetAvailableMacros() {
  if (m_macroManager) {
    return m_macroManager->GetMacroNames();
  }
  return {};
}

std::vector<std::string> MacroPanelUI::GetAvailablePrograms() {
  if (m_macroManager) {
    return m_macroManager->GetProgramNames();
  }
  return {};
}

void MacroPanelUI::RenderPrompts() {
  // Render prompt UI if available
  if (m_promptUI) {
    m_promptUI->Render();
  }
}



