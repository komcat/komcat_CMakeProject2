// UICameraPanel.cpp - Main Camera Panel Implementation
#include "UICameraPanel.h"
#include "UICameraPanelLiveVideo.h"
#include "UICameraPanelSingleGrab.h"
#include "UICameraPanelUtility.h"
#include "include/camera/CameraManager.h"
#include "include/camera/ICameraHardware.h"
#include "imgui.h"
#include <iostream>

UICameraPanel::UICameraPanel(CameraManager& cameraManager)
  : m_cameraManager(cameraManager) {

  // Create sub-panels
  m_liveVideoPanel = std::make_unique<UICameraPanelLiveVideo>(cameraManager);
  m_singleGrabPanel = std::make_unique<UICameraPanelSingleGrab>(cameraManager);
  m_utilityPanel = std::make_unique<UICameraPanelUtility>(cameraManager);

  std::cout << "[INFO] UICameraPanel created with ICameraHardware interface" << std::endl;
}

UICameraPanel::~UICameraPanel() {
  ClearAllPanels();
  std::cout << "[INFO] UICameraPanel destroyed" << std::endl;
}

void UICameraPanel::RenderUI() {
  if (!m_showWindow) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(1400, 800), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Camera Control Panel", &m_showWindow, ImGuiWindowFlags_MenuBar)) {

    // Menu bar
    if (ImGui::BeginMenuBar()) {
      if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Camera List", nullptr, &m_showWindow);
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Tools")) {
        if (ImGui::MenuItem("Initialize All Cameras")) {
          m_cameraManager.InitializeAllCameras();
        }
        if (ImGui::MenuItem("Refresh Camera List")) {
          // Force refresh of camera list
          std::cout << "[INFO] Refreshing camera list..." << std::endl;
        }
        ImGui::EndMenu();
      }
      ImGui::EndMenuBar();
    }

    // Main content area
    ImVec2 contentSize = ImGui::GetContentRegionAvail();

    // Calculate panel widths
    float leftPanelWidth = contentSize.x * 0.25f;   // 25% for camera list and controls
    float middlePanelWidth = contentSize.x * 0.45f; // 45% for video feeds
    float rightPanelWidth = contentSize.x * 0.30f;  // 30% for camera settings

    // Left Panel - Camera list and global controls
    ImGui::BeginChild("LeftPanel", ImVec2(leftPanelWidth, contentSize.y), true);
    RenderLeftPanel();
    ImGui::EndChild();

    ImGui::SameLine();

    // Middle Panel - Tabbed video display
    ImGui::BeginChild("MiddlePanel", ImVec2(middlePanelWidth, contentSize.y), true);
    RenderMiddlePanelTabs();
    ImGui::EndChild();

    ImGui::SameLine();

    // Right Panel - Camera controls and settings
    ImGui::BeginChild("RightPanel", ImVec2(rightPanelWidth, contentSize.y), true);
    RenderRightPanel();
    ImGui::EndChild();
  }
  ImGui::End();
}

void UICameraPanel::RenderLeftPanel() {
  ImGui::Text("Camera System");
  ImGui::Separator();

  // Camera list first, at the top
  RenderCameraList();

  // Separator before global controls
  ImGui::Separator();

  // Global controls moved to bottom
  RenderGlobalControls();
}

