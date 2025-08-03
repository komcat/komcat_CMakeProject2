#include "RunPageUI.h"
#include "imgui.h"
#include <chrono>
#include <iostream>
#include <algorithm>
#include "uaa3_process_builders.h"
#include "ProcessRegistry.h"
#include "LiveVideoSubscriber.h"
// UPDATE RunPageUI.cpp - Constructor

// NEW: Add to constructor

// NEW: Add to constructor
RunPageUI::RunPageUI(MachineOperations& machineOps)
  : m_machineOps(machineOps),
  m_uiManager(std::make_unique<MockUserInteractionManager>()),
  m_stopRequested(false),
  m_pauseRequested(false),
  m_cameraManager(nullptr),  // NEW: Initialize camera manager
  m_cameraSystemInitialized(false)  // NEW: Initialize camera system flag
{
  m_logger = Logger::GetInstance();

  // Initialize filter manager
  m_filterManager = std::make_unique<ProcessFilterManager>();

  // Set up callback for when filters change
  m_filterManager->SetOnFilterChangedCallback([this]() {
    OnFilterChanged();
  });

  // Initialize operations display UI for detail results tab
  m_operationsDisplayUI = std::make_unique<OperationsDisplayUI>(machineOps);

  m_logger->LogInfo("RunPageUI: Initialized with process filtering and operations display support");
}

// NEW: Add to destructor
RunPageUI::~RunPageUI() {
  StopProcess();
  CleanupCameraTexture();  // NEW: Clean up OpenGL texture
  ClearCameraFeed();  // NEW: Clear camera feed
  m_logger->LogInfo("RunPageUI: Destroyed");
}


void RunPageUI::RenderUI() {


  // Get available content region
  ImVec2 contentRegion = ImGui::GetContentRegionAvail();

  // Calculate column widths (25%, 25%, 50%)
  float col1Width = contentRegion.x * 0.25f;
  float col2Width = contentRegion.x * 0.25f;
  float col3Width = contentRegion.x * 0.50f;

  // Begin 3-column layout
  ImGui::BeginChild("Column1", ImVec2(col1Width, 0), true);
  RenderColumn1();
  ImGui::EndChild();

  ImGui::SameLine();
  ImGui::BeginChild("Column2", ImVec2(col2Width, 0), true);
  RenderColumn2();
  ImGui::EndChild();

  ImGui::SameLine();
  ImGui::BeginChild("Column3", ImVec2(col3Width, 0), true);
  RenderColumn3();
  ImGui::EndChild();

  // NEW: Render filter configuration window
  if (m_showFilterWindow) {
    m_filterManager->RenderFilterWindow(&m_showFilterWindow);
  }
}

void RunPageUI::RenderColumn1() {



  ImGui::Text(reinterpret_cast<const char*>(u8"🔧 Process Control"));
  ImGui::Separator();

  // Render control buttons at the top
  RenderControlButtons();

  ImGui::Spacing();
  ImGui::Separator();

  // NEW: Single-line running status with large font and dark green background
  RenderRunningStatus();

  ImGui::Spacing();
  ImGui::Separator();

  // Render process step buttons (now takes more space)
  RenderProcessButtons();


}

// NEW: Render single-line running status with progress bar
void RunPageUI::RenderRunningStatus() {
  // Calculate the available width for the status bar
  float availableWidth = ImGui::GetContentRegionAvail().x;
  float statusHeight = 60.0f; // Back to original height since progress bar is outside

  // Get current cursor position
  ImVec2 cursorPos = ImGui::GetCursorScreenPos();

  // Draw background rectangle (dark green)
  ImU32 bgColor = IM_COL32(0, 100, 0, 255); // Dark green background
  ImGui::GetWindowDrawList()->AddRectFilled(
    cursorPos,
    ImVec2(cursorPos.x + availableWidth, cursorPos.y + statusHeight),
    bgColor,
    4.0f // Rounded corners
  );

  // Set cursor position for text (centered)
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f); // Vertical padding

  // Large font for status text
  ImGui::SetWindowFontScale(2.0f);

  // Status text with black color
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text

  std::string statusText;
  if (m_processRunning) {
    if (m_processPaused) {
      statusText = "PAUSED: " + m_selectedProcess;
    }
    else {
      statusText = "RUNNING: " + m_selectedProcess;
    }
  }
  else {
    statusText = "READY: " + m_selectedProcess;
  }

  // Center the text horizontally
  ImVec2 textSize = ImGui::CalcTextSize(statusText.c_str());
  float textPosX = (availableWidth - textSize.x) * 0.5f;
  ImGui::SetCursorPosX(textPosX);

  ImGui::Text("%s", statusText.c_str());

  // Reset font scale after status text
  ImGui::SetWindowFontScale(1.0f);
  ImGui::PopStyleColor(); // Pop text color

  // Move cursor to end of green background area
  ImGui::SetCursorPosY(cursorPos.y + statusHeight);

  // Add some spacing below green area
  ImGui::Spacing();

  // Progress bar below the green background
  float progressValue = m_processRunning ? m_progress : 0.0f; // 0% when idle

  // Create progress text with percentage
  char progressText[32];
  snprintf(progressText, sizeof(progressText), "%.1f%%", progressValue * 100.0f);

  ImGui::ProgressBar(progressValue, ImVec2(availableWidth, 25.0f), progressText);
}




// UPDATE: StartProcess method to sync auto-confirm on sequence start
void RunPageUI::StartProcess(const std::string& processName) {
  if (m_processRunning) {
    m_logger->LogWarning("RunPageUI: Process already running");
    return;
  }

  // Check if this is a UAA3 sequence and sync auto-confirm
  bool isUAA3 = (processName.find("UAA3_") == 0);
  if (isUAA3 && m_promptUI) {
    // Ensure auto-confirm settings are synced
    m_promptUI->SetAutoConfirm(m_autoConfirm);

    if (m_autoConfirm) {
      UpdateStatus("Starting UAA3 sequence with auto-confirm enabled (delay: " +
        std::to_string(m_promptUI->GetAutoConfirmDelay()) + "s)");
    }
    else {
      UpdateStatus("Starting UAA3 sequence with manual confirmation required");
    }
  }
  else if (isUAA3 && !m_promptUI) {
    UpdateStatus("UAA3 sequence requires UserPromptUI - falling back to legacy", true);
  }

  m_processRunning = true;
  m_processPaused = false;
  m_progress = 0.0f;
  m_stopRequested = false;
  m_pauseRequested = false;

  UpdateStatus("Starting process: " + processName);

  m_processThread = std::thread(&RunPageUI::ProcessThreadFunc, this, processName);
  m_processThread.detach();
}


