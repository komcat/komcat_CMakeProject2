#pragma once
#include "VisionCameraExposureManager.h"
#include "include/camera/CameraFrameData.h"
#include "include/camera/CameraManager.h"
#include "include/machine_operations.h"
#include "../mainUI/MenuManager_uaa3.h"
#include "include/logger.h"
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <SDL_opengl.h>

// Forward declarations
class CameraFrameSubscriber;

/**
 * @brief Camera subscriber for VisionCameraExposureUI
 * Receives frames via broadcasting system for live preview during exposure testing
 */
class VisionExposureUISubscriber : public CameraFrameSubscriber {
public:
  explicit VisionExposureUISubscriber(const std::string& cameraId);
  ~VisionExposureUISubscriber();

  // CameraFrameSubscriber interface
  void OnNewFrame(const CameraFrameData& frameData) override;
  void OnCameraStatusChanged(const std::string& cameraId, bool connected, bool grabbing) override;
  std::string GetSubscriberId() const override { return m_subscriberId; }
  bool WantsFramesFromCamera(const std::string& cameraId) const override;
  int GetMinFrameIntervalMs() const override { return 100; } // 10fps for exposure testing

  // Texture management (call from main thread)
  void UpdateTextureIfNeeded();
  unsigned int GetTextureID() const { return m_textureID; }
  bool HasValidTexture() const { return m_textureInitialized; }
  uint32_t GetTextureWidth() const { return m_textureWidth; }
  uint32_t GetTextureHeight() const { return m_textureHeight; }

  // Status
  bool IsCameraConnected() const { return m_cameraConnected.load(); }
  bool IsCameraGrabbing() const { return m_cameraGrabbing.load(); }
  uint64_t GetFrameCount() const { return m_frameCount.load(); }

private:
  std::string m_subscriberId;
  std::string m_targetCameraId;

  // Frame data
  mutable std::mutex m_frameMutex;
  CameraFrameData m_latestFrame;
  std::atomic<bool> m_hasNewFrame{ false };

  // Status
  std::atomic<bool> m_cameraConnected{ false };
  std::atomic<bool> m_cameraGrabbing{ false };
  std::atomic<uint64_t> m_frameCount{ 0 };

  // OpenGL texture
  unsigned int m_textureID = 0;
  bool m_textureInitialized = false;
  uint32_t m_textureWidth = 0;
  uint32_t m_textureHeight = 0;

  void CreateOrUpdateTexture(const CameraFrameData& frameData);
  void CleanupTexture();
};

/**
 * @brief UI for configuring and testing vision camera exposure settings per node
 */
class VisionCameraExposureUI : public IImguiUI {
public:
  VisionCameraExposureUI();
  ~VisionCameraExposureUI();

  // Service connections
  void SetExposureManager(VisionCameraExposureManager* manager);
  void SetCameraManager(CameraManager* cameraManager);
  void SetMachineOperations(MachineOperations* machineOps);


  // Main render function
  void RenderUI();

  // IImguiUI interface implementation
  void Render() override { RenderUI(); }
  void Show() override { m_visible = true; }
  void Hide() override { m_visible = false; }
  bool IsVisible() const override { return m_visible; }
  const std::string& GetName() const override {
    static std::string name = "Vision Camera Exposure";
    return name;
  }

private:
  // Services
  VisionCameraExposureManager* m_exposureManager = nullptr;
  CameraManager* m_cameraManager = nullptr;
  MachineOperations* m_machineOperations = nullptr;
  Logger* m_logger = nullptr;

  // Camera subscription
  std::shared_ptr<VisionExposureUISubscriber> m_cameraSubscriber;

  // UI State
  bool m_visible = false;
  std::string m_selectedGraph = "Process_Flow";
  std::string m_selectedNodeId;
  std::string m_selectedCameraId = "main_camera";

  // Current settings being edited
  VisionCameraExposureManager::NodeExposureSettings m_currentSettings;
  VisionCameraExposureManager::NodeExposureSettings m_editingSettings;
  bool m_hasUnsavedChanges = false;


  // Save status tracking
  std::chrono::steady_clock::time_point m_lastSaveTime;
  bool m_showSaveSuccess = false;
  std::string m_lastError;

  // Helper methods
  void AddNewNodeSettings(const std::string& nodeId);
  void CopyNodeSettings(const std::string& sourceNodeId, const std::string& targetNodeId);
  bool SaveConfiguration();
  std::string GetCurrentTimestamp();
  void RenderSaveStatus();  // Optional: show save status in UI

  // UI Layout state
  enum class ViewMode {
    NODE_LIST,
    TEST_SEQUENCE,
    MANUAL_CONTROL
  };
  ViewMode m_currentView = ViewMode::NODE_LIST;

  // Test sequence state
  struct TestSequence {
    bool running = false;
    bool paused = false;
    std::vector<std::string> nodeList;
    size_t currentIndex = 0;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point nodeStartTime;
    float dwellTimeSeconds = 3.0f;
    bool autoApplySettings = true;
    std::string status;
  };
  TestSequence m_testSequence;

  // Render sections
  void RenderTopControls();
  void RenderLeftPanel();
  void RenderCenterPanel();
  void RenderRightPanel();

  // Left panel views
  void RenderNodeListView();
  void RenderTestSequenceView();
  void RenderManualControlView();

  // Center panel - camera preview
  void RenderCameraPreview();

  // Right panel - settings editor
  void RenderSettingsEditor();
  void RenderQuickActions();

  // Test sequence methods
  void StartTestSequence();
  void StopTestSequence();
  void UpdateTestSequence();
  void MoveToNextTestNode();

  // Helper methods
  void LoadNodeSettings(const std::string& nodeId);
  void SaveNodeSettings();
  void ApplySettingsToCamera();
  std::vector<std::string> GetNodesForGraph(const std::string& graphName);
  void InitializeCameraFeed();
  void CleanupCameraFeed();
};