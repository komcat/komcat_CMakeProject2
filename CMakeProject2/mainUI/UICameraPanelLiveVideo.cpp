// UICameraPanelLiveVideo.cpp - Live Video Feed Panel Implementation
#include "UICameraPanelLiveVideo.h"
#include "include/camera/CameraManager.h"
#include "include/camera/ICameraHardware.h"
#include "include/camera/CameraFrameData.h"
#include "mainUI/LiveVideoSubscriber.h"
#include "imgui.h"
#include "GL/gl.h"
#include <iostream>
#include <chrono>

UICameraPanelLiveVideo::UICameraPanelLiveVideo(CameraManager& cameraManager)
  : m_cameraManager(cameraManager) {
  std::cout << "[INFO] UICameraPanelLiveVideo created with ICameraHardware interface" << std::endl;
}

UICameraPanelLiveVideo::~UICameraPanelLiveVideo() {
  ClearCamera();
  CleanupTexture();
  std::cout << "[INFO] UICameraPanelLiveVideo destroyed" << std::endl;
}

bool UICameraPanelLiveVideo::ValidateCamera() const {
  return m_currentCamera != nullptr && m_currentCamera->IsConnected();
}

void UICameraPanelLiveVideo::ToggleLiveVideo() {
  if (!ValidateCamera()) {
    std::cout << "[WARN] Cannot toggle live video: camera not valid" << std::endl;
    return;
  }

  if (m_isGrabbing) {
    // Stop live video
    m_cameraManager.StopGrabbing(m_currentCameraId);
  }
  else {
    // Start live video with broadcasting
    m_cameraManager.StartGrabbingWithBroadcast(m_currentCameraId);
  }

  UpdateGrabbingState();
}

void UICameraPanelLiveVideo::UpdateGrabbingState() {
  if (ValidateCamera()) {
    m_isGrabbing = m_currentCamera->IsGrabbing();
  }
  else {
    m_isGrabbing = false;
  }
}

