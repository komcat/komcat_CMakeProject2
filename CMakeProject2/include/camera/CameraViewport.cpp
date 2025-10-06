// CameraViewport.cpp - Reusable camera viewport implementation
#include "CameraViewport.h"
#include "LiveVideoSubscriber.h"
#include "include/camera/CameraManager.h"
#include <imgui.h>
#include <algorithm>
#include <sstream>
#include <thread>  // For std::this_thread::sleep_for

CameraViewport::CameraViewport(const std::string& name, const ViewportConfig& config)
  : m_name(name), m_config(config) {
  m_zoom = m_config.defaultZoom;
  m_viewportSize = m_config.defaultSize;
}

CameraViewport::~CameraViewport() {
  StopFeed();
  CleanupTexture();
}

void CameraViewport::SetCameraManager(CameraManager* manager) {
  if (m_cameraManager != manager) {
    StopFeed();
    m_cameraManager = manager;
  }
}

void CameraViewport::SetCameraId(const std::string& cameraId) {
  if (m_cameraId != cameraId) {
    StopFeed();
    m_cameraId = cameraId;
    m_retryCount = 0;
  }
}

void CameraViewport::StartFeed() {
  if (m_cameraId.empty() || !m_cameraManager) {
    return;
  }

  m_subscriptionActive = true;
  m_shouldRetry = true;
  m_retryCount = 0;
  UpdateSubscription();
}

void CameraViewport::StopFeed() {
  m_subscriptionActive = false;
  m_shouldRetry = false;

  // Unsubscribe from camera manager if we have a subscriber
  if (m_subscriber && m_cameraManager) {
    m_cameraManager->UnsubscribeFromFrames(m_subscriber->GetSubscriberId());
  }

  if (m_subscriber) {
    m_subscriber.reset();
  }

  if (m_isConnected) {
    m_isConnected = false;
    if (m_onConnectionChanged) {
      m_onConnectionChanged(false);
    }
  }
}

void CameraViewport::UpdateSubscription() {
  if (!m_subscriptionActive || !m_cameraManager || m_cameraId.empty()) {
    return;
  }

  try {
    // Create new subscriber for the specific camera
    m_subscriber = std::make_shared<LiveVideoSubscriber>(m_cameraId);

    // Subscribe to camera manager's broadcasting system
    m_cameraManager->SubscribeToFrames(m_subscriber);

    // Give it a moment to receive frames and check connection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Check if camera is connected and we're getting frames
    if (m_subscriber->IsCameraConnected() && m_subscriber->IsCameraGrabbing()) {
      m_isConnected = true;
      m_retryCount = 0;
      if (m_onConnectionChanged) {
        m_onConnectionChanged(true);
      }
    }
    else {
      // Camera not ready yet
      m_isConnected = false;
    }
  }
  catch (const std::exception& ) {
    m_isConnected = false;
    if (m_subscriber && m_cameraManager) {
      m_cameraManager->UnsubscribeFromFrames(m_subscriber->GetSubscriberId());
    }
    m_subscriber.reset();
  }
}



void CameraViewport::HandleRetry() {
  if (!m_shouldRetry || m_subscriptionActive == false) {
    return;
  }

  // Update connection state based on subscriber status
  if (m_subscriber) {
    bool currentlyConnected = m_subscriber->IsCameraConnected() && m_subscriber->IsCameraGrabbing();
    if (currentlyConnected && !m_isConnected) {
      // Just connected
      m_isConnected = true;
      m_shouldRetry = false;
      if (m_onConnectionChanged) {
        m_onConnectionChanged(true);
      }
      return;
    }
    else if (!currentlyConnected && m_isConnected) {
      // Lost connection
      m_isConnected = false;
      m_shouldRetry = true;
      if (m_onConnectionChanged) {
        m_onConnectionChanged(false);
      }
    }
  }

  // If already connected or not retrying, skip retry logic
  if (m_isConnected || !m_shouldRetry) {
    return;
  }

  // Check if enough time has passed for retry
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastRetryTime);

  if (elapsed.count() >= m_config.retryIntervalMs) {
    // Check retry limit
    if (m_config.maxRetryAttempts > 0 && m_retryCount >= m_config.maxRetryAttempts) {
      m_shouldRetry = false;
      return;
    }

    m_retryCount++;
    m_lastRetryTime = now;
    UpdateSubscription();
  }
}



