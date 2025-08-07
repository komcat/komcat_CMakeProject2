// UIVisionPanel_Presets.cpp - Preset management UI and operations
#include "UIVisionPanel.h"
#include <iostream>

void UIVisionPanel::RenderPresetControls() {
  ImGui::Text("Preset Management");

  if (!m_presetManager || !m_presetManager->IsInitialized()) {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Preset manager not available");
    if (ImGui::Button("Retry Initialize", ImVec2(-1, 25))) {
      InitializePresetManager();
    }
    return;
  }

  // Preset selector dropdown
  const char* previewText = "Select preset...";
  if (m_selectedPresetId >= 0) {
    for (const auto& preset : m_availablePresets) {
      if (preset.id == m_selectedPresetId) {
        previewText = preset.name.c_str();
        break;
      }
    }
  }

  if (ImGui::BeginCombo("##PresetSelector", previewText, ImGuiComboFlags_PopupAlignLeft)) {
    for (const auto& preset : m_availablePresets) {
      bool isSelected = (m_selectedPresetId == preset.id);
      std::string label = preset.name;
      if (preset.isDefault) label += " (Default)";

      if (ImGui::Selectable(label.c_str(), isSelected)) {
        if (LoadPreset(preset.id)) {
          m_selectedPresetId = preset.id;
        }
      }

      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  // Quick preset buttons (enhanced with database loading)
  ImGui::Text("Quick Load:");
  if (ImGui::Button("Small", ImVec2(50, 25))) {
    LoadPresetByName("Small Circle");
  }
  ImGui::SameLine();
  if (ImGui::Button("Medium", ImVec2(50, 25))) {
    LoadPresetByName("Medium Circle");
  }
  ImGui::SameLine();
  if (ImGui::Button("Large", ImVec2(50, 25))) {
    LoadPresetByName("Large Circle");
  }
  ImGui::SameLine();
  if (ImGui::Button("Precision", ImVec2(60, 25))) {
    LoadPresetByName("High Precision");
  }

  ImGui::Spacing();

  // Preset actions
  if (ImGui::Button("Save Current", ImVec2(-1, 25))) {
    m_showNewPresetDialog = true;
    m_newPresetName = "Custom_" + std::to_string(m_presetManager->GetNextAvailableId());
    m_newPresetDescription = "Custom preset created from current parameters";
  }

  // Delete button (only for custom presets)
  bool canDelete = false;
  if (m_selectedPresetId >= 0) {
    for (const auto& preset : m_availablePresets) {
      if (preset.id == m_selectedPresetId && !preset.isDefault) {
        canDelete = true;
        break;
      }
    }
  }

  if (!canDelete) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }

  if (ImGui::Button("Delete Selected", ImVec2(-1, 25))) {
    if (canDelete) {
      m_presetToDelete = m_selectedPresetId;
      m_showDeleteConfirmDialog = true;
    }
  }

  if (!canDelete) {
    ImGui::PopStyleVar();
  }

  // Show preset count
  ImGui::Text("Available: %zu presets", m_availablePresets.size());

  // Dialogs
  RenderNewPresetDialog();
  RenderDeleteConfirmDialog();
}



// Fixed version of RenderNewPresetDialog
void UIVisionPanel::RenderNewPresetDialog() {
  if (m_showNewPresetDialog) {
    ImGui::OpenPopup("Save New Preset");
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(400, 250));

  if (ImGui::BeginPopupModal("Save New Preset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Create a new preset from current parameters:");
    ImGui::Spacing();

    // Preset name input
    char nameBuffer[256];
    strncpy(nameBuffer, m_newPresetName.c_str(), sizeof(nameBuffer) - 1);
    nameBuffer[sizeof(nameBuffer) - 1] = '\0';

    if (ImGui::InputText("Preset Name", nameBuffer, sizeof(nameBuffer))) {
      m_newPresetName = nameBuffer;
    }

    // Description input
    char descBuffer[512];
    strncpy(descBuffer, m_newPresetDescription.c_str(), sizeof(descBuffer) - 1);
    descBuffer[sizeof(descBuffer) - 1] = '\0';

    if (ImGui::InputTextMultiline("Description", descBuffer, sizeof(descBuffer), ImVec2(-1, 80))) {
      m_newPresetDescription = descBuffer;
    }

    ImGui::Spacing();

    // Buttons
    if (ImGui::Button("Save", ImVec2(120, 0))) {
      if (!m_newPresetName.empty()) {
        if (SaveCurrentAsPreset(m_newPresetName, m_newPresetDescription)) {
          RefreshPresetList();
          m_showNewPresetDialog = false;
          ImGui::CloseCurrentPopup();  // <- THIS IS THE KEY FIX!
        }
      }
    }
    ImGui::SetItemDefaultFocus();
    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      m_showNewPresetDialog = false;
      ImGui::CloseCurrentPopup();  // <- THIS IS THE KEY FIX!
    }

    if (m_newPresetName.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Name cannot be empty");
    }

    ImGui::EndPopup();
  }
}

