#include "RunPageUI.h"
#include "imgui.h"
#include <chrono>
#include <iostream>
#include <algorithm>
#include "uaa3_process_builders.h"
#include "ProcessRegistry.h"
#include "LiveVideoSubscriber.h"
#include "ManualAdjustmentOperation.h"
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

  // ADD THIS: Render any active manual adjustment windows
  ManualAdjustmentRegistry::GetInstance().RenderAll();
}


// In RunPageUI.cpp, update RenderColumn1:
void RunPageUI::RenderColumn1() {
  ImGui::Text(reinterpret_cast<const char*>(u8"🔧 Process Control"));
  ImGui::Separator();

  // Render control buttons at the top
  RenderControlButtons();

  ImGui::Spacing();
  ImGui::Separator();

  // Single-line running status with large font and dark green background
  RenderRunningStatus();

  ImGui::Spacing();
  ImGui::Separator();

  // Progress bar
  //RenderProgressBar();

  ImGui::Spacing();
  ImGui::Separator();

  // NEW: Auto-start checkbox
  ImGui::Checkbox("Automatic Start", &m_autoStartOnSelect);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("When checked, selecting a process will automatically start it");
  }

  ImGui::Separator();
  ImGui::Spacing();

  // NEW: Render process tree view instead of buttons
  RenderProcessTreeView();
}


// Add this method to RunPageUI.cpp
void RunPageUI::RenderProgressBar() {
  // Get available width
  float availableWidth = ImGui::GetContentRegionAvail().x;

  // Calculate progress value
  float progressValue = m_processRunning ? m_progress : 0.0f;

  // Create progress text with percentage
  char progressText[32];
  snprintf(progressText, sizeof(progressText), "%.1f%%", progressValue * 100.0f);

  // Render the progress bar
  ImGui::ProgressBar(progressValue, ImVec2(availableWidth, 25.0f), progressText);
}



void RunPageUI::RenderProcessTreeView() {
  ImGui::Text("Process Steps");
  ImGui::Separator();

  // Create a scrollable region
  ImGui::BeginChild("ProcessTree", ImVec2(0, 0), true,
    ImGuiWindowFlags_HorizontalScrollbar);

  auto sortedList = GetSortedProcessList();

  // Make items taller
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 10));    // More space between items
  ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f)); // Center text vertically

  for (const auto& process : sortedList) {
    std::string displayName = process;
    if (m_filterManager) {
      int sortNum = m_filterManager->GetProcessSortNumber(process);
      if (sortNum > 0) {
        displayName = std::to_string(sortNum) + ". " + process;
      }
    }

    bool isSelected = (m_selectedProcess == process);
    bool isRunning = (m_processRunning && m_selectedProcess == process);

    if (isRunning) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    }

    // Use Selectable with custom height
    if (ImGui::Selectable(displayName.c_str(), isSelected, 0, ImVec2(0, 35))) { // 35 pixels tall
      m_selectedProcess = process;
      UpdateStatus("Selected: " + process);
      ExtractSelectedProcessOperations();

      if (m_autoStartOnSelect && !m_processRunning) {
        StartProcess(process);
      }
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Click to select: %s\n%s",
        process.c_str(),
        m_autoStartOnSelect ? "Will auto-start" : "Use START button to run");
    }

    if (isRunning) {
      ImGui::PopStyleColor();
    }
  }

  ImGui::PopStyleVar(2);

  ImGui::EndChild();
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



// In RunPageUI.cpp, update RenderColumn2:
void RunPageUI::RenderColumn2() {
  ImGui::Text(reinterpret_cast<const char*>(u8"📊 Status & Controls"));
  ImGui::Separator();

  // Add tab bar for the second column
  if (ImGui::BeginTabBar("Column2Tabs")) {

    // Tab 1: Sequence Breakdown (NEW - show first)
    if (ImGui::BeginTabItem("Sequence")) {
      RenderSequenceBreakdownTab();
      ImGui::EndTabItem();
    }

    // Tab 2: Process Config 
    if (ImGui::BeginTabItem("Config")) {
      RenderProcessConfigTab();
      ImGui::EndTabItem();
    }

    // Tab 3: Status & History 
    if (ImGui::BeginTabItem("Status")) {
      RenderStatusTabCol2();
      ImGui::EndTabItem();
    }

    // Tab 4: Global Jog
    if (ImGui::BeginTabItem("Jog")) {
      RenderJogControlTab();
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }
}

// Update RenderSequenceBreakdownTab() in RunPageUI.cpp:
void RunPageUI::RenderSequenceBreakdownTab() {
  ImGui::Text("Sequence Operations");
  ImGui::Separator();

  if (m_selectedProcess.empty()) {
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
      "Select a process to view its operations");
    return;
  }

  // Show selected process name with better formatting
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 1.0f, 0.8f, 1.0f));
  ImGui::Text("Process:");
  ImGui::PopStyleColor();
  ImGui::SameLine();
  ImGui::TextWrapped("%s", m_selectedProcess.c_str());
  ImGui::Separator();

  // Show operation count
  ImGui::Text("Total Operations: %zu", m_selectedProcessOperations.size());
  ImGui::Spacing();

  // Get current operation index safely
  size_t currentOpIndex = 0;
  bool isExecuting = false;
  {
    std::lock_guard<std::mutex> lock(m_operationMutex);
    currentOpIndex = m_currentOperationIndex;
    isExecuting = m_operationInProgress && m_processRunning;
  }

  // Create scrollable region with horizontal scrollbar
  ImGui::BeginChild("OperationsList", ImVec2(0, 0), true,
    ImGuiWindowFlags_HorizontalScrollbar);

  // Use a table for better alignment
  if (ImGui::BeginTable("OperationsTable", 2,
    ImGuiTableFlags_ScrollX |
    ImGuiTableFlags_RowBg |
    ImGuiTableFlags_BordersInnerV)) {

    // Setup columns
    ImGui::TableSetupColumn("Step", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableSetupColumn("Operation", ImGuiTableColumnFlags_WidthStretch);

    // Render each operation
    for (size_t i = 0; i < m_selectedProcessOperations.size(); ++i) {
      ImGui::TableNextRow();

      bool isCurrentOperation = (isExecuting && currentOpIndex == i);
      bool isCompleted = (isExecuting && i < currentOpIndex) ||
        (!m_processRunning && m_lastCompletedIndex >= i);

      // First column: Step number with status
      ImGui::TableSetColumnIndex(0);

      // In the table version, update the emoji parts:
      if (isCurrentOperation) {
        // Currently running - yellow
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%zu. %s", i + 1,
          reinterpret_cast<const char*>(u8"►"));
      }
      else if (isCompleted) {
        // Completed - green
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%zu. %s", i + 1,
          reinterpret_cast<const char*>(u8"✓"));
      }
      else {
        // Not yet run - normal
        ImGui::Text("%zu.", i + 1);
      }

      // Second column: Operation description
      ImGui::TableSetColumnIndex(1);

      if (isCurrentOperation) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
      }
      else if (isCompleted) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f));
      }

      // Wrap text to prevent cutoff
      ImGui::TextWrapped("%s", m_selectedProcessOperations[i].c_str());

      // Add status tag if running
      if (isCurrentOperation) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), " [RUNNING]");
        ImGui::PopStyleColor();
      }
      else if (isCompleted) {
        ImGui::PopStyleColor();
      }

      // Tooltip for long descriptions
      if (ImGui::IsItemHovered() && m_selectedProcessOperations[i].length() > 50) {
        ImGui::SetTooltip("%s", m_selectedProcessOperations[i].c_str());
      }
    }

    ImGui::EndTable();
  }

  ImGui::EndChild();
}


