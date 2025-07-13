// UICameraPanelSingleGrab.cpp - Reorganized with right status column
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

  // **NEW LAYOUT: Split the tab into two columns**
  ImVec2 availableSize = ImGui::GetContentRegionAvail();
  float leftColumnWidth = availableSize.x * 0.75f;  // 75% for frame display
  float rightColumnWidth = availableSize.x * 0.25f; // 25% for status and controls

  // **LEFT COLUMN: Frame Display and Main Controls**
  ImGui::BeginChild("FrameDisplayColumn", ImVec2(leftColumnWidth, availableSize.y), false);

  // Render main controls at top of left column
  RenderMainControls();

  ImGui::Separator();

  // Render frame display (takes remaining space)
  RenderFrameDisplay();

  ImGui::EndChild();

  ImGui::SameLine();

  // **RIGHT COLUMN: Status and Settings**
  ImGui::BeginChild("StatusControlColumn", ImVec2(rightColumnWidth, availableSize.y), true);

  // Status header
  ImGui::Text("Single Frame Status");
  ImGui::Separator();

  // Render detailed status
  RenderDetailedStatus();

  ImGui::Separator();

  // Render capture settings
  RenderCaptureSettings();

  ImGui::EndChild();
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
  }
}

void UICameraPanelSingleGrab::ClearCamera() {
  if (m_frameDisplay) {
    m_frameDisplay->ClearSource();
  }

  m_currentCamera = nullptr;
  m_currentCameraId = "";
  m_hasCapturedFrame = false;
  m_lastSavedPath = "";

  std::cout << "[INFO] SingleGrab panel camera cleared" << std::endl;
}

// **NEW METHOD: Main controls for left column**
void UICameraPanelSingleGrab::RenderMainControls() {
  if (!ValidateCamera()) {
    ImGui::Text("Camera not connected");
    return;
  }

  // Main grab button
  ImVec4 grabButtonColor = ImVec4(0.3f, 0.7f, 0.3f, 1.0f); // Green
  ImGui::PushStyleColor(ImGuiCol_Button, grabButtonColor);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));

  if (ImGui::Button("Grab Single Frame", ImVec2(150, 30))) {
    std::cout << "\n*** GRAB SINGLE FRAME BUTTON CLICKED ***" << std::endl;
    bool result = GrabSingleFrame();
    std::cout << "GrabSingleFrame returned: " << (result ? "SUCCESS" : "FAILED") << std::endl;
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

// **NEW METHOD: Detailed status for right column**
void UICameraPanelSingleGrab::RenderDetailedStatus() {
  if (!ValidateCamera()) {
    ImGui::Text("No camera selected");
    return;
  }

  auto& pylonCamera = m_currentCamera->GetCamera();

  // Connection status with color coding
  ImGui::Text("Connected:");
  ImGui::SameLine();
  if (pylonCamera.IsConnected()) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Yes");
  }
  else {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "No");
  }

  // Frame captured status with color coding
  ImGui::Text("Frame Captured:");
  ImGui::SameLine();
  if (m_hasCapturedFrame) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Yes");
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "No");
  }

  if (m_hasCapturedFrame) {
    std::string captureTime = GetCaptureTimeText();
    ImGui::Text("Captured:");
    ImGui::SameLine();
    ImGui::Text("%s", captureTime.c_str());
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Camera Settings:");

  if (pylonCamera.IsConnected()) {
    auto settings = pylonCamera.GetCurrentExposureSettings();

    ImGui::Text("Exposure:");
    ImGui::SameLine();
    ImGui::Text("%.0f μs", settings.exposure_time);

    ImGui::Text("Gain:");
    ImGui::SameLine();
    ImGui::Text("%.1f", settings.gain);

    ImGui::Text("Auto Exposure:");
    ImGui::SameLine();
    if (settings.exposure_auto) {
      ImGui::TextColored(ImVec4(0, 1, 0, 1), "On");
    }
    else {
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "Off");
    }

    ImGui::Text("Auto Gain:");
    ImGui::SameLine();
    if (settings.gain_auto) {
      ImGui::TextColored(ImVec4(0, 1, 0, 1), "On");
    }
    else {
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "Off");
    }
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Frame Information:");

  // Display info if frame is available
  if (m_frameDisplay && m_frameDisplay->HasValidTexture()) {
    ImGui::Text("Resolution:");
    ImGui::SameLine();
    ImGui::Text("%dx%d", m_frameDisplay->GetTextureWidth(), m_frameDisplay->GetTextureHeight());

    // Calculate aspect ratio
    if (m_frameDisplay->GetTextureHeight() > 0) {
      float aspectRatio = (float)m_frameDisplay->GetTextureWidth() / (float)m_frameDisplay->GetTextureHeight();
      ImGui::Text("Aspect Ratio:");
      ImGui::SameLine();
      ImGui::Text("%.2f:1", aspectRatio);
    }

    // Calculate image size in MB (rough estimate)
    uint32_t totalPixels = m_frameDisplay->GetTextureWidth() * m_frameDisplay->GetTextureHeight();
    float imageSizeMB = (totalPixels * 3) / (1024.0f * 1024.0f); // RGB = 3 bytes per pixel
    ImGui::Text("Size (est.):");
    ImGui::SameLine();
    ImGui::Text("%.2f MB", imageSizeMB);
  }
  else {
    ImGui::Text("No frame data");
  }

  // Show last saved path if available
  if (!m_lastSavedPath.empty()) {
    ImGui::Spacing();
    ImGui::Text("Last Saved:");
    ImGui::TextWrapped("%s", m_lastSavedPath.c_str());
  }
}

