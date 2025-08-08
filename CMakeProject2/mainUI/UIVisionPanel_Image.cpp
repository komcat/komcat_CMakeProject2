// UIVisionPanel_Image.cpp - Image display and exposure controls
#include "UIVisionPanel.h"
#include "include/camera/CameraManager.h"
#include "include/camera/ICameraHardware.h"
#include <iostream>

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


void UIVisionPanel::RenderImageDisplay() {
  ImGui::Text("Image Display");
  ImGui::Separator();

  // Calculate available space, reserving room for exposure controls
  ImVec2 availableSize = ImGui::GetContentRegionAvail();
  float exposureControlsHeight = 120.0f; // Reserve space for exposure controls
  ImVec2 imageAreaSize = ImVec2(availableSize.x, availableSize.y - exposureControlsHeight);

  if (!m_hasImageData || m_imageTextureId == 0) {
    ImGui::BeginChild("ImagePlaceholder", imageAreaSize, true);

    ImVec2 centerPos = ImVec2(imageAreaSize.x * 0.5f - 60, imageAreaSize.y * 0.5f - 10);
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
  }
  else {
    // Render image with the reserved space
    ImGui::BeginChild("ImageArea", imageAreaSize, false);
    RenderImageWithOverlay();
    ImGui::EndChild();

    ImGui::Text("Image size: %dx%d (%d channels)", m_imageWidth, m_imageHeight,
      m_lastImageData.size() / (m_imageWidth * m_imageHeight));
  }

  // Add exposure controls underneath the image
  ImGui::Spacing();
  ImGui::Separator();
  RenderExposureControls();
}



void UIVisionPanel::RenderExposureControls() {
  ImGui::Text("Camera Exposure Controls");

  // Check if camera is available
  ICameraHardware* camera = nullptr;
  if (m_cameraManager && !m_selectedCameraId.empty()) {
    camera = m_cameraManager->GetCameraHardware(m_selectedCameraId);
  }

  if (!camera || !camera->IsConnected()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No camera connected");
    return;
  }

  // Update UI from camera settings periodically
  static auto lastUpdate = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count() > 1000) {
    UpdateExposureUIFromCamera();
    lastUpdate = now;
  }

  // Exposure time control
  ImGui::Text("Exposure Time (μs):");
  ImGui::SetNextItemWidth(200);
  if (ImGui::SliderFloat("##ExposureTime", &m_exposureTimeUI, 100.0f, 100000.0f, "%.0f")) {
    ApplyExposureSettings();
  }

  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, m_autoExposureUI ?
    ImVec4(0.0f, 0.7f, 0.0f, 1.0f) : ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
  if (ImGui::Button("Auto##Exposure", ImVec2(50, 0))) {
    m_autoExposureUI = !m_autoExposureUI;
    ApplyExposureSettings();
  }
  ImGui::PopStyleColor();

  // Gain control
  ImGui::Text("Gain:");
  ImGui::SetNextItemWidth(200);
  if (ImGui::SliderFloat("##Gain", &m_gainUI, 0.0f, 10.0f, "%.1f")) {
    ApplyExposureSettings();
  }

  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, m_autoGainUI ?
    ImVec4(0.0f, 0.7f, 0.0f, 1.0f) : ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
  if (ImGui::Button("Auto##Gain", ImVec2(50, 0))) {
    m_autoGainUI = !m_autoGainUI;
    ApplyExposureSettings();
  }
  ImGui::PopStyleColor();

  // Quick preset buttons
  ImGui::Spacing();
  if (ImGui::Button("Dark (Fast)", ImVec2(100, 25))) {
    m_exposureTimeUI = 1000.0f;
    m_gainUI = 1.0f;
    m_autoExposureUI = false;
    m_autoGainUI = false;
    ApplyExposureSettings();
  }

  ImGui::SameLine();
  if (ImGui::Button("Normal", ImVec2(100, 25))) {
    m_exposureTimeUI = 10000.0f;
    m_gainUI = 1.0f;
    m_autoExposureUI = false;
    m_autoGainUI = false;
    ApplyExposureSettings();
  }

  ImGui::SameLine();
  if (ImGui::Button("Bright (Slow)", ImVec2(100, 25))) {
    m_exposureTimeUI = 50000.0f;
    m_gainUI = 2.0f;
    m_autoExposureUI = false;
    m_autoGainUI = false;
    ApplyExposureSettings();
  }
}

void UIVisionPanel::ApplyExposureSettings() {
  if (!m_cameraManager || m_selectedCameraId.empty()) {
    return;
  }

  ICameraHardware* camera = m_cameraManager->GetCameraHardware(m_selectedCameraId);
  if (!camera || !camera->IsConnected()) {
    return;
  }

  try {
    ICameraHardware::ExposureSettings settings;
    settings.exposure_time = static_cast<double>(m_exposureTimeUI);
    settings.gain = static_cast<double>(m_gainUI);
    settings.auto_exposure = m_autoExposureUI;
    settings.auto_gain = m_autoGainUI;

    if (m_cameraManager->ApplyExposureSettings(m_selectedCameraId, settings)) {
      std::cout << "[UIVisionPanel] Applied exposure settings: exp=" << settings.exposure_time
        << "μs, gain=" << settings.gain << std::endl;
    }
    else {
      std::cout << "[UIVisionPanel] Failed to apply exposure settings" << std::endl;
    }
  }
  catch (const std::exception& e) {
    std::cout << "[UIVisionPanel] Exception applying exposure settings: " << e.what() << std::endl;
  }
}

