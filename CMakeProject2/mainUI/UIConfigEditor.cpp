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

    // Set global font scale for better readability
    ImGui::SetWindowFontScale(1.50f);

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

    // Save button at the bottom with enhanced styling
    ImGui::Separator();
    ImGui::Spacing();

    // Center the save button
    float windowWidth = ImGui::GetWindowSize().x;
    float buttonWidth = 150.0f;
    ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));
    if (ImGui::Button("Save Changes", ImVec2(buttonWidth, 0))) {
        SaveChanges();
    }
    ImGui::PopStyleColor(3);

    ImGui::Spacing();

    // Debug section for clipboard content with collapsible header
    if (ImGui::CollapsingHeader("Debug & Testing Tools")) {
        ImGui::Indent();

        if (ImGui::Button("Show Clipboard Content", ImVec2(200, 0))) {
            std::string clipboardText = ImGui::GetClipboardText();
            if (clipboardText.empty()) {
                m_logger->LogInfo("Clipboard is empty");
            }
            else {
                m_logger->LogInfo("Current clipboard content: " + clipboardText);
            }
        }

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.9f, 1.0f));
        if (ImGui::Button("Set Test Position JSON", ImVec2(200, 0))) {
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
        ImGui::PopStyleColor(2);

        // Additional test data for hex devices
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.4f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.5f, 0.9f, 1.0f));
        if (ImGui::Button("Set Test Hex JSON", ImVec2(200, 0))) {
            const char* testHexJson = R"({
  "device": "hex-platform",
  "positions": {
    "X": 100.500000,
    "Y": 200.750000,
    "Z": 50.125000,
    "U": 15.000000,
    "V": -10.500000,
    "W": 5.250000
  }
})";
            ImGui::SetClipboardText(testHexJson);
            m_logger->LogInfo("Set test hex JSON to clipboard");
        }
        ImGui::PopStyleColor(2);

        ImGui::Unindent();
    }

    // Handle the confirmation popup (this needs to stay as it creates its own modal)
    RenderClipboardConfirmationPopup();
}

void UIConfigEditor::RenderSettingsTab() {
    // Set font scale for consistency
    ImGui::SetWindowFontScale(1.50f);

    ImGui::TextWrapped("Settings editing is not implemented yet.");
    ImGui::Spacing();

    // Display current settings in a more organized way
    const auto& settings = configManager.GetSettings();

    ImGui::Text("Current Configuration Settings:");
    ImGui::Separator();

    // Motion Settings
    if (ImGui::CollapsingHeader("Motion Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        ImGui::BulletText("Default Speed: %.2f units/sec", settings.DefaultSpeed);
        ImGui::BulletText("Default Acceleration: %.2f units/sec²", settings.DefaultAcceleration);
        ImGui::BulletText("Position Tolerance: %.6f units", settings.PositionTolerance);
        ImGui::Unindent();
    }

    // Connection Settings  
    if (ImGui::CollapsingHeader("Connection Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        ImGui::BulletText("Auto Reconnect: %s", settings.AutoReconnect ? "Enabled" : "Disabled");
        ImGui::BulletText("Connection Timeout: %d ms", settings.ConnectionTimeout);
        ImGui::Unindent();
    }

    // System Settings
    if (ImGui::CollapsingHeader("System Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        ImGui::BulletText("Log Level: %s", settings.LogLevel.c_str());
        ImGui::Unindent();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Future implementation placeholder
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
        "Note: Settings editing interface will be implemented in a future update.");
}

void UIConfigEditor::SaveChanges() {
    try {
        bool success = configManager.SaveConfig();
        if (success) {
            m_logger->LogInfo("✓ Configuration saved successfully");

            // Visual feedback for successful save
            ImGui::OpenPopup("Save Successful");
        }
        else {
            m_logger->LogError("✗ Failed to save configuration");
            ImGui::OpenPopup("Save Failed");
        }
    }
    catch (const std::exception& e) {
        m_logger->LogError("Exception while saving configuration: " + std::string(e.what()));
        ImGui::OpenPopup("Save Failed");
    }

    // Success popup
    if (ImGui::BeginPopupModal("Save Successful", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "✓ Configuration saved successfully!");
        ImGui::Spacing();

        float windowWidth = ImGui::GetWindowSize().x;
        float buttonWidth = 100.0f;
        ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);

        if (ImGui::Button("OK", ImVec2(buttonWidth, 0))) {
            ImGui::CloseCurrentPopup();
        }

        // Auto-close after 2 seconds
        static float timer = 0.0f;
        timer += ImGui::GetIO().DeltaTime;
        if (timer > 2.0f) {
            timer = 0.0f;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // Error popup  
    if (ImGui::BeginPopupModal("Save Failed", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1.0f), "✗ Failed to save configuration!");
        ImGui::TextWrapped("Check the logs for more details.");
        ImGui::Spacing();

        float windowWidth = ImGui::GetWindowSize().x;
        float buttonWidth = 100.0f;
        ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);

        if (ImGui::Button("OK", ImVec2(buttonWidth, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void UIConfigEditor::ToggleWindow() {
    showWindow = !showWindow;
    if (showWindow) {
        m_logger->LogInfo("UIConfigEditor window opened");
    }
    else {
        m_logger->LogInfo("UIConfigEditor window closed");
    }
}

// Note: Other methods are implemented in separate files:
// - Device methods in UIConfigEditor_Devices.cpp
// - Position methods in UIConfigEditor_Positions.cpp  
// - Graph methods in UIConfigEditor_Graphs.cpp
// - Details methods in UIConfigEditor_Details.cpp
// - Helper methods in UIConfigEditor_Helpers.cpp