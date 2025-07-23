// CameraFeedDisplay.cpp - Updated with broadcasting system support
#include "CameraFeedDisplay.h"
#include "include/camera/pylon_camera_test.h"
#include "include/logger.h"

#ifdef _WIN32
#include <Windows.h>
#include <GL/gl.h>
#ifndef GL_RGB8
#define GL_RGB8 0x8051
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif
#ifndef GL_R8
#define GL_R8 0x8229
#endif
#ifndef GL_RED
#define GL_RED 0x1903
#endif
#elif defined(__linux__)
#include <GL/gl.h>
#ifndef GL_RGB8
#define GL_RGB8 0x8051
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif
#ifndef GL_R8
#define GL_R8 0x8229
#endif
#ifndef GL_RED
#define GL_RED 0x1903
#endif
#elif defined(__APPLE__)
#include <OpenGL/gl.h>
#ifndef GL_RGB8
#define GL_RGB8 0x8051
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif
#ifndef GL_R8
#define GL_R8 0x8229
#endif
#ifndef GL_RED
#define GL_RED 0x1903
#endif
#endif

#include <iostream>
#include <sstream>
#include <iomanip>

CameraFeedDisplay::CameraFeedDisplay()
  : m_camera(nullptr)
  , m_subscriberId("CameraFeedDisplay_1")
  , m_targetCameraId("")
  , m_isSubscriberMode(false)
  , m_hasNewFrame(false)
  , m_lastFrameTimestamp(0)
  , m_totalFramesReceived(0)
  , m_minFrameIntervalMs(33)  // ~30fps default
  , m_textureID(0)
  , m_textureWidth(0)
  , m_textureHeight(0)
  , m_textureValid(false)
  , m_actualFrameRate(0.0f)
  , m_frameCounter(0)
  , m_cameraConnected(false)
  , m_cameraGrabbing(false)
  , m_placeholderText("No Camera Feed") {

  m_lastFrameTime = std::chrono::steady_clock::now();
  m_lastUpdateTime = std::chrono::steady_clock::now();

  Logger* logger = Logger::GetInstance();
  if (logger) {
    logger->LogInfo("CameraFeedDisplay created with ID: " + m_subscriberId);
  }
}

CameraFeedDisplay::~CameraFeedDisplay() {
  CleanupTexture();

  Logger* logger = Logger::GetInstance();
  if (logger) {
    logger->LogInfo("CameraFeedDisplay destroyed: " + m_subscriberId);
  }
}

// **NEW: CameraFrameSubscriber interface implementation**

void CameraFeedDisplay::OnNewFrame(const CameraFrameData& frameData) {
  // Frame rate limiting check
  if (IsFrameRateLimited()) {
    return;
  }

  // Update statistics
  m_totalFramesReceived.fetch_add(1);
  m_lastFrameTimestamp.store(frameData.timestamp);
  m_lastFrameTime = std::chrono::steady_clock::now();

  // Store frame data thread-safely
  SetLatestFrameThreadSafe(frameData);
  m_hasNewFrame.store(true);

  // Update frame rate statistics
  UpdateFrameRateStatistics();

  // Debug logging (reduced frequency)
  static int frameCount = 0;
  frameCount++;
  if (frameCount % 300 == 1) {  // Log every 10 seconds at 30fps
    Logger* logger = Logger::GetInstance();
    if (logger) {
      logger->LogInfo("CameraFeedDisplay: Received frame #" + std::to_string(frameCount) +
        " from " + frameData.cameraId + " (" +
        std::to_string(frameData.width) + "x" + std::to_string(frameData.height) + ")");
    }
  }
}

void CameraFeedDisplay::OnCameraStatusChanged(const std::string& cameraId, bool connected, bool grabbing) {
  if (cameraId == m_targetCameraId || m_targetCameraId.empty()) {
    m_cameraConnected.store(connected);
    m_cameraGrabbing.store(grabbing);

    Logger* logger = Logger::GetInstance();
    if (logger) {
      logger->LogInfo("CameraFeedDisplay: Camera " + cameraId + " status - Connected: " +
        (connected ? "Yes" : "No") + ", Grabbing: " + (grabbing ? "Yes" : "No"));
    }

    // If camera disconnected or stopped grabbing, clear the texture
    if (!connected || !grabbing) {
      m_hasNewFrame.store(false);
    }
  }
}

std::string CameraFeedDisplay::GetSubscriberId() const {
  return m_subscriberId;
}

bool CameraFeedDisplay::WantsFramesFromCamera(const std::string& cameraId) const {
  // If no target camera specified, accept frames from any camera
  if (m_targetCameraId.empty()) {
    return true;
  }

  // Otherwise, only accept frames from the target camera
  return (cameraId == m_targetCameraId);
}

