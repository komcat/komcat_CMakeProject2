#include "UIConfigEditor.h"
#include "imgui.h"
#include <iostream>
#include <algorithm>
#include <set>
#include <cstring>
#include "include/motions/MotionConfigManager.h"

UIConfigEditor::UIConfigEditor(MotionConfigManager& configMgr)
  : configManager(configMgr)
  , m_logger(Logger::GetInstance()) {
  std::cout << "UIConfigEditor created" << std::endl;
  m_logger->LogInfo("UIConfigEditor initialized");
}

UIConfigEditor::~UIConfigEditor() {
  std::cout << "UIConfigEditor destroyed" << std::endl;
}

void UIConfigEditor::RenderUI() {
  if (!showWindow) return;

  // Tabs for different config sections
  if (ImGui::BeginTabBar("ConfigTabs")) {
    if (ImGui::BeginTabItem("Devices")) {
      m_showDevicesTab = true;
      m_showPositionsTab = false;
      m_showGraphsTab = false;
      m_showSettingsTab = false;
      RenderDevicesTab();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Positions")) {
      m_showDevicesTab = false;
      m_showPositionsTab = true;
      m_showGraphsTab = false;
      m_showSettingsTab = false;
      RenderPositionsTab();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Graphs")) {
      m_showDevicesTab = false;
      m_showPositionsTab = false;
      m_showGraphsTab = true;
      m_showSettingsTab = false;
      RenderGraphsTab();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Settings")) {
      m_showDevicesTab = false;
      m_showPositionsTab = false;
      m_showGraphsTab = false;
      m_showSettingsTab = true;
      RenderSettingsTab();
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  // Save button at the bottom
  ImGui::Separator();
  if (ImGui::Button("Save Changes")) {
    SaveChanges();
  }

  // Debug section for clipboard content
  if (ImGui::CollapsingHeader("Debug Clipboard")) {
    if (ImGui::Button("Show Clipboard Content")) {
      std::string clipboardText = ImGui::GetClipboardText();
      m_logger->LogInfo("Current clipboard content: " + clipboardText);
    }

    ImGui::SameLine();

    if (ImGui::Button("Test Position JSON")) {
      const char* testJson = R"({
  "device": "gantry-main",
  "positions": {
    "X": 143.200000,
    "Y": 75.700000,
    "Z": 8.244764
  }
})";
      ImGui::SetClipboardText(testJson);
      m_logger->LogInfo("Set test JSON to clipboard");
    }
  }

  // Handle the confirmation popup (this needs to stay as it creates its own modal)
  RenderClipboardConfirmationPopup();
}



void UIConfigEditor::RenderSettingsTab() {
  ImGui::Text("Settings editing is not implemented yet.");

  // Display current settings
  const auto& settings = configManager.GetSettings();

  ImGui::Text("Current Settings:");
  ImGui::BulletText("Default Speed: %.2f", settings.DefaultSpeed);
  ImGui::BulletText("Default Acceleration: %.2f", settings.DefaultAcceleration);
  ImGui::BulletText("Log Level: %s", settings.LogLevel.c_str());
  ImGui::BulletText("Auto Reconnect: %s", settings.AutoReconnect ? "Yes" : "No");
  ImGui::BulletText("Connection Timeout: %d ms", settings.ConnectionTimeout);
  ImGui::BulletText("Position Tolerance: %.3f", settings.PositionTolerance);
}

void UIConfigEditor::SaveChanges() {
  try {
    bool success = configManager.SaveConfig();
    if (success) {
      m_logger->LogInfo("Configuration saved successfully");
    }
    else {
      m_logger->LogError("Failed to save configuration");
    }
  }
  catch (const std::exception& e) {
    m_logger->LogError("Exception while saving configuration: " + std::string(e.what()));
  }
}

void UIConfigEditor::ToggleWindow() {
  showWindow = !showWindow;
}

// Note: Other methods are implemented in separate files:
// - Device methods in UIConfigEditor_Devices.cpp
// - Position methods in UIConfigEditor_Positions.cpp  
// - Graph methods in UIConfigEditor_Graphs.cpp
// - Helper methods in UIConfigEditor_Helpers.cpp