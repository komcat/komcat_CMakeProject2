// toolbar.cpp
#include "toolbar.h"
#include "imgui.h"
#include "logger.h"

Toolbar::Toolbar(MotionConfigEditor& configEditor)
    : m_configEditor(configEditor),
    m_configEditorVisible(configEditor.IsVisible())
{
    // Initialize with empty callbacks
    m_button2Callback = []() {};
    m_button3Callback = []() {};

    // Log toolbar initialization
    Logger::GetInstance()->LogInfo("Toolbar initialized");
}

void Toolbar::RenderUI()
{
    // Set toolbar style
    ImGuiStyle& style = ImGui::GetStyle();
    float oldWindowPadding = style.WindowPadding.y;
    style.WindowPadding.y = 8.0f; // Increase padding for the toolbar

    // Create toolbar window
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 40));

    ImGuiWindowFlags toolbarFlags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("##Toolbar", nullptr, toolbarFlags);

    // Button colors
    ImVec4 activeButtonColor = ImVec4(0.2f, 0.7f, 0.2f, 1.0f); // Green for active
    ImVec4 inactiveButtonColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); // Gray for inactive

    // Get the current state to determine button color
    m_configEditorVisible = m_configEditor.IsVisible();

    // Set button color based on state
    if (m_configEditorVisible) {
        ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Button, inactiveButtonColor);
    }

    // Button 1: Toggle Motion Config Editor
    if (ImGui::Button("Config Editor", ImVec2(120, 24))) {
        m_configEditor.ToggleWindow();
        m_configEditorVisible = !m_configEditorVisible;
    }
    ImGui::PopStyleColor(); // Reset button color

    ImGui::SameLine(0, 10); // Add spacing between buttons

    // Button 2: Mock button
    if (m_button2Active) {
        ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Button, inactiveButtonColor);
    }

    if (ImGui::Button("Button 2", ImVec2(120, 24))) {
        m_button2Active = !m_button2Active;
        m_button2Callback();
    }
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 10);

    // Button 3: Mock button
    if (m_button3Active) {
        ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Button, inactiveButtonColor);
    }

    if (ImGui::Button("Button 3", ImVec2(120, 24))) {
        m_button3Active = !m_button3Active;
        m_button3Callback();
    }
    ImGui::PopStyleColor();

    ImGui::End();

    // Restore original style
    style.WindowPadding.y = oldWindowPadding;
}

void Toolbar::SetButton2Callback(std::function<void()> callback)
{
    m_button2Callback = callback;
}

void Toolbar::SetButton3Callback(std::function<void()> callback)
{
    m_button3Callback = callback;
}