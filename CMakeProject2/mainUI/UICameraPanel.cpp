// UICameraPanel.cpp - Updated with reorganized left panel layout
#include "UICameraPanel.h"
#include "UICameraPanelLiveVideo.h"
#include "UICameraPanelSingleGrab.h"
#include "UICameraPanelUtility.h"
#include "include/camera/CameraManager.h"
#include "include/camera/pylon_camera_test.h"
#include "include/camera/pylon_camera.h"
#include "imgui.h"
#include <iostream>

UICameraPanel::UICameraPanel(CameraManager& cameraManager)
  : m_cameraManager(cameraManager) {

  // Create sub-panels
  m_liveVideoPanel = std::make_unique<UICameraPanelLiveVideo>(cameraManager);
  m_singleGrabPanel = std::make_unique<UICameraPanelSingleGrab>(cameraManager);
  m_utilityPanel = std::make_unique<UICameraPanelUtility>(cameraManager);

  std::cout << "[INFO] UICameraPanel created with sub-panels" << std::endl;
}

UICameraPanel::~UICameraPanel() {
  // Clear all panels before destruction
  ClearAllPanels();
}

void UICameraPanel::RenderUI() {
  if (!m_showWindow) {
    return;
  }


  // Calculate content size for 3-column layout: 25% / 50% / 25%
  ImVec2 contentSize = ImGui::GetContentRegionAvail();
  float leftPanelWidth = contentSize.x * 0.25f;
  float middlePanelWidth = contentSize.x * 0.50f;
  float rightPanelWidth = contentSize.x * 0.25f;

  // Left Panel - Camera List and Global Controls (25% width)
  ImGui::BeginChild("LeftCameraPanel", ImVec2(leftPanelWidth, contentSize.y), true);
  RenderLeftPanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // Middle Panel - Tabbed Camera Feed (50% width)
  ImGui::BeginChild("MiddleCameraPanel", ImVec2(middlePanelWidth, contentSize.y), true);
  RenderMiddlePanelTabs();
  ImGui::EndChild();

  ImGui::SameLine();

  // Right Panel - Camera Utility Controls (25% width)
  ImGui::BeginChild("RightCameraPanel", ImVec2(rightPanelWidth, contentSize.y), true);
  RenderRightPanel();
  ImGui::EndChild();
}

void UICameraPanel::RenderLeftPanel() {
  ImGui::Text("Camera System");
  ImGui::Separator();

  // **REORGANIZED: Camera list first, at the top**
  RenderCameraList();

  // **REORGANIZED: Separator before global controls**
  ImGui::Separator();

  // **REORGANIZED: Global controls moved to bottom**
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

  ImGui::Spacing();
  ImGui::Text("Total Cameras: %zu", m_cameraManager.GetCameraCount());
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

  // Get selected camera
  PylonCameraTest* camera = GetSelectedCamera();
  if (!camera) {
    ImGui::Text("Selected camera not available");
    ClearAllPanels();
    return;
  }

  // Camera info header
  ImGui::Text("Camera: %s", m_selectedCameraId.c_str());
  ImGui::Separator();

  // **Tab Bar for Live Feed vs Single Frame**
  if (ImGui::BeginTabBar("CameraFeedTabs", ImGuiTabBarFlags_None)) {

    // **TAB 1: Live Video Feed**
    if (ImGui::BeginTabItem("Live Video")) {
      m_liveVideoPanel->RenderTab(camera, m_selectedCameraId);
      ImGui::EndTabItem();
    }

    // **TAB 2: Single Frame Capture**
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
    PylonCameraTest* camera = GetSelectedCamera();
    if (camera) {
      m_utilityPanel->RenderPanel(camera, m_selectedCameraId);
    }
    else {
      ImGui::Text("Selected camera not found");
    }
  }
}

