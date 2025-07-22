#include "CameraManager.h"
#include "imgui.h"
#include <iostream>
#include <algorithm>

CameraManager::CameraManager()
  : m_showUI(true), m_selectedCameraId("") {
  std::cout << "CameraManager initialized" << std::endl;
}

CameraManager::~CameraManager() {
  // Stop all cameras before cleanup
  StopGrabbingAll();
  m_cameras.clear();
  std::cout << "CameraManager destroyed" << std::endl;
}

bool CameraManager::StopGrabbingAll() {
  bool allSuccess = true;

  std::cout << "Stopping grabbing on all cameras..." << std::endl;

  for (auto& [id, managedCamera] : m_cameras) {
    auto& camera = managedCamera->camera->GetCamera();

    if (!camera.IsGrabbing()) {
      continue; // Already stopped
    }

    camera.StopGrabbing();
    std::cout << "Stopped grabbing on camera: " << id << std::endl;
  }

  return allSuccess;
}

bool CameraManager::StartGrabbing(const std::string& cameraId) {
  auto* managedCamera = FindCamera(cameraId);
  if (!managedCamera) {
    std::cerr << "Camera '" << cameraId << "' not found" << std::endl;
    return false;
  }

  auto& camera = managedCamera->camera->GetCamera();

  if (!camera.IsConnected()) {
    std::cerr << "Camera '" << cameraId << "' is not connected" << std::endl;
    return false;
  }

  if (camera.StartGrabbing()) {
    std::cout << "Started grabbing on camera: " << cameraId << std::endl;
    return true;
  }
  else {
    std::cerr << "Failed to start grabbing on camera: " << cameraId << std::endl;
    return false;
  }
}

bool CameraManager::StopGrabbing(const std::string& cameraId) {
  auto* managedCamera = FindCamera(cameraId);
  if (!managedCamera) {
    std::cerr << "Camera '" << cameraId << "' not found" << std::endl;
    return false;
  }

  auto& camera = managedCamera->camera->GetCamera();
  camera.StopGrabbing();

  std::cout << "Stopped grabbing on camera: " << cameraId << std::endl;
  return true;
}

bool CameraManager::ApplyExposureForNode(const std::string& cameraId, const std::string& nodeId) {
  auto* managedCamera = FindCamera(cameraId);
  if (!managedCamera) {
    std::cerr << "Camera '" << cameraId << "' not found" << std::endl;
    return false;
  }

  return managedCamera->camera->ApplyExposureForNode(nodeId);
}

bool CameraManager::ApplyExposureForNodeAll(const std::string& nodeId) {
  bool allSuccess = true;

  std::cout << "Applying exposure settings for node '" << nodeId << "' to all cameras..." << std::endl;

  for (auto& [id, managedCamera] : m_cameras) {
    if (!managedCamera->camera->ApplyExposureForNode(nodeId)) {
      std::cerr << "Failed to apply exposure for node '" << nodeId << "' to camera: " << id << std::endl;
      allSuccess = false;
    }
  }

  return allSuccess;
}

bool CameraManager::ApplyExposureSettings(const std::string& cameraId, const PylonCamera::ExposureSettings& settings) {
  auto* managedCamera = FindCamera(cameraId);
  if (!managedCamera) {
    std::cerr << "Camera '" << cameraId << "' not found" << std::endl;
    return false;
  }

  auto& camera = managedCamera->camera->GetCamera();
  return camera.ApplyExposureSettings(settings);
}

bool CameraManager::CaptureImage(const std::string& cameraId) {
  auto* managedCamera = FindCamera(cameraId);
  if (!managedCamera) {
    std::cerr << "Camera '" << cameraId << "' not found" << std::endl;
    return false;
  }

  return managedCamera->camera->CaptureImage();
}