void UICameraPanelLiveVideo::UpdateTextureFromFrameData(const CameraFrameData& frameData) {
  if (!frameData.IsValid() || frameData.imageData.empty()) {
    return;
  }

  // Create or update OpenGL texture
  if (m_textureId == 0) {
    glGenTextures(1, &m_textureId);
  }

  // Check if texture size changed
  if (m_textureWidth != frameData.width || m_textureHeight != frameData.height) {
    m_textureWidth = frameData.width;
    m_textureHeight = frameData.height;
  }

  glBindTexture(GL_TEXTURE_2D, m_textureId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // Upload frame data to texture
  GLenum format = (frameData.channels == 3) ? GL_RGB : GL_RGBA;
  glTexImage2D(GL_TEXTURE_2D, 0, format, frameData.width, frameData.height,
    0, format, GL_UNSIGNED_BYTE, frameData.imageData.data());

  glBindTexture(GL_TEXTURE_2D, 0);
}

void UICameraPanelLiveVideo::CleanupTexture() {
  if (m_textureId != 0) {
    glDeleteTextures(1, &m_textureId);
    m_textureId = 0;
    m_textureWidth = 0;
    m_textureHeight = 0;
    m_needsTextureCleanup = false;
    std::cout << "[INFO] LiveVideo texture cleaned up" << std::endl;
  }
}

void UICameraPanelLiveVideo::RenderFeedDisplay() {
  if (!ValidateCamera()) {
    ImGui::Text("Camera not connected");
    return;
  }

  // Display area for live feed
  ImVec2 displaySize = ImGui::GetContentRegionAvail();
  displaySize.y -= 30; // Leave space for status info

  if (m_textureId == 0 || m_textureWidth == 0 || m_textureHeight == 0) {
    // No frame available yet
    ImGui::BeginChild("LiveFeedPlaceholder", displaySize, true);
    ImGui::SetCursorPos(ImVec2(displaySize.x * 0.5f - 50, displaySize.y * 0.5f - 10));
    ImGui::Text("No live feed");

    if (m_isGrabbing) {
      ImGui::SetCursorPos(ImVec2(displaySize.x * 0.5f - 60, displaySize.y * 0.5f + 10));
      ImGui::Text("Waiting for frames...");
    }
    else {
      ImGui::SetCursorPos(ImVec2(displaySize.x * 0.5f - 75, displaySize.y * 0.5f + 10));
      ImGui::Text("Click 'Start Live Video'");
    }

    ImGui::EndChild();
  }
  else {
    // Calculate aspect-preserving display size
    float aspectRatio = static_cast<float>(m_textureWidth) / static_cast<float>(m_textureHeight);
    ImVec2 imageSize;

    if (displaySize.x / aspectRatio <= displaySize.y) {
      imageSize.x = displaySize.x;
      imageSize.y = displaySize.x / aspectRatio;
    }
    else {
      imageSize.x = displaySize.y * aspectRatio;
      imageSize.y = displaySize.y;
    }

    // Center the image
    ImVec2 imagePos = ImVec2(
      (displaySize.x - imageSize.x) * 0.5f,
      (displaySize.y - imageSize.y) * 0.5f
    );

    ImGui::SetCursorPos(imagePos);
    ImGui::Image((ImTextureID)(intptr_t)m_textureId, imageSize);
  }

  // Status line at bottom
  ImGui::Text("Resolution: %dx%d | Status: %s | Type: %s",
    m_textureWidth, m_textureHeight,
    m_isGrabbing ? "Live" : "Stopped",
    m_currentCamera->GetCameraType() == ICameraHardware::CameraType::PYLON ? "Pylon" : "IDS");
}

void UICameraPanelLiveVideo::RenderTab(ICameraHardware* camera, const std::string& cameraId) {
  // Update camera reference if changed
  if (m_currentCamera != camera || m_currentCameraId != cameraId) {
    SetSelectedCamera(camera, cameraId);
  }

  // Process new frames from subscriber
  if (m_subscriber && m_subscriber->HasNewFrame()) {
    const auto& frameData = m_subscriber->GetLatestFrame();

    // Debug info (limited frequency)
    static int frameCount = 0;
    static auto lastDebugTime = std::chrono::steady_clock::now();
    frameCount++;

    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastDebugTime).count() >= 2) {
      std::cout << "[DEBUG] LiveVideo frames processed: " << frameCount
        << ", Latest frame: " << frameData.width << "x" << frameData.height
        << ", valid: " << (frameData.IsValid() ? "yes" : "no")
        << ", channels: " << (int)frameData.channels << std::endl;
      frameCount = 0;
      lastDebugTime = now;
    }

    // Only update texture if frame is valid
    if (frameData.IsValid() && frameData.channels == 3 && !frameData.imageData.empty()) {
      UpdateTextureFromFrameData(frameData);
    }
    else {
      std::cout << "[WARN] Received invalid frame data" << std::endl;
    }

    m_subscriber->MarkFrameConsumed();
  }

  // Update internal state
  UpdateGrabbingState();

  // LAYOUT: Split the tab into two columns
  ImVec2 availableSize = ImGui::GetContentRegionAvail();
  float leftColumnWidth = availableSize.x * 0.75f;  // 75% for video feed
  float rightColumnWidth = availableSize.x * 0.25f; // 25% for status

  // LEFT COLUMN: Video Feed and Controls
  ImGui::BeginChild("VideoFeedColumn", ImVec2(leftColumnWidth, availableSize.y), false);

  // Render controls at top of left column
  RenderControls();

  ImGui::Separator();

  // Render live feed display (takes remaining space)
  RenderFeedDisplay();

  ImGui::EndChild();

  ImGui::SameLine();

  // RIGHT COLUMN: Status Information
  ImGui::BeginChild("StatusColumn", ImVec2(rightColumnWidth, availableSize.y), true);

  // Status header
  ImGui::Text("Live Feed Status");
  ImGui::Separator();

  // Render detailed status
  RenderDetailedStatus();

  ImGui::EndChild();
}

