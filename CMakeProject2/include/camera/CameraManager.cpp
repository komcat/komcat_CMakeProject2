// CameraManager.cpp - Complete implementation with interface support
#include "CameraManager.h"
#include "ICameraHardware.h"
#include "CameraHardwareFactory.h"
#include "pylon_camera_test.h"  // For backward compatibility
#include "include/logger.h"
#include "ids_camera_test.h"
#include "imgui.h"
#include <algorithm>
#include <iostream>
#include <pylon/PylonIncludes.h>


// Add these includes to CameraManager.cpp at the top
#include <fstream>
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <sstream>

// Add this helper function to CameraManager.cpp (private helper methods section)
// BMP file header structures
#pragma pack(push, 1)
struct BMPFileHeader {
  uint16_t type = 0x4D42; // "BM"
  uint32_t size;
  uint16_t reserved1 = 0;
  uint16_t reserved2 = 0;
  uint32_t offset = 54;
};

struct BMPInfoHeader {
  uint32_t size = 40;
  int32_t width;
  int32_t height;
  uint16_t planes = 1;
  uint16_t bitCount = 24;
  uint32_t compression = 0;
  uint32_t imageSize;
  int32_t xPixelsPerMeter = 2835; // 72 DPI
  int32_t yPixelsPerMeter = 2835; // 72 DPI
  uint32_t colorsUsed = 0;
  uint32_t colorsImportant = 0;
};
#pragma pack(pop)

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

CameraManager::CameraManager()
  : m_showUI(false)
  , m_broadcastSystemRunning(false)
  , m_shouldStopBroadcast(false)
  , m_globalBroadcastRate(30.0f) {

  Logger* logger = Logger::GetInstance();
  if (logger) {
    logger->LogInfo("CameraManager created with interface support");
  }
}

CameraManager::~CameraManager() {
  StopBroadcastSystem();

  // Stop all cameras
  StopGrabbingAll();

  // Clear all cameras
  m_cameras.clear();

  Logger* logger = Logger::GetInstance();
  if (logger) {
    logger->LogInfo("CameraManager destroyed");
  }
}

// ============================================================================
// MANAGED CAMERA IMPLEMENTATION
// ============================================================================

CameraManager::ManagedCamera::ManagedCamera(const ExtendedCameraInfo& cameraInfo)
  : info(cameraInfo) {

  // Create camera using factory
  camera = CameraHardwareFactory::CreateCamera(cameraInfo.type, cameraInfo.id, cameraInfo.deviceInfo);

  // For Pylon cameras, also create PylonCameraTest for backward compatibility
  if (cameraInfo.type == ICameraHardware::CameraType::PYLON && camera) {
    pylonCameraTest = std::make_unique<PylonCameraTest>();
  }

  Logger* logger = Logger::GetInstance();
  if (logger) {
    std::string typeName = CameraHardwareFactory::GetCameraTypeName(cameraInfo.type);
    if (camera) {
      logger->LogInfo("Created " + typeName + " camera: " + cameraInfo.id);
    }
    else {
      logger->LogError("Failed to create " + typeName + " camera: " + cameraInfo.id);
    }
  }
}

// ============================================================================
// CAMERA MANAGEMENT - ADD CAMERAS
// ============================================================================
// Add this method to CameraManager.cpp in the "CAMERA MANAGEMENT - ADD CAMERAS" section
// This should go right after the existing ExtendedCameraInfo AddCamera method

bool CameraManager::AddCamera(const CameraInfo& cameraInfo) {
  // Convert CameraInfo to ExtendedCameraInfo with default Pylon type for backward compatibility
  ExtendedCameraInfo extendedInfo(cameraInfo, ICameraHardware::CameraType::PYLON);
  return AddCamera(extendedInfo);
}
bool CameraManager::AddCamera(const ExtendedCameraInfo& cameraInfo) {
  std::lock_guard<std::mutex> lock(m_camerasMutex);

  // Check if camera already exists
  if (m_cameras.find(cameraInfo.id) != m_cameras.end()) {
    Logger* logger = Logger::GetInstance();
    if (logger) {
      logger->LogWarning("Camera already exists: " + cameraInfo.id);
    }
    return false;
  }

  try {
    auto managedCamera = std::make_unique<ManagedCamera>(cameraInfo);
    if (!managedCamera->camera) {
      Logger* logger = Logger::GetInstance();
      if (logger) {
        logger->LogError("Failed to create camera instance for: " + cameraInfo.id);
      }
      return false;
    }

    // Set up frame callback for broadcasting
    managedCamera->camera->SetFrameCallback([this](const CameraFrameData& frameData) {
      BroadcastFrame(frameData);
    });

    m_cameras[cameraInfo.id] = std::move(managedCamera);

    Logger* logger = Logger::GetInstance();
    if (logger) {
      std::string typeStr = CameraHardwareFactory::GetCameraTypeName(cameraInfo.type);
      logger->LogInfo("Added " + typeStr + " camera: " + cameraInfo.id);
    }

    return true;

  }
  catch (const std::exception& e) {
    Logger* logger = Logger::GetInstance();
    if (logger) {
      logger->LogError("Exception adding camera [" + cameraInfo.id + "]: " + std::string(e.what()));
    }
    return false;
  }
}