// NEW: Extract operations from selected process
void RunPageUI::ExtractSelectedProcessOperations() {
  m_selectedProcessOperations.clear();

  if (m_selectedProcess.empty()) {
    return;
  }

  // Build the process to get its sequence
  auto sequence = BuildSelectedProcess();
  if (sequence) {
    // Get operations from the sequence
    const auto& operations = sequence->GetOperations();

    // Extract descriptions
    for (const auto& op : operations) {
      if (op) {
        m_selectedProcessOperations.push_back(op->GetDescription());
      }
    }

    m_logger->LogInfo("Extracted " + std::to_string(m_selectedProcessOperations.size()) +
      " operations from process: " + m_selectedProcess);
  }
}



// Move existing Column2 content to this new function
void RunPageUI::RenderStatusTabCol2() {
  // Status display section
  ImGui::Text("Current Status:");
  ImGui::Separator();

  // Show current status message
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    ImGui::TextWrapped("%s", m_statusMessage.c_str());
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Completed processes section with clear button
  float clearButtonWidth = 80.0f;
  ImGui::Text("Completed Processes:");
  ImGui::SameLine(ImGui::GetWindowWidth() - clearButtonWidth - 15);
  if (ImGui::SmallButton("Clear")) {
    ClearCompletedSteps();
  }

  ImGui::Separator();

  // Scrollable completed processes list
  ImGui::BeginChild("CompletedList", ImVec2(0, 0), true,
    ImGuiWindowFlags_HorizontalScrollbar);

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& completed : m_completedSteps) {
      // Color based on success/failure
      if (completed.isSuccess) {
        ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "[OK]");
      }
      else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[FAIL]");
      }

      ImGui::SameLine();
      ImGui::Text("%s", completed.processName.c_str());

      ImGui::Indent();
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
        "Time: %s", completed.dateTime.c_str());
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
        "Duration: %s", completed.duration.c_str());

      if (!completed.idleTime.empty() && completed.idleTime != "N/A") {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.0f, 1.0f),
          "Idle: %s", completed.idleTime.c_str());
      }

      ImGui::Unindent();
      ImGui::Spacing();
    }
  }

  ImGui::EndChild();
}

// NEW: Move all the original control functionality here
void RunPageUI::RenderProcessConfigTab() {
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

  // Show sort info
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

  // Quick button ordering controls
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

  // Auto-confirm checkbox
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

    std::string status = autoConfirmValue ?
      "Auto-confirm enabled - Interactions will auto-proceed" :
      "Auto-confirm disabled - Manual confirmation required";
    UpdateStatus(status);
  }

  ImGui::Spacing();

  // If auto-confirm is enabled, show delay slider
  if (m_autoConfirm && m_promptUI) {
    int delaySeconds = m_promptUI->GetAutoConfirmDelay();
    if (ImGui::SliderInt("Auto-confirm delay", &delaySeconds, 1, 10, "%d sec")) {
      m_promptUI->SetAutoConfirmDelay(delaySeconds);
      UpdateStatus("Auto-confirm delay set to " + std::to_string(delaySeconds) + " seconds");
    }
  }
}

// New placeholder function for jog control
void RunPageUI::RenderJogControlTab() {
  ImGui::Text("Global Jog Controls place holder");
  
}


