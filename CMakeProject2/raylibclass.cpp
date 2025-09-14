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


// Update the constructor to remove old video frame initialization:
RaylibWindow::RaylibWindow()
  : isRunning(false), isVisible(false), shouldClose(false), shouldShutdown(false)
  , piManager(nullptr), dataStore(nullptr), logger(nullptr), machineOperations(nullptr)
  // REMOVED: , newVideoFrameReady(false)
{
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

    // Disable raylib debug output
    SetTraceLogLevel(LOG_WARNING);

    if (!IsWindowReady()) {
      if (logger) logger->LogError("Failed to create raylib window in thread");
      return;
    }

    if (logger) logger->LogInfo("Raylib window created successfully in thread");
    SetTargetFPS(60);

    // Create page instances
    StatusPage statusPage(logger);
    VisualizePage visualizePage(logger);
    RealtimeChartPage realtimeChartPage(logger);

    // Connect machine operations to realtime chart page
    if (dataStore) {
      realtimeChartPage.SetDataStore(dataStore);
      if (logger) {
        logger->LogInfo("RaylibWindow: Connected dataStore to RealtimeChartPage");
      }
    }

    if (machineOperations) {
      realtimeChartPage.SetMachineOperations(machineOperations);
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

    // Page management
    PageType currentPage = LIVE_VIDEO_PAGE;

    // State management for Live Video Page
    bool savedFullscreenState = false;
    bool savedShowState = true;

    // Mark as running AFTER successful initialization
    isRunning.store(true);
    isVisible.store(true);

    if (logger) logger->LogInfo("Raylib ready with camera feed support");

    // Main raylib loop
    while (!WindowShouldClose() && !shouldShutdown.load()) {

      // Update camera texture from OpenGL - MUST be called every frame
      UpdateCameraTexture();

      // Track previous page for state management
      PageType previousPage = currentPage;

      // Handle page switching input
      if (IsKeyPressed(KEY_M)) currentPage = MENU_PAGE;
      if (IsKeyPressed(KEY_V)) currentPage = LIVE_VIDEO_PAGE;
      if (IsKeyPressed(KEY_S)) currentPage = STATUS_PAGE;
      if (IsKeyPressed(KEY_R)) currentPage = VISUALIZE_PAGE;
      if (IsKeyPressed(KEY_C)) currentPage = REALTIME_CHART_PAGE;

      // Handle Live Video Page special fullscreen behavior
      if (currentPage != previousPage) {
        if (currentPage == LIVE_VIDEO_PAGE) {
          // Entering Live Video Page - save current state and force fullscreen
          savedFullscreenState = m_cameraFullscreenMode;
          savedShowState = m_showCameraFeed;
          m_cameraFullscreenMode = true;  // Force fullscreen on Live Video Page
          m_showCameraFeed = true;        // Force camera visible
          if (logger) {
            logger->LogInfo("Entered Live Video Page - camera forced to fullscreen");
          }
        }
        else if (previousPage == LIVE_VIDEO_PAGE) {
          // Leaving Live Video Page - restore previous state
          m_cameraFullscreenMode = savedFullscreenState;
          m_showCameraFeed = savedShowState;
          if (logger) {
            logger->LogInfo("Left Live Video Page - restored camera state");
          }
        }
      }

      // Camera controls
      if (IsKeyPressed(KEY_F)) {
        // F key toggles camera visibility
        ToggleCameraFeed();
        if (logger) {
          logger->LogInfo("Camera feed " + std::string(m_showCameraFeed ? "enabled" : "disabled"));
        }
      }

      if (IsKeyPressed(KEY_G)) {
        // G key toggles fullscreen/corner mode (disabled on Live Video Page)
        if (currentPage != LIVE_VIDEO_PAGE) {
          m_cameraFullscreenMode = !m_cameraFullscreenMode;
          if (logger) {
            logger->LogInfo("Camera view mode: " + std::string(m_cameraFullscreenMode ? "Fullscreen" : "Corner"));
          }
        }
        else {
          if (logger) {
            logger->LogInfo("Fullscreen toggle disabled on Live Video Page");
          }
        }
      }

      if (IsKeyPressed(KEY_H)) {
        // H key toggles crosshair (only works in fullscreen mode)
        if (m_cameraFullscreenMode && m_showCameraFeed) {
          m_showCrosshair = !m_showCrosshair;
          if (logger) {
            logger->LogInfo("Camera crosshair " + std::string(m_showCrosshair ? "enabled" : "disabled"));
          }
        }
        else {
          if (logger) {
            logger->LogInfo("Crosshair requires fullscreen mode with camera visible");
          }
        }
      }

      // Begin rendering
      BeginDrawing();
      ClearBackground(DARKGRAY);

      // Render current page
      switch (currentPage) {
      case LIVE_VIDEO_PAGE:
        RenderLiveVideoPage();  // Member function, no parameters
        break;
      case MENU_PAGE:
        RenderMenuPage(logger);
        break;
      case STATUS_PAGE:
        statusPage.Render();
        break;
      case VISUALIZE_PAGE:
        visualizePage.Render();
        break;
      case REALTIME_CHART_PAGE:
        realtimeChartPage.Render();
        break;
      }

      // Always render camera overlay (this actually draws the camera feed)
      RenderCameraOverlay();

      // Display camera status for non-Live Video pages
      if (currentPage != LIVE_VIDEO_PAGE && m_cameraFeedDisplay) {
        std::string cameraStatus = m_cameraFeedDisplay->GetStatusText();
        Color statusColor = m_cameraFeedDisplay->IsReceivingFrames() ? GREEN : ORANGE;
        DrawText(cameraStatus.c_str(), 10, GetScreenHeight() - 60, 16, statusColor);

        if (m_cameraFeedDisplay->HasValidTexture()) {
          DrawText("Camera: F=Toggle, G=Fullscreen", 10, GetScreenHeight() - 40, 12, LIGHTGRAY);

          if (m_cameraFullscreenMode) {
            std::string crosshairStatus = m_showCrosshair ? "H=Crosshair [ON]" : "H=Crosshair [OFF]";
            Color crosshairColor = m_showCrosshair ? GREEN : LIGHTGRAY;
            DrawText(crosshairStatus.c_str(), 10, GetScreenHeight() - 20, 12, crosshairColor);
          }
        }
      }

      // Always show FPS
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



// ============================================
// SIMPLIFIED RenderCameraOverlay - Remove duplicate key handling
void RaylibWindow::RenderCameraOverlay() {
  if (!m_showCameraFeed || !m_cameraFeedDisplay || m_cameraTextureID == 0) {
    return;
  }

  // Get camera texture dimensions
  uint32_t texWidth = m_cameraFeedDisplay->GetTextureWidth();
  uint32_t texHeight = m_cameraFeedDisplay->GetTextureHeight();

  if (texWidth == 0 || texHeight == 0) {
    return;
  }

  // REMOVED duplicate key handling - it's now only in main loop
  // Just render based on current state
  if (m_cameraFullscreenMode) {
    RenderCameraFullscreen();
  }
  else {
    RenderCameraInCorner();
    // Reset crosshair when not in fullscreen
    if (m_showCrosshair) {
      m_showCrosshair = false;
    }
  }
}

void RaylibWindow::RenderLiveVideoPage() {
  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();

  // The camera should already be in fullscreen mode from the main loop
  // Just draw the UI overlay

  // Draw header with transparency so camera shows through
  DrawRectangle(0, 0, screenWidth, 90, Fade(BLACK, 0.5f));
  DrawText("LIVE CAMERA FEED", 10, 10, 24, WHITE);
  DrawText("Navigation: M=Menu | S=Status | R=Visualize | C=Chart | ESC=Exit", 10, 40, 14, LIGHTGRAY);

  // Show camera controls
  if (m_cameraFeedDisplay && m_cameraTextureID != 0) {
    DrawText("Camera Controls: F=Hide/Show | H=Crosshair", 10, 65, 14, GREEN);
  }
  else {
    DrawText("Camera: Not Connected", 10, 65, 14, RED);
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



// In raylibclass.cpp, replace the RenderCameraFullscreen() method with this simpler version:

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
    displayWidth = (float)screenWidth;
    displayHeight = screenWidth / aspectRatio;
    offsetY = (screenHeight - displayHeight) * 0.5f;
  }
  else {
    // Fit by height
    displayHeight = (float)screenHeight;
    displayWidth = screenHeight * aspectRatio;
    offsetX = (screenWidth - displayWidth) * 0.5f;
  }

  Rectangle destRect = { offsetX, offsetY, displayWidth, displayHeight };
  Rectangle sourceRect = { 0, 0, texWidth, texHeight };

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

  // SIMPLIFIED CROSSHAIR - Just basic lines, no fancy features
  if (m_showCrosshair) {
    // Calculate center of the IMAGE
    float imageCenterX = offsetX + displayWidth / 2.0f;
    float imageCenterY = offsetY + displayHeight / 2.0f;

    // Simple crosshair - just draw the lines directly
    // Horizontal line (full width of image)
    DrawLineEx(
      Vector2{ offsetX, imageCenterY },
      Vector2{ offsetX + displayWidth, imageCenterY },
      2.0f,
      GREEN  // Simple green color
    );

    // Vertical line (full height of image)
    DrawLineEx(
      Vector2{ imageCenterX, offsetY },
      Vector2{ imageCenterX, offsetY + displayHeight },
      2.0f,
      GREEN
    );

    // Center dot
    DrawCircle((int)imageCenterX, (int)imageCenterY, 5, RED);

    // Simple status text
    DrawText("CROSSHAIR ON", screenWidth - 150, 20, 20, GREEN);
  }

  // Draw controls
  DrawText("CAMERA FEED - FULLSCREEN MODE", 20, 20, 24, WHITE);
  DrawText("Press 'G' to return to corner view", 20, 50, 16, LIGHTGRAY);
  DrawText("Press 'F' to hide camera feed", 20, 70, 16, LIGHTGRAY);

  // Show crosshair control
  if (m_showCrosshair) {
    DrawText("Press 'H' to hide crosshair [ON]", 20, 130, 16, GREEN);
  }
  else {
    DrawText("Press 'H' to show crosshair [OFF]", 20, 130, 16, LIGHTGRAY);
  }
}

// Also add this debug method to help troubleshoot:
void RaylibWindow::DebugCrosshair() {
  if (m_logger) {
    m_logger->LogInfo("=== CROSSHAIR DEBUG ===");
    m_logger->LogInfo("Crosshair enabled: " + std::string(m_showCrosshair ? "YES" : "NO"));
    m_logger->LogInfo("Fullscreen mode: " + std::string(m_cameraFullscreenMode ? "YES" : "NO"));
    m_logger->LogInfo("Camera feed visible: " + std::string(m_showCameraFeed ? "YES" : "NO"));

    if (m_cameraFeedDisplay) {
      m_logger->LogInfo("Camera texture ID: " + std::to_string(m_cameraTextureID));
      m_logger->LogInfo("Camera dimensions: " +
        std::to_string(m_cameraFeedDisplay->GetTextureWidth()) + "x" +
        std::to_string(m_cameraFeedDisplay->GetTextureHeight()));
    }
    m_logger->LogInfo("===================");
  }

  // Also draw debug info on screen
  if (m_showCrosshair && m_cameraFullscreenMode) {
    DrawText("CROSSHAIR DEBUG: SHOULD BE VISIBLE", 400, 20, 20, YELLOW);
    DrawRectangle(400, 50, 300, 100, Fade(RED, 0.3f));
    DrawText("If you see this red box but no crosshair,", 410, 60, 14, WHITE);
    DrawText("there's a rendering issue.", 410, 80, 14, WHITE);
    DrawText("Check console for debug output.", 410, 100, 14, WHITE);
  }
}