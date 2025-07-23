// MacroPanelCameraHandler.cpp - Updated with Broadcasting Support
#include "MacroPanelCameraHandler.h"
#include "LiveVideoSubscriber.h"
#include "include/camera/CameraManager.h"
#include "include/camera/pylon_camera_test.h"
#include "include/camera/pylon_camera.h"
#include "imgui.h"
#include <iostream>
#include <chrono>
#include <SDL_opengl.h>  // For texture management

MacroPanelCameraHandler::MacroPanelCameraHandler(CameraManager* cameraManager)
  : m_cameraManager(cameraManager) {

  // Try to get first available camera if camera manager is available
  if (m_cameraManager) {
    auto cameraIds = m_cameraManager->GetCameraIds();
    if (!cameraIds.empty()) {
      m_selectedCameraId = cameraIds[0];
      InitializeCameraFeed();
    }
  }

  std::cout << "[INFO] MacroPanelCameraHandler created with broadcasting support" << std::endl;
}

MacroPanelCameraHandler::~MacroPanelCameraHandler() {
  // NEW: Clean up subscriber and texture
  if (m_subscriber && m_cameraManager) {
    m_cameraManager->UnsubscribeFromFrames(m_subscriber->GetSubscriberId());
    std::cout << "[INFO] Unsubscribed MacroPanelCameraHandler from broadcasting" << std::endl;
  }

  CleanupTexture();
  std::cout << "[INFO] MacroPanelCameraHandler destroyed" << std::endl;
}

void MacroPanelCameraHandler::SetCameraManager(CameraManager* cameraManager) {
  // Clean up previous connection
  if (m_subscriber && m_cameraManager) {
    m_cameraManager->UnsubscribeFromFrames(m_subscriber->GetSubscriberId());
    m_subscriber.reset();
  }

  m_cameraManager = cameraManager;

  // Reset camera state when manager changes
  m_cameraInitialized = false;
  m_selectedCameraId.clear();
  CleanupTexture();

  // Try to initialize with first available camera
  if (m_cameraManager) {
    auto cameraIds = m_cameraManager->GetCameraIds();
    if (!cameraIds.empty()) {
      m_selectedCameraId = cameraIds[0];
      InitializeCameraFeed();
    }
  }

  std::cout << "[INFO] MacroPanelCameraHandler camera manager updated" << std::endl;
}

void MacroPanelCameraHandler::SetSelectedCameraId(const std::string& cameraId) {
  if (m_selectedCameraId != cameraId) {
    m_selectedCameraId = cameraId;
    InitializeCameraFeed();
  }
}

void MacroPanelCameraHandler::InitializeCameraFeed() {
  // Clean up previous subscriber
  if (m_subscriber && m_cameraManager) {
    m_cameraManager->UnsubscribeFromFrames(m_subscriber->GetSubscriberId());
    m_subscriber.reset();
  }

  CleanupTexture();
  m_cameraInitialized = false;

  if (m_selectedCameraId.empty() || !m_cameraManager) {
    return;
  }

  PylonCameraTest* camera = m_cameraManager->GetCamera(m_selectedCameraId);
  if (camera) {
    // NEW: Create subscriber for broadcasting
    m_subscriber = std::make_shared<LiveVideoSubscriber>(m_selectedCameraId);

    // Subscribe to camera manager's broadcasting system
    m_cameraManager->SubscribeToFrames(m_subscriber);

    // Start broadcast system if not already started
    m_cameraManager->StartBroadcastSystem();

    m_cameraInitialized = true;

    std::cout << "[INFO] MacroPanel camera feed initialized for: " << m_selectedCameraId << std::endl;
    std::cout << "[INFO] Created and subscribed LiveVideoSubscriber: " << m_subscriber->GetSubscriberId() << std::endl;
  }
  else {
    std::cout << "[WARNING] Camera not found for MacroPanel: " << m_selectedCameraId << std::endl;
  }

  UpdateGrabbingState();
}

