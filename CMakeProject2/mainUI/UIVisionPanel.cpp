#include "UIVisionPanel.h"
#include "include/halcon/VisionCircleDetection.h"
#include "include/camera/CameraManager.h"
#include "include/camera/ICameraHardware.h"
#include "include/camera/CameraFrameData.h"
#include <iostream>
#include <filesystem>

// OpenGL headers for texture management
#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#else
#include <OpenGL/gl.h>
#endif

UIVisionPanel::UIVisionPanel() {
  std::cout << "[UIVisionPanel] Initializing Vision Panel with Circle Detection" << std::endl;
  InitializeCircleDetection();
}

UIVisionPanel::~UIVisionPanel() {
  CleanupImageTexture();
  std::cout << "[UIVisionPanel] Vision Panel destroyed" << std::endl;
}

void UIVisionPanel::InitializeCircleDetection() {
  // Create circle detector
  m_circleDetector = std::make_unique<VisionCircleDetection>();

  // Try to load existing parameters, create defaults if not found
  if (!m_circleDetector->LoadParameters(m_parameterFilePath)) {
    std::cout << "[UIVisionPanel] Creating default parameter file" << std::endl;
    if (VisionCircleDetection::CreateDefaultParameterFile(m_parameterFilePath)) {
      m_circleDetector->LoadParameters(m_parameterFilePath);
    }
  }

  std::cout << "[UIVisionPanel] Circle detection initialized successfully" << std::endl;
}

void UIVisionPanel::SetCameraManager(CameraManager* cameraManager) {
  m_cameraManager = cameraManager;

  if (m_cameraManager) {
    std::cout << "[UIVisionPanel] Camera Manager connected" << std::endl;

    // Auto-select first available camera
    auto cameras = GetAvailableCameras();
    if (!cameras.empty()) {
      m_selectedCameraId = cameras[0];
      std::cout << "[UIVisionPanel] Auto-selected camera: " << m_selectedCameraId << std::endl;
    }
  }
}

void UIVisionPanel::RenderUI() {
  if (!m_showWindow) return;

  ImGui::SetNextWindowSize(ImVec2(1400, 800), ImGuiCond_FirstUseEver);

  if (!ImGui::Begin("Vision Processing", &m_showWindow)) {
    ImGui::End();
    return;
  }

  // Split into three columns: Controls, Image, Results
  ImVec2 contentSize = ImGui::GetContentRegionAvail();
  float leftWidth = contentSize.x * 0.3f;    // 30% for controls
  float middleWidth = contentSize.x * 0.45f; // 45% for image display
  float rightWidth = contentSize.x * 0.25f;  // 25% for results

  // Left Panel - Controls
  ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, contentSize.y), true);
  RenderLeftPanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // Middle Panel - Image Display
  ImGui::BeginChild("ImagePanel", ImVec2(middleWidth, contentSize.y), true);
  RenderImageDisplay();
  ImGui::EndChild();

  ImGui::SameLine();

  // Right Panel - Results
  ImGui::BeginChild("RightPanel", ImVec2(rightWidth, contentSize.y), true);
  RenderRightPanel();
  ImGui::EndChild();

  ImGui::End();
}

void UIVisionPanel::RenderLeftPanel() {
  ImGui::Text("Circle Detection");
  ImGui::Separator();

  // Camera Selection
  RenderCameraSelection();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Circle Detection Controls
  RenderCircleDetectionControls();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Parameter Controls
  if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
    RenderCircleParameterControls();
  }
}

void UIVisionPanel::RenderRightPanel() {
  ImGui::Text("Detection Results");
  ImGui::Separator();

  RenderCircleDetectionResults();
}