void RunPageUI::RenderCompletedSteps() {
  ImGui::Text("Completed Steps");
  ImGui::Separator();

  // Variables to hold data after mutex is released
  size_t successCount = 0;
  size_t failureCount = 0;
  std::vector<CompletedProcess> stepsCopy;
  bool shouldClear = false;

  // Scope block for mutex lock
  {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Count successes/failures
    for (const auto& step : m_completedSteps) {
      if (step.isSuccess) successCount++;
      else failureCount++;
    }

    // Make a copy for rendering
    stepsCopy = m_completedSteps;
  } // lock automatically released here when it goes out of scope

  // Now we can safely render without holding the lock
  ImGui::Text("Total: %zu (Success: %zu, Failed: %zu)",
    stepsCopy.size(), successCount, failureCount);

  // Clear button
  if (ImGui::Button("Clear History", ImVec2(-1, 25))) {
    ClearCompletedSteps(); // This will acquire its own lock
  }

  ImGui::Spacing();

  // Render the list using our copy
  float remainingHeight = ImGui::GetContentRegionAvail().y;
  ImGui::BeginChild("CompletedStepsList", ImVec2(0, remainingHeight), true);

  if (stepsCopy.empty()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No processes completed yet");
  }
  else {
    // Show most recent first
    for (int i = static_cast<int>(stepsCopy.size()) - 1; i >= 0; i--) {
      const auto& completed = stepsCopy[i];

      if (completed.isSuccess) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Complete %s",
          completed.processName.c_str());
      }
      else {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "✗ Failed %s",
          completed.processName.c_str());
      }

      ImGui::Indent();
      ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%s (%s)",
        completed.dateTime.c_str(), completed.duration.c_str());

      if (completed.idleTime != "00:00.000") {
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Idle: %s",
          completed.idleTime.c_str());
      }

      ImGui::Unindent();
      ImGui::Spacing();
    }
  }

  ImGui::EndChild();
}

// NEW: Add completed/failed step with date/time/duration/idle time/success status
void RunPageUI::AddCompletedStep(const std::string& stepName, const std::string& duration, const std::string& idleTime, bool isSuccess) {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Get current date/time
  // Timestamp (adjusted position based on whether we have percentage)
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  // Use localtime_s for Windows
  std::tm tm;
  localtime_s(&tm, &time_t);

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
    sprintf_s(idleStr, sizeof(idleStr), "%02d:%02d.%03d",
      static_cast<int>(minutes), static_cast<int>(seconds), static_cast<int>(ms));
    idleTimeStr = std::string(idleStr);
  }

  // NEW: Reset operation tracking
  {
    std::lock_guard<std::mutex> lock(m_operationMutex);
    m_currentOperationIndex = 0;
    m_operationInProgress = false;
  }

  try {
    auto sequence = BuildSelectedProcess();
    if (!sequence) {
      UpdateStatus("Failed to build process sequence", true);
      m_processRunning = false;
      return;
    }

    // NEW: Set up operation tracking callback
    sequence->SetOperationCallback([this](size_t index, const std::string& description, bool starting) {
      std::lock_guard<std::mutex> lock(m_operationMutex);
      if (starting) {
        m_currentOperationIndex = index;
        m_operationInProgress = true;

        // Update progress based on operation index
        if (!m_selectedProcessOperations.empty()) {
          m_progress = static_cast<float>(index) / static_cast<float>(m_selectedProcessOperations.size());
        }
      }
    });

    UpdateStatus("Starting process: " + processName);
    // Call the sequence's Execute method - it handles fallbacks internally
    bool processSuccess = sequence->Execute();


    //// Reset operation tracking after completion - IMPORTANT FIX
    //{
    //  std::lock_guard<std::mutex> lock(m_operationMutex);
    //  m_operationInProgress = false;
    //  // Don't leave m_currentOperationIndex at 0 when done
    //  if (processSuccess) {
    //    m_currentOperationIndex = m_selectedProcessOperations.size(); // Set to end
    //  }
    //}

    // NEW: Reset operation tracking after completion
    {
      std::lock_guard<std::mutex> lock(m_operationMutex);
      m_operationInProgress = false;
    }

    // Calculate final duration
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - m_processStartTime);
    // Format duration as mm:ss.xxx
    auto totalMs = duration.count();
    auto minutes = totalMs / 60000;
    auto seconds = (totalMs % 60000) / 1000;
    auto ms = totalMs % 1000;
    char durationStr[32];
    sprintf_s(durationStr, sizeof(durationStr), "%02d:%02d.%03d",
      static_cast<int>(minutes), static_cast<int>(seconds), static_cast<int>(ms));
    if (m_stopRequested) {
      UpdateStatus("Process stopped by user");
      AddCompletedStep(processName, std::string(durationStr), idleTimeStr, false);
    }
    else if (processSuccess) {
      m_progress = 1.0f;  // NEW: Set to 100% on success
      UpdateStatus("Process completed successfully");
      AddCompletedStep(processName, std::string(durationStr), idleTimeStr, true);
    }
    else {
      UpdateStatus("Process failed", true);
      AddCompletedStep(processName, std::string(durationStr), idleTimeStr, false);
    }
    // Always record end time for next idle calculation
    m_lastProcessEndTime = endTime;
    m_hasLastProcessEndTime = true;
  }
  catch (const std::exception& e) {
    // Reset operation tracking on exception
    {
      std::lock_guard<std::mutex> lock(m_operationMutex);
      m_operationInProgress = false;
      m_currentOperationIndex = 0; // Reset on error
    }

    UpdateStatus("Process error: " + std::string(e.what()), true);
    // Track exception as failed process
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - m_processStartTime);
    auto totalMs = duration.count();
    auto minutes = totalMs / 60000;
    auto seconds = (totalMs % 60000) / 1000;
    auto ms = totalMs % 1000;
    char durationStr[32];
    sprintf_s(durationStr, sizeof(durationStr), "%02d:%02d.%03d",
      static_cast<int>(minutes), static_cast<int>(seconds), static_cast<int>(ms));
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

