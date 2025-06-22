// raylibclass.cpp - Threaded implementation
#include "raylibclass.h"
#include "include/logger.h"  // Add logger include

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#undef Rectangle
#undef DrawText
#endif

#include <raylib.h>
#include <iostream>

RaylibWindow::RaylibWindow()
  : isRunning(false), isVisible(false), shouldClose(false), shouldShutdown(false)
  , piManager(nullptr), dataStore(nullptr), logger(nullptr) {  // Initialize logger to nullptr

  // Initialize machine data
  machineData = { 0, 0, 0, 0, 0, 0, 0, 0, 0, false, false, false };
}

RaylibWindow::~RaylibWindow() {
  if (logger) logger->LogInfo("RaylibWindow destructor called");

  // Ensure proper shutdown before destruction
  if (isRunning.load()) {
    Shutdown();
  }

  // Double-check thread is properly cleaned up
  if (raylibThread.joinable()) {
    if (logger) logger->LogWarning("Thread still joinable in destructor, forcing join");
    try {
      shouldShutdown.store(true);
      raylibThread.join();
    }
    catch (...) {
      // Ignore exceptions in destructor
    }
  }

  if (logger) logger->LogInfo("RaylibWindow destructor completed");
}

bool RaylibWindow::Initialize() {
  if (isRunning.load()) return true;

  try {
    if (logger) logger->LogInfo("Starting raylib thread...");

    // Start raylib in separate thread
    raylibThread = std::thread(&RaylibWindow::RaylibThreadFunction, this);

    // Wait longer for thread to start and check if it actually started
    for (int i = 0; i < 50; i++) { // Wait up to 5 seconds
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      if (isRunning.load()) {
        if (logger) logger->LogInfo("Raylib thread started successfully after " + std::to_string(i * 100) + "ms");
        return true;
      }
    }

    // Thread didn't start properly
    if (logger) logger->LogError("Raylib thread failed to start within timeout");

    // Clean up the thread if it exists but didn't start properly
    if (raylibThread.joinable()) {
      shouldShutdown.store(true);
      raylibThread.join();
    }

    return false;

  }
  catch (const std::exception& e) {
    if (logger) logger->LogError("Failed to start raylib thread: " + std::string(e.what()));
    else std::cerr << "Failed to start raylib thread: " << e.what() << std::endl;
    return false;
  }
}

void RaylibWindow::Shutdown() {
  if (!isRunning.load()) return;

  if (logger) logger->LogInfo("Shutting down Raylib window...");

  // Signal thread to shutdown
  shouldShutdown.store(true);

  // Wait for thread to finish with timeout
  if (raylibThread.joinable()) {
    // Give thread time to clean up (max 5 seconds)
    auto start = std::chrono::steady_clock::now();
    while (isRunning.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      auto now = std::chrono::steady_clock::now();
      if (std::chrono::duration_cast<std::chrono::seconds>(now - start).count() > 5) {
        if (logger) logger->LogWarning("Raylib thread shutdown timeout, forcing termination");
        break;
      }
    }

    // Join the thread
    try {
      raylibThread.join();
      if (logger) logger->LogInfo("Raylib thread joined successfully");
    }
    catch (const std::exception& e) {
      if (logger) logger->LogError("Error joining raylib thread: " + std::string(e.what()));
    }
  }

  isRunning.store(false);
  isVisible.store(false);

  if (logger) logger->LogInfo("Raylib window shutdown complete");
}

void RaylibWindow::SetPIControllerManager(PIControllerManager* manager) {
  piManager = manager;
}

void RaylibWindow::SetDataStore(GlobalDataStore* store) {
  dataStore = store;
}

void RaylibWindow::SetLogger(Logger* loggerInstance) {
  logger = loggerInstance;
}

void RaylibWindow::UpdateMachineData(const MachineData& data) {
  std::lock_guard<std::mutex> lock(dataMutex);
  machineData = data;
}

MachineData RaylibWindow::GetMachineDataThreadSafe() {
  std::lock_guard<std::mutex> lock(dataMutex);
  return machineData;
}