void MacroPanelCameraHandler::RenderCameraCanvas() {
  ImGui::Text("Camera Feed");

  // Camera selection and controls
  RenderCameraControls();
  ImGui::Spacing();

  // Calculate canvas size
  ImVec2 canvasSize = CalculateCanvasSize();

  // Create camera canvas
  ImGui::BeginChild("MacroCameraCanvas", canvasSize, false, ImGuiWindowFlags_NoScrollbar);

  // NEW: Update texture from broadcasted frames
  if (m_subscriber) {
    // Process all available frames
    while (m_subscriber->HasNewFrame()) {
      const auto frameData = m_subscriber->GetLatestFrame();

      // Only update texture if frame is valid
      if (frameData.IsValid() && frameData.channels == 3 && !frameData.imageData.empty()) {
        UpdateTextureFromFrameData(frameData);
      }

      m_subscriber->MarkFrameConsumed();
    }
  }

  // Update internal state
  UpdateGrabbingState();

  // Render camera feed or placeholder
  if (m_cameraInitialized && m_textureInitialized) {
    RenderCameraFeed(canvasSize);
  }
  else {
    RenderPlaceholder(canvasSize);
  }

  ImGui::EndChild();
}

void MacroPanelCameraHandler::RenderCameraControls() {
  if (!m_cameraManager) {
    ImGui::Text("Camera Manager not available");
    return;
  }

  auto cameraIds = m_cameraManager->GetCameraIds();
  if (!cameraIds.empty()) {
    ImGui::Text("Camera:");

    // Find current selection index
    int currentCameraIndex = 0;
    for (size_t i = 0; i < cameraIds.size(); i++) {
      if (cameraIds[i] == m_selectedCameraId) {
        currentCameraIndex = (int)i;
        break;
      }
    }

    // Create camera selection array
    std::vector<const char*> cameraNames;
    for (const auto& id : cameraIds) {
      cameraNames.push_back(id.c_str());
    }

    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##MacroCameraSelection", &currentCameraIndex, cameraNames.data(), (int)cameraNames.size())) {
      SetSelectedCameraId(cameraIds[currentCameraIndex]);
      std::cout << "[INFO] MacroPanel camera selection changed to: " << m_selectedCameraId << std::endl;
    }
  }
  else {
    ImGui::Text("No cameras available");
  }

  // Camera controls for selected camera
  if (!m_selectedCameraId.empty()) {
    ImGui::Spacing();
    PylonCameraTest* camera = m_cameraManager->GetCamera(m_selectedCameraId);
    if (camera) {
      auto& pylonCamera = camera->GetCamera();

      // Connection status
      ImGui::Text("Status: %s", pylonCamera.IsConnected() ? "Connected" : "Disconnected");

      if (pylonCamera.IsConnected()) {
        ImGui::SameLine();
        ImGui::Text("| Grabbing: %s", pylonCamera.IsGrabbing() ? "Yes" : "No");

        // Start/Stop grabbing controls
        if (!pylonCamera.IsGrabbing()) {
          if (ImGui::Button("Start Live Feed", ImVec2(120, 25))) {
            StartLiveVideo();
          }
        }
        else {
          if (ImGui::Button("Stop Live Feed", ImVec2(120, 25))) {
            StopLiveVideo();
          }
        }

        ImGui::SameLine();
        if (ImGui::Button("Reconnect", ImVec2(80, 25))) {
          m_cameraManager->ConnectCamera(m_selectedCameraId);
          std::cout << "[INFO] MacroPanel reconnect camera: " << m_selectedCameraId << std::endl;
        }
      }
      else {
        if (ImGui::Button("Connect Camera", ImVec2(120, 25))) {
          m_cameraManager->ConnectCamera(m_selectedCameraId);
          InitializeCameraFeed();
          std::cout << "[INFO] MacroPanel connecting camera: " << m_selectedCameraId << std::endl;
        }
      }
    }
  }
}

