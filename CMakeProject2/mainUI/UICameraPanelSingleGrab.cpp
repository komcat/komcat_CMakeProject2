// UICameraPanelSingleGrab.cpp - Single Frame Capture Panel Implementation
#include "UICameraPanelSingleGrab.h"
#include "include/camera/CameraManager.h"
#include "include/camera/ICameraHardware.h"
#include "include/camera/CameraFrameData.h"
#include "imgui.h"
#include "GL/gl.h"
#include <iostream>
#include <chrono>
#include <thread>

UICameraPanelSingleGrab::UICameraPanelSingleGrab(CameraManager& cameraManager)
  : m_cameraManager(cameraManager) {
  std::cout << "[INFO] UICameraPanelSingleGrab created with ICameraHardware interface" << std::endl;
}

UICameraPanelSingleGrab::~UICameraPanelSingleGrab() {
  ClearCamera();
  CleanupCapturedTexture();
  std::cout << "[INFO] UICameraPanelSingleGrab destroyed" << std::endl;
}

bool UICameraPanelSingleGrab::ValidateCamera() const {
  return m_currentCamera != nullptr && m_currentCamera->IsConnected();
}

void UICameraPanelSingleGrab::CaptureSingleFrame() {
  if (!ValidateCamera()) {
    m_lastCaptureStatus = "Camera not connected";
    std::cout << "[WARN] Cannot capture: camera not valid" << std::endl;
    return;
  }

  m_captureInProgress = true;
  m_lastCaptureStatus = "Capturing...";

  try {
    // Use ICameraHardware interface to capture frame
    CameraFrameData frameData;
    if (m_currentCamera->CaptureFrame(frameData)) {
      if (frameData.IsValid() && !frameData.imageData.empty()) {
        UpdateCapturedTexture(frameData);
        m_lastCaptureStatus = "Capture successful";
        std::cout << "[INFO] Single frame captured: " << frameData.width << "x" << frameData.height << std::endl;
      }
      else {
        m_lastCaptureStatus = "Invalid frame data received";
        std::cout << "[WARN] Captured frame data is invalid" << std::endl;
      }
    }
    else {
      m_lastCaptureStatus = "Capture failed: " + m_currentCamera->GetLastError();
      std::cout << "[ERROR] Frame capture failed: " << m_currentCamera->GetLastError() << std::endl;
    }
  }
  catch (const std::exception& e) {
    m_lastCaptureStatus = "Exception during capture: " + std::string(e.what());
    std::cout << "[ERROR] Exception during single frame capture: " << e.what() << std::endl;
  }

  m_captureInProgress = false;
}

void UICameraPanelSingleGrab::ClearCapturedFrame() {
  CleanupCapturedTexture();
  m_hasCapturedFrame = false;
  m_lastCaptureStatus = "Frame cleared";
  std::cout << "[INFO] Captured frame cleared" << std::endl;
}