bool CameraManager::AddPylonCamera(const CameraInfo& cameraInfo) {
  ExtendedCameraInfo extendedInfo(cameraInfo, ICameraHardware::CameraType::PYLON);
  return AddCamera(extendedInfo);
}

bool CameraManager::AddIDSCamera(const CameraInfo& cameraInfo, int deviceId) {
  ExtendedCameraInfo extendedInfo(cameraInfo, ICameraHardware::CameraType::IDS, std::to_string(deviceId));
  return AddCamera(extendedInfo);
}

bool CameraManager::AddCameraAutoDetect(const CameraInfo& cameraInfo, const std::string& deviceInfo) {
  ICameraHardware::CameraType detectedType = CameraHardwareFactory::DetectCameraType(deviceInfo);

  if (detectedType == ICameraHardware::CameraType::UNKNOWN) {
    Logger* logger = Logger::GetInstance();
    if (logger) {
      logger->LogWarning("Could not auto-detect camera type for: " + cameraInfo.id + ", defaulting to Pylon");
    }
    detectedType = ICameraHardware::CameraType::PYLON;
  }

  ExtendedCameraInfo extendedInfo(cameraInfo, detectedType, deviceInfo);
  return AddCamera(extendedInfo);
}

bool CameraManager::RemoveCamera(const std::string& cameraId) {
  std::lock_guard<std::mutex> lock(m_camerasMutex);

  auto it = m_cameras.find(cameraId);
  if (it != m_cameras.end()) {
    // Stop camera if running
    if (it->second->camera) {
      if (it->second->camera->IsGrabbing()) {
        it->second->camera->StopGrabbing();
      }
      if (it->second->camera->IsConnected()) {
        it->second->camera->Disconnect();
      }
    }

    m_cameras.erase(it);

    Logger* logger = Logger::GetInstance();
    if (logger) {
      logger->LogInfo("Removed camera: " + cameraId);
    }
    return true;
  }

  return false;
}

// ============================================================================
// CAMERA ACCESS METHODS
// ============================================================================

ICameraHardware* CameraManager::GetCameraHardware(const std::string& cameraId) {
  std::lock_guard<std::mutex> lock(m_camerasMutex);

  auto it = m_cameras.find(cameraId);
  if (it != m_cameras.end()) {
    return it->second->camera.get();
  }
  return nullptr;
}

PylonCameraTest* CameraManager::GetCamera(const std::string& cameraId) {
  std::lock_guard<std::mutex> lock(m_camerasMutex);

  auto it = m_cameras.find(cameraId);
  if (it != m_cameras.end() && it->second->pylonCameraTest) {
    return it->second->pylonCameraTest.get();
  }
  return nullptr;  // Return nullptr for non-Pylon cameras
}

std::vector<std::string> CameraManager::GetCameraIds() const {
  std::lock_guard<std::mutex> lock(m_camerasMutex);

  std::vector<std::string> ids;
  for (const auto& [id, camera] : m_cameras) {
    ids.push_back(id);
  }
  return ids;
}

std::vector<std::string> CameraManager::GetCameraIdsByType(ICameraHardware::CameraType type) const {
  std::lock_guard<std::mutex> lock(m_camerasMutex);

  std::vector<std::string> result;
  for (const auto& [id, camera] : m_cameras) {
    if (camera->info.type == type) {
      result.push_back(id);
    }
  }
  return result;
}

// ============================================================================
// CAMERA OPERATIONS
// ============================================================================


bool CameraManager::InitializeAllCameras() {
  std::lock_guard<std::mutex> lock(m_camerasMutex);

  bool allSuccess = true;
  for (const auto& [id, managedCamera] : m_cameras) {
    if (managedCamera->camera) {
      // Direct call to avoid nested locking
      if (!managedCamera->camera->Initialize()) {
        allSuccess = false;
        Logger* logger = Logger::GetInstance();
        if (logger) {
          logger->LogError("Failed to initialize camera: " + id);
        }
      }
    }
  }

  return allSuccess;
}

bool CameraManager::ConnectCamera(const std::string& cameraId) {
  // Get camera without holding lock to avoid deadlock
  ICameraHardware* camera = nullptr;
  {
    std::lock_guard<std::mutex> lock(m_camerasMutex);
    auto it = m_cameras.find(cameraId);
    if (it != m_cameras.end()) {
      camera = it->second->camera.get();
    }
  }

  if (camera) {
    return camera->Initialize() && camera->Connect();
  }
  return false;
}

bool CameraManager::DisconnectCamera(const std::string& cameraId) {
  // Get camera without holding lock to avoid deadlock
  ICameraHardware* camera = nullptr;
  {
    std::lock_guard<std::mutex> lock(m_camerasMutex);
    auto it = m_cameras.find(cameraId);
    if (it != m_cameras.end()) {
      camera = it->second->camera.get();
    }
  }

  if (camera) {
    return camera->Disconnect();
  }
  return false;
}

