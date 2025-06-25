#include "include/camera/pylon_camera_test.h"
#include "include/machine_operations.h"
#include "include/SequenceStep.h"
#include <iostream>
#include <SDL_opengl.h>

// Modify the PylonCameraTest constructor in pylon_camera_test.cpp
PylonCameraTest::PylonCameraTest()
  : m_frameCounter(0)
  , m_lastFrameTimestamp(0)
  , m_hasValidImage(false)
  , m_textureID(0)
  , m_textureInitialized(false)
  , m_newFrameReady(false)
  , m_showMouseCrosshair(false)
  , m_logPixelOnClick(false)
  , m_exposureManager("camera_exposure_config.json")
{
  // Initialize format converter for display
  m_formatConverter.OutputPixelFormat = Pylon::PixelType_RGB8packed;
  m_formatConverter.OutputBitAlignment = Pylon::OutputBitAlignment_MsbAligned;

  // Load calibration from file (do this BEFORE setting callbacks)
  if (!LoadCalibrationFromFile()) {
    // If loading fails, use default values and save them to create the file
    m_pixelToMMFactorX = 0.00248f;  // Default from your JSON
    m_pixelToMMFactorY = 0.00252f;  // Default from your JSON
    SaveCalibrationToFile();  // Create the file with defaults
  }
  // Set callbacks
  m_camera.SetDeviceRemovalCallback([this]() {
    std::cout << "Device removal callback called" << std::endl;
    m_deviceRemoved = true;
  });



  m_camera.SetNewFrameCallback([this](const Pylon::CGrabResultPtr& grabResult) {
    // Process the new frame
    if (grabResult->GrabSucceeded()) {
      // Update frame counter
      m_frameCounter++;

      // Get timestamp
      m_lastFrameTimestamp = grabResult->GetTimeStamp();

      // Get image width and height
      m_lastFrameWidth = grabResult->GetWidth();
      m_lastFrameHeight = grabResult->GetHeight();

      try {
        // Lock mutex while updating the image
        std::lock_guard<std::mutex> lock(m_imageMutex);

        // Store the grab result
        m_ptrGrabResult = grabResult;

        // Convert the grabbed buffer to a pylon image
        if (m_pylonImage.IsValid()) {
          m_pylonImage.Release();
        }
        m_pylonImage.AttachGrabResultBuffer(m_ptrGrabResult);

        // Convert to a format suitable for display
        if (m_formatConverterOutput.IsValid()) {
          m_formatConverterOutput.Release();
        }
        m_formatConverter.Convert(m_formatConverterOutput, m_pylonImage);

        // Signal that a new frame is ready
        m_newFrameReady = true;




        // NEW: Just add this at the end of the callback
        if (m_enableRaylibFeed && m_raylibWindow) {
          SendFrameToRaylib();
        }

      }
      catch (const std::exception& e) {
        std::cerr << "Error in frame callback: " << e.what() << std::endl;
      }
    }


  });

  // Set exposure manager callback to log when settings are applied
  m_exposureManager.SetSettingsAppliedCallback([](const std::string& nodeId, const CameraExposureSettings& settings) {
    std::cout << "Camera exposure settings applied for node: " << nodeId << std::endl;
    std::cout << "  - Exposure: " << settings.exposure_time << " μs" << std::endl;
    std::cout << "  - Gain: " << settings.gain << " dB" << std::endl;
    std::cout << "  - Description: " << settings.description << std::endl;
  });
}

PylonCameraTest::~PylonCameraTest() {
  // Clean up texture if it was created
  if (m_textureInitialized) {
    glDeleteTextures(1, &m_textureID);
    m_textureInitialized = false;
  }

  // Release pylon image resources
  m_ptrGrabResult.Release();
  m_formatConverterOutput.Release();
  m_pylonImage.Release();
}