void CameraViewport::Render(const ImVec2& availableSize) {
  // Handle retry logic
  HandleRetry();

  // Calculate viewport size
  ImVec2 targetSize = availableSize;
  if (targetSize.x <= 0 || targetSize.y <= 0) {
    targetSize = m_config.defaultSize;
  }

  // Clamp to min/max size
  targetSize.x = std::clamp(targetSize.x, m_config.minSize.x, m_config.maxSize.x);
  targetSize.y = std::clamp(targetSize.y, m_config.minSize.y, m_config.maxSize.y);
  m_viewportSize = targetSize;

  // Render controls if enabled
  if (m_config.showControls) {
    RenderControls();
    ImGui::Separator();
  }

  // Render status if enabled
  if (m_config.showStatus) {
    RenderStatus();
  }

  // Render main viewport
  RenderViewport(m_viewportSize);
}

void CameraViewport::RenderControls() {
  ImGui::PushID(m_name.c_str());

  // Zoom controls
  ImGui::Text("Zoom:");
  ImGui::SameLine();

  float zoomPercent = m_zoom * 100.0f;
  ImGui::SetNextItemWidth(100);
  if (ImGui::SliderFloat("##zoom", &zoomPercent, m_config.minZoom * 100, m_config.maxZoom * 100, "%.1f%%")) {
    SetZoom(zoomPercent / 100.0f);
  }

  ImGui::SameLine();
  if (ImGui::Button("Reset")) {
    ResetView();
  }

  ImGui::SameLine();
  if (ImGui::Button("Fit")) {
    FitToViewport();
  }

  // Debug toggle
  ImGui::SameLine();
  ImGui::Checkbox("Debug", &m_showDebugInfo);

  ImGui::PopID();
}

void CameraViewport::RenderStatus() {
  std::string status = GetStatusText();

  // Color based on connection state
  ImVec4 color = m_isConnected ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
  ImGui::TextColored(color, "%s", status.c_str());

  if (m_showDebugInfo) {
    ImGui::SameLine();
    ImGui::Text("| Retry: %d | Zoom: %.2f | Pan: (%.1f, %.1f)",
      m_retryCount, m_zoom, m_panOffset.x, m_panOffset.y);
  }
}

void CameraViewport::RenderViewport(const ImVec2& viewportSize) {
  // Create a child window for the viewport
  std::string childName = m_name + "_viewport";
  if (ImGui::BeginChild(childName.c_str(), viewportSize, true)) {

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    // Handle mouse input
    HandleMouseInput(canvasPos, canvasSize);

    // Update texture if we have a subscriber
    if (m_subscriber && m_isConnected) {
      UpdateTexture();
    }

    // Render the image or placeholder
    if (m_textureInitialized && m_isConnected) {
      RenderImage(canvasPos, canvasSize);
    }
    else {
      RenderPlaceholder(canvasPos, canvasSize, GetStatusText());
    }
  }
  ImGui::EndChild();
}

void CameraViewport::RenderImage(const ImVec2& canvasPos, const ImVec2& canvasSize) {
  ImDrawList* drawList = ImGui::GetWindowDrawList();

  // Calculate image display size and position
  ImVec2 imageSize = CalculateImageSize(canvasSize);
  ImVec2 imageOffset = CalculateImageOffset(canvasSize, imageSize);
  ImVec2 imagePos = ImVec2(canvasPos.x + imageOffset.x, canvasPos.y + imageOffset.y);

  // Calculate source rectangle (ROI) - center-based crop
  ViewRect sourceRect = CalculateSourceRect();

  // Convert source rect to UV coordinates
  ImVec2 uv0 = ImVec2(sourceRect.Min_x / m_imageWidth, sourceRect.Min_y / m_imageHeight);
  ImVec2 uv1 = ImVec2(sourceRect.Max_x / m_imageWidth, sourceRect.Max_y / m_imageHeight);

  // Draw the image - simple cast to ImTextureID
  ImTextureID textureId = (ImTextureID)(intptr_t)m_textureID;
  drawList->AddImage(
    textureId,
    imagePos,
    ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y),
    uv0, uv1
  );

  // Draw border
  drawList->AddRect(imagePos, ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y),
    IM_COL32(100, 100, 100, 255));
}


