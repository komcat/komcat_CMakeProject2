// UICameraPanelLiveVideo.cpp - Updated with Broadcasting Support
#include "UICameraPanelLiveVideo.h"
#include "LiveVideoSubscriber.h"  // NEW: Include our subscriber
#include "include/camera/CameraManager.h"
#include "include/camera/pylon_camera_test.h"
#include "include/camera/pylon_camera.h"
#include "imgui.h"
#include <iostream>
#include <chrono>
#include <SDL_opengl.h>  // NEW: For texture management

UICameraPanelLiveVideo::UICameraPanelLiveVideo(CameraManager& cameraManager)
  : m_cameraManager(cameraManager) {

  std::cout << "[INFO] UICameraPanelLiveVideo created with broadcasting support" << std::endl;
}

UICameraPanelLiveVideo::~UICameraPanelLiveVideo() {
  ClearCamera();
  CleanupTexture();  // NEW: Clean up OpenGL texture
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

  // NEW: Update texture from broadcasted frames with debugging
  static int frameCount = 0;
  static auto lastDebugTime = std::chrono::steady_clock::now();

  if (m_subscriber) {
    // CRITICAL: Always check for new frames, not just once
    while (m_subscriber->HasNewFrame()) {  // Changed from 'if' to 'while'
      const auto frameData = m_subscriber->GetLatestFrame();

      // Debug logging (remove after testing)
      frameCount++;
      auto now = std::chrono::steady_clock::now();
      if (std::chrono::duration_cast<std::chrono::seconds>(now - lastDebugTime).count() >= 2) {
        std::cout << "[DEBUG] LiveVideo received " << frameCount << " frames in 2s. "
          << "Latest frame: " << frameData.width << "x" << frameData.height
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
  }

  // Update internal state
  UpdateGrabbingState();

  // **LAYOUT: Split the tab into two columns**
  ImVec2 availableSize = ImGui::GetContentRegionAvail();
  float leftColumnWidth = availableSize.x * 0.75f;  // 75% for video feed
  float rightColumnWidth = availableSize.x * 0.25f; // 25% for status

  // **LEFT COLUMN: Video Feed and Controls**
  ImGui::BeginChild("VideoFeedColumn", ImVec2(leftColumnWidth, availableSize.y), false);

  // Render controls at top of left column
  RenderControls();

  ImGui::Separator();

  // Render live feed display (takes remaining space)
  RenderFeedDisplay();

  ImGui::EndChild();

  ImGui::SameLine();

  // **RIGHT COLUMN: Status Information**
  ImGui::BeginChild("StatusColumn", ImVec2(rightColumnWidth, availableSize.y), true);

  // Status header
  ImGui::Text("Live Feed Status");
  ImGui::Separator();

  // Render detailed status
  RenderDetailedStatus();

  ImGui::EndChild();
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

    // NEW: Create or update subscriber
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
  // NEW: Unsubscribe from broadcasting
  if (m_subscriber) {
    m_cameraManager.UnsubscribeFromFrames(m_subscriber->GetSubscriberId());
    m_subscriber.reset();
    std::cout << "[INFO] Unsubscribed and cleared LiveVideoSubscriber" << std::endl;
  }

  // NEW: Clear texture
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

  // NEW: Use subscriber status if available, otherwise fall back to direct camera access
  bool isConnected = false;
  bool isGrabbing = false;

  if (m_subscriber) {
    isConnected = m_subscriber->IsCameraConnected();
    isGrabbing = m_subscriber->IsCameraGrabbing();
  }
  else {
    auto& pylonCamera = m_currentCamera->GetCamera();
    isConnected = pylonCamera.IsConnected();
    isGrabbing = pylonCamera.IsGrabbing();
  }

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

  // NEW: Broadcasting status
  ImGui::Text("Broadcasting:");
  ImGui::SameLine();
  if (m_subscriber) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Yes");
  }
  else {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "No");
  }

  // Device status with color coding
  if (m_currentCamera) {
    auto& pylonCamera = m_currentCamera->GetCamera();
    ImGui::Text("Device OK:");
    ImGui::SameLine();
    if (pylonCamera.IsCameraDeviceRemoved()) {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "No");
    }
    else {
      ImGui::TextColored(ImVec4(0, 1, 0, 1), "Yes");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Camera Settings:");

    if (pylonCamera.IsConnected()) {
      auto settings = pylonCamera.GetCurrentExposureSettings();

      ImGui::Text("Exposure:");
      ImGui::SameLine();
      ImGui::Text("%.0f μs", settings.exposure_time);

      ImGui::Text("Gain:");
      ImGui::SameLine();
      ImGui::Text("%.1f", settings.gain);

      ImGui::Text("Auto Exposure:");
      ImGui::SameLine();
      if (settings.exposure_auto) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "On");
      }
      else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "Off");
      }

      ImGui::Text("Auto Gain:");
      ImGui::SameLine();
      if (settings.gain_auto) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "On");
      }
      else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "Off");
      }
    }
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Feed Information:");

  // NEW: Feed display status from texture
  ImGui::Text("Feed Active:");
  ImGui::SameLine();
  if (m_textureInitialized && m_textureWidth > 0 && m_textureHeight > 0) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Yes");

    ImGui::Text("Resolution:");
    ImGui::SameLine();
    ImGui::Text("%ux%u", m_textureWidth, m_textureHeight);

    // Calculate aspect ratio
    if (m_textureHeight > 0) {
      float aspectRatio = (float)m_textureWidth / (float)m_textureHeight;
      ImGui::Text("Aspect Ratio:");
      ImGui::SameLine();
      ImGui::Text("%.2f:1", aspectRatio);
    }
  }
  else {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "No");
  }

  // Frame rate estimation (simple)
  static int frameCounter = 0;
  static auto lastTime = std::chrono::steady_clock::now();
  static float estimatedFPS = 0.0f;

  if (m_isGrabbing && m_textureInitialized) {
    frameCounter++;
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime);

    if (elapsed.count() >= 1000) { // Update every second
      estimatedFPS = frameCounter * 1000.0f / elapsed.count();
      frameCounter = 0;
      lastTime = currentTime;
    }

    ImGui::Text("Est. FPS:");
    ImGui::SameLine();
    ImGui::Text("%.1f", estimatedFPS);
  }

  // NEW: Subscriber statistics
  if (m_subscriber) {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Broadcasting Stats:");

    ImGui::Text("Total Frames:");
    ImGui::SameLine();
    ImGui::Text("%llu", m_subscriber->GetTotalFramesReceived());

    size_t subscriberCount = m_cameraManager.GetSubscriberCount();
    ImGui::Text("Total Subscribers:");
    ImGui::SameLine();
    ImGui::Text("%zu", subscriberCount);
  }

  // Quick actions section
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Quick Actions:");

  if (ImGui::Button("Reconnect", ImVec2(-1, 25))) {
    if (isConnected && m_currentCamera) {
      m_currentCamera->GetCamera().TryReconnect();
    }
  }

  if (ImGui::Button("Debug Settings", ImVec2(-1, 25))) {
    if (isConnected && m_currentCamera) {
      m_currentCamera->GetCamera().DebugCameraSettings();
    }
  }
}