bool CameraManager::CaptureImageAll() {
  bool allSuccess = true;

  std::cout << "Capturing images from all cameras..." << std::endl;

  for (auto& [id, managedCamera] : m_cameras) {
    auto& camera = managedCamera->camera->GetCamera();

    if (!camera.IsConnected() || !camera.IsGrabbing()) {
      std::cout << "Skipping camera " << id << " (not connected or not grabbing)" << std::endl;
      continue;
    }

    if (!managedCamera->camera->CaptureImage()) {
      std::cerr << "Failed to capture image from camera: " << id << std::endl;
      allSuccess = false;
    }
    else {
      std::cout << "Captured image from camera: " << id << std::endl;
    }
  }

  return allSuccess;
}

CameraManager::CameraStatus CameraManager::GetCameraStatus(const std::string& cameraId) const {
  CameraStatus status;
  status.id = cameraId;

  const auto* managedCamera = FindCamera(cameraId);
  if (!managedCamera) {
    status.connected = false;
    status.grabbing = false;
    status.deviceRemoved = false;
    status.deviceInfo = "Camera not found";
    return status;
  }

  const auto& camera = managedCamera->camera->GetCamera();
  status.connected = camera.IsConnected();
  status.grabbing = camera.IsGrabbing();
  status.deviceRemoved = camera.IsCameraDeviceRemoved();
  status.deviceInfo = camera.GetDeviceInfo();
  status.currentExposure = camera.GetCurrentExposureSettings();

  return status;
}

std::vector<CameraManager::CameraStatus> CameraManager::GetAllCameraStatus() const {
  std::vector<CameraStatus> statusList;
  statusList.reserve(m_cameras.size());

  for (const auto& [id, managedCamera] : m_cameras) {
    statusList.push_back(GetCameraStatus(id));
  }

  return statusList;
}

void CameraManager::RenderUI() {
  if (!m_showUI) return;

  ImGui::Begin("Camera Manager", &m_showUI);

  // Camera count and status
  ImGui::Text("Cameras: %zu", m_cameras.size());

  if (ImGui::Button("Initialize All")) {
    InitializeAllCameras();
  }
  ImGui::SameLine();
  if (ImGui::Button("Start All")) {
    StartGrabbingAll();
  }
  ImGui::SameLine();
  if (ImGui::Button("Stop All")) {
    StopGrabbingAll();
  }
  ImGui::SameLine();
  if (ImGui::Button("Capture All")) {
    CaptureImageAll();
  }

  ImGui::Separator();

  // Create a two-panel layout
  ImGui::Columns(2, "CameraManagerColumns");

  // Left panel - Camera list
  RenderCameraList();

  ImGui::NextColumn();

  // Right panel - Selected camera details
  RenderSelectedCameraPanel();

  ImGui::Columns(1); // Reset columns

  ImGui::Separator();

  // Bulk operations
  RenderBulkOperations();

  ImGui::Separator();

  // Status table
  RenderCameraStatusTable();

  ImGui::End();
}

CameraManager::ManagedCamera* CameraManager::FindCamera(const std::string& cameraId) {
  auto it = m_cameras.find(cameraId);
  return (it != m_cameras.end()) ? it->second.get() : nullptr;
}

const CameraManager::ManagedCamera* CameraManager::FindCamera(const std::string& cameraId) const {
  auto it = m_cameras.find(cameraId);
  return (it != m_cameras.end()) ? it->second.get() : nullptr;
}

