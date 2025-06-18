// VerticalToolbarMenu.h
#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <set>
#include "include/logger.h"
#include "ToolbarStateManager.h"

// Forward declarations
class Logger;

// Enhanced interface for hierarchical togglable UI components
class IHierarchicalTogglableUI {
public:
  virtual ~IHierarchicalTogglableUI() = default;
  virtual bool IsVisible() const = 0;
  virtual void ToggleWindow() = 0;
  virtual const std::string& GetName() const = 0;

  // New methods for hierarchical structure
  virtual bool HasChildren() const = 0;
  virtual const std::vector<std::shared_ptr<IHierarchicalTogglableUI>>& GetChildren() const = 0;
};

// Base implementation that can contain children
class HierarchicalTogglableUI : public IHierarchicalTogglableUI {
public:
  HierarchicalTogglableUI(const std::string& name)
    : m_name(name), m_isVisible(false) {
  }

  bool IsVisible() const override { return m_isVisible; }

  void ToggleWindow() override {
    m_isVisible = !m_isVisible;
  }

  const std::string& GetName() const override { return m_name; }

  bool HasChildren() const override { return !m_children.empty(); }

  const std::vector<std::shared_ptr<IHierarchicalTogglableUI>>& GetChildren() const override {
    return m_children;
  }

  void AddChild(std::shared_ptr<IHierarchicalTogglableUI> child) {
    if (child) {
      m_children.push_back(child);
    }
  }

protected:
  std::string m_name;
  bool m_isVisible;
  std::vector<std::shared_ptr<IHierarchicalTogglableUI>> m_children;
};

// Adapter for existing components to fit into the hierarchical structure
template<typename T>
class HierarchicalUIAdapter : public IHierarchicalTogglableUI {
public:
  HierarchicalUIAdapter(T& component, const std::string& name)
    : m_component(component), m_name(name), m_children() {
  }

  bool IsVisible() const override {
    return m_component.IsVisible();
  }

  void ToggleWindow() override {
    m_component.ToggleWindow();
  }

  const std::string& GetName() const override {
    return m_name;
  }

  bool HasChildren() const override { return false; }

  const std::vector<std::shared_ptr<IHierarchicalTogglableUI>>& GetChildren() const override {
    return m_children;
  }

private:
  T& m_component;
  std::string m_name;
  std::vector<std::shared_ptr<IHierarchicalTogglableUI>> m_children;
};

// Custom adapter for components with different method names
template <typename T,
  typename IsVisibleFunc = bool (T::*)() const,
  typename ToggleFunc = void (T::*)()>
class CustomHierarchicalUIAdapter : public IHierarchicalTogglableUI {
public:
  CustomHierarchicalUIAdapter(T& component,
    const std::string& name,
    IsVisibleFunc isVisibleFunc,
    ToggleFunc toggleFunc)
    : m_component(component),
    m_name(name),
    m_isVisibleFunc(isVisibleFunc),
    m_toggleFunc(toggleFunc),
    m_children() {
  }

  bool IsVisible() const override {
    return (m_component.*m_isVisibleFunc)();
  }

  void ToggleWindow() override {
    (m_component.*m_toggleFunc)();
  }

  const std::string& GetName() const override {
    return m_name;
  }

  bool HasChildren() const override { return false; }

  const std::vector<std::shared_ptr<IHierarchicalTogglableUI>>& GetChildren() const override {
    return m_children;
  }

private:
  T& m_component;
  std::string m_name;
  IsVisibleFunc m_isVisibleFunc;
  ToggleFunc m_toggleFunc;
  std::vector<std::shared_ptr<IHierarchicalTogglableUI>> m_children;
};

// Placeholder component for missing items from toolbar_state.json (DEPRECATED - not used)
class PlaceholderUIComponent : public IHierarchicalTogglableUI {
public:
  PlaceholderUIComponent(const std::string& name, bool initialState = false)
    : m_name(name), m_isVisible(initialState), m_children() {
  }

  bool IsVisible() const override { return m_isVisible; }

  void ToggleWindow() override {
    m_isVisible = !m_isVisible;
    // Log the toggle action for placeholders
    Logger::GetInstance()->LogInfo("Placeholder '" + m_name + "' toggled to " +
      (m_isVisible ? "visible" : "hidden"));
  }

