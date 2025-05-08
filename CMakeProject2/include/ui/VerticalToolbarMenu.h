// VerticalToolbarMenu.h
#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include "include/logger.h"

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

private:
  // Collection of root UI components and categories
  std::vector<std::shared_ptr<IHierarchicalTogglableUI>> m_rootComponents;

  // Map for quick access to categories by name
  std::unordered_map<std::string, std::shared_ptr<HierarchicalTogglableUI>> m_categories;

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
};

// Note: The specialized adapter functions for controller managers have been moved to HierarchicalControllerAdapters.h