bool CameraManager::StartGrabbing(const std::string& cameraId) {
  // Get camera without holding lock to avoid deadlock
  ICameraHardware* camera = nullptr;
  {
    std::lock_guard<std::mutex> lock(m_camerasMutex);
    auto it = m_cameras.find(cameraId);
    if (it != m_cameras.end()) {
      camera = it->second->camera.get();
    }
  }

  if (camera) {
    return camera->StartGrabbing();
  }
  return false;
}

bool CameraManager::StopGrabbing(const std::string& cameraId) {
  // Get camera without holding lock to avoid deadlock
  ICameraHardware* camera = nullptr;
  {
    std::lock_guard<std::mutex> lock(m_camerasMutex);
    auto it = m_cameras.find(cameraId);
    if (it != m_cameras.end()) {
      camera = it->second->camera.get();
    }
  }

  if (camera) {
    return camera->StopGrabbing();
  }
  return false;
}



bool CameraManager::StartGrabbingAll() {
  std::lock_guard<std::mutex> lock(m_camerasMutex);

  bool allSuccess = true;
  for (const auto& [id, managedCamera] : m_cameras) {
    if (managedCamera->camera && managedCamera->camera->IsConnected()) {
      if (!managedCamera->camera->StartGrabbing()) {
        allSuccess = false;
        Logger* logger = Logger::GetInstance();
        if (logger) {
          logger->LogError("Failed to start grabbing for camera: " + id);
        }
      }
    }
  }

  return allSuccess;
}

bool CameraManager::StopGrabbingAll() {
  std::lock_guard<std::mutex> lock(m_camerasMutex);

  bool allSuccess = true;
  for (const auto& [id, managedCamera] : m_cameras) {
    if (managedCamera->camera && managedCamera->camera->IsGrabbing()) {
      if (!managedCamera->camera->StopGrabbing()) {
        allSuccess = false;
        Logger* logger = Logger::GetInstance();
        if (logger) {
          logger->LogError("Failed to stop grabbing for camera: " + id);
        }
      }
    }
  }

  return allSuccess;
}


bool CameraManager::StartGrabbingWithBroadcast(const std::string& cameraId) {
  // Start broadcasting system if not running
  if (!m_broadcastSystemRunning.load()) {
    StartBroadcastSystem();
  }

  return StartGrabbing(cameraId);
}

// ============================================================================
// EXPOSURE CONTROL
// ============================================================================
// Add this method to CameraManager.cpp in the "EXPOSURE CONTROL" section
// This should go right after the existing ApplyExposureForNodeAll method

bool CameraManager::ApplyExposureSettings(const std::string& cameraId, const PylonCamera::ExposureSettings& settings) {
  // Legacy method - use PylonCameraTest for backward compatibility
  auto pylonCamera = GetCamera(cameraId);
  if (pylonCamera) {
    if (settings.exposure_auto)
    {
      return pylonCamera->GetCamera().SetExposureAuto(settings.exposure_auto);
    }
    else
    {
      return pylonCamera->GetCamera().SetExposureTime(settings.exposure_time);
    }
    
  }
  return false;
}


bool CameraManager::ApplyExposureForNode(const std::string& cameraId, const std::string& nodeId) {
  // This is for Pylon cameras with exposure manager - keep for compatibility
  auto pylonCamera = GetCamera(cameraId);
  if (pylonCamera) {
    return pylonCamera->ApplyExposureForNode(nodeId);
  }
  return false;
}

bool CameraManager::ApplyExposureForNodeAll(const std::string& nodeId) {
  std::lock_guard<std::mutex> lock(m_camerasMutex);

  bool allSuccess = true;
  for (const auto& [id, managedCamera] : m_cameras) {
    if (managedCamera->pylonCameraTest) {
      if (!managedCamera->pylonCameraTest->ApplyExposureForNode(nodeId)) {
        allSuccess = false;
      }
    }
  }

  return allSuccess;
}

bool CameraManager::ApplyExposureSettings(const std::string& cameraId, const ICameraHardware::ExposureSettings& settings) {
  auto camera = GetCameraHardware(cameraId);
  if (camera) {
    return camera->SetExposureSettings(settings);
  }
  return false;
}




// ============================================================================
// CAMERA STATUS
// ============================================================================

CameraManager::CameraStatus CameraManager::GetCameraStatus(const std::string& cameraId) const {
  CameraStatus status;
  status.id = cameraId;
  status.type = ICameraHardware::CameraType::UNKNOWN;
  status.connected = false;
  status.grabbing = false;
  status.deviceRemoved = true;

  std::lock_guard<std::mutex> lock(m_camerasMutex);
  auto it = m_cameras.find(cameraId);
  if (it != m_cameras.end()) {
    const auto& managedCamera = it->second;
    const auto& camera = managedCamera->camera;

    status.type = managedCamera->info.type;
    status.deviceInfo = managedCamera->info.deviceInfo;

    if (camera) {
      status.connected = camera->IsConnected();
      status.grabbing = camera->IsGrabbing();
      status.deviceRemoved = camera->IsDeviceRemoved();
      status.modelName = camera->GetModelName();
      status.serialNumber = camera->GetSerialNumber();
      status.currentExposure = camera->GetExposureSettings();
    }
  }

  return status;
}

