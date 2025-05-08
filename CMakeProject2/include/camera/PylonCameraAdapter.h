// Much simpler PylonCameraAdapter.h
#pragma once

#include "include/ui/VerticalToolbarMenu.h"
#include "include/camera/pylon_camera_test.h"

// Adapter for PylonCameraTest that implements the IHierarchicalTogglableUI interface
class PylonCameraAdapter : public IHierarchicalTogglableUI {
public:
  PylonCameraAdapter(PylonCameraTest& camera, const std::string& name)
    : m_camera(camera), m_name(name), m_children() {
  }

  bool IsVisible() const override {
    return m_camera.IsVisible();
  }

  void ToggleWindow() override {
    m_camera.ToggleWindow();
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
  PylonCameraTest& m_camera;
  std::string m_name;
  std::vector<std::shared_ptr<IHierarchicalTogglableUI>> m_children;
};

// Helper function to create a PylonCameraAdapter
inline std::shared_ptr<IHierarchicalTogglableUI> CreatePylonCameraAdapter(
  PylonCameraTest& camera, const std::string& name) {
  return std::make_shared<PylonCameraAdapter>(camera, name);
}