void CameraViewport::RenderPlaceholder(const ImVec2& canvasPos, const ImVec2& canvasSize, const std::string& message) {
  ImDrawList* drawList = ImGui::GetWindowDrawList();

  // Draw background
  drawList->AddRectFilled(canvasPos,
    ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
    IM_COL32(50, 50, 50, 255));

  // Draw border
  drawList->AddRect(canvasPos,
    ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
    IM_COL32(100, 100, 100, 255));

  // Draw message
  ImVec2 textSize = ImGui::CalcTextSize(message.c_str());
  ImVec2 textPos = ImVec2(
    canvasPos.x + (canvasSize.x - textSize.x) * 0.5f,
    canvasPos.y + (canvasSize.y - textSize.y) * 0.5f
  );

  drawList->AddText(textPos, IM_COL32(200, 200, 200, 255), message.c_str());
}

void CameraViewport::HandleMouseInput(const ImVec2& canvasPos, const ImVec2& canvasSize) {
  ImGuiIO& io = ImGui::GetIO();
  ImVec2 mousePos = io.MousePos;

  // Check if mouse is over canvas
  bool isHovered = mousePos.x >= canvasPos.x && mousePos.x <= canvasPos.x + canvasSize.x &&
    mousePos.y >= canvasPos.y && mousePos.y <= canvasPos.y + canvasSize.y;

  if (!isHovered) {
    m_isDragging = false;
    return;
  }

  // Handle zoom with scroll wheel
  if (io.MouseWheel != 0) {
    float zoomFactor = io.MouseWheel > 0 ? 1.1f : 0.9f;
    SetZoom(m_zoom * zoomFactor);
  }

  // Handle panning with mouse drag
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    m_isDragging = true;
    m_dragStart = mousePos;
    m_dragStartPan = m_panOffset;
  }

  if (m_isDragging) {
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      ImVec2 delta = ImVec2(mousePos.x - m_dragStart.x, mousePos.y - m_dragStart.y);
      SetPanOffset(ImVec2(m_dragStartPan.x + delta.x, m_dragStartPan.y + delta.y));
    }
    else {
      m_isDragging = false;
    }
  }
}

void CameraViewport::UpdateTexture() {
  if (!m_subscriber) {
    return;
  }

  // Check if we have a new frame available
  if (!m_subscriber->HasNewFrame()) {
    return;
  }

  // Get the latest frame data (thread-safe copy)
  CameraFrameData frameData = m_subscriber->GetLatestFrame();

  // Mark the frame as consumed
  m_subscriber->MarkFrameConsumed();

  // Validate frame data
  if (!frameData.IsValid() || frameData.imageData.empty()) {
    return;
  }

  uint32_t width = frameData.width;
  uint32_t height = frameData.height;

  if (width == 0 || height == 0) {
    return;
  }

  // Initialize or resize texture if needed
  if (!m_textureInitialized || m_textureWidth != width || m_textureHeight != height) {
    CleanupTexture();

    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    m_textureWidth = width;
    m_textureHeight = height;
    m_imageWidth = width;
    m_imageHeight = height;
    m_textureInitialized = true;
  }

  // Update texture with new frame data
  glBindTexture(GL_TEXTURE_2D, m_textureID);

  // Determine OpenGL format based on channels
  GLenum format = GL_RGB;
  GLenum internalFormat = GL_RGB;

  if (frameData.channels == 1) {
    format = GL_LUMINANCE;
    internalFormat = GL_LUMINANCE;
  }
  else if (frameData.channels == 3) {
    format = GL_RGB;
    internalFormat = GL_RGB;
  }
  else if (frameData.channels == 4) {
    format = GL_RGBA;
    internalFormat = GL_RGBA;
  }

  glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, frameData.imageData.data());
}
// View control methods
void CameraViewport::SetZoom(float zoom) {
  m_zoom = std::clamp(zoom, m_config.minZoom, m_config.maxZoom);
  ClampPanOffset();
}

void CameraViewport::SetPanOffset(ImVec2 offset) {
  m_panOffset = offset;
  ClampPanOffset();
}

void CameraViewport::ResetView() {
  m_zoom = m_config.defaultZoom;
  m_panOffset = ImVec2(0, 0);
}

void CameraViewport::FitToViewport() {
  if (m_imageWidth == 0 || m_imageHeight == 0) {
    return;
  }

  float scaleX = m_viewportSize.x / m_imageWidth;
  float scaleY = m_viewportSize.y / m_imageHeight;
  m_zoom = std::min(scaleX, scaleY);
  ClampZoom();
  m_panOffset = ImVec2(0, 0);
}