void UICameraPanel::RenderGlobalControls() {
  ImGui::Text("Global Controls");
  ImGui::Spacing();

  if (ImGui::Button("Initialize All", ImVec2(-1, 30))) {
    m_cameraManager.InitializeAllCameras();
  }

  if (ImGui::Button("Start All", ImVec2(-1, 30))) {
    m_cameraManager.StartGrabbingAll();
  }

  if (ImGui::Button("Stop All", ImVec2(-1, 30))) {
    m_cameraManager.StopGrabbingAll();
  }

  if (ImGui::Button("Capture All", ImVec2(-1, 30))) {
    m_cameraManager.CaptureImageAll();
  }

  // Show where images are saved
  ImGui::Spacing();
  ImGui::Text("Image Output:");
  std::string outputDir = m_cameraManager.GetImageOutputDirectory();
  ImGui::TextWrapped("%s", outputDir.c_str());

  ImGui::Spacing();
  ImGui::Text("Total Cameras: %zu", m_cameraManager.GetCameraCount());

  ImGui::Separator();
  ImGui::Text("Camera Discovery:");

  // Manual IDS camera addition for testing
  if (ImGui::Button("Add IDS Camera (ID: 0)")) {
    CameraInfo testIDSCamera("IDS_Test_Camera", "Test IDS Camera");
    if (m_cameraManager.AddIDSCamera(testIDSCamera, 0)) {
      std::cout << "[INFO] Successfully added IDS test camera" << std::endl;
    }
    else {
      std::cout << "[ERROR] Failed to add IDS test camera" << std::endl;
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Discover All Cameras")) {
    auto discoveredCameras = CameraManager::DiscoverAllCameras();
    std::cout << "[INFO] Discovered " << discoveredCameras.size() << " cameras" << std::endl;

    for (const auto& camera : discoveredCameras) {
      std::cout << "[INFO] Found: " << camera.id << " (Type: "
        << (camera.type == ICameraHardware::CameraType::IDS ? "IDS" : "Pylon")
        << ")" << std::endl;
    }
  }
}

void UICameraPanel::RenderMiddlePanelTabs() {
  if (m_selectedCameraId.empty()) {
    ImGui::Text("Camera Feed Viewer");
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Text("No camera selected");
    ImGui::Spacing();
    ImGui::Text("Select a camera from the left panel");
    ImGui::Text("to view feeds here");
    return;
  }

  // Get selected camera via ICameraHardware interface
  ICameraHardware* camera = GetSelectedCamera();
  if (!camera) {
    ImGui::Text("Selected camera not available");
    ClearAllPanels();
    return;
  }

  // Camera info header
  ImGui::Text("Camera: %s (%s)", m_selectedCameraId.c_str(),
    camera->GetModelName().c_str());
  ImGui::Separator();

  // Tab Bar for Live Feed vs Single Frame
  if (ImGui::BeginTabBar("CameraFeedTabs", ImGuiTabBarFlags_None)) {

    // TAB 1: Live Video Feed
    if (ImGui::BeginTabItem("Live Video")) {
      m_liveVideoPanel->RenderTab(camera, m_selectedCameraId);
      ImGui::EndTabItem();
    }

    // TAB 2: Single Frame Capture
    if (ImGui::BeginTabItem("Single Frame")) {
      m_singleGrabPanel->RenderTab(camera, m_selectedCameraId);
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }
}

void UICameraPanel::RenderRightPanel() {
  if (m_selectedCameraId.empty()) {
    RenderNoSelectionMessage();
  }
  else {
    ICameraHardware* camera = GetSelectedCamera();

    if (camera) {
      // Check connection status and provide appropriate UI
      if (!camera->IsConnected()) {
        // Only log once per camera selection change, not every frame
        static std::string lastDisconnectedCamera = "";
        if (lastDisconnectedCamera != m_selectedCameraId) {
          std::cout << "[DEBUG] Camera " << m_selectedCameraId << " found but not connected" << std::endl;
          lastDisconnectedCamera = m_selectedCameraId;
        }

        // Header for disconnected camera
        ImGui::SetWindowFontScale(1.2f);
        ImGui::Text("Camera: %s", m_selectedCameraId.c_str());
        ImGui::SetWindowFontScale(1.0f);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Status information
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Status: Disconnected");

        // Camera type information
        const char* typeStr = (camera->GetCameraType() == ICameraHardware::CameraType::PYLON) ? "Pylon" : "IDS";
        ImVec4 typeColor = (camera->GetCameraType() == ICameraHardware::CameraType::PYLON) ?
          ImVec4(0.3f, 0.8f, 0.3f, 1.0f) : ImVec4(0.3f, 0.3f, 0.8f, 1.0f);

        ImGui::Text("Type: ");
        ImGui::SameLine();
        ImGui::TextColored(typeColor, "%s", typeStr);

        // Show connection method for IDS cameras
        if (camera->GetCameraType() == ICameraHardware::CameraType::IDS) {
          ImGui::Text("Connection: USB Device Index");
          ImGui::TextColored(ImVec4(1, 1, 0, 1), "Note: IDS camera requires USB connection");
        }

        // Try to get model name (might be available even when disconnected)
        try {
          std::string modelName = camera->GetModelName();
          if (!modelName.empty()) {
            ImGui::Text("Model: %s", modelName.c_str());
          }
        }
        catch (...) {
          ImGui::Text("Model: Unknown (not connected)");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Different instructions based on camera type
        if (camera->GetCameraType() == ICameraHardware::CameraType::IDS) {
          ImGui::Text("IDS Camera Connection:");
          ImGui::TextColored(ImVec4(1, 1, 0, 1), "1. Ensure IDS camera is connected via USB");
          ImGui::TextColored(ImVec4(1, 1, 0, 1), "2. Check that IDS drivers are installed");
          ImGui::TextColored(ImVec4(1, 1, 0, 1), "3. Verify camera appears in IDS Camera Manager");
        }
        else {
          ImGui::Text("Pylon Camera Connection:");
          ImGui::TextColored(ImVec4(1, 1, 0, 1), "1. Check network cable connection");
          ImGui::TextColored(ImVec4(1, 1, 0, 1), "2. Verify IP address is reachable");
        }

        ImGui::Spacing();

        // Connect button - make it prominent
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));

        if (ImGui::Button("Connect Camera", ImVec2(-1, 40))) {
          std::cout << "[INFO] Attempting to connect " << typeStr << " camera: " << m_selectedCameraId << std::endl;

          // For IDS cameras, might need special handling
          if (camera->GetCameraType() == ICameraHardware::CameraType::IDS) {
            std::cout << "[INFO] Connecting IDS camera via USB device index..." << std::endl;
          }

          bool success = m_cameraManager.ConnectCamera(m_selectedCameraId);

          if (success) {
            std::cout << "[INFO] Successfully connected camera: " << m_selectedCameraId << std::endl;
            lastDisconnectedCamera = ""; // Reset to allow new logging
          }
          else {
            std::cout << "[ERROR] Failed to connect camera: " << m_selectedCameraId << std::endl;
            if (camera->GetCameraType() == ICameraHardware::CameraType::IDS) {
              std::cout << "[ERROR] IDS camera connection failed. Check:" << std::endl;
              std::cout << "[ERROR] - USB cable connection" << std::endl;
              std::cout << "[ERROR] - IDS driver installation" << std::endl;
              std::cout << "[ERROR] - Camera power and device index" << std::endl;
            }
          }
        }

        ImGui::PopStyleColor(3);

        ImGui::Spacing();

        // Additional troubleshooting for IDS cameras
        if (camera->GetCameraType() == ICameraHardware::CameraType::IDS) {
          ImGui::Separator();
          ImGui::Text("IDS Camera Troubleshooting:");

          if (ImGui::Button("Check IDS Camera Discovery", ImVec2(-1, 25))) {
            std::cout << "[INFO] Checking for IDS cameras..." << std::endl;
            // This would call IDS-specific discovery if available
            // You might want to add a method to check IDS camera availability
          }

          if (ImGui::Button("Reinitialize IDS System", ImVec2(-1, 25))) {
            std::cout << "[INFO] Reinitializing IDS camera system..." << std::endl;
            // This could reinitialize the IDS camera subsystem
          }
        }

        // General actions
        ImGui::Spacing();
        ImGui::Text("General Actions:");
        if (ImGui::Button("Initialize All Cameras", ImVec2(-1, 25))) {
          std::cout << "[INFO] Initializing all cameras..." << std::endl;
          m_cameraManager.InitializeAllCameras();
        }

        return;
      }

      // Camera is connected - render normal utility panel
      m_utilityPanel->RenderPanel(camera, m_selectedCameraId);
    }
    else {
      // Camera not found
      ImGui::SetWindowFontScale(1.2f);
      ImGui::Text("Camera: %s", m_selectedCameraId.c_str());
      ImGui::SetWindowFontScale(1.0f);

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Status: Camera Not Found");

      ImGui::Spacing();
      ImGui::Text("The selected camera could not be found.");
      ImGui::Text("Try initializing cameras:");

      if (ImGui::Button("Initialize All Cameras", ImVec2(-1, 30))) {
        std::cout << "[INFO] Initializing all cameras..." << std::endl;
        m_cameraManager.InitializeAllCameras();
      }
    }
  }
}


void UICameraPanel::RenderCameraList() {
  ImGui::Text("Camera Selection");
  ImGui::Spacing();

  // Get all camera IDs from the camera manager
  std::vector<std::string> cameraIds = m_cameraManager.GetCameraIds();

  if (cameraIds.empty()) {
    
    // Enlarged placeholder for better visibility
    ImGui::BeginChild("CameraListPlaceholder", ImVec2(-1, 150), true, ImGuiWindowFlags_NoScrollbar);

    ImGui::Spacing();
    ImGui::Text("No cameras available");
    ImGui::Spacing();
    ImGui::TextWrapped("Use 'Add Camera' to add cameras to the system");
    ImGui::Spacing();

    // Add helpful instructions
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Steps to add cameras:");
    ImGui::BulletText("Connect camera hardware");
    ImGui::BulletText("Use Initialize All button");
    ImGui::BulletText("Cameras will appear here");

    ImGui::EndChild();
    return;
  }

  // Enlarged camera list - Calculate height for better visibility
  ImVec2 availableSize = ImGui::GetContentRegionAvail();
  float listHeight = (std::min)(200.0f, availableSize.y * 0.6f);

  ImGui::BeginChild("CameraList", ImVec2(-1, listHeight), true);

  for (const std::string& cameraId : cameraIds) {
    // Get camera status for display
    auto status = m_cameraManager.GetCameraStatus(cameraId);
    //std::cout << "[DEBUG] Camera " << cameraId << " type: " << (int)status.type << std::endl;
    // Color coding based on camera type and status
    ImVec4 textColor = ImVec4(1, 1, 1, 1); // Default white
    if (status.type == ICameraHardware::CameraType::PYLON) {
      textColor = ImVec4(0.3f, 0.8f, 0.3f, 1.0f); // Green for Pylon
    }
    else if (status.type == ICameraHardware::CameraType::IDS) {
      textColor = ImVec4(0.3f, 0.3f, 0.8f, 1.0f); // Blue for IDS
    }

    // Selection button with color coding
    bool isSelected = (m_selectedCameraId == cameraId);
    if (isSelected) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.6f, 0.9f, 1.0f));
    }

    ImGui::PushStyleColor(ImGuiCol_Text, textColor);

    if (ImGui::Button(cameraId.c_str(), ImVec2(-1, 35))) {
      OnCameraSelectionChanged(cameraId);
    }

    ImGui::PopStyleColor(); // Text color
    if (isSelected) {
      ImGui::PopStyleColor(); // Button color
    }

    // Status indicators
    ImGui::SameLine();
    if (status.connected) {
      ImGui::TextColored(ImVec4(0, 1, 0, 1), status.grabbing ? "[LIVE]" : "[READY]");
    }
    else {
      ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "[DISCONNECTED]");
    }
  }

  ImGui::EndChild();
}