void CameraManager::RenderCameraList() {
  ImGui::Text("Camera List");
  ImGui::Separator();

  // Add camera button
  if (ImGui::Button("Add Camera")) {
    static int cameraCounter = 1;
    std::string newId = "camera_" + std::to_string(cameraCounter++);
    CameraInfo info(newId, "New Camera");
    AddCamera(info);
  }

  // Camera list
  for (const auto& [id, managedCamera] : m_cameras) {
    bool isSelected = (m_selectedCameraId == id);

    if (ImGui::Selectable(id.c_str(), isSelected)) {
      m_selectedCameraId = id;
    }

    // Right-click context menu
    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("Remove Camera")) {
        RemoveCamera(id);
        if (m_selectedCameraId == id) {
          m_selectedCameraId = "";
        }
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    // Show status indicator
    const auto& camera = managedCamera->camera->GetCamera();
    ImGui::SameLine();
    if (camera.IsConnected()) {
      if (camera.IsGrabbing()) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "[GRAB]");
      }
      else {
        ImGui::TextColored(ImVec4(0, 0.8f, 0, 1), "[CONN]");
      }
    }
    else {
      ImGui::TextColored(ImVec4(0.8f, 0, 0, 1), "[DISC]");
    }
  }
}

void CameraManager::RenderSelectedCameraPanel() {
  ImGui::Text("Camera Details");
  ImGui::Separator();

  if (m_selectedCameraId.empty()) {
    ImGui::Text("No camera selected");
    return;
  }

  auto* managedCamera = FindCamera(m_selectedCameraId);
  if (!managedCamera) {
    ImGui::Text("Selected camera not found");
    return;
  }

  // Camera info
  ImGui::Text("ID: %s", managedCamera->info.id.c_str());
  ImGui::Text("Description: %s", managedCamera->info.description.c_str());
  if (!managedCamera->info.serialNumber.empty()) {
    ImGui::Text("Serial: %s", managedCamera->info.serialNumber.c_str());
  }

  auto& camera = managedCamera->camera->GetCamera();
  auto& cameraTest = *managedCamera->camera;

  // Connection controls
  ImGui::Separator();
  ImGui::Text("Connection");

  if (!camera.IsConnected()) {
    if (ImGui::Button("Connect")) {
      ConnectCamera(m_selectedCameraId);
    }
  }
  else {
    if (ImGui::Button("Disconnect")) {
      DisconnectCamera(m_selectedCameraId);
    }

    ImGui::SameLine();
    if (!camera.IsGrabbing()) {
      if (ImGui::Button("Start Grab")) {
        StartGrabbing(m_selectedCameraId);
      }
    }
    else {
      if (ImGui::Button("Stop Grab")) {
        StopGrabbing(m_selectedCameraId);
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("Capture")) {
      CaptureImage(m_selectedCameraId);
    }
  }

  // Quick exposure controls if connected
  if (camera.IsConnected()) {
    ImGui::Separator();
    ImGui::Text("Quick Exposure");

    // Quick node buttons
    const std::vector<std::pair<std::string, std::string>> quickNodes = {
        {"node_4083", "Sled"},
        {"node_4107", "PIC"},
        {"node_4137", "Coll Lens"},
        {"node_4156", "Focus Lens"}
    };

    int buttonCount = 0;
    for (const auto& [nodeId, nodeName] : quickNodes) {
      if (ImGui::Button(nodeName.c_str())) {
        ApplyExposureForNode(m_selectedCameraId, nodeId);
      }
      buttonCount++;
      if ((buttonCount % 2) != 0) {
        ImGui::SameLine();
      }
    }

    // Show current exposure settings
    ImGui::Separator();
    ImGui::Text("Current Settings");
    auto settings = camera.GetCurrentExposureSettings();
    ImGui::Text("Exposure: %.0f us", settings.exposure_time);
    ImGui::Text("Gain: %.1f", settings.gain);
    ImGui::Text("Auto Exp: %s", settings.exposure_auto ? "On" : "Off");
    ImGui::Text("Auto Gain: %s", settings.gain_auto ? "On" : "Off");
  }

  // Camera window toggle
  ImGui::Separator();
  if (ImGui::Button("Toggle Camera Window")) {
    cameraTest.ToggleWindow();
  }
}

void CameraManager::RenderBulkOperations() {
  ImGui::Text("Bulk Operations");

  // Node-based exposure application
  static char nodeIdBuffer[64] = "node_4083";
  ImGui::InputText("Node ID", nodeIdBuffer, sizeof(nodeIdBuffer));
  ImGui::SameLine();
  if (ImGui::Button("Apply to All")) {
    ApplyExposureForNodeAll(std::string(nodeIdBuffer));
  }
}

void CameraManager::RenderCameraStatusTable() {
  ImGui::Text("Camera Status");

  if (ImGui::BeginTable("CameraStatusTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("ID");
    ImGui::TableSetupColumn("Connected");
    ImGui::TableSetupColumn("Grabbing");
    ImGui::TableSetupColumn("Device Info");
    ImGui::TableSetupColumn("Exposure");
    ImGui::TableSetupColumn("Gain");
    ImGui::TableHeadersRow();

    for (const auto& [id, managedCamera] : m_cameras) {
      ImGui::TableNextRow();

      const auto& camera = managedCamera->camera->GetCamera();

      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%s", id.c_str());

      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%s", camera.IsConnected() ? "Yes" : "No");

      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%s", camera.IsGrabbing() ? "Yes" : "No");

      ImGui::TableSetColumnIndex(3);
      if (camera.IsConnected()) {
        std::string deviceInfo = camera.GetDeviceInfo();
        if (deviceInfo.length() > 20) {
          deviceInfo = deviceInfo.substr(0, 17) + "...";
        }
        ImGui::Text("%s", deviceInfo.c_str());
      }
      else {
        ImGui::Text("N/A");
      }

      ImGui::TableSetColumnIndex(4);
      if (camera.IsConnected()) {
        ImGui::Text("%.0f us", camera.GetExposureTime());
      }
      else {
        ImGui::Text("N/A");
      }

      ImGui::TableSetColumnIndex(5);
      if (camera.IsConnected()) {
        ImGui::Text("%.1f", camera.GetGain());
      }
      else {
        ImGui::Text("N/A");
      }
    }

    ImGui::EndTable();
  }
}


// UPDATE your existing AddCamera method to show connection info:
bool CameraManager::AddCamera(const CameraInfo& cameraInfo) {
  // Check if camera with this ID already exists
  if (m_cameras.find(cameraInfo.id) != m_cameras.end()) {
    std::cerr << "Camera with ID '" << cameraInfo.id << "' already exists" << std::endl;
    return false;
  }

  // Create new managed camera
  auto managedCamera = std::make_unique<ManagedCamera>(cameraInfo);

  std::cout << "Added camera: " << cameraInfo.id;
  if (!cameraInfo.description.empty()) {
    std::cout << " (" << cameraInfo.description << ")";
  }
  std::cout << " - Connection: " << cameraInfo.GetConnectionInfo() << std::endl;  // NEW: Show connection method

  m_cameras[cameraInfo.id] = std::move(managedCamera);

  // Auto-select first camera
  if (m_selectedCameraId.empty()) {
    m_selectedCameraId = cameraInfo.id;
  }

  return true;
}


bool CameraManager::RemoveCamera(const std::string& cameraId) {
  auto it = m_cameras.find(cameraId);
  if (it == m_cameras.end()) {
    std::cerr << "Camera '" << cameraId << "' not found" << std::endl;
    return false;
  }

  // Stop grabbing if active
  StopGrabbing(cameraId);

  // Remove camera
  m_cameras.erase(it);

  // Update selection if needed
  if (m_selectedCameraId == cameraId) {
    m_selectedCameraId = m_cameras.empty() ? "" : m_cameras.begin()->first;
  }

  std::cout << "Removed camera: " << cameraId << std::endl;
  return true;
}

PylonCameraTest* CameraManager::GetCamera(const std::string& cameraId) {
  auto* managedCamera = FindCamera(cameraId);
  return managedCamera ? managedCamera->camera.get() : nullptr;
}

std::vector<std::string> CameraManager::GetCameraIds() const {
  std::vector<std::string> ids;
  ids.reserve(m_cameras.size());

  for (const auto& [id, camera] : m_cameras) {
    ids.push_back(id);
  }

  return ids;
}


// UPDATE your existing InitializeAllCameras method to show connection details:
bool CameraManager::InitializeAllCameras() {
  bool allSuccess = true;

  std::cout << "Initializing all cameras with enhanced connection support..." << std::endl;

  for (auto& [id, managedCamera] : m_cameras) {
    if (!managedCamera->info.autoConnect) {
      std::cout << "Skipping camera " << id << " (auto-connect disabled)" << std::endl;
      continue;
    }

    std::cout << "Initializing camera: " << id << " (" << managedCamera->info.GetConnectionInfo() << ")" << std::endl;  // NEW: Show connection method

    if (!ConnectCamera(id)) {
      std::cerr << "Failed to connect camera: " << id << std::endl;
      allSuccess = false;
    }
  }

  std::cout << "Camera initialization " << (allSuccess ? "completed successfully" : "completed with errors") << std::endl;
  return allSuccess;
}



bool CameraManager::ConnectCamera(const std::string& cameraId) {
  auto* managedCamera = FindCamera(cameraId);
  if (!managedCamera) {
    std::cerr << "Camera '" << cameraId << "' not found" << std::endl;
    return false;
  }

  auto& camera = managedCamera->camera->GetCamera();
  const auto& info = managedCamera->info;

  if (!camera.Initialize()) {
    std::cerr << "Failed to initialize camera: " << cameraId << std::endl;
    return false;
  }

  bool connected = false;

  // NEW: Enhanced connection logic based on camera info
  switch (info.connectionMethod) {
  case CameraInfo::ConnectionMethod::IP_ADDRESS:
    std::cout << "Connecting to camera " << cameraId << " via IP: " << info.ipAddress << std::endl;
    connected = camera.ConnectToIP(info.ipAddress);
    break;

  case CameraInfo::ConnectionMethod::SERIAL_NUMBER:
    std::cout << "Connecting to camera " << cameraId << " via Serial: " << info.serialNumber << std::endl;
    connected = camera.ConnectToSerial(info.serialNumber);
    break;

  case CameraInfo::ConnectionMethod::DEVICE_INDEX:
    std::cout << "Connecting to camera " << cameraId << " via Index: " << info.deviceIndex << std::endl;
    connected = camera.ConnectToIndex(info.deviceIndex);
    break;

  case CameraInfo::ConnectionMethod::AUTO:
  default:
    std::cout << "Connecting to camera " << cameraId << " via Auto-detection" << std::endl;
    connected = camera.Connect();
    break;
  }

  if (connected) {
    std::cout << "Successfully connected camera: " << cameraId << std::endl;

    // NEW: Log network information for GigE cameras
    if (info.IsIPConnection()) {
      std::string networkInfo = camera.GetNetworkInfo();
      if (!networkInfo.empty()) {
        std::cout << "  Network Info: " << networkInfo << std::endl;
      }
    }
  }
  else {
    std::cerr << "Failed to connect camera: " << cameraId << std::endl;
  }

  return connected;
}


bool CameraManager::DisconnectCamera(const std::string& cameraId) {
  auto* managedCamera = FindCamera(cameraId);
  if (!managedCamera) {
    std::cerr << "Camera '" << cameraId << "' not found" << std::endl;
    return false;
  }

  auto& camera = managedCamera->camera->GetCamera();
  camera.Disconnect();

  std::cout << "Disconnected camera: " << cameraId << std::endl;
  return true;
}

bool CameraManager::StartGrabbingAll() {
  bool allSuccess = true;

  std::cout << "Starting grabbing on all cameras..." << std::endl;

  for (auto& [id, managedCamera] : m_cameras) {
    auto& camera = managedCamera->camera->GetCamera();

    if (!camera.IsConnected()) {
      std::cout << "Skipping camera " << id << " (not connected)" << std::endl;
      continue;
    }

    if (!camera.StartGrabbing()) {
      std::cerr << "Failed to start grabbing on camera: " << id << std::endl;
      allSuccess = false;
    }
    else {
      std::cout << "Started grabbing on camera: " << id << std::endl;
    }
  }

  return allSuccess;
}


// ADD this new method to your CameraManager.cpp (enhanced status table):
void CameraManager::RenderEnhancedCameraStatusTable() {
  ImGui::Text("Enhanced Camera Status");

  if (ImGui::BeginTable("EnhancedCameraStatusTable", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("ID");
    ImGui::TableSetupColumn("Connection");
    ImGui::TableSetupColumn("Connected");
    ImGui::TableSetupColumn("Grabbing");
    ImGui::TableSetupColumn("Device Info");
    ImGui::TableSetupColumn("Network Info");
    ImGui::TableSetupColumn("Exposure");
    ImGui::TableSetupColumn("Gain");
    ImGui::TableHeadersRow();

    for (const auto& [id, managedCamera] : m_cameras) {
      ImGui::TableNextRow();

      const auto& camera = managedCamera->camera->GetCamera();
      const auto& info = managedCamera->info;

      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%s", id.c_str());

      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%s", info.GetConnectionInfo().c_str());

      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%s", camera.IsConnected() ? "Yes" : "No");

      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%s", camera.IsGrabbing() ? "Yes" : "No");

      ImGui::TableSetColumnIndex(4);
      if (camera.IsConnected()) {
        std::string deviceInfo = camera.GetDeviceInfo();
        if (deviceInfo.length() > 20) {
          deviceInfo = deviceInfo.substr(0, 17) + "...";
        }
        ImGui::Text("%s", deviceInfo.c_str());
      }
      else {
        ImGui::Text("N/A");
      }

      ImGui::TableSetColumnIndex(5);
      if (camera.IsConnected() && info.IsIPConnection()) {
        std::string networkInfo = camera.GetNetworkInfo();
        if (networkInfo.length() > 15) {
          networkInfo = networkInfo.substr(0, 12) + "...";
        }
        ImGui::Text("%s", networkInfo.c_str());
      }
      else {
        ImGui::Text("N/A");
      }

      ImGui::TableSetColumnIndex(6);
      if (camera.IsConnected()) {
        ImGui::Text("%.0f us", camera.GetExposureTime());
      }
      else {
        ImGui::Text("N/A");
      }

      ImGui::TableSetColumnIndex(7);
      if (camera.IsConnected()) {
        ImGui::Text("%.1f", camera.GetGain());
      }
      else {
        ImGui::Text("N/A");
      }
    }

    ImGui::EndTable();
  }
}

// ADD this new method to your CameraManager.cpp (enhanced camera list):
void CameraManager::RenderEnhancedCameraList() {
  ImGui::Text("Camera List");
  ImGui::Separator();

  for (const auto& [id, managedCamera] : m_cameras) {
    bool isSelected = (m_selectedCameraId == id);

    if (ImGui::Selectable(id.c_str(), isSelected)) {
      m_selectedCameraId = id;
    }

    // Right-click context menu
    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("Remove Camera")) {
        RemoveCamera(id);
        if (m_selectedCameraId == id) {
          m_selectedCameraId = "";
        }
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    // NEW: Show connection info
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "[%s]", managedCamera->info.GetConnectionInfo().c_str());

    // Show status indicator
    const auto& camera = managedCamera->camera->GetCamera();
    ImGui::SameLine();
    if (camera.IsConnected()) {
      if (camera.IsGrabbing()) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "[GRAB]");
      }
      else {
        ImGui::TextColored(ImVec4(0, 0.8f, 0, 1), "[CONN]");
      }
    }
    else {
      ImGui::TextColored(ImVec4(0.8f, 0, 0, 1), "[DISC]");
    }
  }
}