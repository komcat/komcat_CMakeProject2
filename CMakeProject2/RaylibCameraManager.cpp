#include "RaylibCameraManager.h"
#include "include/camera/CameraManager.h"
#include <thread>
#include <chrono>

RaylibCameraManager::RaylibCameraManager(AppContext& context, Logger* logger)
  : m_context(context), m_logger(logger) {
  if (!m_logger) {
    m_logger = Logger::GetInstance();
  }
}

RaylibCameraManager::~RaylibCameraManager() {
  Shutdown();
}

bool RaylibCameraManager::Initialize(const InitializationOptions& options) {
  LogInfo("=== Initializing RaylibCameraManager ===");

  if (m_initialized) {
    LogError("Already initialized");
    return false;
  }

  // Step 1: Setup camera feed first
  if (!SetupCameraFeed()) {
    LogError("Failed to setup camera feed");
    return false;
  }

  // Step 2: Setup raylib window if enabled
  if (options.enableRaylib3D) {
    if (!SetupRaylibWindow()) {
      LogError("Failed to setup raylib window");
      return false;
    }
  }

  // Step 3: Setup debug window if enabled
  if (options.enableDebugWindow) {
    if (!SetupDebugWindow()) {
      LogError("Failed to setup debug window");
      return false;
    }
  }

  // Step 4: Auto-connect camera if enabled
  if (options.autoConnectCamera) {
    AutoConnectCamera();
  }

  m_initialized = true;
  LogInfo("✅ RaylibCameraManager initialized successfully");
  return true;
}

void RaylibCameraManager::Shutdown() {
  if (!m_initialized) {
    return;
  }

  LogInfo("Shutting down RaylibCameraManager...");

  // Shutdown raylib window
  if (m_raylibWindow) {
    LogInfo("Shutting down Raylib window thread...");
    m_raylibWindow->Shutdown();
    m_raylibWindow.reset();
  }

  // Reset other components
  m_raylibDebugWindow.reset();
  m_raylibCameraFeed.reset();

  m_initialized = false;
  LogInfo("RaylibCameraManager shutdown complete");
}

bool RaylibCameraManager::SetupCameraFeed() {
  LogInfo("Setting up camera feed...");

  auto* cameraManager = m_context.GetCameraManager();
  if (!cameraManager || cameraManager->GetCameraCount() == 0) {
    if (!cameraManager) {
      LogInfo("No camera manager available for raylib integration");
    }
    else {
      LogInfo("No cameras available for raylib integration");
    }
    return true; // Not an error, just no cameras
  }

  LogInfo("Setting up camera feed for raylib 3D window...");

  // Create camera feed display
  m_raylibCameraFeed = std::make_unique<CameraFeedDisplay>();

  // Connect to broadcasting system instead of direct camera
  auto cameraIds = cameraManager->GetCameraIds();
  if (cameraIds.empty()) {
    LogError("No camera IDs found");
    return false;
  }

  // Try to find a connected camera
  std::string selectedCameraId;
  for (const auto& cameraId : cameraIds) {
    auto status = cameraManager->GetCameraStatus(cameraId);
    if (status.connected) {
      selectedCameraId = cameraId;
      break;
    }
  }

  // If no connected camera found, use the first one anyway
  if (selectedCameraId.empty()) {
    selectedCameraId = cameraIds[0];
    LogInfo("No connected cameras found, will try to connect to: " + selectedCameraId);
  }

  LogInfo("=== SETTING UP CAMERA FEED WITH BROADCASTING ===");

  // Set the target camera for the feed display
  m_raylibCameraFeed->SetTargetCamera(selectedCameraId);
  LogInfo("CameraFeedDisplay set to target camera: " + selectedCameraId);

  // Subscribe the CameraFeedDisplay to the broadcasting system
  std::shared_ptr<CameraFrameSubscriber> feedSubscriber =
    std::static_pointer_cast<CameraFrameSubscriber>(
      std::shared_ptr<CameraFeedDisplay>(m_raylibCameraFeed.get(), [](CameraFeedDisplay*) {}));
  cameraManager->SubscribeToFrames(feedSubscriber);
  LogInfo("CameraFeedDisplay subscribed to broadcasting system");

  // Start the broadcasting system if not already started
  cameraManager->StartBroadcastSystem();
  LogInfo("Camera broadcasting system started");

  // Try to auto-start the camera if it's connected
  PylonCameraTest* selectedCamera = cameraManager->GetCamera(selectedCameraId);
  if (selectedCamera) {
    auto& pylonCamera = selectedCamera->GetCamera();
    if (pylonCamera.IsConnected()) {
      if (!pylonCamera.IsGrabbing()) {
        // Use StartGrabbing which automatically sets up broadcasting
        if (cameraManager->StartGrabbing(selectedCameraId)) {
          LogInfo("Started camera grabbing with broadcasting for raylib feed");
        }
        else {
          LogInfo("Failed to start camera grabbing for raylib feed");
        }
      }
      else {
        LogInfo("Camera already grabbing - should be broadcasting");
      }
    }
    else {
      LogInfo("Camera not connected yet - feed will activate when camera connects");
    }
  }

  LogInfo("=== CAMERA FEED SETUP COMPLETE ===");
  return true;
}

