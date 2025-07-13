// UICameraPanel.cpp - FIXED VERSION with safer camera feed handling
#include "UICameraPanel.h"
#include "include/camera/CameraManager.h"
#include "include/camera/pylon_camera_test.h"
#include "include/camera/pylon_camera.h"
#include "CameraFeedDisplay.h"
#include "imgui.h"
#include <iostream>
#include <iomanip>
#include <sstream>

UICameraPanel::UICameraPanel(CameraManager& cameraManager)
  : m_cameraManager(cameraManager) {
  // Create camera feed display
  m_feedDisplay = std::make_unique<CameraFeedDisplay>();
}

UICameraPanel::~UICameraPanel() {
  // Properly clear the feed display before destruction
  if (m_feedDisplay) {
    m_feedDisplay->ClearSource();
  }
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

  // Left Panel - Camera List (25% width)
  ImGui::BeginChild("LeftCameraPanel", ImVec2(leftPanelWidth, contentSize.y), true);
  RenderLeftPanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // Middle Panel - Live Camera Feed (50% width)
  ImGui::BeginChild("MiddleCameraPanel", ImVec2(middlePanelWidth, contentSize.y), true);
  RenderMiddlePanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // Right Panel - Camera Controls (25% width)
  ImGui::BeginChild("RightCameraPanel", ImVec2(rightPanelWidth, contentSize.y), true);
  RenderRightPanel();
  ImGui::EndChild();
}

void UICameraPanel::RenderLeftPanel() {
  ImGui::Text("Camera Controls");
  ImGui::Separator();

  // Global camera controls
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

  ImGui::Separator();

  // Render camera list
  RenderCameraList();
}

void UICameraPanel::RenderMiddlePanel() {
  ImGui::Text("Live Camera Feed");
  ImGui::Separator();

  if (m_selectedCameraId.empty()) {
    ImGui::Spacing();
    ImGui::Text("No camera selected");
    ImGui::Spacing();
    ImGui::Text("Select a camera from the left panel");
    ImGui::Text("to view live feed here");
    return;
  }

  // **CRITICAL FIX: Validate camera exists before proceeding**
  PylonCameraTest* camera = m_cameraManager.GetCamera(m_selectedCameraId);
  if (!camera) {
    ImGui::Text("Selected camera not available");

    // **SAFETY: Clear feed display if camera is no longer available**
    if (m_feedDisplay) {
      m_feedDisplay->ClearSource();
    }
    return;
  }

  auto& pylonCamera = camera->GetCamera();

  if (!pylonCamera.IsConnected()) {
    ImGui::Spacing();
    ImGui::Text("Camera not connected");
    ImGui::Spacing();
    ImGui::Text("Connect the camera to view");
    ImGui::Text("live video feed");

    // **SAFETY: Clear feed when not connected**
    if (m_feedDisplay) {
      m_feedDisplay->ClearSource();
    }
    return;
  }

  // Live feed controls
  ImGui::Text("Camera: %s", m_selectedCameraId.c_str());

  // Toggle live video button - now controls grabbing directly
  if (ImGui::Button("Toggle Live Video", ImVec2(150, 30))) {
    if (pylonCamera.IsGrabbing()) {
      m_cameraManager.StopGrabbing(m_selectedCameraId);
      // **SAFETY: Clear feed when stopping**
      if (m_feedDisplay) {
        m_feedDisplay->ClearSource();
      }
    }
    else {
      m_cameraManager.StartGrabbing(m_selectedCameraId);
    }
  }

  ImGui::SameLine();
  ImGui::Text("Status: %s", pylonCamera.IsGrabbing() ? "Live" : "Off");

  // Camera feed status
  ImGui::Separator();
  ImGui::Text("Feed Status:");
  ImGui::Text("• Connected: %s", pylonCamera.IsConnected() ? "Yes" : "No");
  ImGui::Text("• Grabbing: %s", pylonCamera.IsGrabbing() ? "Yes" : "No");

  if (pylonCamera.IsConnected()) {
    auto settings = pylonCamera.GetCurrentExposureSettings();
    ImGui::Text("• Exposure: %.0f μs", settings.exposure_time);
    ImGui::Text("• Gain: %.1f", settings.gain);
  }

  ImGui::Separator();

  // **SAFE CAMERA FEED HANDLING**

  // Calculate canvas size (leave space for controls below)
  ImVec2 canvasSize = ImGui::GetContentRegionAvail();
  canvasSize.y = canvasSize.y - 50; // Leave space for controls below

  // **CRITICAL: Only set camera source when actually grabbing and valid**
  if (pylonCamera.IsGrabbing() && camera && m_feedDisplay) {
    try {
      // Set camera source only when safe to do so
      m_feedDisplay->SetPylonCameraSource(camera);

      // Render the camera feed
      m_feedDisplay->RenderToCanvas(canvasSize.x, canvasSize.y);
    }
    catch (const std::exception& e) {
      // **SAFETY: Handle any exceptions during feed display**
      std::cout << "[ERROR] Exception in camera feed display: " << e.what() << std::endl;

      // Clear source and show error message
      m_feedDisplay->ClearSource();

      ImVec2 canvasPos = ImGui::GetCursorScreenPos();
      ImDrawList* drawList = ImGui::GetWindowDrawList();

      // Error canvas
      drawList->AddRectFilled(canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(80, 20, 20, 255));

      // Error text
      std::string errorText = "Camera Feed Error\nPlease restart grabbing";
      ImVec2 textSize = ImGui::CalcTextSize(errorText.c_str());
      ImVec2 textPos = ImVec2(canvasPos.x + (canvasSize.x - textSize.x) * 0.5f,
        canvasPos.y + (canvasSize.y - textSize.y) * 0.5f);
      drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), errorText.c_str());

      // Advance cursor
      ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, canvasPos.y + canvasSize.y));
    }
  }
  else {
    // **SAFETY: Clear source when not grabbing**
    if (m_feedDisplay) {
      m_feedDisplay->ClearSource();

      // Render placeholder
      m_feedDisplay->RenderToCanvas(canvasSize.x, canvasSize.y);
    }
  }

  // Quick capture button below canvas
  ImGui::Spacing();
  if (ImGui::Button("Capture Image", ImVec2(-1, 30))) {
    m_cameraManager.CaptureImage(m_selectedCameraId);
  }
}

