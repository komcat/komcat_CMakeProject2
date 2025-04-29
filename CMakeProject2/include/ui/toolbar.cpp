// toolbar.cpp
#include "include/ui/toolbar.h"
#include "imgui.h"
#include "include/logger.h"

Toolbar::Toolbar(MotionConfigEditor& configEditor, GraphVisualizer& graphVisualizer, EziIO_UI& ioUI)
  : m_configEditor(configEditor),
  m_graphVisualizer(graphVisualizer),
  m_ioUI(ioUI),  // Initialize the EziIO_UI reference
  m_configEditorVisible(configEditor.IsVisible()),
  m_graphVisualizerVisible(graphVisualizer.IsVisible())
{
  // Initialize with empty callbacks
  m_button2Callback = []() {};
  m_button3Callback = []() {};

  // Log toolbar initialization
  Logger::GetInstance()->LogInfo("Toolbar initialized with GraphVisualizer and EziIO_UI support");
}

void Toolbar::RenderUI()
{
  // Set toolbar style
  ImGuiStyle& style = ImGui::GetStyle();
  float oldWindowPadding = style.WindowPadding.y;
  style.WindowPadding.y = 8.0f; // Increase padding for the toolbar

  // Create toolbar window
  ImGui::SetNextWindowPos(ImVec2(0, 50));
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
  m_graphVisualizerVisible = m_graphVisualizer.IsVisible();
  bool ioUIVisible = m_ioUI.IsVisible();  // Get current visibility state

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

  // Graph Visualizer button
  if (m_graphVisualizerVisible) {
    ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, inactiveButtonColor);
  }

  if (ImGui::Button("Graph Visualizer", ImVec2(120, 24))) {
    m_graphVisualizer.ToggleWindow();
    m_graphVisualizerVisible = !m_graphVisualizerVisible;
  }
  ImGui::PopStyleColor();

  ImGui::SameLine(0, 10); // Add spacing between buttons

  // Button 2: Custom button with callback
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

  // Button 3: IO Control Toggle button
  if (ioUIVisible) {
    ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, inactiveButtonColor);
  }

  if (ImGui::Button("IO Control", ImVec2(120, 24))) {
    m_ioUI.ToggleWindow();  // Toggle visibility using the method
    m_button3Active = m_ioUI.IsVisible();  // Update button state
    m_button3Callback();  // Call the callback if set
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