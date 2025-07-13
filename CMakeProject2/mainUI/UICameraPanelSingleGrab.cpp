// UICameraPanelSingleGrab.cpp - CRITICAL FIX VERSION
#include "UICameraPanelSingleGrab.h"
#include "include/camera/CameraManager.h"
#include "include/camera/pylon_camera_test.h"
#include "include/camera/pylon_camera.h"
#include "CameraFeedDisplay.h"
#include "imgui.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>

UICameraPanelSingleGrab::UICameraPanelSingleGrab(CameraManager& cameraManager)
  : m_cameraManager(cameraManager) {

  // Create camera feed display for single frames
  m_frameDisplay = std::make_unique<CameraFeedDisplay>();
  m_frameDisplay->SetPlaceholderText("Single Frame Capture");

  std::cout << "[INFO] UICameraPanelSingleGrab created" << std::endl;
}

UICameraPanelSingleGrab::~UICameraPanelSingleGrab() {
  ClearCamera();
}

void UICameraPanelSingleGrab::RenderTab(PylonCameraTest* camera, const std::string& cameraId) {
  // Update camera reference if changed
  if (m_currentCamera != camera || m_currentCameraId != cameraId) {
    SetSelectedCamera(camera, cameraId);
  }

  if (!ValidateCamera()) {
    ImGui::Text("Camera not available");
    return;
  }

  // Render controls
  RenderControls();

  ImGui::Separator();

  // Render status
  RenderStatus();

  ImGui::Separator();

  // Render capture settings
  RenderCaptureSettings();

  ImGui::Separator();

  // Render frame display
  RenderFrameDisplay();
}

void UICameraPanelSingleGrab::SetSelectedCamera(PylonCameraTest* camera, const std::string& cameraId) {
  // Clear previous camera
  if (m_currentCamera != camera) {
    ClearCamera();
  }

  m_currentCamera = camera;
  m_currentCameraId = cameraId;

  if (camera) {
    std::cout << "[INFO] SingleGrab panel set to camera: " << cameraId << std::endl;

    // **IMPORTANT: Don't automatically set camera source**
    // Only set source when user actually captures a frame
    // This prevents live video from bleeding into this tab
  }
}

void UICameraPanelSingleGrab::ClearCamera() {
  if (m_frameDisplay) {
    // **IMPORTANT: Clear the display source to stop showing live video**
    m_frameDisplay->ClearSource();
  }

  m_currentCamera = nullptr;
  m_currentCameraId = "";
  m_hasCapturedFrame = false;
  m_lastSavedPath = "";

  std::cout << "[INFO] SingleGrab panel camera cleared" << std::endl;
}

void UICameraPanelSingleGrab::RenderControls() {
  if (!ValidateCamera()) {
    ImGui::Text("Camera not connected");
    return;
  }

  // **DEBUG BUTTON FIRST**
  //if (ImGui::Button("TEST - Debug Panel", ImVec2(150, 30))) {
  //  std::cout << "\n=== SINGLE GRAB PANEL DEBUG ===" << std::endl;
  //  std::cout << "Camera ID: " << m_currentCameraId << std::endl;
  //  std::cout << "Camera valid: " << (m_currentCamera ? "true" : "false") << std::endl;
  //  std::cout << "Has captured frame: " << (m_hasCapturedFrame ? "true" : "false") << std::endl;
  //  std::cout << "Frame display valid: " << (m_frameDisplay ? "true" : "false") << std::endl;

  //  if (m_currentCamera) {
  //    auto& pylonCamera = m_currentCamera->GetCamera();
  //    std::cout << "Camera connected: " << (pylonCamera.IsConnected() ? "true" : "false") << std::endl;
  //    std::cout << "Camera grabbing: " << (pylonCamera.IsGrabbing() ? "true" : "false") << std::endl;
  //    std::cout << "Camera has texture: " << (m_currentCamera->HasValidTexture() ? "true" : "false") << std::endl;
  //    std::cout << "Camera texture ID: " << m_currentCamera->GetTextureID() << std::endl;
  //  }

  //  if (m_frameDisplay) {
  //    std::cout << "Display has texture: " << (m_frameDisplay->HasValidTexture() ? "true" : "false") << std::endl;
  //    std::cout << "Display texture ID: " << m_frameDisplay->GetTextureID() << std::endl;
  //  }
  //  std::cout << "================================\n" << std::endl;
  //}

  // Main grab button
  ImVec4 grabButtonColor = ImVec4(0.3f, 0.7f, 0.3f, 1.0f); // Green
  ImGui::PushStyleColor(ImGuiCol_Button, grabButtonColor);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));

  if (ImGui::Button("Grab Single Frame", ImVec2(150, 30))) {
    std::cout << "\n*** GRAB SINGLE FRAME BUTTON CLICKED ***" << std::endl;
    std::cout << "Calling UICameraPanelSingleGrab::GrabSingleFrame()" << std::endl;
    bool result = GrabSingleFrame();
    std::cout << "GrabSingleFrame returned: " << (result ? "SUCCESS" : "FAILED") << std::endl;
    std::cout << "*** END GRAB SINGLE FRAME ***\n" << std::endl;
  }

  ImGui::PopStyleColor(2);

  ImGui::SameLine();

  // Clear frame button
  if (ImGui::Button("Clear Frame", ImVec2(100, 30))) {
    std::cout << "[INFO] Clear Frame button clicked" << std::endl;
    ClearCapturedFrame();
  }

  // Save frame button
  ImGui::Spacing();
  bool canSave = m_hasCapturedFrame;

  if (!canSave) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f); // Dim the button
  }

  if (ImGui::Button("Save Frame to Disk", ImVec2(-1, 30))) {
    if (canSave) {
      SaveFrameToDisk();
    }
  }

  if (!canSave) {
    ImGui::PopStyleVar();
  }

  if (ImGui::IsItemHovered() && !canSave) {
    ImGui::SetTooltip("Capture a frame first to save");
  }
}