// Utility methods
ViewRect CameraViewport::CalculateSourceRect() const {
  if (m_imageWidth == 0 || m_imageHeight == 0) {
    return ViewRect(0, 0, 0, 0);
  }

  // Calculate visible area in image coordinates (center-based)
  float visibleWidth = m_imageWidth / m_zoom;
  float visibleHeight = m_imageHeight / m_zoom;

  // Center point with pan offset
  float centerX = m_imageWidth * 0.5f - m_panOffset.x / m_zoom;
  float centerY = m_imageHeight * 0.5f - m_panOffset.y / m_zoom;

  // Calculate rectangle
  float left = centerX - visibleWidth * 0.5f;
  float top = centerY - visibleHeight * 0.5f;
  float right = centerX + visibleWidth * 0.5f;
  float bottom = centerY + visibleHeight * 0.5f;

  // Clamp to image bounds
  left = std::clamp(left, 0.0f, (float)m_imageWidth);
  top = std::clamp(top, 0.0f, (float)m_imageHeight);
  right = std::clamp(right, 0.0f, (float)m_imageWidth);
  bottom = std::clamp(bottom, 0.0f, (float)m_imageHeight);

  return ViewRect(left, top, right, bottom);
}

ImVec2 CameraViewport::CalculateImageSize(const ImVec2& canvasSize) const {
  if (m_imageWidth == 0 || m_imageHeight == 0) {
    return canvasSize;
  }

  // Calculate aspect-ratio preserving size
  float imageAspect = (float)m_imageWidth / (float)m_imageHeight;
  float canvasAspect = canvasSize.x / canvasSize.y;

  ImVec2 imageSize;
  if (imageAspect > canvasAspect) {
    // Image is wider than canvas
    imageSize.x = canvasSize.x;
    imageSize.y = canvasSize.x / imageAspect;
  }
  else {
    // Image is taller than canvas
    imageSize.x = canvasSize.y * imageAspect;
    imageSize.y = canvasSize.y;
  }

  return imageSize;
}

ImVec2 CameraViewport::CalculateImageOffset(const ImVec2& canvasSize, const ImVec2& imageSize) const {
  // Center the image in the canvas
  return ImVec2(
    (canvasSize.x - imageSize.x) * 0.5f,
    (canvasSize.y - imageSize.y) * 0.5f
  );
}

void CameraViewport::ClampZoom() {
  m_zoom = std::clamp(m_zoom, m_config.minZoom, m_config.maxZoom);
}

void CameraViewport::ClampPanOffset() {
  // Limit panning based on zoom level to prevent showing empty areas
  if (m_zoom <= 1.0f) {
    m_panOffset = ImVec2(0, 0);
    return;
  }

  float maxPanX = (m_imageWidth * (m_zoom - 1.0f)) * 0.5f;
  float maxPanY = (m_imageHeight * (m_zoom - 1.0f)) * 0.5f;

  m_panOffset.x = std::clamp(m_panOffset.x, -maxPanX, maxPanX);
  m_panOffset.y = std::clamp(m_panOffset.y, -maxPanY, maxPanY);
}

std::string CameraViewport::GetStatusText() const {
  if (m_cameraId.empty()) {
    return "No camera selected";
  }

  if (!m_cameraManager) {
    return "Camera manager not available";
  }

  if (!m_subscriber) {
    if (m_shouldRetry) {
      return "Initializing... (attempt " + std::to_string(m_retryCount) + ")";
    }
    return "Not subscribed";
  }

  // Get detailed status from subscriber
  bool connected = m_subscriber->IsCameraConnected();
  bool grabbing = m_subscriber->IsCameraGrabbing();
  uint64_t frameCount = m_subscriber->GetTotalFramesReceived();

  if (connected && grabbing && frameCount > 0) {
    return "Live: " + m_cameraId + " (" + std::to_string(frameCount) + " frames)";
  }
  else if (connected && grabbing) {
    return "Connected: " + m_cameraId + " (waiting for frames...)";
  }
  else if (connected) {
    return "Connected: " + m_cameraId + " (not grabbing)";
  }
  else {
    if (m_shouldRetry) {
      return "Connecting to " + m_cameraId + "... (attempt " + std::to_string(m_retryCount) + ")";
    }
    return "Disconnected: " + m_cameraId;
  }
}

void CameraViewport::CleanupTexture() {
  if (m_textureInitialized) {
    glDeleteTextures(1, &m_textureID);
    m_textureInitialized = false;
    m_textureID = 0;
  }
}