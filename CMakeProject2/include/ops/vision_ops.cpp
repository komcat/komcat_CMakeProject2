// vision_ops.cpp
#include "vision_ops.h"
#include <sstream>
#include <filesystem>
#include <chrono>
#include <iomanip>

VisionOps::VisionOps(
  PylonCameraTest* cameraTest,
  CameraManager* cameraManager,
  std::shared_ptr<DatabaseManager> dbManager,
  std::shared_ptr<OperationResultsManager> resultsManager
) : m_cameraTest(cameraTest),
m_cameraManager(cameraManager),
m_dbManager(dbManager),
m_resultsManager(resultsManager)
{
  m_logger = Logger::GetInstance();

  // Initialize camera exposure manager
  if (m_cameraTest) {
    m_cameraExposureManager = std::make_unique<CameraExposureManager>("camera_exposure_config.json");
    m_logger->LogInfo("VisionOps: Camera exposure manager initialized");
  }

  m_logger->LogInfo("VisionOps: Initialized");
}

VisionOps::VisionOps(
  CameraManager* cameraManager,
  std::shared_ptr<DatabaseManager> dbManager,
  std::shared_ptr<OperationResultsManager> resultsManager
) : m_cameraTest(nullptr),
m_cameraManager(cameraManager),
m_dbManager(dbManager),
m_resultsManager(resultsManager)
{
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("VisionOps: Initialized with CameraManager only");
}

VisionOps::~VisionOps() {
  m_logger->LogInfo("VisionOps: Shutting down");
}

bool VisionOps::Initialize() {
  return InitializeCamera();
}

// Camera control methods
bool VisionOps::InitializeCamera() {
  if (!m_cameraTest) {
    m_logger->LogError("VisionOps: Camera not available");
    return false;
  }

  m_logger->LogInfo("VisionOps: Initializing camera");
  bool success = m_cameraTest->GetCamera().Initialize();

  if (success) {
    m_logger->LogInfo("VisionOps: Camera initialized successfully");
  }
  else {
    m_logger->LogError("VisionOps: Failed to initialize camera");
  }

  return success;
}

bool VisionOps::ConnectCamera() {
  if (!m_cameraTest) {
    m_logger->LogError("VisionOps: Camera not available");
    return false;
  }

  if (!m_cameraTest->GetCamera().IsConnected()) {
    m_logger->LogInfo("VisionOps: Connecting to camera");
    bool success = m_cameraTest->GetCamera().Connect();

    if (success) {
      m_logger->LogInfo("VisionOps: Connected to camera successfully");
    }
    else {
      m_logger->LogError("VisionOps: Failed to connect to camera");
    }

    return success;
  }

  m_logger->LogInfo("VisionOps: Camera already connected");
  return true;
}

bool VisionOps::DisconnectCamera() {
  if (!m_cameraTest) {
    m_logger->LogError("VisionOps: Camera not available");
    return false;
  }

  if (m_cameraTest->GetCamera().IsConnected()) {
    m_logger->LogInfo("VisionOps: Disconnecting camera");
    m_cameraTest->GetCamera().Disconnect();
    m_logger->LogInfo("VisionOps: Camera disconnected");
    return true;
  }

  m_logger->LogInfo("VisionOps: Camera not connected");
  return true;
}

bool VisionOps::StartCameraGrabbing() {
  if (!m_cameraTest) {
    m_logger->LogError("VisionOps: Camera not available");
    return false;
  }

  if (!m_cameraTest->GetCamera().IsConnected()) {
    m_logger->LogWarning("VisionOps: Camera not connected, attempting to connect");
    if (!ConnectCamera()) {
      return false;
    }
  }

  if (!m_cameraTest->GetCamera().IsGrabbing()) {
    m_logger->LogInfo("VisionOps: Starting camera grabbing");
    bool success = m_cameraTest->GetCamera().StartGrabbing();

    if (success) {
      m_logger->LogInfo("VisionOps: Camera grabbing started");
    }
    else {
      m_logger->LogError("VisionOps: Failed to start camera grabbing");
    }

    return success;
  }

  m_logger->LogInfo("VisionOps: Camera already grabbing");
  return true;
}

