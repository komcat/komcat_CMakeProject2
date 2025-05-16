// VerticalToolbarMenu.cpp
#include "VerticalToolbarMenu.h"
#include "imgui.h"
#include <algorithm>

VerticalToolbarMenu::VerticalToolbarMenu() {
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("VerticalToolbarMenu initialized");
}

VerticalToolbarMenu::~VerticalToolbarMenu() {
  // Clear all references to UI components
  m_rootComponents.clear();
  m_categories.clear();

  // Log the completion
  if (m_logger) {
    m_logger->LogInfo("VerticalToolbarMenu: Destroyed");
  }
}

void VerticalToolbarMenu::AddReference(std::shared_ptr<IHierarchicalTogglableUI> component) {
  if (!component) {
    m_logger->LogWarning("Attempted to add null component to VerticalToolbarMenu");
    return;
  }

  // Check if component with this name already exists at root level
  const std::string& name = component->GetName();
  auto it = std::find_if(m_rootComponents.begin(), m_rootComponents.end(),
    [&name](const std::shared_ptr<IHierarchicalTogglableUI>& comp) {
    return comp->GetName() == name;
  });

  if (it != m_rootComponents.end()) {
    m_logger->LogWarning("Component with name '" + name + "' already exists in VerticalToolbarMenu");
    return;
  }

  m_rootComponents.push_back(component);
  m_logger->LogInfo("Added component '" + name + "' to VerticalToolbarMenu");
}

std::shared_ptr<HierarchicalTogglableUI> VerticalToolbarMenu::CreateCategory(const std::string& name) {
  // Check if category already exists
  auto it = m_categories.find(name);
  if (it != m_categories.end()) {
    return it->second;
  }

  // Create new category
  auto category = std::make_shared<HierarchicalTogglableUI>(name);
  m_categories[name] = category;

  // Add to root components as well
  m_rootComponents.push_back(category);

  m_logger->LogInfo("Created category '" + name + "' in VerticalToolbarMenu");
  return category;
}

bool VerticalToolbarMenu::AddReferenceToCategory(const std::string& categoryName,
  std::shared_ptr<IHierarchicalTogglableUI> component) {
  if (!component) {
    m_logger->LogWarning("Attempted to add null component to category '" + categoryName + "'");
    return false;
  }

  // Find or create the category
  auto categoryIt = m_categories.find(categoryName);
  if (categoryIt == m_categories.end()) {
    m_logger->LogWarning("Category '" + categoryName + "' not found, creating it");
    auto category = CreateCategory(categoryName);
    category->AddChild(component);
    return true;
  }

  // Add component to the category
  auto category = categoryIt->second;

  // Check if component with this name already exists in category
  const std::string& name = component->GetName();
  const auto& children = category->GetChildren();
  auto it = std::find_if(children.begin(), children.end(),
    [&name](const std::shared_ptr<IHierarchicalTogglableUI>& comp) {
    return comp->GetName() == name;
  });

  if (it != children.end()) {
    m_logger->LogWarning("Component with name '" + name + "' already exists in category '" +
      categoryName + "'");
    return false;
  }

  category->AddChild(component);
  m_logger->LogInfo("Added component '" + name + "' to category '" + categoryName + "'");
  return true;
}

bool VerticalToolbarMenu::RemoveReference(const std::string& name) {
  // First check root components
  auto it = std::find_if(m_rootComponents.begin(), m_rootComponents.end(),
    [&name](const std::shared_ptr<IHierarchicalTogglableUI>& comp) {
    return comp->GetName() == name;
  });

  if (it != m_rootComponents.end()) {
    // Remove from categories map if it's a category
    m_categories.erase(name);

    // Remove from root components
    m_rootComponents.erase(it);
    m_logger->LogInfo("Removed component '" + name + "' from VerticalToolbarMenu");
    return true;
  }

  // Not found at root level, check categories
  for (auto& [categoryName, category] : m_categories) {
    const auto& children = category->GetChildren();
    auto childrenCopy = children; // Create a copy since we're modifying

    for (size_t i = 0; i < childrenCopy.size(); ++i) {
      if (childrenCopy[i]->GetName() == name) {
        // We've found the component to remove
        // We cannot directly modify children from here, so we need to recreate
        // the category's children list excluding this component
        std::vector<std::shared_ptr<IHierarchicalTogglableUI>> newChildren;
        for (auto& child : childrenCopy) {
          if (child->GetName() != name) {
            newChildren.push_back(child);
          }
        }

        // Replace category's children (this would need appropriate accessor methods)
        // For now, let's assume we can't modify the children directly
        m_logger->LogWarning("Removing children from categories is not supported yet");
        return false;
      }
    }
  }

  m_logger->LogWarning("Component '" + name + "' not found in VerticalToolbarMenu");
  return false;
}

size_t VerticalToolbarMenu::GetComponentCount() const {
  size_t count = m_rootComponents.size();

  // Count all children in categories
  for (const auto& [name, category] : m_categories) {
    count += category->GetChildren().size();
  }

  return count;
}