// NEW: Add this method
void PylonCameraTest::SendFrameToRaylib() {
  if (!m_raylibWindow || !m_formatConverterOutput.IsValid()) {
    return;
  }

  // Rate limit updates to avoid overwhelming raylib
  auto now = std::chrono::steady_clock::now();
  auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastRaylibUpdate);

  if (timeSinceLastUpdate.count() < (1000 / RAYLIB_FPS_LIMIT)) {
    return; // Skip this frame to maintain rate limit
  }

  try {
    // Get image data
    const uint8_t* pImageBuffer = static_cast<uint8_t*>(m_formatConverterOutput.GetBuffer());
    uint32_t width = m_formatConverterOutput.GetWidth();
    uint32_t height = m_formatConverterOutput.GetHeight();

    if (pImageBuffer && width > 0 && height > 0) {
      // Send frame to raylib window
      m_raylibWindow->UpdateVideoFrame(pImageBuffer, width, height, m_lastFrameTimestamp);
      m_lastRaylibUpdate = now;
    }
  }
  catch (const std::exception& e) {
    std::cerr << "Error sending frame to raylib: " << e.what() << std::endl;
  }
}



bool PylonCameraTest::ApplyExposureForNode(const std::string& nodeId) {
  if (!m_camera.IsConnected()) {
    std::cerr << "Cannot apply exposure settings: Camera not connected" << std::endl;
    return false;
  }

  std::cout << "Applying camera exposure settings for node: " << nodeId << std::endl;
  return m_exposureManager.ApplySettingsForNode(m_camera, nodeId);
}

bool PylonCameraTest::ApplyDefaultExposure() {
  if (!m_camera.IsConnected()) {
    std::cerr << "Cannot apply default exposure settings: Camera not connected" << std::endl;
    return false;
  }

  std::cout << "Applying default camera exposure settings" << std::endl;
  return m_exposureManager.ApplyDefaultSettings(m_camera);
}

