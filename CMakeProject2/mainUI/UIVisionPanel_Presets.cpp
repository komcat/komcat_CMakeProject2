// UIVisionPanel_Presets.cpp - Preset management UI and operations with camera exposure
#include "UIVisionPanel.h"
#include "CameraManager.h"
#include "ICameraHardware.h"
#include <iostream>
#include <nlohmann/json.hpp>

// Get current camera exposure settings
UIVisionPanel::CameraExposurePreset UIVisionPanel::GetCurrentExposureSettings() const {
  CameraExposurePreset preset;
  preset.exposureTime = m_exposureTimeUI;
  preset.gain = m_gainUI;
  preset.autoExposure = m_autoExposureUI;
  preset.autoGain = m_autoGainUI;
  return preset;
}

// Apply camera exposure preset
void UIVisionPanel::ApplyExposurePreset(const CameraExposurePreset& preset) {
  m_exposureTimeUI = preset.exposureTime;
  m_gainUI = preset.gain;
  m_autoExposureUI = preset.autoExposure;
  m_autoGainUI = preset.autoGain;

  // Apply to actual camera
  ApplyExposureSettings();
}

// Convert exposure preset to JSON
nlohmann::json UIVisionPanel::ExposurePresetToJson(const CameraExposurePreset& preset) const {
  nlohmann::json j;
  j["exposureTime"] = preset.exposureTime;
  j["gain"] = preset.gain;
  j["autoExposure"] = preset.autoExposure;
  j["autoGain"] = preset.autoGain;
  return j;
}

// Convert JSON to exposure preset
UIVisionPanel::CameraExposurePreset UIVisionPanel::ExposurePresetFromJson(const nlohmann::json& j) const {
  CameraExposurePreset preset;

  if (j.contains("exposureTime")) preset.exposureTime = j["exposureTime"];
  if (j.contains("gain")) preset.gain = j["gain"];
  if (j.contains("autoExposure")) preset.autoExposure = j["autoExposure"];
  if (j.contains("autoGain")) preset.autoGain = j["autoGain"];

  return preset;
}

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

  // Enhanced quick preset buttons with camera exposure
  ImGui::Text("Quick Load:");
  if (ImGui::Button("Small + Fast", ImVec2(90, 25))) {
    LoadPresetByName("Small Circle");
    // Override with fast exposure for small objects
    CameraExposurePreset fastExposure;
    fastExposure.exposureTime = 1000.0f;
    fastExposure.gain = 1.0f;
    fastExposure.autoExposure = false;
    fastExposure.autoGain = false;
    ApplyExposurePreset(fastExposure);
  }

  ImGui::SameLine();
  if (ImGui::Button("Medium + Normal", ImVec2(100, 25))) {
    LoadPresetByName("Medium Circle");
    // Override with normal exposure
    CameraExposurePreset normalExposure;
    normalExposure.exposureTime = 10000.0f;
    normalExposure.gain = 1.0f;
    normalExposure.autoExposure = false;
    normalExposure.autoGain = false;
    ApplyExposurePreset(normalExposure);
  }

  if (ImGui::Button("Large + Bright", ImVec2(100, 25))) {
    LoadPresetByName("Large Circle");
    // Override with slow exposure for large objects
    CameraExposurePreset brightExposure;
    brightExposure.exposureTime = 50000.0f;
    brightExposure.gain = 2.0f;
    brightExposure.autoExposure = false;
    brightExposure.autoGain = false;
    ApplyExposurePreset(brightExposure);
  }
  ImGui::SameLine();
  if (ImGui::Button("Precision", ImVec2(60, 25))) {
    LoadPresetByName("High Precision");
  }

  ImGui::Spacing();

  // Preset actions
  if (ImGui::Button("Save Current + Camera", ImVec2(-1, 25))) {
    m_showNewPresetDialog = true;
    m_newPresetName = "Custom_" + std::to_string(m_presetManager->GetNextAvailableId());
    m_newPresetDescription = "Custom preset with vision parameters and camera exposure settings";
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

  // Show current camera exposure info
  ImGui::Spacing();
  ImGui::Text("Current Camera Settings:");
  ImGui::Text("Exposure: %.0fμs, Gain: %.1f", m_exposureTimeUI, m_gainUI);
  ImGui::Text("Auto: %s/%s",
    m_autoExposureUI ? "Exp" : "-",
    m_autoGainUI ? "Gain" : "-");

  // Show preset count
  ImGui::Text("Available: %zu presets", m_availablePresets.size());

  // Dialogs
  RenderNewPresetDialog();
  RenderDeleteConfirmDialog();
}

