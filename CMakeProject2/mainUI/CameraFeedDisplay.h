#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <mutex>
#include <vector>

// Forward declarations
class PylonCameraTest;

// Generic camera feed display class that can work with any camera type
class CameraFeedDisplay {
public:
  CameraFeedDisplay();
  ~CameraFeedDisplay();

  // Disable copy/move to avoid texture issues
  CameraFeedDisplay(const CameraFeedDisplay&) = delete;
  CameraFeedDisplay& operator=(const CameraFeedDisplay&) = delete;
  CameraFeedDisplay(CameraFeedDisplay&&) = delete;
  CameraFeedDisplay& operator=(CameraFeedDisplay&&) = delete;

  // Camera source management
  void SetPylonCameraSource(PylonCameraTest* camera);
  void ClearSource();
  bool HasSource() const { return m_sourceType != SourceType::NONE; }

  // Texture update and rendering
  bool UpdateTexture();
  void RenderToCanvas(float canvasWidth, float canvasHeight);

  // Texture info
  bool HasValidTexture() const { return m_textureInitialized && m_hasValidFrame; }
  unsigned int GetTextureID() const { return m_textureID; }
  uint32_t GetTextureWidth() const { return m_frameWidth; }
  uint32_t GetTextureHeight() const { return m_frameHeight; }

  // Display settings
  void SetMaintainAspectRatio(bool maintain) { m_maintainAspectRatio = maintain; }
  void SetCenterImage(bool center) { m_centerImage = center; }
  void SetPlaceholderText(const std::string& text) { m_placeholderText = text; }

  // Status
  std::string GetStatusText() const;
  bool IsReceivingFrames() const;

private:
  enum class SourceType {
    NONE,
    PYLON_CAMERA_TEST
    // Future: FLIR_CAMERA, USB_CAMERA, etc.
  };

  // Source management
  SourceType m_sourceType = SourceType::NONE;
  PylonCameraTest* m_pylonCamera = nullptr;

  // OpenGL texture
  unsigned int m_textureID = 0;
  bool m_textureInitialized = false;
  bool m_hasValidFrame = false;
  uint32_t m_frameWidth = 0;
  uint32_t m_frameHeight = 0;

  // **REMOVED PROBLEMATIC MEMBERS:**
  // - m_imageBuffer
  // - m_bufferMutex  
  // - m_bufferValid
  // - m_bufferWidth
  // - m_bufferHeight
  // - m_safeImageData (this was causing the access violation)

  // Display settings
  bool m_maintainAspectRatio = true;
  bool m_centerImage = true;
  std::string m_placeholderText = "No Camera Feed";

  // Statistics
  uint64_t m_lastFrameTime = 0;
  uint32_t m_frameCount = 0;

  // Internal methods
  bool InitializeTexture();
  void CleanupTexture();
  bool UpdateFromPylonCamera();
  void RenderPlaceholder(float canvasWidth, float canvasHeight, const std::string& text, uint32_t color);
  void CalculateDisplaySize(float canvasWidth, float canvasHeight, float& displayWidth, float& displayHeight, float& offsetX, float& offsetY);
};