void UICameraPanel::RenderRightPanel() {
  if (m_selectedCameraId.empty()) {
    RenderNoSelectionMessage();
  }
  else {
    RenderSelectedCameraUI();
  }
}

void UICameraPanel::RenderCameraList() {
  // Get all camera IDs from the camera manager
  std::vector<std::string> cameraIds = m_cameraManager.GetCameraIds();

  if (cameraIds.empty()) {
    ImGui::Text("No cameras available");
    ImGui::Spacing();
    ImGui::Text("Use 'Add Camera' to add");
    ImGui::Text("cameras to the system");
    return;
  }

  for (const std::string& cameraId : cameraIds) {
    ImGui::PushID(cameraId.c_str());

    // Get camera status for display
    auto status = m_cameraManager.GetCameraStatus(cameraId);

    // Connection status indicator
    ImVec4 statusColor;
    std::string statusText;

    if (status.connected) {
      if (status.grabbing) {
        statusColor = ImVec4(0, 1, 0, 1);     // Green - grabbing
        statusText = "[GRAB]";
      }
      else {
        statusColor = ImVec4(0, 0.8f, 0, 1);  // Light green - connected
        statusText = "[CONN]";
      }
    }
    else {
      statusColor = ImVec4(0.8f, 0, 0, 1);    // Red - disconnected
      statusText = "[DISC]";
    }

    // Create selectable item for camera
    bool isSelected = (m_selectedCameraId == cameraId);
    if (ImGui::Selectable(cameraId.c_str(), isSelected)) {
      // **SAFETY: Clear feed when switching cameras**
      if (m_selectedCameraId != cameraId && m_feedDisplay) {
        m_feedDisplay->ClearSource();
      }
      m_selectedCameraId = cameraId;
    }

    // Show status on same line
    ImGui::SameLine();
    ImGui::TextColored(statusColor, "%s", statusText.c_str());

    // Right-click context menu (simplified to avoid popup issues)
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup(("context_" + cameraId).c_str());
    }

    if (ImGui::BeginPopup(("context_" + cameraId).c_str())) {
      if (ImGui::MenuItem("Connect")) {
        m_cameraManager.ConnectCamera(cameraId);
      }
      if (ImGui::MenuItem("Disconnect")) {
        m_cameraManager.DisconnectCamera(cameraId);
        // **SAFETY: Clear feed if disconnecting selected camera**
        if (m_selectedCameraId == cameraId && m_feedDisplay) {
          m_feedDisplay->ClearSource();
        }
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Remove Camera")) {
        // **SAFETY: Clear feed if removing selected camera**
        if (m_selectedCameraId == cameraId && m_feedDisplay) {
          m_feedDisplay->ClearSource();
        }

        m_cameraManager.RemoveCamera(cameraId);

        if (m_selectedCameraId == cameraId) {
          m_selectedCameraId = "";
        }
      }
      ImGui::EndPopup();
    }

    ImGui::PopID();
  }
}

