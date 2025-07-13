// CameraFeedDisplay.cpp - FIXED VERSION for proper texture initialization
#include "CameraFeedDisplay.h"
#include "include/camera/pylon_camera_test.h"
#include "include/camera/pylon_camera.h"
#include "imgui.h"
#include <SDL_opengl.h>
#include <iostream>
#include <chrono>

CameraFeedDisplay::CameraFeedDisplay() {
  // Initialize display settings
  m_maintainAspectRatio = true;
  m_centerImage = true;
  m_placeholderText = "No Camera Feed";
}

CameraFeedDisplay::~CameraFeedDisplay() {
  // Safe cleanup
  ClearSource();
}

void CameraFeedDisplay::ClearSource() {
  // Only clear if we're actually changing sources
  if (m_sourceType != SourceType::NONE) {
    CleanupTexture();
  }

  m_sourceType = SourceType::NONE;
  m_pylonCamera = nullptr;
}

void CameraFeedDisplay::SetPylonCameraSource(PylonCameraTest* camera) {
  // Only clear if camera actually changed
  if (m_pylonCamera != camera) {
    ClearSource();
    m_sourceType = SourceType::PYLON_CAMERA_TEST;
    m_pylonCamera = camera;

    // Reset frame statistics
    m_frameCount = 0;
    m_lastFrameTime = 0;

    //std::cout << "[DEBUG] Camera source set to: " << (void*)camera << std::endl;
  }
}

bool CameraFeedDisplay::UpdateTexture() {
  if (!HasSource()) {
    return false;
  }

  switch (m_sourceType) {
  case SourceType::PYLON_CAMERA_TEST:
    return UpdateFromPylonCamera();
  default:
    return false;
  }
}

bool CameraFeedDisplay::UpdateFromPylonCamera() {
  if (!m_pylonCamera) {
    return false;
  }

  auto& pylonCamera = m_pylonCamera->GetCamera();

  // Check if camera is in a state to provide frames
  if (!pylonCamera.IsConnected()) {
    return false;
  }

  // **FIX 1: Force camera to update its texture if grabbing**
  if (pylonCamera.IsGrabbing()) {
    m_pylonCamera->UpdateTextureIfReady();
  }

  // **FIX 2: Check if camera has valid texture**
  if (!m_pylonCamera->HasValidTexture()) {
    return m_hasValidFrame; // Keep previous state
  }

  // Get texture info from camera
  unsigned int cameraTextureID = m_pylonCamera->GetTextureID();
  uint32_t width = m_pylonCamera->GetImageWidth();
  uint32_t height = m_pylonCamera->GetImageHeight();

  if (cameraTextureID == 0 || width == 0 || height == 0) {
    return m_hasValidFrame;
  }

  // **FIX 3: Rate limiting but allow initial setup**
  static auto lastUpdate = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  auto timeSinceUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate);

  // Skip rate limiting if we don't have a valid frame yet (initial setup)
  bool needsInitialSetup = !m_hasValidFrame || !m_textureInitialized;
  bool rateLimitExceeded = (timeSinceUpdate.count() < 50) && !needsInitialSetup;

  if (rateLimitExceeded) {
    return true; // Skip this update, use cached frame
  }

  lastUpdate = now;

  //std::cout << "[DEBUG] UpdateFromPylonCamera: Camera texture ID=" << cameraTextureID    << " size=" << width << "x" << height    << " needsSetup=" << (needsInitialSetup ? "true" : "false") << std::endl;

  // **FIX 4: Create our own texture that references camera data properly**
  if (!m_textureInitialized || m_textureID != cameraTextureID) {

    // Clean up old texture if we had one
    if (m_textureInitialized && m_textureID != 0 && m_textureID != cameraTextureID) {
      glDeleteTextures(1, &m_textureID);
      //std::cout << "[DEBUG] Deleted old display texture" << std::endl;
    }

    // **CRITICAL FIX: Use camera's texture directly**
    m_textureID = cameraTextureID;
    m_textureInitialized = true;
    m_frameWidth = width;
    m_frameHeight = height;
    m_hasValidFrame = true;
    m_frameCount++;
    m_lastFrameTime = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    //std::cout << "[DEBUG] Linked to camera texture - Display texture ID now: " << m_textureID << std::endl;
    return true;
  }

  // **FIX 5: Update frame statistics even if texture ID hasn't changed**
  if (m_textureID == cameraTextureID && width == m_frameWidth && height == m_frameHeight) {
    m_hasValidFrame = true;
    m_frameCount++;
    m_lastFrameTime = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return true;
  }

  return m_hasValidFrame;
}