bool PylonCameraTest::CreateTexture() {
  std::lock_guard<std::mutex> lock(m_imageMutex);

  if (!m_newFrameReady || !m_formatConverterOutput.IsValid()) {
    return false;
  }

  uint32_t width = m_formatConverterOutput.GetWidth();
  uint32_t height = m_formatConverterOutput.GetHeight();
  const uint8_t* pImageBuffer = static_cast<uint8_t*>(m_formatConverterOutput.GetBuffer());

  if (!pImageBuffer || width == 0 || height == 0) {
    return false;
  }

  // Create texture only once
  if (!m_textureInitialized) {
    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);

    // Set parameters once
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Initial texture allocation
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pImageBuffer);
    m_textureInitialized = true;
    m_lastTextureWidth = width;
    m_lastTextureHeight = height;
  }
  else {
    glBindTexture(GL_TEXTURE_2D, m_textureID);

    // Only reallocate if size changed
    if (width != m_lastTextureWidth || height != m_lastTextureHeight) {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pImageBuffer);
      m_lastTextureWidth = width;
      m_lastTextureHeight = height;
    }
    else {
      // Just update existing texture (much faster)
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pImageBuffer);
    }
  }

  glBindTexture(GL_TEXTURE_2D, 0);
  m_hasValidImage = true;
  m_newFrameReady = false;

  return true;
}
// New method for rendering UI with machine operations support
// In RenderUIWithMachineOps(), add frame rate limiting:
void PylonCameraTest::RenderUIWithMachineOps(MachineOperations* machineOps) {
  if (!m_isVisible) return;

  // Rate limit texture updates to 30fps max
  static auto lastTextureUpdate = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  auto timeSinceUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTextureUpdate);

  bool shouldUpdateTexture = (timeSinceUpdate.count() >= 33); // ~30fps

  if (m_newFrameReady && shouldUpdateTexture) {
    if (CreateTexture()) {
      lastTextureUpdate = now;
    }
  }

  // Cache UI calculations only when image size changes
  if (m_lastFrameWidth != m_lastCachedWidth || m_lastFrameHeight != m_lastCachedHeight) {
    float availWidth = ImGui::GetContentRegionAvail().x;
    m_cachedAspectRatio = static_cast<float>(m_lastFrameWidth) / static_cast<float>(m_lastFrameHeight);
    m_cachedDisplayWidth = (std::min)(availWidth, 800.0f);
    m_cachedDisplayHeight = m_cachedDisplayWidth / m_cachedAspectRatio;

    m_lastCachedWidth = m_lastFrameWidth;
    m_lastCachedHeight = m_lastFrameHeight;
  }

  // Main control window
  ImGui::Begin("Pylon Camera Test", &m_controlWindowOpen);

  // Camera connection/initialization
  if (!m_camera.IsConnected()) {
    if (ImGui::Button("Initialize & Connect")) {
      if (m_camera.Initialize() && m_camera.Connect()) {
        std::cout << "Camera initialized and connected" << std::endl;
        // Apply default exposure settings after connecting
        ApplyDefaultExposure();
      }
      else {
        std::cout << "Failed to initialize or connect camera" << std::endl;
      }
    }
  }
  else {
    // Display camera info
    ImGui::Text("%s", m_camera.GetDeviceInfo().c_str());

    // Camera exposure controls section
    if (ImGui::CollapsingHeader("Camera Exposure Settings")) {
      // Button to open exposure manager UI
      if (ImGui::Button("Open Exposure Manager")) {
        m_exposureManager.ToggleWindow();
      }

      ImGui::SameLine();
      if (ImGui::Button("Apply Default Exposure")) {
        ApplyDefaultExposure();
      }

      // Quick exposure controls
      ImGui::Separator();
      ImGui::Text("Quick Node Exposure Controls:");

      // Create buttons for common nodes
      const std::vector<std::pair<std::string, std::string>> commonNodes = {
        {"node_4083", "Sled View"},
        {"node_4107", "PIC View"},
        {"node_4137", "Collimate Lens"},
        {"node_4156", "Focus Lens"},
        {"node_4186", "Pick Coll Lens"},
        {"node_4209", "Pick Focus Lens"},
        {"node_4500", "Serial Number"}
      };

      int buttonCount = 0;
      for (const auto& [nodeId, nodeName] : commonNodes) {
        if (m_exposureManager.HasSettingsForNode(nodeId)) {
          if (ImGui::Button(nodeName.c_str())) {
            ApplyExposureForNode(nodeId);
          }
          buttonCount++;
          if ((buttonCount % 3) != 0) { // 3 buttons per row
            ImGui::SameLine();
          }
        }
      }
    }

    // Grabbing control
    if (!m_camera.IsGrabbing()) {
      // Add "Start Grabbing" button for continuous acquisition
      if (ImGui::Button("Start Grabbing")) {
        // Reset texture when starting grabbing
        if (m_textureInitialized) {
          glDeleteTextures(1, &m_textureID);
          m_textureInitialized = false;
        }
        m_hasValidImage = false;
        m_frameCounter = 0;
        m_newFrameReady = false;

        if (m_camera.StartGrabbing()) {
          std::cout << "Started grabbing" << std::endl;
        }
        else {
          std::cout << "Failed to start grabbing" << std::endl;
        }
      }

      // Add "Grab One Image" button for single frame acquisition
      ImGui::SameLine();
      if (ImGui::Button("Grab One Image")) {
        if (GrabSingleFrame()) {
          // If frame was grabbed successfully, update texture
          if (m_newFrameReady) {
            CreateTexture();
          }
        }
      }
    }
    else {
      // Button to stop continuous acquisition
      if (ImGui::Button("Stop Grabbing")) {
        m_camera.StopGrabbing();
        std::cout << "Stopped grabbing" << std::endl;
      }

      // Add capture button
      ImGui::SameLine();
      if (ImGui::Button("Capture Image")) {
        CaptureImage();
      }

      // Display capture status if an image was captured
      if (m_imageCaptured) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Saved to: %s", m_lastSavedPath.c_str());

        // Auto-reset the status after 3 seconds
        static float statusTimer = 0.0f;
        statusTimer += ImGui::GetIO().DeltaTime;
        if (statusTimer > 3.0f) {
          m_imageCaptured = false;
          statusTimer = 0.0f;
        }
      }

      // Show grabbing stats
      ImGui::Text("Frames received: %d", m_frameCounter);
      ImGui::Text("Last frame size: %dx%d", m_lastFrameWidth, m_lastFrameHeight);
      ImGui::Text("Last timestamp: %llu", m_lastFrameTimestamp);

      // Added: Crosshair controls
      ImGui::Checkbox("Show Mouse Crosshair", &m_showMouseCrosshair);

      // Add gantry movement control checkbox
      ImGui::Checkbox("Enable Click to Move Gantry", &m_enableClickToMove);

      // Add pixel-to-mm calibration controls
      ImGui::Separator();
      ImGui::Text("Pixel-to-MM Calibration:");

      // Add pixel-to-mm calibration controls
      ImGui::Separator();
      ImGui::Text("Pixel-to-MM Calibration:");

      // Store original values to detect changes
      float originalPixelToMMX = m_pixelToMMFactorX;
      float originalPixelToMMY = m_pixelToMMFactorY;

      // Modify the pixel-to-mm factors with improved precision
      float pixelToMMX = m_pixelToMMFactorX;
      float pixelToMMY = m_pixelToMMFactorY;

      bool xChanged = false, yChanged = false;

      if (ImGui::InputFloat("X Factor (mm/pixel)", &pixelToMMX, 0.00001f, 0.0001f, "%.5f")) {
        if (pixelToMMX > 0) {
          m_pixelToMMFactorX = pixelToMMX;
          xChanged = true;
        }
      }

      if (ImGui::InputFloat("Y Factor (mm/pixel)", &pixelToMMY, 0.00001f, 0.0001f, "%.5f")) {
        if (pixelToMMY > 0) {
          m_pixelToMMFactorY = pixelToMMY;
          yChanged = true;
        }
      }

      // Auto-save if values changed
      if (xChanged || yChanged) {
        SaveCalibrationToFile();
        std::cout << "Camera calibration auto-saved due to user changes" << std::endl;
      }

      ImGui::SameLine();
      if (ImGui::Button("Reset to Default")) {
        m_pixelToMMFactorX = 0.00248f;
        m_pixelToMMFactorY = 0.00252f;
        SaveCalibrationToFile();  // Auto-save the reset values
        std::cout << "Camera calibration reset to defaults and saved" << std::endl;
      }

      ImGui::SameLine();
      if (ImGui::Button("Reload from File")) {
        if (LoadCalibrationFromFile()) {
          std::cout << "Camera calibration reloaded from file" << std::endl;
        }
        else {
          std::cout << "Failed to reload camera calibration from file" << std::endl;
        }
      }

      // Show current file path
      ImGui::TextDisabled("Config file: %s", m_calibrationFilePath.c_str());

      // Show preview of what 100 pixels would equal in mm
      float preview100X = 100.0f * m_pixelToMMFactorX;
      float preview100Y = 100.0f * m_pixelToMMFactorY;
      ImGui::TextDisabled("Preview: 100 pixels = %.3f mm (X), %.3f mm (Y)", preview100X, preview100Y);

      // Check if we need to create a new texture from the latest frame
      if (m_newFrameReady) {
        if (CreateTexture()) {
          if (enableDebug) {
            // Debug output for texture creation                     
            std::cout << "successfully created texture with ID: " << m_textureID << std::endl;
          }
        }
      }
    }

    // Disconnect button
    if (ImGui::Button("Disconnect")) {
      m_camera.Disconnect();

      // Clean up texture when disconnecting
      if (m_textureInitialized) {
        glDeleteTextures(1, &m_textureID);
        m_textureInitialized = false;
      }
      m_hasValidImage = false;

      std::cout << "Camera disconnected" << std::endl;
      m_frameCounter = 0;
    }

    // ADD NEW WINDOW CONTROL SECTION HERE
    ImGui::Separator();
    ImGui::Text("Window Controls:");
    if (ImGui::Button(m_imageWindowOpen ? "Hide Image Window" : "Show Image Window")) {
      m_imageWindowOpen = !m_imageWindowOpen;
    }
  }

  // Device removal handling
  if (m_camera.IsCameraDeviceRemoved() || m_deviceRemoved) {
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Camera has been removed!");

    if (ImGui::Button("Try Reconnect")) {
      if (m_camera.TryReconnect()) {
        std::cout << "Successfully reconnected to camera" << std::endl;
        m_deviceRemoved = false;
        // Apply default exposure settings after reconnecting
        ApplyDefaultExposure();
      }
      else {
        std::cout << "Failed to reconnect to camera" << std::endl;
      }
    }
  }

  // Add this somewhere in the camera controls (maybe after exposure settings):
  if (ImGui::CollapsingHeader("Raylib Video Feed")) {
    ImGui::Checkbox("Send to Raylib Window", &m_enableRaylibFeed);
    ImGui::SameLine();
    ImGui::TextDisabled("(Live Video page)");

    if (m_raylibWindow) {
      ImGui::Text("Status: %s", m_raylibWindow->HasVideoFeed() ? "Active" : "Ready");
    }
    else {
      ImGui::TextColored(ImVec4(1, 1, 0, 1), "Raylib window not connected");
    }
  }

  ImGui::End();

  // Render the exposure manager UI
  m_exposureManager.RenderUI();

  // Image display window - always show when we have a valid image
  if ((m_camera.IsGrabbing() || m_hasValidImage) && m_imageWindowOpen) {
    ImGui::Begin("Camera Image", &m_imageWindowOpen);

    if (m_textureInitialized && m_hasValidImage) {
      // Calculate proper display size to fit container
      ImVec2 availSize = ImGui::GetContentRegionAvail();
      float availWidth = availSize.x - 20.0f;  // Leave margin
      float availHeight = availSize.y - 20.0f; // Leave margin

      // Calculate aspect ratio
      float aspectRatio = static_cast<float>(m_lastFrameWidth) / static_cast<float>(m_lastFrameHeight);
      if (aspectRatio <= 0.0f) aspectRatio = 1.0f;

      // Calculate display size to fit container while maintaining aspect ratio
      float displayWidth = availWidth;
      float displayHeight = displayWidth / aspectRatio;

      // If height is too big, fit by height instead
      if (displayHeight > availHeight) {
        displayHeight = availHeight;
        displayWidth = displayHeight * aspectRatio;
      }

      // Ensure reasonable minimum size
      displayWidth = (std::max)(displayWidth, 200.0f);
      displayHeight = (std::max)(displayHeight, 150.0f);

      // Center the image horizontally if it's smaller than available width
      if (displayWidth < availWidth) {
        float centerOffset = (availWidth - displayWidth) * 0.5f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centerOffset);
      }

      // CRITICAL: Get cursor position BEFORE drawing the image
      ImVec2 cursorPos = ImGui::GetCursorScreenPos();

      // Draw the image
      ImVec2 imageSize(displayWidth, displayHeight);
      ImGui::Image((ImTextureID)(intptr_t)m_textureID, imageSize, ImVec2(0, 0), ImVec2(1, 1));

      // Draw crosshairs only if image is reasonable size
      if (displayWidth > 100 && displayHeight > 100) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Calculate image center (relative to where image actually starts)
        ImVec2 imageCenter(
          cursorPos.x + displayWidth * 0.5f,
          cursorPos.y + displayHeight * 0.5f
        );

        // Draw static crosshair (centered on image)
        drawList->AddLine(
          ImVec2(cursorPos.x, imageCenter.y),                // Left edge to right edge at center Y
          ImVec2(cursorPos.x + displayWidth, imageCenter.y),
          IM_COL32(0, 255, 255, 255), 2.0f); // Horizontal line - cyan, thicker

        drawList->AddLine(
          ImVec2(imageCenter.x, cursorPos.y),                // Top edge to bottom edge at center X
          ImVec2(imageCenter.x, cursorPos.y + displayHeight),
          IM_COL32(0, 255, 255, 255), 2.0f); // Vertical line - cyan, thicker

        // Mouse crosshair only when needed
        if (m_showMouseCrosshair) {
          ImVec2 mousePos = ImGui::GetMousePos();
          bool mouseOverImage =
            mousePos.x >= cursorPos.x && mousePos.x <= cursorPos.x + displayWidth &&
            mousePos.y >= cursorPos.y && mousePos.y <= cursorPos.y + displayHeight;

          if (mouseOverImage) {
            drawList->AddLine(
              ImVec2(cursorPos.x, mousePos.y),
              ImVec2(cursorPos.x + displayWidth, mousePos.y),
              IM_COL32(255, 255, 0, 255), 1.0f); // Horizontal line - yellow
            drawList->AddLine(
              ImVec2(mousePos.x, cursorPos.y),
              ImVec2(mousePos.x, cursorPos.y + displayHeight),
              IM_COL32(255, 255, 0, 255), 1.0f); // Vertical line - yellow
          }
        }
      }

      // ADD THIS: Handle mouse clicks on the image for gantry movement
      if (m_enableClickToMove && machineOps && displayWidth > 100 && displayHeight > 100) {
        // Check if mouse was clicked on the image
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
          ImVec2 mousePos = ImGui::GetMousePos();

          // Calculate relative position within the image
          float relativeX = (mousePos.x - cursorPos.x) / displayWidth;
          float relativeY = (mousePos.y - cursorPos.y) / displayHeight;

          // Ensure click is within image bounds
          if (relativeX >= 0.0f && relativeX <= 1.0f &&
            relativeY >= 0.0f && relativeY <= 1.0f) {

            // Convert to pixel coordinates
            float pixelX = relativeX * static_cast<float>(m_lastFrameWidth);
            float pixelY = relativeY * static_cast<float>(m_lastFrameHeight);

            // Calculate center of image in pixels
            float centerPixelX = static_cast<float>(m_lastFrameWidth) * 0.5f;
            float centerPixelY = static_cast<float>(m_lastFrameHeight) * 0.5f;

            // Calculate offset from center in pixels
            float deltaPixelX = pixelX - centerPixelX;
            float deltaPixelY = pixelY - centerPixelY;

            // Convert to mm using calibration factors
            float deltaMM_X = deltaPixelX * m_pixelToMMFactorX;
            float deltaMM_Y = deltaPixelY * m_pixelToMMFactorY;

            std::cout << "Mouse clicked at pixel (" << pixelX << ", " << pixelY << ")" << std::endl;
            std::cout << "Delta from center: (" << deltaPixelX << ", " << deltaPixelY << ") pixels" << std::endl;
            std::cout << "Delta in mm: (" << deltaMM_X << ", " << deltaMM_Y << ")" << std::endl;

            // Move gantry relative to current position
            std::cout << "Moving gantry by (" << deltaMM_X << ", " << deltaMM_Y << ") mm" << std::endl;

            // Perform the relative moves (non-blocking for smoother UI)
            bool success = true;
            if (std::abs(deltaMM_X) > 0.001f) { // Only move if significant distance
              success &= machineOps->MoveRelative("gantry-main", "X", deltaMM_X, false);
            }
            if (std::abs(deltaMM_Y) > 0.001f) { // Only move if significant distance  
              success &= machineOps->MoveRelative("gantry-main", "Y", deltaMM_Y, false);
            }

            if (success) {
              std::cout << "[Yes] Gantry movement commands sent successfully" << std::endl;
            }
            else {
              std::cout << "✗ Failed to send gantry movement commands" << std::endl;
            }
          }
        }
      }
    }
    else {
      ImGui::Text("Waiting for valid image from camera...");
      if (!m_textureInitialized) {
        ImGui::Text("Texture not initialized");
      }
      if (!m_hasValidImage) {
        ImGui::Text("No valid image data");
      }
    }

    ImGui::End();
  }
}