// ENHANCED RenderLiveViewTab() method with crosshair overlay
// Replace the existing RenderLiveViewTab() method in RunPageUI.cpp


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

  // First row: Camera selection and controls
  ImGui::BeginGroup();

  // Camera selection dropdown
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

  ImGui::SetNextItemWidth(150);
  if (ImGui::Combo("##CameraSelection", &currentCameraIndex, cameraNames.data(), (int)cameraNames.size())) {
    SetSelectedCamera(cameraIds[currentCameraIndex]);
  }

  // Crosshair overlay controls
  ImGui::SameLine();
  ImGui::Spacing();
  ImGui::SameLine();
  if (ImGui::Checkbox("Show Crosshair", &m_showCrosshair)) {
    UpdateStatus(m_showCrosshair ? "Crosshair overlay enabled" : "Crosshair overlay disabled");
  }

  ImGui::EndGroup();

  // NEW: Second row: Data channel selection with spec input
  ImGui::BeginGroup();
  ImGui::Text("Data Channel:");

  // Get available data channels from GlobalDataStore
  GlobalDataStore* dataStore = GlobalDataStore::GetInstance();
  std::vector<std::string> dataChannels;

  if (dataStore) {
    dataChannels = dataStore->GetAvailableChannels();
  }

  if (!dataChannels.empty()) {
    // Find current data channel selection index
    int currentDataChannelIndex = 0;
    if (!m_selectedDataChannel.empty()) {
      auto it = std::find(dataChannels.begin(), dataChannels.end(), m_selectedDataChannel);
      if (it != dataChannels.end()) {
        currentDataChannelIndex = static_cast<int>(std::distance(dataChannels.begin(), it));
      }
    }
    else {
      // Select first channel by default
      m_selectedDataChannel = dataChannels[0];
    }

    // Create data channel selection array
    std::vector<const char*> dataChannelNames;
    for (const auto& channel : dataChannels) {
      dataChannelNames.push_back(channel.c_str());
    }

    ImGui::SetNextItemWidth(200);
    if (ImGui::Combo("##DataChannelSelection", &currentDataChannelIndex, dataChannelNames.data(), (int)dataChannelNames.size())) {
      m_selectedDataChannel = dataChannels[currentDataChannelIndex];
      UpdateStatus("Selected data channel: " + m_selectedDataChannel);
    }

    // Show data overlay checkbox
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    if (ImGui::Checkbox("Show Data Overlay", &m_showDataOverlay)) {
      UpdateStatus(m_showDataOverlay ? "Data overlay enabled" : "Data overlay disabled");
    }

    // NEW: Add spec value input on the next line
    if (m_showDataOverlay && !m_selectedDataChannel.empty()) {
      ImGui::Text("Spec Value:");
      ImGui::SameLine();

      // Create unique ID for the input based on channel name
      std::string inputId = "##SpecValue_" + m_selectedDataChannel;

      // Get or initialize spec value for this channel
      if (m_specValues.find(m_selectedDataChannel) == m_specValues.end()) {
        m_specValues[m_selectedDataChannel] = 0.0f;
        m_specValueStrings[m_selectedDataChannel] = "";
      }

      // Use a string buffer for the input
      char specBuffer[64];
      if (m_specValueStrings[m_selectedDataChannel].empty()) {
        strcpy_s(specBuffer, sizeof(specBuffer), "");
      }
      else {
        strncpy_s(specBuffer, sizeof(specBuffer), m_specValueStrings[m_selectedDataChannel].c_str(), _TRUNCATE);
      }

      ImGui::SetNextItemWidth(120);
      if (ImGui::InputText(inputId.c_str(), specBuffer, sizeof(specBuffer), ImGuiInputTextFlags_CharsDecimal)) {
        m_specValueStrings[m_selectedDataChannel] = std::string(specBuffer);

        // Try to parse the value
        if (strlen(specBuffer) > 0) {
          try {
            m_specValues[m_selectedDataChannel] = std::stof(specBuffer);
            m_hasValidSpec[m_selectedDataChannel] = true;
          }
          catch (...) {
            m_hasValidSpec[m_selectedDataChannel] = false;
          }
        }
        else {
          m_hasValidSpec[m_selectedDataChannel] = false;
        }
      }

      // Show clear button
      ImGui::SameLine();
      if (ImGui::Button("Clear##SpecClear")) {
        m_specValueStrings[m_selectedDataChannel] = "";
        m_hasValidSpec[m_selectedDataChannel] = false;
        UpdateStatus("Cleared spec value for: " + m_selectedDataChannel);
      }

      // Show current percentage if spec is valid
      if (m_hasValidSpec[m_selectedDataChannel] && dataStore) {
        float currentValue = dataStore->GetValue(m_selectedDataChannel, 0.0f);
        float specValue = m_specValues[m_selectedDataChannel];

        if (specValue != 0.0f) {
          float percentage = (currentValue / specValue) * 100.0f;
          ImGui::SameLine();
          ImGui::Text("(%.1f%%)", percentage);

          // Color code the percentage based on deviation
          float deviation = std::abs(percentage - 100.0f);
          if (deviation > 10.0f) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "OUT OF SPEC");
          }
          else if (deviation > 5.0f) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "WARNING");
          }
        }
      }
    }
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No data channels available");
    m_selectedDataChannel = "";
    m_showDataOverlay = false;
  }

  ImGui::EndGroup();

  // Camera status and controls
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

  // Calculate canvas size with aspect ratio
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

  // Create camera canvas with overlays
  ImGui::BeginChild("CameraCanvas", canvasSize, true, ImGuiWindowFlags_NoScrollbar);

  // Store canvas position for overlays
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();
  ImVec2 canvasMax = ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);

  // Render camera feed using existing method
  RenderCameraFeedFromSubscriber(canvasSize);

  // Render crosshair overlay if enabled
  if (m_showCrosshair) {
    RenderCrosshairOverlay(canvasPos, canvasSize);
  }

  // NEW: Render data value overlay if enabled
  if (m_showDataOverlay && !m_selectedDataChannel.empty()) {
    RenderDataValueOverlay(canvasPos, canvasSize);
  }

  ImGui::EndChild();

  // Display statistics below the feed
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

    // Show overlay status
    if (m_showCrosshair) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "| Crosshair: ON");
    }

    if (m_showDataOverlay && !m_selectedDataChannel.empty()) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.0f, 0.5f, 1.0f, 1.0f), "| Data: %s", m_selectedDataChannel.c_str());
    }

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