bool RaylibCameraManager::SetupRaylibWindow() {
  LogInfo("Setting up raylib window...");

  m_raylibWindow = std::make_unique<RaylibWindow>();

  // Set the logger first
  m_raylibWindow->SetLogger(m_logger);

  // Set connections using AppContext
  auto* piControllerManager = m_context.GetPIController();
  if (piControllerManager) {
    m_raylibWindow->SetPIControllerManager(piControllerManager);
  }

  if (m_dataStore) {
    m_raylibWindow->SetDataStore(m_dataStore);
  }

  auto* machineOps = m_context.GetMachineOperations();
  if (machineOps) {
    m_raylibWindow->SetMachineOperations(machineOps);
  }

  // Set camera feed if available
  if (m_raylibCameraFeed) {
    m_raylibWindow->SetCameraFeedDisplay(m_raylibCameraFeed.get());
    LogInfo("Connected camera feed to raylib window");
  }
  else {
    LogInfo("No camera feed available for raylib window");
  }

  // Initialize the thread (this starts the render loop)
  if (m_raylibWindow->Initialize()) {
    LogInfo("✅ Raylib 3D Window thread started successfully");
    return true;
  }
  else {
    LogError("❌ Failed to start Raylib 3D Window");
    m_raylibWindow.reset(); // Clean up on failure
    return false;
  }
}

bool RaylibCameraManager::SetupDebugWindow() {
  LogInfo("Setting up raylib debug window...");

  m_raylibDebugWindow = std::make_unique<RaylibDebugWindow>();

  // Set up the raylib debug window using AppContext
  auto* cameraManager = m_context.GetCameraManager();
  if (cameraManager) {
    m_raylibDebugWindow->SetCameraManager(cameraManager);
  }

  if (m_raylibWindow) {
    m_raylibDebugWindow->SetRaylibWindow(m_raylibWindow.get());
  }

  if (m_raylibCameraFeed) {
    m_raylibDebugWindow->SetCameraFeedDisplay(m_raylibCameraFeed.get());
  }

  m_raylibDebugWindow->SetLogger(m_logger);

  LogInfo("✅ Raylib debug window setup complete");
  return true;
}

bool RaylibCameraManager::AutoConnectCamera() {
  LogInfo("=== AUTO-CONNECTING FIRST CAMERA ===");

  auto* cameraManager = m_context.GetCameraManager();
  if (!cameraManager || cameraManager->GetCameraCount() == 0) {
    LogInfo("No cameras available for auto-connection");
    return false;
  }

  auto cameraIds = cameraManager->GetCameraIds();
  bool connectedSuccessfully = false;

  // Try each camera until one connects successfully
  for (const auto& cameraId : cameraIds) {
    LogInfo("Attempting to auto-connect camera: " + cameraId);

    // Try to connect the camera
    if (cameraManager->ConnectCamera(cameraId)) {
      LogInfo("Successfully connected camera: " + cameraId);

      // Wait a moment for connection to stabilize
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      // Try to start grabbing (which enables broadcasting)
      if (cameraManager->StartGrabbing(cameraId)) {
        LogInfo("Successfully started grabbing for camera: " + cameraId);
        m_connectedCameraId = cameraId;
        connectedSuccessfully = true;
        break; // Success! Stop trying other cameras
      }
      else {
        LogInfo("Failed to start grabbing for camera: " + cameraId);
        // Try to disconnect cleanly before trying next camera
        cameraManager->DisconnectCamera(cameraId);
      }
    }
    else {
      LogInfo("Failed to connect camera: " + cameraId);
    }
  }

  if (connectedSuccessfully) {
    LogInfo("=== AUTO-CONNECTION SUCCESS ===");
    LogInfo("Connected camera: " + m_connectedCameraId);

    // Start the broadcasting system
    cameraManager->StartBroadcastSystem();
    LogInfo("Broadcasting system started");

    // Set the raylib debug window to this camera
    if (m_raylibDebugWindow) {
      m_raylibDebugWindow->SelectCamera(m_connectedCameraId);
      LogInfo("Raylib debug window set to camera: " + m_connectedCameraId);
    }

    // Set the main camera feed
    if (m_raylibCameraFeed) {
      m_raylibCameraFeed->SetTargetCamera(m_connectedCameraId);
      LogInfo("Raylib camera feed set to camera: " + m_connectedCameraId);
    }

    LogInfo("=== AUTO-CONNECTION COMPLETE ===");
    return true;
  }
  else {
    LogInfo("=== AUTO-CONNECTION FAILED ===");
    LogInfo("Could not connect to any cameras automatically");
    LogInfo("You will need to connect manually in the debug window");
    return false;
  }
}

