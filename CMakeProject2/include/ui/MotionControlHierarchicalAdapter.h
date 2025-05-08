// MotionControlHierarchicalAdapter.h
#pragma once

#include "include/ui/VerticalToolbarMenu.h"
#include "include/motions/motion_control_layer.h"

// Adapter for MotionControlLayer that implements the IHierarchicalTogglableUI interface
class MotionControlHierarchicalAdapter : public IHierarchicalTogglableUI {
public:
  MotionControlHierarchicalAdapter(MotionControlLayer& motionControl, const std::string& name)
    : m_motionControl(motionControl), m_name(name), m_children(), m_isVisible(false) {
  }

  bool IsVisible() const override {
    return m_isVisible;
  }

  void ToggleWindow() override {
    m_isVisible = !m_isVisible;
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

  // Method to allow checking the window visibility from the MotionControlLayer
  bool IsWindowVisible() const {
    return m_isVisible;
  }

private:
  MotionControlLayer& m_motionControl;
  std::string m_name;
  std::vector<std::shared_ptr<IHierarchicalTogglableUI>> m_children;
  bool m_isVisible;
};

// Helper function to create a hierarchical adapter for the MotionControlLayer
inline std::shared_ptr<IHierarchicalTogglableUI> CreateHierarchicalMotionControlAdapter(MotionControlLayer& motionControl, const std::string& name) {
  return std::make_shared<MotionControlHierarchicalAdapter>(motionControl, name);
}