// This method should replace the original RenderUI method
void PylonCameraTest::RenderUI() {
  // Call the extended version with nullptr for machineOps
  RenderUIWithMachineOps(nullptr);
}

bool PylonCameraTest::CaptureImage()
{
  if (!m_camera.IsConnected() || !m_camera.IsGrabbing()) {
    std::cerr << "Cannot capture image: Camera not connected or not grabbing" << std::endl;
    return false;
  }

  try {
    // Lock mutex to ensure we get a complete frame
    std::lock_guard<std::mutex> lock(m_imageMutex);

    // Check if we have a valid grab result
    if (!m_ptrGrabResult || !m_ptrGrabResult->GrabSucceeded() || !m_pylonImage.IsValid()) {
      std::cerr << "No valid frame available to capture" << std::endl;
      return false;
    }

    // Generate a filename based on current time
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "capture_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S") << ".png";
    std::string filename = ss.str();

    // Save the image
    if (SaveImageToDisk(filename)) {
      m_imageCaptured = true;
      m_lastSavedPath = filename;
      std::cout << "Image captured and saved as: " << filename << std::endl;
      return true;
    }
    else {
      std::cerr << "Failed to save image" << std::endl;
      return false;
    }
  }
  catch (const std::exception& e) {
    std::cerr << "Error during image capture: " << e.what() << std::endl;
    return false;
  }
}