void UICameraPanel::RenderNoSelectionMessage() {
  ImGui::SetWindowFontScale(1.2f);
  ImGui::Text("Camera Management");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::Text("Select a camera from the list on the left to view");
  ImGui::Text("its controls and status information.");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::Text("Camera System Features:");
  ImGui::BulletText("Multi-camera support (Pylon and IDS)");
  ImGui::BulletText("Unified interface for all camera types");
  ImGui::BulletText("Individual camera connection control");
  ImGui::BulletText("Real-time image acquisition and viewing");
  ImGui::BulletText("Exposure and gain control per camera");
  ImGui::BulletText("Quick exposure presets for different operations");
  ImGui::BulletText("Image capture and save functionality");
  ImGui::BulletText("Separate live video and single frame views");

  ImGui::Spacing();
  ImGui::Text("Interface Layout:");
  ImGui::BulletText("Left Panel - Camera list (top) and global controls (bottom)");
  ImGui::BulletText("Middle Panel - Live video and single frame tabs");
  ImGui::BulletText("Right Panel - Camera settings and utilities");

  ImGui::Spacing();
  ImGui::Text("Supported Camera Types:");
  ImGui::BulletText("Basler Pylon cameras (Green)");
  ImGui::BulletText("IDS uEye cameras (Blue)");

  ImGui::Spacing();
  ImGui::Text("Camera Count: %zu", m_cameraManager.GetCameraCount());
}

