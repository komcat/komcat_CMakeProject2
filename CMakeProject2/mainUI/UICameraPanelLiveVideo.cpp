// UICameraPanelLiveVideo.cpp - Live Video Feed Panel Implementation
#include "UICameraPanelLiveVideo.h"
#include "include/camera/CameraManager.h"
#include "include/camera/pylon_camera_test.h"
#include "include/camera/pylon_camera.h"
#include "CameraFeedDisplay.h"
#include "imgui.h"
#include <iostream>
#include <chrono>

UICameraPanelLiveVideo::UICameraPanelLiveVideo(CameraManager& cameraManager)
  : m_cameraManager(cameraManager) {

  // Create camera feed display for live video
  m_feedDisplay = std::make_unique<CameraFeedDisplay>();
  m_feedDisplay->SetPlaceholderText("Live Video Feed");

  std::cout << "[INFO] UICameraPanelLiveVideo created" << std::endl;
}

UICameraPanelLiveVideo::~UICameraPanelLiveVideo() {
  ClearCamera();
}

void UICameraPanelLiveVideo::RenderTab(PylonCameraTest* camera, const std::string& cameraId) {
  // Update camera reference if changed
  if (m_currentCamera != camera || m_currentCameraId != cameraId) {
    SetSelectedCamera(camera, cameraId);
  }

  if (!ValidateCamera()) {
    ImGui::Text("Camera not available");
    return;
  }

  // Update internal state
  UpdateGrabbingState();

  // Render controls
  RenderControls();

  ImGui::Separator();

  // Render status
  RenderStatus();

  ImGui::Separator();

  // Render live feed display
  RenderFeedDisplay();
}

void UICameraPanelLiveVideo::SetSelectedCamera(PylonCameraTest* camera, const std::string& cameraId) {
  // Clear previous camera
  if (m_currentCamera != camera) {
    ClearCamera();
  }

  m_currentCamera = camera;
  m_currentCameraId = cameraId;

  if (camera) {
    std::cout << "[INFO] LiveVideo panel set to camera: " << cameraId << std::endl;
    UpdateGrabbingState();
  }
}

void UICameraPanelLiveVideo::ClearCamera() {
  if (m_feedDisplay) {
    m_feedDisplay->ClearSource();
  }

  m_currentCamera = nullptr;
  m_currentCameraId = "";
  m_isGrabbing = false;

  std::cout << "[INFO] LiveVideo panel camera cleared" << std::endl;
}

void UICameraPanelLiveVideo::RenderControls() {
  if (!ValidateCamera()) {
    ImGui::Text("Camera not connected");
    return;
  }

  auto& pylonCamera = m_currentCamera->GetCamera();

  // Main live video toggle button
  ImVec4 buttonColor;
  std::string buttonText;

  if (m_isGrabbing) {
    buttonColor = ImVec4(0.8f, 0.3f, 0.3f, 1.0f); // Red for stop
    buttonText = "Stop Live Video";
  }
  else {
    buttonColor = ImVec4(0.3f, 0.8f, 0.3f, 1.0f); // Green for start
    buttonText = "Start Live Video";
  }

  ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(buttonColor.x * 1.2f, buttonColor.y * 1.2f, buttonColor.z * 1.2f, 1.0f));

  if (ImGui::Button(buttonText.c_str(), ImVec2(150, 30))) {
    ToggleLiveVideo();
  }

  ImGui::PopStyleColor(2);

  ImGui::SameLine();
  ImGui::Text("Status: %s", m_isGrabbing ? "Live" : "Off");

  // Quick capture button
  ImGui::Spacing();
  if (ImGui::Button("Quick Capture", ImVec2(-1, 30))) {
    m_cameraManager.CaptureImage(m_currentCameraId);
  }
}

void UICameraPanelLiveVideo::RenderStatus() {
  if (!ValidateCamera()) {
    return;
  }

  auto& pylonCamera = m_currentCamera->GetCamera();

  ImGui::Text("Live Feed Status:");
  ImGui::Text("• Connected: %s", pylonCamera.IsConnected() ? "Yes" : "No");
  ImGui::Text("• Grabbing: %s", m_isGrabbing ? "Yes" : "No");
  ImGui::Text("• Device OK: %s", pylonCamera.IsCameraDeviceRemoved() ? "No" : "Yes");

  if (pylonCamera.IsConnected()) {
    auto settings = pylonCamera.GetCurrentExposureSettings();
    ImGui::Text("• Exposure: %.0f μs", settings.exposure_time);
    ImGui::Text("• Gain: %.1f", settings.gain);
    ImGui::Text("• Auto Exposure: %s", settings.exposure_auto ? "On" : "Off");
  }

  // Feed display status
  if (m_feedDisplay) {
    ImGui::Text("• Feed Active: %s", m_feedDisplay->HasValidTexture() ? "Yes" : "No");
    if (m_feedDisplay->HasValidTexture()) {
      ImGui::Text("• Resolution: %dx%d", m_feedDisplay->GetTextureWidth(), m_feedDisplay->GetTextureHeight());
    }
  }
}