void UICameraPanel::RenderCameraList() {
  ImGui::Text("Camera Selection");
  ImGui::Spacing();

  // Get all camera IDs from the camera manager
  std::vector<std::string> cameraIds = m_cameraManager.GetCameraIds();

  if (cameraIds.empty()) {
    // **ENLARGED PLACEHOLDER for better visibility**
    ImGui::BeginChild("CameraListPlaceholder", ImVec2(-1, 150), true, ImGuiWindowFlags_NoScrollbar);

    ImGui::Spacing();
    ImGui::Text("No cameras available");
    ImGui::Spacing();
    ImGui::TextWrapped("Use 'Add Camera' to add cameras to the system");
    ImGui::Spacing();

    // **Add helpful instructions**
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Steps to add cameras:");
    ImGui::BulletText("Connect camera hardware");
    ImGui::BulletText("Use Initialize All button");
    ImGui::BulletText("Cameras will appear here");

    ImGui::EndChild();
    return;
  }

  // **ENLARGED CAMERA LIST - Calculate height for better visibility**
  ImVec2 availableSize = ImGui::GetContentRegionAvail();
  float listHeight = availableSize.y - 200.0f; // Reserve space for global controls
  listHeight = (std::max)(listHeight, 200.0f);   // Minimum height
  listHeight = (std::min)(listHeight, 400.0f);   // Maximum height

  // **SCROLLABLE CAMERA LIST with increased height**
  ImGui::BeginChild("CameraListChild", ImVec2(-1, listHeight), true);

  for (const std::string& cameraId : cameraIds) {
    ImGui::PushID(cameraId.c_str());

    // Get camera status for display
    auto status = m_cameraManager.GetCameraStatus(cameraId);

    // **ENLARGED SELECTABLE ITEMS for easier selection**
    bool isSelected = (m_selectedCameraId == cameraId);

    // **LARGER SELECTABLE HEIGHT for easier clicking**
    if (ImGui::Selectable(cameraId.c_str(), isSelected, ImGuiSelectableFlags_None, ImVec2(0, 25))) {
      OnCameraSelectionChanged(cameraId);
    }

    // **STATUS INDICATOR on same line but more prominent**
    ImGui::SameLine();

    // Connection status indicator with better colors
    ImVec4 statusColor;
    std::string statusText;

    if (status.connected) {
      if (status.grabbing) {
        statusColor = ImVec4(0, 1, 0, 1);     // Bright green - grabbing
        statusText = "[LIVE]";
      }
      else {
        statusColor = ImVec4(0, 0.8f, 0, 1);  // Light green - connected
        statusText = "[CONN]";
      }
    }
    else {
      statusColor = ImVec4(1, 0.3f, 0.3f, 1); // Bright red - disconnected
      statusText = "[DISC]";
    }

    ImGui::TextColored(statusColor, "%s", statusText.c_str());

    // **TOOLTIP with more information on hover**
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::Text("Camera: %s", cameraId.c_str());
      ImGui::Text("Connected: %s", status.connected ? "Yes" : "No");
      ImGui::Text("Grabbing: %s", status.grabbing ? "Yes" : "No");
      if (status.connected) {
        ImGui::Text("Exposure: %.0f μs", status.currentExposure.exposure_time);
        ImGui::Text("Gain: %.1f", status.currentExposure.gain);
      }
      ImGui::Separator();
      ImGui::Text("Click to select camera");
      ImGui::Text("Right-click for context menu");
      ImGui::EndTooltip();
    }

    // **RIGHT-CLICK CONTEXT MENU**
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup(("context_" + cameraId).c_str());
    }

    if (ImGui::BeginPopup(("context_" + cameraId).c_str())) {
      ImGui::Text("Camera: %s", cameraId.c_str());
      ImGui::Separator();

      if (ImGui::MenuItem("Connect")) {
        m_cameraManager.ConnectCamera(cameraId);
      }
      if (ImGui::MenuItem("Disconnect")) {
        m_cameraManager.DisconnectCamera(cameraId);
        // Clear panels if disconnecting selected camera
        if (m_selectedCameraId == cameraId) {
          ClearAllPanels();
        }
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Start Grabbing")) {
        m_cameraManager.StartGrabbing(cameraId);
      }
      if (ImGui::MenuItem("Stop Grabbing")) {
        m_cameraManager.StopGrabbing(cameraId);
      }
      if (ImGui::MenuItem("Capture Image")) {
        m_cameraManager.CaptureImage(cameraId);
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Remove Camera")) {
        // Clear panels if removing selected camera
        if (m_selectedCameraId == cameraId) {
          ClearAllPanels();
          m_selectedCameraId = "";
        }
        m_cameraManager.RemoveCamera(cameraId);
      }
      ImGui::EndPopup();
    }

    // **ADD SPACING between camera entries for better readability**
    ImGui::Spacing();

    ImGui::PopID();
  }

  ImGui::EndChild();

  // **SHOW SELECTED CAMERA INFO below the list**
  if (!m_selectedCameraId.empty()) {
    ImGui::Separator();
    ImGui::Text("Selected: %s", m_selectedCameraId.c_str());

    auto status = m_cameraManager.GetCameraStatus(m_selectedCameraId);
    if (status.connected) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0, 1, 0, 1), status.grabbing ? "[LIVE]" : "[READY]");
    }
    else {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "[DISCONNECTED]");
    }
  }
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
  ImGui::BulletText("Multi-camera support and management");
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
  ImGui::Text("Tabbed Interface:");
  ImGui::BulletText("Live Video - Real-time camera feed");
  ImGui::BulletText("Single Frame - Captured still images");
  ImGui::BulletText("Independent controls for each mode");

  ImGui::Spacing();
  ImGui::Text("Camera Count: %zu", m_cameraManager.GetCameraCount());
}

void UICameraPanel::OnCameraSelectionChanged(const std::string& newCameraId) {
  if (m_selectedCameraId == newCameraId) {
    return; // No change
  }

  // Clear previous selection
  if (!m_selectedCameraId.empty()) {
    ClearAllPanels();
  }

  // Set new selection
  m_selectedCameraId = newCameraId;

  // Update all panels with new camera
  if (!m_selectedCameraId.empty()) {
    PylonCameraTest* camera = GetSelectedCamera();
    if (camera) {
      m_liveVideoPanel->SetSelectedCamera(camera, m_selectedCameraId);
      m_singleGrabPanel->SetSelectedCamera(camera, m_selectedCameraId);
      m_utilityPanel->SetSelectedCamera(camera, m_selectedCameraId);

      std::cout << "[INFO] Camera selection changed to: " << m_selectedCameraId << std::endl;
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

PylonCameraTest* UICameraPanel::GetSelectedCamera() const {
  if (m_selectedCameraId.empty()) {
    return nullptr;
  }
  return m_cameraManager.GetCamera(m_selectedCameraId);
}

void UICameraPanel::ToggleWindow() {
  m_showWindow = !m_showWindow;

  // Clear all panels when hiding window
  if (!m_showWindow) {
    ClearAllPanels();
  }
}