// **ENHANCED: Source management methods**

void CameraFeedDisplay::SetPylonCameraSource(PylonCameraTest* camera) {
  m_camera = camera;
  m_isSubscriberMode.store(false);  // Switch to legacy mode

  if (camera) {
    m_targetCameraId = "";  // Clear target for legacy mode
    Logger* logger = Logger::GetInstance();
    if (logger) {
      logger->LogInfo("CameraFeedDisplay: Set to legacy camera source mode");
    }
  }
}

void CameraFeedDisplay::SetTargetCamera(const std::string& cameraId) {
  m_targetCameraId = cameraId;
  m_camera = nullptr;  // Clear legacy camera
  m_isSubscriberMode.store(true);  // Switch to subscriber mode

  // Reset statistics
  ResetStatistics();

  Logger* logger = Logger::GetInstance();
  if (logger) {
    logger->LogInfo("CameraFeedDisplay: Set to subscriber mode for camera: " + cameraId);
  }
}

void CameraFeedDisplay::ClearSource() {
  m_camera = nullptr;
  m_targetCameraId.clear();
  m_isSubscriberMode.store(false);
  m_hasNewFrame.store(false);
  CleanupTexture();

  Logger* logger = Logger::GetInstance();
  if (logger) {
    logger->LogInfo("CameraFeedDisplay: Cleared all sources");
  }
}

bool CameraFeedDisplay::HasSource() const {
  return (m_camera != nullptr) || (!m_targetCameraId.empty() && m_isSubscriberMode.load());
}

// **ENHANCED: Texture management methods**
bool CameraFeedDisplay::UpdateTexture() {
  // **SUBSCRIBER MODE: Use broadcast frames**
  if (m_isSubscriberMode.load() && m_hasNewFrame.load()) {
    CameraFrameData frameData = GetLatestFrameThreadSafe();
    if (frameData.IsValid()) {
      // **NEW APPROACH: Create texture using raylib-compatible method**

      // Clean up old texture first
      if (m_textureID != 0 && m_textureID != 999) {
        glDeleteTextures(1, &m_textureID);
        m_textureID = 0;
      }

      // Store frame dimensions
      m_textureWidth = frameData.width;
      m_textureHeight = frameData.height;

      // **RAYLIB-COMPATIBLE: Create texture using basic OpenGL calls**
      glGenTextures(1, &m_textureID);
      if (m_textureID != 0) {
        glBindTexture(GL_TEXTURE_2D, m_textureID);

        // Use the most basic texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        // **SIMPLEST APPROACH: Try BGR format which cameras often use**
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
          frameData.width, frameData.height, 0,
          GL_BGR_EXT, GL_UNSIGNED_BYTE, frameData.imageData.data());

        // Check for errors
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
          // **FALLBACK: Try standard RGB if BGR fails**
          glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
            frameData.width, frameData.height, 0,
            GL_RGB, GL_UNSIGNED_BYTE, frameData.imageData.data());

          error = glGetError();
          if (error == GL_NO_ERROR) {
            Logger* logger = Logger::GetInstance();
            if (logger) {
              logger->LogInfo("CameraFeedDisplay: Texture created with RGB format");
            }
          }
        }

        glBindTexture(GL_TEXTURE_2D, 0);

        if (error == GL_NO_ERROR) {
          m_textureValid.store(true);
          m_hasNewFrame.store(false);  // Mark frame as consumed
          return true;
        }
      }
    }
  }

  // **LEGACY MODE: unchanged**
  else if (m_camera && !m_isSubscriberMode.load()) {
    auto& camera = m_camera->GetCamera();
    if (camera.IsConnected() && camera.IsGrabbing() && m_camera->HasValidTexture()) {
      unsigned int cameraTextureID = m_camera->GetTextureID();
      if (cameraTextureID != 0 && cameraTextureID != m_textureID) {
        m_textureID = cameraTextureID;
        m_textureWidth = 640;
        m_textureHeight = 480;
        m_textureValid.store(true);
        return true;
      }
    }
  }

  return false;
}


bool CameraFeedDisplay::CreateOrUpdateTexture(const CameraFrameData& frameData) {
  if (!frameData.IsValid() || frameData.imageData.empty()) {
    return false;
  }

  // **SIMPLE APPROACH: Skip texture creation, just mark as valid**
  // Let the rendering system handle texture creation using the frame data directly

  m_textureWidth = frameData.width;
  m_textureHeight = frameData.height;

  // Store a dummy texture ID to indicate we have valid data
  if (m_textureID == 0) {
    m_textureID = 999;  // Dummy ID to indicate we have frame data
  }

  m_textureValid.store(true);

  Logger* logger = Logger::GetInstance();
  static int successCount = 0;
  successCount++;
  if (successCount % 300 == 1) {
    if (logger) {
      logger->LogInfo("CameraFeedDisplay: Frame data stored successfully #" + std::to_string(successCount) +
        " (" + std::to_string(frameData.width) + "x" + std::to_string(frameData.height) +
        ", " + std::to_string(frameData.channels) + " channels)");
    }
  }

  return true;
}


