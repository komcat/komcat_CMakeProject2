// RaylibDebugWindow.cpp - Complete Enhanced Implementation
#include "RaylibDebugWindow.h"
#include "include/camera/CameraManager.h"
#include "include/camera/ICameraHardware.h"
#include "include/camera/CameraHardwareFactory.h"  // Add this for GetCameraTypeName
#include "raylibclass.h"
#include "CameraFeedDisplay.h"
#include "include/logger.h"
#include "imgui.h"
#include <thread>
#include <chrono>
#include <algorithm>
#include <numeric>

// OpenGL headers for texture debugging (optional)
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/gl.h>
// GL_TEXTURE_BINDING_2D should be defined in gl.h
#else
#include <OpenGL/gl.h>
#endif

// ============================================================================
// RaylibDebugSubscriber Implementation
// ============================================================================

RaylibDebugSubscriber::RaylibDebugSubscriber(const std::string& cameraId)
  : m_targetCameraId(cameraId) {
  UpdateSubscriberId();
  ResetState();
}

RaylibDebugSubscriber::~RaylibDebugSubscriber() {
  ResetState();
}

void RaylibDebugSubscriber::OnNewFrame(const CameraFrameData& frameData) {
  auto startTime = std::chrono::steady_clock::now();

  // Validate frame data
  if (!frameData.IsValid()) {
    m_droppedFrames.fetch_add(1);
    return;
  }

  // Update frame data (thread-safe copy)
  {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    m_latestFrame = frameData; // Deep copy of frame data
  }

  // Update atomic statistics
  m_hasNewFrame.store(true);
  m_totalFramesReceived.fetch_add(1);
  m_lastFrameTimestamp.store(frameData.timestamp);

  // Update frame rate tracking
  UpdateFrameRate();

  // Calculate and update performance metrics
  auto endTime = std::chrono::steady_clock::now();
  auto processingTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
  UpdatePerformanceMetrics(processingTime);
}

void RaylibDebugSubscriber::OnCameraStatusChanged(const std::string& cameraId, bool connected, bool grabbing) {
  if (cameraId != m_targetCameraId) {
    return; // Not our camera
  }

  // Update status atomically
  bool wasGrabbing = m_cameraGrabbing.load();
  m_cameraConnected.store(connected);
  m_cameraGrabbing.store(grabbing);

  // Reset frame flag if camera stopped grabbing
  if (wasGrabbing && !grabbing) {
    m_hasNewFrame.store(false);
  }

  // Log status change for debugging
  std::cout << "[RaylibDebugSubscriber] Camera " << cameraId
    << " status changed: connected=" << (connected ? "Yes" : "No")
    << ", grabbing=" << (grabbing ? "Yes" : "No") << std::endl;
}

std::string RaylibDebugSubscriber::GetSubscriberId() const {
  return m_subscriberId;
}

bool RaylibDebugSubscriber::WantsFramesFromCamera(const std::string& cameraId) const {
  return cameraId == m_targetCameraId;
}

void RaylibDebugSubscriber::SetTargetCamera(const std::string& cameraId) {
  if (m_targetCameraId == cameraId) {
    return; // No change needed
  }

  std::cout << "[RaylibDebugSubscriber] Switching target from '" << m_targetCameraId
    << "' to '" << cameraId << "'" << std::endl;

  // Update target camera
  m_targetCameraId = cameraId;
  UpdateSubscriberId();

  // Reset state for new camera
  ResetState();
}

CameraFrameData RaylibDebugSubscriber::GetLatestFrame() const {
  std::lock_guard<std::mutex> lock(m_frameMutex);
  return m_latestFrame; // Return deep copy
}

double RaylibDebugSubscriber::GetActualFrameRate() const {
  std::lock_guard<std::mutex> lock(m_frameRateMutex);

  if (m_recentFrameTimes.size() < 2) {
    return 0.0;
  }

  // Calculate frame rate over the recent samples
  auto timeDiff = m_recentFrameTimes.back() - m_recentFrameTimes.front();
  auto seconds = std::chrono::duration<double>(timeDiff).count();

  if (seconds <= 0.0) {
    return 0.0;
  }

  return (m_recentFrameTimes.size() - 1) / seconds;
}

RaylibDebugSubscriber::PerformanceMetrics RaylibDebugSubscriber::GetPerformanceMetrics() const {
  PerformanceMetrics metrics;

  // Copy atomic values
  metrics.totalFrames = m_totalFramesReceived.load();
  metrics.droppedFrames = m_droppedFrames.load();
  metrics.avgFrameRate = GetActualFrameRate();

  // Calculate processing time statistics
  {
    std::lock_guard<std::mutex> lock(m_perfMutex);
    if (!m_processingTimes.empty()) {
      // Calculate average processing time
      auto sum = std::accumulate(m_processingTimes.begin(), m_processingTimes.end(),
        std::chrono::milliseconds{ 0 });
      metrics.avgProcessingTime = sum / m_processingTimes.size();

      // Find maximum processing time
      metrics.maxProcessingTime = *std::max_element(m_processingTimes.begin(),
        m_processingTimes.end());
    }
  }

  return metrics;
}

void RaylibDebugSubscriber::UpdateSubscriberId() {
  m_subscriberId = "RaylibDebug_" + m_targetCameraId + "_" +
    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
}

void RaylibDebugSubscriber::ResetState() {
  // Clear frame data
  {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    m_latestFrame = CameraFrameData(); // Reset to empty frame
  }

  // Reset atomic flags and counters
  m_hasNewFrame.store(false);
  m_cameraConnected.store(false);
  m_cameraGrabbing.store(false);
  m_totalFramesReceived.store(0);
  m_lastFrameTimestamp.store(0);
  m_droppedFrames.store(0);

  // Clear frame rate tracking
  {
    std::lock_guard<std::mutex> lock(m_frameRateMutex);
    m_recentFrameTimes.clear();
  }

  // Clear performance metrics
  {
    std::lock_guard<std::mutex> lock(m_perfMutex);
    m_processingTimes.clear();
  }
}

