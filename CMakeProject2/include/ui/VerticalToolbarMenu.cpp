// VerticalToolbarMenu.cpp
#include "VerticalToolbarMenu.h"
#include "ToolbarStateManager.h"
#include "imgui.h"
#include <algorithm>
#include <fstream>
#include "nlohmann/json.hpp"

VerticalToolbarMenu::VerticalToolbarMenu() {
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("VerticalToolbarMenu initialized");
}

// Modify the destructor to save states before destruction
VerticalToolbarMenu::~VerticalToolbarMenu() {
  // Save all window states before destruction
  SaveAllWindowStates();

  // Clear all references to UI components
  m_rootComponents.clear();
  m_categories.clear();

  // Log the completion
  if (m_logger) {
    m_logger->LogInfo("VerticalToolbarMenu: Destroyed");
  }
}

// In VerticalToolbarMenu::AddReference
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

  // Load saved state if available and apply it
  auto& stateManager = ToolbarStateManager::GetInstance();
  bool savedState = stateManager.GetWindowState(name, component->IsVisible());

  // Only toggle if the current state is different from the saved state
  if (savedState != component->IsVisible()) {
    component->ToggleWindow();
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

  // Add to known category names
  m_categoryNames.insert(name);

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

  // Load saved state if available and apply it
  auto& stateManager = ToolbarStateManager::GetInstance();
  bool savedState = stateManager.GetWindowState(name, component->IsVisible());

  // Only toggle if the current state is different from the saved state
  if (savedState != component->IsVisible()) {
    component->ToggleWindow();
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


// NEW METHOD: Check if a component exists
bool VerticalToolbarMenu::HasComponent(const std::string& name) const {
  // Check root components
  for (const auto& component : m_rootComponents) {
    if (component->GetName() == name) {
      return true;
    }

    // Check children in categories
    if (component->HasChildren()) {
      const auto& children = component->GetChildren();
      for (const auto& child : children) {
        if (child->GetName() == name) {
          return true;
        }
      }
    }
  }

  return false;
}

// NEW METHOD: Get all component names
std::set<std::string> VerticalToolbarMenu::GetAllComponentNames() const {
  std::set<std::string> names;

  // Add root component names
  for (const auto& component : m_rootComponents) {
    names.insert(component->GetName());

    // Add children names
    if (component->HasChildren()) {
      const auto& children = component->GetChildren();
      for (const auto& child : children) {
        names.insert(child->GetName());
      }
    }
  }

  return names;
}

// MINIMAL TEST VERSION - Replace RenderComponent with this:
// Add these methods to VerticalToolbarMenu.cpp

void VerticalToolbarMenu::RenderUI() {
  // Set vertical toolbar style
  ImGuiStyle& style = ImGui::GetStyle();
  float oldWindowPadding = style.WindowPadding.x;
  style.WindowPadding.x = 8.0f; // Adjust padding for vertical toolbar

  // Create toolbar window that can be moved and resized
  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(m_width, ImGui::GetIO().DisplaySize.y), ImGuiCond_FirstUseEver);

  // Allow default window behavior
  ImGuiWindowFlags toolbarFlags = ImGuiWindowFlags_None;

  ImGui::Begin("Toolbar", nullptr, toolbarFlags);

  // Add a simplified debug section at the top (collapsible)
  if (ImGui::CollapsingHeader("Toolbar Info##Debug", ImGuiTreeNodeFlags_None)) {
    ImGui::Text("Total Components: %zu", GetTotalWindowCount());
    ImGui::Text("Visible Windows: %zu", GetVisibleWindowCount());
    ImGui::Separator();
  }

  // Render all root components using tree structure
  for (auto& component : m_rootComponents) {
    RenderComponent(component);
  }

  ImGui::End();

  // Restore original style
  style.WindowPadding.x = oldWindowPadding;
}

size_t VerticalToolbarMenu::GetTotalWindowCount() const {
  size_t count = 0;

  // Count root components
  count += m_rootComponents.size();

  // Count children in categories
  for (const auto& [name, category] : m_categories) {
    count += category->GetChildren().size();
  }

  return count;
}

size_t VerticalToolbarMenu::GetVisibleWindowCount() const {
  size_t count = 0;

  // Count visible root components
  for (const auto& component : m_rootComponents) {
    if (component->IsVisible()) {
      count++;
    }

    // If this is a category, also count visible children
    if (component->HasChildren()) {
      const auto& children = component->GetChildren();
      for (const auto& child : children) {
        if (child->IsVisible()) {
          count++;
        }
      }
    }
  }

  return count;
}


bool VerticalToolbarMenu::ToggleComponentByName(const std::string& componentName) {
  // Search in root components first
  for (auto& component : m_rootComponents) {
    if (component->GetName() == componentName) {
      component->ToggleWindow();

      // Save the new state
      auto& stateManager = ToolbarStateManager::GetInstance();
      stateManager.SaveWindowState(componentName, component->IsVisible());

      m_logger->LogInfo("VerticalToolbarMenu: Toggled component '" + componentName + "' to " +
        (component->IsVisible() ? "visible" : "hidden"));
      return true;
    }

    // Search in category children
    if (component->HasChildren()) {
      const auto& children = component->GetChildren();
      for (auto& child : children) {
        if (child->GetName() == componentName) {
          child->ToggleWindow();

          // Save the new state
          auto& stateManager = ToolbarStateManager::GetInstance();
          stateManager.SaveWindowState(componentName, child->IsVisible());

          m_logger->LogInfo("VerticalToolbarMenu: Toggled component '" + componentName + "' to " +
            (child->IsVisible() ? "visible" : "hidden"));
          return true;
        }
      }
    }
  }

  m_logger->LogWarning("VerticalToolbarMenu: Component '" + componentName + "' not found");
  return false;
}

std::shared_ptr<IHierarchicalTogglableUI> VerticalToolbarMenu::GetComponentByName(const std::string& componentName) const {
  // Search in root components first
  for (const auto& component : m_rootComponents) {
    if (component->GetName() == componentName) {
      return component;
    }

    // Search in category children
    if (component->HasChildren()) {
      const auto& children = component->GetChildren();
      for (const auto& child : children) {
        if (child->GetName() == componentName) {
          return child;
        }
      }
    }
  }

  return nullptr;
}


std::vector<std::string> VerticalToolbarMenu::GetVisibleWindowNames() const {
  std::vector<std::string> visibleWindows;

  // Check root components
  for (const auto& component : m_rootComponents) {
    if (component->IsVisible()) {
      visibleWindows.push_back(component->GetName());
    }

    // If this is a category, also check its children
    if (component->HasChildren()) {
      const auto& children = component->GetChildren();
      for (const auto& child : children) {
        if (child->IsVisible()) {
          visibleWindows.push_back(child->GetName());
        }
      }
    }
  }

  return visibleWindows;
}
// Modify RenderComponent to save state when toggled and handle placeholders

void VerticalToolbarMenu::RenderComponent(const std::shared_ptr<IHierarchicalTogglableUI>& component) {
  bool isVisible = component->IsVisible();
  bool hasChildren = component->HasChildren();

  // Check if this is a placeholder component
  bool isPlaceholder = std::dynamic_pointer_cast<PlaceholderUIComponent>(component) != nullptr;

  if (hasChildren) {
    // This is a category - use CollapsingHeader for expandable dropdown
    RenderCategoryWithDropdown(component, isPlaceholder);
  }
  else {
    // This is a regular component - render as a button
    RenderRegularComponent(component, isVisible, isPlaceholder);
  }
}



void VerticalToolbarMenu::RenderCategoryWithDropdown(const std::shared_ptr<IHierarchicalTogglableUI>& category, bool isPlaceholder) {
  // Create unique ID for the collapsing header
  std::string headerId = category->GetName() + "##Category";

  // Set colors for category headers
  ImVec4 categoryHeaderColor = ImVec4(0.4f, 0.5f, 0.7f, 0.9f); // Blue for categories
  ImVec4 placeholderHeaderColor = ImVec4(0.7f, 0.5f, 0.2f, 1.0f); // Orange for placeholder categories

  // Push header colors
  if (isPlaceholder) {
    ImGui::PushStyleColor(ImGuiCol_Header, placeholderHeaderColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.8f, 0.6f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.6f, 0.4f, 0.1f, 1.0f));
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Header, categoryHeaderColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.5f, 0.6f, 0.8f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.4f, 0.6f, 0.9f));
  }

  // Create the collapsing header
  std::string displayName = category->GetName();
  if (isPlaceholder) {
    displayName += " [P]"; // Add placeholder indicator
  }

  // Use CollapsingHeader with default open state (you can change this)
  if (ImGui::CollapsingHeader(displayName.c_str(), ImGuiTreeNodeFlags_None)) {
    // The header is expanded, render all children with indentation
    ImGui::Indent(16.0f); // Indent child items

    const auto& children = category->GetChildren();
    for (const auto& child : children) {
      if (!child) continue; // Safety check

      bool childVisible = child->IsVisible();
      bool childIsPlaceholder = std::dynamic_pointer_cast<PlaceholderUIComponent>(child) != nullptr;

      RenderRegularComponent(child, childVisible, childIsPlaceholder);
    }

    ImGui::Unindent(16.0f); // Remove indentation
  }

  ImGui::PopStyleColor(3); // Reset header colors

  // Show tooltip for placeholder categories
  if (isPlaceholder && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Placeholder category from toolbar_state.json\nNo actual UI components connected");
  }

  // Add spacing after category
  ImGui::Spacing();
}