void UICameraPanelLiveVideo::SetSelectedCamera(ICameraHardware* camera, const std::string& cameraId) {
  // Clear previous camera
  if (m_currentCamera != camera) {
    ClearCamera();
  }

  m_currentCamera = camera;
  m_currentCameraId = cameraId;

  if (camera) {
    std::cout << "[INFO] LiveVideo panel set to camera: " << cameraId
      << " (Type: " << (camera->GetCameraType() == ICameraHardware::CameraType::PYLON ? "Pylon" : "IDS")
      << ")" << std::endl;

    // Create or update subscriber
    if (!m_subscriber) {
      m_subscriber = std::make_shared<LiveVideoSubscriber>(cameraId);
      // Subscribe to camera manager's broadcasting system
      m_cameraManager.SubscribeToFrames(m_subscriber);
      std::cout << "[INFO] Created and subscribed LiveVideoSubscriber for " << cameraId << std::endl;
    }
    else {
      // Update existing subscriber to watch new camera
      m_subscriber->SetTargetCamera(cameraId);
      std::cout << "[INFO] Updated LiveVideoSubscriber target to " << cameraId << std::endl;
    }

    UpdateGrabbingState();
  }
}

void UICameraPanelLiveVideo::ClearCamera() {
  // Unsubscribe from broadcasting
  if (m_subscriber) {
    m_cameraManager.UnsubscribeFromFrames(m_subscriber->GetSubscriberId());
    m_subscriber.reset();
    std::cout << "[INFO] Unsubscribed and cleared LiveVideoSubscriber" << std::endl;
  }

  // Clear texture
  CleanupTexture();

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

void UICameraPanelLiveVideo::RenderDetailedStatus() {
  if (!ValidateCamera()) {
    ImGui::Text("No camera selected");
    return;
  }

  // Use ICameraHardware interface for status
  bool isConnected = m_currentCamera->IsConnected();
  bool isGrabbing = m_currentCamera->IsGrabbing();

  // Connection status with color coding
  ImGui::Text("Connection:");
  ImGui::SameLine();
  if (isConnected) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Yes");
  }
  else {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "No");
  }

  // Grabbing status with color coding
  ImGui::Text("Grabbing:");
  ImGui::SameLine();
  if (isGrabbing) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Yes");
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "No");
  }

  // Broadcasting status
  ImGui::Text("Broadcasting:");
  ImGui::SameLine();
  if (m_subscriber) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Active");
  }
  else {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Inactive");
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Camera information
  ImGui::Text("Camera Info:");
  ImGui::Text("Model: %s", m_currentCamera->GetModelName().c_str());
  ImGui::Text("Serial: %s", m_currentCamera->GetSerialNumber().c_str());
  ImGui::Text("Vendor: %s", m_currentCamera->GetVendorName().c_str());
  ImGui::Text("Type: %s", m_currentCamera->GetCameraType() == ICameraHardware::CameraType::PYLON ? "Pylon" : "IDS");

  // Current exposure settings
  if (isConnected) {
    auto settings = m_currentCamera->GetExposureSettings();
    ImGui::Spacing();
    ImGui::Text("Exposure Settings:");
    ImGui::Text("Time: %.0f μs", settings.exposure_time);
    ImGui::Text("Gain: %.1f", settings.gain);
    ImGui::Text("Auto Exp: %s", settings.auto_exposure ? "On" : "Off");
    ImGui::Text("Auto Gain: %s", settings.auto_gain ? "On" : "Off");
  }

  // Frame information
  if (m_textureId > 0) {
    ImGui::Spacing();
    ImGui::Text("Current Frame:");
    ImGui::Text("Size: %dx%d", m_textureWidth, m_textureHeight);
    ImGui::Text("Texture ID: %u", m_textureId);
  }
}