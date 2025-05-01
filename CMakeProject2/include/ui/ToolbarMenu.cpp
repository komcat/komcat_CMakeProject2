// ToolbarMenu.cpp
#include "include/ui/ToolbarMenu.h"
#include "imgui.h"
#include <algorithm>

ToolbarMenu::ToolbarMenu() {
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("ToolbarMenu initialized");
}

void ToolbarMenu::AddReference(std::shared_ptr<ITogglableUI> component) {
  if (!component) {
    m_logger->LogWarning("Attempted to add null component to ToolbarMenu");
    return;
  }

  // Check if component with this name already exists
  const std::string& name = component->GetName();
  auto it = std::find_if(m_components.begin(), m_components.end(),
    [&name](const std::shared_ptr<ITogglableUI>& comp) {
    return comp->GetName() == name;
  });

  if (it != m_components.end()) {
    m_logger->LogWarning("Component with name '" + name + "' already exists in ToolbarMenu");
    return;
  }

  m_components.push_back(component);
  m_logger->LogInfo("Added component '" + name + "' to ToolbarMenu");
}

bool ToolbarMenu::RemoveReference(const std::string& name) {
  auto it = std::find_if(m_components.begin(), m_components.end(),
    [&name](const std::shared_ptr<ITogglableUI>& comp) {
    return comp->GetName() == name;
  });

  if (it != m_components.end()) {
    m_components.erase(it);
    m_logger->LogInfo("Removed component '" + name + "' from ToolbarMenu");
    return true;
  }

  m_logger->LogWarning("Component '" + name + "' not found in ToolbarMenu");
  return false;
}

size_t ToolbarMenu::GetComponentCount() const {
  return m_components.size();
}

void ToolbarMenu::RenderUI() {
  // Set toolbar style
  ImGuiStyle& style = ImGui::GetStyle();
  float oldWindowPadding = style.WindowPadding.y;
  style.WindowPadding.y = 8.0f; // Increase padding for the toolbar

  // Create toolbar window at the top of the screen
  ImGui::SetNextWindowPos(ImVec2(0, 50));
  ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 40));

  ImGuiWindowFlags toolbarFlags =
    ImGuiWindowFlags_NoTitleBar |
    ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoCollapse;

  ImGui::Begin("##ToolbarMenu", nullptr, toolbarFlags);

  // Button colors
  ImVec4 activeButtonColor = ImVec4(0.2f, 0.7f, 0.2f, 1.0f); // Green for active
  ImVec4 inactiveButtonColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); // Gray for inactive

  // Calculate button width based on number of components
  float availableWidth = ImGui::GetContentRegionAvail().x;
  float buttonWidth = std::min(120.0f, (availableWidth - 10.0f * (m_components.size() - 1)) / m_components.size());

  // Handle scrolling for many components
  bool needScroll = m_components.size() * (buttonWidth + 10.0f) > availableWidth;

  if (needScroll) {
    // Create a horizontal scrolling region
    ImGui::BeginChild("##ToolbarScroll", ImVec2(availableWidth, 30), false, ImGuiWindowFlags_HorizontalScrollbar);
  }

  // Render buttons for each component
  for (size_t i = 0; i < m_components.size(); ++i) {
    auto& component = m_components[i];
    bool isVisible = component->IsVisible();

    // Set button color based on state
    if (isVisible) {
      ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
    }
    else {
      ImGui::PushStyleColor(ImGuiCol_Button, inactiveButtonColor);
    }

    // Create button with component name
    if (ImGui::Button(component->GetName().c_str(), ImVec2(buttonWidth, 24))) {
      component->ToggleWindow();
    }

    ImGui::PopStyleColor(); // Reset button color

    // Add spacing between buttons (except after the last one)
    if (i < m_components.size() - 1) {
      ImGui::SameLine(0, 10);
    }
  }

  if (needScroll) {
    ImGui::EndChild();
  }

  ImGui::End();

  // Restore original style
  style.WindowPadding.y = oldWindowPadding;
}