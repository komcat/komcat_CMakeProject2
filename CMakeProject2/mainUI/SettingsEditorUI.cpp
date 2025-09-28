// SettingsEditorUI.cpp
#include "SettingsEditorUI.h"
#include "logger.h"
#include <algorithm>

SettingsEditorUI::SettingsEditorUI()
  : IImguiWindowUI() // Explicitly call base constructor
{
  m_logger = Logger::GetInstance();
  InitializeCategories();

  // Load all categories from database
  for (auto& category : m_categories) {
    LoadCategoryFromDatabase(category);
  }

  m_logger->LogInfo("SettingsEditorUI: Initialized with " +
    std::to_string(m_categories.size()) + " categories");
}


//SettingsEditorUI::~SettingsEditorUI() {
//  m_logger->LogInfo("SettingsEditorUI: Destroyed");
//}

// IImguiWindowUI interface implementations
void SettingsEditorUI::Show() {
  m_isVisible = true;
  UpdateStatus("Settings editor opened");
}

void SettingsEditorUI::Hide() {
  m_isVisible = false;
  UpdateStatus("Settings editor closed");
}

bool SettingsEditorUI::IsVisible() const {
  return m_isVisible;
}

const std::string& SettingsEditorUI::GetName() const {
  return m_windowName;
}

void SettingsEditorUI::InitializeCategories() {
  m_categories.clear();

  // Spec Settings Category
  SettingsCategory specCategory;
  specCategory.name = "spec_settings";
  specCategory.displayName = "Specification Settings";

  // Spec threshold
  SettingsParameter specThreshold;
  specThreshold.name = "spec_threshold_ua";
  specThreshold.displayName = "Spec Threshold";
  specThreshold.type = "float";
  specThreshold.value = "800";
  specThreshold.unit = "µA";
  specThreshold.minValue = 0.1f;
  specThreshold.maxValue = 10000.0f;
  specCategory.parameters.push_back(specThreshold);

  // Spec unit
  SettingsParameter specUnit;
  specUnit.name = "spec_unit";
  specUnit.displayName = "Spec Unit";
  specUnit.type = "string";
  specUnit.value = "A";
  specUnit.unit = "";
  specCategory.parameters.push_back(specUnit);

  // Pass threshold
  SettingsParameter passThreshold;
  passThreshold.name = "pass_threshold";
  passThreshold.displayName = "Pass Threshold";
  passThreshold.type = "float";
  passThreshold.value = "100.0";
  passThreshold.unit = "%";
  passThreshold.minValue = 50.0f;
  passThreshold.maxValue = 200.0f;
  specCategory.parameters.push_back(passThreshold);

  // Fail threshold
  SettingsParameter failThreshold;
  failThreshold.name = "fail_threshold";
  failThreshold.displayName = "Fail Threshold";
  failThreshold.type = "float";
  failThreshold.value = "90.0";
  failThreshold.unit = "%";
  failThreshold.minValue = 0.0f;
  failThreshold.maxValue = 150.0f;
  specCategory.parameters.push_back(failThreshold);

  // Exceptional threshold
  SettingsParameter exceptionalThreshold;
  exceptionalThreshold.name = "exceptional_threshold";
  exceptionalThreshold.displayName = "Exceptional Threshold";
  exceptionalThreshold.type = "float";
  exceptionalThreshold.value = "135.0";
  exceptionalThreshold.unit = "%";
  exceptionalThreshold.minValue = 100.0f;
  exceptionalThreshold.maxValue = 300.0f;
  specCategory.parameters.push_back(exceptionalThreshold);

  m_categories.push_back(specCategory);

  // UI Settings Category
  SettingsCategory uiCategory;
  uiCategory.name = "ui_settings";
  uiCategory.displayName = "UI Settings";

  // Add future UI parameters here

  m_categories.push_back(uiCategory);
}

void SettingsEditorUI::LoadCategoryFromDatabase(SettingsCategory& category) {
  auto& settings = AppSettings::getInstance();

  for (auto& param : category.parameters) {
    if (param.type == "float") {
      auto value = settings.getFloat("ui_settings", param.name);
      if (value.has_value()) {
        param.value = std::to_string(value.value());
      }
    }
    else if (param.type == "string") {
      auto value = settings.getString("ui_settings", param.name);
      if (value.has_value()) {
        param.value = value.value();
      }
    }
    else if (param.type == "int") {
      auto value = settings.getInt("ui_settings", param.name);
      if (value.has_value()) {
        param.value = std::to_string(value.value());
      }
    }
    else if (param.type == "bool") {
      auto value = settings.getBool("ui_settings", param.name);
      if (value.has_value()) {
        param.value = value.value() ? "true" : "false";
      }
    }
  }

  m_logger->LogInfo("Loaded category: " + category.displayName);
}




void SettingsEditorUI::Render() {
  if (!m_isVisible) return;

  // Window setup
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse;
  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImVec2(200, 100), ImGuiCond_FirstUseEver);

  // Use the window name from GetName()
  if (ImGui::Begin(GetName().c_str(), &m_isVisible, flags)) {
    // Get content region
    ImVec2 contentRegion = ImGui::GetContentRegionAvail();

    // Calculate column widths (15% left, 85% right)
    float leftWidth = contentRegion.x * 0.15f;
    float rightWidth = contentRegion.x * 0.85f - 10.0f; // Account for spacing

    // Left panel (15%)
    ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, 0), true);
    RenderLeftPanel();
    ImGui::EndChild();

    ImGui::SameLine();

    // Right panel (85%)
    ImGui::BeginChild("RightPanel", ImVec2(rightWidth, 0), true);
    RenderRightPanel();
    ImGui::EndChild();

    // Status bar at bottom
    if (!m_statusMessage.empty()) {
      ImGui::Separator();
      ImGui::Text("Status: %s", m_statusMessage.c_str());
    }
  }
  ImGui::End();
}


