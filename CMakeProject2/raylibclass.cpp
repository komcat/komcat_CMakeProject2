// raylibclass.cpp - Updated with VisualizePage integration
#include "raylibclass.h"
#include "include/logger.h"
#include "StatusPage.h"     // Include the StatusPage
#include "VisualizePage.h"  // Include the new VisualizePage

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#undef Rectangle
#undef DrawText
#endif

#include <raylib.h>
#include <iostream>

RaylibWindow::RaylibWindow()
  : isRunning(false), isVisible(false), shouldClose(false), shouldShutdown(false)
  , piManager(nullptr), dataStore(nullptr), logger(nullptr) {

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

// Helper function to render Live Video Page
void RenderLiveVideoPage(RenderTexture2D& canvas, Vector2& canvasPos) {
  // Draw the canvas
  Rectangle sourceRec = { 0, 0, (float)canvas.texture.width, -(float)canvas.texture.height };
  DrawTextureRec(canvas.texture, sourceRec, canvasPos, WHITE);

  // Draw canvas border
  DrawRectangleLines((int)canvasPos.x - 2, (int)canvasPos.y - 2,
    canvas.texture.width + 4, canvas.texture.height + 4, BLACK);

  // Live Video Page UI
  DrawText("Live Video Page", 10, 10, 20, DARKBLUE);
  DrawText("M: Menu | S: Status | R: Rectangles | ESC: Close", 10, 40, 14, GRAY);
}

// Helper function to render Menu Page
void RenderMenuPage(Logger* logger) {
  DrawText("Menu Page", 10, 10, 20, DARKBLUE);
  DrawText("V: Live Video | S: Status | R: Rectangles | ESC: Close", 10, 40, 14, GRAY);

  // Button properties
  int buttonWidth = 200;
  int buttonHeight = 60;
  int buttonSpacing = 20;
  int startX = GetScreenWidth() / 2 - buttonWidth / 2;
  int startY = 100;

  Vector2 mousePos = GetMousePosition();

  // Draw 5 buttons in a column
  for (int i = 0; i < 5; i++) {
    Rectangle button = {
      (float)startX,
      (float)(startY + i * (buttonHeight + buttonSpacing)),
      (float)buttonWidth,
      (float)buttonHeight
    };

    // Check if mouse is over button
    bool isHovered = CheckCollisionPointRec(mousePos, button);
    Color buttonColor = isHovered ? LIGHTGRAY : GRAY;
    Color textColor = isHovered ? BLACK : WHITE;

    // Check if button is clicked
    if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      if (logger) logger->LogInfo("Step " + std::to_string(i + 1) + " button clicked");
    }

    // Draw button
    DrawRectangleRec(button, buttonColor);
    DrawRectangleLinesEx(button, 2, BLACK);

    // Draw button text
    const char* buttonText = TextFormat("Step %d", i + 1);
    int textWidth = MeasureText(buttonText, 20);
    DrawText(buttonText,
      (int)(button.x + button.width / 2 - textWidth / 2),
      (int)(button.y + button.height / 2 - 10),
      20, textColor);
  }
}

void RaylibWindow::RaylibThreadFunction() {
  try {
    if (logger) logger->LogInfo("Raylib thread function starting...");

    // Initialize raylib in this thread
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1200, 800, "Raylib Canvas Window");

    if (!IsWindowReady()) {
      if (logger) logger->LogError("Failed to create raylib window in thread");
      return;
    }

    if (logger) logger->LogInfo("Raylib window created successfully in thread");
    SetTargetFPS(60);

    // Create a render texture as canvas for picture rendering
    RenderTexture2D canvas = LoadRenderTexture(600, 400);

    // Canvas position on screen
    Vector2 canvasPos = { 100, 100 };

    // Initialize canvas with white background
    BeginTextureMode(canvas);
    ClearBackground(WHITE);
    EndTextureMode();

    // Create page instances
    StatusPage statusPage(logger);
    VisualizePage visualizePage(logger);  // Create VisualizePage instance

    // Page system - now with 4 pages
    enum PageType { LIVE_VIDEO_PAGE, MENU_PAGE, STATUS_PAGE, VISUALIZE_PAGE };
    PageType currentPage = LIVE_VIDEO_PAGE;

    // Mark as running AFTER successful initialization
    isRunning.store(true);
    isVisible.store(true);

    if (logger) logger->LogInfo("Raylib canvas ready for picture rendering");

    // Main raylib loop
    while (!WindowShouldClose() && !shouldShutdown.load()) {

      // Handle page switching input
      if (IsKeyPressed(KEY_M)) {
        currentPage = MENU_PAGE;
      }
      if (IsKeyPressed(KEY_V)) {
        currentPage = LIVE_VIDEO_PAGE;
      }
      if (IsKeyPressed(KEY_S)) {
        currentPage = STATUS_PAGE;
      }
      if (IsKeyPressed(KEY_R)) {  // R for Rectangles/Visualize
        currentPage = VISUALIZE_PAGE;
      }

      // Render everything
      BeginDrawing();
      ClearBackground(DARKGRAY);

      if (currentPage == LIVE_VIDEO_PAGE) {
        RenderLiveVideoPage(canvas, canvasPos);
      }
      else if (currentPage == MENU_PAGE) {
        RenderMenuPage(logger);
      }
      else if (currentPage == STATUS_PAGE) {
        statusPage.Render();
      }
      else if (currentPage == VISUALIZE_PAGE) {
        visualizePage.Render();  // Use the new VisualizePage class
      }

      DrawFPS(10, GetScreenHeight() - 30);

      EndDrawing();
    }

    if (logger) logger->LogInfo("Raylib thread main loop ended, cleaning up...");

    // Cleanup
    UnloadRenderTexture(canvas);
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