void UIVisionPanel::RenderCameraSelection() {
  ImGui::Text("Camera Source");

  if (!m_cameraManager) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Camera Manager not available");
    return;
  }

  auto cameras = GetAvailableCameras();

  if (cameras.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No cameras available");
    return;
  }

  // Camera dropdown
  if (ImGui::BeginCombo("Select Camera", m_selectedCameraId.c_str())) {
    for (const auto& cameraId : cameras) {
      bool isSelected = (m_selectedCameraId == cameraId);
      if (ImGui::Selectable(cameraId.c_str(), isSelected)) {
        m_selectedCameraId = cameraId;
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  // Camera status
  if (!m_selectedCameraId.empty()) {
    auto status = m_cameraManager->GetCameraStatus(m_selectedCameraId);

    ImGui::Text("Status:");
    ImGui::SameLine();
    if (status.connected) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected");
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Disconnected");
    }

    if (!status.connected) {
      ImGui::SameLine();
      if (ImGui::Button("Connect")) {
        m_cameraManager->ConnectCamera(m_selectedCameraId);
      }
    }
  }
}

void UIVisionPanel::RenderCircleDetectionControls() {
  ImGui::Text("Circle Detection");

  if (!m_circleDetector) {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Circle detector not initialized");
    return;
  }

  // Main execute button
  bool canExecute = m_cameraManager && !m_selectedCameraId.empty();

  if (!canExecute) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));

  if (ImGui::Button("Execute Detection", ImVec2(-1, 40))) {
    if (canExecute) {
      ExecuteCircleDetection();
    }
  }

  ImGui::PopStyleColor(2);

  if (!canExecute) {
    ImGui::PopStyleVar();
  }

  if (!canExecute) {
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Select and connect a camera first");
  }

  // Processing time display
  if (m_hasResult) {
    ImGui::Text("Last processing time: %.1f ms", m_circleDetector->GetLastProcessingTime());
  }
}

void UIVisionPanel::RenderCircleDetectionResults() {
  if (!m_hasResult) {
    ImGui::Text("No detection results yet.");
    ImGui::Text("Execute circle detection to see results.");
    return;
  }

  const auto& result = m_lastResult;

  // Detection status
  ImGui::SetWindowFontScale(1.2f);
  if (result.found) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Circle Detected");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "✗ No Circle Found");
  }
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();

  if (result.found) {
    // Results table
    if (ImGui::BeginTable("Results", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120.0f);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      // Center coordinates
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Center X");
      ImGui::TableNextColumn();
      ImGui::Text("%.1f pixels", result.centerX);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Center Y");
      ImGui::TableNextColumn();
      ImGui::Text("%.1f pixels", result.centerY);

      // Radius
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Radius");
      ImGui::TableNextColumn();
      ImGui::Text("%.1f pixels", result.radius);

      // Confidence
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Confidence");
      ImGui::TableNextColumn();
      ImGui::Text("%.1f%%", result.confidence * 100.0);

      // Additional metrics
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Circularity");
      ImGui::TableNextColumn();
      ImGui::Text("%.3f", result.circularity);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Area");
      ImGui::TableNextColumn();
      ImGui::Text("%.0f pixels²", result.area);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Candidates");
      ImGui::TableNextColumn();
      ImGui::Text("%d regions", result.numCandidates);

      ImGui::EndTable();
    }

    ImGui::Spacing();

    // Action buttons
    if (ImGui::Button("Send to Robot", ImVec2(120, 30))) {
      std::cout << "[UIVisionPanel] Sending coordinates to robot: ("
        << result.centerX << ", " << result.centerY << ")" << std::endl;
      // TODO: Integrate with robot/motion system
    }

    ImGui::SameLine();
    if (ImGui::Button("Save Results", ImVec2(120, 30))) {
      // TODO: Save results to file
      std::cout << "[UIVisionPanel] Saving detection results" << std::endl;
    }
  }
  else {
    // Show error information
    ImGui::Text("Detection failed:");
    ImGui::BulletText("Candidates found: %d", result.numCandidates);

    if (!result.errorMessage.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Error: %s", result.errorMessage.c_str());
    }

    ImGui::Spacing();
    ImGui::Text("Try adjusting parameters:");
    ImGui::BulletText("Lower threshold values for darker images");
    ImGui::BulletText("Adjust radius range for different circle sizes");
    ImGui::BulletText("Modify ROI size and position");
  }
}