void MacroPanelCameraHandler::RenderCameraFeed(ImVec2 canvasSize) {
  if (!m_textureInitialized || m_textureWidth == 0 || m_textureHeight == 0) {
    RenderPlaceholder(canvasSize);
    return;
  }

  // Verify texture is still valid in OpenGL
  GLboolean isValid = glIsTexture(m_textureID);
  if (!isValid) {
    std::cout << "[ERROR] MacroPanel texture ID " << m_textureID << " is no longer valid!" << std::endl;
    RenderPlaceholder(canvasSize);
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

  // Add debug tooltip on hover
  if (ImGui::IsItemHovered()) {
    uint64_t totalFrames = m_subscriber ? m_subscriber->GetTotalFramesReceived() : 0;
    ImGui::SetTooltip("MacroPanel Live Video Feed\nTexture ID: %u\nResolution: %ux%u\nTotal Frames: %llu",
      m_textureID, m_textureWidth, m_textureHeight, totalFrames);
  }
}

void MacroPanelCameraHandler::RenderPlaceholder(ImVec2 canvasSize) {
  // Show placeholder when no camera feed
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();
  ImVec2 canvasMax = ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);

  // Background
  drawList->AddRectFilled(canvasPos, canvasMax, IM_COL32(80, 80, 80, 255));
  drawList->AddRect(canvasPos, canvasMax, IM_COL32(100, 150, 200, 255), 0.0f, 0, 2.0f);

  // Status text
  std::string statusText = "Live Camera Feed\n(MacroPanel)";
  if (!m_selectedCameraId.empty()) {
    statusText += "\nCamera: " + m_selectedCameraId;

    if (m_subscriber) {
      statusText += "\nSubscriber: " + m_subscriber->GetSubscriberId();
      statusText += "\nFrames: " + std::to_string(m_subscriber->GetTotalFramesReceived());
    }
  }
  else {
    statusText += "\n(No camera selected)";
  }

  ImVec2 textSize = ImGui::CalcTextSize(statusText.c_str());
  ImVec2 textPos = ImVec2(
    canvasPos.x + (canvasSize.x - textSize.x) * 0.5f,
    canvasPos.y + (canvasSize.y - textSize.y) * 0.5f
  );
  drawList->AddText(textPos, IM_COL32(200, 200, 200, 255), statusText.c_str());

  // Advance cursor
  ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, canvasPos.y + canvasSize.y));
}

ImVec2 MacroPanelCameraHandler::CalculateCanvasSize() {
  // Camera canvas area with fixed aspect ratio
  const float CAMERA_ASPECT_RATIO = 1280.0f / 1024.0f; // 5:4 aspect ratio

  float availableWidth = ImGui::GetContentRegionAvail().x;
  float availableHeight = ImGui::GetContentRegionAvail().y - 10.0f; // Small margin

  // Calculate canvas size maintaining aspect ratio
  ImVec2 canvasSize;
  canvasSize.x = availableWidth;
  canvasSize.y = availableWidth / CAMERA_ASPECT_RATIO;

  // Ensure it fits in available height
  if (canvasSize.y > availableHeight) {
    canvasSize.y = availableHeight;
    canvasSize.x = availableHeight * CAMERA_ASPECT_RATIO;
  }

  // Ensure minimum size
  if (canvasSize.x < 160.0f) {
    canvasSize.x = 160.0f;
    canvasSize.y = 160.0f / CAMERA_ASPECT_RATIO;
  }

  return canvasSize;
}

// NEW: Camera operations with broadcasting
void MacroPanelCameraHandler::StartLiveVideo() {
  if (!ValidateCamera()) {
    return;
  }

  if (!m_isGrabbing) {
    std::cout << "[INFO] MacroPanel starting live video with broadcasting for: " << m_selectedCameraId << std::endl;

    // Use CameraManager's StartGrabbing (which handles broadcasting automatically)
    if (m_cameraManager->StartGrabbing(m_selectedCameraId)) {
      std::cout << "[INFO] MacroPanel StartGrabbing successful" << std::endl;

      // Ensure broadcast system is started
      m_cameraManager->StartBroadcastSystem();
      std::cout << "[INFO] MacroPanel broadcast system started" << std::endl;
    }
    else {
      std::cout << "[ERROR] MacroPanel StartGrabbing failed" << std::endl;
    }

    UpdateGrabbingState();
  }
}