// Updated new preset dialog to show camera exposure info
void UIVisionPanel::RenderNewPresetDialog() {
  if (m_showNewPresetDialog) {
    ImGui::OpenPopup("Save New Preset");
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(400, 350));

  if (ImGui::BeginPopupModal("Save New Preset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Create a new preset from current parameters:");
    ImGui::Spacing();

    // Show what will be saved
    ImGui::Text("Will save:");
    ImGui::BulletText("Vision algorithm parameters");
    ImGui::BulletText("Camera exposure settings:");
    ImGui::Indent();
    ImGui::Text("- Exposure: %.0fμs", m_exposureTimeUI);
    ImGui::Text("- Gain: %.1f", m_gainUI);
    ImGui::Text("- Auto Exposure: %s", m_autoExposureUI ? "On" : "Off");
    ImGui::Text("- Auto Gain: %s", m_autoGainUI ? "On" : "Off");
    ImGui::Unindent();
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

    if (ImGui::InputTextMultiline("Description", descBuffer, sizeof(descBuffer), ImVec2(-1, 60))) {
      m_newPresetDescription = descBuffer;
    }

    ImGui::Spacing();

    // Buttons
    if (ImGui::Button("Save Preset", ImVec2(120, 0))) {
      if (!m_newPresetName.empty()) {
        if (SaveCurrentAsPreset(m_newPresetName, m_newPresetDescription)) {
          RefreshPresetList();
          m_showNewPresetDialog = false;
          ImGui::CloseCurrentPopup();
        }
      }
    }
    ImGui::SetItemDefaultFocus();
    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      m_showNewPresetDialog = false;
      ImGui::CloseCurrentPopup();
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
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      m_showDeleteConfirmDialog = false;
      ImGui::CloseCurrentPopup();
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

// Updated load preset method to include camera exposure
bool UIVisionPanel::LoadPreset(int presetId) {
  if (!m_presetManager || !m_circleDetector) {
    std::cerr << "[UIVisionPanel] Cannot load preset: components not initialized" << std::endl;
    return false;
  }

  nlohmann::json parameters;
  if (m_presetManager->LoadPreset(presetId, parameters)) {
    // Load vision algorithm parameters
    if (m_circleDetector->LoadParametersFromJson(parameters)) {

      // Load camera exposure settings if they exist
      if (parameters.contains("cameraExposure")) {
        CameraExposurePreset exposurePreset = ExposurePresetFromJson(parameters["cameraExposure"]);
        ApplyExposurePreset(exposurePreset);

        std::cout << "[UIVisionPanel] Loaded preset with camera exposure (ID: " << presetId << ")" << std::endl;
        std::cout << "  - Exposure: " << exposurePreset.exposureTime << "μs" << std::endl;
        std::cout << "  - Gain: " << exposurePreset.gain << std::endl;
        std::cout << "  - Auto Exposure: " << (exposurePreset.autoExposure ? "On" : "Off") << std::endl;
        std::cout << "  - Auto Gain: " << (exposurePreset.autoGain ? "On" : "Off") << std::endl;
      }
      else {
        std::cout << "[UIVisionPanel] Loaded preset without camera exposure settings (ID: " << presetId << ")" << std::endl;
      }

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

// Updated save preset method to include camera exposure
bool UIVisionPanel::SaveCurrentAsPreset(const std::string& name, const std::string& description) {
  if (!m_presetManager || !m_circleDetector) {
    std::cerr << "[UIVisionPanel] Cannot save preset: components not initialized" << std::endl;
    return false;
  }

  // Get vision algorithm parameters
  nlohmann::json parameters = m_circleDetector->GetParametersAsJson();

  // Add camera exposure settings to the preset
  CameraExposurePreset exposurePreset = GetCurrentExposureSettings();
  parameters["cameraExposure"] = ExposurePresetToJson(exposurePreset);

  int presetId = m_presetManager->SavePreset(name, parameters, description);

  if (presetId > 0) {
    std::cout << "[UIVisionPanel] Saved preset with camera exposure: " << name
      << " (ID: " << presetId << ")" << std::endl;
    std::cout << "  - Exposure: " << exposurePreset.exposureTime << "μs" << std::endl;
    std::cout << "  - Gain: " << exposurePreset.gain << std::endl;
    std::cout << "  - Auto Exposure: " << (exposurePreset.autoExposure ? "On" : "Off") << std::endl;
    std::cout << "  - Auto Gain: " << (exposurePreset.autoGain ? "On" : "Off") << std::endl;
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