void UICameraPanel::RenderSelectedCameraUI() {
  PylonCameraTest* camera = m_cameraManager.GetCamera(m_selectedCameraId);
  if (!camera) {
    ImGui::Text("Selected camera not found");
    return;
  }

  // Render all camera control sections
  RenderCameraHeader(camera);
  ImGui::Separator();

  RenderConnectionControls(camera);
  ImGui::Separator();

  RenderCameraStatus(camera);
  ImGui::Separator();

  RenderGrabbingControls(camera);
  ImGui::Separator();

  RenderExposureControls(camera);
  ImGui::Separator();

  RenderImageControls(camera);
  ImGui::Separator();

  RenderUtilityControls(camera);
}

void UICameraPanel::RenderCameraHeader(PylonCameraTest* camera) {
  ImGui::SetWindowFontScale(1.2f);
  ImGui::Text("Camera: %s", m_selectedCameraId.c_str());
  ImGui::SetWindowFontScale(1.0f);

  auto& pylonCamera = camera->GetCamera();
  if (pylonCamera.IsConnected()) {
    std::string deviceInfo = pylonCamera.GetDeviceInfo();
    ImGui::Text("Device: %s", deviceInfo.c_str());
  }
  else {
    ImGui::Text("Device: Not connected");
  }
}

void UICameraPanel::RenderConnectionControls(PylonCameraTest* camera) {
  ImGui::Text("Connection Controls");

  auto& pylonCamera = camera->GetCamera();

  if (!pylonCamera.IsConnected()) {
    if (ImGui::Button("Connect Camera", ImVec2(150, 30))) {
      m_cameraManager.ConnectCamera(m_selectedCameraId);
    }
  }
  else {
    if (ImGui::Button("Disconnect Camera", ImVec2(150, 30))) {
      // **SAFETY: Clear feed before disconnecting**
      if (m_feedDisplay) {
        m_feedDisplay->ClearSource();
      }
      m_cameraManager.DisconnectCamera(m_selectedCameraId);
    }

    ImGui::SameLine();
    if (ImGui::Button("Reconnect", ImVec2(150, 30))) {
      // **SAFETY: Clear feed during reconnect**
      if (m_feedDisplay) {
        m_feedDisplay->ClearSource();
      }
      pylonCamera.TryReconnect();
    }
  }
}

void UICameraPanel::RenderCameraStatus(PylonCameraTest* camera) {
  ImGui::Text("Camera Status");

  auto& pylonCamera = camera->GetCamera();

  // Status indicators
  ImGui::Text("Connected: %s", pylonCamera.IsConnected() ? "Yes" : "No");
  ImGui::Text("Grabbing: %s", pylonCamera.IsGrabbing() ? "Yes" : "No");
  ImGui::Text("Device Removed: %s", pylonCamera.IsCameraDeviceRemoved() ? "Yes" : "No");

  if (pylonCamera.IsConnected()) {
    // Show current exposure settings
    auto settings = pylonCamera.GetCurrentExposureSettings();
    ImGui::Text("Current Exposure: %.0f μs", settings.exposure_time);
    ImGui::Text("Current Gain: %.1f", settings.gain);
    ImGui::Text("Auto Exposure: %s", settings.exposure_auto ? "On" : "Off");
    ImGui::Text("Auto Gain: %s", settings.gain_auto ? "On" : "Off");
  }
}

void UICameraPanel::RenderGrabbingControls(PylonCameraTest* camera) {
  ImGui::Text("Image Acquisition");

  auto& pylonCamera = camera->GetCamera();

  if (!pylonCamera.IsConnected()) {
    ImGui::Text("Camera not connected");
    return;
  }

  if (!pylonCamera.IsGrabbing()) {
    if (ImGui::Button("Start Grabbing", ImVec2(150, 30))) {
      m_cameraManager.StartGrabbing(m_selectedCameraId);
    }

    ImGui::SameLine();
    if (ImGui::Button("Grab Single Frame", ImVec2(150, 30))) {
      camera->GrabSingleFrame();
    }
  }
  else {
    if (ImGui::Button("Stop Grabbing", ImVec2(150, 30))) {
      // **SAFETY: Clear feed before stopping grabbing**
      if (m_feedDisplay) {
        m_feedDisplay->ClearSource();
      }
      m_cameraManager.StopGrabbing(m_selectedCameraId);
    }
  }
}

