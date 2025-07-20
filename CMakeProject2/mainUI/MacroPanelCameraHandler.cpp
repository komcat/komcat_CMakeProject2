// MacroPanelCameraHandler.cpp
#include "MacroPanelCameraHandler.h"
#include "CameraFeedDisplay.h"                      // Fixed: Remove mainUI/ path
#include "include/camera/CameraManager.h"
#include "include/camera/pylon_camera_test.h"
#include <algorithm>
#include <iostream>

MacroPanelCameraHandler::MacroPanelCameraHandler(CameraManager* cameraManager)
  : m_cameraManager(cameraManager) {

  // Initialize camera feed display
  m_cameraFeedDisplay = std::make_unique<CameraFeedDisplay>();

  // Try to get first available camera if camera manager is available
  if (m_cameraManager) {
    auto cameraIds = m_cameraManager->GetCameraIds();
    if (!cameraIds.empty()) {
      m_selectedCameraId = cameraIds[0];
      InitializeCameraFeed();
    }
  }
}

void MacroPanelCameraHandler::SetCameraManager(CameraManager* cameraManager) {
  m_cameraManager = cameraManager;

  // Reset camera state when manager changes
  m_cameraInitialized = false;
  m_selectedCameraId.clear();

  if (m_cameraFeedDisplay) {
    m_cameraFeedDisplay->ClearSource();
  }

  // Try to initialize with first available camera
  if (m_cameraManager) {
    auto cameraIds = m_cameraManager->GetCameraIds();
    if (!cameraIds.empty()) {
      m_selectedCameraId = cameraIds[0];
      InitializeCameraFeed();
    }
  }
}

void MacroPanelCameraHandler::SetSelectedCameraId(const std::string& cameraId) {
  if (m_selectedCameraId != cameraId) {
    m_selectedCameraId = cameraId;
    InitializeCameraFeed();
  }
}

void MacroPanelCameraHandler::InitializeCameraFeed() {
  if (m_selectedCameraId.empty() || !m_cameraManager) {
    m_cameraInitialized = false;
    if (m_cameraFeedDisplay) {
      m_cameraFeedDisplay->ClearSource();
    }
    return;
  }

  PylonCameraTest* camera = m_cameraManager->GetCamera(m_selectedCameraId);
  if (camera && m_cameraFeedDisplay) {
    m_cameraFeedDisplay->SetPylonCameraSource(camera);
    m_cameraInitialized = true;
    std::cout << "Camera feed initialized for: " << m_selectedCameraId << std::endl;
  }
  else {
    m_cameraInitialized = false;
    if (m_cameraFeedDisplay) {
      m_cameraFeedDisplay->ClearSource();
    }
  }
}

void MacroPanelCameraHandler::RenderCameraCanvas() {
  ImGui::Text("Camera Feed");

  // Camera selection and controls
  RenderCameraControls();

  ImGui::Spacing();

  // Calculate canvas size
  ImVec2 canvasSize = CalculateCanvasSize();

  // Create camera canvas
  ImGui::BeginChild("CameraCanvas", canvasSize, false, ImGuiWindowFlags_NoScrollbar);

  // Render camera feed or placeholder
  if (m_cameraFeedDisplay && m_cameraInitialized) {
    RenderCameraFeed(canvasSize);
  }
  else {
    RenderPlaceholder(canvasSize);
  }

  ImGui::EndChild();
}

