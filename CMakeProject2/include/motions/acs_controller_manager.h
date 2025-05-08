// acs_controller_manager.h
#pragma once

#include "acs_controller.h"
#include "MotionConfigManager.h"
#include <string>
#include <map>
#include <memory>

class ACSControllerManager {
public:
  ACSControllerManager(MotionConfigManager& configManager);
  ~ACSControllerManager();

  // Initialize controllers from configuration
  void InitializeControllers();

  // Connect all enabled controllers
  bool ConnectAll();

  // Disconnect all controllers
  void DisconnectAll();

  // Get a specific controller by device name
  ACSController* GetController(const std::string& deviceName);

  // Check if a device has a controller
  bool HasController(const std::string& deviceName) const;

  // Move a device to a named position
  bool MoveToNamedPosition(const std::string& deviceName, const std::string& positionName, bool blocking = true);

  // Render UI for all controllers
  void RenderUI();
  // Add these new methods for UI toggling
  // Add these methods for UI visibility control if they don't exist
  bool IsVisible() const { return m_isWindowVisible; }
  void ToggleWindow() {
    m_isWindowVisible = !m_isWindowVisible;
    // Update the visibility of all controllers
    for (auto& [name, controller] : m_controllers) {
      controller->SetWindowVisible(m_isWindowVisible);
    }
  }

  // Method to directly set window visibility
  void SetWindowVisible(bool visible) {
    m_isWindowVisible = visible;
    // Update the visibility of all controllers if window is visible
    if (m_isWindowVisible) {
      for (auto& [name, controller] : m_controllers) {
        controller->SetWindowVisible(visible);
      }
    }
  }
  //void ToggleWindow() {
  //  m_isWindowVisible = !m_isWindowVisible;
  //  // Update the visibility of all controllers
  //  for (auto& [name, controller] : m_controllers) {
  //    controller->SetWindowVisible(m_isWindowVisible);
  //  }
  //}
private:
  MotionConfigManager& m_configManager;
  std::map<std::string, std::unique_ptr<ACSController>> m_controllers;
  Logger* m_logger;

  // Helper method to execute position movement without blocking UI
  bool ExecutePositionMove(ACSController* controller,
    const std::string& deviceName,
    const std::string& positionName,
    const PositionStruct& position);
  // Add this member to track window visibility
  bool m_isWindowVisible = false;
};