void UICameraPanelLiveVideo::RenderFeedDisplay() {
  // Calculate canvas size
  ImVec2 canvasSize = ImGui::GetContentRegionAvail();
  canvasSize.y = (std::max)(canvasSize.y - 10.0f, 200.0f); // Small margin

  if (!ValidateCamera()) {
    RenderErrorCanvas(canvasSize.x, canvasSize.y, "Camera Not Available\nSelect a connected camera");
    return;
  }

  // NEW: Debug texture state
  static auto lastTextureDebug = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  if (std::chrono::duration_cast<std::chrono::seconds>(now - lastTextureDebug).count() >= 3) {
    std::cout << "[DEBUG] Texture state - ID: " << m_textureID
      << ", initialized: " << (m_textureInitialized ? "yes" : "no")
      << ", size: " << m_textureWidth << "x" << m_textureHeight
      << ", subscriber: " << (m_subscriber ? "yes" : "no") << std::endl;

    if (m_subscriber) {
      std::cout << "[DEBUG] Subscriber - frames: " << m_subscriber->GetTotalFramesReceived()
        << ", has new: " << (m_subscriber->HasNewFrame() ? "yes" : "no") << std::endl;
    }
    lastTextureDebug = now;
  }

  // NEW: Display video from broadcasted frames instead of CameraFeedDisplay
  if (m_textureInitialized && m_textureWidth > 0 && m_textureHeight > 0) {
    // Verify texture is still valid in OpenGL
    GLboolean isValid = glIsTexture(m_textureID);
    if (!isValid) {
      std::cout << "[ERROR] Texture ID " << m_textureID << " is no longer valid in OpenGL!" << std::endl;
      RenderErrorCanvas(canvasSize.x, canvasSize.y, "Texture Error\nTexture became invalid");
      return;
    }

    // Calculate proper display size to fit container
    float aspectRatio = (float)m_textureWidth / (float)m_textureHeight;

    float displayWidth = canvasSize.x;
    float displayHeight = displayWidth / aspectRatio;

    // If height is too big, fit by height instead
    if (displayHeight > canvasSize.y) {
      displayHeight = canvasSize.y;
      displayWidth = displayHeight * aspectRatio;
    }

    // Center the image
    float offsetX = (canvasSize.x - displayWidth) * 0.5f;
    float offsetY = (canvasSize.y - displayHeight) * 0.5f;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);

    // Display the image
    ImGui::Image((ImTextureID)(intptr_t)m_textureID, ImVec2(displayWidth, displayHeight));

    // Add debug overlay on hover
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Live Video Feed\nTexture ID: %u\nResolution: %ux%u\nTotal Frames: %llu",
        m_textureID, m_textureWidth, m_textureHeight,
        m_subscriber ? m_subscriber->GetTotalFramesReceived() : 0);
    }
  }
  else {
    // Show placeholder when no video is available
    std::string errorText;
    if (!m_textureInitialized) {
      errorText = "No Texture Initialized\nWaiting for video frames...";
    }
    else if (m_textureWidth == 0 || m_textureHeight == 0) {
      errorText = "Invalid Texture Size\nCheck camera settings";
    }
    else {
      errorText = m_isGrabbing ?
        "Waiting for video frames...\nCheck camera connection" :
        "Live Video Stopped\nClick 'Start Live Video' to begin";
    }

    RenderErrorCanvas(canvasSize.x, canvasSize.y, errorText);
  }
}