void MacroPanelCameraHandler::RenderCameraControls() {
  if (!m_cameraManager) {
    ImGui::Text("Camera Manager not available");
    return;
  }

  auto cameraIds = m_cameraManager->GetCameraIds();
  if (!cameraIds.empty()) {
    ImGui::Text("Camera:");

    // Find current selection index
    int currentCameraIndex = 0;
    for (size_t i = 0; i < cameraIds.size(); i++) {
      if (cameraIds[i] == m_selectedCameraId) {
        currentCameraIndex = (int)i;
        break;
      }
    }

    // Create camera selection array
    std::vector<const char*> cameraNames;
    for (const auto& id : cameraIds) {
      cameraNames.push_back(id.c_str());
    }

    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##CameraSelection", &currentCameraIndex, cameraNames.data(), (int)cameraNames.size())) {
      SetSelectedCameraId(cameraIds[currentCameraIndex]);
      std::cout << "Camera selection changed to: " << m_selectedCameraId << std::endl;
    }
  }
  else {
    ImGui::Text("No cameras available");
  }

  // Camera controls for selected camera
  if (!m_selectedCameraId.empty()) {
    ImGui::Spacing();
    PylonCameraTest* camera = m_cameraManager->GetCamera(m_selectedCameraId);
    if (camera) {
      auto& pylonCamera = camera->GetCamera();

      // Connection status
      ImGui::Text("Status: %s", pylonCamera.IsConnected() ? "Connected" : "Disconnected");

      if (pylonCamera.IsConnected()) {
        ImGui::SameLine();
        ImGui::Text("| Grabbing: %s", pylonCamera.IsGrabbing() ? "Yes" : "No");

        // Start/Stop grabbing controls
        if (!pylonCamera.IsGrabbing()) {
          if (ImGui::Button("Start Live Feed", ImVec2(120, 25))) {
            m_cameraManager->StartGrabbing(m_selectedCameraId);
            std::cout << "Started grabbing for camera: " << m_selectedCameraId << std::endl;
          }
        }
        else {
          if (ImGui::Button("Stop Live Feed", ImVec2(120, 25))) {
            m_cameraManager->StopGrabbing(m_selectedCameraId);
            std::cout << "Stopped grabbing for camera: " << m_selectedCameraId << std::endl;
          }
        }

        ImGui::SameLine();
        if (ImGui::Button("Reconnect", ImVec2(80, 25))) {
          m_cameraManager->ConnectCamera(m_selectedCameraId);
          std::cout << "Reconnect camera: " << m_selectedCameraId << std::endl;
        }
      }
      else {
        if (ImGui::Button("Connect Camera", ImVec2(120, 25))) {
          m_cameraManager->ConnectCamera(m_selectedCameraId);
          InitializeCameraFeed();
          std::cout << "Connecting camera: " << m_selectedCameraId << std::endl;
        }
      }
    }
  }
}

void MacroPanelCameraHandler::RenderCameraFeed(ImVec2 canvasSize) {
  // Render live camera feed
  m_cameraFeedDisplay->RenderToCanvas(canvasSize.x, canvasSize.y);
}

void MacroPanelCameraHandler::RenderPlaceholder(ImVec2 canvasSize) {
  // Show placeholder when no camera feed
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();
  ImVec2 canvasMax = ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);

  // Background
  drawList->AddRectFilled(canvasPos, canvasMax, IM_COL32(80, 80, 80, 255));
  drawList->AddRect(canvasPos, canvasMax, IM_COL32(100, 150, 200, 255), 0.0f, 0, 2.0f);

  // Status text
  std::string statusText = "Live Camera Feed\n(No camera initialized)";
  if (!m_selectedCameraId.empty()) {
    statusText += "\nCamera: " + m_selectedCameraId;
  }

  ImVec2 textSize = ImGui::CalcTextSize(statusText.c_str());
  ImVec2 textPos = ImVec2(
    canvasPos.x + (canvasSize.x - textSize.x) * 0.5f,
    canvasPos.y + (canvasSize.y - textSize.y) * 0.5f
  );
  drawList->AddText(textPos, IM_COL32(200, 200, 200, 255), statusText.c_str());
}

ImVec2 MacroPanelCameraHandler::CalculateCanvasSize() {
  // Camera canvas area with fixed aspect ratio
  const float CAMERA_ASPECT_RATIO = 1280.0f / 1024.0f; // 5:4 aspect ratio

  float availableWidth = ImGui::GetContentRegionAvail().x;
  float availableHeight = ImGui::GetContentRegionAvail().y - 10.0f; // Small margin

  // Calculate canvas size maintaining aspect ratio
  ImVec2 canvasSize;
  canvasSize.x = availableWidth;
  canvasSize.y = availableWidth / CAMERA_ASPECT_RATIO;

  // Ensure it fits in available height
  if (canvasSize.y > availableHeight) {
    canvasSize.y = availableHeight;
    canvasSize.x = availableHeight * CAMERA_ASPECT_RATIO;
  }

  // Ensure minimum size
  if (canvasSize.x < 160.0f) {
    canvasSize.x = 160.0f;
    canvasSize.y = 160.0f / CAMERA_ASPECT_RATIO;
  }

  return canvasSize;
}