void UIVisionPanel::UpdateExposureUIFromCamera() {
  if (!m_cameraManager || m_selectedCameraId.empty()) {
    return;
  }

  ICameraHardware* camera = m_cameraManager->GetCameraHardware(m_selectedCameraId);
  if (!camera || !camera->IsConnected()) {
    return;
  }

  try {
    auto settings = camera->GetExposureSettings();
    m_exposureTimeUI = static_cast<float>(settings.exposure_time);
    m_gainUI = static_cast<float>(settings.gain);
    m_autoExposureUI = settings.auto_exposure;
    m_autoGainUI = settings.auto_gain;
  }
  catch (const std::exception& e) {
    // Silently ignore errors - camera may not be fully ready
  }
}




void UIVisionPanel::RenderImageWithOverlay() {
  if (m_imageTextureId == 0 || m_imageWidth == 0 || m_imageHeight == 0) {
    return;
  }

  ImVec2 availableSize = ImGui::GetContentRegionAvail();
  availableSize.y -= 30; // Reserve space for image size text

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

  ImVec2 imagePos = ImVec2(
    (availableSize.x - displaySize.x) * 0.5f,
    (availableSize.y - displaySize.y) * 0.5f
  );

  ImGui::SetCursorPos(imagePos);
  ImVec2 screenImagePos = ImGui::GetCursorScreenPos();

  ImGui::Image((ImTextureID)(intptr_t)m_imageTextureId, displaySize);

  // Draw detection overlays if we have results
  if (m_hasResult && m_lastResult.found) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float scaleX = displaySize.x / m_imageWidth;
    float scaleY = displaySize.y / m_imageHeight;

    float centerX = screenImagePos.x + (m_lastResult.centerX * scaleX);
    float centerY = screenImagePos.y + (m_lastResult.centerY * scaleY);
    float radius = m_lastResult.radius * scaleX;

    // Draw detected circle
    ImU32 circleColor = IM_COL32(255, 0, 0, 255);
    drawList->AddCircle(ImVec2(centerX, centerY), radius, circleColor, 64, 2.0f);

    // Draw center crosshair
    ImU32 crosshairColor = IM_COL32(0, 255, 0, 255);
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

    drawList->AddCircleFilled(ImVec2(centerX, centerY), 3.0f, crosshairColor);

    // Draw coordinate text
    std::string coordText = "(" + std::to_string((int)m_lastResult.centerX) +
      ", " + std::to_string((int)m_lastResult.centerY) + ")";
    ImVec2 textPos(centerX + 15, centerY - 25);
    ImVec2 textSize = ImGui::CalcTextSize(coordText.c_str());

    drawList->AddRectFilled(
      ImVec2(textPos.x - 2, textPos.y - 2),
      ImVec2(textPos.x + textSize.x + 2, textPos.y + textSize.y + 2),
      IM_COL32(0, 0, 0, 180)
    );

    drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), coordText.c_str());

    // Draw ROI if circle detector is available
    if (m_circleDetector) {
      auto params = m_circleDetector->GetParameters();

      float roiCenterX = screenImagePos.x + ((m_imageWidth / 2.0f + params.roiOffsetX) * scaleX);
      float roiCenterY = screenImagePos.y + ((m_imageHeight / 2.0f + params.roiOffsetY) * scaleY);
      float roiSize = params.roiSize * scaleX;

      ImVec2 roiTopLeft(roiCenterX - roiSize, roiCenterY - roiSize);
      ImVec2 roiBottomRight(roiCenterX + roiSize, roiCenterY + roiSize);

      ImU32 roiColor = IM_COL32(255, 255, 0, 128);
      drawList->AddRect(roiTopLeft, roiBottomRight, roiColor, 0.0f, 0, 1.0f);
    }
  }

  // Draw overlay legend
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
  m_lastImageData = imageData;
  m_imageWidth = width;
  m_imageHeight = height;
  m_hasImageData = true;

  if (m_imageTextureId == 0) {
    glGenTextures(1, &m_imageTextureId);
  }

  glBindTexture(GL_TEXTURE_2D, m_imageTextureId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  if (channels == 1) {
    // Convert grayscale to RGB
    std::vector<uint8_t> rgbData(width * height * 3);
    for (int i = 0; i < width * height; i++) {
      rgbData[i * 3 + 0] = imageData[i];
      rgbData[i * 3 + 1] = imageData[i];
      rgbData[i * 3 + 2] = imageData[i];
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbData.data());
  }
  else if (channels == 3) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, imageData.data());
  }
  else if (channels == 4) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData.data());
  }

  glBindTexture(GL_TEXTURE_2D, 0);
}

void UIVisionPanel::CleanupImageTexture() {
  if (m_imageTextureId != 0) {
    glDeleteTextures(1, &m_imageTextureId);
    m_imageTextureId = 0;
    m_hasImageData = false;
    std::cout << "[UIVisionPanel] Cleaned up image texture" << std::endl;
  }
}
