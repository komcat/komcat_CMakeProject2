// acs_controller_manager.cpp
#include "include/motions/acs_controller_manager.h"
#include "imgui.h"
#include <iostream>

ACSControllerManager::ACSControllerManager(MotionConfigManager& configManager)
  : m_configManager(configManager) {
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("ACSControllerManager: Initializing");

  // Initialize controllers from configuration
  InitializeControllers();
}

ACSControllerManager::~ACSControllerManager() {
  m_logger->LogInfo("ACSControllerManager: Shutting down");
  DisconnectAll();
}

void ACSControllerManager::InitializeControllers() {
  m_logger->LogInfo("ACSControllerManager: Creating controllers from configuration");

  // Get all devices from configuration
  const auto& devices = m_configManager.GetAllDevices();

  // In ACSControllerManager::InitializeControllers()
  for (const auto& [name, device] : devices) {
    // Check if this is an ACS device by checking the port
    // Assuming ACS controllers use port ACSC_SOCKET_STREAM_PORT (typically 701)
    if (device.Port == ACSC_SOCKET_STREAM_PORT) {
      m_logger->LogInfo("ACSControllerManager: Creating controller for device " + name);

      // Create a new controller
      auto controller = std::make_unique<ACSController>();

      // Configure the controller from the device
      controller->ConfigureFromDevice(device);

      // Set a unique window title
      controller->SetWindowTitle("ACS Controller: " + name);

      // Add to our map
      m_controllers[name] = std::move(controller);
    }
  }

  m_logger->LogInfo("ACSControllerManager: Created " + std::to_string(m_controllers.size()) + " controllers");
}

bool ACSControllerManager::ConnectAll() {
  m_logger->LogInfo("ACSControllerManager: Connecting all enabled controllers");

  bool allConnected = true;

  for (const auto& [name, controller] : m_controllers) {
    // Get the device info to check if it's enabled
    auto deviceOpt = m_configManager.GetDevice(name);
    if (!deviceOpt.has_value()) {
      m_logger->LogError("ACSControllerManager: Device " + name + " not found in configuration");
      continue;
    }

    const auto& device = deviceOpt.value().get();

    // Only connect enabled devices
    if (device.IsEnabled) {
      m_logger->LogInfo("ACSControllerManager: Connecting to " + name + " (" + device.IpAddress + ")");

      if (!controller->Connect(device.IpAddress, device.Port)) {
        m_logger->LogError("ACSControllerManager: Failed to connect to " + name);
        allConnected = false;
      }
    }
  }

  return allConnected;
}

void ACSControllerManager::DisconnectAll() {
  m_logger->LogInfo("ACSControllerManager: Disconnecting all controllers");

  for (auto& [name, controller] : m_controllers) {
    if (controller->IsConnected()) {
      m_logger->LogInfo("ACSControllerManager: Disconnecting " + name);
      controller->Disconnect();
    }
  }
}

ACSController* ACSControllerManager::GetController(const std::string& deviceName) {
  auto it = m_controllers.find(deviceName);
  if (it != m_controllers.end()) {
    return it->second.get();
  }

  return nullptr;
}

bool ACSControllerManager::HasController(const std::string& deviceName) const {
  return m_controllers.find(deviceName) != m_controllers.end();
}

bool ACSControllerManager::MoveToNamedPosition(const std::string& deviceName, const std::string& positionName, bool blocking) {
  // Get the controller for this device
  auto controller = GetController(deviceName);
  if (!controller) {
    m_logger->LogError("ACSControllerManager: No controller found for device " + deviceName);
    return false;
  }

  if (!controller->IsConnected()) {
    m_logger->LogError("ACSControllerManager: Controller for device " + deviceName + " is not connected");
    return false;
  }

  // Get the position from configuration
  auto posOpt = m_configManager.GetNamedPosition(deviceName, positionName);
  if (!posOpt.has_value()) {
    m_logger->LogError("ACSControllerManager: Position " + positionName + " not found for device " + deviceName);
    return false;
  }

  const auto& position = posOpt.value().get();

  m_logger->LogInfo("ACSControllerManager: Moving " + deviceName + " to position " + positionName);

  // If blocking is requested but we're on the main thread, create a new thread
  if (blocking && ImGui::GetCurrentContext() != nullptr) {
    // We're likely on the UI thread, so let's run this asynchronously
    std::thread moveThread([this, deviceName, positionName, controller, position]() {
      this->ExecutePositionMove(controller, deviceName, positionName, position);
    });
    moveThread.detach(); // Detach the thread to let it run independently
    return true; // Return immediately to prevent UI freezing
  }
  else {
    // Execute directly if not blocking or not on UI thread
    return ExecutePositionMove(controller, deviceName, positionName, position);
  }
}