// UPDATED: RenderColumn2 to include button ordering controls
void RunPageUI::RenderColumn2() {
    ImGui::Text("Process Filters & Order");
    ImGui::Separator();

    // Current preset info
    auto currentPresets = m_filterManager->GetAvailablePresetFiles();
    ImGui::Text("Available Presets: %zu", currentPresets.size());

    // Quick filter controls
    if (ImGui::Button("Configure Filters", ImVec2(-1, 30))) {
        ShowFilterConfiguration();
    }

    ImGui::Separator();

    // Show current filter info
    auto currentList = GetCurrentProcessList();
    auto sortedList = GetSortedProcessList();
    auto totalList = m_filterManager->GetAllAvailableProcesses();

    ImGui::Text("Visible processes: %zu / %zu", currentList.size(), totalList.size());

    // NEW: Show sort info
    if (m_filterManager) {
        int numberedCount = 0;
        for (const auto& process : sortedList) {
            if (m_filterManager->GetProcessSortNumber(process) > 0) {
                numberedCount++;
            }
        }
        ImGui::Text("Numbered buttons: %d / %zu", numberedCount, sortedList.size());
    }

    if (currentList.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "All processes hidden");
        ImGui::TextWrapped("Load a preset or configure filters to show processes");
    }

    ImGui::Separator();

    // NEW: Quick button ordering controls
    if (m_filterManager && !sortedList.empty()) {
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "Quick Button Ordering:");

        if (ImGui::Button("Number 1,2,3...", ImVec2(-1, 25))) {
            m_filterManager->AssignSequentialNumbers();
        }

        if (ImGui::Button("Number 10,20,30...", ImVec2(-1, 25))) {
            m_filterManager->AssignSpacedNumbers();
        }

        if (ImGui::Button("Clear All Numbers", ImVec2(-1, 25))) {
            m_filterManager->ClearAllSortNumbers();
        }

        ImGui::Separator();
    }

    // Auto-confirm checkbox (existing code)
    bool autoConfirmValue = m_autoConfirm;
    if (ImGui::Checkbox("Auto-confirm Interactions", &autoConfirmValue)) {
        m_autoConfirm = autoConfirmValue;

        if (m_promptUI) {
            m_promptUI->SetAutoConfirm(autoConfirmValue);
            UpdateStatus("Auto-confirm " + std::string(autoConfirmValue ? "enabled" : "disabled") +
                " for UAA3 sequences");
        }

        if (m_uiManager) {
            m_uiManager->SetAutoConfirm(autoConfirmValue);
        }

        std::string status = autoConfirmValue ? "Auto-confirm enabled" : "Auto-confirm disabled";
        UpdateStatus(status);
    }

    ImGui::Separator();

    // Completed Steps Section (existing code)
    RenderCompletedSteps();
}



// NEW: Render completed steps list with success/failure indication
void RunPageUI::RenderCompletedSteps() {
  ImGui::Text("Completed Steps");
  ImGui::Separator();

  // Show count with success/failure breakdown
  size_t successCount = 0;
  size_t failureCount = 0;
  for (const auto& step : m_completedSteps) {
    if (step.isSuccess) successCount++;
    else failureCount++;
  }

  ImGui::Text("Total: %zu (Success: %zu, Failed: %zu)", m_completedSteps.size(), successCount, failureCount);

  // Clear button
  if (ImGui::Button("Clear History", ImVec2(-1, 25))) {
    ClearCompletedSteps();
  }

  ImGui::Spacing();

  // Scrollable list of completed steps
  float remainingHeight = ImGui::GetContentRegionAvail().y;
  ImGui::BeginChild("CompletedStepsList", ImVec2(0, remainingHeight), true);

  if (m_completedSteps.empty()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No processes completed yet");
  }
  else {
    // Show most recent first
    for (int i = static_cast<int>(m_completedSteps.size()) - 1; i >= 0; i--) {
      const auto& completed = m_completedSteps[i];

      // Different colors for success vs failure
      if (completed.isSuccess) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Complete %s", completed.processName.c_str());
      }
      else {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "✗ Failed %s", completed.processName.c_str());
      }

      ImGui::Indent();
      ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%s (%s)", completed.dateTime.c_str(), completed.duration.c_str());

      // Show idle time if not the first process (00:00.000 means no previous process)
      if (completed.idleTime != "00:00.000") {
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Idle: %s", completed.idleTime.c_str());
      }

      ImGui::Unindent();
      ImGui::Spacing();
    }
  }

  ImGui::EndChild();
}// NEW: Add completed step with date/time/duration


// NEW: Add completed/failed step with date/time/duration/idle time/success status
void RunPageUI::AddCompletedStep(const std::string& stepName, const std::string& duration, const std::string& idleTime, bool isSuccess) {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Get current date/time
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto tm = *std::localtime(&time_t);

  char dateTimeStr[100];
  std::strftime(dateTimeStr, sizeof(dateTimeStr), "%Y-%m-%d %H:%M:%S", &tm);

  CompletedProcess completed;
  completed.processName = stepName;
  completed.dateTime = std::string(dateTimeStr);
  completed.duration = duration;
  completed.idleTime = idleTime;
  completed.isSuccess = isSuccess;

  m_completedSteps.push_back(completed);

  // Limit history size
  if (m_completedSteps.size() > MAX_COMPLETED_STEPS) {
    m_completedSteps.erase(m_completedSteps.begin());
  }
}

// NEW: Clear completed steps
void RunPageUI::ClearCompletedSteps() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_completedSteps.clear();
  m_hasLastProcessEndTime = false;  // Reset idle time tracking
  UpdateStatus("Completed processes history cleared");
}



