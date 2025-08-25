// UICameraPanelUtility.cpp - Camera Utility and Control Panel Implementation
#include "UICameraPanelUtility.h"
#include "include/camera/CameraManager.h"
#include "include/camera/ICameraHardware.h"
#include "imgui.h"
#include <iostream>

UICameraPanelUtility::UICameraPanelUtility(CameraManager& cameraManager)
  : m_cameraManager(cameraManager), m_lastErrorTime(std::chrono::steady_clock::now()) {
  std::cout << "[INFO] UICameraPanelUtility created with ICameraHardware interface" << std::endl;
}

UICameraPanelUtility::~UICameraPanelUtility() {
  ClearCamera();
  std::cout << "[INFO] UICameraPanelUtility destroyed" << std::endl;
}

bool UICameraPanelUtility::ValidateCamera() const {
  // Only return true if camera exists AND is connected
  // This prevents calling methods on disconnected cameras
  return m_currentCamera != nullptr && m_currentCamera->IsConnected();
}

void UICameraPanelUtility::RenderPanel(ICameraHardware* camera, const std::string& cameraId) {
  // Update camera reference if changed
  if (m_currentCamera != camera || m_currentCameraId != cameraId) {
    SetSelectedCamera(camera, cameraId);
  }

  if (!m_currentCamera) {
    ImGui::Text("No camera available");
    ImGui::Spacing();
    ImGui::Text("Camera reference is null");
    return;
  }

  // Always render camera header and connection controls, even if not connected
  RenderCameraHeader();
  ImGui::Separator();

  RenderConnectionControls();
  ImGui::Separator();

  // Only validate connection for operations that require it
  if (m_currentCamera->IsConnected()) {
    RenderCameraStatus();
    ImGui::Separator();

    RenderGrabbingControls();
    ImGui::Separator();

    RenderExposureControls();
    ImGui::Separator();

    RenderImageControls();
    ImGui::Separator();

    RenderAdvancedControls();
    ImGui::Separator();
  }
  else {
    ImGui::Text("Camera Status: Not Connected");
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Click 'Connect' button above to connect the camera");
    ImGui::Spacing();
  }

  RenderDebugControls();
}

void UICameraPanelUtility::SetSelectedCamera(ICameraHardware* camera, const std::string& cameraId) {
  m_currentCamera = camera;
  m_currentCameraId = cameraId;

  if (camera) {
    std::cout << "[INFO] Utility panel set to camera: " << cameraId
      << " (Type: " << (camera->GetCameraType() == ICameraHardware::CameraType::PYLON ? "Pylon" : "IDS")
      << ")" << std::endl;
    // Update exposure UI with current camera settings
    UpdateExposureUIFromCamera();
  }
}

void UICameraPanelUtility::ClearCamera() {
  m_currentCamera = nullptr;
  m_currentCameraId = "";
  std::cout << "[INFO] Utility panel camera cleared" << std::endl;
}

void UICameraPanelUtility::RenderCameraHeader() {
  ImGui::SetWindowFontScale(1.2f);
  ImGui::Text("Camera: %s", m_currentCameraId.c_str());
  ImGui::SetWindowFontScale(1.0f);

  if (!m_currentCamera) {
    ImGui::Text("Device: Not available");
    return;
  }

  // Camera type indicator (works without connection)
  const char* typeStr = (m_currentCamera->GetCameraType() == ICameraHardware::CameraType::PYLON) ? "Pylon" : "IDS";
  ImVec4 typeColor = (m_currentCamera->GetCameraType() == ICameraHardware::CameraType::PYLON) ?
    ImVec4(0.3f, 0.8f, 0.3f, 1.0f) : ImVec4(0.3f, 0.3f, 0.8f, 1.0f);

  ImGui::Text("Type: ");
  ImGui::SameLine();
  ImGui::TextColored(typeColor, "%s", typeStr);

  // Try to get model and serial (may fail if not connected)
  try {
    std::string modelName = m_currentCamera->GetModelName();
    std::string serialNumber = m_currentCamera->GetSerialNumber();

    ImGui::Text("Model: %s", modelName.empty() ? "Unknown" : modelName.c_str());
    ImGui::Text("Serial: %s", serialNumber.empty() ? "Unknown" : serialNumber.c_str());
  }
  catch (...) {
    ImGui::Text("Model: (Connect camera to view)");
    ImGui::Text("Serial: (Connect camera to view)");
  }

}