void UICameraPanelSingleGrab::RenderCaptureSettings() {
  ImGui::Text("Capture Settings:");
  ImGui::Spacing();

  ImGui::Checkbox("Auto-save frames", &m_autoSave);
  ImGui::Checkbox("Show capture info", &m_showCaptureInfo);

  if (m_autoSave) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Auto-save enabled");
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Quick Actions:");

  if (ImGui::Button("Reconnect", ImVec2(-1, 25))) {
    auto& pylonCamera = m_currentCamera->GetCamera();
    if (pylonCamera.IsConnected()) {
      pylonCamera.TryReconnect();
    }
  }

  if (ImGui::Button("Debug Camera", ImVec2(-1, 25))) {
    auto& pylonCamera = m_currentCamera->GetCamera();
    pylonCamera.DebugCameraSettings();
  }
}

void UICameraPanelSingleGrab::RenderFrameDisplay() {
  // Calculate canvas size (no need to reserve space for status now)
  ImVec2 canvasSize = ImGui::GetContentRegionAvail();
  canvasSize.y = (std::max)(canvasSize.y - 10.0f, 200.0f); // Small margin

  if (!ValidateCamera()) {
    RenderPlaceholderCanvas(canvasSize.x, canvasSize.y, "Camera Not Available\nSelect a connected camera");
    return;
  }

  // **CRITICAL: Only show captured frames, NOT live video**
  if (m_hasCapturedFrame && m_frameDisplay) {
    try {
      if (m_frameDisplay->HasValidTexture()) {
        // Render the captured single frame
        m_frameDisplay->RenderToCanvas(canvasSize.x, canvasSize.y);

        // Overlay capture info if enabled
        if (m_showCaptureInfo) {
          RenderCaptureInfoOverlay(canvasSize);
        }
      }
      else {
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
    // **NO FRAME CAPTURED: Show placeholder**
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

// **ENHANCED GrabSingleFrame METHOD**
bool UICameraPanelSingleGrab::GrabSingleFrame() {
  if (!ValidateCamera()) {
    std::cout << "[ERROR] Cannot grab frame: Camera not valid" << std::endl;
    return false;
  }

  std::cout << "[DEBUG] *** GrabSingleFrame called for: " << m_currentCameraId << " ***" << std::endl;

  auto& pylonCamera = m_currentCamera->GetCamera();
  bool wasGrabbing = pylonCamera.IsGrabbing();

  // **STEP 1: Stop continuous grabbing if active**
  if (wasGrabbing) {
    std::cout << "[DEBUG] Stopping continuous grabbing for single frame capture..." << std::endl;
    m_cameraManager.StopGrabbing(m_currentCameraId);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // **STEP 2: Grab the single frame**
  if (!m_currentCamera->GrabSingleFrame()) {
    std::cout << "[ERROR] Failed to grab single frame" << std::endl;

    if (wasGrabbing) {
      std::cout << "[DEBUG] Restarting continuous grabbing..." << std::endl;
      m_cameraManager.StartGrabbing(m_currentCameraId);
    }
    return false;
  }

  std::cout << "[DEBUG] Single frame captured by camera, now setting up display..." << std::endl;

  // **STEP 3: Update capture state**
  m_hasCapturedFrame = true;
  m_lastCaptureTime = std::chrono::steady_clock::now();

  // **STEP 4: Set display to use camera's texture**
  if (m_frameDisplay) {
    std::cout << "[DEBUG] Setting display source to camera..." << std::endl;
    m_frameDisplay->SetPylonCameraSource(m_currentCamera);

    if (m_currentCamera->HasValidTexture()) {
      std::cout << "[DEBUG] Camera has valid texture ID: " << m_currentCamera->GetTextureID() << std::endl;

      if (m_frameDisplay->UpdateTexture()) {
        std::cout << "[DEBUG] Display successfully linked to camera texture" << std::endl;
      }
    }
  }

  // **STEP 5: Restart continuous grabbing if it was running**
  if (wasGrabbing) {
    std::cout << "[DEBUG] Restarting continuous grabbing..." << std::endl;
    m_cameraManager.StartGrabbing(m_currentCameraId);
  }

  // **STEP 6: Auto-save if enabled**
  if (m_autoSave) {
    SaveFrameToDisk();
  }

  std::cout << "[DEBUG] *** GrabSingleFrame completed successfully ***" << std::endl;
  return true;
}

void UICameraPanelSingleGrab::ClearCapturedFrame() {
  if (m_frameDisplay) {
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

  if (m_currentCamera->CaptureImage()) {
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

std::string UICameraPanelSingleGrab::GenerateFilename() const {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << "single_frame_" << m_currentCameraId << "_"
    << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S") << ".png";

  return ss.str();
}