// Replace GetAllCameraStatus method in CameraManager.cpp to fix the deadlock:

std::vector<CameraManager::CameraStatus> CameraManager::GetAllCameraStatus() const {
  std::vector<CameraStatus> statuses;

  std::lock_guard<std::mutex> lock(m_camerasMutex);

  // Direct iteration without calling GetCameraStatus to avoid nested locking
  for (const auto& [id, managedCamera] : m_cameras) {
    CameraStatus status;
    status.id = id;
    status.type = ICameraHardware::CameraType::UNKNOWN;
    status.connected = false;
    status.grabbing = false;
    status.deviceRemoved = true;

    if (managedCamera) {
      const auto& camera = managedCamera->camera;

      status.type = managedCamera->info.type;
      status.deviceInfo = managedCamera->info.deviceInfo;

      if (camera) {
        status.connected = camera->IsConnected();
        status.grabbing = camera->IsGrabbing();
        status.deviceRemoved = camera->IsDeviceRemoved();
        status.modelName = camera->GetModelName();
        status.serialNumber = camera->GetSerialNumber();
        status.currentExposure = camera->GetExposureSettings();
      }
    }

    statuses.push_back(status);
  }

  return statuses;
}
// ============================================================================
// CAMERA CONFIGURATION
// ============================================================================

bool CameraManager::SetCameraConfiguration(const std::string& cameraId, const std::string& key, const std::string& value) {
  auto camera = GetCameraHardware(cameraId);
  if (camera) {
    return camera->SetConfiguration(key, value);
  }
  return false;
}

std::string CameraManager::GetCameraConfiguration(const std::string& cameraId, const std::string& key) const {
  auto camera = const_cast<CameraManager*>(this)->GetCameraHardware(cameraId);
  if (camera) {
    return camera->GetConfiguration(key);
  }
  return "";
}

// ============================================================================
// BROADCASTING SYSTEM
// ============================================================================

void CameraManager::StartBroadcastSystem() {
  if (m_broadcastSystemRunning.exchange(true)) {
    return; // Already running
  }

  m_shouldStopBroadcast.store(false);
  m_broadcastThread = std::thread(&CameraManager::BroadcastThreadFunction, this);

  Logger* logger = Logger::GetInstance();
  if (logger) {
    logger->LogInfo("Camera broadcasting system started");
  }
}

void CameraManager::StopBroadcastSystem() {
  if (!m_broadcastSystemRunning.exchange(false)) {
    return; // Already stopped
  }

  m_shouldStopBroadcast.store(true);

  if (m_broadcastThread.joinable()) {
    m_broadcastThread.join();
  }

  Logger* logger = Logger::GetInstance();
  if (logger) {
    logger->LogInfo("Camera broadcasting system stopped");
  }
}

void CameraManager::SubscribeToFrames(std::shared_ptr<CameraFrameSubscriber> subscriber) {
  if (!subscriber) {
    return;
  }

  std::lock_guard<std::mutex> lock(m_subscribersMutex);
  m_subscribers[subscriber->GetSubscriberId()] = SubscriberInfo(subscriber);

  Logger* logger = Logger::GetInstance();
  if (logger) {
    logger->LogInfo("Subscriber added: " + subscriber->GetSubscriberId());
  }
}

void CameraManager::UnsubscribeFromFrames(const std::string& subscriberId) {
  std::lock_guard<std::mutex> lock(m_subscribersMutex);

  auto it = m_subscribers.find(subscriberId);
  if (it != m_subscribers.end()) {
    m_subscribers.erase(it);

    Logger* logger = Logger::GetInstance();
    if (logger) {
      logger->LogInfo("Subscriber removed: " + subscriberId);
    }
  }
}

void CameraManager::SetGlobalBroadcastRate(float fps) {
  m_globalBroadcastRate = fps;
}

size_t CameraManager::GetSubscriberCount() const {
  std::lock_guard<std::mutex> lock(m_subscribersMutex);
  return m_subscribers.size();
}

std::vector<std::string> CameraManager::GetSubscriberIds() const {
  std::lock_guard<std::mutex> lock(m_subscribersMutex);

  std::vector<std::string> ids;
  for (const auto& [id, info] : m_subscribers) {
    ids.push_back(id);
  }
  return ids;
}

void CameraManager::BroadcastFrame(const CameraFrameData& frameData) {
  std::lock_guard<std::mutex> lock(m_subscribersMutex);

  auto now = std::chrono::steady_clock::now();

  for (auto& [id, subscriberInfo] : m_subscribers) {
    auto subscriber = subscriberInfo.subscriber;
    if (!subscriber) {
      continue;
    }

    // Check if subscriber wants frames from this camera
    if (!subscriber->WantsFramesFromCamera(frameData.cameraId)) {
      continue;
    }

    // Check rate limiting
    int minInterval = subscriber->GetMinFrameIntervalMs();
    if (minInterval > 0) {
      auto timeSinceLastBroadcast = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - subscriberInfo.lastBroadcast).count();

      if (timeSinceLastBroadcast < minInterval) {
        continue; // Skip this frame for rate limiting
      }
    }

    // Send frame to subscriber
    try {
      subscriber->OnNewFrame(frameData);
      subscriberInfo.lastBroadcast = now;
    }
    catch (const std::exception& e) {
      Logger* logger = Logger::GetInstance();
      if (logger) {
        logger->LogError("Exception in subscriber [" + id + "]: " + std::string(e.what()));
      }
    }
  }
}

