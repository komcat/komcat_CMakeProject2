// CameraViewport.h - Reusable camera viewport with zoom, resize, and auto-retry
#pragma once

#include <imgui.h>  // For ImVec2, etc.
#include <memory>
#include <string>
#include <chrono>
#include <functional>
#include <SDL_opengl.h>

// Forward declarations
class CameraManager;
class LiveVideoSubscriber;

// Simple rectangle struct since ImRect is not available
struct ViewRect {
  float Min_x, Min_y, Max_x, Max_y;
  ViewRect() : Min_x(0), Min_y(0), Max_x(0), Max_y(0) {}
  ViewRect(float x1, float y1, float x2, float y2) : Min_x(x1), Min_y(y1), Max_x(x2), Max_y(y2) {}
  ImVec2 Min() const { return ImVec2(Min_x, Min_y); }
  ImVec2 Max() const { return ImVec2(Max_x, Max_y); }
};

class CameraViewport {
public:
  struct ViewportConfig {
    ImVec2 minSize = ImVec2(160, 120);     // Minimum viewport size
    ImVec2 maxSize = ImVec2(1920, 1080);   // Maximum viewport size
    ImVec2 defaultSize = ImVec2(640, 480); // Default viewport size
    float minZoom = 0.1f;                  // 10% zoom out
    float maxZoom = 10.0f;                 // 1000% zoom in
    float defaultZoom = 1.0f;              // 100% (no zoom)
    bool showControls = true;              // Show zoom/pan controls
    bool showStatus = true;                // Show connection status
    int retryIntervalMs = 2000;            // Retry connection every 2 seconds
    int maxRetryAttempts = -1;             // -1 = infinite retries
  };

  CameraViewport(const std::string& name, const ViewportConfig& config = ViewportConfig{});
  ~CameraViewport();

  // Camera management
  void SetCameraManager(CameraManager* manager);
  void SetCameraId(const std::string& cameraId);
  void StartFeed();
  void StopFeed();
  bool IsConnected() const { return m_isConnected; }

  // Render the viewport (call this in your UI)
  void Render(const ImVec2& availableSize = ImVec2(0, 0));

  // View controls
  void SetZoom(float zoom);
  void SetPanOffset(ImVec2 offset);
  void ResetView();
  void FitToViewport();

  // Configuration
  void SetConfig(const ViewportConfig& config) { m_config = config; }
  ViewportConfig& GetConfig() { return m_config; }

  // Callbacks for external handling
  void SetOnConnectionChanged(std::function<void(bool connected)> callback) {
    m_onConnectionChanged = callback;
  }

private:
  // Core members
  std::string m_name;
  std::string m_cameraId;
  CameraManager* m_cameraManager = nullptr;
  ViewportConfig m_config;

  // Subscription management
  std::shared_ptr<LiveVideoSubscriber> m_subscriber;
  bool m_isConnected = false;
  bool m_subscriptionActive = false;

  // Retry logic
  std::chrono::steady_clock::time_point m_lastRetryTime;
  int m_retryCount = 0;
  bool m_shouldRetry = false;

  // OpenGL texture management
  unsigned int m_textureID = 0;
  bool m_textureInitialized = false;
  uint32_t m_textureWidth = 0;
  uint32_t m_textureHeight = 0;
  uint32_t m_imageWidth = 0;
  uint32_t m_imageHeight = 0;

  // View state
  float m_zoom = 1.0f;
  ImVec2 m_panOffset = ImVec2(0, 0);
  ImVec2 m_viewportSize = ImVec2(640, 480);
  bool m_isDragging = false;
  ImVec2 m_dragStart = ImVec2(0, 0);
  ImVec2 m_dragStartPan = ImVec2(0, 0);

  // UI state
  bool m_showDebugInfo = false;

  // Callbacks
  std::function<void(bool)> m_onConnectionChanged;

  // Internal methods
  void UpdateSubscription();
  void HandleRetry();
  void UpdateTexture();
  void CleanupTexture();

  // Rendering methods
  void RenderViewport(const ImVec2& viewportSize);
  void RenderControls();
  void RenderStatus();
  void RenderImage(const ImVec2& canvasPos, const ImVec2& canvasSize);
  void RenderPlaceholder(const ImVec2& canvasPos, const ImVec2& canvasSize, const std::string& message);

  // View calculations
  ImVec2 CalculateImageSize(const ImVec2& canvasSize) const;
  ImVec2 CalculateImageOffset(const ImVec2& canvasSize, const ImVec2& imageSize) const;
  ViewRect CalculateSourceRect() const; // ROI crop rectangle

  // Input handling
  void HandleMouseInput(const ImVec2& canvasPos, const ImVec2& canvasSize);

  // Utility
  void ClampZoom();
  void ClampPanOffset();
  std::string GetStatusText() const;
};