bool PylonCameraTest::SaveImageToDisk(const std::string& filename)
{
  try {
    // Save the image using Pylon's image writer
    Pylon::CImagePersistence::Save(Pylon::ImageFileFormat_Png, filename.c_str(), m_pylonImage);
    return true;
  }
  catch (const Pylon::GenericException& e) {
    std::cerr << "Error saving image: " << e.GetDescription() << std::endl;
    return false;
  }
  catch (const std::exception& e) {
    std::cerr << "Error saving image: " << e.what() << std::endl;
    return false;
  }
}

bool PylonCameraTest::GrabSingleFrame()
{
  if (!m_camera.IsConnected()) {
    std::cerr << "Cannot grab frame: Camera not connected" << std::endl;
    return false;
  }

  // If already grabbing continuously, don't do anything
  if (m_camera.IsGrabbing()) {
    std::cerr << "Already grabbing continuously" << std::endl;
    return false;
  }

  try {
    // Start single frame acquisition
    std::cout << "Grabbing single frame..." << std::endl;

    // Create a temporary grab result
    Pylon::CGrabResultPtr grabResult;

    // Open camera if not already open
    if (!m_camera.GetInternalCamera().IsOpen()) {
      m_camera.GetInternalCamera().Open();
    }

    // Set the camera to SingleFrame acquisition mode if possible
    try {
      Pylon::CEnumParameter(m_camera.GetInternalCamera().GetNodeMap(), "AcquisitionMode").SetValue("SingleFrame");
    }
    catch (...) {
      // Some cameras might not support this mode, ignore the error
    }

    // Execute software trigger if camera supports it
    try {
      m_camera.GetInternalCamera().ExecuteSoftwareTrigger();
    }
    catch (...) {
      // If software trigger is not supported, we'll just grab
    }

    // Start grabbing with a timeout of 5 seconds
    m_camera.GetInternalCamera().GrabOne(5000, grabResult);

    if (grabResult && grabResult->GrabSucceeded()) {
      std::cout << "Single frame grabbed successfully" << std::endl;

      // Lock mutex while updating the image
      std::lock_guard<std::mutex> lock(m_imageMutex);

      // Store the grab result
      m_ptrGrabResult = grabResult;

      // Update frame counter and dimensions
      m_frameCounter++;
      m_lastFrameWidth = grabResult->GetWidth();
      m_lastFrameHeight = grabResult->GetHeight();
      m_lastFrameTimestamp = grabResult->GetTimeStamp();

      // Convert the grabbed buffer to a pylon image
      if (m_pylonImage.IsValid()) {
        m_pylonImage.Release();
      }
      m_pylonImage.AttachGrabResultBuffer(m_ptrGrabResult);

      // Convert to a format suitable for display
      if (m_formatConverterOutput.IsValid()) {
        m_formatConverterOutput.Release();
      }
      m_formatConverter.Convert(m_formatConverterOutput, m_pylonImage);

      // Signal that a new frame is ready
      m_newFrameReady = true;

      return true;
    }
    else {
      std::cerr << "Failed to grab single frame" << std::endl;
      return false;
    }
  }
  catch (const Pylon::GenericException& e) {
    std::cerr << "Pylon exception during single frame grab: " << e.GetDescription() << std::endl;
    return false;
  }
  catch (const std::exception& e) {
    std::cerr << "Exception during single frame grab: " << e.what() << std::endl;
    return false;
  }
}