void CameraManager::BroadcastThreadFunction() {
  Logger* logger = Logger::GetInstance();
  if (logger) {
    logger->LogInfo("Camera broadcast thread started");
  }

  while (!m_shouldStopBroadcast.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    // The actual broadcasting is triggered by frame callbacks
    // This thread just keeps the system alive
  }

  if (logger) {
    logger->LogInfo("Camera broadcast thread stopped");
  }
}

// ============================================================================
// UI RENDERING
// ============================================================================

void CameraManager::RenderUI() {
  if (!m_showUI) {
    return;
  }

  ImGui::Begin("Camera Manager", &m_showUI);

  // System status
  ImGui::Text("Cameras: %zu", GetCameraCount());
  ImGui::Text("Subscribers: %zu", GetSubscriberCount());
  ImGui::Text("Broadcasting: %s", m_broadcastSystemRunning.load() ? "Active" : "Inactive");

  ImGui::Separator();

  // Broadcasting controls
  if (ImGui::CollapsingHeader("Broadcasting System")) {
    if (!m_broadcastSystemRunning.load()) {
      if (ImGui::Button("Start Broadcasting")) {
        StartBroadcastSystem();
      }
    }
    else {
      if (ImGui::Button("Stop Broadcasting")) {
        StopBroadcastSystem();
      }
    }

    float rate = m_globalBroadcastRate;
    if (ImGui::SliderFloat("Broadcast Rate (FPS)", &rate, 1.0f, 60.0f)) {
      SetGlobalBroadcastRate(rate);
    }
  }

  // Camera list
  if (ImGui::CollapsingHeader("Camera List", ImGuiTreeNodeFlags_DefaultOpen)) {
    std::lock_guard<std::mutex> lock(m_camerasMutex);

    for (const auto& [id, managedCamera] : m_cameras) {
      ImGui::PushID(id.c_str());

      auto status = GetCameraStatus(id);
      std::string typeName = CameraHardwareFactory::GetCameraTypeName(status.type);

      // Camera header
      bool nodeOpen = ImGui::TreeNode(id.c_str(), "%s (%s)", id.c_str(), typeName.c_str());

      // Status indicators
      ImGui::SameLine();
      if (status.connected) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "[CONNECTED]");
      }
      else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "[DISCONNECTED]");
      }

      if (status.grabbing) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0, 0, 1, 1), "[GRABBING]");
      }

      if (nodeOpen) {
        // Camera details
        ImGui::Text("Type: %s", typeName.c_str());
        ImGui::Text("Model: %s", status.modelName.c_str());
        ImGui::Text("Serial: %s", status.serialNumber.c_str());
        ImGui::Text("Device Info: %s", status.deviceInfo.c_str());

        // Camera controls
        if (!status.connected) {
          if (ImGui::Button("Connect")) {
            ConnectCamera(id);
          }
        }
        else {
          if (ImGui::Button("Disconnect")) {
            DisconnectCamera(id);
          }

          ImGui::SameLine();
          if (!status.grabbing) {
            if (ImGui::Button("Start Grabbing")) {
              StartGrabbing(id);
            }
          }
          else {
            if (ImGui::Button("Stop Grabbing")) {
              StopGrabbing(id);
            }
          }
        }

        // Exposure settings
        if (status.connected) {
          ImGui::Separator();
          ImGui::Text("Exposure Settings:");
          ImGui::Text("Exposure: %.1f μs", status.currentExposure.exposure_time);
          ImGui::Text("Gain: %.2f", status.currentExposure.gain);
          ImGui::Text("Auto Exposure: %s", status.currentExposure.auto_exposure ? "On" : "Off");
          ImGui::Text("Auto Gain: %s", status.currentExposure.auto_gain ? "On" : "Off");
        }

        ImGui::TreePop();
      }

      ImGui::PopID();
    }
  }

  // Global operations

// Global operations
  ImGui::Separator();
  ImGui::Text("Global Operations:");

  if (ImGui::Button("Discover & Add Cameras")) {
    Logger* logger = Logger::GetInstance();
    if (logger) {
      logger->LogInfo("Manual camera discovery triggered from UI");
    }
    DiscoverAndAddAllCameras();
  }

  ImGui::SameLine();
  if (ImGui::Button("Initialize All")) {
    InitializeAllCameras();
  }

  ImGui::SameLine();
  if (ImGui::Button("Connect All")) {
    // Try to connect all cameras
    std::lock_guard<std::mutex> lock(m_camerasMutex);
    for (const auto& [id, managedCamera] : m_cameras) {
      if (managedCamera->camera && !managedCamera->camera->IsConnected()) {
        if (managedCamera->camera->Initialize()) {
          managedCamera->camera->Connect();
        }
      }
    }
  }

  if (ImGui::Button("Start All Grabbing")) {
    StartGrabbingAll();
  }

  ImGui::SameLine();
  if (ImGui::Button("Stop All Grabbing")) {
    StopGrabbingAll();
  }

  ImGui::End();
}