bool CameraFeedDisplay::HasValidTexture() const {
  return m_textureValid.load() && (m_textureID != 0);
}

unsigned int CameraFeedDisplay::GetTextureID() const {
  return m_textureID;
}

uint32_t CameraFeedDisplay::GetTextureWidth() const {
  return m_textureWidth;
}

uint32_t CameraFeedDisplay::GetTextureHeight() const {
  return m_textureHeight;
}

// **NEW: Frame reception status methods**

bool CameraFeedDisplay::IsReceivingFrames() const {
  auto now = std::chrono::steady_clock::now();
  auto timeSinceLastFrame = std::chrono::duration_cast<std::chrono::milliseconds>(
    now - m_lastFrameTime).count();

  // Consider "receiving" if we got a frame within the last 200ms
  return (timeSinceLastFrame < 200) && (m_totalFramesReceived.load() > 0);
}

uint64_t CameraFeedDisplay::GetLastFrameTime() const {
  return m_lastFrameTimestamp.load();
}

uint64_t CameraFeedDisplay::GetTotalFramesReceived() const {
  return m_totalFramesReceived.load();
}

std::string CameraFeedDisplay::GetStatusText() const {
  std::ostringstream oss;

  if (m_isSubscriberMode.load()) {
    // Subscriber mode status
    oss << "Subscriber Mode";
    if (!m_targetCameraId.empty()) {
      oss << " (" << m_targetCameraId << ")";
    }

    if (IsReceivingFrames()) {
      oss << " - LIVE " << std::fixed << std::setprecision(1) << GetActualFrameRate() << " fps";
    }
    else if (m_cameraConnected.load()) {
      if (m_cameraGrabbing.load()) {
        oss << " - Connected, No Frames";
      }
      else {
        oss << " - Connected, Not Grabbing";
      }
    }
    else {
      oss << " - Disconnected";
    }
  }
  else if (m_camera) {
    // Legacy mode status
    oss << "Legacy Mode";
    auto& camera = m_camera->GetCamera();
    if (camera.IsConnected()) {
      if (camera.IsGrabbing()) {
        oss << " - Grabbing";
        if (HasValidTexture()) {
          oss << " " << std::fixed << std::setprecision(1) << GetActualFrameRate() << " fps";
        }
      }
      else {
        oss << " - Not Grabbing";
      }
    }
    else {
      oss << " - Disconnected";
    }
  }
  else {
    oss << "No Source";
  }

  return oss.str();
}

// **NEW: Performance and debugging methods**

void CameraFeedDisplay::SetFrameRateLimit(float fps) {
  if (fps > 0) {
    m_minFrameIntervalMs.store(static_cast<int>(1000.0f / fps));
  }
  else {
    m_minFrameIntervalMs.store(0);  // No limit
  }
}

float CameraFeedDisplay::GetActualFrameRate() const {
  return m_actualFrameRate.load();
}

void CameraFeedDisplay::ResetStatistics() {
  m_totalFramesReceived.store(0);
  m_frameCounter.store(0);
  m_actualFrameRate.store(0.0f);
  m_lastFrameTimestamp.store(0);
  m_lastUpdateTime = std::chrono::steady_clock::now();
}

// **ENHANCED: Internal methods**

void CameraFeedDisplay::UpdateFrameRateStatistics() {
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastUpdateTime).count();

  m_frameCounter.fetch_add(1);

  // Update frame rate every second
  if (elapsed >= 1000) {
    float fps = (m_frameCounter.load() * 1000.0f) / elapsed;
    m_actualFrameRate.store(fps);
    m_frameCounter.store(0);
    m_lastUpdateTime = now;
  }
}

void CameraFeedDisplay::CleanupTexture() {
  if (m_textureID != 0) {
    glDeleteTextures(1, &m_textureID);
    m_textureID = 0;
  }
  m_textureWidth = 0;
  m_textureHeight = 0;
  m_textureValid.store(false);
}

bool CameraFeedDisplay::IsFrameRateLimited() const {
  int intervalMs = m_minFrameIntervalMs.load();
  if (intervalMs <= 0) {
    return false;  // No rate limiting
  }

  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastFrameTime).count();

  return (elapsed < intervalMs);
}

// **NEW: Thread-safe frame data methods**

CameraFrameData CameraFeedDisplay::GetLatestFrameThreadSafe() const {
  std::lock_guard<std::mutex> lock(m_frameMutex);
  return m_latestFrame;  // This copies the frame data
}

