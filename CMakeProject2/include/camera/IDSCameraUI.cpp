#include "IDSCameraUI.h"
#include <GL/gl.h>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>

IDSCameraUI::IDSCameraUI()
  : m_camera(std::make_unique<IDSCameraTest>()),
  m_isVisible(true),
  m_selectedCameraIdIndex(0),
  m_textureID(0),
  m_textureInitialized(false),
  m_imageTextureWidth(0),
  m_imageTextureHeight(0),
  m_autoSaveWithTimestamp(true),
  m_autoScrollStatusLog(true) {

  // Initialize filename buffer
  strcpy(m_saveFilename, "capture.bmp");

  // Set up camera status callback
  m_camera->SetStatusCallback([this](const std::string& message) {
    OnCameraStatus(message);
  });

  // Refresh camera list
  RefreshCameraList();
}

IDSCameraUI::~IDSCameraUI() {
  CleanupTexture();
}

void IDSCameraUI::RenderUI() {
  if (!m_isVisible) return;

  ImGui::Begin("IDS Camera Test", &m_isVisible, ImGuiWindowFlags_AlwaysAutoResize);

  // Camera selection and connection
  RenderCameraSelection();
  ImGui::Separator();

  RenderConnectionControls();
  ImGui::Separator();

  // Only show camera controls if connected
  if (m_camera->IsConnected()) {
    RenderCameraInfo();
    ImGui::Separator();

    RenderImageDisplay();
    ImGui::Separator();

    RenderCaptureControls();
    ImGui::Separator();

    RenderLiveControls();
    ImGui::Separator();
  }

  RenderStatusLog();

  ImGui::End();
}

void IDSCameraUI::RenderCameraSelection() {
  ImGui::Text("Available Cameras:");

  ImGui::SameLine();
  if (ImGui::Button("Refresh")) {
    RefreshCameraList();
  }

  ImGui::Text("Found %zu cameras", m_availableCameraIds.size());
}

void IDSCameraUI::RenderConnectionControls() {
  ImGui::Text("Connect by Camera ID:");

  if (!m_availableCameraIds.empty()) {
    // Create dropdown for camera IDs
    std::vector<std::string> idStrings;
    for (int id : m_availableCameraIds) {
      idStrings.push_back("Camera ID: " + std::to_string(id));
    }

    std::vector<const char*> idCStrings;
    for (const auto& str : idStrings) {
      idCStrings.push_back(str.c_str());
    }

    ImGui::SetNextItemWidth(200);
    ImGui::Combo("##CameraID", &m_selectedCameraIdIndex, idCStrings.data(), idCStrings.size());
    ImGui::SameLine();

    if (ImGui::Button("Connect") && !m_camera->IsConnected()) {
      ConnectBySelectedId();
    }
  }
  else {
    ImGui::Text("No cameras detected");
    if (ImGui::Button("Connect to ID 0 (fallback)") && !m_camera->IsConnected()) {
      m_camera->ConnectById(0);
    }
    ImGui::SameLine();
    if (ImGui::Button("Connect to ID 1 (fallback)") && !m_camera->IsConnected()) {
      m_camera->ConnectById(1);
    }
  }

  // Disconnect button
  ImGui::Separator();
  if (ImGui::Button("Disconnect", ImVec2(100, 30)) && m_camera->IsConnected()) {
    m_camera->Disconnect();
    CleanupTexture();
  }
}

void IDSCameraUI::RenderCameraInfo() {
  ImGui::Text("Camera Information:");
  std::string info = m_camera->GetCameraInfo();
  ImGui::TextWrapped("%s", info.c_str());
}