// Add this implementation to CameraManager.cpp

// ============================================================================
// CAMERA DISCOVERY - Implementation 
// ============================================================================

std::vector<CameraInfo> CameraManager::DiscoverPylonCameras() {
  std::vector<CameraInfo> pylonCameras;

  Logger* logger = Logger::GetInstance();
  if (logger) {
    logger->LogInfo("Discovering Pylon cameras...");
  }

  try {
    // Initialize Pylon if not already done
    Pylon::PylonInitialize();

    // Get the transport layer factory
    Pylon::CTlFactory& tlFactory = Pylon::CTlFactory::GetInstance();

    // Enumerate all devices
    Pylon::DeviceInfoList_t devices;
    size_t numDevices = tlFactory.EnumerateDevices(devices);

    if (logger) {
      logger->LogInfo("Found " + std::to_string(numDevices) + " Pylon cameras");
    }

    for (size_t i = 0; i < numDevices; i++) {
      const auto& device = devices[i];

      std::string modelName = device.GetModelName().c_str();
      std::string serialNumber = device.GetSerialNumber().c_str();
      std::string deviceClass = device.GetDeviceClass().c_str();

      // Create camera info
      std::string cameraId = "Pylon_" + modelName + "_" + serialNumber;
      std::string description = modelName + " (" + deviceClass + ")";

      CameraInfo cameraInfo(cameraId, serialNumber, description);

      // Determine connection method based on device class
      if (deviceClass.find("GigE") != std::string::npos) {
        // For GigE cameras, try to get IP address
        try {
          // Create temporary camera to get IP info
          Pylon::CInstantCamera tempCamera;
          tempCamera.Attach(tlFactory.CreateDevice(device));
          tempCamera.Open();

          if (tempCamera.GetTLNodeMap().GetNode("GevCurrentIPAddress")) {
            Pylon::CIntegerParameter ipParam(tempCamera.GetTLNodeMap(), "GevCurrentIPAddress");
            if (ipParam.IsReadable()) {
              int64_t ipValue = ipParam.GetValue();
              int a = (ipValue >> 24) & 0xFF;
              int b = (ipValue >> 16) & 0xFF;
              int c = (ipValue >> 8) & 0xFF;
              int d = ipValue & 0xFF;
              std::string ipAddress = std::to_string(a) + "." + std::to_string(b) + "." +
                std::to_string(c) + "." + std::to_string(d);

              // Create IP-based camera info
              cameraInfo = CameraInfo::CreateByIP(cameraId, ipAddress, description);
            }
          }

          tempCamera.Close();
        }
        catch (...) {
          // If IP detection fails, use serial number method
          if (logger) {
            logger->LogWarning("Failed to get IP for GigE camera " + cameraId + ", using serial");
          }
        }
      }

      pylonCameras.push_back(cameraInfo);

      if (logger) {
        logger->LogInfo("Discovered Pylon camera: " + cameraId + " [" + cameraInfo.GetConnectionInfo() + "]");
      }
    }
  }
  catch (const std::exception& e) {
    if (logger) {
      logger->LogError("Exception during Pylon camera discovery: " + std::string(e.what()));
    }
  }

  return pylonCameras;
}



std::vector<ExtendedCameraInfo> CameraManager::DiscoverIDSCameras() {
  std::vector<ExtendedCameraInfo> idsCameras;

  Logger* logger = Logger::GetInstance();
  if (logger) {
    logger->LogInfo("Discovering IDS cameras...");
  }

  try {
    // Use IDSCameraTest to get available cameras
    std::vector<int> availableIds = IDSCameraTest::GetAvailableCameraIds();
    std::vector<std::string> availableSerials = IDSCameraTest::GetAvailableCameraSerials();

    if (logger) {
      logger->LogInfo("Found " + std::to_string(availableIds.size()) + " IDS cameras");
    }

    for (size_t i = 0; i < availableIds.size(); i++) {
      int deviceId = availableIds[i];
      std::string serial = (i < availableSerials.size()) ? availableSerials[i] : "Unknown";

      // Create camera info
      CameraInfo baseInfo("IDS_Camera_" + std::to_string(deviceId),
        "IDS Camera (ID: " + std::to_string(deviceId) + ")");

      ExtendedCameraInfo cameraInfo(baseInfo, ICameraHardware::CameraType::IDS, std::to_string(deviceId));
      cameraInfo.serialNumber = serial;

      idsCameras.push_back(cameraInfo);

      if (logger) {
        logger->LogInfo("Discovered IDS camera: ID=" + std::to_string(deviceId) + ", Serial=" + serial);
      }
    }
  }
  catch (const std::exception& e) {
    if (logger) {
      logger->LogError("Exception during IDS camera discovery: " + std::string(e.what()));
    }
  }

  return idsCameras;
}