// NEW: Data value overlay rendering method

// ENHANCED: Update the RenderDataValueOverlay method to include percentage
void RunPageUI::RenderDataValueOverlay(const ImVec2& canvasPos, const ImVec2& canvasSize) {
  if (m_selectedDataChannel.empty()) return;

  GlobalDataStore* dataStore = GlobalDataStore::GetInstance();
  if (!dataStore) return;

  // Get current value
  float currentValue = dataStore->GetValue(m_selectedDataChannel, 0.0f);

  // Format value with appropriate units
  char valueText[128];
  FormatDataValueWithUnit(m_selectedDataChannel, currentValue, valueText, sizeof(valueText));

  // Check if we have a valid spec value for percentage calculation
  bool hasSpec = (m_hasValidSpec.find(m_selectedDataChannel) != m_hasValidSpec.end() &&
    m_hasValidSpec[m_selectedDataChannel] &&
    m_specValues[m_selectedDataChannel] != 0.0f);

  float percentage = 0.0f;
  char percentageText[64] = "";

  if (hasSpec) {
    float specValue = m_specValues[m_selectedDataChannel];
    percentage = (currentValue / specValue) * 100.0f;
    snprintf(percentageText, sizeof(percentageText), "%.1f%%", percentage);
  }

  ImDrawList* drawList = ImGui::GetWindowDrawList();

  // Overlay dimensions - make it taller if we have percentage
  const float padding = 10.0f;
  const float overlayWidth = 220.0f;
  const float overlayHeight = hasSpec ? 85.0f : 60.0f; // Taller with percentage

  ImVec2 overlayPos = ImVec2(canvasPos.x + padding, canvasPos.y + padding);
  ImVec2 overlayMax = ImVec2(overlayPos.x + overlayWidth, overlayPos.y + overlayHeight);

  // Background with transparency
  const ImU32 bgColor = IM_COL32(0, 0, 0, 180); // Semi-transparent black
  const ImU32 borderColor = IM_COL32(100, 150, 255, 200); // Light blue border

  drawList->AddRectFilled(overlayPos, overlayMax, bgColor, 5.0f);
  drawList->AddRect(overlayPos, overlayMax, borderColor, 5.0f, 0, 2.0f);

  // Channel name text
  ImVec2 channelTextPos = ImVec2(overlayPos.x + 8, overlayPos.y + 8);
  const ImU32 channelTextColor = IM_COL32(200, 200, 200, 255); // Light gray

  // Use smaller font for channel name
  ImFont* font = ImGui::GetFont();
  float originalScale = font->Scale;
  font->Scale = 0.8f;
  ImGui::PushFont(font);

  drawList->AddText(channelTextPos, channelTextColor, m_selectedDataChannel.c_str());

  ImGui::PopFont();
  font->Scale = originalScale;

  // Value text (larger)
  ImVec2 valueTextPos = ImVec2(overlayPos.x + 8, overlayPos.y + 28);
  const ImU32 valueTextColor = IM_COL32(255, 255, 255, 255); // White

  // Check if value changed for highlighting
  bool valueChanged = HasDataValueChanged(m_selectedDataChannel, currentValue);
  const ImU32 highlightColor = IM_COL32(0, 255, 100, 255); // Bright green for changes

  // Use larger font for value
  font->Scale = 1.2f;
  ImGui::PushFont(font);

  drawList->AddText(valueTextPos, valueChanged ? highlightColor : valueTextColor, valueText);

  ImGui::PopFont();
  font->Scale = originalScale;

  // NEW: Percentage text if we have spec
  if (hasSpec) {
    ImVec2 percentageTextPos = ImVec2(overlayPos.x + 8, overlayPos.y + 50);

    // Color code based on deviation from 100%
    float deviation = std::abs(percentage - 100.0f);
    ImU32 percentageColor;

    if (deviation > 10.0f) {
      percentageColor = IM_COL32(255, 100, 100, 255); // Red for large deviation
    }
    else if (deviation > 5.0f) {
      percentageColor = IM_COL32(255, 200, 0, 255);   // Yellow for moderate deviation
    }
    else {
      percentageColor = IM_COL32(100, 255, 100, 255); // Green for good values
    }

    // Use medium font for percentage
    font->Scale = 1.1f;
    ImGui::PushFont(font);

    drawList->AddText(percentageTextPos, percentageColor, percentageText);

    ImGui::PopFont();
    font->Scale = originalScale;
  }

  // Timestamp (adjusted position based on whether we have percentage)
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  // Use localtime_s for Windows
  std::tm tm;
  localtime_s(&tm, &time_t);

  char timeStr[32];
  std::strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &tm);

  ImVec2 timeTextPos = ImVec2(overlayPos.x + overlayWidth - 60, overlayPos.y + (hasSpec ? 67 : 45));
  const ImU32 timeTextColor = IM_COL32(150, 150, 150, 200); // Dim gray

  font->Scale = 0.7f;
  ImGui::PushFont(font);

  drawList->AddText(timeTextPos, timeTextColor, timeStr);

  ImGui::PopFont();
  font->Scale = originalScale;
}