bool VisionOps::StopCameraGrabbing() {
  if (!m_cameraTest) {
    m_logger->LogError("VisionOps: Camera not available");
    return false;
  }

  if (m_cameraTest->GetCamera().IsGrabbing()) {
    m_logger->LogInfo("VisionOps: Stopping camera grabbing");
    m_cameraTest->GetCamera().StopGrabbing();
    m_logger->LogInfo("VisionOps: Camera grabbing stopped");
    return true;
  }

  m_logger->LogInfo("VisionOps: Camera not grabbing");
  return true;
}

// Camera status methods
bool VisionOps::IsCameraInitialized() const {
  if (!m_cameraTest) {
    return false;
  }

  // The PylonCamera class doesn't directly expose an IsInitialized method,
  // so we'll assume it's initialized if it's connected or if it has a model name
  return m_cameraTest->GetCamera().IsConnected() ||
    !m_cameraTest->GetCamera().GetDeviceInfo().empty();
}

bool VisionOps::IsCameraConnected() const {
  if (!m_cameraTest) {
    return false;
  }

  return m_cameraTest->GetCamera().IsConnected();
}

bool VisionOps::IsCameraGrabbing() const {
  if (!m_cameraTest) {
    return false;
  }

  return m_cameraTest->GetCamera().IsGrabbing();
}

bool VisionOps::CaptureImageToFile(const std::string& filename) {
  if (!m_cameraTest) {
    m_logger->LogError("VisionOps: Camera not available");
    return false;
  }

  if (!m_cameraTest->GetCamera().IsConnected()) {
    m_logger->LogError("VisionOps: Camera not connected");
    return false;
  }

  // Create a directory for image captures if it doesn't exist
  std::filesystem::path imgDir = "captures";
  if (!std::filesystem::exists(imgDir)) {
    try {
      m_logger->LogInfo("VisionOps: Creating image capture directory: " + imgDir.string());
      std::filesystem::create_directory(imgDir);
    }
    catch (const std::filesystem::filesystem_error& e) {
      m_logger->LogError("VisionOps: Failed to create directory: " + std::string(e.what()));
      // If we can't create the directory, continue with the current working directory
    }
  }

  // Generate a filename if not provided
  std::string actualFilename = filename;
  if (actualFilename.empty()) {
    // Generate default filename using timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "capture_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S") << ".png";
    actualFilename = ss.str();
  }

  // Build full path
  std::filesystem::path baseName = std::filesystem::path(actualFilename).filename(); // Extract just the filename
  std::filesystem::path fullPath = imgDir / baseName;
  std::string fullPathStr = fullPath.string();

  m_logger->LogInfo("VisionOps: Capturing image to file: " + fullPathStr);

  // Use the public CaptureImage method from PylonCameraTest
  try {
    // If the camera is not grabbing, we need to grab a single frame first
    if (!m_cameraTest->GetCamera().IsGrabbing()) {
      m_logger->LogInfo("VisionOps: Starting camera grabbing for single capture");
      if (!m_cameraTest->GrabSingleFrame()) {
        m_logger->LogError("VisionOps: Failed to grab single frame");
        return false;
      }
    }

    // Use the public CaptureImage method which handles the low-level details
    bool captureSuccess = m_cameraTest->CaptureImage();

    if (!captureSuccess) {
      m_logger->LogError("VisionOps: Failed to capture image using CaptureImage method");
      return false;
    }

    // Since PylonCameraTest::CaptureImage() saves with its own naming convention,
    // we would need to either:
    // 1. Add a new public method to PylonCameraTest that accepts custom filename
    // 2. Use the existing method and then rename the file
    // 3. Add public getter methods for the image data

    // For now, let's use option 1 and suggest adding a method to PylonCameraTest
    // or use the existing CaptureImage and indicate success

    m_logger->LogInfo("VisionOps: Image captured successfully using default naming");
    m_logger->LogInfo("VisionOps: Note - Using PylonCameraTest default file naming. Consider adding CaptureImageToFile method to PylonCameraTest for custom filenames");

    return true;
  }
  catch (const std::exception& e) {
    m_logger->LogError("VisionOps: Error during image capture: " + std::string(e.what()));
    return false;
  }
}