void UICameraPanelSingleGrab::UpdateCapturedTexture(const CameraFrameData& frameData) {
  if (!frameData.IsValid() || frameData.imageData.empty()) {
    return;
  }

  // Create or update OpenGL texture
  if (m_capturedTextureId == 0) {
    glGenTextures(1, &m_capturedTextureId);
  }

  m_capturedWidth = frameData.width;
  m_capturedHeight = frameData.height;

  glBindTexture(GL_TEXTURE_2D, m_capturedTextureId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // Upload frame data to texture
  GLenum format = (frameData.channels == 3) ? GL_RGB : GL_RGBA;
  glTexImage2D(GL_TEXTURE_2D, 0, format, frameData.width, frameData.height,
    0, format, GL_UNSIGNED_BYTE, frameData.imageData.data());

  glBindTexture(GL_TEXTURE_2D, 0);
  m_hasCapturedFrame = true;
}

void UICameraPanelSingleGrab::CleanupCapturedTexture() {
  if (m_capturedTextureId != 0) {
    glDeleteTextures(1, &m_capturedTextureId);
    m_capturedTextureId = 0;
    m_capturedWidth = 0;
    m_capturedHeight = 0;
    std::cout << "[INFO] Single grab texture cleaned up" << std::endl;
  }
}

void UICameraPanelSingleGrab::RenderTab(ICameraHardware* camera, const std::string& cameraId) {
  // Update camera reference if changed
  if (m_currentCamera != camera || m_currentCameraId != cameraId) {
    SetSelectedCamera(camera, cameraId);
  }

  // LAYOUT: Split the tab into two columns
  ImVec2 availableSize = ImGui::GetContentRegionAvail();
  float leftColumnWidth = availableSize.x * 0.75f;  // 75% for captured frame display
  float rightColumnWidth = availableSize.x * 0.25f; // 25% for controls

  // LEFT COLUMN: Captured Frame Display
  ImGui::BeginChild("CapturedFrameColumn", ImVec2(leftColumnWidth, availableSize.y), false);

  RenderCapturedFrameDisplay();

  ImGui::EndChild();

  ImGui::SameLine();

  // RIGHT COLUMN: Capture Controls and Status
  ImGui::BeginChild("CaptureControlsColumn", ImVec2(rightColumnWidth, availableSize.y), true);

  // Controls header
  ImGui::Text("Single Frame Capture");
  ImGui::Separator();

  // Render capture controls
  RenderCaptureControls();

  ImGui::Separator();

  // Render capture status
  RenderCaptureStatus();

  ImGui::EndChild();
}

void UICameraPanelSingleGrab::RenderCaptureControls() {
  if (!ValidateCamera()) {
    ImGui::Text("Camera not connected");
    return;
  }

  // Main capture button
  ImVec4 buttonColor = m_captureInProgress ?
    ImVec4(0.7f, 0.7f, 0.7f, 1.0f) : ImVec4(0.3f, 0.8f, 0.3f, 1.0f);

  ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);

  bool buttonEnabled = !m_captureInProgress;
  if (!buttonEnabled) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }

  if (ImGui::Button("Capture Frame", ImVec2(-1, 40))) {
    if (!m_captureInProgress) {
      CaptureSingleFrame();
    }
  }

  if (!buttonEnabled) {
    ImGui::PopStyleVar();
  }
  ImGui::PopStyleColor();

  // Clear captured frame button
  ImGui::Spacing();
  if (ImGui::Button("Clear Frame", ImVec2(-1, 30))) {
    ClearCapturedFrame();
  }

  // Save captured frame button
  if (m_hasCapturedFrame) {
    ImGui::Spacing();
    if (ImGui::Button("Save to Disk", ImVec2(-1, 30))) {
      // Use camera manager's capture image function which saves to disk
      if (m_cameraManager.CaptureImage(m_currentCameraId)) {
        m_lastCaptureStatus = "Saved to: " + m_cameraManager.GetLastCapturedImagePath();
      }
      else {
        m_lastCaptureStatus = "Failed to save image";
      }
    }

  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Exposure quick controls
  ImGui::Text("Quick Settings:");

  if (ImGui::Button("Fast Exp.", ImVec2(-1, 25))) {
    ICameraHardware::ExposureSettings settings(1000.0, 1.0, false, false);
    m_cameraManager.ApplyExposureSettings(m_currentCameraId, settings);
  }

  if (ImGui::Button("Normal Exp.", ImVec2(-1, 25))) {
    ICameraHardware::ExposureSettings settings(10000.0, 1.0, false, false);
    m_cameraManager.ApplyExposureSettings(m_currentCameraId, settings);
  }

  if (ImGui::Button("Slow Exp.", ImVec2(-1, 25))) {
    ICameraHardware::ExposureSettings settings(50000.0, 2.0, false, false);
    m_cameraManager.ApplyExposureSettings(m_currentCameraId, settings);
  }
}