// KEEP: GetCurrentProcessList method unchanged (for backward compatibility)
std::vector<std::string> RunPageUI::GetCurrentProcessList() const {
    // Get all processes from registry
    auto allProcesses = ProcessRegistry::GetInstance().GetAllProcessNames();

    // Apply filters if filter manager is available
    if (m_filterManager) {
        std::vector<std::string> filtered;
        filtered.reserve(allProcesses.size());

        for (const auto& process : allProcesses) {
            if (m_filterManager->IsProcessVisible(process)) {
                filtered.push_back(process);
            }
        }
        return filtered;
    }

    // If no filter manager, return all processes
    return allProcesses;
}



// UPDATED: GetSortedProcessList method - NEW method for sorted buttons
std::vector<std::string> RunPageUI::GetSortedProcessList() const {
    if (m_filterManager) {
        return m_filterManager->GetSortedFilteredProcessList();  // NEW: Use sorted method
    }

    // Fallback: return all processes from registry if no filter manager
    return ProcessRegistry::GetInstance().GetAllProcessNames();
}


// UPDATE: ProcessThreadFunc to track both completed and failed processes
void RunPageUI::ProcessThreadFunc(const std::string& processName) {
  // Record start time and calculate idle time
  m_processStartTime = std::chrono::steady_clock::now();

  std::string idleTimeStr = "00:00.000";
  if (m_hasLastProcessEndTime) {
    auto idleDuration = std::chrono::duration_cast<std::chrono::milliseconds>(m_processStartTime - m_lastProcessEndTime);
    auto totalMs = idleDuration.count();
    auto minutes = totalMs / 60000;
    auto seconds = (totalMs % 60000) / 1000;
    auto ms = totalMs % 1000;

    char idleStr[32];
    std::sprintf(idleStr, "%02d:%02d.%03d", static_cast<int>(minutes), static_cast<int>(seconds), static_cast<int>(ms));
    idleTimeStr = std::string(idleStr);
  }

  try {
    auto sequence = BuildSelectedProcess();
    if (!sequence) {
      UpdateStatus("Failed to build process sequence", true);
      m_processRunning = false;
      return;
    }

    const auto& operations = sequence->GetOperations();
    size_t totalOps = operations.size();

    UpdateStatus("Starting sequence with " + std::to_string(totalOps) + " operations");

    bool processSuccess = true;
    std::string failureReason = "";

    for (size_t i = 0; i < totalOps && !m_stopRequested; ++i) {
      // Handle pause
      while (m_pauseRequested && !m_stopRequested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      if (m_stopRequested) break;

      std::string stepDescription = operations[i]->GetDescription();
      UpdateStatus("Step " + std::to_string(i + 1) + "/" + std::to_string(totalOps) +
        ": " + stepDescription);

      bool success = operations[i]->Execute(m_machineOps);
      if (!success && !m_stopRequested) {
        UpdateStatus("Operation failed: " + stepDescription, true);
        processSuccess = false;
        failureReason = "Failed at step " + std::to_string(i + 1) + ": " + stepDescription;
        break;
      }

      m_progress = static_cast<float>(i + 1) / static_cast<float>(totalOps);
    }

    // Calculate final duration (for both success and failure)
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - m_processStartTime);

    // Format duration as mm:ss.xxx
    auto totalMs = duration.count();
    auto minutes = totalMs / 60000;
    auto seconds = (totalMs % 60000) / 1000;
    auto ms = totalMs % 1000;

    char durationStr[32];
    std::sprintf(durationStr, "%02d:%02d.%03d", static_cast<int>(minutes), static_cast<int>(seconds), static_cast<int>(ms));

    if (m_stopRequested) {
      UpdateStatus("Process stopped by user");
      // NEW: Track stopped processes as failed
      AddCompletedStep(processName, std::string(durationStr), idleTimeStr, false);
    }
    else if (processSuccess) {
      UpdateStatus("Process completed successfully - " + std::to_string(totalOps) + " operations executed");
      // NEW: Add completed process
      AddCompletedStep(processName, std::string(durationStr), idleTimeStr, true);
    }
    else {
      UpdateStatus("Process failed: " + failureReason, true);
      // NEW: Add failed process
      AddCompletedStep(processName, std::string(durationStr), idleTimeStr, false);
    }

    // Always record end time for next idle calculation (whether success or failure)
    m_lastProcessEndTime = endTime;
    m_hasLastProcessEndTime = true;

  }
  catch (const std::exception& e) {
    UpdateStatus("Process error: " + std::string(e.what()), true);

    // NEW: Track exception as failed process
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - m_processStartTime);
    auto totalMs = duration.count();
    auto minutes = totalMs / 60000;
    auto seconds = (totalMs % 60000) / 1000;
    auto ms = totalMs % 1000;

    char durationStr[32];
    std::sprintf(durationStr, "%02d:%02d.%03d", static_cast<int>(minutes), static_cast<int>(seconds), static_cast<int>(ms));

    AddCompletedStep(processName, std::string(durationStr), idleTimeStr, false);

    m_lastProcessEndTime = endTime;
    m_hasLastProcessEndTime = true;
  }

  m_processRunning = false;
  m_processPaused = false;
  m_progress = 0.0f;
}




// UPDATED: OnFilterChanged to handle sorted list changes
void RunPageUI::OnFilterChanged() {
    // Update selected process if it's no longer visible
    auto currentList = GetSortedProcessList();  // Use sorted list for consistency
    auto it = std::find(currentList.begin(), currentList.end(), m_selectedProcess);

    if (it == currentList.end() && !currentList.empty()) {
        // Selected process is no longer visible, select first available (which is now the first sorted)
        m_selectedProcess = currentList[0];
        UpdateStatus("Process selection changed due to filter update - selected: " + m_selectedProcess);
    }
    else if (currentList.empty()) {
        // No processes visible, keep current selection but warn user
        UpdateStatus("No processes visible with current filter settings");
    }
}