void UICameraPanelUtility::RenderConnectionControls() {
  ImGui::Text("Connection Controls");

  if (!m_currentCamera) {
    ImGui::Text("No camera selected");
    return;
  }

  bool isConnected = m_currentCamera->IsConnected();

  if (!isConnected) {
    if (ImGui::Button("Connect", ImVec2(150, 30))) {
      m_cameraManager.ConnectCamera(m_currentCameraId);
      // Update exposure UI after connecting
      UpdateExposureUIFromCamera();
    }
  }
  else {
    if (ImGui::Button("Disconnect", ImVec2(150, 30))) {
      m_cameraManager.DisconnectCamera(m_currentCameraId);
    }

    ImGui::SameLine();
    if (ImGui::Button("Reconnect", ImVec2(150, 30))) {
      m_currentCamera->Disconnect();
      m_currentCamera->Connect();
      // Update exposure UI after reconnecting
      UpdateExposureUIFromCamera();
    }
  }
}

void UICameraPanelUtility::RenderCameraStatus() {
  ImGui::Text("Camera Status");

  if (!m_currentCamera) {
    ImGui::Text("Camera not available");
    return;
  }

  // Always show basic info, even if not connected
  ImGui::Text("Camera Type: %s",
    m_currentCamera->GetCameraType() == ICameraHardware::CameraType::PYLON ? "Pylon" : "IDS");

  // Status indicators with colors
  ImGui::Text("Connected: ");
  ImGui::SameLine();
  if (m_currentCamera->IsConnected()) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Yes");
  }
  else {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "No");
    ImGui::Text("Connect camera to view more details");
    return; // Don't try to get other info if not connected
  }

  ImGui::Text("Grabbing: ");
  ImGui::SameLine();
  if (m_currentCamera->IsGrabbing()) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Yes");
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "No");
  }

  ImGui::Text("Device OK: ");
  ImGui::SameLine();
  if (m_currentCamera->IsDeviceRemoved()) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Removed");
  }
  else {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "OK");
  }

  // Only get exposure settings if connected - this prevents the error spam
  try {
    auto settings = m_currentCamera->GetExposureSettings();
    ImGui::Spacing();
    ImGui::Text("Current Settings:");
    ImGui::Text("Exposure: %.0f μs", settings.exposure_time);
    ImGui::Text("Gain: %.1f", settings.gain);
    ImGui::Text("Auto Exposure: %s", settings.auto_exposure ? "On" : "Off");
    ImGui::Text("Auto Gain: %s", settings.auto_gain ? "On" : "Off");
  }
  catch (...) {
    // Silently ignore exposure setting errors when not connected
    if (CanLogError()) {
      std::cout << "[INFO] Cannot get exposure settings - camera may not be fully connected" << std::endl;
    }
  }

  // Error display
  std::string lastError = m_currentCamera->GetLastError();
  if (!lastError.empty()) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Last Error:");
    ImGui::TextWrapped("%s", lastError.c_str());

    if (ImGui::Button("Clear Error")) {
      m_currentCamera->ClearLastError();
    }
  }
}

void UICameraPanelUtility::RenderGrabbingControls() {
  ImGui::Text("Image Acquisition");

  if (!ValidateCamera()) {
    ImGui::Text("Camera not available");
    return;
  }

  if (!m_currentCamera->IsConnected()) {
    ImGui::Text("Camera not connected");
    return;
  }

  if (!m_currentCamera->IsGrabbing()) {
    if (ImGui::Button("Start Grabbing", ImVec2(150, 30))) {
      m_cameraManager.StartGrabbing(m_currentCameraId);
    }

    ImGui::SameLine();
    if (ImGui::Button("Grab Single Frame", ImVec2(150, 30))) {
      std::cout << "[INFO] Use Single Frame tab for single frame capture with display" << std::endl;
      // Could implement direct single frame capture here if needed
    }
  }
  else {
    if (ImGui::Button("Stop Grabbing", ImVec2(150, 30))) {
      m_cameraManager.StopGrabbing(m_currentCameraId);
    }
  }

  // Add helpful text
  ImGui::Spacing();
  ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Tip:");
  ImGui::TextWrapped("Use Live Video tab for continuous feed or Single Frame tab for capture");
}