void UICameraPanel::OnCameraSelectionChanged(const std::string& newCameraId) {
  if (m_selectedCameraId == newCameraId) {
    return; // No change
  }

  std::cout << "[DEBUG] Camera selection changing from '" << m_selectedCameraId
    << "' to '" << newCameraId << "'" << std::endl;

  // Clear previous selection
  if (!m_selectedCameraId.empty()) {
    ClearAllPanels();
  }

  // Set new selection
  m_selectedCameraId = newCameraId;

  // Update all panels with new camera
  if (!m_selectedCameraId.empty()) {
    ICameraHardware* camera = GetSelectedCamera();
    std::cout << "[DEBUG] GetSelectedCamera() returned: " << (camera ? "valid camera" : "nullptr") << std::endl;

    if (camera) {
      std::cout << "[DEBUG] Camera type from interface: " << (camera->GetCameraType() == ICameraHardware::CameraType::PYLON ? "Pylon" : "IDS") << std::endl;
      std::cout << "[DEBUG] Camera model: " << camera->GetModelName() << std::endl;

      // Also check camera manager status
      auto status = m_cameraManager.GetCameraStatus(m_selectedCameraId);
      std::cout << "[DEBUG] Camera status type: " << (int)status.type << " (0=Pylon, 1=IDS, 2=Unknown)" << std::endl;
      std::cout << "[DEBUG] Status connected: " << status.connected << ", device info: " << status.deviceInfo << std::endl;

      m_liveVideoPanel->SetSelectedCamera(camera, m_selectedCameraId);
      m_singleGrabPanel->SetSelectedCamera(camera, m_selectedCameraId);
      m_utilityPanel->SetSelectedCamera(camera, m_selectedCameraId);

      std::cout << "[INFO] Camera selection changed to: " << m_selectedCameraId
        << " (Type: " << (camera->GetCameraType() == ICameraHardware::CameraType::PYLON ? "Pylon" : "IDS")
        << ")" << std::endl;
    }
    else {
      std::cout << "[ERROR] GetSelectedCamera() returned nullptr for ID: " << m_selectedCameraId << std::endl;

      // Try to get camera status for debugging
      auto status = m_cameraManager.GetCameraStatus(m_selectedCameraId);
      std::cout << "[DEBUG] Camera status - connected: " << status.connected
        << ", type: " << (int)status.type << std::endl;
    }
  }
}

void UICameraPanel::ClearAllPanels() {
  if (m_liveVideoPanel) {
    m_liveVideoPanel->ClearCamera();
  }
  if (m_singleGrabPanel) {
    m_singleGrabPanel->ClearCamera();
  }
  if (m_utilityPanel) {
    m_utilityPanel->ClearCamera();
  }
}

ICameraHardware* UICameraPanel::GetSelectedCamera() const {
  if (m_selectedCameraId.empty()) {
    return nullptr;
  }
  return m_cameraManager.GetCameraHardware(m_selectedCameraId);
}

void UICameraPanel::ToggleWindow() {
  m_showWindow = !m_showWindow;

  // Clear all panels when hiding window
  if (!m_showWindow) {
    ClearAllPanels();
  }
}