// UPDATED: RenderColumn3 to include Live View tab
void RunPageUI::RenderColumn3() {
  // Create tab bar for Column3
  if (ImGui::BeginTabBar("Column3Tabs", ImGuiTabBarFlags_None)) {

    // Status tab (existing content)
    if (ImGui::BeginTabItem("Status")) {
      RenderStatusTab();
      ImGui::EndTabItem();
    }

    // Detail Results tab (existing content)
    if (ImGui::BeginTabItem("Detail Results")) {
      RenderDetailResultsTab();
      ImGui::EndTabItem();
    }

    // NEW: Live View tab
    if (ImGui::BeginTabItem("Live View")) {
      RenderLiveViewTab();
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }
}



// REPLACE the RenderLiveViewTab() method in RunPageUI.cpp with this UIConfigVisualizer-style implementation

void RunPageUI::RenderLiveViewTab() {
  ImGui::Text("Live Camera Feed");
  ImGui::Separator();

  if (!m_cameraManager) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Camera system not available");
    ImGui::TextWrapped("Camera manager not initialized. Enable camera support in the main application.");
    return;
  }

  // Get available cameras
  auto cameraIds = m_cameraManager->GetCameraIds();

  if (cameraIds.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No cameras available");
    ImGui::TextWrapped("No cameras found. Please configure cameras in the Config section.");
    return;
  }

  // Camera selection dropdown (similar to UIConfigVisualizer)
  ImGui::Text("Camera:");

  // Find current selection index
  int currentCameraIndex = 0;
  if (!m_selectedCameraId.empty()) {
    for (size_t i = 0; i < cameraIds.size(); i++) {
      if (cameraIds[i] == m_selectedCameraId) {
        currentCameraIndex = (int)i;
        break;
      }
    }
  }
  else {
    // Select first camera by default
    m_selectedCameraId = cameraIds[0];
    SetSelectedCamera(m_selectedCameraId);
  }

  // Create camera selection array
  std::vector<const char*> cameraNames;
  for (const auto& id : cameraIds) {
    cameraNames.push_back(id.c_str());
  }

  ImGui::SetNextItemWidth(200);
  if (ImGui::Combo("##CameraSelection", &currentCameraIndex, cameraNames.data(), (int)cameraNames.size())) {
    SetSelectedCamera(cameraIds[currentCameraIndex]);
  }

  // Camera status and controls (similar to UIConfigVisualizer)
  if (!m_selectedCameraId.empty()) {
    ImGui::Spacing();
    auto status = m_cameraManager->GetCameraStatus(m_selectedCameraId);

    // Connection status
    ImGui::Text("Status: %s", status.connected ? "Connected" : "Disconnected");

    if (status.connected) {
      ImGui::SameLine();
      ImGui::Text("| Grabbing: %s", status.grabbing ? "Yes" : "No");

      // Quick controls
      ImGui::Spacing();
      if (!status.grabbing) {
        if (ImGui::Button("Start Live Feed", ImVec2(120, 25))) {
          // Use StartGrabbingWithBroadcast if available, otherwise regular StartGrabbing
          bool success = false;
          try {
            success = m_cameraManager->StartGrabbingWithBroadcast(m_selectedCameraId);
            UpdateStatus("Started live feed with broadcast: " + m_selectedCameraId);
          }
          catch (...) {
            success = m_cameraManager->StartGrabbing(m_selectedCameraId);
            UpdateStatus("Started live feed: " + m_selectedCameraId);
          }

          if (success && !m_cameraSubscriber) {
            // Ensure subscriber is initialized
            InitializeCameraFeed();
          }
        }
      }
      else {
        if (ImGui::Button("Stop Live Feed", ImVec2(120, 25))) {
          m_cameraManager->StopGrabbing(m_selectedCameraId);
          UpdateStatus("Stopped live feed: " + m_selectedCameraId);
        }
      }

      ImGui::SameLine();
      if (ImGui::Button("Reconnect", ImVec2(80, 25))) {
        m_cameraManager->ConnectCamera(m_selectedCameraId);
        UpdateStatus("Reconnecting camera: " + m_selectedCameraId);
      }
    }
    else {
      if (ImGui::Button("Connect Camera", ImVec2(120, 25))) {
        if (m_cameraManager->ConnectCamera(m_selectedCameraId)) {
          UpdateStatus("Connected camera: " + m_selectedCameraId);
        }
        else {
          UpdateStatus("Failed to connect camera: " + m_selectedCameraId);
        }
      }
    }
  }

  ImGui::Separator();

  // Live video feed display area
  ImVec2 availableSize = ImGui::GetContentRegionAvail();
  float feedHeight = availableSize.y - 60.0f; // Leave space for statistics

  // Calculate canvas size with aspect ratio (similar to UIConfigVisualizer)
  const float CAMERA_ASPECT_RATIO = 1280.0f / 1024.0f;
  ImVec2 canvasSize;
  canvasSize.x = availableSize.x;
  canvasSize.y = canvasSize.x / CAMERA_ASPECT_RATIO;

  // Ensure we don't exceed available height
  if (canvasSize.y > feedHeight) {
    canvasSize.y = feedHeight;
    canvasSize.x = canvasSize.y * CAMERA_ASPECT_RATIO;
  }

  // Minimum size check
  if (canvasSize.x < 160.0f) {
    canvasSize.x = 160.0f;
    canvasSize.y = 160.0f / CAMERA_ASPECT_RATIO;
  }

  // Create camera canvas (similar to UIConfigVisualizer)
  ImGui::BeginChild("CameraCanvas", canvasSize, true, ImGuiWindowFlags_NoScrollbar);

  // Render camera feed using existing method
  RenderCameraFeedFromSubscriber(canvasSize);

  ImGui::EndChild();

  // Display statistics below the feed (similar to UIConfigVisualizer)
  if (m_cameraSubscriber) {
    ImGui::Text("Frames: %llu | Subscriber: %s",
      m_cameraSubscriber->GetTotalFramesReceived(),
      m_cameraSubscriber->GetSubscriberId().c_str());

    if (m_textureInitialized) {
      ImGui::SameLine();
      ImGui::Text("| Resolution: %ux%u", m_textureWidth, m_textureHeight);
    }

    // Frame rate calculation
    static auto lastTime = std::chrono::steady_clock::now();
    static uint64_t lastFrameCount = 0;
    static float frameRate = 0.0f;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime).count();

    if (elapsed >= 2000) { // Update every 2 seconds
      uint64_t currentFrameCount = m_cameraSubscriber->GetTotalFramesReceived();
      uint64_t frameDelta = currentFrameCount - lastFrameCount;
      frameRate = (float)frameDelta / (elapsed / 1000.0f);

      lastTime = now;
      lastFrameCount = currentFrameCount;
    }

    ImGui::SameLine();
    ImGui::Text("| FPS: %.1f", frameRate);

    // Status indicators
    ImGui::Text("Camera Connected: %s | Camera Grabbing: %s",
      m_cameraSubscriber->IsCameraConnected() ? "Yes" : "No",
      m_cameraSubscriber->IsCameraGrabbing() ? "Yes" : "No");

    // Debug: Show if subscriber is receiving frames
    if (m_cameraSubscriber->GetTotalFramesReceived() == 0) {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No frames received - check camera connection and grabbing status");
    }
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No subscriber - camera feed not initialized");
    if (ImGui::Button("Initialize Camera Feed", ImVec2(-1, 25))) {
      InitializeCameraFeed();
      UpdateStatus("Attempting to initialize camera feed for: " + m_selectedCameraId);
    }
  }
}

