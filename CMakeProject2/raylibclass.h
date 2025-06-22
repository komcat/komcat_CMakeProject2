// raylibclass.h - Updated for threading
#pragma once

#include "imgui.h"
#include <thread>
#include <atomic>
#include <mutex>

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
  void SetLogger(Logger* loggerInstance);  // Add logger setter
  void UpdateMachineData(const MachineData& data);

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

  // Machine integration (accessed from main thread)
  PIControllerManager* piManager;
  GlobalDataStore* dataStore;
  Logger* logger;  // Add logger pointer

  // Thread functions
  void RaylibThreadFunction();
  void RenderScene();
  void UpdateFromMachineData();
  MachineData GetMachineDataThreadSafe();
};