// pi_controller_manager.h
#pragma once

#include "pi_controller.h"
#include "MotionConfigManager.h"
#include <string>
#include <map>
#include <memory>

class PIControllerManager {
public:
  PIControllerManager(MotionConfigManager& configManager);
  ~PIControllerManager();

  // Initialize controllers from configuration
  void InitializeControllers();

  // Connect all enabled controllers
  bool ConnectAll();

  // Disconnect all controllers
  void DisconnectAll();

  // Get a specific controller by device name
  PIController* GetController(const std::string& deviceName);

  // Check if a device has a controller
  bool HasController(const std::string& deviceName) const;

  // Move a device to a named position using a single batch command for all axes
  // The function moves a device to a predefined position stored in the configuration
  // @param deviceName The name of the device to move
  // @param positionName The name of the stored position to move to
  // @param blocking If true, function will wait for motion to complete before returning
  // @return True if the movement command was successful, false otherwise
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
  std::map<std::string, std::unique_ptr<PIController>> m_controllers;
  Logger* m_logger;
  // Add this member to track window visibility
  bool m_isWindowVisible = false;
};