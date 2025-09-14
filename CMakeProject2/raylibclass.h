// raylibclass.h - CLEANED VERSION
#pragma once

#include "imgui.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

// Forward declarations
class PIControllerManager;
class GlobalDataStore;
class Logger;
class CameraFeedDisplay;

struct MachineData {
  float gantryX, gantryY, gantryZ;
  float hexLeftX, hexLeftY, hexLeftZ;
  float hexRightX, hexRightY, hexRightZ;
  bool gantryConnected;
  bool hexLeftConnected;
  bool hexRightConnected;
};

// REMOVED: VideoFrame struct - no longer needed

class RaylibWindow {
public:
  RaylibWindow();
  ~RaylibWindow();

  // Thread control
  bool Initialize();
  void Shutdown();
  bool IsRunning() const { return isRunning; }

  // Data integration
  void SetPIControllerManager(PIControllerManager* manager);
  void SetDataStore(GlobalDataStore* store);
  void SetLogger(Logger* loggerInstance);
  void SetMachineOperations(void* machineOps);
  void UpdateMachineData(const MachineData& data);

  // REMOVED: Old video frame methods
  // void UpdateVideoFrame(...);
  // void ClearVideoFrame();
  // bool HasVideoFeed() const;

  // Thread-safe status
  bool IsVisible() const { return isVisible.load(); }
  bool ShouldClose() const { return shouldClose.load(); }

  // Camera feed methods (keeping these)
  void SetCameraFeedDisplay(CameraFeedDisplay* feedDisplay);
  void ClearCameraFeed();
  bool HasCameraFeed() const { return m_cameraFeedDisplay != nullptr; }

  // Camera overlay controls
  void ToggleCameraFeed() { m_showCameraFeed = !m_showCameraFeed; }
  void SetCameraFeedVisible(bool visible) { m_showCameraFeed = visible; }
  void SetCameraFeedAlpha(float alpha) { m_cameraFeedAlpha = alpha; }
  bool IsCameraFeedVisible() const { return m_showCameraFeed; }

private:
  Logger* m_logger = nullptr;

  // Threading
  std::thread raylibThread;
  std::atomic<bool> isRunning;
  std::atomic<bool> isVisible;
  std::atomic<bool> shouldClose;
  std::atomic<bool> shouldShutdown;
  bool m_showCrosshair = false;

  // Thread-safe data
  std::mutex dataMutex;
  MachineData machineData;

  // REMOVED: Old video frame members
  // std::mutex videoMutex;
  // VideoFrame currentVideoFrame;
  // VideoFrame raylibVideoFrame;
  // std::atomic<bool> newVideoFrameReady;

  // Machine integration
  PIControllerManager* piManager;
  GlobalDataStore* dataStore;
  Logger* logger;
  void* machineOperations;

  // Thread functions
  void RaylibThreadFunction();
  void RenderScene();
  void UpdateFromMachineData();
  MachineData GetMachineDataThreadSafe();

  // REMOVED: Old video frame management
  // VideoFrame GetVideoFrameThreadSafe();
  // void UpdateRaylibVideoFrame();

  // Camera feed integration members
  CameraFeedDisplay* m_cameraFeedDisplay = nullptr;
  unsigned int m_cameraTextureID = 0;
  bool m_showCameraFeed = true;
  float m_cameraFeedAlpha = 0.8f;
  bool m_cameraFullscreenMode = false;

  // Camera rendering methods
  void UpdateCameraTexture();
  void RenderCameraOverlay();
  void RenderCameraInCorner();
  void RenderCameraFullscreen();
  void DebugCrosshair();

  // NEW: Clean render method for Live Video Page
  void RenderLiveVideoPage();
};