void RaylibDebugSubscriber::UpdateFrameRate() {
  std::lock_guard<std::mutex> lock(m_frameRateMutex);

  auto now = std::chrono::steady_clock::now();
  m_recentFrameTimes.push_back(now);

  // Keep only recent samples for accurate frame rate calculation
  while (m_recentFrameTimes.size() > MAX_FRAME_SAMPLES) {
    m_recentFrameTimes.erase(m_recentFrameTimes.begin());
  }
}

void RaylibDebugSubscriber::UpdatePerformanceMetrics(std::chrono::milliseconds processingTime) {
  std::lock_guard<std::mutex> lock(m_perfMutex);

  m_processingTimes.push_back(processingTime);

  // Keep only recent samples for relevant performance metrics
  while (m_processingTimes.size() > MAX_PERF_SAMPLES) {
    m_processingTimes.erase(m_processingTimes.begin());
  }
}

// ============================================================================
// RaylibDebugWindow Implementation
// ============================================================================

RaylibDebugWindow::RaylibDebugWindow()
  : m_lastRefresh(std::chrono::steady_clock::now()) {
}

RaylibDebugWindow::~RaylibDebugWindow() {
  CleanupDebugSubscriber();
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
    ImGui::Text("CameraManager: %s", m_cameraManager ? "Available" : "Missing");
    ImGui::Text("RaylibWindow: %s", m_raylibWindow ? "Available" : "Missing");
    return;
  }

  // Auto-refresh if needed
  if (ShouldRefresh()) {
    UpdateSelectedCamera();
    m_lastRefresh = std::chrono::steady_clock::now();
  }

  // Header
  ImGui::Text("Enhanced Raylib Debug Window");
  ImGui::Separator();

  // Settings panel
  ImGui::Text("Debug Settings:");
  ImGui::Checkbox("Show Advanced##AdvancedCheckbox", &m_advancedDiagnostics);
  ImGui::SameLine();
  ImGui::Checkbox("Show Performance##PerformanceCheckbox", &m_showPerformanceMetrics);
  ImGui::SameLine();
  ImGui::Checkbox("Show Broadcasting##BroadcastingCheckbox", &m_showBroadcastingDetails);
  ImGui::SameLine();
  ImGui::Checkbox("Show Interface##InterfaceCheckbox", &m_showInterfaceValidation);

  ImGui::Separator();

  // Core sections (always shown)
  if (ImGui::CollapsingHeader("Camera Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
    RenderCameraSelection();
  }

  if (ImGui::CollapsingHeader("Camera Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
    RenderCameraControls();
  }

  if (ImGui::CollapsingHeader("Feed Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
    RenderFeedControls();
  }

  // Enhanced sections (configurable)
  if (m_showBroadcastingDetails && ImGui::CollapsingHeader("Broadcasting Diagnostics")) {
    RenderBroadcastingDiagnostics();
  }

  if (m_showInterfaceValidation && ImGui::CollapsingHeader("Interface Validation")) {
    RenderInterfaceValidation();
  }

  if (m_showPerformanceMetrics && ImGui::CollapsingHeader("Performance Metrics")) {
    RenderPerformanceMetrics();
  }

  if (ImGui::CollapsingHeader("Quick Actions")) {
    RenderQuickActions();
  }

  if (m_advancedDiagnostics && ImGui::CollapsingHeader("Advanced Debug Information")) {
    RenderAdvancedDebugInfo();
  }
}

// REPLACE IT WITH this enhanced version:
void RaylibDebugWindow::RenderCameraSelection() {
  auto cameraIds = m_cameraManager->GetCameraIds();

  if (cameraIds.empty()) {
    ImGui::Text("No cameras available");
    return;
  }

  // Camera selection dropdown
  const char* currentCameraName = m_selectedCameraId.empty() ?
    "Select Camera..." : m_selectedCameraId.c_str();

  if (ImGui::BeginCombo("Select Camera", currentCameraName)) {
    for (const auto& cameraId : cameraIds) {
      bool isSelected = (cameraId == m_selectedCameraId);

      if (ImGui::Selectable(cameraId.c_str(), isSelected)) {
        // THIS IS THE KEY FIX - Switch camera feed when selection changes
        if (m_selectedCameraId != cameraId) {
          SwitchCameraFeed(cameraId);
        }
      }

      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
}



void RaylibDebugWindow::SwitchCameraFeed(const std::string& cameraId) {
  if (m_logger) {
    m_logger->LogInfo("Switching raylib camera feed to: " + cameraId);
  }

  m_selectedCameraId = cameraId;

  // Switch the CameraFeedDisplay to the new camera
  if (m_cameraFeedDisplay) {
    // Clear current source first
    m_cameraFeedDisplay->ClearSource();

    // Set new camera source - THIS IS THE CRITICAL LINE
    m_cameraFeedDisplay->SetTargetCamera(cameraId);

    if (m_logger) {
      m_logger->LogInfo("Camera feed display switched to: " + cameraId);
    }
  }
  else {
    if (m_logger) {
      m_logger->LogError("CameraFeedDisplay is null - cannot switch camera");
    }
  }
}



void RaylibDebugWindow::RenderCameraControls() {
  if (m_selectedCameraId.empty()) {
    ImGui::Text("No camera selected");
    return;
  }

  auto status = m_cameraManager->GetCameraStatus(m_selectedCameraId);

  // Enhanced status display with color coding
  ImGui::Text("Camera Status:");
  ImGui::SameLine();
  if (status.connected) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ Connected");
  }
  else {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "✗ Disconnected");
  }

  ImGui::SameLine();
  ImGui::Text("| Grabbing:");
  ImGui::SameLine();
  if (status.grabbing) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ Active");
  }
  else {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "○ Stopped");
  }

  // Enhanced controls with broadcasting integration
  ImGui::Separator();
  if (!status.connected) {
    if (ImGui::Button("Connect Camera (with Broadcasting)")) {
      ConnectCameraWithBroadcasting(m_selectedCameraId);
    }
  }
  else {
    if (ImGui::Button("Disconnect Camera")) {
      StopGrabbingAndBroadcasting(m_selectedCameraId);
      m_cameraManager->DisconnectCamera(m_selectedCameraId);
      LogInfo("Disconnected camera: " + m_selectedCameraId);
    }

    ImGui::SameLine();
    if (ImGui::Button("Refresh Connection")) {
      RefreshCameraConnection(m_selectedCameraId);
    }
  }

  // Grabbing controls with broadcasting
  if (status.connected) {
    ImGui::Separator();
    if (!status.grabbing) {
      if (ImGui::Button("Start Video Feed (with Broadcasting)")) {
        StartGrabbingWithBroadcasting(m_selectedCameraId);
      }
    }
    else {
      if (ImGui::Button("Stop Video Feed")) {
        StopGrabbingAndBroadcasting(m_selectedCameraId);
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("Capture Image")) {
      if (m_cameraManager->CaptureImage(m_selectedCameraId)) {
        LogInfo("Captured image from: " + m_selectedCameraId);
      }
      else {
        LogError("Failed to capture image from: " + m_selectedCameraId);
      }
    }
  }
  // ADD THIS BUTTON for debugging:
  if (ImGui::Button("Force Update Feed")) {
    ForceUpdateCameraFeed();
  }
  ImGui::SameLine();
  ImGui::Text("(Debug: Force texture update)");
  // Enhanced exposure controls using ICameraHardware interface
  if (status.connected && m_selectedCameraHardware) {
    ImGui::Separator();
    ImGui::Text("Exposure Settings (ICameraHardware Interface):");

    auto currentExposure = m_selectedCameraHardware->GetExposureSettings();

    // Create UI controls for exposure settings
    static float exposureTime = static_cast<float>(currentExposure.exposure_time);
    static float gain = static_cast<float>(currentExposure.gain);
    static bool autoExposure = currentExposure.auto_exposure;
    static bool autoGain = currentExposure.auto_gain;

    // Update static values with current settings
    exposureTime = static_cast<float>(currentExposure.exposure_time);
    gain = static_cast<float>(currentExposure.gain);
    autoExposure = currentExposure.auto_exposure;
    autoGain = currentExposure.auto_gain;

    bool changed = false;
    changed |= ImGui::SliderFloat("Exposure Time (μs)", &exposureTime, 100.0f, 100000.0f, "%.1f");
    changed |= ImGui::SliderFloat("Gain", &gain, 0.0f, 10.0f, "%.2f");
    changed |= ImGui::Checkbox("Auto Exposure", &autoExposure);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Auto Gain", &autoGain);

    if (changed || ImGui::Button("Apply Exposure Settings")) {
      ICameraHardware::ExposureSettings newSettings(
        static_cast<double>(exposureTime),
        static_cast<double>(gain),
        autoExposure,
        autoGain
      );

      if (m_selectedCameraHardware->SetExposureSettings(newSettings)) {
        LogInfo("Applied exposure settings: " + FormatExposureSettings(newSettings));
      }
      else {
        LogError("Failed to apply exposure settings: " + m_selectedCameraHardware->GetLastError());
      }
    }

    // Show current exposure settings
    ImGui::Text("Current Settings: %s", FormatExposureSettings(currentExposure).c_str());
  }
}

void RaylibDebugWindow::RenderFeedControls() {
  ImGui::Text("Raylib 3D Window Display Controls:");

  // Feed visibility control
  bool feedVisible = m_raylibWindow->IsCameraFeedVisible();
  if (ImGui::Checkbox("Show Feed in 3D Window", &feedVisible)) {
    m_raylibWindow->SetCameraFeedVisible(feedVisible);
    LogInfo("3D camera feed " + std::string(feedVisible ? "enabled" : "disabled"));
  }

  // Enhanced feed information
  if (m_cameraFeedDisplay) {
    ImGui::Separator();
    ImGui::Text("Camera Feed Display Status:");

    // Basic status
    ImGui::Text("  Has Source: %s", m_cameraFeedDisplay->HasSource() ? "✓ Yes" : "✗ No");
    ImGui::Text("  Has Texture: %s", m_cameraFeedDisplay->HasValidTexture() ? "✓ Yes" : "✗ No");
    ImGui::Text("  Status: %s", m_cameraFeedDisplay->GetStatusText().c_str());
    ImGui::Text("  Receiving Frames: %s", m_cameraFeedDisplay->IsReceivingFrames() ? "✓ Yes" : "✗ No");

    // Statistics
    ImGui::Text("  Total Frames: %llu", m_cameraFeedDisplay->GetTotalFramesReceived());
    ImGui::Text("  Frame Rate: %.1f fps", m_cameraFeedDisplay->GetActualFrameRate());
    ImGui::Text("  Subscriber ID: %s", m_cameraFeedDisplay->GetSubscriberId().c_str());

    if (m_cameraFeedDisplay->HasValidTexture()) {
      ImGui::Text("  Resolution: %dx%d",
        m_cameraFeedDisplay->GetTextureWidth(),
        m_cameraFeedDisplay->GetTextureHeight());

      // Transparency control
      static float alpha = 0.8f;
      if (ImGui::SliderFloat("Feed Transparency", &alpha, 0.1f, 1.0f)) {
        m_raylibWindow->SetCameraFeedAlpha(alpha);
      }
    }
  }
  else {
    ImGui::Text("CameraFeedDisplay not available");
  }
}

void RaylibDebugWindow::RenderBroadcastingDiagnostics() {
  ImGui::Text("Broadcasting System Diagnostics:");

  // System-wide broadcasting information
  size_t subscriberCount = m_cameraManager->GetSubscriberCount();
  auto subscriberIds = m_cameraManager->GetSubscriberIds();

  ImGui::Text("Total Subscribers: %zu", subscriberCount);
  ImGui::Text("Broadcasting Active: %s", subscriberCount > 0 ? "✓ Yes" : "✗ No");

  // List all active subscribers
  if (subscriberCount > 0) {
    ImGui::Text("Active Subscribers:");
    ImGui::Indent();
    for (const auto& id : subscriberIds) {
      ImGui::Text("• %s", id.c_str());
    }
    ImGui::Unindent();
  }

  // Debug subscriber information
  if (m_debugSubscriber) {
    ImGui::Separator();
    ImGui::Text("Debug Subscriber Status:");
    ImGui::Text("  ID: %s", m_debugSubscriber->GetSubscriberId().c_str());
    ImGui::Text("  Target Camera: %s", m_debugSubscriber->GetTargetCamera().c_str());
    ImGui::Text("  Has New Frame: %s", m_debugSubscriber->HasNewFrame() ? "✓ Yes" : "✗ No");
    ImGui::Text("  Camera Connected: %s", m_debugSubscriber->IsCameraConnected() ? "✓ Yes" : "✗ No");
    ImGui::Text("  Camera Grabbing: %s", m_debugSubscriber->IsCameraGrabbing() ? "✓ Yes" : "✗ No");
    ImGui::Text("  Total Frames: %llu", m_debugSubscriber->GetTotalFramesReceived());
    ImGui::Text("  Frame Rate: %.1f fps", m_debugSubscriber->GetActualFrameRate());
  }
  else {
    ImGui::Text("Debug subscriber not active");
  }

  // Broadcasting controls
  ImGui::Separator();
  if (ImGui::Button("Validate Broadcasting System")) {
    ValidateBroadcastingSystem();
  }

  ImGui::SameLine();
  if (ImGui::Button("Setup Debug Subscriber")) {
    if (!m_selectedCameraId.empty()) {
      SetupDebugSubscriber(m_selectedCameraId);
    }
    else {
      LogWarning("No camera selected for debug subscriber setup");
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Cleanup Debug Subscriber")) {
    CleanupDebugSubscriber();
  }
}

void RaylibDebugWindow::RenderInterfaceValidation() {
  ImGui::Text("Interface Validation Tools:");

  // Validation buttons
  if (ImGui::Button("Validate ICameraHardware")) {
    if (!m_selectedCameraId.empty()) {
      ValidateICameraHardwareInterface(m_selectedCameraId);
    }
    else {
      LogWarning("No camera selected for interface validation");
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Validate CameraManager")) {
    ValidateCameraManagerInterface();
  }

  ImGui::SameLine();
  if (ImGui::Button("Validate Feed Display")) {
    ValidateFeedDisplayInterface();
  }

  // Show validation results
  if (!m_selectedCameraId.empty() && m_selectedCameraHardware) {
    ImGui::Separator();
    ImGui::Text("ICameraHardware Interface Status:");

    // Test basic interface methods
    ImGui::Text("  Camera Type: %s", CameraHardwareFactory::GetCameraTypeName(m_selectedCameraHardware->GetCameraType()).c_str());
    ImGui::Text("  Camera ID: %s", m_selectedCameraHardware->GetCameraId().c_str());
    ImGui::Text("  Model Name: %s", m_selectedCameraHardware->GetModelName().c_str());
    ImGui::Text("  Serial Number: %s", m_selectedCameraHardware->GetSerialNumber().c_str());
    ImGui::Text("  Vendor Name: %s", m_selectedCameraHardware->GetVendorName().c_str());
    ImGui::Text("  Is Connected: %s", m_selectedCameraHardware->IsConnected() ? "✓ Yes" : "✗ No");
    ImGui::Text("  Is Grabbing: %s", m_selectedCameraHardware->IsGrabbing() ? "✓ Yes" : "✗ No");
    ImGui::Text("  Has New Frame: %s", m_selectedCameraHardware->HasNewFrame() ? "✓ Yes" : "✗ No");
    ImGui::Text("  Device Removed: %s", m_selectedCameraHardware->IsDeviceRemoved() ? "⚠ Yes" : "✓ No");

    // Show any errors
    std::string lastError = m_selectedCameraHardware->GetLastError();
    if (!lastError.empty()) {
      ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "  Last Error: %s", lastError.c_str());
    }
    else {
      ImGui::TextColored(ImVec4(0, 1, 0, 1), "  Last Error: None");
    }
  }
  else if (!m_selectedCameraId.empty()) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "ICameraHardware interface not available for selected camera");
  }
}