void IDSCameraUI::RenderImageDisplay() {
  ImGui::Text("Image Display:");

  // Calculate canvas size 
  ImVec2 canvasSize = ImGui::GetContentRegionAvail();
  canvasSize.y = (std::max)(canvasSize.y - 50.0f, 200.0f); // Leave space for buttons below

  // Update texture if new frame available
  if (m_camera->HasNewFrame()) {
    UpdateImageTexture();
    m_camera->MarkFrameProcessed();
  }

  // Display image if texture is available
  if (m_textureInitialized && m_textureID != 0) {
    // Verify texture is still valid in OpenGL
    GLboolean isValid = glIsTexture(m_textureID);
    if (!isValid) {
      std::cout << "[ERROR] Texture ID " << m_textureID << " is no longer valid in OpenGL!" << std::endl;
      RenderErrorCanvas(canvasSize.x, canvasSize.y, "Texture Error\nTexture became invalid");
      return;
    }

    // Calculate display size while maintaining aspect ratio
    float aspectRatio = (float)m_imageTextureWidth / (float)m_imageTextureHeight;

    float displayWidth = canvasSize.x;
    float displayHeight = displayWidth / aspectRatio;

    // If height is too big, fit by height instead
    if (displayHeight > canvasSize.y) {
      displayHeight = canvasSize.y;
      displayWidth = displayHeight * aspectRatio;
    }

    // Center the image
    float offsetX = (canvasSize.x - displayWidth) * 0.5f;
    float offsetY = (canvasSize.y - displayHeight) * 0.5f;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);

    // Display the image
    ImGui::Image((ImTextureID)(intptr_t)m_textureID, ImVec2(displayWidth, displayHeight));

    // Add tooltip on hover
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("IDS Camera Image\nTexture ID: %u\nResolution: %ux%u\nBits per Pixel: %d",
        m_textureID, m_imageTextureWidth, m_imageTextureHeight,
        m_camera->GetImageBitsPerPixel());
    }

    // Show image info below
    ImGui::Text("Image: %dx%d, %d bpp",
      m_camera->GetImageWidth(),
      m_camera->GetImageHeight(),
      m_camera->GetImageBitsPerPixel());
  }
  else {
    // Show placeholder when no image is available
    std::string errorText;
    if (!m_textureInitialized) {
      errorText = "No Image Available\nCapture an image to see it here";
    }
    else {
      errorText = "Invalid Image Data\nCheck camera connection";
    }

    RenderErrorCanvas(canvasSize.x, canvasSize.y, errorText);
    ImGui::Text("No image available - capture an image first");
  }
}

void IDSCameraUI::RenderErrorCanvas(float width, float height, const std::string& errorText) {
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();
  ImDrawList* drawList = ImGui::GetWindowDrawList();

  // Error canvas - dark background
  drawList->AddRectFilled(canvasPos,
    ImVec2(canvasPos.x + width, canvasPos.y + height),
    IM_COL32(40, 40, 40, 255));

  // Error border
  drawList->AddRect(canvasPos,
    ImVec2(canvasPos.x + width, canvasPos.y + height),
    IM_COL32(100, 100, 100, 255));

  // Error text
  ImVec2 textSize = ImGui::CalcTextSize(errorText.c_str());
  ImVec2 textPos = ImVec2(canvasPos.x + (width - textSize.x) * 0.5f,
    canvasPos.y + (height - textSize.y) * 0.5f);
  drawList->AddText(textPos, IM_COL32(200, 200, 200, 255), errorText.c_str());

  // Advance cursor
  ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, canvasPos.y + height));
}