void SettingsEditorUI::RenderLeftPanel() {
  ImGui::Text("Categories");
  ImGui::Separator();

  // Category list
  for (int i = 0; i < static_cast<int>(m_categories.size()); i++) {
    bool isSelected = (m_selectedCategoryIndex == i);

    if (ImGui::Selectable(m_categories[i].displayName.c_str(), isSelected)) {
      m_selectedCategoryIndex = i;
      UpdateStatus("Selected category: " + m_categories[i].displayName);
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Click to edit %s", m_categories[i].displayName.c_str());
    }
  }

  ImGui::Spacing();
  ImGui::Separator();

  // Save button
  if (ImGui::Button("Save All", ImVec2(-1, 30))) {
    SaveAllCategories();
  }

  ImGui::Spacing();

  // Close button
  if (ImGui::Button("Close", ImVec2(-1, 30))) {
    Hide();
  }
}

void SettingsEditorUI::RenderRightPanel() {
  if (m_selectedCategoryIndex < 0 || m_selectedCategoryIndex >= static_cast<int>(m_categories.size())) {
    ImGui::Text("Select a category to edit parameters");
    return;
  }

  auto& category = m_categories[m_selectedCategoryIndex];

  ImGui::Text("Editing: %s", category.displayName.c_str());
  ImGui::Separator();

  // Parameters list
  for (auto& param : category.parameters) {
    RenderParameterInput(param);
    ImGui::Spacing();
  }

  ImGui::Separator();

  // Save this category button
  if (ImGui::Button("Save Category", ImVec2(150, 25))) {
    SaveCategoryToDatabase(category);
    UpdateStatus("Saved: " + category.displayName);
  }
}

void SettingsEditorUI::RenderParameterInput(SettingsParameter& param) {
  // Parameter name (left aligned)
  float nameWidth = 200.0f;
  ImGui::Text("%s:", param.displayName.c_str());

  ImGui::SameLine();
  ImGui::SetCursorPosX(nameWidth);

  // Input field based on type
  std::string inputId = "##" + param.name;

  if (param.type == "float") {
    char buffer[64];
    strncpy_s(buffer, sizeof(buffer), param.value.c_str(), _TRUNCATE);

    ImGui::SetNextItemWidth(120);
    if (ImGui::InputText(inputId.c_str(), buffer, sizeof(buffer), ImGuiInputTextFlags_CharsDecimal)) {
      param.value = std::string(buffer);
    }

    // Show unit
    if (!param.unit.empty()) {
      ImGui::SameLine();
      ImGui::Text("%s", param.unit.c_str());
    }

    // Show range hint
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
      "(%.1f - %.1f)", param.minValue, param.maxValue);

  }
  else if (param.type == "string") {
    char buffer[256];
    strncpy_s(buffer, sizeof(buffer), param.value.c_str(), _TRUNCATE);

    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText(inputId.c_str(), buffer, sizeof(buffer))) {
      param.value = std::string(buffer);
    }

  }
  else if (param.type == "bool") {
    bool value = (param.value == "true" || param.value == "1");
    if (ImGui::Checkbox(inputId.c_str(), &value)) {
      param.value = value ? "true" : "false";
    }

  }
  else if (param.type == "int") {
    int value = 0;
    try {
      value = std::stoi(param.value);
    }
    catch (...) {
      value = 0;
    }

    ImGui::SetNextItemWidth(120);
    if (ImGui::InputInt(inputId.c_str(), &value)) {
      param.value = std::to_string(value);
    }
  }
}

void SettingsEditorUI::UpdateStatus(const std::string& message, bool isError) {
  m_statusMessage = message;

  if (isError) {
    m_logger->LogError("SettingsEditorUI: " + message);
  }
  else {
    m_logger->LogInfo("SettingsEditorUI: " + message);
  }
}


void SettingsEditorUI::SaveAllCategories() {
  for (const auto& category : m_categories) {
    SaveCategoryToDatabase(category);
  }
  UpdateStatus("All settings saved successfully");

  // Trigger callback to notify RunPageUI
  if (m_onSettingsChanged) {
    m_onSettingsChanged();
    m_logger->LogInfo("SettingsEditorUI: Notified listeners of settings change");
  }
}

void SettingsEditorUI::SaveCategoryToDatabase(const SettingsCategory& category) {
  auto& settings = AppSettings::getInstance();
  bool success = true;

  for (const auto& param : category.parameters) {
    if (param.type == "float") {
      try {
        float value = std::stof(param.value);
        if (!settings.setFloat("ui_settings", param.name, value)) {
          success = false;
        }
      }
      catch (...) {
        success = false;
      }
    }
    else if (param.type == "string") {
      if (!settings.setString("ui_settings", param.name, param.value)) {
        success = false;
      }
    }
    else if (param.type == "int") {
      try {
        int value = std::stoi(param.value);
        if (!settings.setInt("ui_settings", param.name, value)) {
          success = false;
        }
      }
      catch (...) {
        success = false;
      }
    }
    else if (param.type == "bool") {
      bool value = (param.value == "true" || param.value == "1");
      if (!settings.setBool("ui_settings", param.name, value)) {
        success = false;
      }
    }
  }

  if (success) {
    m_logger->LogInfo("Saved category: " + category.displayName);

    // Trigger callback for individual category saves too
    if (m_onSettingsChanged) {
      m_onSettingsChanged();
      m_logger->LogInfo("SettingsEditorUI: Notified listeners of category save: " + category.displayName);
    }
  }
  else {
    m_logger->LogError("Failed to save some parameters in: " + category.displayName);
  }
}