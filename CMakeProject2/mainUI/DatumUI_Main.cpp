// DatumUI_Main.cpp - Core window management and main rendering
#include "DatumUI.h"

// =============================================================================
// CONSTRUCTION & LIFECYCLE
// =============================================================================

DatumUI::DatumUI()
  : m_referenceManager(std::make_unique<ProductReferenceManager>()) {
  // Initialize UI state
  ClearInputBuffers();
}

DatumUI::~DatumUI() {
  // Cleanup handled by RAII
}

// =============================================================================
// MAIN UI INTERFACE
// =============================================================================

void DatumUI::RenderUI() {
  if (!m_showWindow) {
    return;
  }

  // Update message display timers
  UpdateMessageDisplays();

  ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
  ImGui::Begin("Datum Reference System", &m_showWindow);

  // Display error/success messages at top
  if (!m_errorMessage.empty() && m_errorDisplayTime > 0.0f) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
    ImGui::Text("Error: %s", m_errorMessage.c_str());
    ImGui::PopStyleColor();
    ImGui::Separator();
  }

  if (!m_successMessage.empty() && m_successDisplayTime > 0.0f) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
    ImGui::Text("Success: %s", m_successMessage.c_str());
    ImGui::PopStyleColor();
    ImGui::Separator();
  }

  // Calculate panel layout
  ImVec2 contentSize = ImGui::GetContentRegionAvail();
  float leftWidth = contentSize.x * 0.25f;    // 25% for product list and controls
  float middleWidth = contentSize.x * 0.40f;  // 40% for point/edge management
  float rightWidth = contentSize.x * 0.35f;   // 35% for visual display

  // Left Panel - Product List and Controls
  ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, contentSize.y), true);
  RenderLeftPanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // Middle Panel - Point/Edge Management and Datum Creation
  ImGui::BeginChild("MiddlePanel", ImVec2(middleWidth, contentSize.y), true);
  RenderMiddlePanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // Right Panel - Visual Display
  ImGui::BeginChild("RightPanel", ImVec2(rightWidth, contentSize.y), true);
  RenderRightPanel();
  ImGui::EndChild();

  // Render dialogs
  RenderNewProductDialog();
  RenderAddPointDialog();           // Enhanced with action types
  RenderEditPointDialog();
  RenderChangeActionTypeDialog();   // NEW: Action type management
  RenderDatumCreationDialog();
  RenderDeleteConfirmDialog();

  ImGui::End();
}