// ALSO ADD: Update the SetCameraManager method to ensure proper initialization
void RunPageUI::SetCameraManager(CameraManager* cameraManager) {
  m_cameraManager = cameraManager;

  if (m_cameraManager) {
    auto cameraIds = m_cameraManager->GetCameraIds();
    if (!cameraIds.empty()) {
      m_selectedCameraId = cameraIds[0];
      // Don't initialize feed immediately - let user select camera first
      m_cameraSystemInitialized = true;
    }
    m_logger->LogInfo("RunPageUI: Camera manager set with " +
      std::to_string(cameraIds.size()) + " cameras");
  }
  else {
    m_logger->LogInfo("RunPageUI: Camera manager cleared");
    m_cameraSystemInitialized = false;
  }
}

// ALSO UPDATE: InitializeCameraFeed method to be more robust
void RunPageUI::InitializeCameraFeed() {
  if (m_selectedCameraId.empty() || !m_cameraManager) {
    m_cameraSystemInitialized = false;
    UpdateStatus("Cannot initialize camera feed: no camera selected or manager unavailable");
    return;
  }

  // Clear existing subscription
  ClearCameraFeed();

  // Create new subscriber
  m_cameraSubscriber = std::make_shared<LiveVideoSubscriber>(m_selectedCameraId);

  // Subscribe to the broadcasting system
  m_cameraManager->SubscribeToFrames(m_cameraSubscriber);

  // Start broadcast system if not already active
  m_cameraManager->StartBroadcastSystem();

  m_cameraSystemInitialized = true;
  m_logger->LogInfo("RunPageUI: Camera feed initialized for: " + m_selectedCameraId);
  UpdateStatus("Camera feed initialized for: " + m_selectedCameraId);
}


// NEW: Camera feed rendering using subscriber
void RunPageUI::RenderCameraFeedFromSubscriber(const ImVec2& canvasSize) {
  // Update texture from latest frame
  UpdateCameraTexture();

  if (m_textureInitialized && m_textureWidth > 0 && m_textureHeight > 0) {
    // Calculate display size maintaining aspect ratio
    float aspectRatio = (float)m_textureWidth / (float)m_textureHeight;

    float displayWidth = canvasSize.x;
    float displayHeight = displayWidth / aspectRatio;

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
    ImGui::Image((ImTextureID)(intptr_t)m_cameraTextureID,
      ImVec2(displayWidth, displayHeight));

    // Add info overlay on hover
    if (ImGui::IsItemHovered() && m_cameraSubscriber) {
      ImGui::SetTooltip("Live Camera Feed\nCamera: %s\nResolution: %ux%u\nFrames: %llu",
        m_selectedCameraId.c_str(),
        m_textureWidth,
        m_textureHeight,
        m_cameraSubscriber->GetTotalFramesReceived());
    }
  }
  else {
    // Show status-based placeholder
    std::string message;
    if (!m_cameraSystemInitialized) {
      message = "Camera System Not Initialized\nNo camera manager available";
    }
    else if (!m_cameraSubscriber) {
      message = "No Camera Subscriber\nSelect a camera to begin";
    }
    else if (!m_cameraSubscriber->IsCameraConnected()) {
      message = "Camera Disconnected\nCamera: " + m_selectedCameraId + "\nCheck camera connection";
    }
    else if (!m_cameraSubscriber->IsCameraGrabbing()) {
      message = "Camera Not Grabbing\nCamera: " + m_selectedCameraId + "\nStart live feed to begin";
    }
    else {
      message = "Waiting for Video Frames...\nCamera: " + m_selectedCameraId +
        "\nFrames received: " + std::to_string(m_cameraSubscriber->GetTotalFramesReceived());
    }

    RenderCameraPlaceholder(canvasSize, message);
  }
}

// NEW: Camera placeholder rendering
void RunPageUI::RenderCameraPlaceholder(const ImVec2& canvasSize, const std::string& message) {
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();
  ImVec2 canvasMax = ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);

  // Background
  drawList->AddRectFilled(canvasPos, canvasMax, IM_COL32(60, 60, 60, 255));
  drawList->AddRect(canvasPos, canvasMax, IM_COL32(100, 150, 200, 255), 0.0f, 0, 2.0f);

  // Status text
  ImVec2 textSize = ImGui::CalcTextSize(message.c_str());
  ImVec2 textPos = ImVec2(
    canvasPos.x + (canvasSize.x - textSize.x) * 0.5f,
    canvasPos.y + (canvasSize.y - textSize.y) * 0.5f
  );
  drawList->AddText(textPos, IM_COL32(200, 200, 200, 255), message.c_str());
}