void UICameraPanelSingleGrab::RenderStatus() {
  if (!ValidateCamera()) {
    return;
  }

  auto& pylonCamera = m_currentCamera->GetCamera();

  ImGui::Text("Single Frame Status:");
  ImGui::Text("• Connected: %s", pylonCamera.IsConnected() ? "Yes" : "No");
  ImGui::Text("• Frame Captured: %s", m_hasCapturedFrame ? "Yes" : "No");

  if (m_hasCapturedFrame) {
    std::string captureTime = GetCaptureTimeText();
    ImGui::Text("• Captured: %s", captureTime.c_str());
  }

  if (pylonCamera.IsConnected()) {
    auto settings = pylonCamera.GetCurrentExposureSettings();
    ImGui::Text("• Exposure: %.0f μs", settings.exposure_time);
    ImGui::Text("• Gain: %.1f", settings.gain);
  }

  // Display info if frame is available
  if (m_frameDisplay && m_frameDisplay->HasValidTexture()) {
    ImGui::Text("• Resolution: %dx%d", m_frameDisplay->GetTextureWidth(), m_frameDisplay->GetTextureHeight());
  }

  // Show last saved path if available
  if (!m_lastSavedPath.empty()) {
    ImGui::Text("• Last Saved: %s", m_lastSavedPath.c_str());
  }
}

void UICameraPanelSingleGrab::RenderCaptureSettings() {
  ImGui::Text("Capture Settings:");

  ImGui::Checkbox("Auto-save captured frames", &m_autoSave);
  ImGui::SameLine();
  ImGui::Checkbox("Show capture info", &m_showCaptureInfo);

  if (m_autoSave) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "  Frames will be automatically saved to disk");
  }
}

void UICameraPanelSingleGrab::RenderFrameDisplay() {
  // Calculate canvas size
  ImVec2 canvasSize = ImGui::GetContentRegionAvail();
  canvasSize.y = (std::max)(canvasSize.y - 10.0f, 200.0f); // Reserve some space, minimum height

  if (!ValidateCamera()) {
    RenderPlaceholderCanvas(canvasSize.x, canvasSize.y, "Camera Not Available\nSelect a connected camera");
    return;
  }

  // **CRITICAL FIX: Only show captured frames, NOT live video**
  if (m_hasCapturedFrame && m_frameDisplay) {
    try {
      // **IMPORTANT: Only render if we actually have a captured frame**
      // Don't show live video in this tab

      if (m_frameDisplay->HasValidTexture()) {
        //std::cout << "[DEBUG] RenderFrameDisplay: Showing captured frame" << std::endl;

        // Render the captured single frame
        m_frameDisplay->RenderToCanvas(canvasSize.x, canvasSize.y);

        // Overlay capture info if enabled
        if (m_showCaptureInfo) {
          RenderCaptureInfoOverlay(canvasSize);
        }
      }
      else {
        std::cout << "[DEBUG] RenderFrameDisplay: Have captured frame but no valid texture" << std::endl;
        RenderPlaceholderCanvas(canvasSize.x, canvasSize.y,
          "Frame Captured\nBut display not ready\nTry 'Clear Frame' and capture again");
      }
    }
    catch (const std::exception& e) {
      std::cout << "[ERROR] Exception in single frame display: " << e.what() << std::endl;

      RenderPlaceholderCanvas(canvasSize.x, canvasSize.y, "Single Frame Error\nTry capturing again");
      m_hasCapturedFrame = false;
    }
  }
  else {
    // **NO FRAME CAPTURED: Show placeholder, don't show live video**
    std::string placeholderMsg;

    auto& pylonCamera = m_currentCamera->GetCamera();
    if (pylonCamera.IsGrabbing()) {
      placeholderMsg = "No Frame Captured\n\nLive video is running\nClick 'Grab Single Frame' to capture\na still image for this tab";
    }
    else {
      placeholderMsg = "No Frame Captured\n\nClick 'Grab Single Frame' to capture\na still image";
    }

    RenderPlaceholderCanvas(canvasSize.x, canvasSize.y, placeholderMsg);
  }
}