// Also fix the delete confirmation dialog the same way
void UIVisionPanel::RenderDeleteConfirmDialog() {
  if (m_showDeleteConfirmDialog) {
    ImGui::OpenPopup("Confirm Delete");
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Are you sure you want to delete this preset?");
    ImGui::Text("This action cannot be undone.");
    ImGui::Spacing();

    if (ImGui::Button("Delete", ImVec2(120, 0))) {
      if (m_presetManager->DeletePreset(m_presetToDelete)) {
        RefreshPresetList();
        if (m_selectedPresetId == m_presetToDelete) {
          m_selectedPresetId = -1;
        }
        std::cout << "[UIVisionPanel] Deleted preset ID: " << m_presetToDelete << std::endl;
      }
      m_showDeleteConfirmDialog = false;
      ImGui::CloseCurrentPopup();  // <- ADD THIS TOO!
    }
    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      m_showDeleteConfirmDialog = false;
      ImGui::CloseCurrentPopup();  // <- ADD THIS TOO!
    }

    ImGui::EndPopup();
  }
}



void UIVisionPanel::RefreshPresetList() {
  if (m_presetManager && m_presetManager->IsInitialized()) {
    m_availablePresets = m_presetManager->GetAllPresets();
    std::cout << "[UIVisionPanel] Refreshed preset list: "
      << m_availablePresets.size() << " presets available" << std::endl;
  }
}

bool UIVisionPanel::LoadPreset(int presetId) {
  if (!m_presetManager || !m_circleDetector) {
    std::cerr << "[UIVisionPanel] Cannot load preset: components not initialized" << std::endl;
    return false;
  }

  nlohmann::json parameters;
  if (m_presetManager->LoadPreset(presetId, parameters)) {
    if (m_circleDetector->LoadParametersFromJson(parameters)) {
      std::cout << "[UIVisionPanel] Successfully loaded preset ID: " << presetId << std::endl;
      return true;
    }
    else {
      std::cerr << "[UIVisionPanel] Failed to apply preset parameters" << std::endl;
    }
  }
  else {
    std::cerr << "[UIVisionPanel] Failed to load preset: " << m_presetManager->GetLastError() << std::endl;
  }
  return false;
}

bool UIVisionPanel::SaveCurrentAsPreset(const std::string& name, const std::string& description) {
  if (!m_presetManager || !m_circleDetector) {
    std::cerr << "[UIVisionPanel] Cannot save preset: components not initialized" << std::endl;
    return false;
  }

  nlohmann::json parameters = m_circleDetector->GetParametersAsJson();
  int presetId = m_presetManager->SavePreset(name, parameters, description);

  if (presetId > 0) {
    std::cout << "[UIVisionPanel] Saved current parameters as preset: " << name << " (ID: " << presetId << ")" << std::endl;
    return true;
  }
  else {
    std::cerr << "[UIVisionPanel] Failed to save preset: " << m_presetManager->GetLastError() << std::endl;
    return false;
  }
}

bool UIVisionPanel::LoadPresetByName(const std::string& name) {
  for (const auto& preset : m_availablePresets) {
    if (preset.name == name) {
      if (LoadPreset(preset.id)) {
        m_selectedPresetId = preset.id;
        return true;
      }
    }
  }
  std::cout << "[UIVisionPanel] Preset not found: " << name << std::endl;
  return false;
}