// NEW: Camera controls rendering
void RunPageUI::RenderCameraControls() {
  if (!m_cameraManager) {
    ImGui::Text("Camera Manager not available");
    return;
  }

  auto cameraIds = m_cameraManager->GetCameraIds();
  if (cameraIds.empty()) {
    ImGui::Text("No cameras available");
    if (ImGui::Button("Refresh Camera List", ImVec2(-1, 25))) {
      m_logger->LogInfo("Refreshing camera list");
    }
    return;
  }

  // Camera selection
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

  ImGui::SetNextItemWidth(200);
  if (ImGui::Combo("##CameraSelection", &currentCameraIndex, cameraNames.data(), (int)cameraNames.size())) {
    SetSelectedCamera(cameraIds[currentCameraIndex]);
  }

  // Camera status and controls
  if (!m_selectedCameraId.empty()) {
    ImGui::SameLine();

    auto status = m_cameraManager->GetCameraStatus(m_selectedCameraId);

    // Connection status
    if (status.connected) {
      if (status.grabbing) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "[LIVE]");
      }
      else {
        ImGui::TextColored(ImVec4(0, 0.8f, 0, 1), "[CONN]");
      }
    }
    else {
      ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "[DISC]");
    }

    // Quick controls
    ImGui::Spacing();

    if (status.connected) {
      if (!status.grabbing) {
        if (ImGui::Button("Start Live Feed", ImVec2(120, 25))) {
          m_cameraManager->StartGrabbing(m_selectedCameraId);
          m_logger->LogInfo("Started grabbing for camera: " + m_selectedCameraId);
        }
      }
      else {
        if (ImGui::Button("Stop Live Feed", ImVec2(120, 25))) {
          m_cameraManager->StopGrabbing(m_selectedCameraId);
          m_logger->LogInfo("Stopped grabbing for camera: " + m_selectedCameraId);
        }
      }

      ImGui::SameLine();
      if (ImGui::Button("Reconnect", ImVec2(80, 25))) {
        m_cameraManager->ConnectCamera(m_selectedCameraId);
        m_logger->LogInfo("Reconnecting camera: " + m_selectedCameraId);
      }
    }
    else {
      if (ImGui::Button("Connect Camera", ImVec2(120, 25))) {
        m_cameraManager->ConnectCamera(m_selectedCameraId);
        m_logger->LogInfo("Connecting camera: " + m_selectedCameraId);
      }
    }

    // Show subscriber statistics
    if (m_cameraSubscriber) {
      ImGui::SameLine();
      ImGui::Text("| Frames: %llu", m_cameraSubscriber->GetTotalFramesReceived());
    }
  }
}

// NEW: Update OpenGL texture from camera frames
void RunPageUI::UpdateCameraTexture() {
  if (!m_cameraSubscriber || !m_cameraSubscriber->HasNewFrame()) {
    return;
  }

  CameraFrameData frameData = m_cameraSubscriber->GetLatestFrame();
  m_cameraSubscriber->MarkFrameConsumed();

  if (!frameData.IsValid() || frameData.channels != 3) {
    return;
  }

  // Create texture if not initialized
  if (!m_textureInitialized) {
    glGenTextures(1, &m_cameraTextureID);
    glBindTexture(GL_TEXTURE_2D, m_cameraTextureID);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    m_textureInitialized = true;
    m_textureWidth = frameData.width;
    m_textureHeight = frameData.height;

    // Upload initial texture data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frameData.width, frameData.height,
      0, GL_RGB, GL_UNSIGNED_BYTE, frameData.imageData.data());

    m_logger->LogInfo("RunPageUI: Created OpenGL texture " + std::to_string(m_cameraTextureID) +
      " (" + std::to_string(frameData.width) + "x" + std::to_string(frameData.height) + ")");
  }
  else {
    glBindTexture(GL_TEXTURE_2D, m_cameraTextureID);

    // Check if we need to resize texture
    if (frameData.width != m_textureWidth || frameData.height != m_textureHeight) {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frameData.width, frameData.height,
        0, GL_RGB, GL_UNSIGNED_BYTE, frameData.imageData.data());
      m_textureWidth = frameData.width;
      m_textureHeight = frameData.height;

      m_logger->LogInfo("RunPageUI: Resized texture to " +
        std::to_string(frameData.width) + "x" + std::to_string(frameData.height));
    }
    else {
      // Update existing texture (more efficient)
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frameData.width, frameData.height,
        GL_RGB, GL_UNSIGNED_BYTE, frameData.imageData.data());
    }
  }

  glBindTexture(GL_TEXTURE_2D, 0);
}

// NEW: Cleanup OpenGL texture
void RunPageUI::CleanupCameraTexture() {
  if (m_textureInitialized) {
    glDeleteTextures(1, &m_cameraTextureID);
    m_textureInitialized = false;
    m_cameraTextureID = 0;
    m_textureWidth = 0;
    m_textureHeight = 0;
    m_logger->LogInfo("RunPageUI: Cleaned up OpenGL texture");
  }
}


void RunPageUI::RenderStatusTab() {
  ImGui::Text("Status & Information");
  ImGui::Text("Process Filter Status:");
  ImGui::Text("Showing %d of %d sequences",
    static_cast<int>(GetCurrentProcessList().size()), 12);

  if (m_processRunning) {
    ImGui::Text("Process running");
  }
  else {
    ImGui::Text("No process running");

    ImGui::Separator();
    ImGui::Text("Ready to Start:");
    ImGui::BulletText("Select a process from Column 1");
    ImGui::BulletText("Click START to begin execution");
  }

  ImGui::Spacing();
  ImGui::Separator();

  // Filter usage guide - simplified
  if (ImGui::CollapsingHeader("Filter Usage Guide")) {
    ImGui::BulletText("Default: All processes are hidden");
    ImGui::BulletText("Load presets to show specific processes");
    ImGui::BulletText("Create custom presets with your preferred setup");
    ImGui::BulletText("Last used preset is remembered on restart");

    ImGui::Spacing();
    ImGui::TextWrapped("Tip: Use 'Configure Filters' in Column 2 to create and manage your custom presets");
  }

  // Current preset information
  if (ImGui::CollapsingHeader("Current Preset Info")) {
    auto availablePresets = m_filterManager->GetAvailablePresetFiles();

    if (availablePresets.empty()) {
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No custom presets available");
      ImGui::Text("Create your first preset using 'Configure Filters'");
    }
    else {
      ImGui::Text("Available Custom Presets:");
      for (const auto& preset : availablePresets) {
        ImGui::BulletText("%s", preset.c_str());
      }
    }
  }

  // Status Messages at bottom of tab
  // Calculate space for status area at bottom
  float remainingHeight = ImGui::GetContentRegionAvail().y;
  float statusAreaHeight = 200.0f;

  // If we have space, add the status area at bottom
  if (remainingHeight > statusAreaHeight + 20) {
    // Add spacer to push status to bottom
    ImGui::Dummy(ImVec2(0, remainingHeight - statusAreaHeight - 20));
  }

  ImGui::Separator();
  RenderStatusArea(); // Status area at bottom
}