void VerticalToolbarMenu::RenderUI() {
  // Set vertical toolbar style
  ImGuiStyle& style = ImGui::GetStyle();
  float oldWindowPadding = style.WindowPadding.x;
  style.WindowPadding.x = 8.0f; // Adjust padding for vertical toolbar

  // Set window background transparent
  ImVec4 oldBgColor = style.Colors[ImGuiCol_WindowBg];
  style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f); // Fully transparent

  // Create toolbar window on the left side of the screen
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2(m_width, ImGui::GetIO().DisplaySize.y));

  ImGuiWindowFlags toolbarFlags =
    ImGuiWindowFlags_NoTitleBar |
    ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoScrollWithMouse;

  ImGui::Begin("##VerticalToolbarMenu", nullptr, toolbarFlags);

  // Render all root components
  for (auto& component : m_rootComponents) {
    RenderComponent(component);
  }

  ImGui::End();

  // Render secondary panel if needed
  if (m_showSecondaryPanel && m_selectedCategory) {
    RenderSecondaryPanel();
  }

  // Restore original style
  style.WindowPadding.x = oldWindowPadding;
  style.Colors[ImGuiCol_WindowBg] = oldBgColor; // Restore original background color
}

void VerticalToolbarMenu::RenderComponent(const std::shared_ptr<IHierarchicalTogglableUI>& component) {
  bool isVisible = component->IsVisible();
  bool hasChildren = component->HasChildren();

  // Button colors - updated with softer, more natural colors
  ImVec4 activeButtonColor = ImVec4(0.2f, 0.7f, 0.2f, 1.0f); // Green for active
  ImVec4 inactiveButtonColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); // Gray for inactive
  ImVec4 categoryButtonColor = ImVec4(0.4f, 0.5f, 0.7f, 0.9f); // Softer blue for categories

  // Set button width to fill the toolbar
  float buttonWidth = ImGui::GetContentRegionAvail().x;

  // Set button color based on state and type
  if (hasChildren) {
    ImGui::PushStyleColor(ImGuiCol_Button, categoryButtonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.6f, 0.8f, 0.9f)); // Lighter blue when hovered
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.4f, 0.6f, 0.9f)); // Darker blue when clicked
  }
  else if (isVisible) {
    ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, inactiveButtonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
  }

  // Create the button
  if (ImGui::Button(component->GetName().c_str(), ImVec2(buttonWidth, 30))) {
    if (hasChildren) {
      // It's a category, show secondary panel
      m_showSecondaryPanel = true;
      m_selectedCategory = component;
    }
    else {
      // Regular component, toggle its window
      component->ToggleWindow();
    }
  }

  ImGui::PopStyleColor(3); // Reset all button colors (button, hovered, active)

  // Add a small spacing between buttons
  ImGui::Spacing();
}


void VerticalToolbarMenu::RenderSecondaryPanel() {
  // Safety check - ensure m_selectedCategory is valid
  if (!m_selectedCategory) {
    m_showSecondaryPanel = false;
    return;
  }

  // Create secondary panel next to the main toolbar
  ImGui::SetNextWindowPos(ImVec2(m_width, 0));
  ImGui::SetNextWindowSize(ImVec2(m_width, ImGui::GetIO().DisplaySize.y));

  ImGuiWindowFlags panelFlags =
    ImGuiWindowFlags_NoTitleBar |
    ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoCollapse;

  std::string panelName = "##SecondaryPanel_" + m_selectedCategory->GetName();

  // Begin the secondary panel window
  bool keepOpen = true;
  if (!ImGui::Begin(panelName.c_str(), &keepOpen, panelFlags)) {
    ImGui::End();
    return;
  }

  // If the window was closed via the ImGui window close button
  if (!keepOpen) {
    m_showSecondaryPanel = false;
    m_selectedCategory = nullptr;
    ImGui::End();
    return;
  }

  // Add header with category name and close button
  ImGui::Text("%s", m_selectedCategory->GetName().c_str());
  ImGui::SameLine(ImGui::GetWindowWidth() - 30);
  if (ImGui::Button("X", ImVec2(20, 20))) {
    m_showSecondaryPanel = false;
    m_selectedCategory = nullptr;
    ImGui::End();
    return;
  }

  ImGui::Separator();

  // Safely get the children
  const auto& children = m_selectedCategory->GetChildren();

  // Render all child components
  for (const auto& child : children) {
    // Safety check - ensure child is valid
    if (!child) continue;

    bool isVisible = child->IsVisible();

    // Button colors
    ImVec4 activeButtonColor = ImVec4(0.2f, 0.7f, 0.2f, 1.0f); // Green for active
    ImVec4 inactiveButtonColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); // Gray for inactive

    // Set button width to fill the panel
    float buttonWidth = ImGui::GetContentRegionAvail().x;

    // Set button color based on state
    if (isVisible) {
      ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
    }
    else {
      ImGui::PushStyleColor(ImGuiCol_Button, inactiveButtonColor);
    }

    // Create the button
    if (ImGui::Button(child->GetName().c_str(), ImVec2(buttonWidth, 30))) {
      child->ToggleWindow();
    }

    ImGui::PopStyleColor(); // Reset button color

    // Add a small spacing between buttons
    ImGui::Spacing();
  }

  ImGui::End();
}