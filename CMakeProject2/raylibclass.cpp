// raylibclass.cpp - Updated with live video feed implementation
#include "raylibclass.h"
#include "include/logger.h"
#include "StatusPage.h"
#include "VisualizePage.h"
// 1. ADD INCLUDE at the top with other includes:
#include "RealtimeChartPage.h"
#include "include/data/global_data_store.h"  // ADD THIS LINE

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#undef Rectangle
#undef DrawText
#endif

#include <raylib.h>
#include <iostream>
    // Page system - now with 4 pages
enum PageType { 
  LIVE_VIDEO_PAGE,
  MENU_PAGE,
  STATUS_PAGE, 
  VISUALIZE_PAGE,
  REALTIME_CHART_PAGE  // ADD THIS LINE
};


RaylibWindow::RaylibWindow()
  : isRunning(false), isVisible(false), shouldClose(false), shouldShutdown(false)
  , piManager(nullptr), dataStore(nullptr), logger(nullptr)
  , newVideoFrameReady(false) {  // Initialize new video flag

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

// NEW: Video frame management methods
void RaylibWindow::UpdateVideoFrame(const unsigned char* imageData, int width, int height, uint64_t timestamp) {
  if (!imageData || width <= 0 || height <= 0) return;

  std::lock_guard<std::mutex> lock(videoMutex);
  currentVideoFrame.UpdateFrame(imageData, width, height, timestamp);
  newVideoFrameReady.store(true);
}

void RaylibWindow::ClearVideoFrame() {
  std::lock_guard<std::mutex> lock(videoMutex);
  currentVideoFrame.Clear();
  raylibVideoFrame.Clear();
  newVideoFrameReady.store(false);
}

bool RaylibWindow::HasVideoFeed() const {
  return newVideoFrameReady.load() || raylibVideoFrame.isValid;
}

VideoFrame RaylibWindow::GetVideoFrameThreadSafe() {
  std::lock_guard<std::mutex> lock(videoMutex);
  return currentVideoFrame;  // This will copy the frame data
}

void RaylibWindow::UpdateRaylibVideoFrame() {
  if (newVideoFrameReady.load()) {
    std::lock_guard<std::mutex> lock(videoMutex);
    raylibVideoFrame = currentVideoFrame;  // Copy frame data
    newVideoFrameReady.store(false);
  }
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

// Helper function to render Live Video Page with video feed
void RenderLiveVideoPage(RenderTexture2D& canvas, Vector2& canvasPos, VideoFrame& videoFrame, bool& videoPaused) {
  // Update video texture if we have a new frame and video is not paused
  static Texture2D videoTexture = { 0 };
  static bool videoTextureLoaded = false;

  if (videoFrame.isValid && !videoPaused) {
    // Unload previous texture if it exists
    if (videoTextureLoaded) {
      UnloadTexture(videoTexture);
    }

    // Create Image from video frame data (C++17 compatible)
    Image videoImage;
    videoImage.data = videoFrame.data.data();
    videoImage.width = videoFrame.width;
    videoImage.height = videoFrame.height;
    videoImage.mipmaps = 1;
    videoImage.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;

    // Load texture from image
    videoTexture = LoadTextureFromImage(videoImage);
    videoTextureLoaded = true;
  }

  // Calculate video display area (maintain aspect ratio)
  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();

  // Reserve space for UI at top
  int uiHeight = 80;
  int availableWidth = screenWidth - 20;  // 10px margin on each side
  int availableHeight = screenHeight - uiHeight - 20;  // Space for UI + margins

  Rectangle videoDisplayArea = { 10, (float)uiHeight, (float)availableWidth, (float)availableHeight };

  if (videoTextureLoaded && videoFrame.isValid) {
    // Calculate scaled size maintaining aspect ratio
    float videoAspect = (float)videoFrame.width / (float)videoFrame.height;
    float containerAspect = videoDisplayArea.width / videoDisplayArea.height;

    float displayWidth, displayHeight;
    if (videoAspect > containerAspect) {
      // Video is wider - fit to width
      displayWidth = videoDisplayArea.width;
      displayHeight = displayWidth / videoAspect;
    }
    else {
      // Video is taller - fit to height
      displayHeight = videoDisplayArea.height;
      displayWidth = displayHeight * videoAspect;
    }

    // Center the video in the display area
    float videoX = videoDisplayArea.x + (videoDisplayArea.width - displayWidth) / 2;
    float videoY = videoDisplayArea.y + (videoDisplayArea.height - displayHeight) / 2;

    Rectangle videoDest = { videoX, videoY, displayWidth, displayHeight };
    Rectangle videoSource = { 0, 0, (float)videoTexture.width, (float)videoTexture.height };

    // Draw the video
    DrawTexturePro(videoTexture, videoSource, videoDest, { 0, 0 }, 0.0f, WHITE);

    // Draw video info overlay
    DrawText(TextFormat("Video: %dx%d", videoFrame.width, videoFrame.height),
      (int)videoX, (int)videoY - 20, 14, WHITE);
  }
  else {
    // No video available
    DrawRectangleRec(videoDisplayArea, DARKGRAY);
    const char* noVideoText = "No Video Feed Available";
    int textWidth = MeasureText(noVideoText, 20);
    DrawText(noVideoText,
      (screenWidth - textWidth) / 2,
      screenHeight / 2,
      20, LIGHTGRAY);
  }

  // Draw UI elements
  DrawText("Live Video Page", 10, 10, 20, DARKBLUE);
  DrawText("M: Menu | S: Status | R: Rectangles | C: Chart | ESC: Close", 10, 35, 14, GRAY);

  // Video controls
  static Rectangle playPauseButton = { 10, 50, 80, 25 };
  static Rectangle stopButton = { 100, 50, 60, 25 };

  Vector2 mousePos = GetMousePosition();

  // Play/Pause button
  Color playPauseColor = CheckCollisionPointRec(mousePos, playPauseButton) ? LIGHTGRAY : GRAY;
  DrawRectangleRec(playPauseButton, playPauseColor);
  DrawRectangleLinesEx(playPauseButton, 1, BLACK);
  const char* playPauseText = videoPaused ? "Play" : "Pause";
  DrawText(playPauseText, (int)playPauseButton.x + 15, (int)playPauseButton.y + 5, 14, BLACK);

  if (CheckCollisionPointRec(mousePos, playPauseButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    videoPaused = !videoPaused;
  }

  // Status indicator
  DrawText(videoPaused ? "PAUSED" : "LIVE", 170, 55, 14, videoPaused ? RED : GREEN);
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

    // ADD THIS LINE to disable raylib debug output
    SetTraceLogLevel(LOG_WARNING); // Only show warnings and errors
    // Or use LOG_ERROR to only show errors
    // Or use LOG_NONE to disable all raylib logging


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
    VisualizePage visualizePage(logger);
    RealtimeChartPage realtimeChartPage(logger);  // ADD THIS LINE
    // 4. ADD data store connection (after dataStore is set up):
    if (dataStore) {
      realtimeChartPage.SetDataStore(dataStore);  // ADD THIS LINE

      
    }
    if (logger) {
      logger->LogInfo("Raylib thread: Checking dataStore connection...");
      if (dataStore) {
        logger->LogInfo("Raylib thread: dataStore found, connecting to RealtimeChartPage");
        realtimeChartPage.SetDataStore(dataStore);

        // Test the connection immediately
        auto channels = dataStore->GetAvailableChannels();
        logger->LogInfo("Raylib thread: dataStore has " + std::to_string(channels.size()) + " channels");
        for (const auto& ch : channels) {
          logger->LogInfo("  Available: " + ch);
        }
      }
      else {
        logger->LogError("Raylib thread: dataStore is NULL!");
      }
    }


    PageType currentPage = LIVE_VIDEO_PAGE;

    // Video playback control
    bool videoPaused = false;

    // Mark as running AFTER successful initialization
    isRunning.store(true);
    isVisible.store(true);

    if (logger) logger->LogInfo("Raylib ready with live video support");

    // Main raylib loop
    while (!WindowShouldClose() && !shouldShutdown.load()) {

      // Update video frame for raylib thread
      UpdateRaylibVideoFrame();

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
      if (IsKeyPressed(KEY_C)) {          // ADD THIS BLOCK
        currentPage = REALTIME_CHART_PAGE;
      }

      // Handle video controls via keyboard
      if (IsKeyPressed(KEY_SPACE)) {
        videoPaused = !videoPaused;
      }

      // Render everything
      BeginDrawing();
      ClearBackground(DARKGRAY);

      if (currentPage == LIVE_VIDEO_PAGE) {
        RenderLiveVideoPage(canvas, canvasPos, raylibVideoFrame, videoPaused);
      }
      else if (currentPage == MENU_PAGE) {
        RenderMenuPage(logger);
      }
      else if (currentPage == STATUS_PAGE) {
        statusPage.Render();
      }
      else if (currentPage == VISUALIZE_PAGE) {
        visualizePage.Render();
      }
      else if (currentPage == REALTIME_CHART_PAGE) {  // ADD THIS BLOCK
        realtimeChartPage.Render();
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