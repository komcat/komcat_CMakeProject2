// ToggleableUIAdapter.h
#pragma once

#include "include/ui/ToolbarMenu.h"
#include <string>

// Adapter for classes with different method names for visibility
template <typename T,
  typename IsVisibleFunc = bool (T::*)() const,
  typename ToggleFunc = void (T::*)()>
class CustomUIAdapter : public ITogglableUI {
public:
  CustomUIAdapter(T& component,
    const std::string& name,
    IsVisibleFunc isVisibleFunc,
    ToggleFunc toggleFunc)
    : m_component(component),
    m_name(name),
    m_isVisibleFunc(isVisibleFunc),
    m_toggleFunc(toggleFunc) {
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

private:
  T& m_component;
  std::string m_name;
  IsVisibleFunc m_isVisibleFunc;
  ToggleFunc m_toggleFunc;
};

// Helper function to create custom adapters with any method names
template<typename T,
  typename IsVisibleFunc = bool (T::*)() const,
  typename ToggleFunc = void (T::*)()>
std::shared_ptr<ITogglableUI> CreateCustomTogglableUI(
  T& component,
  const std::string& name,
  IsVisibleFunc isVisibleFunc,
  ToggleFunc toggleFunc)
{
  return std::make_shared<CustomUIAdapter<T, IsVisibleFunc, ToggleFunc>>(
    component, name, isVisibleFunc, toggleFunc);
}

// Example usage:
/*
// For a component with standard method names
toolbarMenu.AddReference(CreateTogglableUI(configEditor, "Config Editor"));

// For a component with custom method names
toolbarMenu.AddReference(CreateCustomTogglableUI(
    customComponent,
    "Custom UI",
    &CustomClass::IsWindowVisible,  // Different method name for visibility check
    &CustomClass::SwitchWindow      // Different method name for toggling
));
*/