// raylibclass.h - Updated for live video feed support and ScanningUI integration
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

struct MachineData {
  float gantryX, gantryY, gantryZ;
  float hexLeftX, hexLeftY, hexLeftZ;
  float hexRightX, hexRightY, hexRightZ;
  bool gantryConnected;
  bool hexLeftConnected;
  bool hexRightConnected;
};

// Structure for sharing video frames with raylib
struct VideoFrame {
  std::vector<unsigned char> data;
  int width;
  int height;
  bool isValid;
  uint64_t timestamp;  // For frame rate tracking

  VideoFrame() : width(0), height(0), isValid(false), timestamp(0) {}

  void UpdateFrame(const unsigned char* imageData, int w, int h, uint64_t ts = 0) {
    if (imageData && w > 0 && h > 0) {
      width = w;
      height = h;
      timestamp = ts;
      size_t dataSize = w * h * 3; // RGB format
      data.resize(dataSize);
      std::memcpy(data.data(), imageData, dataSize);
      isValid = true;
    }
    else {
      isValid = false;
    }
  }

  void Clear() {
    data.clear();
    width = 0;
    height = 0;
    isValid = false;
    timestamp = 0;
  }
};

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
  void SetMachineOperations(void* machineOps);  // Change from SetScanningUI
  void UpdateMachineData(const MachineData& data);

  // NEW: Video feed integration
  void UpdateVideoFrame(const unsigned char* imageData, int width, int height, uint64_t timestamp = 0);
  void ClearVideoFrame();
  bool HasVideoFeed() const;

  // Thread-safe status
  bool IsVisible() const { return isVisible.load(); }
  bool ShouldClose() const { return shouldClose.load(); }

private:
  // Threading
  std::thread raylibThread;
  std::atomic<bool> isRunning;
  std::atomic<bool> isVisible;
  std::atomic<bool> shouldClose;
  std::atomic<bool> shouldShutdown;

  // Thread-safe data
  std::mutex dataMutex;
  MachineData machineData;

  // NEW: Video frame data (thread-safe)
  std::mutex videoMutex;
  VideoFrame currentVideoFrame;
  VideoFrame raylibVideoFrame; // Copy for raylib thread
  std::atomic<bool> newVideoFrameReady;

  // Machine integration (accessed from main thread)
  PIControllerManager* piManager;
  GlobalDataStore* dataStore;
  Logger* logger;
  void* machineOperations;  // Change from scanningUI


  // Thread functions
  void RaylibThreadFunction();
  void RenderScene();
  void UpdateFromMachineData();
  MachineData GetMachineDataThreadSafe();

  // NEW: Video frame management
  VideoFrame GetVideoFrameThreadSafe();
  void UpdateRaylibVideoFrame();
};