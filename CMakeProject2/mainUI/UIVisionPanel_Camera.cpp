// UIVisionPanel_Camera.cpp - Camera integration and UI
#include "UIVisionPanel.h"
#include "include/camera/CameraManager.h"
#include "include/camera/ICameraHardware.h"
#include "include/camera/CameraFrameData.h"
#include <iostream>

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

  ICameraHardware* camera = m_cameraManager->GetCameraHardware(m_selectedCameraId);
  if (!camera || !camera->IsConnected()) {
    std::cout << "[UIVisionPanel] Camera not connected: " << m_selectedCameraId << std::endl;
    return false;
  }

  CameraFrameData frameData;
  if (!camera->CaptureFrame(frameData)) {
    std::cout << "[UIVisionPanel] Failed to capture frame" << std::endl;
    return false;
  }

  if (!frameData.IsValid() || frameData.imageData.empty()) {
    std::cout << "[UIVisionPanel] Invalid frame data" << std::endl;
    return false;
  }

  width = frameData.width;
  height = frameData.height;
  channels = frameData.channels;
  imageBuffer = frameData.imageData;

  UpdateImageTexture(imageBuffer, width, height, channels);

  return true;
}