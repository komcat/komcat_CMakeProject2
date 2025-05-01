// ToolbarMenu.h
#pragma once

#include <vector>
#include <string>
#include <functional>
#include <memory>
#include "include/logger.h"

// Forward declarations to avoid circular dependencies
class Logger;

// Interface for UI components that can be toggled on/off
class ITogglableUI {
public:
  virtual ~ITogglableUI() = default;
  virtual bool IsVisible() const = 0;
  virtual void ToggleWindow() = 0;
  virtual const std::string& GetName() const = 0;
};

class ToolbarMenu {
public:
  ToolbarMenu();
  ~ToolbarMenu();

  // Add a reference to a UI component
  void AddReference(std::shared_ptr<ITogglableUI> component);

  // Remove a component by name
  bool RemoveReference(const std::string& name);

  // Render the toolbar UI
  void RenderUI();

  // Get the number of components
  size_t GetComponentCount() const;

private:
  // Collection of UI components
  std::vector<std::shared_ptr<ITogglableUI>> m_components;

  // Logger instance
  Logger* m_logger;

  // UI state
  bool m_showWindow = true;
};

// Helper adapter template to turn any class with ToggleWindow/IsVisible into an ITogglableUI
template<typename T>
class TogglableUIAdapter : public ITogglableUI {
public:
  TogglableUIAdapter(T& component, const std::string& name)
    : m_component(component), m_name(name) {
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

private:
  T& m_component;
  std::string m_name;
};

// Helper function to create adapters
template<typename T>
std::shared_ptr<ITogglableUI> CreateTogglableUI(T& component, const std::string& name) {
  return std::make_shared<TogglableUIAdapter<T>>(component, name);
}