// NEW: Format data value with units (adapted from GlobalDataStoreViewerUI)
void RunPageUI::FormatDataValueWithUnit(const std::string& channel, float value, char* buffer, size_t bufferSize) {
  float absValue = std::abs(value);

  // Try to determine unit from channel name or use generic formatting
  std::string lowerChannel = channel;
  std::transform(lowerChannel.begin(), lowerChannel.end(), lowerChannel.begin(), ::tolower);

  if (lowerChannel.find("current") != std::string::npos || lowerChannel.find("amp") != std::string::npos || lowerChannel.back() == 'a') {
    // Current formatting
    if (absValue == 0.0f) {
      snprintf(buffer, bufferSize, "0.00 A");
    }
    else if (absValue < 1e-12f) {
      snprintf(buffer, bufferSize, "%.3e A", value);
    }
    else if (absValue < 1e-9f) {
      float pAValue = value * 1e12f;
      snprintf(buffer, bufferSize, "%.2f pA", pAValue);
    }
    else if (absValue < 1e-6f) {
      float nAValue = value * 1e9f;
      snprintf(buffer, bufferSize, "%.2f nA", nAValue);
    }
    else if (absValue < 1e-3f) {
      float uAValue = value * 1e6f;
      snprintf(buffer, bufferSize, "%.2f µA", uAValue);
    }
    else if (absValue < 1.0f) {
      float mAValue = value * 1e3f;
      snprintf(buffer, bufferSize, "%.3f mA", mAValue);
    }
    else {
      snprintf(buffer, bufferSize, "%.3f A", value);
    }
  }
  else if (lowerChannel.find("voltage") != std::string::npos || lowerChannel.find("volt") != std::string::npos || lowerChannel.back() == 'v') {
    // Voltage formatting
    if (absValue == 0.0f) {
      snprintf(buffer, bufferSize, "0.00 V");
    }
    else if (absValue < 1e-6f) {
      float uVValue = value * 1e6f;
      snprintf(buffer, bufferSize, "%.2f µV", uVValue);
    }
    else if (absValue < 1e-3f) {
      float mVValue = value * 1e3f;
      snprintf(buffer, bufferSize, "%.2f mV", mVValue);
    }
    else {
      snprintf(buffer, bufferSize, "%.3f V", value);
    }
  }
  else if (lowerChannel.find("temp") != std::string::npos) {
    // Temperature formatting
    snprintf(buffer, bufferSize, "%.1f °C", value);
  }
  else if (lowerChannel.find("pressure") != std::string::npos) {
    // Pressure formatting
    snprintf(buffer, bufferSize, "%.2f Pa", value);
  }
  else {
    // Generic formatting
    if (absValue == 0.0f) {
      snprintf(buffer, bufferSize, "0.00");
    }
    else if (absValue < 1e-6f) {
      snprintf(buffer, bufferSize, "%.3e", value);
    }
    else if (absValue < 0.001f) {
      snprintf(buffer, bufferSize, "%.6f", value);
    }
    else {
      snprintf(buffer, bufferSize, "%.3f", value);
    }
  }
}

// NEW: Check if data value changed (for highlighting)
bool RunPageUI::HasDataValueChanged(const std::string& channel, float currentValue) {
  bool valueChanged = false;

  if (m_lastDataValues.find(channel) != m_lastDataValues.end()) {
    valueChanged = (std::abs(m_lastDataValues[channel] - currentValue) > 1e-9f); // Small threshold for float comparison
  }
  else {
    valueChanged = true; // First time seeing this channel
  }

  m_lastDataValues[channel] = currentValue;

  // Reset highlight after a short time
  static auto lastChangeTime = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastChangeTime).count();

  if (valueChanged) {
    lastChangeTime = now;
    return true;
  }

  // Show highlight for 500ms after change
  return elapsed < 500;
}