std::vector<ExtendedCameraInfo> CameraManager::DiscoverAllCameras() {
  std::vector<ExtendedCameraInfo> allCameras;

  Logger* logger = Logger::GetInstance();
  if (logger) {
    logger->LogInfo("Discovering all cameras...");
  }

  // Discover Pylon cameras (convert CameraInfo to ExtendedCameraInfo)
  try {
    auto pylonCameras = DiscoverPylonCameras();
    for (const auto& pylonCamera : pylonCameras) {
      ExtendedCameraInfo extendedInfo(pylonCamera, ICameraHardware::CameraType::PYLON);
      allCameras.push_back(extendedInfo);
    }
  }
  catch (const std::exception& e) {
    if (logger) {
      logger->LogError("Error discovering Pylon cameras: " + std::string(e.what()));
    }
  }

  // Discover IDS cameras
  try {
    auto idsCameras = DiscoverIDSCameras();
    allCameras.insert(allCameras.end(), idsCameras.begin(), idsCameras.end());
  }
  catch (const std::exception& e) {
    if (logger) {
      logger->LogError("Error discovering IDS cameras: " + std::string(e.what()));
    }
  }

  if (logger) {
    logger->LogInfo("Total cameras discovered: " + std::to_string(allCameras.size()));
  }

  return allCameras;
}

// ============================================================================
// CAMERA INITIALIZATION WITH DISCOVERY
// ============================================================================

bool CameraManager::DiscoverAndAddAllCameras() {
  Logger* logger = Logger::GetInstance();
  if (logger) {
    logger->LogInfo("Starting camera discovery and addition...");
  }

  auto discoveredCameras = DiscoverAllCameras();

  bool allSuccess = true;
  for (const auto& cameraInfo : discoveredCameras) {
    if (!AddCamera(cameraInfo)) {
      allSuccess = false;
      if (logger) {
        logger->LogWarning("Failed to add discovered camera: " + cameraInfo.id);
      }
    }
  }

  if (logger) {
    logger->LogInfo("Camera discovery completed. Added " +
      std::to_string(GetCameraCount()) + " cameras.");
  }

  return allSuccess;
}


// Add these new methods to CameraManager class implementation

bool CameraManager::SaveFrameToBMP(const CameraFrameData& frameData, const std::string& filePath) {
  if (!frameData.IsValid()) {
    Logger* logger = Logger::GetInstance();
    if (logger) {
      logger->LogError("Cannot save invalid frame data to BMP");
    }
    return false;
  }

  // Only support RGB images for now
  if (frameData.channels != 3) {
    Logger* logger = Logger::GetInstance();
    if (logger) {
      logger->LogError("BMP save only supports RGB images (3 channels), got " +
        std::to_string(frameData.channels) + " channels");
    }
    return false;
  }

  try {
    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
      Logger* logger = Logger::GetInstance();
      if (logger) {
        logger->LogError("Failed to open file for writing: " + filePath);
      }
      return false;
    }

    // Calculate padded row size (BMP rows must be aligned to 4 bytes)
    int rowSize = ((frameData.width * 3 + 3) / 4) * 4;
    int paddingSize = rowSize - (frameData.width * 3);
    uint32_t imageSize = rowSize * frameData.height;

    // Prepare BMP headers
    BMPFileHeader fileHeader;
    fileHeader.size = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + imageSize;

    BMPInfoHeader infoHeader;
    infoHeader.width = frameData.width;
    infoHeader.height = frameData.height;
    infoHeader.imageSize = imageSize;

    // Write headers
    file.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
    file.write(reinterpret_cast<const char*>(&infoHeader), sizeof(infoHeader));

    // Write pixel data (BMP stores images bottom-to-top, BGR format)
    std::vector<uint8_t> padding(paddingSize, 0);

    for (int y = frameData.height - 1; y >= 0; y--) {
      for (int x = 0; x < static_cast<int>(frameData.width); x++) {
        int idx = (y * frameData.width + x) * 3;
        // Convert RGB to BGR for BMP format
        uint8_t b = frameData.imageData[idx + 2];
        uint8_t g = frameData.imageData[idx + 1];
        uint8_t r = frameData.imageData[idx];

        file.write(reinterpret_cast<const char*>(&b), 1);
        file.write(reinterpret_cast<const char*>(&g), 1);
        file.write(reinterpret_cast<const char*>(&r), 1);
      }
      // Write padding bytes
      if (paddingSize > 0) {
        file.write(reinterpret_cast<const char*>(padding.data()), paddingSize);
      }
    }

    file.close();

    Logger* logger = Logger::GetInstance();
    if (logger) {
      logger->LogInfo("Saved image to: " + filePath);
    }

    return true;
  }
  catch (const std::exception& e) {
    Logger* logger = Logger::GetInstance();
    if (logger) {
      logger->LogError("Exception saving BMP file: " + std::string(e.what()));
    }
    return false;
  }
}

std::string CameraManager::GenerateImageFilename(const std::string& cameraId, const std::string& extension) {
  // Get current time for timestamp
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << cameraId << "_";
	std::tm timeinfo;
	localtime_s(&timeinfo, &time_t);
  ss << std::put_time(&timeinfo, "%Y%m%d_%H%M%S");

  // Add milliseconds for uniqueness
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    now.time_since_epoch()) % 1000;
  ss << "_" << std::setfill('0') << std::setw(3) << ms.count();
  ss << "." << extension;

  return ss.str();
}