void UICameraPanelLiveVideo::RenderErrorCanvas(float width, float height, const std::string& errorText) {
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();
  ImDrawList* drawList = ImGui::GetWindowDrawList();

  // Error canvas - dark background instead of red
  drawList->AddRectFilled(canvasPos,
    ImVec2(canvasPos.x + width, canvasPos.y + height),
    IM_COL32(40, 40, 40, 255));

  // Error border
  drawList->AddRect(canvasPos,
    ImVec2(canvasPos.x + width, canvasPos.y + height),
    IM_COL32(100, 100, 100, 255));

  // Error text
  ImVec2 textSize = ImGui::CalcTextSize(errorText.c_str());
  ImVec2 textPos = ImVec2(canvasPos.x + (width - textSize.x) * 0.5f,
    canvasPos.y + (height - textSize.y) * 0.5f);
  drawList->AddText(textPos, IM_COL32(200, 200, 200, 255), errorText.c_str());

  // Advance cursor
  ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, canvasPos.y + height));
}

void UICameraPanelLiveVideo::StartLiveVideo() {
  if (!ValidateCamera()) {
    return;
  }

  if (!m_isGrabbing) {
    std::cout << "[INFO] Starting live video with broadcasting for: " << m_currentCameraId << std::endl;

    // FIXED: Use standard StartGrabbing (which handles broadcasting automatically)
    if (m_cameraManager.StartGrabbing(m_currentCameraId)) {
      std::cout << "[INFO] StartGrabbing successful" << std::endl;

      // CRITICAL: Ensure broadcast system is started
      m_cameraManager.StartBroadcastSystem();
      std::cout << "[INFO] Broadcast system started" << std::endl;
    }
    else {
      std::cout << "[ERROR] StartGrabbing failed" << std::endl;
    }

    UpdateGrabbingState();
  }
}

void UICameraPanelLiveVideo::StopLiveVideo() {
  if (!ValidateCamera()) {
    return;
  }

  if (m_isGrabbing) {
    std::cout << "[INFO] Stopping live video for: " << m_currentCameraId << std::endl;

    // NEW: Clear texture first
    CleanupTexture();

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

// NEW: Frame processing methods
void UICameraPanelLiveVideo::UpdateTextureFromFrameData(const CameraFrameData& frameData) {
  if (frameData.IsValid() && frameData.channels == 3) {
    CreateOrUpdateTexture(frameData.imageData.data(), frameData.width, frameData.height);
  }
}

void UICameraPanelLiveVideo::CreateOrUpdateTexture(const uint8_t* imageData, uint32_t width, uint32_t height) {
  if (!imageData || width == 0 || height == 0) return;

  // Create texture if not initialized
  if (!m_textureInitialized) {
    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    m_textureInitialized = true;
    m_textureWidth = width;
    m_textureHeight = height;

    // Upload initial texture data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, imageData);

    std::cout << "[INFO] Created OpenGL texture " << m_textureID << " for live video ("
      << width << "x" << height << ")" << std::endl;
  }
  else {
    glBindTexture(GL_TEXTURE_2D, m_textureID);

    // Check if we need to resize texture
    if (width != m_textureWidth || height != m_textureHeight) {
      // Reallocate texture with new size
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, imageData);
      m_textureWidth = width;
      m_textureHeight = height;

      std::cout << "[INFO] Resized texture to " << width << "x" << height << std::endl;
    }
    else {
      // Update existing texture (more efficient)
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, imageData);
    }
  }

  glBindTexture(GL_TEXTURE_2D, 0);
}

void UICameraPanelLiveVideo::CleanupTexture() {
  if (m_textureInitialized) {
    glDeleteTextures(1, &m_textureID);
    m_textureInitialized = false;
    m_textureID = 0;
    m_textureWidth = 0;
    m_textureHeight = 0;

    std::cout << "[INFO] Cleaned up OpenGL texture for live video" << std::endl;
  }
}

void UICameraPanelLiveVideo::UpdateGrabbingState() {
  if (!ValidateCamera()) {
    m_isGrabbing = false;
    return;
  }

  bool wasGrabbing = m_isGrabbing;

  // NEW: Use subscriber status if available
  if (m_subscriber) {
    m_isGrabbing = m_subscriber->IsCameraGrabbing();
  }
  else {
    auto& pylonCamera = m_currentCamera->GetCamera();
    m_isGrabbing = pylonCamera.IsGrabbing();
  }

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

  // NEW: Use subscriber status if available
  if (m_subscriber) {
    return m_subscriber->IsCameraConnected();
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
    return "Live Active (Broadcasting)";  // NEW: Indicate broadcasting mode
  }
  else {
    return "Live Stopped";
  }
}