void UICameraPanelUtility::RenderExposureControls() {
  ImGui::Text("Exposure Controls");

  if (!ValidateCamera()) {
    ImGui::Text("Camera not available for exposure control");
    return;
  }

  // Exposure time control
  ImGui::Text("Exposure Time (μs):");
  if (ImGui::SliderFloat("##ExposureTime", &m_exposureTimeUI, 100.0f, 100000.0f, "%.0f")) {
    // Apply immediately when slider changes
    ApplyExposureSettingsFromUI();
  }

  // Gain control
  ImGui::Text("Gain:");
  if (ImGui::SliderFloat("##Gain", &m_gainUI, 0.0f, 10.0f, "%.1f")) {
    // Apply immediately when slider changes
    ApplyExposureSettingsFromUI();
  }

  // Auto modes
  ImGui::Spacing();
  if (ImGui::Checkbox("Auto Exposure", &m_autoExposureUI)) {
    ApplyExposureSettingsFromUI();
  }

  if (ImGui::Checkbox("Auto Gain", &m_autoGainUI)) {
    ApplyExposureSettingsFromUI();
  }

  // Quick preset buttons
  ImGui::Spacing();
  ImGui::Text("Quick Presets:");

  if (ImGui::Button("Dark (Fast)", ImVec2(120, 25))) {
    m_exposureTimeUI = 1000.0f;
    m_gainUI = 1.0f;
    m_autoExposureUI = false;
    m_autoGainUI = false;
    ApplyExposureSettingsFromUI();
  }

  ImGui::SameLine();
  if (ImGui::Button("Normal", ImVec2(120, 25))) {
    m_exposureTimeUI = 10000.0f;
    m_gainUI = 1.0f;
    m_autoExposureUI = false;
    m_autoGainUI = false;
    ApplyExposureSettingsFromUI();
  }

  if (ImGui::Button("Bright (Slow)", ImVec2(120, 25))) {
    m_exposureTimeUI = 50000.0f;
    m_gainUI = 2.0f;
    m_autoExposureUI = false;
    m_autoGainUI = false;
    ApplyExposureSettingsFromUI();
  }

  ImGui::SameLine();
  if (ImGui::Button("Auto Mode", ImVec2(120, 25))) {
    m_autoExposureUI = true;
    m_autoGainUI = true;
    ApplyExposureSettingsFromUI();
  }

  // Manual refresh button
  ImGui::Spacing();
  if (ImGui::Button("Refresh Settings", ImVec2(-1, 25))) {
    UpdateExposureUIFromCamera();
  }
}

void UICameraPanelUtility::RenderImageControls() {
  ImGui::Text("Image Operations");

  if (!ValidateCamera()) {
    ImGui::Text("Camera not available");
    return;
  }

  // Capture controls
  if (ImGui::Button("Capture Image", ImVec2(-1, 30))) {
    if (m_cameraManager.CaptureImage(m_currentCameraId)) {
      std::cout << "[INFO] Image saved to: " << m_cameraManager.GetLastCapturedImagePath() << std::endl;
    }
    else {
      std::cout << "[ERROR] Failed to capture and save image" << std::endl;
    }
  }

  ImGui::Spacing();
  ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Image Capture:");
  ImGui::TextWrapped("Captured images are saved to the configured output directory");
}

void UICameraPanelUtility::RenderAdvancedControls() {
  ImGui::Text("Advanced Controls");

  if (!ValidateCamera()) {
    ImGui::Text("Camera not available");
    return;
  }

  // Camera-specific configuration
  ImGui::Text("Camera Configuration:");

  // Show camera type specific options
  if (m_currentCamera->GetCameraType() == ICameraHardware::CameraType::PYLON) {
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Pylon Camera Options:");

    if (ImGui::Button("Set Trigger Mode Off", ImVec2(200, 25))) {
      m_currentCamera->SetConfiguration("TriggerMode", "Off");
    }

    if (ImGui::Button("Set RGB8 Format", ImVec2(200, 25))) {
      m_currentCamera->SetConfiguration("PixelFormat", "RGB8");
    }

  }
  else if (m_currentCamera->GetCameraType() == ICameraHardware::CameraType::IDS) {
    ImGui::TextColored(ImVec4(0.3f, 0.3f, 0.8f, 1.0f), "IDS Camera Options:");

    if (ImGui::Button("Reset to Defaults", ImVec2(200, 25))) {
      std::cout << "[INFO] IDS camera reset functionality to be implemented" << std::endl;
    }
  }
}

