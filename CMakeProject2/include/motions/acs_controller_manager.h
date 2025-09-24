// acs_controller_manager.h
#pragma once
// In acs_controller_manager.h - Add these includes at the top
#include "include/data/global_data_store.h"
#include "include/motions/ACSMotionSubscriber.h"
#include "acs_controller.h"
#include "MotionConfigManager.h"
#include <string>
#include <map>
#include <memory>
#include <chrono>
#include <thread>

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

  // Connect/disconnect individual devices
  bool ConnectDevice(const std::string& deviceName);
  bool DisconnectDevice(const std::string& deviceName);
  bool ReconnectDevice(const std::string& deviceName);

  // Connect only enabled devices (alternative to ConnectAll)
  int ConnectEnabledDevices();

  // Get connection status of all controllers
  std::map<std::string, bool> GetConnectionStatus() const;

  // Get a specific controller by device name
  ACSController* GetController(const std::string& deviceName);

  // Get the number of controllers managed  
  size_t GetControllerCount() const { return m_controllers.size(); }

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

  // ACS-specific buffer program methods (acsc_ prefix)
  bool acsc_RunBuffer(const std::string& deviceName, int bufferNumber, const std::string& labelName = "");
  bool acsc_StopBuffer(const std::string& deviceName, int bufferNumber);
  bool acsc_StopAllBuffers(const std::string& deviceName);
  bool acsc_IsBufferRunning(const std::string& deviceName, int bufferNumber);


  // Add to acs_controller_manager.h
  std::vector<std::string> GetDeviceNames() const {
    std::vector<std::string> names;
    for (const auto& [name, controller] : m_controllers) {
      names.push_back(name);
    }
    return names;
  }

  std::vector<std::string> GetConnectedDeviceNames() const {
    std::vector<std::string> names;
    for (const auto& [name, controller] : m_controllers) {
      if (controller && controller->IsConnected()) {
        names.push_back(name);
      }
    }
    return names;
  }

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