void UIVisionPanel::RenderCircleParameterControls() {
  if (!m_circleDetector) return;

  auto params = m_circleDetector->GetParameters();
  bool paramsChanged = false;

  // Quick presets
  ImGui::Text("Quick Presets:");
  if (ImGui::Button("Small Circles", ImVec2(100, 25))) {
    params.minRadius = 10.0f;
    params.maxRadius = 30.0f;
    params.targetRadius = 20.0f;
    params.minArea = 100;
    paramsChanged = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Medium Circles", ImVec2(100, 25))) {
    params.minRadius = 40.0f;
    params.maxRadius = 80.0f;
    params.targetRadius = 60.0f;
    params.minArea = 500;
    paramsChanged = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Large Circles", ImVec2(100, 25))) {
    params.minRadius = 80.0f;
    params.maxRadius = 150.0f;
    params.targetRadius = 115.0f;
    params.minArea = 2000;
    paramsChanged = true;
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Key parameters
  ImGui::Text("Circle Size:");
  if (ImGui::SliderFloat("Target Radius", &params.targetRadius, 10.0f, 200.0f, "%.1f px")) {
    paramsChanged = true;
  }
  if (ImGui::SliderFloat("Min Radius", &params.minRadius, 1.0f, 200.0f, "%.1f px")) {
    paramsChanged = true;
  }
  if (ImGui::SliderFloat("Max Radius", &params.maxRadius, 1.0f, 200.0f, "%.1f px")) {
    paramsChanged = true;
  }

  ImGui::Spacing();
  ImGui::Text("Threshold:");
  if (ImGui::SliderInt("Low Threshold", &params.thresholdLow, 0, 255)) {
    paramsChanged = true;
  }
  if (ImGui::SliderInt("High Threshold", &params.thresholdHigh, 0, 255)) {
    paramsChanged = true;
  }
  if (ImGui::Checkbox("Invert Image", &params.invertImage)) {
    paramsChanged = true;
  }

  ImGui::Spacing();
  ImGui::Text("ROI Settings:");
  if (ImGui::SliderInt("ROI Size", &params.roiSize, 50, 500, "%d px")) {
    paramsChanged = true;
  }
  if (ImGui::SliderInt("ROI Offset X", &params.roiOffsetX, -200, 200, "%d px")) {
    paramsChanged = true;
  }
  if (ImGui::SliderInt("ROI Offset Y", &params.roiOffsetY, -200, 200, "%d px")) {
    paramsChanged = true;
  }

  // Apply changes
  if (paramsChanged) {
    // Ensure parameter consistency
    if (params.minRadius > params.maxRadius) {
      params.maxRadius = params.minRadius;
    }
    if (params.targetRadius < params.minRadius) {
      params.targetRadius = params.minRadius;
    }
    if (params.targetRadius > params.maxRadius) {
      params.targetRadius = params.maxRadius;
    }
    if (params.thresholdLow > params.thresholdHigh) {
      params.thresholdHigh = params.thresholdLow;
    }

    m_circleDetector->SetParameters(params);
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Parameter file operations
  ImGui::Text("Parameter File:");
  if (ImGui::Button("Load", ImVec2(60, 25))) {
    LoadParameters();
  }
  ImGui::SameLine();
  if (ImGui::Button("Save", ImVec2(60, 25))) {
    SaveParameters();
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset", ImVec2(60, 25))) {
    ResetToDefaults();
  }
}

void UIVisionPanel::ExecuteCircleDetection() {
  if (!m_circleDetector || !m_cameraManager || m_selectedCameraId.empty()) {
    std::cout << "[UIVisionPanel] Cannot execute: missing components" << std::endl;
    return;
  }

  std::cout << "[UIVisionPanel] Executing circle detection on camera: " << m_selectedCameraId << std::endl;

  // Capture image from camera
  std::vector<uint8_t> imageBuffer;
  int width, height, channels;

  if (!CaptureImageFromCamera(imageBuffer, width, height, channels)) {
    std::cout << "[UIVisionPanel] Failed to capture image from camera" << std::endl;
    return;
  }

  std::cout << "[UIVisionPanel] Captured image: " << width << "x" << height << " (" << channels << " channels)" << std::endl;

  // Execute detection
  m_lastResult = m_circleDetector->DetectFromBuffer(imageBuffer.data(), width, height, channels);
  m_hasResult = true;

  // Log results
  if (m_lastResult.found) {
    std::cout << "[UIVisionPanel] Circle detected at (" << m_lastResult.centerX
      << ", " << m_lastResult.centerY << ") with radius " << m_lastResult.radius << std::endl;
  }
  else {
    std::cout << "[UIVisionPanel] No circle detected. Candidates: " << m_lastResult.numCandidates << std::endl;
  }
}

std::vector<std::string> UIVisionPanel::GetAvailableCameras() {
  if (!m_cameraManager) {
    return {};
  }

  return m_cameraManager->GetCameraIds();
}

bool UIVisionPanel::CaptureImageFromCamera(std::vector<uint8_t>& imageBuffer, int& width, int& height, int& channels) {
  if (!m_cameraManager || m_selectedCameraId.empty()) {
    return false;
  }

  // Get camera hardware
  ICameraHardware* camera = m_cameraManager->GetCameraHardware(m_selectedCameraId);
  if (!camera || !camera->IsConnected()) {
    std::cout << "[UIVisionPanel] Camera not connected: " << m_selectedCameraId << std::endl;
    return false;
  }

  // Capture frame
  CameraFrameData frameData;
  if (!camera->CaptureFrame(frameData)) {
    std::cout << "[UIVisionPanel] Failed to capture frame" << std::endl;
    return false;
  }

  if (!frameData.IsValid() || frameData.imageData.empty()) {
    std::cout << "[UIVisionPanel] Invalid frame data" << std::endl;
    return false;
  }

  // Copy frame data for display
  width = frameData.width;
  height = frameData.height;
  channels = frameData.channels;
  imageBuffer = frameData.imageData;

  // Update texture for display
  UpdateImageTexture(imageBuffer, width, height, channels);

  return true;
}

void UIVisionPanel::LoadParameters() {
  if (m_circleDetector && m_circleDetector->LoadParameters(m_parameterFilePath)) {
    std::cout << "[UIVisionPanel] Parameters loaded from: " << m_parameterFilePath << std::endl;
  }
}

void UIVisionPanel::SaveParameters() {
  if (m_circleDetector && m_circleDetector->SaveParameters(m_parameterFilePath)) {
    std::cout << "[UIVisionPanel] Parameters saved to: " << m_parameterFilePath << std::endl;
  }
}

void UIVisionPanel::ResetToDefaults() {
  if (m_circleDetector) {
    auto defaults = VisionCircleDetection::GetDefaultParameters();
    m_circleDetector->SetParameters(defaults);
    std::cout << "[UIVisionPanel] Parameters reset to defaults" << std::endl;
  }
}

void UIVisionPanel::RenderImageDisplay() {
  ImGui::Text("Image Display");
  ImGui::Separator();

  if (!m_hasImageData || m_imageTextureId == 0) {
    // No image available
    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    ImVec2 placeholderSize = ImVec2(availableSize.x, availableSize.y - 30);

    ImGui::BeginChild("ImagePlaceholder", placeholderSize, true);

    ImVec2 centerPos = ImVec2(placeholderSize.x * 0.5f - 60, placeholderSize.y * 0.5f - 10);
    ImGui::SetCursorPos(centerPos);

    if (m_hasResult) {
      ImGui::Text("Image processed");
      ImGui::SetCursorPos(ImVec2(centerPos.x - 20, centerPos.y + 20));
      ImGui::Text("Click 'Execute' to");
      ImGui::SetCursorPos(ImVec2(centerPos.x - 15, centerPos.y + 35));
      ImGui::Text("capture new image");
    }
    else {
      ImGui::Text("No image captured");
      ImGui::SetCursorPos(ImVec2(centerPos.x - 30, centerPos.y + 20));
      ImGui::Text("Click 'Execute Detection'");
      ImGui::SetCursorPos(ImVec2(centerPos.x - 20, centerPos.y + 35));
      ImGui::Text("to capture and process");
    }

    ImGui::EndChild();

    ImGui::Text("Image size: No image loaded");
    return;
  }

  // Render image with detection overlay
  RenderImageWithOverlay();

  // Image info
  ImGui::Text("Image size: %dx%d (%d channels)", m_imageWidth, m_imageHeight,
    m_lastImageData.size() / (m_imageWidth * m_imageHeight));
}

void UIVisionPanel::RenderImageWithOverlay() {
  if (m_imageTextureId == 0 || m_imageWidth == 0 || m_imageHeight == 0) {
    return;
  }

  // Calculate display size maintaining aspect ratio
  ImVec2 availableSize = ImGui::GetContentRegionAvail();
  availableSize.y -= 30; // Leave space for image info

  float imageAspect = static_cast<float>(m_imageWidth) / static_cast<float>(m_imageHeight);
  ImVec2 displaySize;

  if (availableSize.x / imageAspect <= availableSize.y) {
    displaySize.x = availableSize.x;
    displaySize.y = availableSize.x / imageAspect;
  }
  else {
    displaySize.x = availableSize.y * imageAspect;
    displaySize.y = availableSize.y;
  }

  // Center the image
  ImVec2 imagePos = ImVec2(
    (availableSize.x - displaySize.x) * 0.5f,
    (availableSize.y - displaySize.y) * 0.5f
  );

  ImGui::SetCursorPos(imagePos);
  ImVec2 screenImagePos = ImGui::GetCursorScreenPos();

  // Display the image
  ImGui::Image((ImTextureID)(intptr_t)m_imageTextureId, displaySize);

  // Draw overlay if detection was successful
  if (m_hasResult && m_lastResult.found) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Calculate scale factors
    float scaleX = displaySize.x / m_imageWidth;
    float scaleY = displaySize.y / m_imageHeight;

    // Circle center in screen coordinates
    float centerX = screenImagePos.x + (m_lastResult.centerX * scaleX);
    float centerY = screenImagePos.y + (m_lastResult.centerY * scaleY);
    float radius = m_lastResult.radius * scaleX; // Use X scale for radius

    // Draw detected circle outline
    ImU32 circleColor = IM_COL32(255, 0, 0, 255); // Red circle
    drawList->AddCircle(ImVec2(centerX, centerY), radius, circleColor, 64, 2.0f);

    // Draw center crosshair
    ImU32 crosshairColor = IM_COL32(0, 255, 0, 255); // Green crosshair
    float crossSize = 10.0f;
    drawList->AddLine(
      ImVec2(centerX - crossSize, centerY),
      ImVec2(centerX + crossSize, centerY),
      crosshairColor, 2.0f
    );
    drawList->AddLine(
      ImVec2(centerX, centerY - crossSize),
      ImVec2(centerX, centerY + crossSize),
      crosshairColor, 2.0f
    );

    // Draw center dot
    drawList->AddCircleFilled(ImVec2(centerX, centerY), 3.0f, crosshairColor);

    // Draw coordinate text
    std::string coordText = "(" + std::to_string((int)m_lastResult.centerX) +
      ", " + std::to_string((int)m_lastResult.centerY) + ")";
    ImVec2 textPos(centerX + 15, centerY - 25);
    ImVec2 textSize = ImGui::CalcTextSize(coordText.c_str());

    // Text background
    drawList->AddRectFilled(
      ImVec2(textPos.x - 2, textPos.y - 2),
      ImVec2(textPos.x + textSize.x + 2, textPos.y + textSize.y + 2),
      IM_COL32(0, 0, 0, 180)
    );

    // Text
    drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), coordText.c_str());

    // Draw ROI rectangle if parameters are available
    if (m_circleDetector) {
      auto params = m_circleDetector->GetParameters();

      float roiCenterX = screenImagePos.x + ((m_imageWidth / 2.0f + params.roiOffsetX) * scaleX);
      float roiCenterY = screenImagePos.y + ((m_imageHeight / 2.0f + params.roiOffsetY) * scaleY);
      float roiSize = params.roiSize * scaleX;

      ImVec2 roiTopLeft(roiCenterX - roiSize, roiCenterY - roiSize);
      ImVec2 roiBottomRight(roiCenterX + roiSize, roiCenterY + roiSize);

      // Draw ROI rectangle
      ImU32 roiColor = IM_COL32(255, 255, 0, 128); // Yellow ROI
      drawList->AddRect(roiTopLeft, roiBottomRight, roiColor, 0.0f, 0, 1.0f);
    }
  }

  // Legend
  if (m_hasResult && m_lastResult.found) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "● Detected Circle");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "+ Center Point");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "□ ROI");
  }
}