void UICameraPanelUtility::RenderDebugControls() {
  ImGui::Text("Debug Information");

  if (!ValidateCamera()) {
    ImGui::Text("No debug info available");
    return;
  }

  // Camera identification
  ImGui::Text("Camera ID: %s", m_currentCameraId.c_str());
  ImGui::Text("Vendor: %s", m_currentCamera->GetVendorName().c_str());

  // Interface validation
  ImGui::Spacing();
  if (ImGui::Button("Test Interface", ImVec2(-1, 25))) {
    std::cout << "[DEBUG] Testing ICameraHardware interface for " << m_currentCameraId << std::endl;
    std::cout << "  - Connected: " << m_currentCamera->IsConnected() << std::endl;
    std::cout << "  - Grabbing: " << m_currentCamera->IsGrabbing() << std::endl;
    std::cout << "  - Device Removed: " << m_currentCamera->IsDeviceRemoved() << std::endl;
    std::cout << "  - Model: " << m_currentCamera->GetModelName() << std::endl;
    std::cout << "  - Serial: " << m_currentCamera->GetSerialNumber() << std::endl;
    std::cout << "  - Type: " << (m_currentCamera->GetCameraType() == ICameraHardware::CameraType::PYLON ? "Pylon" : "IDS") << std::endl;
  }
}

void UICameraPanelUtility::UpdateExposureUIFromCamera() {
  if (!m_currentCamera || !m_currentCamera->IsConnected()) {
    // Don't spam errors when camera is not connected
    static auto lastErrorTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastErrorTime).count() >= 5) {
      std::cout << "[INFO] Camera not connected - skipping exposure UI update" << std::endl;
      lastErrorTime = now;
    }
    return;
  }

  try {
    auto settings = m_currentCamera->GetExposureSettings();
    m_exposureTimeUI = static_cast<float>(settings.exposure_time);
    m_gainUI = static_cast<float>(settings.gain);
    m_autoExposureUI = settings.auto_exposure;
    m_autoGainUI = settings.auto_gain;

    std::cout << "[INFO] Updated exposure UI from camera settings" << std::endl;
  }
  catch (const std::exception& e) {
    static auto lastErrorTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastErrorTime).count() >= 5) {
      std::cout << "[WARN] Failed to update exposure UI: " << e.what() << std::endl;
      lastErrorTime = now;
    }
  }
}

void UICameraPanelUtility::ApplyExposureSettingsFromUI() {
  if (!m_currentCamera || !m_currentCamera->IsConnected()) {
    // Don't spam errors when camera is not connected
    if (CanLogError()) {
      std::cout << "[INFO] Camera not connected - skipping exposure settings apply" << std::endl;
    }
    return;
  }

  try {
    ICameraHardware::ExposureSettings settings;
    settings.exposure_time = static_cast<double>(m_exposureTimeUI);
    settings.gain = static_cast<double>(m_gainUI);
    settings.auto_exposure = m_autoExposureUI;
    settings.auto_gain = m_autoGainUI;

    if (m_cameraManager.ApplyExposureSettings(m_currentCameraId, settings)) {
      std::cout << "[INFO] Applied exposure settings: exp=" << settings.exposure_time
        << "μs, gain=" << settings.gain << std::endl;
    }
    else {
      if (CanLogError()) {
        std::cout << "[WARN] Failed to apply exposure settings" << std::endl;
      }
    }
  }
  catch (const std::exception& e) {
    if (CanLogError()) {
      std::cout << "[ERROR] Exception applying exposure settings: " << e.what() << std::endl;
    }
  }
}

bool UICameraPanelUtility::CanLogError() {
  auto now = std::chrono::steady_clock::now();
  if (std::chrono::duration_cast<std::chrono::seconds>(now - m_lastErrorTime) >= ERROR_RATE_LIMIT) {
    m_lastErrorTime = now;
    return true;
  }
  return false;
}