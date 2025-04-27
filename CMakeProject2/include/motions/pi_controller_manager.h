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

  // Move a device to a named position
  bool MoveToNamedPosition(const std::string& deviceName, const std::string& positionName, bool blocking = true);

  // Render UI for all controllers
  void RenderUI();

private:
  MotionConfigManager& m_configManager;
  std::map<std::string, std::unique_ptr<PIController>> m_controllers;
  Logger* m_logger;
};