bool PylonCameraTest::NeedsTextureCleanup() const {
  return m_needsTextureCleanup && m_textureInitialized;
}

void PylonCameraTest::CleanupTexture() {
  if (m_textureInitialized) {
    glDeleteTextures(1, &m_textureID);
    m_textureInitialized = false;
  }
  m_needsTextureCleanup = false;
}

// Load calibration from JSON file
bool PylonCameraTest::LoadCalibrationFromFile() {
  try {
    std::ifstream file(m_calibrationFilePath);
    if (!file.is_open()) {
      std::cout << "Camera calibration file not found: " << m_calibrationFilePath
        << ", using default values" << std::endl;
      return false;
    }

    nlohmann::json config;
    file >> config;
    file.close();

    if (config.contains("pixelToMillimeterFactorX")) {
      m_pixelToMMFactorX = config["pixelToMillimeterFactorX"].get<float>();
    }
    if (config.contains("pixelToMillimeterFactorY")) {
      m_pixelToMMFactorY = config["pixelToMillimeterFactorY"].get<float>();
    }

    std::cout << "[Yes] Camera calibration loaded: X=" << m_pixelToMMFactorX
      << ", Y=" << m_pixelToMMFactorY << " mm/pixel" << std::endl;
    return true;
  }
  catch (const std::exception& e) {
    std::cerr << "Error loading camera calibration: " << e.what() << std::endl;
    return false;
  }
}

// Save calibration to JSON file
bool PylonCameraTest::SaveCalibrationToFile() {
  try {
    nlohmann::json config;
    config["pixelToMillimeterFactorX"] = m_pixelToMMFactorX;
    config["pixelToMillimeterFactorY"] = m_pixelToMMFactorY;

    std::ofstream file(m_calibrationFilePath);
    if (!file.is_open()) {
      std::cerr << "Cannot save camera calibration to: " << m_calibrationFilePath << std::endl;
      return false;
    }

    file << std::setw(2) << config << std::endl;
    file.close();

    std::cout << "[Yes] Camera calibration saved: X=" << m_pixelToMMFactorX
      << ", Y=" << m_pixelToMMFactorY << " mm/pixel" << std::endl;
    return true;
  }
  catch (const std::exception& e) {
    std::cerr << "Error saving camera calibration: " << e.what() << std::endl;
    return false;
  }
}