void RunPageUI::RenderDetailResultsTab() {
  // Embed the existing OperationsDisplayUI
  if (m_operationsDisplayUI) {
    // Render the operations display UI within this tab
    m_operationsDisplayUI->RenderUI();
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Operations Display UI not available");
    ImGui::Text("Unable to load operation results display.");
  }
}

void RunPageUI::RenderControlButtons() {
  // Calculate button width for 3 horizontal buttons
  float buttonWidth = (ImGui::GetContentRegionAvail().x - 2 * ImGui::GetStyle().ItemSpacing.x) / 3.0f;
  float buttonHeight = 40.0f;

  // Start button (green when not running)
  if (!m_processRunning) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.8f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.6f, 0.1f, 1.0f));
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
  }

  if (ImGui::Button("START", ImVec2(buttonWidth, buttonHeight))) {
    if (!m_processRunning) {
      StartProcess(m_selectedProcess);
    }
  }
  ImGui::PopStyleColor(3);

  ImGui::SameLine();

  // FIXED: Pause/Resume button with proper logic
  if (m_processRunning) {
    if (m_processPaused) {
      // Show RESUME button when paused (green)
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 0.2f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.8f, 0.3f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.6f, 0.1f, 1.0f));

      if (ImGui::Button("RESUME", ImVec2(buttonWidth, buttonHeight))) {
        ResumeProcess();
      }
    }
    else {
      // Show PAUSE button when running (yellow)
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.0f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.7f, 0.1f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.5f, 0.0f, 1.0f));

      if (ImGui::Button("PAUSE", ImVec2(buttonWidth, buttonHeight))) {
        PauseProcess();
      }
    }
  }
  else {
    // Disabled pause button when not running
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));

    ImGui::Button("PAUSE", ImVec2(buttonWidth, buttonHeight));
  }
  ImGui::PopStyleColor(3);

  ImGui::SameLine();

  // Stop button (red when process running)
  if (m_processRunning) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
  }

  if (ImGui::Button("STOP", ImVec2(buttonWidth, buttonHeight))) {
    if (m_processRunning) {
      StopProcess();
    }
  }
  ImGui::PopStyleColor(3);
}

// Keep the status area rendering method unchanged
void RunPageUI::RenderStatusArea() {
  ImGui::Text("Status Messages");

  // Status area with scrolling (keep same height)
  ImGui::BeginChild("StatusArea", ImVec2(0, 200), true, ImGuiWindowFlags_HorizontalScrollbar);

  // Display current status
  ImGui::Text("Current: %s", m_statusMessage.c_str());
  ImGui::Separator();

  // Display status history
  for (const auto& msg : m_statusHistory) {
    ImGui::Text("%s", msg.c_str());
  }

  // Auto-scroll to bottom if new messages
  if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
    ImGui::SetScrollHereY(1.0f);
  }

  ImGui::EndChild();
}


// UPDATED: RenderProcessButtons to use sorted process list
void RunPageUI::RenderProcessButtons() {
    ImGui::Text("Process Steps");

    // Push emoji font if available
    if (m_imguiFont) {
        ImGui::PushFont(m_imguiFont);
    }

    // Process buttons with vertical layout
    ImGui::BeginChild("ProcessButtons", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    const float buttonWidth = ImGui::GetContentRegionAvail().x * 0.95f;
    const float buttonHeight = 35.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

    // NEW: Use sorted filtered process list instead of regular filtered list
    auto processesToShow = GetSortedProcessList();

    if (processesToShow.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No processes visible");
        ImGui::Text("Load a preset to show processes");
        ImGui::Spacing();
        if (ImGui::Button("Configure Filters", ImVec2(buttonWidth, buttonHeight))) {
            ShowFilterConfiguration();
        }
    }
    else {
        for (const auto& process : processesToShow) {
            // Different colors for UAA3 vs Legacy processes (your existing logic)
            bool isUAA3 = (process.find("UAA3_") == 0);

            if (m_selectedProcess == process) {
                // Selected process - bright green
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.8f, 0.3f, 1.0f));
            }
            else if (isUAA3) {
                // UAA3 process - blue tint
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.6f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.7f, 1.0f));
            }
            else {
                // Legacy process - gray
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
            }

            // NEW: Enhanced display name with sort numbers
            std::string displayName = process;
            int sortNum = 0;

            if (m_filterManager) {
                sortNum = m_filterManager->GetProcessSortNumber(process);
            }

            if (sortNum > 0) {
                // Show button with number: "1. ProcessName"
                displayName = std::to_string(sortNum) + ". " + process;
            }
            else if (isUAA3) {
                // UAA3 without number: "⚡ ProcessName"
                displayName = std::string(reinterpret_cast<const char*>(u8"⚡ ")) + process;
            }
            // Regular processes without numbers show as normal

            if (ImGui::Button(displayName.c_str(), ImVec2(buttonWidth, buttonHeight))) {
                m_selectedProcess = process;
                if (!m_processRunning) {
                    StartProcess(process);
                }
            }

            // Enhanced tooltips with sort number info
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Process: %s", process.c_str());

                // NEW: Show sort number in tooltip
                if (sortNum > 0) {
                    ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "Button Order: %d", sortNum);
                }

                if (isUAA3) {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "UAA3 Modern Sequence");
                    ImGui::Text("• Uses UserPromptUI");
                    ImGui::Text("• Enhanced safety checks");
                    if (!m_promptUI) {
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "⚠ Requires UserPromptUI setup");
                    }
                }
                else {
                    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Legacy Sequence");
                    ImGui::Text("• Uses MockUserInteractionManager");
                }

                // NEW: Quick sort number assignment in tooltip
                if (m_filterManager && !m_processRunning) {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "Right-click for quick number assignment");
                }

                ImGui::EndTooltip();
            }

            // NEW: Right-click context menu for quick sort number assignment
            if (ImGui::BeginPopupContextItem(("ProcessMenu_" + process).c_str())) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Process: %s", process.c_str());

                if (isUAA3) {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "Type: UAA3 Modern");
                }
                else {
                    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Type: Legacy");
                }

                ImGui::Separator();

                // NEW: Quick sort number assignment
                if (m_filterManager) {
                    ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "Button Order:");

                    static int quickSortNum = sortNum > 0 ? sortNum : 1;
                    ImGui::SetNextItemWidth(80);
                    if (ImGui::InputInt("Number", &quickSortNum, 1, 5)) {
                        if (quickSortNum < 0) quickSortNum = 0;
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Apply")) {
                        m_filterManager->SetProcessSortNumber(process, quickSortNum);
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Remove")) {
                        m_filterManager->RemoveProcessSortNumber(process);
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::Separator();
                }

                // Existing process details section
                try {
                    std::string originalSelected = m_selectedProcess;
                    m_selectedProcess = process;

                    auto sequence = BuildSelectedProcess();
                    if (sequence) {
                        const auto& operations = sequence->GetOperations();
                        ImGui::Text("Operations: %zu", operations.size());
                        ImGui::Spacing();

                        for (size_t i = 0; i < (std::min)(operations.size(), size_t(10)); ++i) {
                            ImGui::Text("%zu. %s", i + 1, operations[i]->GetDescription().c_str());
                        }

                        if (operations.size() > 10) {
                            ImGui::Text("... and %zu more", operations.size() - 10);
                        }
                    }
                    else {
                        ImGui::Text("Error: Could not build sequence");
                        if (isUAA3 && !m_promptUI) {
                            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "UserPromptUI not available");
                        }
                    }

                    m_selectedProcess = originalSelected;
                }
                catch (const std::exception& e) {
                    ImGui::Text("Error: %s", e.what());
                }

                if (ImGui::Button("Close")) {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            ImGui::PopStyleColor(2);
            ImGui::Spacing();
        }
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();

    // Pop emoji font if we pushed it
    if (m_imguiFont) {
        ImGui::PopFont();
    }
}