void VerticalToolbarMenu::RenderRegularComponent(const std::shared_ptr<IHierarchicalTogglableUI>& component, bool isVisible, bool isPlaceholder) {
  // Button colors
  ImVec4 activeButtonColor = ImVec4(0.2f, 0.7f, 0.2f, 1.0f); // Green for active
  ImVec4 inactiveButtonColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); // Gray for inactive
  ImVec4 placeholderActiveColor = ImVec4(0.7f, 0.5f, 0.2f, 1.0f); // Orange for active placeholders
  ImVec4 placeholderInactiveColor = ImVec4(0.6f, 0.4f, 0.3f, 1.0f); // Dark orange for inactive placeholders

  // Set button width to fill available space
  float buttonWidth = ImGui::GetContentRegionAvail().x;

  // Set button color based on state and type
  if (isPlaceholder) {
    if (isVisible) {
      ImGui::PushStyleColor(ImGuiCol_Button, placeholderActiveColor);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.6f, 0.3f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.4f, 0.1f, 1.0f));
    }
    else {
      ImGui::PushStyleColor(ImGuiCol_Button, placeholderInactiveColor);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.5f, 0.4f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.3f, 0.2f, 1.0f));
    }
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

  // Create the button with placeholder indicator
  std::string buttonText = component->GetName();
  if (isPlaceholder) {
    buttonText += " [P]"; // Add placeholder indicator
  }

  if (ImGui::Button(buttonText.c_str(), ImVec2(buttonWidth, 30))) {
    // Toggle the window
    component->ToggleWindow();

    // Save the new state
    auto& stateManager = ToolbarStateManager::GetInstance();
    stateManager.SaveWindowState(component->GetName(), component->IsVisible());
  }

  // Show tooltip for placeholders
  if (isPlaceholder && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Placeholder component from toolbar_state.json\nNo actual UI component is connected");
  }

  ImGui::PopStyleColor(3); // Reset all button colors
  ImGui::Spacing(); // Add spacing between buttons
}

// Add a new method to save the current state of all windows
void VerticalToolbarMenu::SaveAllWindowStates() {
  auto& stateManager = ToolbarStateManager::GetInstance();

  // Save state for all root components
  for (const auto& component : m_rootComponents) {
    stateManager.SaveWindowState(component->GetName(), component->IsVisible());

    // If this is a category, also save states of its children
    if (component->HasChildren()) {
      const auto& children = component->GetChildren();
      for (const auto& child : children) {
        stateManager.SaveWindowState(child->GetName(), child->IsVisible());
      }
    }
  }

  // Force a save to file
  stateManager.SaveState();
}

