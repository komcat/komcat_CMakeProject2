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

  // Prepare axis names and positions for batch move
  std::vector<std::string> axes = { "X", "Y", "Z", "U", "V", "W" };
  std::vector<double> positions = { position.x, position.y, position.z, position.u, position.v, position.w };

  // Use the multi-axis move function that handles all axes at once
  return controller->MoveToPositionMultiAxis(axes, positions, blocking);
}
// In pi_controller_manager.cpp, modify the RenderUI method:

// pi_controller_manager.cpp - updated RenderUI method
// Modified PIControllerManager::RenderUI method with X button support

void PIControllerManager::RenderUI() {
  // Only render if the window is visible
  if (!m_isWindowVisible) {
    return;
  }

  // Create a window with a close button
  bool windowOpen = m_isWindowVisible;
  if (!ImGui::Begin("PI Controller Manager", &windowOpen, ImGuiWindowFlags_None)) {
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

  // List all controllers with their connection status and positions
  for (const auto& [name, controller] : m_controllers) {
    ImGui::PushID(name.c_str());

    auto deviceOpt = m_configManager.GetDevice(name);
    bool isEnabled = deviceOpt.has_value() && deviceOpt.value().get().IsEnabled;
    bool isConnected = controller->IsConnected();

    // Display device name and status
    ImVec4 statusColor = isConnected ? ImVec4(0.0f, 0.8f, 0.0f, 1.0f) : ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
    ImGui::TextColored(statusColor, "%s", isConnected ? "Y " : "N ");
    ImGui::SameLine();
    ImGui::Text("%s: %s %s",
      name.c_str(),
      isEnabled ? "(Enabled)" : "(Disabled)",
      isConnected ? "[Connected]" : "[Disconnected]");

    // Allow opening individual controller windows
    if (ImGui::Button("Open Control Panel")) {
      // Set the showWindow flag to true explicitly
      controller->SetWindowVisible(true);
    }

    // If controller is connected, show available positions
    if (isConnected) {
      // Get the positions for this device
      auto positionsOpt = m_configManager.GetDevicePositions(name);
      if (positionsOpt.has_value()) {
        const auto& positions = positionsOpt.value().get();
        if (!positions.empty()) {
          // Indent all position buttons to make hierarchy clear
          ImGui::Indent(20.0f);

          // Display position buttons in a compact layout
          ImGui::Text("Positions:");
          ImGui::SameLine();

          // Calculate available width for position buttons
          float availableWidth = ImGui::GetContentRegionAvail().x;
          float buttonWidth = 80.0f; // Default button width
          float spacing = 5.0f;
          float xPos = ImGui::GetCursorPosX();
          float initialX = xPos;

          // Add position buttons
          bool firstButton = true;
          for (const auto& [posName, position] : positions) {
            // Check if we need to wrap to next line
            if (!firstButton && xPos + buttonWidth > initialX + availableWidth) {
              ImGui::NewLine();
              xPos = initialX;
            }

            // Set the next button position
            ImGui::SetCursorPosX(xPos);

            // Create the button
            ImVec4 buttonColor = ImVec4(0.3f, 0.5f, 0.7f, 0.7f);
            ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.6f, 0.8f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.7f, 0.9f, 0.9f));

            if (ImGui::Button(posName.c_str(), ImVec2(buttonWidth, 25))) {
              // When clicked, move to this position
              MoveToNamedPosition(name, posName, false);
            }

            ImGui::PopStyleColor(3);

            // If tooltip is required, add it
            if (ImGui::IsItemHovered()) {
              ImGui::BeginTooltip();
              ImGui::Text("X: %.3f, Y: %.3f, Z: %.3f", position.x, position.y, position.z);
              if (position.u != 0.0 || position.v != 0.0 || position.w != 0.0) {
                ImGui::Text("U: %.3f, V: %.3f, W: %.3f", position.u, position.v, position.w);
              }
              ImGui::EndTooltip();
            }

            // Stay on same line for next button
            ImGui::SameLine();
            xPos = ImGui::GetCursorPosX() + spacing;
            firstButton = false;
          }

          // End the line after all buttons
          ImGui::NewLine();
          ImGui::Unindent(20.0f);
        }
      }
    }

    ImGui::Separator();
    ImGui::PopID();
  }

  ImGui::End();
}