// NEW: Crosshair overlay rendering method
void RunPageUI::RenderCrosshairOverlay(const ImVec2& canvasPos, const ImVec2& canvasSize) {
  ImDrawList* drawList = ImGui::GetWindowDrawList();

  // Calculate center point
  ImVec2 center = ImVec2(
    canvasPos.x + canvasSize.x * 0.5f,
    canvasPos.y + canvasSize.y * 0.5f
  );

  // Crosshair parameters
  const float crosshairLength = 20.0f;
  const float crosshairThickness = 2.0f;
  const ImU32 crosshairColor = IM_COL32(0, 255, 0, 200); // Green with transparency
  const ImU32 crosshairOutlineColor = IM_COL32(0, 0, 0, 150); // Black outline

  // Draw crosshair outline (black) for better visibility
  // Horizontal line outline
  drawList->AddLine(
    ImVec2(center.x - crosshairLength - 1, center.y - 1),
    ImVec2(center.x + crosshairLength + 1, center.y + 1),
    crosshairOutlineColor,
    crosshairThickness + 2.0f
  );

  // Vertical line outline
  drawList->AddLine(
    ImVec2(center.x - 1, center.y - crosshairLength - 1),
    ImVec2(center.x + 1, center.y + crosshairLength + 1),
    crosshairOutlineColor,
    crosshairThickness + 2.0f
  );

  // Draw main crosshair (green)
  // Horizontal line
  drawList->AddLine(
    ImVec2(center.x - crosshairLength, center.y),
    ImVec2(center.x + crosshairLength, center.y),
    crosshairColor,
    crosshairThickness
  );

  // Vertical line
  drawList->AddLine(
    ImVec2(center.x, center.y - crosshairLength),
    ImVec2(center.x, center.y + crosshairLength),
    crosshairColor,
    crosshairThickness
  );

  // Optional: Add center dot
  drawList->AddCircleFilled(center, 2.0f, crosshairColor);

  // Optional: Add corner markers for better visibility
  const float cornerOffset = 40.0f;
  const float cornerLength = 8.0f;
  const ImU32 cornerColor = IM_COL32(255, 255, 0, 150); // Yellow corners

  // Top-left corner
  drawList->AddLine(
    ImVec2(center.x - cornerOffset, center.y - cornerOffset),
    ImVec2(center.x - cornerOffset + cornerLength, center.y - cornerOffset),
    cornerColor, 1.5f
  );
  drawList->AddLine(
    ImVec2(center.x - cornerOffset, center.y - cornerOffset),
    ImVec2(center.x - cornerOffset, center.y - cornerOffset + cornerLength),
    cornerColor, 1.5f
  );

  // Top-right corner
  drawList->AddLine(
    ImVec2(center.x + cornerOffset, center.y - cornerOffset),
    ImVec2(center.x + cornerOffset - cornerLength, center.y - cornerOffset),
    cornerColor, 1.5f
  );
  drawList->AddLine(
    ImVec2(center.x + cornerOffset, center.y - cornerOffset),
    ImVec2(center.x + cornerOffset, center.y - cornerOffset + cornerLength),
    cornerColor, 1.5f
  );

  // Bottom-left corner
  drawList->AddLine(
    ImVec2(center.x - cornerOffset, center.y + cornerOffset),
    ImVec2(center.x - cornerOffset + cornerLength, center.y + cornerOffset),
    cornerColor, 1.5f
  );
  drawList->AddLine(
    ImVec2(center.x - cornerOffset, center.y + cornerOffset),
    ImVec2(center.x - cornerOffset, center.y + cornerOffset - cornerLength),
    cornerColor, 1.5f
  );

  // Bottom-right corner
  drawList->AddLine(
    ImVec2(center.x + cornerOffset, center.y + cornerOffset),
    ImVec2(center.x + cornerOffset - cornerLength, center.y + cornerOffset),
    cornerColor, 1.5f
  );
  drawList->AddLine(
    ImVec2(center.x + cornerOffset, center.y + cornerOffset),
    ImVec2(center.x + cornerOffset, center.y + cornerOffset - cornerLength),
    cornerColor, 1.5f
  );
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
  // Direct embedded operations display - no separate UI class needed
  auto resultsManager = m_machineOps.GetResultsManager();
  if (!resultsManager) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Results Manager not available");
    ImGui::Text("Operation results display is not initialized.");
    return;
  }

  // Get operations data
  static std::vector<OperationResult> operations;
  static std::chrono::steady_clock::time_point lastRefresh;
  static constexpr std::chrono::milliseconds refreshInterval{ 1000 }; // 1 second
  static int selectedOperationIndex = -1;
  static std::string selectedOperationId;
  static OperationResult selectedOperation;

  // Check if we need to refresh
  auto now = std::chrono::steady_clock::now();
  if ((now - lastRefresh) >= refreshInterval) {
    try {
      operations = resultsManager->GetOperationHistory(50); // Get last 50 operations
      lastRefresh = now;
    }
    catch (const std::exception& e) {
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error loading operations:");
      ImGui::Text("%s", e.what());
      return;
    }
  }

  // Header
  ImGui::Text("Operation Results - %d operations", static_cast<int>(operations.size()));
  ImGui::Separator();

  // Split view: Operations list on left, details on right
  float availableWidth = ImGui::GetContentRegionAvail().x;
  float leftPanelWidth = availableWidth * 0.6f; // 60% for list

  // Left panel - Operations list
  ImGui::BeginChild("OperationsList", ImVec2(leftPanelWidth, 0), true);
  {
    ImGui::Text("Operations List");
    ImGui::Separator();

    // Operations table
    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
      ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders;

    if (ImGui::BeginTable("OperationsTable", 5, flags)) {
      ImGui::TableSetupColumn("Select", ImGuiTableColumnFlags_WidthFixed, 50.0f);
      ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 60.0f);
      ImGui::TableSetupColumn("Method", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Device", ImGuiTableColumnFlags_WidthFixed, 80.0f);
      ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, 80.0f);
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableHeadersRow();

      // Render operations
      for (int i = 0; i < static_cast<int>(operations.size()); i++) {
        const auto& op = operations[i];

        ImGui::TableNextRow();

        // Select column with radio button
        ImGui::TableNextColumn();
        bool isSelected = (selectedOperationIndex == i);
        if (ImGui::RadioButton(("##select_" + std::to_string(i)).c_str(), isSelected)) {
          selectedOperationIndex = i;
          selectedOperationId = op.operationId;
          selectedOperation = op;
        }

        // Status column with color coding
        ImGui::TableNextColumn();
        ImVec4 statusColor;
        const char* statusIcon;

        if (op.status == "success" || op.status == "completed") {
          statusColor = ImVec4(0.0f, 0.8f, 0.0f, 1.0f); // Green
          statusIcon = "✓";
        }
        else if (op.status == "failed" || op.status == "error") {
          statusColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); // Red
          statusIcon = "✗";
        }
        else if (op.status == "running") {
          statusColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
          statusIcon = "⟳";
        }
        else {
          statusColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Gray
          statusIcon = "?";
        }

        ImGui::TextColored(statusColor, "%s", statusIcon);
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", op.status.c_str());
        }

        // Method column
        ImGui::TableNextColumn();
        ImGui::Text("%s", op.methodName.c_str());

        // Device column
        ImGui::TableNextColumn();
        ImGui::Text("%s", op.deviceName.c_str());

        // Duration column
        ImGui::TableNextColumn();
        if (op.elapsedTimeMs > 0) {
          // Format duration
          int64_t seconds = op.elapsedTimeMs / 1000;
          int64_t minutes = seconds / 60;
          seconds = seconds % 60;

          if (minutes > 0) {
            ImGui::Text("%lldm %llds", minutes, seconds);
          }
          else {
            ImGui::Text("%llds", seconds);
          }
        }
        else if (op.status == "running") {
          ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Running...");
        }
        else {
          ImGui::Text("-");
        }
      }

      ImGui::EndTable();
    }
  }
  ImGui::EndChild();

  ImGui::SameLine();

  // Right panel - Operation details
  ImGui::BeginChild("OperationDetails", ImVec2(0, 0), true);
  {
    ImGui::Text("Operation Details");
    ImGui::Separator();

    if (selectedOperationIndex >= 0 && selectedOperationIndex < static_cast<int>(operations.size())) {
      const auto& op = selectedOperation;

      // Operation metadata
      ImGui::Text("Operation ID: %s", op.operationId.c_str());
      ImGui::Text("Method: %s", op.methodName.c_str());
      ImGui::Text("Device: %s", op.deviceName.c_str());

      ImGui::Text("Status: ");
      ImGui::SameLine();
      ImVec4 statusColor;
      if (op.status == "success" || op.status == "completed") {
        statusColor = ImVec4(0.0f, 0.8f, 0.0f, 1.0f);
      }
      else if (op.status == "failed" || op.status == "error") {
        statusColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
      }
      else if (op.status == "running") {
        statusColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
      }
      else {
        statusColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
      }
      ImGui::TextColored(statusColor, "%s", op.status.c_str());

      if (!op.callerContext.empty()) {
        ImGui::Text("Caller: %s", op.callerContext.c_str());
      }

      if (!op.sequenceName.empty()) {
        ImGui::Text("Sequence: %s", op.sequenceName.c_str());
      }

      // Timing information
      ImGui::Separator();
      ImGui::Text("Timing Information:");

      // Format timestamp
      auto time_t = std::chrono::system_clock::to_time_t(op.timestamp);

      // Use localtime_s for Windows
      std::tm timeinfo;
      localtime_s(&timeinfo, &time_t);

      std::stringstream timeStr;
      timeStr << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
      ImGui::Text("Start Time: %s", timeStr.str().c_str());

      if (op.elapsedTimeMs > 0) {
        int64_t totalSeconds = op.elapsedTimeMs / 1000;
        int64_t minutes = totalSeconds / 60;
        int64_t seconds = totalSeconds % 60;
        int64_t milliseconds = op.elapsedTimeMs % 1000;

        if (minutes > 0) {
          ImGui::Text("Duration: %lldm %llds %lldms", minutes, seconds, milliseconds);
        }
        else {
          ImGui::Text("Duration: %llds %lldms", seconds, milliseconds);
        }
      }

      // Error message if failed
      if (op.status == "failed" || op.status == "error") {
        auto errorIt = op.data.find("error_message");
        if (errorIt != op.data.end() && !errorIt->second.empty()) {
          ImGui::Separator();
          ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Error Message:");
          ImGui::TextWrapped("%s", errorIt->second.c_str());
        }
      }

      // Parameters & Results
      if (!op.data.empty()) {
        ImGui::Separator();
        ImGui::Text("Parameters & Results:");

        if (ImGui::BeginTable("ResultsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
          ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 150.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
          ImGui::TableHeadersRow();

          for (const auto& [key, value] : op.data) {
            // Skip error_message as it's shown above
            if (key == "error_message") continue;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            // Color-code parameter vs result keys
            if (key.find("param_") == 0) {
              ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "%s", key.c_str());
            }
            else {
              ImGui::Text("%s", key.c_str());
            }

            ImGui::TableNextColumn();
            ImGui::Text("%s", value.c_str());
          }

          ImGui::EndTable();
        }
      }
    }
    else {
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
        "Select an operation from the list to view details");
    }
  }
  ImGui::EndChild();
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
// Timestamp (adjusted position based on whether we have percentage)
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  // Use localtime_s for Windows
  std::tm tm;
  localtime_s(&tm, &time_t);

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