void IDSCameraUI::RenderCaptureControls() {
  ImGui::Text("Single Image Capture:");

  // Filename input
  ImGui::Text("Filename:");
  ImGui::SameLine();
  ImGui::Checkbox("Auto timestamp", &m_autoSaveWithTimestamp);

  if (!m_autoSaveWithTimestamp) {
    ImGui::SetNextItemWidth(200);
    ImGui::InputText("##filename", m_saveFilename, sizeof(m_saveFilename));
  }

  // Capture buttons
  if (ImGui::Button("Capture to Memory", ImVec2(150, 30))) {
    if (m_camera->CaptureImage()) {
      AddStatusMessage("Image captured to memory");
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Capture to Disk", ImVec2(150, 30))) {
    std::string filename = m_autoSaveWithTimestamp ? "" : m_saveFilename;
    if (m_camera->CaptureImageToDisk(filename)) {
      AddStatusMessage("Image captured and saved to disk");
    }
  }
}

void IDSCameraUI::RenderLiveControls() {
  ImGui::Text("Live Video Feed:");

  bool isGrabbing = m_camera->IsGrabbing();

  // Start/Stop grabbing buttons
  if (!isGrabbing) {
    if (ImGui::Button("Start Grabbing", ImVec2(120, 30))) {
      if (m_camera->StartGrabbing()) {
        AddStatusMessage("Started live grabbing");
      }
    }
  }
  else {
    if (ImGui::Button("Stop Grabbing", ImVec2(120, 30))) {
      if (m_camera->StopGrabbing()) {
        AddStatusMessage("Stopped live grabbing");
      }
    }
  }

  ImGui::SameLine();
  ImGui::Text("Status: %s", isGrabbing ? "Grabbing" : "Stopped");
}

void IDSCameraUI::RenderStatusLog() {
  ImGui::Text("Status Log:");
  ImGui::Checkbox("Auto-scroll", &m_autoScrollStatusLog);

  ImGui::BeginChild("StatusLog", ImVec2(500, 150), true, ImGuiWindowFlags_HorizontalScrollbar);

  for (const auto& message : m_statusMessages) {
    ImGui::TextWrapped("%s", message.c_str());
  }

  if (m_autoScrollStatusLog && !m_statusMessages.empty()) {
    ImGui::SetScrollHereY(1.0f);
  }

  ImGui::EndChild();

  if (ImGui::Button("Clear Log")) {
    m_statusMessages.clear();
  }
}

void IDSCameraUI::RefreshCameraList() {
  m_availableCameraIds = IDSCameraTest::GetAvailableCameraIds();
  m_selectedCameraIdIndex = 0;

  AddStatusMessage("Refreshed camera list - found " + std::to_string(m_availableCameraIds.size()) + " cameras");
}

void IDSCameraUI::ConnectBySelectedId() {
  if (m_selectedCameraIdIndex >= 0 && m_selectedCameraIdIndex < m_availableCameraIds.size()) {
    int cameraId = m_availableCameraIds[m_selectedCameraIdIndex];
    m_camera->ConnectById(cameraId);
  }
}

void IDSCameraUI::UpdateImageTexture() {
  const char* imageData = m_camera->GetImageData();
  int width = m_camera->GetImageWidth();
  int height = m_camera->GetImageHeight();
  int bpp = m_camera->GetImageBitsPerPixel();

  if (!imageData || width <= 0 || height <= 0) {
    return;
  }

  // Create texture if not initialized or size changed
  if (!m_textureInitialized || m_imageTextureWidth != width || m_imageTextureHeight != height) {
    CleanupTexture();

    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    m_imageTextureWidth = width;
    m_imageTextureHeight = height;
    m_textureInitialized = true;

    std::cout << "[INFO] Created OpenGL texture " << m_textureID << " for IDS camera ("
      << width << "x" << height << ", " << bpp << " bpp)" << std::endl;
  }

  // Update texture data
  glBindTexture(GL_TEXTURE_2D, m_textureID);

  if (bpp == 8) {
    // Mono8 - convert to RGB by replicating the single channel to all 3 channels
    std::vector<uint8_t> rgbData(width * height * 3);
    const uint8_t* srcData = reinterpret_cast<const uint8_t*>(imageData);

    for (int i = 0; i < width * height; i++) {
      uint8_t grayValue = srcData[i];
      rgbData[i * 3 + 0] = grayValue; // R
      rgbData[i * 3 + 1] = grayValue; // G  
      rgbData[i * 3 + 2] = grayValue; // B
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbData.data());
    std::cout << "[DEBUG] Uploaded mono8 image as RGB texture" << std::endl;
  }
  else if (bpp == 24) {
    // RGB24 - direct upload
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, imageData);
    std::cout << "[DEBUG] Uploaded RGB24 image" << std::endl;
  }
  else if (bpp == 32) {
    // RGBA32 - direct upload
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData);
    std::cout << "[DEBUG] Uploaded RGBA32 image" << std::endl;
  }
  else {
    std::cout << "[ERROR] Unsupported bits per pixel: " << bpp << std::endl;
  }

  glBindTexture(GL_TEXTURE_2D, 0);
}

void IDSCameraUI::AddStatusMessage(const std::string& message) {
  // Add timestamp to message
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S") << "] " << message;

  m_statusMessages.push_back(ss.str());

  // Limit the number of messages
  if (m_statusMessages.size() > MAX_STATUS_MESSAGES) {
    m_statusMessages.erase(m_statusMessages.begin());
  }
}

void IDSCameraUI::CleanupTexture() {
  if (m_textureInitialized && m_textureID != 0) {
    glDeleteTextures(1, &m_textureID);
    m_textureID = 0;
  }
  m_textureInitialized = false;
  m_imageTextureWidth = 0;
  m_imageTextureHeight = 0;
}

void IDSCameraUI::OnCameraStatus(const std::string& message) {
  AddStatusMessage(message);
}