std::string CameraManager::GetImageOutputDirectory() {
  // Check if custom directory is set
  if (!m_imageOutputDirectory.empty()) {
    return m_imageOutputDirectory;
  }

  // Default to "captured_images" folder in current working directory
  std::string defaultDir = "captured_images";

  // Create directory if it doesn't exist
  try {
    if (!std::filesystem::exists(defaultDir)) {
      std::filesystem::create_directories(defaultDir);

      Logger* logger = Logger::GetInstance();
      if (logger) {
        logger->LogInfo("Created image output directory: " + defaultDir);
      }
    }
  }
  catch (const std::exception& e) {
    Logger* logger = Logger::GetInstance();
    if (logger) {
      logger->LogError("Failed to create output directory: " + std::string(e.what()));
    }
    // Fall back to current directory
    return ".";
  }

  return defaultDir;
}

void CameraManager::SetImageOutputDirectory(const std::string& directory) {
  m_imageOutputDirectory = directory;

  // Try to create the directory if it doesn't exist
  try {
    if (!std::filesystem::exists(directory)) {
      std::filesystem::create_directories(directory);

      Logger* logger = Logger::GetInstance();
      if (logger) {
        logger->LogInfo("Created image output directory: " + directory);
      }
    }
  }
  catch (const std::exception& e) {
    Logger* logger = Logger::GetInstance();
    if (logger) {
      logger->LogError("Failed to create output directory: " + std::string(e.what()));
    }
  }
}

// Replace the existing CaptureImage method with this enhanced version
bool CameraManager::CaptureImage(const std::string& cameraId) {
  auto camera = GetCameraHardware(cameraId);
  if (!camera) {
    Logger* logger = Logger::GetInstance();
    if (logger) {
      logger->LogError("Camera not found: " + cameraId);
    }
    return false;
  }

  // Capture frame from camera
  CameraFrameData frameData;
  frameData.cameraId = cameraId; // Set camera ID for the frame

  if (!camera->CaptureFrame(frameData)) {
    Logger* logger = Logger::GetInstance();
    if (logger) {
      logger->LogError("Failed to capture frame from camera: " + cameraId);
    }
    return false;
  }

  // Generate filename with timestamp
  std::string filename = GenerateImageFilename(cameraId, "bmp");
  std::string directory = GetImageOutputDirectory();
  std::string fullPath = directory + "/" + filename;

  // Save to BMP file
  if (!SaveFrameToBMP(frameData, fullPath)) {
    Logger* logger = Logger::GetInstance();
    if (logger) {
      logger->LogError("Failed to save captured image to file");
    }
    return false;
  }

  // Store the last captured image path for reference
  m_lastCapturedImagePath = fullPath;

  Logger* logger = Logger::GetInstance();
  if (logger) {
    logger->LogInfo("Image captured and saved: " + fullPath);
  }

  return true;
}

// Add this overload for capturing with custom filename
bool CameraManager::CaptureImage(const std::string& cameraId, const std::string& customFilename) {
  auto camera = GetCameraHardware(cameraId);
  if (!camera) {
    return false;
  }

  CameraFrameData frameData;
  frameData.cameraId = cameraId;

  if (!camera->CaptureFrame(frameData)) {
    return false;
  }

  std::string directory = GetImageOutputDirectory();
  std::string fullPath = directory + "/" + customFilename;

  if (!SaveFrameToBMP(frameData, fullPath)) {
    return false;
  }

  m_lastCapturedImagePath = fullPath;

  Logger* logger = Logger::GetInstance();
  if (logger) {
    logger->LogInfo("Image captured with custom name: " + fullPath);
  }

  return true;
}

// Enhanced CaptureImageAll to save all captured images
bool CameraManager::CaptureImageAll() {
  std::lock_guard<std::mutex> lock(m_camerasMutex);

  bool allSuccess = true;
  int captureCount = 0;

  for (const auto& [id, managedCamera] : m_cameras) {
    if (managedCamera->camera && managedCamera->camera->IsConnected()) {
      CameraFrameData frameData;
      frameData.cameraId = id;

      if (managedCamera->camera->CaptureFrame(frameData)) {
        // Generate unique filename for each camera
        std::string filename = GenerateImageFilename(id, "bmp");
        std::string directory = GetImageOutputDirectory();
        std::string fullPath = directory + "/" + filename;

        if (SaveFrameToBMP(frameData, fullPath)) {
          captureCount++;
          Logger* logger = Logger::GetInstance();
          if (logger) {
            logger->LogInfo("Captured image from " + id + ": " + fullPath);
          }
        }
        else {
          allSuccess = false;
        }
      }
      else {
        allSuccess = false;
        Logger* logger = Logger::GetInstance();
        if (logger) {
          logger->LogError("Failed to capture frame from camera: " + id);
        }
      }
    }
  }

  Logger* logger = Logger::GetInstance();
  if (logger) {
    logger->LogInfo("Captured " + std::to_string(captureCount) + " images from all cameras");
  }

  return allSuccess;
}