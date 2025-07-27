#include "RunPageUI.h"
#include "imgui.h"
#include <chrono>
#include <iostream>
#include <algorithm>
#include "uaa3_process_builders.h"
#include "ProcessRegistry.h"

// UPDATE RunPageUI.cpp - Constructor
RunPageUI::RunPageUI(MachineOperations& machineOps)
  : m_machineOps(machineOps),
  m_uiManager(std::make_unique<MockUserInteractionManager>()),
  m_stopRequested(false),
  m_pauseRequested(false)
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

RunPageUI::~RunPageUI() {
  StopProcess();
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
  ImGui::Text("Process Control");
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

// UPDATE: RenderColumn2 - Add completed steps section
// UPDATE: RenderColumn2 - Add completed steps section
// UPDATE: RenderColumn2 - Add completed steps section
void RunPageUI::RenderColumn2() {
  ImGui::Text("Process Filters");
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
  auto totalList = m_filterManager->GetAllAvailableProcesses();
  ImGui::Text("Visible processes: %zu / %zu", currentList.size(), totalList.size());

  if (currentList.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "All processes hidden");
    ImGui::TextWrapped("Load a preset or configure filters to show processes");
  }

  ImGui::Separator();


  // Auto-confirm checkbox with UserPromptUI integration
  bool autoConfirmValue = m_autoConfirm;
  if (ImGui::Checkbox("Auto-confirm Interactions", &autoConfirmValue)) {
    m_autoConfirm = autoConfirmValue;

    // Update UserPromptUI auto-confirm setting
    if (m_promptUI) {
      m_promptUI->SetAutoConfirm(autoConfirmValue);
      UpdateStatus("Auto-confirm " + std::string(autoConfirmValue ? "enabled" : "disabled") +
        " for UAA3 sequences");
    }

    // Update legacy MockUserInteractionManager
    if (m_uiManager) {
      m_uiManager->SetAutoConfirm(autoConfirmValue);
    }

    std::string status = autoConfirmValue ? "Auto-confirm enabled" : "Auto-confirm disabled";
    UpdateStatus(status);
  }

  ImGui::Separator();

  // NEW: Completed Steps Section
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


// ============================================================================
// REPLACE: GetCurrentProcessList() method in RunPageUI.cpp
// ============================================================================
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




// NEW: Handle filter changes
void RunPageUI::OnFilterChanged() {
  // Update selected process if it's no longer visible
  auto currentList = GetCurrentProcessList();
  auto it = std::find(currentList.begin(), currentList.end(), m_selectedProcess);

  if (it == currentList.end() && !currentList.empty()) {
    // Selected process is no longer visible, select first available
    m_selectedProcess = currentList[0];
    UpdateStatus("Process selection changed due to filter update");
  }
  else if (currentList.empty()) {
    // No processes visible, keep current selection but warn user
    UpdateStatus("No processes visible with current filter settings");
  }
}


void RunPageUI::RenderColumn3() {
  // Create tab bar for Column3
  if (ImGui::BeginTabBar("Column3Tabs", ImGuiTabBarFlags_None)) {

    // Status tab (existing content)
    if (ImGui::BeginTabItem("Status")) {
      RenderStatusTab();
      ImGui::EndTabItem();
    }

    // Detail Results tab (new OperationsDisplayUI)
    if (ImGui::BeginTabItem("Detail Results")) {
      RenderDetailResultsTab();
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
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

// UPDATE: RenderProcessButtons to use filtered list
void RunPageUI::RenderProcessButtons() {
  ImGui::Text("Process Steps");

  // Process buttons with vertical layout
  ImGui::BeginChild("ProcessButtons", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

  const float buttonWidth = ImGui::GetContentRegionAvail().x * 0.95f;
  const float buttonHeight = 35.0f;

  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

  // NEW: Use filtered process list instead of static list
  auto processesToShow = GetCurrentProcessList();

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
      // Different colors for UAA3 vs Legacy processes
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

      // Add prefix indicator for UAA3 processes
      std::string displayName = process;
      if (isUAA3) {
        displayName = "🔧 " + process;
      }

      if (ImGui::Button(displayName.c_str(), ImVec2(buttonWidth, buttonHeight))) {
        m_selectedProcess = process;
        if (!m_processRunning) {
          StartProcess(process);
        }
      }

      // Enhanced tooltips with filter info
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Process: %s", process.c_str());
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
        ImGui::EndTooltip();
      }

      // Right-click for process details
      if (ImGui::BeginPopupContextItem(("ProcessMenu_" + process).c_str())) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Process: %s", process.c_str());

        if (isUAA3) {
          ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "Type: UAA3 Modern");
        }
        else {
          ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Type: Legacy");
        }

        ImGui::Separator();

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