void UICameraPanelSingleGrab::RenderCaptureInfoOverlay(ImVec2 canvasSize) {
  if (!m_hasCapturedFrame) {
    return;
  }

  // Draw info overlay in top-left corner of the canvas
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();
  canvasPos.y -= canvasSize.y; // Move back to top of canvas

  ImDrawList* drawList = ImGui::GetWindowDrawList();

  // Semi-transparent background for text
  ImVec2 overlaySize(200, 60);
  drawList->AddRectFilled(canvasPos,
    ImVec2(canvasPos.x + overlaySize.x, canvasPos.y + overlaySize.y),
    IM_COL32(0, 0, 0, 128));

  // Info text
  std::string infoText = "Captured: " + GetCaptureTimeText();
  if (m_frameDisplay && m_frameDisplay->HasValidTexture()) {
    infoText += "\nSize: " + std::to_string(m_frameDisplay->GetTextureWidth()) +
      "x" + std::to_string(m_frameDisplay->GetTextureHeight());
  }

  ImVec2 textPos(canvasPos.x + 5, canvasPos.y + 5);
  drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), infoText.c_str());
}

void UICameraPanelSingleGrab::RenderPlaceholderCanvas(float width, float height, const std::string& text) {
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();
  ImDrawList* drawList = ImGui::GetWindowDrawList();

  // Canvas background
  drawList->AddRectFilled(canvasPos,
    ImVec2(canvasPos.x + width, canvasPos.y + height),
    IM_COL32(50, 50, 50, 255));

  // Canvas border
  drawList->AddRect(canvasPos,
    ImVec2(canvasPos.x + width, canvasPos.y + height),
    IM_COL32(100, 100, 100, 255));

  // Centered text
  ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
  ImVec2 textPos = ImVec2(canvasPos.x + (width - textSize.x) * 0.5f,
    canvasPos.y + (height - textSize.y) * 0.5f);
  drawList->AddText(textPos, IM_COL32(200, 200, 200, 255), text.c_str());

  // Advance cursor
  ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, canvasPos.y + height));
}