// Add this helper method to handle the actual movement
bool ACSControllerManager::ExecutePositionMove(ACSController* controller,
  const std::string& deviceName,
  const std::string& positionName,
  const PositionStruct& position) {
  bool success = true;
  const auto& availableAxes = controller->GetAvailableAxes();

  // Move X axis if available
  if (std::find(availableAxes.begin(), availableAxes.end(), "X") != availableAxes.end()) {
    if (!controller->MoveToPosition("X", position.x, false)) {
      m_logger->LogError("ACSControllerManager: Failed to move X axis");
      success = false;
    }
  }

  // Move Y axis if available
  if (std::find(availableAxes.begin(), availableAxes.end(), "Y") != availableAxes.end()) {
    if (!controller->MoveToPosition("Y", position.y, false)) {
      m_logger->LogError("ACSControllerManager: Failed to move Y axis");
      success = false;
    }
  }

  // Move Z axis if available
  if (std::find(availableAxes.begin(), availableAxes.end(), "Z") != availableAxes.end()) {
    if (!controller->MoveToPosition("Z", position.z, false)) {
      m_logger->LogError("ACSControllerManager: Failed to move Z axis");
      success = false;
    }
  }

  // Wait for each axis to complete movement
  if (success) {
    for (const auto& axis : availableAxes) {
      if (!controller->WaitForMotionCompletion(axis)) {
        m_logger->LogError("ACSControllerManager: Timeout waiting for axis " + axis + " to complete motion");
        success = false;
      }
    }
  }

  return success;
}

void ACSControllerManager::RenderUI() {
  // Create a window to show all controllers
  if (!ImGui::Begin("ACS Controller Manager")) {
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

  // Add a dropdown for controller selection
  static std::string selectedController;
  if (selectedController.empty() && !m_controllers.empty()) {
    selectedController = m_controllers.begin()->first; // Default to first controller
  }

  if (ImGui::BeginCombo("Select Controller", selectedController.c_str())) {
    for (const auto& [name, controller] : m_controllers) {
      bool isSelected = (selectedController == name);
      if (ImGui::Selectable(name.c_str(), isSelected)) {
        selectedController = name;
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  // Get the selected controller
  ACSController* selectedControllerPtr = nullptr;
  if (!selectedController.empty()) {
    selectedControllerPtr = GetController(selectedController);
  }

  ImGui::Separator();

  // Display controls for the selected controller
  if (selectedControllerPtr) {
    auto deviceOpt = m_configManager.GetDevice(selectedController);
    bool isEnabled = deviceOpt.has_value() && deviceOpt.value().get().IsEnabled;
    bool isConnected = selectedControllerPtr->IsConnected();

    // Display device name and status
    ImGui::Text("Selected: %s %s %s",
      selectedController.c_str(),
      isEnabled ? "(Enabled)" : "(Disabled)",
      isConnected ? "[Connected]" : "[Disconnected]");

    // Actions for the selected controller
    if (ImGui::Button("Open Control Panel")) {
      selectedControllerPtr->SetWindowVisible(true);
    }

    // Add buttons for common positions if connected
    if (isConnected) {
      ImGui::SameLine();
      if (ImGui::Button("Home")) {
        MoveToNamedPosition(selectedController, "home", true);
      }

      // Get all available positions for this device
      auto positionsOpt = m_configManager.GetDevicePositions(selectedController);
      if (positionsOpt.has_value()) {
        const auto& positions = positionsOpt.value().get();
        for (const auto& position_pair : positions) {
          const std::string& posName = position_pair.first;
          if (posName != "home") {
            ImGui::SameLine();
            if (ImGui::Button(posName.c_str())) {
              MoveToNamedPosition(selectedController, posName, true);
            }
          }
        }
      }
    }
  }

  ImGui::Separator();

  // Still list all controllers for reference
  ImGui::Text("All Controllers:");
  for (const auto& [name, controller] : m_controllers) {
    ImGui::PushID(name.c_str());

    auto deviceOpt = m_configManager.GetDevice(name);
    bool isEnabled = deviceOpt.has_value() && deviceOpt.value().get().IsEnabled;
    bool isConnected = controller->IsConnected();

    // Display device name and status
    ImGui::Bullet();
    ImGui::Text("%s: %s %s",
      name.c_str(),
      isEnabled ? "(Enabled)" : "(Disabled)",
      isConnected ? "[Connected]" : "[Disconnected]");

    ImGui::PopID();
  }

  ImGui::End();
}