void UICameraPanelLiveVideo::RenderFeedDisplay() {
  // Calculate canvas size (leave space for any controls below)
  ImVec2 canvasSize = ImGui::GetContentRegionAvail();
  canvasSize.y = (std::max)(canvasSize.y - 10.0f, 200.0f); // Reserve some space, minimum height

  if (!ValidateCamera()) {
    RenderErrorCanvas(canvasSize.x, canvasSize.y, "Camera Not Available\nSelect a connected camera");
    return;
  }

  auto& pylonCamera = m_currentCamera->GetCamera();

  // Handle live feed display
  if (m_isGrabbing && pylonCamera.IsGrabbing()) {
    try {
      // Set camera source for live feed
      m_feedDisplay->SetPylonCameraSource(m_currentCamera);

      // Render the live camera feed
      m_feedDisplay->RenderToCanvas(canvasSize.x, canvasSize.y);
    }
    catch (const std::exception& e) {
      std::cout << "[ERROR] Exception in live feed display: " << e.what() << std::endl;

      // Clear source and show error
      m_feedDisplay->ClearSource();
      RenderErrorCanvas(canvasSize.x, canvasSize.y, "Live Feed Error\nRestart grabbing to fix");
    }
  }
  else {
    // Clear source when not grabbing and show placeholder
    if (m_feedDisplay) {
      m_feedDisplay->ClearSource();
      m_feedDisplay->RenderToCanvas(canvasSize.x, canvasSize.y);
    }
  }
}

void UICameraPanelLiveVideo::RenderErrorCanvas(float width, float height, const std::string& errorText) {
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();
  ImDrawList* drawList = ImGui::GetWindowDrawList();

  // Error canvas - red tinted background
  drawList->AddRectFilled(canvasPos,
    ImVec2(canvasPos.x + width, canvasPos.y + height),
    IM_COL32(80, 20, 20, 255));

  // Error border
  drawList->AddRect(canvasPos,
    ImVec2(canvasPos.x + width, canvasPos.y + height),
    IM_COL32(255, 100, 100, 255));

  // Error text
  ImVec2 textSize = ImGui::CalcTextSize(errorText.c_str());
  ImVec2 textPos = ImVec2(canvasPos.x + (width - textSize.x) * 0.5f,
    canvasPos.y + (height - textSize.y) * 0.5f);
  drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), errorText.c_str());

  // Advance cursor
  ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, canvasPos.y + height));
}

void UICameraPanelLiveVideo::StartLiveVideo() {
  if (!ValidateCamera()) {
    return;
  }

  if (!m_isGrabbing) {
    std::cout << "[INFO] Starting live video for: " << m_currentCameraId << std::endl;
    m_cameraManager.StartGrabbing(m_currentCameraId);
    UpdateGrabbingState();
  }
}

void UICameraPanelLiveVideo::StopLiveVideo() {
  if (!ValidateCamera()) {
    return;
  }

  if (m_isGrabbing) {
    std::cout << "[INFO] Stopping live video for: " << m_currentCameraId << std::endl;

    // Clear feed first
    if (m_feedDisplay) {
      m_feedDisplay->ClearSource();
    }

    m_cameraManager.StopGrabbing(m_currentCameraId);
    UpdateGrabbingState();
  }
}

void UICameraPanelLiveVideo::ToggleLiveVideo() {
  if (m_isGrabbing) {
    StopLiveVideo();
  }
  else {
    StartLiveVideo();
  }
}

void UICameraPanelLiveVideo::UpdateGrabbingState() {
  if (!ValidateCamera()) {
    m_isGrabbing = false;
    return;
  }

  auto& pylonCamera = m_currentCamera->GetCamera();
  bool wasGrabbing = m_isGrabbing;
  m_isGrabbing = pylonCamera.IsGrabbing();

  // If grabbing state changed, log it
  if (wasGrabbing != m_isGrabbing) {
    std::cout << "[INFO] Live video grabbing state changed to: "
      << (m_isGrabbing ? "ON" : "OFF") << " for " << m_currentCameraId << std::endl;
  }
}

bool UICameraPanelLiveVideo::ValidateCamera() const {
  if (!m_currentCamera) {
    return false;
  }

  auto& pylonCamera = m_currentCamera->GetCamera();
  return pylonCamera.IsConnected();
}

bool UICameraPanelLiveVideo::IsLiveActive() const {
  return m_isGrabbing && ValidateCamera();
}

std::string UICameraPanelLiveVideo::GetStatusText() const {
  if (!ValidateCamera()) {
    return "No Camera";
  }

  if (m_isGrabbing) {
    return "Live Active";
  }
  else {
    return "Live Stopped";
  }
}