// **CRITICAL FIX: ENHANCED GrabSingleFrame METHOD**
bool UICameraPanelSingleGrab::GrabSingleFrame() {
  if (!ValidateCamera()) {
    std::cout << "[ERROR] Cannot grab frame: Camera not valid" << std::endl;
    return false;
  }

  std::cout << "[DEBUG] *** CRITICAL FIX GrabSingleFrame called for: " << m_currentCameraId << " ***" << std::endl;

  auto& pylonCamera = m_currentCamera->GetCamera();
  bool wasGrabbing = pylonCamera.IsGrabbing();

  // **STEP 1: Stop continuous grabbing if active**
  if (wasGrabbing) {
    std::cout << "[DEBUG] Stopping continuous grabbing for single frame capture..." << std::endl;
    m_cameraManager.StopGrabbing(m_currentCameraId);

    // Wait a moment for grabbing to stop
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // **STEP 2: Grab the single frame (camera now creates texture automatically)**
  if (!m_currentCamera->GrabSingleFrame()) {
    std::cout << "[ERROR] Failed to grab single frame" << std::endl;

    // **STEP 2a: Restart continuous grabbing if it was running**
    if (wasGrabbing) {
      std::cout << "[DEBUG] Restarting continuous grabbing..." << std::endl;
      m_cameraManager.StartGrabbing(m_currentCameraId);
    }

    return false;
  }

  std::cout << "[DEBUG] Single frame captured by camera, now setting up display..." << std::endl;

  // **STEP 3: Update capture state - CAMERA ALREADY HAS TEXTURE**
  m_hasCapturedFrame = true;
  m_lastCaptureTime = std::chrono::steady_clock::now();

  // **STEP 4: Set display to use camera's texture (camera already created it)**
  if (m_frameDisplay) {
    std::cout << "[DEBUG] Setting display source to camera..." << std::endl;
    m_frameDisplay->SetPylonCameraSource(m_currentCamera);

    // **IMMEDIATE CHECK: Camera should already have valid texture**
    if (m_currentCamera->HasValidTexture()) {
      std::cout << "[DEBUG] Camera has valid texture ID: " << m_currentCamera->GetTextureID() << std::endl;

      // Force display to use the camera's texture
      if (m_frameDisplay->UpdateTexture()) {
        std::cout << "[DEBUG] Display successfully linked to camera texture" << std::endl;
      }
      else {
        std::cout << "[DEBUG] Display failed to link to camera texture" << std::endl;
      }
    }
    else {
      std::cout << "[ERROR] Camera doesn't have valid texture after successful grab!" << std::endl;
    }
  }

  // **STEP 5: Debug final state**
  std::cout << "[DEBUG] Final state after capture:" << std::endl;
  std::cout << "  m_hasCapturedFrame: " << (m_hasCapturedFrame ? "true" : "false") << std::endl;
  std::cout << "  Camera HasValidTexture: " << (m_currentCamera->HasValidTexture() ? "true" : "false") << std::endl;
  std::cout << "  Camera TextureID: " << m_currentCamera->GetTextureID() << std::endl;
  if (m_frameDisplay) {
    std::cout << "  Display HasValidTexture: " << (m_frameDisplay->HasValidTexture() ? "true" : "false") << std::endl;
    std::cout << "  Display TextureID: " << m_frameDisplay->GetTextureID() << std::endl;
  }

  // **STEP 6: Restart continuous grabbing if it was running**
  if (wasGrabbing) {
    std::cout << "[DEBUG] Restarting continuous grabbing..." << std::endl;
    m_cameraManager.StartGrabbing(m_currentCameraId);
  }

  // **STEP 7: Auto-save if enabled**
  if (m_autoSave) {
    SaveFrameToDisk();
  }

  std::cout << "[DEBUG] *** CRITICAL FIX GrabSingleFrame completed successfully ***" << std::endl;
  return true;
}

void UICameraPanelSingleGrab::ClearCapturedFrame() {
  if (m_frameDisplay) {
    // **IMPORTANT: Clear the display source to stop showing live video**
    m_frameDisplay->ClearSource();
  }

  m_hasCapturedFrame = false;
  m_lastSavedPath = "";

  std::cout << "[INFO] Captured frame cleared - tab will now show placeholder only" << std::endl;
}

bool UICameraPanelSingleGrab::SaveFrameToDisk() {
  if (!m_hasCapturedFrame || !ValidateCamera()) {
    std::cout << "[ERROR] Cannot save: No frame captured or camera invalid" << std::endl;
    return false;
  }

  // Use camera's capture method which saves the current frame
  if (m_currentCamera->CaptureImage()) {
    // Generate our own filename for tracking
    m_lastSavedPath = GenerateFilename();
    std::cout << "[INFO] Single frame saved as: " << m_lastSavedPath << std::endl;
    return true;
  }
  else {
    std::cout << "[ERROR] Failed to save single frame" << std::endl;
    return false;
  }
}

bool UICameraPanelSingleGrab::ValidateCamera() const {
  if (!m_currentCamera) {
    return false;
  }

  auto& pylonCamera = m_currentCamera->GetCamera();
  return pylonCamera.IsConnected();
}

std::string UICameraPanelSingleGrab::GetCaptureTimeText() const {
  if (!m_hasCapturedFrame) {
    return "None";
  }

  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastCaptureTime);

  if (elapsed.count() < 60) {
    return std::to_string(elapsed.count()) + "s ago";
  }
  else if (elapsed.count() < 3600) {
    return std::to_string(elapsed.count() / 60) + "m ago";
  }
  else {
    return std::to_string(elapsed.count() / 3600) + "h ago";
  }
}

void UICameraPanelSingleGrab::UpdateCaptureDisplay() {
  if (!m_hasCapturedFrame || !m_frameDisplay) {
    return;
  }

  // **CRITICAL FIX: Force immediate update of the frame display**
  try {
    if (ValidateCamera()) {
      std::cout << "[DEBUG] UpdateCaptureDisplay: Setting camera source and forcing update" << std::endl;

      // Set the camera source
      m_frameDisplay->SetPylonCameraSource(m_currentCamera);

      // **FORCE UPDATE: Call UpdateTexture manually to ensure frame is processed**
      m_frameDisplay->UpdateTexture();

      std::cout << "[DEBUG] UpdateCaptureDisplay: HasValidTexture = "
        << (m_frameDisplay->HasValidTexture() ? "true" : "false") << std::endl;
    }
  }
  catch (const std::exception& e) {
    std::cout << "[ERROR] Failed to update capture display: " << e.what() << std::endl;
  }
}

std::string UICameraPanelSingleGrab::GenerateFilename() const {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << "single_frame_" << m_currentCameraId << "_"
    << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S") << ".png";

  return ss.str();
}