// Also fix the RunPageUI version for consistency
void RunPageUI::SetSelectedCamera(const std::string& cameraId) {
  if (m_selectedCameraId == cameraId) {
    return; // No change
  }

  std::string previousCamera = m_selectedCameraId;
  m_selectedCameraId = cameraId;

  if (m_cameraSubscriber) {
    // CRITICAL FIX: Re-register subscriber when changing cameras
    std::string oldId = m_cameraSubscriber->GetSubscriberId();
    m_logger->LogInfo("RunPageUI: Re-registering subscriber - Old ID: " + oldId);

    // 1. Unsubscribe with old ID
    m_cameraManager->UnsubscribeFromFrames(oldId);
    m_logger->LogInfo("RunPageUI: Unsubscribed old subscriber ID: " + oldId);

    // 2. Update subscriber target camera (this changes the internal ID)
    m_cameraSubscriber->SetTargetCamera(cameraId);
    std::string newId = m_cameraSubscriber->GetSubscriberId();
    m_logger->LogInfo("RunPageUI: Subscriber target updated - New ID: " + newId);

    // 3. Re-subscribe with new ID
    m_cameraManager->SubscribeToFrames(m_cameraSubscriber);
    m_logger->LogInfo("RunPageUI: Re-subscribed with new ID: " + newId);
    m_logger->LogInfo("RunPageUI: Camera switched from " + previousCamera + " to " + cameraId);
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