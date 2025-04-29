// pi_controller_manager.cpp
#include "pi_controller_manager.h"
#include "imgui.h"
#include <iostream>

PIControllerManager::PIControllerManager(MotionConfigManager& configManager)
  : m_configManager(configManager) {
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("PIControllerManager: Initializing");

  // Initialize controllers from configuration
  InitializeControllers();
}

PIControllerManager::~PIControllerManager() {
  m_logger->LogInfo("PIControllerManager: Shutting down");
  DisconnectAll();
}

void PIControllerManager::InitializeControllers() {
  m_logger->LogInfo("PIControllerManager: Creating controllers from configuration");

  // Get all devices from configuration
  const auto& devices = m_configManager.GetAllDevices();

  // In PIControllerManager::InitializeControllers()
  for (const auto& [name, device] : devices) {
    if (device.Port == 50000) {
      m_logger->LogInfo("PIControllerManager: Creating controller for device " + name);

      // Create a new controller
      auto controller = std::make_unique<PIController>();

      // Configure the controller from the device
      controller->ConfigureFromDevice(device);

      // Set a unique window title
      controller->SetWindowTitle("Controller: " + name);

      // Add to our map
      m_controllers[name] = std::move(controller);
    }
  }

  m_logger->LogInfo("PIControllerManager: Created " + std::to_string(m_controllers.size()) + " controllers");
}

bool PIControllerManager::ConnectAll() {
  m_logger->LogInfo("PIControllerManager: Connecting all enabled controllers");

  bool allConnected = true;

  for (const auto& [name, controller] : m_controllers) {
    // Get the device info to check if it's enabled
    auto deviceOpt = m_configManager.GetDevice(name);
    if (!deviceOpt.has_value()) {
      m_logger->LogError("PIControllerManager: Device " + name + " not found in configuration");
      continue;
    }

    const auto& device = deviceOpt.value().get();

    // Only connect enabled devices
    if (device.IsEnabled) {
      m_logger->LogInfo("PIControllerManager: Connecting to " + name + " (" + device.IpAddress + ")");

      if (!controller->Connect(device.IpAddress, device.Port)) {
        m_logger->LogError("PIControllerManager: Failed to connect to " + name);
        allConnected = false;
      }
    }
  }

  return allConnected;
}

void PIControllerManager::DisconnectAll() {
  m_logger->LogInfo("PIControllerManager: Disconnecting all controllers");

  for (auto& [name, controller] : m_controllers) {
    if (controller->IsConnected()) {
      m_logger->LogInfo("PIControllerManager: Disconnecting " + name);
      controller->Disconnect();
    }
  }
}

PIController* PIControllerManager::GetController(const std::string& deviceName) {
  auto it = m_controllers.find(deviceName);
  if (it != m_controllers.end()) {
    return it->second.get();
  }

  return nullptr;
}

bool PIControllerManager::HasController(const std::string& deviceName) const {
  return m_controllers.find(deviceName) != m_controllers.end();
}

bool PIControllerManager::MoveToNamedPosition(const std::string& deviceName, const std::string& positionName, bool blocking) {
  auto controller = GetController(deviceName);
  if (!controller) {
    m_logger->LogError("PIControllerManager: No controller found for device " + deviceName);
    return false;
  }

  if (!controller->IsConnected()) {
    m_logger->LogError("PIControllerManager: Controller for device " + deviceName + " is not connected");
    return false;
  }

  // Get the position from configuration
  auto posOpt = m_configManager.GetNamedPosition(deviceName, positionName);
  if (!posOpt.has_value()) {
    m_logger->LogError("PIControllerManager: Position " + positionName + " not found for device " + deviceName);
    return false;
  }

  const auto& position = posOpt.value().get();

  m_logger->LogInfo("PIControllerManager: Moving " + deviceName + " to position " + positionName);

  // Move each axis to its target position
  bool success = true;

  // Move X axis
  if (!controller->MoveToPosition("X", position.x, false)) {
    m_logger->LogError("PIControllerManager: Failed to move X axis");
    success = false;
  }

  // Move Y axis
  if (!controller->MoveToPosition("Y", position.y, false)) {
    m_logger->LogError("PIControllerManager: Failed to move Y axis");
    success = false;
  }

  // Move Z axis
  if (!controller->MoveToPosition("Z", position.z, false)) {
    m_logger->LogError("PIControllerManager: Failed to move Z axis");
    success = false;
  }

  // Move U axis (roll)
  if (!controller->MoveToPosition("U", position.u, false)) {
    m_logger->LogError("PIControllerManager: Failed to move U axis");
    success = false;
  }

  // Move V axis (pitch)
  if (!controller->MoveToPosition("V", position.v, false)) {
    m_logger->LogError("PIControllerManager: Failed to move V axis");
    success = false;
  }

  // Move W axis (yaw)
  if (!controller->MoveToPosition("W", position.w, false)) {
    m_logger->LogError("PIControllerManager: Failed to move W axis");
    success = false;
  }

  // If blocking mode requested, wait for all axes to complete motion
  if (blocking && success) {
    // Wait for each axis to complete movement
    for (const auto& axis : { "X", "Y", "Z", "U", "V", "W" }) {
      if (!controller->WaitForMotionCompletion(axis)) {
        m_logger->LogError("PIControllerManager: Timeout waiting for axis " + std::string(axis) + " to complete motion");
        success = false;
      }
    }
  }

  return success;
}

// In pi_controller_manager.cpp, modify the RenderUI method:

void PIControllerManager::RenderUI() {
  // Create a window to show all controllers
  if (!ImGui::Begin("PI Controller Manager")) {
    ImGui::End();
    return;
  }

  // Connection controls for all controllers
  if (ImGui::Button("Connect All")) {
    ConnectAll();
  }
  ImGui::SameLine();
  if (ImGui::Button("Disconnect All")) {
    DisconnectAll();
  }




  ImGui::Separator();

  // List all controllers with their connection status
  for (const auto& [name, controller] : m_controllers) {
    ImGui::PushID(name.c_str());

    auto deviceOpt = m_configManager.GetDevice(name);
    bool isEnabled = deviceOpt.has_value() && deviceOpt.value().get().IsEnabled;
    bool isConnected = controller->IsConnected();

    // Display device name and status
    ImGui::Text("%s: %s %s",
      name.c_str(),
      isEnabled ? "(Enabled)" : "(Disabled)",
      isConnected ? "[Connected]" : "[Disconnected]");

    // Allow opening individual controller windows
    if (ImGui::Button("Open Control Panel")) {
      // Set the showWindow flag to true explicitly
      controller->SetWindowVisible(true);
    }



    ImGui::PopID();
    ImGui::Separator();
  }

  ImGui::End();
}