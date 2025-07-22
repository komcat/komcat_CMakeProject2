#include "RaylibDebugWindow.h"
#include "include/camera/CameraManager.h"
#include "raylibclass.h"
#include "CameraFeedDisplay.h"
#include "include/logger.h"
#include "imgui.h"
#include <thread>
#include <chrono>

RaylibDebugWindow::RaylibDebugWindow() {
}

RaylibDebugWindow::~RaylibDebugWindow() {
}

void RaylibDebugWindow::SetCameraManager(CameraManager* cameraManager) {
  m_cameraManager = cameraManager;
}

void RaylibDebugWindow::SetRaylibWindow(RaylibWindow* raylibWindow) {
  m_raylibWindow = raylibWindow;
}

void RaylibDebugWindow::SetCameraFeedDisplay(CameraFeedDisplay* cameraFeed) {
  m_cameraFeedDisplay = cameraFeed;
}

void RaylibDebugWindow::SetLogger(Logger* logger) {
  m_logger = logger;
}

void RaylibDebugWindow::RenderUI() {
  if (!m_cameraManager || !m_raylibWindow) {
    ImGui::Text("RaylibDebugWindow: Missing required components");
    return;
  }

  ImGui::Text("Raylib Live Feed Debug");
  ImGui::Separator();

  // Split into sections
  if (ImGui::CollapsingHeader("Camera Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
    RenderCameraSelection();
  }

  if (ImGui::CollapsingHeader("Camera Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
    RenderCameraControls();
  }

  if (ImGui::CollapsingHeader("Feed Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
    RenderFeedControls();
  }

  if (ImGui::CollapsingHeader("Debug Information")) {
    RenderDebugInfo();
  }

  if (ImGui::CollapsingHeader("Quick Actions")) {
    RenderQuickActions();
  }
}

void RaylibDebugWindow::RenderCameraSelection() {
  auto cameraIds = m_cameraManager->GetCameraIds();

  if (cameraIds.empty()) {
    ImGui::Text("No cameras available");
    if (ImGui::Button("Refresh Camera List")) {
      m_cameraManager->InitializeAllCameras();
    }
    return;
  }

  // Initialize selection if empty
  if (m_selectedCameraId.empty()) {
    m_selectedCameraId = cameraIds[0];
  }

  ImGui::Text("Select Camera:");
  if (ImGui::BeginCombo("##CameraSelection", m_selectedCameraId.c_str())) {
    for (const auto& cameraId : cameraIds) {
      bool isSelected = (m_selectedCameraId == cameraId);
      auto status = m_cameraManager->GetCameraStatus(cameraId);

      std::string displayName = cameraId;
      if (status.connected) {
        displayName += " (Connected)";
      }
      else {
        displayName += " (Disconnected)";
      }

      if (ImGui::Selectable(displayName.c_str(), isSelected)) {
        m_selectedCameraId = cameraId;

        // Connect the new camera to feed
        PylonCameraTest* newCamera = m_cameraManager->GetCamera(m_selectedCameraId);
        if (newCamera && m_cameraFeedDisplay) {
          m_cameraFeedDisplay->SetPylonCameraSource(newCamera);
          if (m_logger) {
            m_logger->LogInfo("Switched raylib camera feed to: " + m_selectedCameraId);
          }
        }
      }

      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
}

void RaylibDebugWindow::RenderCameraControls() {
  if (m_selectedCameraId.empty()) return;

  auto status = m_cameraManager->GetCameraStatus(m_selectedCameraId);

  ImGui::Text("Camera Status:");
  ImGui::SameLine();
  if (status.connected) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Connected");
  }
  else {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Disconnected");
  }

  // Connect/Disconnect button
  if (!status.connected) {
    if (ImGui::Button("Connect Camera")) {
      if (m_cameraManager->ConnectCamera(m_selectedCameraId)) {
        if (m_logger) m_logger->LogInfo("Connected camera: " + m_selectedCameraId);
      }
      else {
        if (m_logger) m_logger->LogError("Failed to connect camera: " + m_selectedCameraId);
      }
    }
  }
  else {
    if (ImGui::Button("Disconnect Camera")) {
      m_cameraManager->StopGrabbing(m_selectedCameraId);
      m_cameraManager->DisconnectCamera(m_selectedCameraId);
      if (m_logger) m_logger->LogInfo("Disconnected camera: " + m_selectedCameraId);
    }
  }

  // Grabbing controls (only if connected)
  if (status.connected) {
    ImGui::Text("Video Feed:");
    ImGui::SameLine();
    if (status.grabbing) {
      ImGui::TextColored(ImVec4(0, 1, 0, 1), "Active");
    }
    else {
      ImGui::TextColored(ImVec4(1, 1, 0, 1), "Stopped");
    }

    if (!status.grabbing) {
      if (ImGui::Button("Start Video Feed")) {
        if (m_cameraManager->StartGrabbing(m_selectedCameraId)) {
          if (m_logger) m_logger->LogInfo("Started video feed for: " + m_selectedCameraId);
        }
      }
    }
    else {
      if (ImGui::Button("Stop Video Feed")) {
        m_cameraManager->StopGrabbing(m_selectedCameraId);
        if (m_logger) m_logger->LogInfo("Stopped video feed for: " + m_selectedCameraId);
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("Capture Image")) {
      if (m_cameraManager->CaptureImage(m_selectedCameraId)) {
        if (m_logger) m_logger->LogInfo("Captured image from: " + m_selectedCameraId);
      }
    }
  }
}

void RaylibDebugWindow::RenderFeedControls() {
  ImGui::Text("3D Window Display:");
  bool feedVisible = m_raylibWindow->IsCameraFeedVisible();
  if (ImGui::Checkbox("Show Feed in 3D Window", &feedVisible)) {
    m_raylibWindow->SetCameraFeedVisible(feedVisible);
    if (m_logger) {
      m_logger->LogInfo("3D camera feed " + std::string(feedVisible ? "enabled" : "disabled"));
    }
  }

  if (m_cameraFeedDisplay && m_cameraFeedDisplay->HasValidTexture()) {
    ImGui::Text("Resolution: %dx%d",
      m_cameraFeedDisplay->GetTextureWidth(),
      m_cameraFeedDisplay->GetTextureHeight());

    bool isReceiving = m_cameraFeedDisplay->IsReceivingFrames();
    ImGui::Text("Receiving Frames: ");
    ImGui::SameLine();
    if (isReceiving) {
      ImGui::TextColored(ImVec4(0, 1, 0, 1), "Yes");
    }
    else {
      ImGui::TextColored(ImVec4(1, 1, 0, 1), "No");
    }

    // Transparency control
    static float alpha = 0.8f;
    if (ImGui::SliderFloat("Feed Transparency", &alpha, 0.1f, 1.0f)) {
      m_raylibWindow->SetCameraFeedAlpha(alpha);
    }
  }

  if (m_cameraFeedDisplay) {
    ImGui::Text("Feed Status: %s", m_cameraFeedDisplay->GetStatusText().c_str());
  }
}

void RaylibDebugWindow::RenderDebugInfo() {
  // Your existing detailed debug code goes here
  // This is where all the texture debugging, OpenGL validation, etc. goes
  ImGui::Text("=== DETAILED TEXTURE DEBUG ===");

  PylonCameraTest* camera = m_cameraManager->GetCamera(m_selectedCameraId);
  if (camera && m_cameraFeedDisplay) {
    unsigned int camTextureID = camera->GetTextureID();
    bool camHasTexture = camera->HasValidTexture();
    unsigned int feedTextureID = m_cameraFeedDisplay->GetTextureID();
    bool feedHasTexture = m_cameraFeedDisplay->HasValidTexture();

    ImGui::Text("Camera Texture ID: %u", camTextureID);
    ImGui::Text("Camera Has Texture: %s", camHasTexture ? "Yes" : "No");
    ImGui::Text("Feed Texture ID: %u", feedTextureID);
    ImGui::Text("Feed Has Texture: %s", feedHasTexture ? "Yes" : "No");
    ImGui::Text("IDs Match: %s", (camTextureID == feedTextureID) ? "Yes" : "No");

    // Add your other debug information here...
  }

  ImGui::Separator();
  ImGui::Text("=== RAYLIB THREAD DEBUG ===");

  ImGui::Text("Raylib Window Running: %s", m_raylibWindow->IsRunning() ? "Yes" : "No");
  ImGui::Text("Raylib Window Visible: %s", m_raylibWindow->IsVisible() ? "Yes" : "No");
  ImGui::Text("Raylib Has Camera Feed: %s", m_raylibWindow->HasCameraFeed() ? "Yes" : "No");
  ImGui::Text("Raylib Feed Visible: %s", m_raylibWindow->IsCameraFeedVisible() ? "Yes" : "No");
}

void RaylibDebugWindow::RenderQuickActions() {
  if (ImGui::Button("Quick Start: Connect & Start Feed")) {
    auto cameraIds = m_cameraManager->GetCameraIds();
    if (!cameraIds.empty()) {
      std::string targetCamera = m_selectedCameraId.empty() ? cameraIds[0] : m_selectedCameraId;

      if (m_cameraManager->ConnectCamera(targetCamera)) {
        if (m_logger) m_logger->LogInfo("Connected camera: " + targetCamera);

        if (m_cameraManager->StartGrabbing(targetCamera)) {
          if (m_logger) m_logger->LogInfo("Started video feed for: " + targetCamera);

          PylonCameraTest* camera = m_cameraManager->GetCamera(targetCamera);
          if (camera && m_cameraFeedDisplay) {
            m_cameraFeedDisplay->SetPylonCameraSource(camera);
            m_raylibWindow->SetCameraFeedVisible(true);
            if (m_logger) m_logger->LogInfo("Camera feed connected to 3D window");
          }
        }
      }
    }
  }

  if (ImGui::Button("Complete Refresh Chain")) {
    // Your refresh logic here...
    if (m_logger) {
      m_logger->LogInfo("=== COMPLETE REFRESH CHAIN ===");
      // Add your refresh steps here...
      m_logger->LogInfo("=== END REFRESH CHAIN ===");
    }
  }
}