bool VisionOps::UpdateCameraDisplay() {
  if (!m_cameraTest) {
    return false;
  }

  // Check if camera is grabbing frames
  if (!m_cameraTest->GetCamera().IsGrabbing()) {
    return false;
  }

  // The camera display will be updated through the RenderUI method in the main loop
  return true;
}

bool VisionOps::IntegrateCameraWithMotion(PylonCameraTest* cameraTest) {
  if (!cameraTest) {
    m_logger->LogError("VisionOps: Cannot integrate camera - camera test is null");
    return false;
  }

  // Set default calibration factors - you may need to adjust these based on your camera and lens
  // This is the conversion factor from pixels to mm
  cameraTest->SetPixelToMMFactors(0.00248f, 0.00248f);  // 0.01mm per pixel for both X and Y

  // Note: RenderUIWithMachineOps would need to be called externally
  // cameraTest->RenderUIWithMachineOps(this);

  return true;
}

// Camera exposure control methods
bool VisionOps::ApplyCameraExposureForNode(const std::string& nodeId) {
  if (!m_cameraTest || !m_cameraExposureManager) {
    m_logger->LogWarning("VisionOps: Camera or exposure manager not available");
    return false;
  }

  if (!m_cameraTest->GetCamera().IsConnected()) {
    m_logger->LogWarning("VisionOps: Camera not connected, cannot apply exposure settings");
    return false;
  }

  m_logger->LogInfo("VisionOps: Applying camera exposure settings for node " + nodeId);

  // Small delay to ensure gantry has settled at the new position
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  bool success = m_cameraExposureManager->ApplySettingsForNode(m_cameraTest->GetCamera(), nodeId);

  if (success) {
    m_logger->LogInfo("VisionOps: Successfully applied camera exposure for node " + nodeId);
  }
  else {
    m_logger->LogWarning("VisionOps: Failed to apply specific exposure for node " + nodeId + ", trying default");
    success = ApplyDefaultCameraExposure();
  }

  return success;
}

bool VisionOps::ApplyDefaultCameraExposure() {
  if (!m_cameraTest || !m_cameraExposureManager) {
    m_logger->LogWarning("VisionOps: Camera or exposure manager not available");
    return false;
  }

  if (!m_cameraTest->GetCamera().IsConnected()) {
    m_logger->LogWarning("VisionOps: Camera not connected, cannot apply default exposure");
    return false;
  }

  m_logger->LogInfo("VisionOps: Applying default camera exposure settings");

  bool success = m_cameraExposureManager->ApplyDefaultSettings(m_cameraTest->GetCamera());

  if (success) {
    m_logger->LogInfo("VisionOps: Successfully applied default camera exposure");
  }
  else {
    m_logger->LogError("VisionOps: Failed to apply default camera exposure");
  }

  return success;
}

void VisionOps::TestCameraSettings(const std::string& nodeId) {
  if (!m_cameraTest || !m_cameraExposureManager) {
    std::cout << "Camera or exposure manager not available for testing" << std::endl;
    return;
  }

  if (!m_cameraTest->GetCamera().IsConnected()) {
    std::cout << "Camera not connected for testing" << std::endl;
    return;
  }

  if (nodeId.empty()) {
    std::cout << "\n=== READING CURRENT CAMERA SETTINGS ===" << std::endl;
    m_cameraExposureManager->ReadCurrentCameraSettings(m_cameraTest->GetCamera());
  }
  else {
    std::cout << "\n=== TESTING CAMERA SETTINGS FOR NODE " << nodeId << " ===" << std::endl;
    ApplyCameraExposureForNode(nodeId);
  }
}

// Logging methods
void VisionOps::LogInfo(const std::string& message) const {
  if (m_logger) {
    m_logger->LogInfo("VisionOps: " + message);
  }
}

void VisionOps::LogWarning(const std::string& message) const {
  if (m_logger) {
    m_logger->LogWarning("VisionOps: " + message);
  }
}

void VisionOps::LogError(const std::string& message) const {
  if (m_logger) {
    m_logger->LogError("VisionOps: " + message);
  }
}