void UICameraPanelSingleGrab::RenderCapturedFrameDisplay() {
  if (!ValidateCamera()) {
    ImGui::Text("Camera not connected");
    return;
  }

  // Display area for captured frame
  ImVec2 displaySize = ImGui::GetContentRegionAvail();
  displaySize.y -= 30; // Leave space for status info

  if (!m_hasCapturedFrame || m_capturedTextureId == 0) {
    // No captured frame available
    ImGui::BeginChild("CaptureFramePlaceholder", displaySize, true);
    ImGui::SetCursorPos(ImVec2(displaySize.x * 0.5f - 70, displaySize.y * 0.5f - 20));

    if (m_captureInProgress) {
      ImGui::Text("Capturing frame...");
    }
    else {
      ImGui::Text("No captured frame");
      ImGui::SetCursorPos(ImVec2(displaySize.x * 0.5f - 85, displaySize.y * 0.5f + 10));
      ImGui::Text("Click 'Capture Frame' to capture");
    }

    ImGui::EndChild();
  }
  else {
    // Calculate aspect-preserving display size
    float aspectRatio = static_cast<float>(m_capturedWidth) / static_cast<float>(m_capturedHeight);
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
    ImGui::Image((ImTextureID)(intptr_t)m_capturedTextureId, imageSize);
  }

  // Status line at bottom
  ImGui::Text("Captured: %dx%d | Status: %s | Type: %s",
    m_capturedWidth, m_capturedHeight,
    m_hasCapturedFrame ? "Ready" : "None",
    m_currentCamera->GetCameraType() == ICameraHardware::CameraType::PYLON ? "Pylon" : "IDS");
}

void UICameraPanelSingleGrab::RenderCaptureStatus() {
  if (!ValidateCamera()) {
    ImGui::Text("No camera selected");
    return;
  }

  ImGui::Text("Capture Status:");

  // Status message with color coding
  if (!m_lastCaptureStatus.empty()) {
    ImVec4 statusColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Default gray

    if (m_lastCaptureStatus.find("successful") != std::string::npos) {
      statusColor = ImVec4(0, 1, 0, 1); // Green for success
    }
    else if (m_lastCaptureStatus.find("failed") != std::string::npos ||
      m_lastCaptureStatus.find("Exception") != std::string::npos) {
      statusColor = ImVec4(1, 0, 0, 1); // Red for errors
    }
    else if (m_lastCaptureStatus.find("Capturing") != std::string::npos) {
      statusColor = ImVec4(1, 1, 0, 1); // Yellow for in progress
    }

    ImGui::TextColored(statusColor, "%s", m_lastCaptureStatus.c_str());
  }
  else {
    ImGui::Text("Ready");
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Camera information
  ImGui::Text("Camera Info:");
  ImGui::Text("Model: %s", m_currentCamera->GetModelName().c_str());
  ImGui::Text("Serial: %s", m_currentCamera->GetSerialNumber().c_str());
  ImGui::Text("Type: %s", m_currentCamera->GetCameraType() == ICameraHardware::CameraType::PYLON ? "Pylon" : "IDS");

  // Current exposure settings
  if (m_currentCamera->IsConnected()) {
    auto settings = m_currentCamera->GetExposureSettings();
    ImGui::Spacing();
    ImGui::Text("Current Settings:");
    ImGui::Text("Exp: %.0f μs", settings.exposure_time);
    ImGui::Text("Gain: %.1f", settings.gain);
    ImGui::Text("Auto: %s/%s",
      settings.auto_exposure ? "E" : "-",
      settings.auto_gain ? "G" : "-");
  }

  // Frame information
  if (m_hasCapturedFrame) {
    ImGui::Spacing();
    ImGui::Text("Frame Info:");
    ImGui::Text("Size: %dx%d", m_capturedWidth, m_capturedHeight);
    ImGui::Text("Texture ID: %u", m_capturedTextureId);
  }
}

void UICameraPanelSingleGrab::SetSelectedCamera(ICameraHardware* camera, const std::string& cameraId) {
  // Clear previous camera
  if (m_currentCamera != camera) {
    ClearCamera();
  }

  m_currentCamera = camera;
  m_currentCameraId = cameraId;

  if (camera) {
    std::cout << "[INFO] SingleGrab panel set to camera: " << cameraId
      << " (Type: " << (camera->GetCameraType() == ICameraHardware::CameraType::PYLON ? "Pylon" : "IDS")
      << ")" << std::endl;

    m_lastCaptureStatus = "Camera selected";
  }
}

void UICameraPanelSingleGrab::ClearCamera() {
  // Clear captured frame when changing cameras
  ClearCapturedFrame();

  m_currentCamera = nullptr;
  m_currentCameraId = "";
  m_captureInProgress = false;
  m_lastCaptureStatus = "";

  std::cout << "[INFO] SingleGrab panel camera cleared" << std::endl;
}