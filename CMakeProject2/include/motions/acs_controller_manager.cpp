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

bool ACSControllerManager::ExecutePositionMove(ACSController* controller,
  const std::string& deviceName,
  const std::string& positionName,
  const PositionStruct& position) {

  const auto& availableAxes = controller->GetAvailableAxes();

  // Create vectors to hold the axes and positions for multi-axis movement
  std::vector<std::string> axesToMove;
  std::vector<double> positionsToMove;

  // Add X axis position if available
  if (std::find(availableAxes.begin(), availableAxes.end(), "X") != availableAxes.end()) {
    axesToMove.push_back("X");
    positionsToMove.push_back(position.x);
  }

  // Add Y axis position if available
  if (std::find(availableAxes.begin(), availableAxes.end(), "Y") != availableAxes.end()) {
    axesToMove.push_back("Y");
    positionsToMove.push_back(position.y);
  }

  // Add Z axis position if available
  if (std::find(availableAxes.begin(), availableAxes.end(), "Z") != availableAxes.end()) {
    axesToMove.push_back("Z");
    positionsToMove.push_back(position.z);
  }

  // If no axes to move, return success
  if (axesToMove.empty()) {
    return true;
  }

  // Log the multi-axis movement
  m_logger->LogInfo("ACSControllerManager: Moving " + deviceName + " to position " +
    positionName + " with multi-axis movement");

  // Execute multi-axis movement
  bool success = controller->MoveToPositionMultiAxis(axesToMove, positionsToMove, true);

  if (!success) {
    m_logger->LogError("ACSControllerManager: Failed multi-axis movement for device " + deviceName);
  }

  return success;
}

// Modified ACSControllerManager::RenderUI method with X button support

void ACSControllerManager::RenderUI() {
  // Only render if the window is visible
  if (!m_isWindowVisible) {
    return;
  }

  // Create a window to show all controllers - with a close button
  bool windowOpen = m_isWindowVisible;
  if (!ImGui::Begin("ACS Controller Manager", &windowOpen, ImGuiWindowFlags_None)) {
    ImGui::End();
    return;
  }

  // Check if window was closed via the X button
  if (!windowOpen) {
    m_isWindowVisible = false;
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

        // Skip if no positions besides "home"
        if (positions.size() <= 1) {
          ImGui::Text("No additional positions available");
        }
        else {
          // Create a child window with fixed height and scrolling
          ImGui::Text("Available Positions:");
          ImGui::BeginChild("PositionButtonsChild", ImVec2(-1, 150), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

          const int COLUMNS = 3;
          const float columnWidth = ImGui::GetContentRegionAvail().x / COLUMNS;
          const float buttonWidth = columnWidth - 8.0f; // Add some padding
          const float buttonHeight = 30.0f;

          // Count positions (excluding home)
          int posCount = 0;
          for (const auto& position_pair : positions) {
            if (position_pair.first != "home") posCount++;
          }

          // Calculate number of rows needed
          int rows = (posCount + COLUMNS - 1) / COLUMNS;

          // Use a grid layout instead of columns
          int index = 0;
          for (int row = 0; row < rows; row++) {
            for (int col = 0; col < COLUMNS; col++) {
              // Find the next non-home position
              std::string posName;
              while (index < positions.size()) {
                auto it = positions.begin();
                std::advance(it, index++);
                if (it->first != "home") {
                  posName = it->first;
                  break;
                }
              }

              // If we run out of positions, break out
              if (posName.empty()) break;

              // Position the button
              if (col > 0) ImGui::SameLine(col * columnWidth);

              // Create the button
              if (ImGui::Button(posName.c_str(), ImVec2(buttonWidth, buttonHeight))) {
                MoveToNamedPosition(selectedController, posName, true);
              }
            }
          }

          ImGui::EndChild();
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