void UIVisionPanel::UpdateImageTexture(const std::vector<uint8_t>& imageData, int width, int height, int channels) {
  // Store image data
  m_lastImageData = imageData;
  m_imageWidth = width;
  m_imageHeight = height;
  m_hasImageData = true;

  // Create or update OpenGL texture
  if (m_imageTextureId == 0) {
    glGenTextures(1, &m_imageTextureId);
  }

  glBindTexture(GL_TEXTURE_2D, m_imageTextureId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Handle different channel formats
  if (channels == 1) {
    // Convert grayscale to RGB for better compatibility
    std::vector<uint8_t> rgbData(width * height * 3);
    for (int i = 0; i < width * height; i++) {
      rgbData[i * 3 + 0] = imageData[i]; // R
      rgbData[i * 3 + 1] = imageData[i]; // G  
      rgbData[i * 3 + 2] = imageData[i]; // B
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbData.data());
  }
  else if (channels == 3) {
    // RGB format
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, imageData.data());
  }
  else if (channels == 4) {
    // RGBA format
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData.data());
  }

  glBindTexture(GL_TEXTURE_2D, 0);

  std::cout << "[UIVisionPanel] Updated image texture: " << width << "x" << height << " (" << channels << " channels)" << std::endl;
}

void UIVisionPanel::CleanupImageTexture() {
  if (m_imageTextureId != 0) {
    glDeleteTextures(1, &m_imageTextureId);
    m_imageTextureId = 0;
    m_hasImageData = false;
    std::cout << "[UIVisionPanel] Cleaned up image texture" << std::endl;
  }
}