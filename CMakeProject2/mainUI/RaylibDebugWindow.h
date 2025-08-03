// RaylibDebugWindow.h - Enhanced with ICameraHardware and Broadcasting Support
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include "include/camera/CameraFrameData.h"
#include "include/camera/ICameraHardware.h"

// Forward declarations
class CameraManager;
class RaylibWindow;
class CameraFeedDisplay;
class Logger;
class ICameraHardware;

/**
 * @brief Enhanced debug subscriber for Raylib window debugging
 *
 * This subscriber receives frames via broadcasting for debugging purposes
 * and provides detailed diagnostics and monitoring capabilities.
 */
class RaylibDebugSubscriber : public CameraFrameSubscriber {
public:
  explicit RaylibDebugSubscriber(const std::string& cameraId);
  ~RaylibDebugSubscriber();

  // Disable copy/move
  RaylibDebugSubscriber(const RaylibDebugSubscriber&) = delete;
  RaylibDebugSubscriber& operator=(const RaylibDebugSubscriber&) = delete;
  RaylibDebugSubscriber(RaylibDebugSubscriber&&) = delete;
  RaylibDebugSubscriber& operator=(RaylibDebugSubscriber&&) = delete;

  // CameraFrameSubscriber interface
  void OnNewFrame(const CameraFrameData& frameData) override;
  void OnCameraStatusChanged(const std::string& cameraId, bool connected, bool grabbing) override;
  std::string GetSubscriberId() const override;
  bool WantsFramesFromCamera(const std::string& cameraId) const override;
  int GetMinFrameIntervalMs() const override { return 100; } // 10fps for debug

  // Camera management
  void SetTargetCamera(const std::string& cameraId);
  std::string GetTargetCamera() const { return m_targetCameraId; }

  // Frame access and diagnostics
  bool HasNewFrame() const { return m_hasNewFrame.load(); }
  CameraFrameData GetLatestFrame() const;
  void MarkFrameConsumed() { m_hasNewFrame.store(false); }

  // Enhanced diagnostics
  uint64_t GetTotalFramesReceived() const { return m_totalFramesReceived.load(); }
  uint64_t GetLastFrameTimestamp() const { return m_lastFrameTimestamp.load(); }
  double GetActualFrameRate() const;
  bool IsCameraConnected() const { return m_cameraConnected.load(); }
  bool IsCameraGrabbing() const { return m_cameraGrabbing.load(); }

  // Performance metrics
  struct PerformanceMetrics {
    double avgFrameRate = 0.0;
    uint64_t totalFrames = 0;
    uint64_t droppedFrames = 0;
    std::chrono::milliseconds avgProcessingTime{ 0 };
    std::chrono::milliseconds maxProcessingTime{ 0 };
  };
  PerformanceMetrics GetPerformanceMetrics() const;

private:
  std::string m_targetCameraId;
  std::string m_subscriberId;

  // Thread-safe frame storage
  mutable std::mutex m_frameMutex;
  CameraFrameData m_latestFrame;
  std::atomic<bool> m_hasNewFrame{ false };

  // Status tracking
  std::atomic<bool> m_cameraConnected{ false };
  std::atomic<bool> m_cameraGrabbing{ false };

  // Performance metrics
  std::atomic<uint64_t> m_totalFramesReceived{ 0 };
  std::atomic<uint64_t> m_lastFrameTimestamp{ 0 };
  std::atomic<uint64_t> m_droppedFrames{ 0 };

  // Frame rate calculation
  mutable std::mutex m_frameRateMutex;
  std::vector<std::chrono::steady_clock::time_point> m_recentFrameTimes;
  static constexpr size_t MAX_FRAME_SAMPLES = 30;

  // Performance timing
  mutable std::mutex m_perfMutex;
  std::vector<std::chrono::milliseconds> m_processingTimes;
  static constexpr size_t MAX_PERF_SAMPLES = 100;

  void UpdateSubscriberId();
  void ResetState();
  void UpdateFrameRate();
  void UpdatePerformanceMetrics(std::chrono::milliseconds processingTime);
};

/**
 * @brief Enhanced Raylib Debug Window with full ICameraHardware integration
 *
 * This class provides comprehensive debugging and monitoring for the Raylib
 * camera feed system, including broadcasting diagnostics, interface validation,
 * and performance monitoring.
 */
class RaylibDebugWindow {
public:
  RaylibDebugWindow();
  ~RaylibDebugWindow();

  // Component setup
  void SetCameraManager(CameraManager* cameraManager);
  void SetRaylibWindow(RaylibWindow* raylibWindow);
  void SetCameraFeedDisplay(CameraFeedDisplay* cameraFeed);
  void SetLogger(Logger* logger);

  // Main rendering
  void RenderUI();

  // Enhanced diagnostics
  void EnableAdvancedDiagnostics(bool enable) { m_advancedDiagnostics = enable; }
  bool IsAdvancedDiagnosticsEnabled() const { return m_advancedDiagnostics; }

private:
  // Core components
  CameraManager* m_cameraManager = nullptr;
  RaylibWindow* m_raylibWindow = nullptr;
  CameraFeedDisplay* m_cameraFeedDisplay = nullptr;
  Logger* m_logger = nullptr;

  // Enhanced state management
  std::string m_selectedCameraId;
  ICameraHardware* m_selectedCameraHardware = nullptr;

  // Debug subscriber for enhanced monitoring
  std::shared_ptr<RaylibDebugSubscriber> m_debugSubscriber;

  // UI state
  bool m_advancedDiagnostics = false;
  bool m_showPerformanceMetrics = true;
  bool m_showBroadcastingDetails = true;
  bool m_showInterfaceValidation = true;

  // Auto-refresh settings
  std::chrono::steady_clock::time_point m_lastRefresh;
  static constexpr std::chrono::milliseconds REFRESH_INTERVAL{ 500 };

  // Enhanced rendering methods
  void RenderCameraSelection();
  void RenderCameraControls();
  void RenderFeedControls();
  void RenderBroadcastingDiagnostics();
  void RenderInterfaceValidation();
  void RenderPerformanceMetrics();
  void RenderQuickActions();
  void RenderAdvancedDebugInfo();

  // Enhanced camera operations
  void SelectCamera(const std::string& cameraId);
  void ConnectCameraWithBroadcasting(const std::string& cameraId);
  void StartGrabbingWithBroadcasting(const std::string& cameraId);
  void StopGrabbingAndBroadcasting(const std::string& cameraId);
  void RefreshCameraConnection(const std::string& cameraId);

  // Broadcasting management
  void SetupDebugSubscriber(const std::string& cameraId);
  void CleanupDebugSubscriber();
  void ValidateBroadcastingSystem();

  // Interface validation
  void ValidateICameraHardwareInterface(const std::string& cameraId);
  void ValidateCameraManagerInterface();
  void ValidateFeedDisplayInterface();

  // Utility methods
  void LogInfo(const std::string& message);
  void LogWarning(const std::string& message);
  void LogError(const std::string& message);
  std::string FormatExposureSettings(const ICameraHardware::ExposureSettings& settings);

  bool ShouldRefresh();
  void UpdateSelectedCamera();
};