// UPDATE: Fix PauseProcess method
void RunPageUI::PauseProcess() {
  if (!m_processRunning || m_processPaused) {
    return;
  }

  m_processPaused = true;
  m_pauseRequested = true;
  UpdateStatus("Process paused - Click RESUME to continue");
}

// NEW: Add ResumeProcess method
void RunPageUI::ResumeProcess() {
  if (!m_processRunning || !m_processPaused) {
    return;
  }

  m_processPaused = false;
  m_pauseRequested = false;  // Clear the pause request
  UpdateStatus("Process resumed");
}

// UPDATE: Fix StopProcess to handle paused state
void RunPageUI::StopProcess() {
  if (!m_processRunning) {
    return;
  }

  UpdateStatus("Stopping process...");
  m_stopRequested = true;

  // If paused, we need to resume briefly to let the thread exit
  if (m_processPaused) {
    m_pauseRequested = false;  // Clear pause to allow thread to exit
  }

  int timeoutCounter = 0;
  while (m_processRunning && timeoutCounter < 100) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    timeoutCounter++;
  }

  if (m_processRunning) {
    m_processRunning = false;
    UpdateStatus("Process forcibly terminated", true);
  }
  else {
    UpdateStatus("Process stopped");
  }

  // Reset all state
  m_processPaused = false;
  m_pauseRequested = false;
}

void RunPageUI::UpdateStatus(const std::string& message, bool isError) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_statusMessage = message;

  // Add to history with timestamp
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto tm = *std::localtime(&time_t);

  char timeStr[100];
  std::strftime(timeStr, sizeof(timeStr), "[%H:%M:%S]", &tm);

  std::string fullMessage = std::string(timeStr) + " " + message;
  m_statusHistory.push_back(fullMessage);

  // Limit history size
  if (m_statusHistory.size() > MAX_STATUS_HISTORY) {
    m_statusHistory.erase(m_statusHistory.begin());
  }

  if (isError) {
    m_logger->LogError("RunPageUI: " + message);
  }
  else {
    m_logger->LogInfo("RunPageUI: " + message);
  }
}


// ============================================================================
// REPLACE: BuildSelectedProcess() method in RunPageUI.cpp
// ============================================================================
std::unique_ptr<SequenceStep> RunPageUI::BuildSelectedProcess() {
    // Check if process exists in registry
    if (ProcessRegistry::GetInstance().HasProcess(m_selectedProcess)) {
        if (m_promptUI) {
            auto process = ProcessRegistry::GetInstance().BuildProcess(m_selectedProcess, m_machineOps, *m_promptUI);
            if (process) {
                UpdateStatus("Building process: " + m_selectedProcess);
                return process;
            }
            else {
                UpdateStatus("Failed to build process: " + m_selectedProcess, true);
                return nullptr;
            }
        }
        else {
            const auto* processInfo = ProcessRegistry::GetInstance().GetProcessInfo(m_selectedProcess);
            if (processInfo && processInfo->requiresUserPromptUI) {
                UpdateStatus("UserPromptUI not available for " + m_selectedProcess, true);
            }
            else {
                UpdateStatus("UserPromptUI not configured", true);
            }
            return nullptr;
        }
    }

    // Process not found in registry
    UpdateStatus("Unknown process selected: " + m_selectedProcess, true);
    return nullptr;
}

void RunPageUI::SetImguiFont(ImFont* font) {
  if (font) {
    m_imguiFont = font;
    std::cout << "RunPageUI: Custom font set successfully" << std::endl;
  }
  else {
    std::cerr << "RunPageUI: Failed to set custom font - font is null" << std::endl;
  }
}



// NEW: Camera management methods

void RunPageUI::SetSelectedCamera(const std::string& cameraId) {
  if (m_selectedCameraId == cameraId) {
    return; // No change
  }

  m_selectedCameraId = cameraId;

  if (m_cameraSubscriber) {
    // Update existing subscriber target
    m_cameraSubscriber->SetTargetCamera(cameraId);
    m_logger->LogInfo("RunPageUI: Camera switched to: " + cameraId);
  }
  else {
    // Initialize new feed
    InitializeCameraFeed();
  }
}


void RunPageUI::ClearCameraFeed() {
  if (m_cameraSubscriber && m_cameraManager) {
    m_cameraManager->UnsubscribeFromFrames(m_cameraSubscriber->GetSubscriberId());
    m_cameraSubscriber.reset();
    m_logger->LogInfo("RunPageUI: Camera feed cleared");
  }
  CleanupCameraTexture();
  m_cameraSystemInitialized = false;
}