  const std::string& GetName() const override { return m_name; }
  bool HasChildren() const override { return false; }
  const std::vector<std::shared_ptr<IHierarchicalTogglableUI>>& GetChildren() const override {
    return m_children;
  }

private:
  std::string m_name;
  bool m_isVisible;
  std::vector<std::shared_ptr<IHierarchicalTogglableUI>> m_children;
};

// Helper function to create hierarchical components
template<typename T>
std::shared_ptr<IHierarchicalTogglableUI> CreateHierarchicalUI(T& component, const std::string& name) {
  return std::make_shared<HierarchicalUIAdapter<T>>(component, name);
}

// Helper for creating custom hierarchical adapters
template<typename T,
  typename IsVisibleFunc = bool (T::*)() const,
  typename ToggleFunc = void (T::*)()>
std::shared_ptr<IHierarchicalTogglableUI> CreateCustomHierarchicalUI(
  T& component,
  const std::string& name,
  IsVisibleFunc isVisibleFunc,
  ToggleFunc toggleFunc)
{
  return std::make_shared<CustomHierarchicalUIAdapter<T, IsVisibleFunc, ToggleFunc>>(
    component, name, isVisibleFunc, toggleFunc);
}

// Helper to create a category/group
inline std::shared_ptr<HierarchicalTogglableUI> CreateUICategory(const std::string& name) {
  return std::make_shared<HierarchicalTogglableUI>(name);
}

// Vertical toolbar menu with hierarchical components
class VerticalToolbarMenu {
public:
  VerticalToolbarMenu();
  ~VerticalToolbarMenu();

  // Add a root-level component
  void AddReference(std::shared_ptr<IHierarchicalTogglableUI> component);

  // Add a component to a specific category
  bool AddReferenceToCategory(const std::string& categoryName,
    std::shared_ptr<IHierarchicalTogglableUI> component);

  // Create a new category (if it doesn't exist already)
  std::shared_ptr<HierarchicalTogglableUI> CreateCategory(const std::string& name);

  // Remove a component by name
  bool RemoveReference(const std::string& name);

  // Render the vertical toolbar UI
  void RenderUI();

  // Get the number of components
  size_t GetComponentCount() const;

  // Configuration
  void SetWidth(float width) { m_width = width; }
  float GetWidth() const { return m_width; }

  // Initialize the toolbar with state persistence
  void InitializeStateTracking(const std::string& stateFilePath = "toolbar_state.json") {
    // Initialize the state manager
    ToolbarStateManager::GetInstance().Initialize(stateFilePath);

    // Note: No longer automatically adding missing items from toolbar_state.json
  }

  // Cross-check with toolbar_state.json and add missing items as placeholders (DEPRECATED - not used)
  void CrossCheckAndAddMissingItems();

  // Check if a component exists (by name)
  bool HasComponent(const std::string& name) const;

  // Get all component names currently in the toolbar
  std::set<std::string> GetAllComponentNames() const;

  // Count total windows (components)
  size_t GetTotalWindowCount() const {
    size_t count = 0;

    // Count root components
    count += m_rootComponents.size();

    // Count children in categories
    for (const auto& [name, category] : m_categories) {
      count += category->GetChildren().size();
    }

    return count;
  }

  // Count visible windows
  size_t GetVisibleWindowCount() const {
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

  // Get a list of all visible window names
  std::vector<std::string> GetVisibleWindowNames() const {
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

  // Save all window states
  void SaveAllWindowStates();

private:
  // Collection of root UI components and categories
  std::vector<std::shared_ptr<IHierarchicalTogglableUI>> m_rootComponents;

  // Map for quick access to categories by name
  std::unordered_map<std::string, std::shared_ptr<HierarchicalTogglableUI>> m_categories;

  // Known category names to exclude from missing item check
  std::set<std::string> m_categoryNames = {
    "Motors", "Manual", "Data", "Products", "General"
  };

  // Logger instance
  Logger* m_logger;

  // Toolbar width
  float m_width = 250.0f;

  // Secondary panel state
  bool m_showSecondaryPanel = false;
  std::shared_ptr<IHierarchicalTogglableUI> m_selectedCategory = nullptr;

  // Helper methods
  void RenderComponent(const std::shared_ptr<IHierarchicalTogglableUI>& component);
  void RenderSecondaryPanel();

  // Helper to check if a name is a category
  bool IsKnownCategory(const std::string& name) const {
    return m_categoryNames.find(name) != m_categoryNames.end();
  }
};