void CameraFeedDisplay::RenderToCanvas(float canvasWidth, float canvasHeight) {
  // **FIX 6: Always try to update texture, especially on first renders**
  bool textureUpdated = UpdateTexture();

  if (HasValidTexture() && m_sourceType != SourceType::NONE) {
    // Calculate display dimensions
    float displayWidth, displayHeight, offsetX, offsetY;
    CalculateDisplaySize(canvasWidth, canvasHeight, displayWidth, displayHeight, offsetX, offsetY);

    // Position cursor for image
    ImVec2 imagePos = ImGui::GetCursorPos();
    imagePos.x += offsetX;
    imagePos.y += offsetY;
    ImGui::SetCursorPos(imagePos);

    // **VERIFY TEXTURE IS STILL VALID BEFORE RENDERING**
    GLboolean isTexture = glIsTexture(m_textureID);
    if (isTexture) {
      // Display using the camera's texture
      ImGui::Image((ImTextureID)(intptr_t)m_textureID, ImVec2(displayWidth, displayHeight));
    }
    else {
      std::cout << "[ERROR] Texture became invalid during render!" << std::endl;
      // Reset texture state and show placeholder
      m_textureInitialized = false;
      m_hasValidFrame = false;
      RenderPlaceholder(canvasWidth, canvasHeight, "Texture Error\nRetry camera operation", IM_COL32(255, 128, 0, 255));
      return;
    }

    // Reset cursor to bottom of canvas
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);
  }
  else {
    // Display placeholder with helpful status messages
    std::string statusText = GetStatusText();
    uint32_t color = IM_COL32(128, 128, 128, 255);

    if (m_sourceType != SourceType::NONE) {
      if (!m_pylonCamera) {
        statusText = "Camera Source Invalid";
        color = IM_COL32(255, 128, 0, 255);
      }
      else {
        auto& pylonCamera = m_pylonCamera->GetCamera();
        if (!pylonCamera.IsConnected()) {
          statusText = "Camera Not Connected\nConnect camera to view feed";
          color = IM_COL32(255, 255, 0, 255);
        }
        else if (!pylonCamera.IsGrabbing()) {
          statusText = "Camera Connected\nStart grabbing to view feed";
          color = IM_COL32(0, 255, 255, 255);
        }
        else if (!m_pylonCamera->HasValidTexture()) {
          statusText = "Camera Grabbing\nWaiting for first frame...";
          color = IM_COL32(0, 255, 0, 255);
        }
        else {
          statusText = "Initializing Display...\nPlease wait";
          color = IM_COL32(255, 255, 0, 255);
        }
      }
    }
    else {
      statusText = m_placeholderText + "\nNo camera source set";
    }

    RenderPlaceholder(canvasWidth, canvasHeight, statusText, color);
  }
}

bool CameraFeedDisplay::InitializeTexture() {
  // We use the camera's texture directly, so no initialization needed
  return true;
}

void CameraFeedDisplay::CleanupTexture() {
  // **FIX 7: Don't delete camera's texture, just reset our references**
  m_textureID = 0;
  m_textureInitialized = false;
  m_hasValidFrame = false;
  m_frameWidth = 0;
  m_frameHeight = 0;

  //std::cout << "[DEBUG] Cleaned up texture state (no deletion of camera texture)" << std::endl;
}

void CameraFeedDisplay::RenderPlaceholder(float canvasWidth, float canvasHeight, const std::string& text, uint32_t color) {
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();
  ImDrawList* drawList = ImGui::GetWindowDrawList();

  // Canvas background
  drawList->AddRectFilled(canvasPos,
    ImVec2(canvasPos.x + canvasWidth, canvasPos.y + canvasHeight),
    IM_COL32(50, 50, 50, 255));

  // Canvas border
  drawList->AddRect(canvasPos,
    ImVec2(canvasPos.x + canvasWidth, canvasPos.y + canvasHeight),
    IM_COL32(100, 100, 100, 255));

  // Centered text
  ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
  ImVec2 textPos = ImVec2(canvasPos.x + (canvasWidth - textSize.x) * 0.5f,
    canvasPos.y + (canvasHeight - textSize.y) * 0.5f);
  drawList->AddText(textPos, color, text.c_str());

  // Advance cursor past canvas
  ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, canvasPos.y + canvasHeight));
}

void CameraFeedDisplay::CalculateDisplaySize(float canvasWidth, float canvasHeight, float& displayWidth, float& displayHeight, float& offsetX, float& offsetY) {
  if (!m_maintainAspectRatio || m_frameWidth == 0 || m_frameHeight == 0) {
    // No aspect ratio constraint or invalid dimensions
    displayWidth = canvasWidth;
    displayHeight = canvasHeight;
    offsetX = 0;
    offsetY = 0;
    return;
  }

  float aspectRatio = static_cast<float>(m_frameWidth) / static_cast<float>(m_frameHeight);

  // Fit by width first
  displayWidth = canvasWidth;
  displayHeight = displayWidth / aspectRatio;

  // If height is too big, fit by height instead
  if (displayHeight > canvasHeight) {
    displayHeight = canvasHeight;
    displayWidth = displayHeight * aspectRatio;
  }

  // Calculate centering offsets
  if (m_centerImage) {
    offsetX = (canvasWidth - displayWidth) * 0.5f;
    offsetY = (canvasHeight - displayHeight) * 0.5f;
  }
  else {
    offsetX = 0;
    offsetY = 0;
  }
}

std::string CameraFeedDisplay::GetStatusText() const {
  if (m_sourceType == SourceType::NONE) {
    return "No Camera Source";
  }

  if (HasValidTexture()) {
    return "Live Feed Active (" + std::to_string(m_frameWidth) + "x" + std::to_string(m_frameHeight) + ")";
  }

  return "Waiting for Camera Feed";
}

bool CameraFeedDisplay::IsReceivingFrames() const {
  if (!HasValidTexture()) {
    return false;
  }

  // Check if we've received a frame recently (within last 5 seconds)
  uint64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count();

  return (currentTime - m_lastFrameTime) < 5000;
}