void UICameraPanel::RenderExposureControls(PylonCameraTest* camera) {
  ImGui::Text("Exposure Controls");

  auto& pylonCamera = camera->GetCamera();

  if (!pylonCamera.IsConnected()) {
    ImGui::Text("Camera not connected");
    return;
  }

  ImGui::Spacing();
  ImGui::Text("Manual Exposure Settings");

  // Custom exposure controls
  ImGui::SliderFloat("Exposure Time (μs)", &m_customExposureTime, 100.0f, 10000.0f, "%.0f");
  ImGui::SliderFloat("Gain", &m_customGain, 0.0f, 10.0f, "%.1f");

  ImGui::Checkbox("Auto Exposure", &m_exposureAuto);
  ImGui::SameLine();
  ImGui::Checkbox("Auto Gain", &m_gainAuto);

  if (ImGui::Button("Apply Settings", ImVec2(150, 30))) {
    PylonCamera::ExposureSettings settings;
    settings.exposure_time = m_customExposureTime;
    settings.gain = m_customGain;
    settings.exposure_auto = m_exposureAuto;
    settings.gain_auto = m_gainAuto;

    m_cameraManager.ApplyExposureSettings(m_selectedCameraId, settings);
  }

  ImGui::SameLine();
  if (ImGui::Button("Read Current", ImVec2(150, 30))) {
    auto settings = pylonCamera.GetCurrentExposureSettings();
    m_customExposureTime = settings.exposure_time;
    m_customGain = settings.gain;
    m_exposureAuto = settings.exposure_auto;
    m_gainAuto = settings.gain_auto;
  }
}

void UICameraPanel::RenderImageControls(PylonCameraTest* camera) {
  ImGui::Text("Image Controls");

  auto& pylonCamera = camera->GetCamera();

  if (!pylonCamera.IsConnected()) {
    ImGui::Text("Camera not connected");
    return;
  }

  if (ImGui::Button("Capture Image", ImVec2(150, 30))) {
    m_cameraManager.CaptureImage(m_selectedCameraId);
  }

  ImGui::SameLine();
  if (ImGui::Button("Toggle View Window", ImVec2(150, 30))) {
    camera->ToggleWindow();
  }

  // Show if camera window is visible
  ImGui::Text("View Window: %s", camera->IsVisible() ? "Open" : "Closed");
}

void UICameraPanel::RenderUtilityControls(PylonCameraTest* camera) {
  ImGui::Text("Utility Controls");

  auto& pylonCamera = camera->GetCamera();

  if (ImGui::Button("Debug Camera Settings", ImVec2(200, 30))) {
    pylonCamera.DebugCameraSettings();
  }

  // Access to exposure manager
  if (ImGui::Button("Open Exposure Manager", ImVec2(200, 30))) {
    camera->GetExposureManager().ToggleWindow();
  }

  // Enable debug mode
  ImGui::Checkbox("Enable Debug Mode", &camera->enableDebug);
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

  ImGui::Spacing();
  ImGui::Text("Supported Operations:");
  ImGui::BulletText("Connect/Disconnect individual cameras");
  ImGui::BulletText("Start/Stop continuous image grabbing");
  ImGui::BulletText("Single frame capture");
  ImGui::BulletText("Apply exposure settings by node position");
  ImGui::BulletText("Manual exposure and gain adjustment");
  ImGui::BulletText("Synchronized multi-camera capture");

  ImGui::Spacing();
  ImGui::Text("Use the buttons in the left panel to control all cameras");
  ImGui::Text("at once, or select individual cameras for specific control.");

  ImGui::Spacing();
  ImGui::Text("Camera Count: %zu", m_cameraManager.GetCameraCount());
}

void UICameraPanel::ToggleWindow() {
  m_showWindow = !m_showWindow;

  // **SAFETY: Clear feed when hiding window**
  if (!m_showWindow && m_feedDisplay) {
    m_feedDisplay->ClearSource();
  }
}