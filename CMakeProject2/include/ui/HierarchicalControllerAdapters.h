// HierarchicalControllerAdapters.h
#pragma once

#include "VerticalToolbarMenu.h"
#include "include/motions/acs_controller_manager.h"
#include "include/motions/pi_controller_manager.h"

// Adapter for ACSControllerManager that implements the IHierarchicalTogglableUI interface
class ACSControllerManagerHierarchicalAdapter : public IHierarchicalTogglableUI {
public:
  ACSControllerManagerHierarchicalAdapter(ACSControllerManager& manager, const std::string& name)
    : m_manager(manager), m_name(name), m_children() {
  }

  bool IsVisible() const override {
    // Use the manager's own visibility status
    return m_manager.IsVisible();
  }

  void ToggleWindow() override {
    // Toggle the manager window directly
    m_manager.ToggleWindow();
  }

  const std::string& GetName() const override {
    return m_name;
  }

  bool HasChildren() const override {
    return false;
  }

  const std::vector<std::shared_ptr<IHierarchicalTogglableUI>>& GetChildren() const override {
    return m_children;
  }

private:
  ACSControllerManager& m_manager;
  std::string m_name;
  std::vector<std::shared_ptr<IHierarchicalTogglableUI>> m_children;
};

// Adapter for PIControllerManager that implements the IHierarchicalTogglableUI interface
class PIControllerManagerHierarchicalAdapter : public IHierarchicalTogglableUI {
public:
  PIControllerManagerHierarchicalAdapter(PIControllerManager& manager, const std::string& name)
    : m_manager(manager), m_name(name), m_children() {
  }

  bool IsVisible() const override {
    // Use the manager's own visibility status
    return m_manager.IsVisible();
  }

  void ToggleWindow() override {
    // Toggle the manager window directly
    m_manager.ToggleWindow();
  }

  const std::string& GetName() const override {
    return m_name;
  }

  bool HasChildren() const override {
    return false;
  }

  const std::vector<std::shared_ptr<IHierarchicalTogglableUI>>& GetChildren() const override {
    return m_children;
  }

private:
  PIControllerManager& m_manager;
  std::string m_name;
  std::vector<std::shared_ptr<IHierarchicalTogglableUI>> m_children;
};

// Helper functions with different names to avoid conflicts with existing ones
inline std::shared_ptr<IHierarchicalTogglableUI> CreateHierarchicalPIControllerAdapter(PIControllerManager& manager, const std::string& name) {
  return std::make_shared<PIControllerManagerHierarchicalAdapter>(manager, name);
}

inline std::shared_ptr<IHierarchicalTogglableUI> CreateHierarchicalACSControllerAdapter(ACSControllerManager& manager, const std::string& name) {
  return std::make_shared<ACSControllerManagerHierarchicalAdapter>(manager, name);
}