#pragma once

#include "ids_camera_test.h"
#include "imgui.h"
#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <chrono>

class IDSCameraUI {
public:
  IDSCameraUI();
  ~IDSCameraUI();

  // Main render function
  void RenderUI();
  void Render() { RenderUI(); }  // Alias for compatibility

  // Get camera instance for external access
  IDSCameraTest* GetCamera() { return m_camera.get(); }

  // Toggle window visibility
  void ToggleWindow() { m_isVisible = !m_isVisible; }
  void ToggleVisibility() { m_isVisible = !m_isVisible; }  // Alias for compatibility
  bool IsVisible() const { return m_isVisible; }

private:
  // Camera instance
  std::unique_ptr<IDSCameraTest> m_camera;

  // UI state
  bool m_isVisible;
  int m_selectedCameraIdIndex;
  std::vector<int> m_availableCameraIds;
  std::vector<std::string> m_statusMessages;

  // Image display with frame buffering
  unsigned int m_textureID;
  bool m_textureInitialized;
  unsigned int m_imageTextureWidth;
  unsigned int m_imageTextureHeight;

  // Frame buffering for smooth display
  struct FrameBuffer {
    std::vector<uint8_t> data;
    uint32_t width;
    uint32_t height;
    bool isValid;
    std::chrono::steady_clock::time_point timestamp;

    FrameBuffer() : width(0), height(0), isValid(false) {}
  };

  FrameBuffer m_bufferedFrame;
  mutable std::mutex m_frameBufferMutex;
  std::chrono::steady_clock::time_point m_lastFrameUpdate;

  // Capture settings
  char m_saveFilename[256];
  bool m_autoSaveWithTimestamp;

  // Status log settings
  static const size_t MAX_STATUS_MESSAGES = 100;
  bool m_autoScrollStatusLog;

  // UI rendering methods
  void RenderCameraSelection();
  void RenderConnectionControls();
  void RenderCameraInfo();
  void RenderImageDisplay();
  void RenderCaptureControls();
  void RenderLiveControls();
  void RenderStatusLog();
  void RenderErrorCanvas(float width, float height, const std::string& errorText);

  // Helper methods
  void RefreshCameraList();
  void ConnectBySelectedId();
  void UpdateImageTexture();
  void UpdateFrameBuffer();
  void AddStatusMessage(const std::string& message);
  void CleanupTexture();

  // Status callback for camera
  void OnCameraStatus(const std::string& message);
};