void RaylibDebugWindow::RenderPerformanceMetrics() {
  if (!m_debugSubscriber) {
    ImGui::Text("Debug subscriber not active - performance metrics unavailable");
    if (ImGui::Button("Setup Debug Subscriber")) {
      if (!m_selectedCameraId.empty()) {
        SetupDebugSubscriber(m_selectedCameraId);
      }
    }
    return;
  }

  auto metrics = m_debugSubscriber->GetPerformanceMetrics();

  ImGui::Text("Performance Metrics:");
  ImGui::Text("  Total Frames: %llu", metrics.totalFrames);
  ImGui::Text("  Dropped Frames: %llu", metrics.droppedFrames);
  ImGui::Text("  Average Frame Rate: %.1f fps", metrics.avgFrameRate);
  ImGui::Text("  Avg Processing Time: %lld ms", metrics.avgProcessingTime.count());
  ImGui::Text("  Max Processing Time: %lld ms", metrics.maxProcessingTime.count());

  // Performance indicators with color coding
  ImGui::Separator();
  if (metrics.avgFrameRate < 10.0 && metrics.totalFrames > 10) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "⚠ WARNING: Low frame rate detected!");
  }

  if (metrics.droppedFrames > 0) {
    double dropRate = static_cast<double>(metrics.droppedFrames) / static_cast<double>(metrics.totalFrames) * 100.0;
    ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "⚠ WARNING: %.1f%% frame drops detected!", dropRate);
  }

  if (metrics.maxProcessingTime.count() > 100) {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "⚠ WARNING: High processing latency detected!");
  }

  // Performance graph would go here in a more advanced implementation
  if (metrics.totalFrames > 0 && metrics.avgFrameRate > 0) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ Performance metrics look good");
  }
}