void RaylibCameraManager::UpdateMachineData(const MachineData& data) {
  if (m_raylibWindow && m_raylibWindow->IsRunning()) {
    // Update 3D visualization (thread-safe)
    m_raylibWindow->UpdateMachineData(data);
  }
}

void RaylibCameraManager::RenderDebugUI(MenuManagerUaa3* menuManager) {
  if (!m_raylibDebugWindow || !menuManager) {
    return;
  }

  if (menuManager->IsRaylibDebugVisible() && m_raylibDebugWindow->IsVisible()) {
    bool isOpen = m_raylibDebugWindow->IsVisible();

    if (ImGui::Begin("Raylib Live Feed Debug", &isOpen)) {
      m_raylibDebugWindow->RenderUI();
    }
    ImGui::End();

    // Update visibility state
    m_raylibDebugWindow->SetVisible(isOpen);

    // Update menu state when window is closed
    if (!isOpen) {
      menuManager->SetRaylibDebugVisible(false);
    }
  }
}

bool RaylibCameraManager::ShouldClose() const {
  return m_raylibWindow && m_raylibWindow->ShouldClose();
}

bool RaylibCameraManager::IsRunning() const {
  return m_raylibWindow && m_raylibWindow->IsRunning();
}

void RaylibCameraManager::DebugCameraFeedSetup() {
  if (!m_logger) return;

  LogInfo("=== ENHANCED CAMERA FEED DEBUG ===");

  auto* cameraManager = m_context.GetCameraManager();
  if (!cameraManager) {
    LogError("CameraManager is null");
    return;
  }

  LogInfo("Camera count: " + std::to_string(cameraManager->GetCameraCount()));
  LogInfo("Subscriber count: " + std::to_string(cameraManager->GetSubscriberCount()));
  LogInfo("Broadcasting active: " + std::string(cameraManager->GetSubscriberCount() > 0 ? "YES" : "NO"));

  auto cameraIds = cameraManager->GetCameraIds();
  for (const auto& id : cameraIds) {
    auto status = cameraManager->GetCameraStatus(id);
    LogInfo("Camera " + id + ":");
    LogInfo("  Connected: " + std::string(status.connected ? "Yes" : "No"));
    LogInfo("  Grabbing: " + std::string(status.grabbing ? "Yes" : "No"));
    LogInfo("  Device: " + status.deviceInfo);
  }

  if (m_raylibCameraFeed) {
    LogInfo("=== CAMERA FEED DISPLAY STATUS ===");
    LogInfo("Feed has source: " + std::string(m_raylibCameraFeed->HasSource() ? "Yes" : "No"));
    LogInfo("Feed has texture: " + std::string(m_raylibCameraFeed->HasValidTexture() ? "Yes" : "No"));
    LogInfo("Feed status: " + m_raylibCameraFeed->GetStatusText());
    LogInfo("Feed receiving frames: " + std::string(m_raylibCameraFeed->IsReceivingFrames() ? "Yes" : "No"));
    LogInfo("Feed total frames: " + std::to_string(m_raylibCameraFeed->GetTotalFramesReceived()));
    LogInfo("Feed frame rate: " + std::to_string(m_raylibCameraFeed->GetActualFrameRate()) + " fps");
    LogInfo("Feed subscriber ID: " + m_raylibCameraFeed->GetSubscriberId());
  }

  if (m_raylibWindow) {
    LogInfo("=== RAYLIB WINDOW STATUS ===");
    LogInfo("Raylib has camera feed: " + std::string(m_raylibWindow->HasCameraFeed() ? "Yes" : "No"));
    LogInfo("Raylib feed visible: " + std::string(m_raylibWindow->IsCameraFeedVisible() ? "Yes" : "No"));
  }

  LogInfo("=== END ENHANCED DEBUG ===");
}

void RaylibCameraManager::LogError(const std::string& message) {
  m_lastError = message;
  if (m_logger) {
    m_logger->LogError("RaylibCameraManager: " + message);
  }
}

void RaylibCameraManager::LogInfo(const std::string& message) {
  if (m_logger) {
    m_logger->LogInfo("RaylibCameraManager: " + message);
  }
}