void MacroPanelCameraHandler::StopLiveVideo() {
  if (!ValidateCamera()) {
    return;
  }

  if (m_isGrabbing) {
    std::cout << "[INFO] MacroPanel stopping live video for: " << m_selectedCameraId << std::endl;

    // Clear texture first
    CleanupTexture();

    m_cameraManager->StopGrabbing(m_selectedCameraId);
    UpdateGrabbingState();
  }
}

void MacroPanelCameraHandler::ToggleLiveVideo() {
  if (m_isGrabbing) {
    StopLiveVideo();
  }
  else {
    StartLiveVideo();
  }
}

// NEW: Frame processing methods
void MacroPanelCameraHandler::UpdateTextureFromFrameData(const CameraFrameData& frameData) {
  if (frameData.IsValid() && frameData.channels == 3) {
    CreateOrUpdateTexture(frameData.imageData.data(), frameData.width, frameData.height);
  }
}

void MacroPanelCameraHandler::CreateOrUpdateTexture(const uint8_t* imageData, uint32_t width, uint32_t height) {
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

    std::cout << "[INFO] MacroPanel created OpenGL texture " << m_textureID << " ("
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

      std::cout << "[INFO] MacroPanel resized texture to " << width << "x" << height << std::endl;
    }
    else {
      // Update existing texture (more efficient)
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, imageData);
    }
  }

  glBindTexture(GL_TEXTURE_2D, 0);
}

void MacroPanelCameraHandler::CleanupTexture() {
  if (m_textureInitialized) {
    glDeleteTextures(1, &m_textureID);
    m_textureInitialized = false;
    m_textureID = 0;
    m_textureWidth = 0;
    m_textureHeight = 0;

    std::cout << "[INFO] MacroPanel cleaned up OpenGL texture" << std::endl;
  }
}

void MacroPanelCameraHandler::UpdateGrabbingState() {
  if (!ValidateCamera()) {
    m_isGrabbing = false;
    return;
  }

  bool wasGrabbing = m_isGrabbing;

  // Use subscriber status if available
  if (m_subscriber) {
    m_isGrabbing = m_subscriber->IsCameraGrabbing();
  }
  else {
    PylonCameraTest* camera = m_cameraManager->GetCamera(m_selectedCameraId);
    if (camera) {
      auto& pylonCamera = camera->GetCamera();
      m_isGrabbing = pylonCamera.IsGrabbing();
    }
    else {
      m_isGrabbing = false;
    }
  }

  // Log state changes
  if (wasGrabbing != m_isGrabbing) {
    std::cout << "[INFO] MacroPanel grabbing state changed to: "
      << (m_isGrabbing ? "ON" : "OFF") << " for " << m_selectedCameraId << std::endl;
  }
}

bool MacroPanelCameraHandler::ValidateCamera() const {
  if (m_selectedCameraId.empty() || !m_cameraManager) {
    return false;
  }

  // Use subscriber status if available
  if (m_subscriber) {
    return m_subscriber->IsCameraConnected();
  }

  PylonCameraTest* camera = m_cameraManager->GetCamera(m_selectedCameraId);
  if (!camera) {
    return false;
  }

  auto& pylonCamera = camera->GetCamera();
  return pylonCamera.IsConnected();
}

bool MacroPanelCameraHandler::IsLiveActive() const {
  return m_isGrabbing && ValidateCamera();
}

std::string MacroPanelCameraHandler::GetStatusText() const {
  if (!ValidateCamera()) {
    return "No Camera";
  }

  if (m_isGrabbing) {
    return "Live Active (Broadcasting)";
  }
  else {
    return "Live Stopped";
  }
}