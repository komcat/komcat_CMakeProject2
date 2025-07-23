// CameraFeedDisplay.h - Updated to support broadcasting system
#pragma once

#include "include/camera/CameraFrameData.h"  // For CameraFrameSubscriber interface
#include <memory>
#include <string>
#include <mutex>
#include <atomic>
#include <chrono>

// Forward declarations
class PylonCameraTest;

class CameraFeedDisplay : public CameraFrameSubscriber {
public:
  CameraFeedDisplay();
  virtual ~CameraFeedDisplay();

  // **NEW: CameraFrameSubscriber interface implementation**
  void OnNewFrame(const CameraFrameData& frameData) override;
  void OnCameraStatusChanged(const std::string& cameraId, bool connected, bool grabbing) override;
  std::string GetSubscriberId() const override;
  bool WantsFramesFromCamera(const std::string& cameraId) const override;
  int GetMinFrameIntervalMs() const override { return 33; } // ~30fps

  // **ENHANCED: Source management methods**
  void SetPylonCameraSource(PylonCameraTest* camera);
  void SetTargetCamera(const std::string& cameraId);  // NEW: For broadcast system
  void ClearSource();
  bool HasSource() const;

  // **ENHANCED: Texture management methods**
  bool UpdateTexture();
  bool HasValidTexture() const;
  unsigned int GetTextureID() const;
  uint32_t GetTextureWidth() const;
  uint32_t GetTextureHeight() const;

  // **NEW: Frame reception status methods**
  bool IsReceivingFrames() const;
  uint64_t GetLastFrameTime() const;
  uint64_t GetTotalFramesReceived() const;
  std::string GetStatusText() const;

  // **NEW: Performance and debugging methods**
  void SetFrameRateLimit(float fps);
  float GetActualFrameRate() const;
  void ResetStatistics();

  // **BACKWARD COMPATIBILITY: Legacy rendering methods**
  void RenderToCanvas();  // For UIConfigVisualizer compatibility
  void RenderToCanvas(int width, int height);  // Overload with size parameters
  void RenderPreview(int width = 320, int height = 240);  // Legacy preview method
  void SetPlaceholderText(const std::string& text);  // Set placeholder text when no feed

private:
  // **LEGACY: Direct camera source (for backward compatibility)**
  PylonCameraTest* m_camera;

  // **NEW: Broadcasting system members**
  std::string m_subscriberId;
  std::string m_targetCameraId;
  std::atomic<bool> m_isSubscriberMode;

  // **NEW: Frame data management (thread-safe)**
  mutable std::mutex m_frameMutex;
  CameraFrameData m_latestFrame;
  std::atomic<bool> m_hasNewFrame;
  std::atomic<uint64_t> m_lastFrameTimestamp;
  std::atomic<uint64_t> m_totalFramesReceived;

  // **NEW: Frame rate limiting**
  std::chrono::steady_clock::time_point m_lastFrameTime;
  std::atomic<int> m_minFrameIntervalMs;

  // **ENHANCED: OpenGL texture management**
  unsigned int m_textureID;
  uint32_t m_textureWidth;
  uint32_t m_textureHeight;
  std::atomic<bool> m_textureValid;

  // **NEW: Performance tracking**
  std::chrono::steady_clock::time_point m_lastUpdateTime;
  std::atomic<float> m_actualFrameRate;
  std::atomic<int> m_frameCounter;

  // **NEW: Status tracking**
  std::atomic<bool> m_cameraConnected;
  std::atomic<bool> m_cameraGrabbing;

  // **NEW: Placeholder text for UI display**
  std::string m_placeholderText;

  // **ENHANCED: Internal methods**
  bool CreateOrUpdateTexture(const CameraFrameData& frameData);
  void UpdateFrameRateStatistics();
  void CleanupTexture();
  bool IsFrameRateLimited() const;

  // **NEW: Thread-safe getters for internal use**
  CameraFrameData GetLatestFrameThreadSafe() const;
  void SetLatestFrameThreadSafe(const CameraFrameData& frame);
};