void CameraFeedDisplay::SetLatestFrameThreadSafe(const CameraFrameData& frame) {
  std::lock_guard<std::mutex> lock(m_frameMutex);
  m_latestFrame = frame;  // This copies the frame data
}

// **BACKWARD COMPATIBILITY: Legacy rendering methods**

void CameraFeedDisplay::RenderToCanvas() {
  // This method provides backward compatibility for UIConfigVisualizer
  // It renders the camera feed using ImGui with automatic sizing

  if (!HasValidTexture()) {
    // Show placeholder when no texture available
    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    float displayWidth = (std::min)(availableSize.x, 400.0f);
    float displayHeight = displayWidth * 0.75f; // 4:3 aspect ratio

    ImGui::Button(m_placeholderText.c_str(), ImVec2(displayWidth, displayHeight));
    ImGui::Text("Status: %s", GetStatusText().c_str());
    return;
  }

  // Calculate display size
  ImVec2 availableSize = ImGui::GetContentRegionAvail();
  float maxWidth = (std::min)(availableSize.x, 600.0f);
  float maxHeight = (std::min)(availableSize.y - 100.0f, 450.0f); // Leave space for controls

  // Calculate aspect ratio preserving size
  float textureAspect = (float)m_textureWidth / (float)m_textureHeight;
  float containerAspect = maxWidth / maxHeight;

  float displayWidth, displayHeight;
  if (textureAspect > containerAspect) {
    displayWidth = maxWidth;
    displayHeight = maxWidth / textureAspect;
  }
  else {
    displayHeight = maxHeight;
    displayWidth = maxHeight * textureAspect;
  }

  // Render the camera texture using ImGui
  ImTextureID textureId = (ImTextureID)(intptr_t)m_textureID;
  ImGui::Image(textureId, ImVec2(displayWidth, displayHeight));

  // Add status info below the image
  ImGui::Text("Camera Feed: %dx%d", m_textureWidth, m_textureHeight);
  ImGui::Text("Status: %s", GetStatusText().c_str());
  ImGui::Text("Frame Rate: %.1f fps", GetActualFrameRate());
  ImGui::Text("Total Frames: %llu", GetTotalFramesReceived());

  // Add hover tooltip with more details
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("Texture ID: %u", m_textureID);
    ImGui::Text("Subscriber ID: %s", m_subscriberId.c_str());
    if (!m_targetCameraId.empty()) {
      ImGui::Text("Target Camera: %s", m_targetCameraId.c_str());
    }
    ImGui::Text("Mode: %s", m_isSubscriberMode.load() ? "Subscriber" : "Legacy");
    ImGui::EndTooltip();
  }
}

void CameraFeedDisplay::RenderToCanvas(int width, int height) {
  // Overload that takes specific width and height parameters
  // This matches your existing code usage

  if (!HasValidTexture()) {
    // Show placeholder when no texture available
    ImGui::Button(m_placeholderText.c_str(), ImVec2((float)width, (float)height));

    // Add status below if there's space
    if (ImGui::GetContentRegionAvail().y > 60) {
      ImGui::Text("Status: %s", GetStatusText().c_str());
    }
    return;
  }

  // Render the camera texture at specified size
  ImTextureID textureId = (ImTextureID)(intptr_t)m_textureID;
  ImGui::Image(textureId, ImVec2((float)width, (float)height));

  // Add hover tooltip with details
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("Camera Feed: %dx%d", m_textureWidth, m_textureHeight);
    ImGui::Text("Frame Rate: %.1f fps", GetActualFrameRate());
    ImGui::Text("Total Frames: %llu", GetTotalFramesReceived());
    ImGui::Text("Status: %s", GetStatusText().c_str());
    ImGui::EndTooltip();
  }

  // Add compact status below if there's space
  if (ImGui::GetContentRegionAvail().y > 40) {
    ImGui::Text("Live: %.1f fps", GetActualFrameRate());
  }
}

void CameraFeedDisplay::RenderPreview(int width, int height) {
  // Legacy preview method for backward compatibility

  if (!HasValidTexture()) {
    ImGui::Button(m_placeholderText.c_str(), ImVec2((float)width, (float)height));
    return;
  }

  // Render at specified size
  ImTextureID textureId = (ImTextureID)(intptr_t)m_textureID;
  ImGui::Image(textureId, ImVec2((float)width, (float)height));

  // Add basic status
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("Camera Preview");
    ImGui::Text("Size: %dx%d", m_textureWidth, m_textureHeight);
    ImGui::Text("FPS: %.1f", GetActualFrameRate());
    ImGui::EndTooltip();
  }
}

void CameraFeedDisplay::SetPlaceholderText(const std::string& text) {
  m_placeholderText = text;
}