void RaylibWindow::RaylibThreadFunction() {
  try {
    if (logger) logger->LogInfo("Raylib thread function starting...");

    // Initialize raylib in this thread
    InitWindow(800, 600, "TEST RAYLIB WINDOW");

    if (!IsWindowReady()) {
      if (logger) logger->LogError("Failed to create raylib window in thread");
      return;
    }

    if (logger) logger->LogInfo("Raylib window created successfully in thread");
    SetTargetFPS(60);

    // No need for camera setup since we're only showing text

    // Mark as running AFTER successful initialization
    isRunning.store(true);
    isVisible.store(true);

    if (logger) logger->LogInfo("Raylib thread fully initialized, entering main loop");

    // Main raylib loop
    while (!WindowShouldClose() && !shouldShutdown.load()) {

      // No camera updates needed for text-only display

      // Render
      BeginDrawing();
      ClearBackground(DARKBLUE);

      // Just display the test text - no 3D scene
      DrawText("TEST RAYLIB WINDOW OK!!!", 200, 300, 60, WHITE);

      // Optional: Add some additional info
      DrawText("Raylib Integration Successful", 250, 400, 30, LIGHTGRAY);
      DrawText("ESC: Close window", 10, 10, 20, LIGHTGRAY);
      DrawFPS(10, GetScreenHeight() - 30);

      EndDrawing();
    }

    if (logger) logger->LogInfo("Raylib thread main loop ended, cleaning up...");

    // Cleanup
    CloseWindow();
    shouldClose.store(true);

  }
  catch (const std::exception& e) {
    if (logger) logger->LogError("Raylib thread error: " + std::string(e.what()));
    else std::cerr << "Raylib thread error: " << e.what() << std::endl;
    shouldClose.store(true);
  }

  isRunning.store(false);
  isVisible.store(false);

  if (logger) logger->LogInfo("Raylib thread function ended");
}

void RaylibWindow::RenderScene() {
  // Get current machine data
  MachineData data = GetMachineDataThreadSafe();

  // Coordinate axes
  DrawLine3D({ 0, 0, 0 }, { 10, 0, 0 }, RED);    // X - Red
  DrawLine3D({ 0, 0, 0 }, { 0, 10, 0 }, GREEN);  // Y - Green
  DrawLine3D({ 0, 0, 0 }, { 0, 0, 10 }, BLUE);   // Z - Blue

  // Gantry system
  Vector3 gantryPos = { data.gantryX * 0.01f, data.gantryY * 0.01f + 5, data.gantryZ * 0.01f };
  Color gantryColor = data.gantryConnected ? BLUE : GRAY;
  DrawCube(gantryPos, 15.0f, 2.0f, 2.0f, gantryColor);
  DrawCubeWires(gantryPos, 15.0f, 2.0f, 2.0f, BLACK);

  // Hexapod Left
  Vector3 hexLeftPos = { data.hexLeftX * 0.01f - 5, data.hexLeftY * 0.01f, data.hexLeftZ * 0.01f };
  Color hexLeftColor = data.hexLeftConnected ? ORANGE : GRAY;
  DrawCylinder(hexLeftPos, 2.0f, 2.0f, 1.0f, 6, hexLeftColor);
  DrawCylinderWires(hexLeftPos, 2.0f, 2.0f, 1.0f, 6, BLACK);

  // Hexapod Right
  Vector3 hexRightPos = { data.hexRightX * 0.01f + 5, data.hexRightY * 0.01f, data.hexRightZ * 0.01f };
  Color hexRightColor = data.hexRightConnected ? PURPLE : GRAY;
  DrawCylinder(hexRightPos, 2.0f, 2.0f, 1.0f, 6, hexRightColor);
  DrawCylinderWires(hexRightPos, 2.0f, 2.0f, 1.0f, 6, BLACK);

  // Work area
  DrawCubeWires({ 0, -2, 0 }, 20.0f, 1.0f, 20.0f, LIGHTGRAY);
}