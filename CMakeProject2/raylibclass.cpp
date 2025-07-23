// raylibclass.cpp - Updated with live video feed implementation and ScanningUI
#include "raylibclass.h"
#include "include/logger.h"
#include "StatusPage.h"
#include "VisualizePage.h"
// 1. ADD INCLUDE at the top with other includes:
#include "RealtimeChartPage.h"
#include "include/data/global_data_store.h"  // ADD THIS LINE
#include "mainUI/CameraFeedDisplay.h"

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
  , piManager(nullptr), dataStore(nullptr), logger(nullptr), machineOperations(nullptr)  // Change variable name
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

// ADD THIS NEW METHOD:
void RaylibWindow::SetMachineOperations(void* machineOpsPtr) {  // Change method name and parameter
  machineOperations = machineOpsPtr;
  if (logger) {
    logger->LogInfo("RaylibWindow: MachineOperations reference set");
  }
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
    SetExitKey(0);  // 0 = disables ESC as exit key


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

    // CONNECT MACHINE OPERATIONS TO REALTIME CHART PAGE:
    if (dataStore) {
      realtimeChartPage.SetDataStore(dataStore);
      if (logger) {
        logger->LogInfo("RaylibWindow: Connected dataStore to RealtimeChartPage");
      }
    }

    if (machineOperations) {  // Change from scanningUI
      realtimeChartPage.SetMachineOperations(machineOperations);  // Change method name
      if (logger) {
        logger->LogInfo("RaylibWindow: Connected MachineOperations to RealtimeChartPage");
      }
    }

    if (piManager) {
      realtimeChartPage.SetPIControllerManager(piManager);
      if (logger) {
        logger->LogInfo("RaylibWindow: Connected PIControllerManager to RealtimeChartPage");
      }
    }

    PageType currentPage = LIVE_VIDEO_PAGE;

    // Video playback control
    bool videoPaused = false;

    // Mark as running AFTER successful initialization
    isRunning.store(true);
    isVisible.store(true);

    if (logger) logger->LogInfo("Raylib ready with live video support and MachineOperations integration");

    // REPLACE your main raylib loop in RaylibThreadFunction() with this:

    // Main raylib loop
    while (!WindowShouldClose() && !shouldShutdown.load()) {

      // === ADD CAMERA UPDATE HERE (CRITICAL!) ===
      // Update camera texture from OpenGL - MUST be called every frame
      UpdateCameraTexture();
      // ==========================================

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
      if (IsKeyPressed(KEY_C)) {
        currentPage = REALTIME_CHART_PAGE;
      }

      // === ADD CAMERA CONTROLS HERE ===
      // Handle camera visibility toggle with different key (since C is used for chart)
      if (IsKeyPressed(KEY_F)) {  // F for Feed toggle
        ToggleCameraFeed();
        if (logger) {
          logger->LogInfo("Camera feed " + std::string(m_showCameraFeed ? "enabled" : "disabled"));
        }
      }

      // Handle camera fullscreen toggle
      if (IsKeyPressed(KEY_G)) {  // G for fullscreen toggle (since V is used for video page)
        m_cameraFullscreenMode = !m_cameraFullscreenMode;
        if (logger) {
          logger->LogInfo("Camera view mode: " + std::string(m_cameraFullscreenMode ? "Fullscreen" : "Corner"));
        }
      }
      // ================================

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
      else if (currentPage == REALTIME_CHART_PAGE) {
        realtimeChartPage.Render();
      }

      // === ADD CAMERA OVERLAY RENDERING HERE (CRITICAL!) ===
      // Render camera overlay on ALL pages - MUST be called every frame
      RenderCameraOverlay();

      // Add camera status display
      if (m_cameraFeedDisplay) {
        std::string cameraStatus = m_cameraFeedDisplay->GetStatusText();
        Color statusColor = m_cameraFeedDisplay->IsReceivingFrames() ? GREEN : ORANGE;
        DrawText(cameraStatus.c_str(), 10, GetScreenHeight() - 60, 16, statusColor);

        if (m_cameraFeedDisplay->HasValidTexture()) {
          DrawText("Camera: F=Toggle, G=Fullscreen", 10, GetScreenHeight() - 40, 12, LIGHTGRAY);
        }
      }
      // =====================================================

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

// Update your existing RaylibWindow::RenderScene() method in raylibclass.cpp
// Check your RenderScene() method in raylibclass.cpp
// Make sure it looks like this and is actually being called:

void RaylibWindow::RenderScene() {
  // Add this debug at the very beginning to verify the method is called
  static int renderCallCount = 0;
  renderCallCount++;

  if (renderCallCount % 300 == 0) { // Every 5 seconds at 60fps
    if (m_logger) {
      m_logger->LogInfo("RaylibWindow::RenderScene called " + std::to_string(renderCallCount) + " times");
    }
  }

  BeginDrawing();

  // Clear background
  ClearBackground(DARKBLUE);

  // === YOUR EXISTING 3D RENDERING CODE ===
  // Draw grid, 3D models, etc.
  DrawGrid(20, 5.0f);
  // ... other 3D rendering ...

  // === CRITICAL: These camera calls MUST be here ===
  // Update camera texture from OpenGL
  UpdateCameraTexture();

  // Render camera overlay (corner or fullscreen)
  RenderCameraOverlay();

  // Handle camera visibility toggle
  if (IsKeyPressed(KEY_C)) {
    ToggleCameraFeed();
    if (m_logger) {
      m_logger->LogInfo("Camera feed " + std::string(m_showCameraFeed ? "enabled" : "disabled"));
    }
  }

  // === YOUR EXISTING 2D RENDERING ===
  DrawFPS(10, 10);

  // Add camera status
  if (m_cameraFeedDisplay) {
    std::string cameraStatus = m_cameraFeedDisplay->GetStatusText();
    Color statusColor = m_cameraFeedDisplay->IsReceivingFrames() ? GREEN : ORANGE;
    DrawText(cameraStatus.c_str(), 10, GetScreenHeight() - 40, 16, statusColor);

    if (m_cameraFeedDisplay->HasValidTexture()) {
      DrawText("Camera Controls: C=Toggle, V=Fullscreen", 10, GetScreenHeight() - 20, 12, LIGHTGRAY);
    }
  }

  EndDrawing();
}
void RaylibWindow::SetCameraFeedDisplay(CameraFeedDisplay* feedDisplay) {
  if (m_cameraFeedDisplay != feedDisplay) {
    m_cameraFeedDisplay = feedDisplay;
    m_cameraTextureID = 0; // Reset texture reference

    // Safe logging - only log if logger exists
    if (m_logger && feedDisplay) {
      m_logger->LogInfo("RaylibWindow: Camera feed display connected");
    }
  }
}

void RaylibWindow::ClearCameraFeed() {
  m_cameraFeedDisplay = nullptr;
  m_cameraTextureID = 0;

  // Safe logging - only log if logger exists
  if (m_logger) {
    m_logger->LogInfo("RaylibWindow: Camera feed cleared");
  }
}


void RaylibWindow::UpdateCameraTexture() {
  if (!m_cameraFeedDisplay) {
    return;
  }

  // NEW: Add debug logging
  static int updateCount = 0;
  updateCount++;

  // Log every 60 calls (about once per second at 60fps)
  if (updateCount % 60 == 0) {
    if (m_logger) {
      m_logger->LogInfo("RaylibWindow: UpdateCameraTexture called " + std::to_string(updateCount) + " times");
      m_logger->LogInfo("  Feed has source: " + std::string(m_cameraFeedDisplay->HasSource() ? "Yes" : "No"));
      m_logger->LogInfo("  Feed has texture: " + std::string(m_cameraFeedDisplay->HasValidTexture() ? "Yes" : "No"));
    }
  }

  // Update the camera feed display (this updates the OpenGL texture)
  if (m_cameraFeedDisplay->UpdateTexture() && m_cameraFeedDisplay->HasValidTexture()) {
    unsigned int newTextureID = m_cameraFeedDisplay->GetTextureID();

    // NEW: Log when texture ID changes
    if (newTextureID != m_cameraTextureID) {
      if (m_logger) {
        m_logger->LogInfo("RaylibWindow: Camera texture ID changed from " +
          std::to_string(m_cameraTextureID) + " to " + std::to_string(newTextureID));
      }
    }

    m_cameraTextureID = newTextureID;
  }
}


void RaylibWindow::RenderCameraOverlay() {
  if (!m_showCameraFeed || !m_cameraFeedDisplay || m_cameraTextureID == 0) {
    // NEW: Log why we're not rendering
    static int noRenderCount = 0;
    noRenderCount++;

    if (noRenderCount % 300 == 0) { // Log every 5 seconds
      if (m_logger) {
        std::string reason = "Unknown";
        if (!m_showCameraFeed) reason = "Feed not visible";
        else if (!m_cameraFeedDisplay) reason = "No feed display";
        else if (m_cameraTextureID == 0) reason = "No texture ID";

        m_logger->LogInfo("RaylibWindow: Not rendering camera overlay - " + reason);
      }
    }
    return;
  }

  // NEW: Log successful renders occasionally
  static int renderCount = 0;
  renderCount++;

  if (renderCount % 300 == 0) { // Log every 5 seconds
    if (m_logger) {
      m_logger->LogInfo("RaylibWindow: Rendering camera overlay (render #" + std::to_string(renderCount) + ")");
      m_logger->LogInfo("  Texture ID: " + std::to_string(m_cameraTextureID));
      m_logger->LogInfo("  Mode: " + std::string(m_cameraFullscreenMode ? "Fullscreen" : "Corner"));
    }
  }

  // Get camera texture dimensions
  uint32_t texWidth = m_cameraFeedDisplay->GetTextureWidth();
  uint32_t texHeight = m_cameraFeedDisplay->GetTextureHeight();

  if (texWidth == 0 || texHeight == 0) {
    if (m_logger && renderCount % 300 == 0) {
      m_logger->LogInfo("RaylibWindow: Invalid texture dimensions: " +
        std::to_string(texWidth) + "x" + std::to_string(texHeight));
    }
    return;
  }

  // Toggle with 'V' key
  if (IsKeyPressed(KEY_V)) {
    m_cameraFullscreenMode = !m_cameraFullscreenMode;
    if (m_logger) {
      m_logger->LogInfo("Camera view mode: " + std::string(m_cameraFullscreenMode ? "Fullscreen" : "Corner"));
    }
  }

  if (m_cameraFullscreenMode) {
    RenderCameraFullscreen();
  }
  else {
    RenderCameraInCorner();
  }
}



// First, add this include at the top of your raylibclass.cpp file:
// #include <GL/gl.h>  // For OpenGL functions like glIsTexture
// OR if you're using a different OpenGL loader:
// #include <glad/glad.h>  // or whatever OpenGL loader you're using

// Add this validation to your RenderCameraInCorner method:

void RaylibWindow::RenderCameraInCorner() {
  if (m_cameraTextureID == 0) return;

  // SIMPLIFIED: Skip OpenGL validation for now - Raylib handles this
  // if (!glIsTexture(m_cameraTextureID)) {
  //     if (m_logger) {
  //         m_logger->LogWarning("RaylibWindow: Texture ID " + std::to_string(m_cameraTextureID) + 
  //                            " is not valid in OpenGL context");
  //     }
  //     return;
  // }

  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();

  // Camera feed in top-right corner
  float feedWidth = 320.0f;
  float feedHeight = 240.0f;
  float margin = 20.0f;

  // Position in corner
  Rectangle destRect = {
      screenWidth - feedWidth - margin,
      margin,
      feedWidth,
      feedHeight
  };

  // Source rectangle (full texture)
  Rectangle sourceRect = {
      0, 0,
      (float)m_cameraFeedDisplay->GetTextureWidth(),
      (float)m_cameraFeedDisplay->GetTextureHeight()  // Negative height to flip Y
  };

  // Create a Raylib texture from OpenGL texture ID
  Texture2D cameraTexture = {
      .id = m_cameraTextureID,
      .width = (int)m_cameraFeedDisplay->GetTextureWidth(),
      .height = (int)m_cameraFeedDisplay->GetTextureHeight(),
      .mipmaps = 1,
      .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
  };

  // NEW: Log texture details occasionally
  static int renderDetailCount = 0;
  renderDetailCount++;

  if (renderDetailCount % 300 == 0) {
    if (m_logger) {
      m_logger->LogInfo("RaylibWindow Corner Render Details:");
      m_logger->LogInfo("  Texture ID: " + std::to_string(cameraTexture.id));
      m_logger->LogInfo("  Texture Size: " + std::to_string(cameraTexture.width) + "x" + std::to_string(cameraTexture.height));
      m_logger->LogInfo("  Dest Rect: " + std::to_string((int)destRect.x) + "," + std::to_string((int)destRect.y) +
        " " + std::to_string((int)destRect.width) + "x" + std::to_string((int)destRect.height));
    }
  }

  // Draw with transparency
  Color tint = { 255, 255, 255, (unsigned char)(255 * m_cameraFeedAlpha) };

  // Draw background for camera feed
  DrawRectangleRec(destRect, Fade(BLACK, 0.3f));

  // Draw camera texture
  DrawTexturePro(cameraTexture, sourceRect, destRect, Vector2{ 0, 0 }, 0.0f, tint);

  // Draw border
  DrawRectangleLinesEx(destRect, 2, WHITE);

  // Draw label
  DrawText("CAMERA FEED",
    (int)(destRect.x + 5),
    (int)(destRect.y + destRect.height + 5),
    16, WHITE);

  // NEW: Add debug info
  DrawText(("ID:" + std::to_string(m_cameraTextureID)).c_str(),
    (int)(destRect.x + 5),
    (int)(destRect.y - 40),
    12, YELLOW);

  // Draw controls hint
  DrawText("Press 'V' for fullscreen",
    (int)(destRect.x + 5),
    (int)(destRect.y - 20),
    12, LIGHTGRAY);
}


void RaylibWindow::RenderCameraFullscreen() {
  if (m_cameraTextureID == 0) return;

  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();

  // Calculate aspect ratio preserving dimensions
  float texWidth = (float)m_cameraFeedDisplay->GetTextureWidth();
  float texHeight = (float)m_cameraFeedDisplay->GetTextureHeight();
  float aspectRatio = texWidth / texHeight;

  float displayWidth, displayHeight;
  float offsetX = 0, offsetY = 0;

  // Fit to screen while maintaining aspect ratio
  if (aspectRatio > ((float)screenWidth / screenHeight)) {
    // Fit by width
    displayWidth = screenWidth;
    displayHeight = screenWidth / aspectRatio;
    offsetY = (screenHeight - displayHeight) * 0.5f;
  }
  else {
    // Fit by height
    displayHeight = screenHeight;
    displayWidth = screenHeight * aspectRatio;
    offsetX = (screenWidth - displayWidth) * 0.5f;
  }

  Rectangle destRect = { offsetX, offsetY, displayWidth, displayHeight };
  Rectangle sourceRect = { 0, 0, texWidth, texHeight };  // Negative height to flip Y

  // Create texture reference
  Texture2D cameraTexture = {
      .id = m_cameraTextureID,
      .width = (int)texWidth,
      .height = (int)texHeight,
      .mipmaps = 1,
      .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
  };

  // Semi-transparent background
  DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.5f));

  // Draw camera feed
  DrawTexturePro(cameraTexture, sourceRect, destRect, Vector2{ 0, 0 }, 0.0f, WHITE);

  // Draw controls
  DrawText("CAMERA FEED - FULLSCREEN MODE", 20, 20, 24, WHITE);
  DrawText("Press 'V' to return to corner view", 20, 50, 16, LIGHTGRAY);
  DrawText("Press 'C' to hide camera feed", 20, 70, 16, LIGHTGRAY);
}