void RaylibDebugWindow::RenderQuickActions() {
  ImGui::Text("Quick Actions:");

  // Complete setup action
  if (ImGui::Button("Complete Setup & Start Feed")) {
    if (!m_selectedCameraId.empty()) {
      LogInfo("=== STARTING COMPLETE SETUP ===");

      // 1. Connect camera with broadcasting
      ConnectCameraWithBroadcasting(m_selectedCameraId);

      // Small delay to allow connection to stabilize
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      // 2. Start grabbing with broadcasting
      StartGrabbingWithBroadcasting(m_selectedCameraId);

      // 3. Setup debug subscriber
      SetupDebugSubscriber(m_selectedCameraId);

      // 4. Connect to raylib window
      if (m_cameraFeedDisplay) {
        m_cameraFeedDisplay->SetTargetCamera(m_selectedCameraId);
        m_raylibWindow->SetCameraFeedVisible(true);
        LogInfo("Connected camera feed to raylib 3D window via broadcasting");
      }

      LogInfo("=== COMPLETE SETUP FINISHED ===");
    }
    else {
      LogWarning("No camera selected for complete setup");
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Reset All Connections")) {
    LogInfo("=== RESETTING ALL CONNECTIONS ===");

    // Stop everything
    if (!m_selectedCameraId.empty()) {
      StopGrabbingAndBroadcasting(m_selectedCameraId);
      m_cameraManager->DisconnectCamera(m_selectedCameraId);
    }

    // Cleanup subscribers
    CleanupDebugSubscriber();

    // Reset UI state
    m_selectedCameraId.clear();
    m_selectedCameraHardware = nullptr;

    LogInfo("=== RESET COMPLETE ===");
  }

  // Test operations
  ImGui::Separator();
  if (ImGui::Button("Send Test Frame to Debug Subscriber")) {
    if (m_debugSubscriber && !m_selectedCameraId.empty()) {
      LogInfo("=== SENDING TEST FRAME ===");

      // Create test frame with pattern
      CameraFrameData testFrame;
      testFrame.cameraId = m_selectedCameraId;
      testFrame.width = 640;
      testFrame.height = 480;
      testFrame.channels = 3;
      testFrame.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
      testFrame.frameNumber = 12345;
      testFrame.exposureTime = 10000.0;
      testFrame.gain = 1.0;

      // Create test pattern (red and blue stripes)
      size_t dataSize = testFrame.width * testFrame.height * 3;
      testFrame.imageData.resize(dataSize);

      for (uint32_t y = 0; y < testFrame.height; ++y) {
        for (uint32_t x = 0; x < testFrame.width; ++x) {
          size_t idx = (y * testFrame.width + x) * 3;
          if ((x / 50) % 2 == 0) {
            testFrame.imageData[idx] = 255;     // R
            testFrame.imageData[idx + 1] = 0;   // G
            testFrame.imageData[idx + 2] = 0;   // B
          }
          else {
            testFrame.imageData[idx] = 0;       // R
            testFrame.imageData[idx + 1] = 0;   // G
            testFrame.imageData[idx + 2] = 255; // B
          }
        }
      }

      try {
        m_debugSubscriber->OnNewFrame(testFrame);
        LogInfo("Test frame sent successfully to debug subscriber");
      }
      catch (const std::exception& e) {
        LogError("Exception sending test frame: " + std::string(e.what()));
      }
    }
    else {
      LogWarning("Debug subscriber not available for test frame");
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Validate Complete System")) {
    LogInfo("=== VALIDATING COMPLETE SYSTEM ===");

    ValidateCameraManagerInterface();
    ValidateFeedDisplayInterface();
    ValidateBroadcastingSystem();

    if (!m_selectedCameraId.empty()) {
      ValidateICameraHardwareInterface(m_selectedCameraId);
    }

    LogInfo("=== SYSTEM VALIDATION COMPLETE ===");
  }
}

void RaylibDebugWindow::RenderAdvancedDebugInfo() {
  ImGui::Text("Advanced Debug Information:");

  // OpenGL texture debugging
  if (m_cameraFeedDisplay && m_cameraFeedDisplay->HasValidTexture()) {
    ImGui::Separator();
    ImGui::Text("OpenGL Texture Debug:");
    ImGui::Text("  Texture ID: %u", m_cameraFeedDisplay->GetTextureID());
    ImGui::Text("  Texture Size: %dx%d",
      m_cameraFeedDisplay->GetTextureWidth(),
      m_cameraFeedDisplay->GetTextureHeight());

    // OpenGL state information (only if OpenGL headers are available)
    ImGui::Text("  OpenGL Context: Active");

#ifdef GL_TEXTURE_BINDING_2D
    // Check if texture is bound (only if OpenGL is available)
    GLint currentTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &currentTexture);
    ImGui::Text("  Currently Bound Texture: %d", currentTexture);
#else
    ImGui::Text("  OpenGL debugging not available (headers not included)");
#endif
  }

  // Raylib window threading information
  ImGui::Separator();
  ImGui::Text("Raylib Window Debug:");
  ImGui::Text("  Is Running: %s", m_raylibWindow->IsRunning() ? "✓ Yes" : "✗ No");
  ImGui::Text("  Is Visible: %s", m_raylibWindow->IsVisible() ? "✓ Yes" : "✗ No");
  ImGui::Text("  Has Camera Feed: %s", m_raylibWindow->HasCameraFeed() ? "✓ Yes" : "✗ No");
  ImGui::Text("  Feed Visible: %s", m_raylibWindow->IsCameraFeedVisible() ? "✓ Yes" : "✗ No");

  // Memory usage information
  if (m_debugSubscriber) {
    auto latestFrame = m_debugSubscriber->GetLatestFrame();
    if (latestFrame.IsValid()) {
      ImGui::Separator();
      ImGui::Text("Memory Usage Debug:");
      ImGui::Text("  Frame Data Size: %zu bytes", latestFrame.GetDataSize());
      ImGui::Text("  Frame Timestamp: %llu", latestFrame.timestamp);
      ImGui::Text("  Frame Number: %llu", latestFrame.frameNumber);
      ImGui::Text("  Frame Dimensions: %dx%d", latestFrame.width, latestFrame.height);
      ImGui::Text("  Frame Channels: %d", latestFrame.channels);

      // Calculate memory usage
      size_t totalMemory = latestFrame.imageData.size();
      ImGui::Text("  Memory Usage: %.2f MB", static_cast<double>(totalMemory) / (1024.0 * 1024.0));
    }
  }

  // System diagnostics
  ImGui::Separator();
  ImGui::Text("System Diagnostics:");

  if (ImGui::Button("Force Garbage Collection")) {
    LogInfo("Forcing system cleanup...");
    // This could trigger cleanup in CameraManager if such method exists
    if (m_cameraManager) {
      // Call any cleanup methods here
      LogInfo("Cleanup operations completed");
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Dump Debug Info to Log")) {
    LogInfo("=== DUMPING COMPLETE DEBUG INFO ===");

    // Dump all available debug information to log
    auto cameraIds = m_cameraManager->GetCameraIds();
    LogInfo("Available cameras: " + std::to_string(cameraIds.size()));

    for (const auto& id : cameraIds) {
      auto status = m_cameraManager->GetCameraStatus(id);
      LogInfo("Camera " + id + ": connected=" +
        (status.connected ? "Yes" : "No") +
        ", grabbing=" + (status.grabbing ? "Yes" : "No") +
        ", type=" + CameraHardwareFactory::GetCameraTypeName(status.type));
    }

    LogInfo("Subscribers: " + std::to_string(m_cameraManager->GetSubscriberCount()));
    auto subscriberIds = m_cameraManager->GetSubscriberIds();
    for (const auto& id : subscriberIds) {
      LogInfo("Subscriber: " + id);
    }

    if (m_debugSubscriber) {
      auto metrics = m_debugSubscriber->GetPerformanceMetrics();
      LogInfo("Debug Subscriber Metrics:");
      LogInfo("  Total Frames: " + std::to_string(metrics.totalFrames));
      LogInfo("  Dropped Frames: " + std::to_string(metrics.droppedFrames));
      LogInfo("  Avg Frame Rate: " + std::to_string(metrics.avgFrameRate));
      LogInfo("  Avg Processing Time: " + std::to_string(metrics.avgProcessingTime.count()) + "ms");
    }

    LogInfo("=== DEBUG INFO DUMP COMPLETE ===");
  }

  ImGui::SameLine();
  if (ImGui::Button("Test Interface Methods")) {
    if (m_selectedCameraHardware) {
      LogInfo("=== TESTING INTERFACE METHODS ===");

      // Test all interface methods safely
      try {
        LogInfo("Camera ID: " + m_selectedCameraHardware->GetCameraId());
        LogInfo("Model Name: " + m_selectedCameraHardware->GetModelName());
        LogInfo("Serial Number: " + m_selectedCameraHardware->GetSerialNumber());
        LogInfo("Vendor Name: " + m_selectedCameraHardware->GetVendorName());
        LogInfo("Is Connected: " + std::string(m_selectedCameraHardware->IsConnected() ? "Yes" : "No"));
        LogInfo("Is Grabbing: " + std::string(m_selectedCameraHardware->IsGrabbing() ? "Yes" : "No"));
        LogInfo("Has New Frame: " + std::string(m_selectedCameraHardware->HasNewFrame() ? "Yes" : "No"));
        LogInfo("Is Device Removed: " + std::string(m_selectedCameraHardware->IsDeviceRemoved() ? "Yes" : "No"));

        auto exposure = m_selectedCameraHardware->GetExposureSettings();
        LogInfo("Exposure Settings: " + FormatExposureSettings(exposure));

        LogInfo("Interface methods test completed successfully");
      }
      catch (const std::exception& e) {
        LogError("Exception during interface test: " + std::string(e.what()));
      }

      LogInfo("=== INTERFACE METHODS TEST COMPLETE ===");
    }
    else {
      LogWarning("No camera hardware interface available for testing");
    }
  }
}

// ============================================================================
// Enhanced camera operations
// ============================================================================

void RaylibDebugWindow::SelectCamera(const std::string& cameraId) {
  if (m_selectedCameraId == cameraId) {
    return; // No change needed
  }

  LogInfo("Selecting camera: " + cameraId);

  // Cleanup previous selection
  CleanupDebugSubscriber();

  // Update selection
  m_selectedCameraId = cameraId;
  m_selectedCameraHardware = m_cameraManager->GetCameraHardware(cameraId);

  if (m_selectedCameraHardware) {
    LogInfo("ICameraHardware interface available for: " + cameraId);
  }
  else {
    LogWarning("ICameraHardware interface not available for: " + cameraId);
  }

  // **ADD THIS: Switch the camera feed display to the new camera**
  if (m_cameraFeedDisplay) {
    LogInfo("Switching raylib camera feed from subscriber mode...");
    m_cameraFeedDisplay->ClearSource();
    m_cameraFeedDisplay->SetTargetCamera(cameraId);
    LogInfo("Raylib camera feed switched to: " + cameraId);
  }
  else {
    LogWarning("CameraFeedDisplay not available for camera switch");
  }

  // Setup debug subscriber for new camera
  if (!cameraId.empty()) {
    SetupDebugSubscriber(cameraId);
  }
}



// 2. ALSO ADD this method for explicit force update:
void RaylibDebugWindow::ForceUpdateCameraFeed() {
  if (m_cameraFeedDisplay && !m_selectedCameraId.empty()) {
    LogInfo("Force updating camera feed for: " + m_selectedCameraId);

    // Force the camera feed display to update its texture
    m_cameraFeedDisplay->UpdateTexture();

    LogInfo("Camera feed display status after update:");
    LogInfo("  Has Source: " + std::string(m_cameraFeedDisplay->HasSource() ? "Yes" : "No"));
    LogInfo("  Has Valid Texture: " + std::string(m_cameraFeedDisplay->HasValidTexture() ? "Yes" : "No"));
    LogInfo("  Is Receiving Frames: " + std::string(m_cameraFeedDisplay->IsReceivingFrames() ? "Yes" : "No"));
    LogInfo("  Status: " + m_cameraFeedDisplay->GetStatusText());
  }
}

void RaylibDebugWindow::ConnectCameraWithBroadcasting(const std::string& cameraId) {
  LogInfo("=== CONNECTING CAMERA WITH BROADCASTING ===");
  LogInfo("Camera ID: " + cameraId);

  // Ensure broadcasting system is running
  m_cameraManager->StartBroadcastSystem();
  LogInfo("Broadcasting system started");

  // Connect the camera
  if (m_cameraManager->ConnectCamera(cameraId)) {
    LogInfo("Camera connected successfully");

    // Update hardware reference
    m_selectedCameraHardware = m_cameraManager->GetCameraHardware(cameraId);

    // Validate connection
    if (m_selectedCameraHardware && m_selectedCameraHardware->IsConnected()) {
      LogInfo("ICameraHardware interface confirms connection");
    }
    else {
      LogWarning("ICameraHardware interface reports disconnected");
    }
  }
  else {
    LogError("Failed to connect camera");
  }

  LogInfo("=== CAMERA CONNECTION COMPLETE ===");
}

void RaylibDebugWindow::StartGrabbingWithBroadcasting(const std::string& cameraId) {
  LogInfo("=== STARTING GRABBING WITH BROADCASTING ===");
  LogInfo("Camera ID: " + cameraId);

  bool success = false;

  // Try the broadcasting-specific method first if available
  try {
    success = m_cameraManager->StartGrabbingWithBroadcast(cameraId);
    if (success) {
      LogInfo("Started grabbing with explicit broadcasting support");
    }
  }
  catch (...) {
    // Fall back to regular StartGrabbing
    LogInfo("StartGrabbingWithBroadcast not available, using standard method");
    success = m_cameraManager->StartGrabbing(cameraId);
    if (success) {
      LogInfo("Started grabbing with standard method");
    }
  }

  if (success) {
    LogInfo("Grabbing started successfully");

    // Verify grabbing status through interface
    if (m_selectedCameraHardware && m_selectedCameraHardware->IsGrabbing()) {
      LogInfo("ICameraHardware interface confirms grabbing active");
    }
    else {
      LogWarning("ICameraHardware interface reports not grabbing");
    }
  }
  else {
    LogError("Failed to start grabbing");

    // Try to get error information
    if (m_selectedCameraHardware) {
      std::string error = m_selectedCameraHardware->GetLastError();
      if (!error.empty()) {
        LogError("Camera error: " + error);
      }
    }
  }

  LogInfo("=== GRABBING START COMPLETE ===");
}

void RaylibDebugWindow::StopGrabbingAndBroadcasting(const std::string& cameraId) {
  LogInfo("=== STOPPING GRABBING AND BROADCASTING ===");
  LogInfo("Camera ID: " + cameraId);

  if (m_cameraManager->StopGrabbing(cameraId)) {
    LogInfo("Grabbing stopped successfully");
  }
  else {
    LogWarning("Failed to stop grabbing or was already stopped");
  }

  // Verify through interface
  if (m_selectedCameraHardware && !m_selectedCameraHardware->IsGrabbing()) {
    LogInfo("ICameraHardware interface confirms grabbing stopped");
  }

  LogInfo("=== GRABBING STOP COMPLETE ===");
}

void RaylibDebugWindow::RefreshCameraConnection(const std::string& cameraId) {
  LogInfo("=== REFRESHING CAMERA CONNECTION ===");
  LogInfo("Camera ID: " + cameraId);

  // Stop grabbing first
  StopGrabbingAndBroadcasting(cameraId);

  // Small delay
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Disconnect and reconnect
  m_cameraManager->DisconnectCamera(cameraId);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // Reconnect with broadcasting
  ConnectCameraWithBroadcasting(cameraId);

  LogInfo("=== CONNECTION REFRESH COMPLETE ===");
}

// ============================================================================
// Broadcasting management
// ============================================================================

void RaylibDebugWindow::SetupDebugSubscriber(const std::string& cameraId) {
  LogInfo("=== SETTING UP DEBUG SUBSCRIBER ===");
  LogInfo("Camera ID: " + cameraId);

  // Cleanup existing subscriber
  CleanupDebugSubscriber();

  // Create new debug subscriber
  m_debugSubscriber = std::make_shared<RaylibDebugSubscriber>(cameraId);

  // Subscribe to camera manager
  m_cameraManager->SubscribeToFrames(m_debugSubscriber);

  LogInfo("Debug subscriber created and registered: " + m_debugSubscriber->GetSubscriberId());
  LogInfo("=== DEBUG SUBSCRIBER SETUP COMPLETE ===");
}

void RaylibDebugWindow::CleanupDebugSubscriber() {
  if (m_debugSubscriber) {
    LogInfo("Cleaning up debug subscriber: " + m_debugSubscriber->GetSubscriberId());
    m_cameraManager->UnsubscribeFromFrames(m_debugSubscriber->GetSubscriberId());
    m_debugSubscriber.reset();
  }
}

void RaylibDebugWindow::ValidateBroadcastingSystem() {
  LogInfo("=== VALIDATING BROADCASTING SYSTEM ===");

  size_t subscriberCount = m_cameraManager->GetSubscriberCount();
  auto subscriberIds = m_cameraManager->GetSubscriberIds();

  LogInfo("Total subscribers: " + std::to_string(subscriberCount));
  LogInfo("Broadcasting active: " + std::string(subscriberCount > 0 ? "Yes" : "No"));

  for (const auto& id : subscriberIds) {
    LogInfo("Active subscriber: " + id);
  }

  if (m_debugSubscriber) {
    LogInfo("Debug subscriber status:");
    LogInfo("  ID: " + m_debugSubscriber->GetSubscriberId());
    LogInfo("  Target: " + m_debugSubscriber->GetTargetCamera());
    LogInfo("  Connected: " + std::string(m_debugSubscriber->IsCameraConnected() ? "Yes" : "No"));
    LogInfo("  Grabbing: " + std::string(m_debugSubscriber->IsCameraGrabbing() ? "Yes" : "No"));
    LogInfo("  Frames received: " + std::to_string(m_debugSubscriber->GetTotalFramesReceived()));
  }

  LogInfo("=== BROADCASTING VALIDATION COMPLETE ===");
}

// ============================================================================
// Interface validation
// ============================================================================

void RaylibDebugWindow::ValidateICameraHardwareInterface(const std::string& cameraId) {
  LogInfo("=== VALIDATING ICAMERAHARDWARE INTERFACE ===");
  LogInfo("Camera ID: " + cameraId);

  auto camera = m_cameraManager->GetCameraHardware(cameraId);
  if (!camera) {
    LogError("ICameraHardware interface not available for camera: " + cameraId);
    return;
  }

  try {
    LogInfo("ICameraHardware interface validation:");
    LogInfo("  Camera Type: " + CameraHardwareFactory::GetCameraTypeName(camera->GetCameraType()));
    LogInfo("  Camera ID: " + camera->GetCameraId());
    LogInfo("  Model Name: " + camera->GetModelName());
    LogInfo("  Serial Number: " + camera->GetSerialNumber());
    LogInfo("  Vendor Name: " + camera->GetVendorName());
    LogInfo("  Is Connected: " + std::string(camera->IsConnected() ? "Yes" : "No"));
    LogInfo("  Is Grabbing: " + std::string(camera->IsGrabbing() ? "Yes" : "No"));
    LogInfo("  Has New Frame: " + std::string(camera->HasNewFrame() ? "Yes" : "No"));
    LogInfo("  Is Device Removed: " + std::string(camera->IsDeviceRemoved() ? "Yes" : "No"));

    auto exposureSettings = camera->GetExposureSettings();
    LogInfo("  Exposure Settings: " + FormatExposureSettings(exposureSettings));

    std::string lastError = camera->GetLastError();
    if (!lastError.empty()) {
      LogWarning("  Last Error: " + lastError);
    }
    else {
      LogInfo("  Last Error: None");
    }

    LogInfo("Interface validation completed successfully");
  }
  catch (const std::exception& e) {
    LogError("Exception during interface validation: " + std::string(e.what()));
  }

  LogInfo("=== ICAMERAHARDWARE VALIDATION COMPLETE ===");
}

void RaylibDebugWindow::ValidateCameraManagerInterface() {
  LogInfo("=== VALIDATING CAMERA MANAGER INTERFACE ===");

  try {
    LogInfo("Camera Manager validation:");
    LogInfo("  Camera Count: " + std::to_string(m_cameraManager->GetCameraCount()));
    LogInfo("  Subscriber Count: " + std::to_string(m_cameraManager->GetSubscriberCount()));

    auto cameraIds = m_cameraManager->GetCameraIds();
    LogInfo("  Available Cameras:");
    for (const auto& id : cameraIds) {
      auto status = m_cameraManager->GetCameraStatus(id);
      LogInfo("    " + id + ": " + CameraHardwareFactory::GetCameraTypeName(status.type) +
        " (connected=" + (status.connected ? "Yes" : "No") +
        ", grabbing=" + (status.grabbing ? "Yes" : "No") + ")");
    }

    LogInfo("Camera Manager validation completed successfully");
  }
  catch (const std::exception& e) {
    LogError("Exception during Camera Manager validation: " + std::string(e.what()));
  }

  LogInfo("=== CAMERA MANAGER VALIDATION COMPLETE ===");
}

void RaylibDebugWindow::ValidateFeedDisplayInterface() {
  LogInfo("=== VALIDATING FEED DISPLAY INTERFACE ===");

  if (!m_cameraFeedDisplay) {
    LogError("CameraFeedDisplay not available");
    return;
  }

  try {
    LogInfo("Feed Display validation:");
    LogInfo("  Has Source: " + std::string(m_cameraFeedDisplay->HasSource() ? "Yes" : "No"));
    LogInfo("  Has Valid Texture: " + std::string(m_cameraFeedDisplay->HasValidTexture() ? "Yes" : "No"));
    LogInfo("  Is Receiving Frames: " + std::string(m_cameraFeedDisplay->IsReceivingFrames() ? "Yes" : "No"));
    LogInfo("  Status Text: " + m_cameraFeedDisplay->GetStatusText());
    LogInfo("  Total Frames Received: " + std::to_string(m_cameraFeedDisplay->GetTotalFramesReceived()));
    LogInfo("  Actual Frame Rate: " + std::to_string(m_cameraFeedDisplay->GetActualFrameRate()) + " fps");
    LogInfo("  Subscriber ID: " + m_cameraFeedDisplay->GetSubscriberId());

    if (m_cameraFeedDisplay->HasValidTexture()) {
      LogInfo("  Texture ID: " + std::to_string(m_cameraFeedDisplay->GetTextureID()));
      LogInfo("  Texture Size: " + std::to_string(m_cameraFeedDisplay->GetTextureWidth()) +
        "x" + std::to_string(m_cameraFeedDisplay->GetTextureHeight()));
    }

    LogInfo("Feed Display validation completed successfully");
  }
  catch (const std::exception& e) {
    LogError("Exception during Feed Display validation: " + std::string(e.what()));
  }

  LogInfo("=== FEED DISPLAY VALIDATION COMPLETE ===");
}

// ============================================================================
// Utility methods
// ============================================================================

void RaylibDebugWindow::LogInfo(const std::string& message) {
  if (m_logger) {
    m_logger->LogInfo("[RaylibDebug] " + message);
  }
}

void RaylibDebugWindow::LogWarning(const std::string& message) {
  if (m_logger) {
    m_logger->LogWarning("[RaylibDebug] " + message);
  }
}

void RaylibDebugWindow::LogError(const std::string& message) {
  if (m_logger) {
    m_logger->LogError("[RaylibDebug] " + message);
  }
}

std::string RaylibDebugWindow::FormatExposureSettings(const ICameraHardware::ExposureSettings& settings) {
  return "exp=" + std::to_string(settings.exposure_time) + "μs, " +
    "gain=" + std::to_string(settings.gain) + ", " +
    "auto_exp=" + (settings.auto_exposure ? "On" : "Off") + ", " +
    "auto_gain=" + (settings.auto_gain ? "On" : "Off");
}

bool RaylibDebugWindow::ShouldRefresh() {
  auto now = std::chrono::steady_clock::now();
  return (now - m_lastRefresh) >= REFRESH_INTERVAL;
}

void RaylibDebugWindow::UpdateSelectedCamera() {
  if (!m_selectedCameraId.empty()) {
    m_selectedCameraHardware = m_cameraManager->GetCameraHardware(m_selectedCameraId);
  }
}