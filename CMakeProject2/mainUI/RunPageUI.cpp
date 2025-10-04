#include "RunPageUI.h"
#include "imgui.h"
#include <chrono>
#include <iostream>
#include <algorithm>
#include "uaa3_process_builders.h"
#include "ProcessRegistry.h"
#include "LiveVideoSubscriber.h"
#include "ManualAdjustmentOperation.h"
#include <filesystem>

using namespace UAA3ProcessBuilders;
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

  // Initialize spec value from database
  LoadSpecFromDatabase();

  m_logger->LogInfo("RunPageUI: Spec value loaded from database");

  // Initialize settings editor with callback
  m_settingsEditor = std::make_unique<SettingsEditorUI>();
  m_settingsEditor->SetOnSettingsChangedCallback([this]() {
    RefreshSettingsFromDatabase();
  });
  m_logger->LogInfo("RunPageUI: Settings editor callback configured");

  // Initialize filter manager
  m_filterManager = std::make_unique<ProcessFilterManager>();

  // Set up callback for when filters change
  m_filterManager->SetOnFilterChangedCallback([this]() {
    OnFilterChanged();
  });

  // Initialize operations display UI for detail results tab
  m_operationsDisplayUI = std::make_unique<OperationsDisplayUI>(machineOps);

  m_logger->LogInfo("RunPageUI: Initialized with process filtering and operations display support");

  // Initialize embedded jog control
  if (auto* globalMotion = m_machineOps.GetGlobalMotionController()) {
    m_jogControl = std::make_unique<EmbeddedJogControl>(*globalMotion);
    m_jogControl->SetCompactMode(false);
    m_jogControl->SetShowPositionDisplay(true);
    m_jogControl->SetShowTransformMatrix(true);

    m_jogControl->SetStatusCallback([this](const std::string& msg) {
      UpdateStatus("Jog: " + msg);
    });

    m_logger->LogInfo("RunPageUI: Embedded jog control initialized");
  }
  else {
    m_logger->LogWarning("RunPageUI: GlobalMotionController not available");
  }



  // Add this after jog control initialization

// Initialize process configuration system
  m_processConfigUI = std::make_unique<ProcessConfigUI>();
  m_currentProcessConfig = ProcessConfigBuilders::createPickPlaceConfig();

  // Try to load last saved configuration
  if (std::filesystem::exists("last_process_config.json")) {
    m_currentProcessConfig.loadFromFile("last_process_config.json");
    m_processConfigUI->setConfiguration(m_currentProcessConfig);
    m_logger->LogInfo("RunPageUI: Loaded last process configuration");
  }

  m_logger->LogInfo("RunPageUI: Process configuration system initialized");


  // Set custom directory for captured images (optional)
  auto& context = AppContext::GetInstance();
  auto* cameraManager = context.GetCameraManager();
  if (cameraManager) {
    cameraManager->SetImageOutputDirectory("captures");
  }

  m_settingsEditor = std::make_unique<SettingsEditorUI>();
}

RunPageUI::~RunPageUI() {
  StopProcess();
  CleanupCameraTexture();
  ClearCameraFeed();
  ClearEmbeddedCameraFeed(); // NEW: Clear embedded feed
  m_logger->LogInfo("RunPageUI: Destroyed");
}


void RunPageUI::RenderUI() {
  // Add this line at the beginning of RenderUI to monitor frame flow
  //DebugCameraFrameFlow();
  if (m_settingsEditor && m_settingsEditor->IsVisible()) {
    m_settingsEditor->Render();
  }

  // Get available content region
  ImVec2 contentRegion = ImGui::GetContentRegionAvail();

  // Calculate column widths (25%, 25%, 50%)
  float col1Width = contentRegion.x * 0.20f;
  float col2Width = contentRegion.x * 0.30f;
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

  // NEW: Recipe load dialog
  ShowRecipeLoadDialog();


  if (m_showFilterWindow) {
    m_filterManager->RenderFilterWindow(&m_showFilterWindow);
  }

  // ADD THIS: Render any active manual adjustment windows
  ManualAdjustmentRegistry::GetInstance().RenderAll();
}


// In RunPageUI.cpp, update RenderColumn1:
// In RunPageUI.cpp - Update RenderColumn1()
void RunPageUI::RenderColumn1() {
  ImGui::Separator();

  // === NEW: Recipe Mode Indicator ===
  if (m_usingRecipe) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.5f, 1.0f));
    ImGui::Text("Recipe Mode: %s", m_loadedRecipe.name.c_str());
    ImGui::PopStyleColor();

    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) {
      m_usingRecipe = false;
      m_selectedProcess = "";
      UpdateStatus("Switched to registry mode");
    }
    ImGui::Separator();
  }

  // === NEW: Recipe Load Button ===
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.0f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.5f, 0.1f, 1.0f));
  if (ImGui::Button("Load Recipe", ImVec2(-1, 30))) {
    m_showRecipeLoadDialog = true;
  }
  ImGui::PopStyleColor(2);

  ImGui::Separator();

  // Existing buttons...
  if (ImGui::Button("Edit Me", ImVec2(-1, 30))) {
    m_settingsEditor->Show();
    UpdateStatus("Opened " + m_settingsEditor->GetName());
  }

  ImGui::Text(reinterpret_cast<const char*>(u8"🔧 Process Control"));
  ImGui::Separator();

  // Rest of existing code...
  RenderControlButtons();
  ImGui::Spacing();
  ImGui::Separator();
  RenderRunningStatus();
  ImGui::Spacing();
  ImGui::Separator();

  ImGui::Checkbox("Automatic Start", &m_autoStartOnSelect);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("When checked, selecting a process will automatically start it");
  }

  ImGui::Separator();
  ImGui::Spacing();

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



// 1. Update RenderProcessTreeView() method to sync config when selecting a configurable process:

void RunPageUI::RenderProcessTreeView() {
  ImGui::Text("Process Steps");
  ImGui::Separator();

  // Create a scrollable region
  ImGui::BeginChild("ProcessTree", ImVec2(0, 0), true,
    ImGuiWindowFlags_HorizontalScrollbar);

  auto sortedList = GetSortedProcessList();

  // Make items taller
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 10));
  ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));

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
    if (ImGui::Selectable(displayName.c_str(), isSelected, 0, ImVec2(0, 35))) {
      m_selectedProcess = process;
      UpdateStatus("Selected: " + process);
      ExtractSelectedProcessOperations();

      // NEW: Sync with ProcessConfigUI if it's a configurable process
      if (process.find("_Configurable") != std::string::npos && m_processConfigUI) {
        m_processConfigUI->setCurrentProcess(process);
        m_logger->LogInfo("Synced ProcessConfigUI with: " + process);
      }

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



void RunPageUI::RenderColumn2() {
  ImGui::Text(reinterpret_cast<const char*>(u8"📊 Status & Controls"));
  ImGui::Separator();

  if (ImGui::BeginTabBar("Column2Tabs")) {
    if (ImGui::BeginTabItem("Sequence")) {
      RenderSequenceBreakdownTab();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Config")) {
      RenderProcessConfigTab();
      ImGui::EndTabItem();
    }

    // REMOVE or comment out this tab - no longer needed
    /*
    if (ImGui::BeginTabItem("Configurable")) {
      RenderConfigurableTab();
      ImGui::EndTabItem();
    }
    */

    if (ImGui::BeginTabItem("Status")) {
      RenderStatusTabCol2();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Jog")) {
      RenderJogControlTab();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Action")) {
      RenderActionTab();
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
    //ClearCompletedSteps();
    std::cout << "Clear completed steps button clicked" << std::endl;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_completedSteps.clear();
		}


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
    int delaySeconds = static_cast<int>(m_promptUI->GetAutoConfirmDelay());
    if (ImGui::SliderInt("Auto-confirm delay", &delaySeconds, 1, 10, "%d sec")) {
      m_promptUI->SetAutoConfirmDelay(static_cast<float>(delaySeconds));
      UpdateStatus("Auto-confirm delay set to " + std::to_string(delaySeconds) + " seconds");
    }
  }
}

// New placeholder function for jog control

// RenderJogControlTab remains the same
void RunPageUI::RenderJogControlTab() {
  if (m_jogControl) {
    m_jogControl->Render();
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
      "Jog Control Not Available");
    ImGui::Spacing();
    ImGui::TextWrapped("Global motion controller is not initialized.");

    // Try to initialize on demand
    if (ImGui::Button("Initialize Motion Control")) {
      m_machineOps.InitializeGlobalMotion();
      if (auto* globalMotion = m_machineOps.GetGlobalMotionController()) {
        m_jogControl = std::make_unique<EmbeddedJogControl>(*globalMotion);
        m_jogControl->SetCompactMode(false);
        m_jogControl->SetShowPositionDisplay(true);
        m_jogControl->SetStatusCallback([this](const std::string& msg) {
          UpdateStatus("Jog: " + msg);
        });
        UpdateStatus("Jog control initialized");
      }
    }
  }
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



// Replace the existing GetCurrentProcessList() method
std::vector<std::string> RunPageUI::GetCurrentProcessList() const {
  // NEW: If using recipe, return recipe instances
  if (m_usingRecipe) {
    return GetRecipeInstanceDisplayNames();
  }

  // Original filter-based logic
  auto allProcesses = ProcessRegistry::GetInstance().GetAllProcessNames();

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

  return allProcesses;
}


// Update GetSortedProcessList() to handle recipes
std::vector<std::string> RunPageUI::GetSortedProcessList() const {
  // NEW: If using recipe, return instances in recipe order
  if (m_usingRecipe) {
    return GetRecipeInstanceDisplayNames();
  }

  // Original filter manager logic
  if (m_filterManager) {
    return m_filterManager->GetSortedFilteredProcessList();
  }

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



void RunPageUI::RenderColumn3() {
  // Get available space
  ImVec2 availableRegion = ImGui::GetContentRegionAvail();

  // === TOP SECTION: EXPANDABLE PANELS ===
  float normalPanelHeight = 250.0f;
  float spacing = 10.0f;

  // Camera aspect ratio (1280x1024)
  const float CAMERA_ASPECT_RATIO = 1280.0f / 1024.0f;

  // Calculate panel dimensions based on current state
  float panel1Width, panel2Width, panel3Width;
  float panelHeight = normalPanelHeight;
  bool showPanel1 = true, showPanel2 = true, showPanel3 = true;

  switch (m_panelState) {
  case PanelState::Normal:
    // All panels equal width
    panel1Width = (availableRegion.x - spacing * 2) / 3.0f;
    panel2Width = panel1Width;
    panel3Width = panel1Width;
    panelHeight = normalPanelHeight;
    break;

  case PanelState::Panel1:
    // Panel 1 takes full width - calculate height based on camera aspect ratio
    panel1Width = availableRegion.x;
    // Calculate height to maintain aspect ratio for full width
    panelHeight = (panel1Width - 10) / CAMERA_ASPECT_RATIO + 35; // Account for padding and header
    // Cap the height to available space if needed
    if (panelHeight > availableRegion.y * 0.6f) {
      panelHeight = availableRegion.y * 0.6f; // Don't take more than 60% of vertical space
    }
    showPanel2 = false;
    showPanel3 = false;
    break;

  case PanelState::Panel2:
    // Panel 2 takes full width
    panel2Width = availableRegion.x;
    panelHeight = 400.0f; // Fixed height for non-camera panels
    showPanel1 = false;
    showPanel3 = false;
    break;

  case PanelState::Panel3:
    // Panel 3 takes full width
    panel3Width = availableRegion.x;
    panelHeight = 400.0f; // Fixed height for non-camera panels
    showPanel1 = false;
    showPanel2 = false;
    break;
  }

  // Render Panel 1 if visible
  if (showPanel1) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
    ImGui::BeginChild("Panel1_Camera", ImVec2(panel1Width, panelHeight), true);

    // Make header clickable
    ImVec2 headerPos = ImGui::GetCursorPos();
    ImGui::Text("Live Camera");

    // Invisible button over header for click detection
    ImVec2 headerSize = ImVec2(panel1Width - 10, 20);
    ImGui::SetCursorPos(headerPos);
    if (ImGui::InvisibleButton("Panel1_Header", headerSize)) {
      HandlePanelClick(1);
    }

    // Show expand/collapse icon
    ImGui::SameLine(panel1Width - 30);
    if (m_panelState == PanelState::Panel1) {
      ImGui::Text("[−]"); // Collapse icon
    }
    else if (m_panelState == PanelState::Normal) {
      ImGui::Text("[+]"); // Expand icon
    }

    ImGui::Separator();

    // Render panel content - use all available space
    ImVec2 contentSize = ImVec2(panel1Width - 10, panelHeight - 35);
    RenderPanelContent(1, contentSize);

    ImGui::EndChild();
    ImGui::PopStyleVar();

    if (m_panelState == PanelState::Normal) {
      ImGui::SameLine();
    }
  }

  // Render Panel 2 if visible
  if (showPanel2) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
    ImGui::BeginChild("Panel2_Blank", ImVec2(panel2Width, panelHeight), true);

    // Make header clickable
    ImVec2 headerPos = ImGui::GetCursorPos();
    ImGui::Text("Panel 2");

    // Invisible button over header
    ImVec2 headerSize = ImVec2(panel2Width - 10, 20);
    ImGui::SetCursorPos(headerPos);
    if (ImGui::InvisibleButton("Panel2_Header", headerSize)) {
      HandlePanelClick(2);
    }

    // Show expand/collapse icon
    ImGui::SameLine(panel2Width - 30);
    if (m_panelState == PanelState::Panel2) {
      ImGui::Text("[−]");
    }
    else if (m_panelState == PanelState::Normal) {
      ImGui::Text("[+]");
    }

    ImGui::Separator();

    // Render panel content
    ImVec2 contentSize = ImVec2(panel2Width - 10, panelHeight - 35);
    RenderPanelContent(2, contentSize);

    ImGui::EndChild();
    ImGui::PopStyleVar();

    if (m_panelState == PanelState::Normal) {
      ImGui::SameLine();
    }
  }

  // Render Panel 3 if visible
  if (showPanel3) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
    ImGui::BeginChild("Panel3_Blank", ImVec2(panel3Width, panelHeight), true);

    // Make header clickable
    ImVec2 headerPos = ImGui::GetCursorPos();
    ImGui::Text("Panel 3");

    // Invisible button over header
    ImVec2 headerSize = ImVec2(panel3Width - 10, 20);
    ImGui::SetCursorPos(headerPos);
    if (ImGui::InvisibleButton("Panel3_Header", headerSize)) {
      HandlePanelClick(3);
    }

    // Show expand/collapse icon
    ImGui::SameLine(panel3Width - 30);
    if (m_panelState == PanelState::Panel3) {
      ImGui::Text("[−]");
    }
    else if (m_panelState == PanelState::Normal) {
      ImGui::Text("[+]");
    }

    ImGui::Separator();

    // Render panel content
    ImVec2 contentSize = ImVec2(panel3Width - 10, panelHeight - 35);
    RenderPanelContent(3, contentSize);

    ImGui::EndChild();
    ImGui::PopStyleVar();
  }

  // === SPACING ===
  ImGui::Spacing();
  ImGui::Spacing();

  // === UPDATED LIVE DATA PLOT ROW WITH SPEC CONTROL ===
  float plotHeight = 250.0f;  // Increased from 200px to 250px
  ImGui::BeginChild("LiveDataPlotRow", ImVec2(-1, plotHeight), true);
  {
    ImGui::Text("Live Data Analysis");  // Changed title
    ImGui::Separator();

    // Get available width and split 50/50
    float availWidth = ImGui::GetContentRegionAvail().x;
    float leftWidth = availWidth * 0.5f - 5.0f;   // 50% minus padding
    float rightWidth = availWidth * 0.5f - 5.0f;  // 50% minus padding

    // === LEFT COLUMN: SPEC CONTROL ===
    ImGui::BeginChild("SpecControl", ImVec2(leftWidth, -1), true);
    {
      RenderSpecControl();  // New method for spec control
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // === RIGHT COLUMN: LIVE PLOT ===
    ImGui::BeginChild("LivePlot", ImVec2(rightWidth, -1), true);
    {
      RenderLivePlot();  // New method for plot rendering
    }
    ImGui::EndChild();
  }
  ImGui::EndChild();

  // === TAB BAR (UNCHANGED) ===
  ImGui::Spacing();
  if (ImGui::BeginTabBar("Column3Tabs", ImGuiTabBarFlags_None)) {
    if (ImGui::BeginTabItem("Status")) {
      RenderStatusTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Detail Results")) {
      RenderDetailResultsTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Live View")) {
      RenderLiveViewTab();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}
// RunPageUI.cpp - Add this new method

ImVec4 RunPageUI::GetPercentageColor(float percentage) {
  if (percentage >= m_exceptionalThreshold) {
    // Bright yellow-green for exceptional
    return ImVec4(0.99f, 0.91f, 0.15f, 1.0f);
  }
  else if (percentage >= m_passThreshold) {
    // Green for pass
    return ImVec4(0.13f, 0.82f, 0.52f, 1.0f);
  }
  else if (percentage >= m_failThreshold) {
    // Teal for above fail threshold
    return ImVec4(0.12f, 0.65f, 0.61f, 1.0f);
  }
  else {
    // Dark purple-blue for below fail threshold
    return ImVec4(0.27f, 0.0f, 0.33f, 1.0f);
  }
}

void RunPageUI::RenderSpecControl() {
  ImGui::Text("Spec Threshold Control");
  ImGui::Separator();

  // === LIVE COMPARISON AT TOP (MOST IMPORTANT) ===
  if (m_specEnabled) {
    // Get current value from plot
    float currentValue = 0.0f;
    if (m_liveDataPlot && m_plotInitialized) {
      currentValue = m_liveDataPlot->GetCurrentValue();
    }

    // Calculate percentage
    float percentage = 0.0f;
    if (m_specThreshold != 0) {
      percentage = (currentValue / m_specThreshold) * 100.0f;
    }

    // Display percentage and status in LARGE text
    ImVec4 percentColor = GetPercentageColor(percentage);
    const char* status = GetStatusText(percentage);

    // Push larger font scale for emphasis
    ImGui::PushStyleColor(ImGuiCol_Text, percentColor);
    float oldScale = ImGui::GetFont()->Scale;
    ImGui::GetFont()->Scale *= 1.8f;  // 80% larger for better visibility
    ImGui::PushFont(ImGui::GetFont());

    ImGui::Text("%.1f%% %s", percentage, status);

    ImGui::GetFont()->Scale = oldScale;
    ImGui::PopFont();
    ImGui::PopStyleColor();

    // Visual progress bar with Viridis gradient
    RenderComparisonBar(percentage);

    ImGui::Spacing();

    // Current vs Target values - slightly larger
    char valueBuffer[64];
    FormatCurrentValue(currentValue, valueBuffer, sizeof(valueBuffer));

    char specBuffer[64];
    FormatCurrentValue(m_specThreshold, specBuffer, sizeof(specBuffer));

    // Slightly larger font for values
    ImGui::GetFont()->Scale *= 1.2f;  // 20% larger
    ImGui::PushFont(ImGui::GetFont());

    ImGui::Text("Current: %s", valueBuffer);
    ImGui::Text("Target: %s (100%%)", specBuffer);

    ImGui::GetFont()->Scale = oldScale;
    ImGui::PopFont();
  }
  else {
    // Show disabled state
    ImGui::TextDisabled("Comparison Disabled");
    ImGui::Text(" ");
    ImGui::Text(" ");
    ImGui::Text(" ");
  }

  ImGui::Spacing();
  ImGui::Separator();

  // === THRESHOLD GUIDELINES with Viridis colors ===
  ImGui::Text("Thresholds:");
  ImGui::TextColored(ImVec4(0.99f, 0.91f, 0.15f, 1.0f), ">110%% Excellent");      // Yellow
  ImGui::TextColored(ImVec4(0.13f, 0.82f, 0.52f, 1.0f), ">100%% Pass");          // Green
  ImGui::TextColored(ImVec4(0.12f, 0.65f, 0.61f, 1.0f), ">95%% Need more work"); // Teal
  ImGui::TextColored(ImVec4(0.27f, 0.0f, 0.33f, 1.0f), "<95%% Are you sure?");   // Purple

  // === SPEC INPUT AT BOTTOM ===
  ImGui::Separator();

  // Enable/disable checkbox
  ImGui::Checkbox("Enable", &m_specEnabled);

  ImGui::SameLine();

  // Compact input field
  ImGui::SetNextItemWidth(60);
  if (ImGui::InputText("##SpecValue", m_specInputBuffer, sizeof(m_specInputBuffer),
    ImGuiInputTextFlags_CharsDecimal)) {
    UpdateSpecThreshold();
  }

  ImGui::SameLine();

  // Unit combo box
  ImGui::SetNextItemWidth(50);
  const char* units[] = { "pA", "nA", "uA", "mA", "A" };
  int currentUnit = GetUnitIndex(m_specUnit);
  if (ImGui::Combo("##Unit", &currentUnit, units, IM_ARRAYSIZE(units))) {
    m_specUnit = units[currentUnit];
    UpdateSpecThreshold();
  }

  // NEW: Save button
  ImGui::SameLine();
  if (ImGui::Button("Save", ImVec2(40, 0))) {
    SaveSpecToDatabase();
  }

  // NEW: Show save status
  if (!m_lastCaptureStatus.empty() && m_lastCaptureStatus.find("Spec") != std::string::npos) {
    ImGui::Spacing();
    if (m_lastCaptureStatus.find("Error") != std::string::npos) {
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", m_lastCaptureStatus.c_str());
    }
    else {
      ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s", m_lastCaptureStatus.c_str());
    }

    // Clear the status after 3 seconds
    static auto statusTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    static bool statusShown = false;

    if (!statusShown && m_lastCaptureStatus.find("Spec") != std::string::npos) {
      statusTime = now;
      statusShown = true;
    }

    if (statusShown && std::chrono::duration_cast<std::chrono::seconds>(now - statusTime).count() > 3) {
      if (m_lastCaptureStatus.find("Spec") != std::string::npos) {
        m_lastCaptureStatus.clear();
        statusShown = false;
      }
    }
  }
}

// Updated comparison bar with Viridis gradient
void RunPageUI::RenderComparisonBar(float percentage) {
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  ImVec2 pos = ImGui::GetCursorScreenPos();
  float width = ImGui::GetContentRegionAvail().x - 10;
  float height = 30.0f;

  // Draw background rectangle (dark gray)
  drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height),
    IM_COL32(30, 30, 30, 255));

  // Calculate fill width based on percentage (max at 120%)
  float fillWidth = (percentage / 120.0f) * width;
  fillWidth = (std::min)(fillWidth, width);

  // Create gradient effect for the fill bar using Viridis colors
  // The bar gradually changes color based on percentage
  int segments = 20;
  float segmentWidth = fillWidth / segments;

  for (int i = 0; i < segments; i++) {
    float segmentPercentage = (percentage / segments) * (i + 1);
    ImVec4 segmentColor = GetPercentageColor(segmentPercentage);
    ImU32 color = ImGui::ColorConvertFloat4ToU32(segmentColor);

    float x1 = pos.x + i * segmentWidth;
    float x2 = pos.x + (i + 1) * segmentWidth;

    drawList->AddRectFilled(ImVec2(x1, pos.y),
      ImVec2(x2, pos.y + height),
      color);
  }

  // Add threshold marker lines
  float line100 = (100.0f / 120.0f) * width;  // 100% line
  float line110 = (110.0f / 120.0f) * width;  // 110% line
  float line95 = (95.0f / 120.0f) * width;    // 95% line

  // Draw threshold lines with appropriate colors
  drawList->AddLine(ImVec2(pos.x + line100, pos.y),
    ImVec2(pos.x + line100, pos.y + height),
    IM_COL32(255, 255, 255, 200), 2.0f);  // White for 100%

  drawList->AddLine(ImVec2(pos.x + line110, pos.y),
    ImVec2(pos.x + line110, pos.y + height),
    IM_COL32(252, 232, 38, 200), 1.0f);   // Yellow for 110%

  drawList->AddLine(ImVec2(pos.x + line95, pos.y),
    ImVec2(pos.x + line95, pos.y + height),
    IM_COL32(31, 166, 156, 200), 1.0f);   // Teal for 95%

  // Add percentage text overlay
  char text[32];
  snprintf(text, sizeof(text), "%.1f%%", percentage);
  ImVec2 textSize = ImGui::CalcTextSize(text);
  ImVec2 textPos = ImVec2(pos.x + width / 2 - textSize.x / 2,
    pos.y + height / 2 - textSize.y / 2);
  drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), text);

  // Move cursor past the bar
  ImGui::Dummy(ImVec2(0, height));
}// RunPageUI.cpp - Add this new method (replaces the plot initialization code)



// Fixed RenderLivePlot method - uses deferred update pattern
void RunPageUI::RenderLivePlot() {
  // === PROCESS ANY PENDING CHANNEL CHANGE FIRST ===
  // This happens before any rendering to avoid mutex issues
  if (m_channelChangeRequested && m_liveDataPlot && m_plotInitialized) {
    if (!m_pendingChannelName.empty() && m_pendingChannelName != m_plotChannelName) {
      // Safely change the channel
      m_plotChannelName = m_pendingChannelName;

      // Instead of calling SetChannel directly, reinitialize the plot config
      LiveDataPlot::Config config;
      config.channelName = m_plotChannelName;
      config.timeWindow = 10.0f;
      config.historySize = 1000;
      config.autoScale = true;
      config.showCurrentValue = true;
      config.enableChannelSelector = false;
      config.showGrid = true;
      config.showLegend = true;
      config.lineColor = ImVec4(0.0f, 1.0f, 0.2f, 1.0f);
      config.lineThickness = 2.0f;
      config.yAxisLabel = "";
      config.enableSpec = m_specEnabled;
      config.specValue = m_specThreshold;

      // Update config instead of SetChannel to avoid mutex issues
      m_liveDataPlot->UpdateConfig(config);

      m_logger->LogInfo("LiveDataPlot channel changed to: " + m_plotChannelName);
    }
    m_channelChangeRequested = false;
    m_pendingChannelName = "";
  }

  // === CHANNEL SELECTOR UI ===
  ImGui::Text("Data Channel:");
  ImGui::SameLine();

  // Refresh available channels periodically
  if (m_availableChannels.empty() || ImGui::GetFrameCount() % 120 == 0) {
    GlobalDataStore* store = GlobalDataStore::GetInstance();
    if (store) {
      m_availableChannels = store->GetAvailableChannels();

      // Find current channel index
      auto it = std::find(m_availableChannels.begin(), m_availableChannels.end(), m_plotChannelName);
      if (it != m_availableChannels.end()) {
        m_selectedChannelIndex = static_cast<int>(std::distance(m_availableChannels.begin(), it));
      }
    }
  }

  // Channel dropdown combo box
  ImGui::SetNextItemWidth(-1); // Use full width
  if (!m_availableChannels.empty()) {
    // Create array of channel names for combo
    std::vector<const char*> items;
    for (const auto& channel : m_availableChannels) {
      items.push_back(channel.c_str());
    }

    if (ImGui::Combo("##ChannelSelect", &m_selectedChannelIndex, items.data(), static_cast<int>(items.size()))) {
      // Channel selected - request a change for next frame
      if (m_selectedChannelIndex >= 0 && m_selectedChannelIndex < m_availableChannels.size()) {
        m_pendingChannelName = m_availableChannels[m_selectedChannelIndex];
        m_channelChangeRequested = true;
      }
    }
  }
  else {
    ImGui::TextDisabled("No channels available");
  }

  ImGui::Separator();
  ImGui::Spacing();

  // === INITIALIZE PLOT IF NEEDED ===
  if (!m_liveDataPlot) {
    auto* plotManager = LiveDataPlotManager::GetInstance();
    auto* plot = plotManager->GetPlot("column3_main_plot");

    LiveDataPlot::Config config;
    config.channelName = m_plotChannelName;
    config.timeWindow = 10.0f;
    config.historySize = 1000;
    config.autoScale = true;
    config.showCurrentValue = true;
    config.enableChannelSelector = false;  // We have our own selector
    config.showGrid = true;
    config.showLegend = true;
    config.lineColor = ImVec4(0.0f, 1.0f, 0.2f, 1.0f);
    config.lineThickness = 2.0f;
    config.yAxisLabel = "";
    config.enableSpec = m_specEnabled;
    config.specValue = m_specThreshold;

    plot->Initialize(config);
    m_liveDataPlot = plot;
    m_plotInitialized = true;

    m_logger->LogInfo("LiveDataPlot initialized with channel: " + m_plotChannelName);
  }

  // === UPDATE SPEC (safe to call every frame) ===
  if (m_liveDataPlot && m_plotInitialized) {
    m_liveDataPlot->SetSpec(m_specThreshold, m_specEnabled);
  }

  // === RENDER THE PLOT ===
  if (m_liveDataPlot && m_plotInitialized) {
    ImVec2 plotSize = ImGui::GetContentRegionAvail();
    m_liveDataPlot->Render(plotSize);
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Initializing plot...");
  }
}


// Helper method to change channel
void RunPageUI::ChangeChannel(const std::string& channelName) {
  m_plotChannelName = channelName;

  // Update index
  auto it = std::find(m_availableChannels.begin(), m_availableChannels.end(), channelName);
  if (it != m_availableChannels.end()) {
    m_selectedChannelIndex = static_cast<int>(std::distance(m_availableChannels.begin(), it));
  }

  // Update the plot
  if (m_liveDataPlot && m_plotInitialized) {
    m_liveDataPlot->SetChannel(m_plotChannelName);
    m_logger->LogInfo("LiveDataPlot channel changed to: " + m_plotChannelName);
  }
}


// ============================================================================
// New method: HandlePanelClick
// ============================================================================
void RunPageUI::HandlePanelClick(int panelNumber) {
  // Toggle between normal and expanded state
  switch (panelNumber) {
  case 1:
    if (m_panelState == PanelState::Panel1) {
      m_panelState = PanelState::Normal; // Collapse back to normal
    }
    else {
      m_panelState = PanelState::Panel1; // Expand panel 1
    }
    break;

  case 2:
    if (m_panelState == PanelState::Panel2) {
      m_panelState = PanelState::Normal;
    }
    else {
      m_panelState = PanelState::Panel2;
    }
    break;

  case 3:
    if (m_panelState == PanelState::Panel3) {
      m_panelState = PanelState::Normal;
    }
    else {
      m_panelState = PanelState::Panel3;
    }
    break;
  }

  // Log state change
  std::string stateStr = "Normal";
  switch (m_panelState) {
  case PanelState::Panel1: stateStr = "Panel1 Expanded"; break;
  case PanelState::Panel2: stateStr = "Panel2 Expanded"; break;
  case PanelState::Panel3: stateStr = "Panel3 Expanded"; break;
  default: break;
  }
  m_logger->LogInfo("Panel state changed to: " + stateStr);
}

void RunPageUI::RenderPanelContent(int panelNumber, const ImVec2& size) {
  switch (panelNumber) {
  case 1:
    // Camera content - use full available size
    if (m_embeddedCameraSubscriber && m_cameraSystemInitialized) {
      // Store the canvas position BEFORE rendering the camera
      ImVec2 canvasPos = ImGui::GetCursorScreenPos();

      // Render the camera feed
      RenderEmbeddedCameraFeed(size);

      // Debug log to verify this code path is reached
      if (m_showCrosshair) {
       // m_logger->LogInfo("RenderPanelContent: Rendering crosshair");
        RenderCrosshairOverlay(canvasPos, size);
      }
      else {
        //m_logger->LogInfo("RenderPanelContent: NOT rendering crosshair (disabled)");
      }
    }
    else {
      RenderCameraPlaceholder(size, "Camera not available");
    }
    break;

  case 2:
    // Panel 2 content (unchanged)
    ImGui::BeginChild("Panel2Content", size);
    ImGui::TextDisabled("Available for\nfuture content");

    if (m_panelState == PanelState::Panel2) {
      ImGui::Separator();
      ImGui::Text("Expanded view for Panel 2");
      ImGui::Text("Additional content can go here");
      ImGui::Text("Width: %.0f", size.x);
      ImGui::Text("Height: %.0f", size.y);
    }
    ImGui::EndChild();
    break;

  case 3:
    // Panel 3 content (unchanged)
    ImGui::BeginChild("Panel3Content", size);
    ImGui::TextDisabled("Available for\nfuture content");

    if (m_panelState == PanelState::Panel3) {
      ImGui::Separator();
      ImGui::Text("Expanded view for Panel 3");
      ImGui::Text("Additional content can go here");
      ImGui::Text("Width: %.0f", size.x);
      ImGui::Text("Height: %.0f", size.y);
    }
    ImGui::EndChild();
    break;
  }
}



// In RunPageUI constructor or initialization method
void RunPageUI::InitializeCameraViewport() {
  if (!m_cameraViewport) {
    CameraViewport::ViewportConfig config;
    config.defaultSize = ImVec2(640, 250);
    config.showControls = false; // Hide controls since it's embedded
    config.showStatus = true;    // Keep status visible
    config.retryIntervalMs = 2000;

    m_cameraViewport = std::make_unique<CameraViewport>("Column3_Camera", config);

    // Set up for main_camera
    if (m_cameraManager) {
      m_cameraViewport->SetCameraManager(m_cameraManager);
      m_cameraViewport->SetCameraId("main_camera");
      m_cameraViewport->StartFeed();
    }
  }
}

// REPLACE the RenderLiveViewTab() method in RunPageUI.cpp with this UIConfigVisualizer-style implementation

// ENHANCED RenderLiveViewTab() method with crosshair overlay
// Replace the existing RenderLiveViewTab() method in RunPageUI.cpp


void RunPageUI::RenderLiveViewTab() {
  ImGui::Text("Live Camera Feed");
  ImGui::Separator();

  // Add this "Fix Broadcast" button to your UI
  if (ImGui::Button("Fix Broadcast", ImVec2(120, 25))) {
    m_logger->LogInfo("RunPageUI: Restarting broadcast system...");

    // Stop and restart the entire broadcast system
    m_cameraManager->StopBroadcastSystem();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    m_cameraManager->StartBroadcastSystem();

    // Re-subscribe
    m_cameraManager->SubscribeToFrames(m_cameraSubscriber);

    UpdateStatus("Broadcast system restarted");
  }
  // Add frame flow monitoring display
  if (m_cameraSubscriber) {
    static uint64_t lastDisplayFrameCount = 0;
    static auto lastDisplayTime = std::chrono::steady_clock::now();
    static bool frameFlowActive = false;

    uint64_t currentFrames = m_cameraSubscriber->GetTotalFramesReceived();
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDisplayTime).count();

    if (elapsed >= 1000) { // Check every second
      frameFlowActive = (currentFrames > lastDisplayFrameCount);
      lastDisplayFrameCount = currentFrames;
      lastDisplayTime = now;
    }

    // Display frame flow status
    ImGui::Text("Frame Flow: ");
    ImGui::SameLine();
    if (frameFlowActive) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "ACTIVE");
    }
    else if (currentFrames > 0) {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "STOPPED");
    }
    else {
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "NO FRAMES");
    }

    ImGui::SameLine();
    ImGui::Text("Total: %llu", currentFrames);
  }

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


void RunPageUI::RenderCrosshairOverlay(const ImVec2& canvasPos, const ImVec2& canvasSize) {
  ImDrawList* drawList = ImGui::GetWindowDrawList();

  // Calculate center point
  float centerX = canvasPos.x + canvasSize.x * 0.5f;
  float centerY = canvasPos.y + canvasSize.y * 0.5f;

  // Simple 1-pixel green lines spanning full width and height
  const ImU32 color = IM_COL32(0, 255, 0, 200); // Green with transparency

  // Horizontal line (full width)
  drawList->AddLine(
    ImVec2(canvasPos.x, centerY),
    ImVec2(canvasPos.x + canvasSize.x, centerY),
    color,
    1.0f  // 1 pixel thickness
  );

  // Vertical line (full height)
  drawList->AddLine(
    ImVec2(centerX, canvasPos.y),
    ImVec2(centerX, canvasPos.y + canvasSize.y),
    color,
    1.0f  // 1 pixel thickness
  );
}


// ENHANCED SetCameraManager with debugging and forced grabbing start
void RunPageUI::SetCameraManager(CameraManager* cameraManager) {
  m_cameraManager = cameraManager;

  // Clear existing feeds first
  ClearCameraFeed();
  ClearEmbeddedCameraFeed();

  if (m_cameraManager) {
    auto cameraIds = m_cameraManager->GetCameraIds();
    m_logger->LogInfo("RunPageUI: Camera manager set with " + std::to_string(cameraIds.size()) + " cameras");

    if (!cameraIds.empty()) {
      // Debug: Print all available cameras
      for (const auto& id : cameraIds) {
        m_logger->LogInfo("RunPageUI: Available camera: " + id);
      }

      // Always prefer main_camera if available
      bool hasMainCamera = std::find(cameraIds.begin(), cameraIds.end(), "main_camera") != cameraIds.end();

      if (hasMainCamera) {
        m_selectedCameraId = "main_camera";
        m_logger->LogInfo("RunPageUI: Selected main_camera");
      }
      else {
        m_selectedCameraId = cameraIds[0];
        m_logger->LogInfo("RunPageUI: main_camera not found, using: " + m_selectedCameraId);
      }

      // Debug: Check camera status before initialization
      auto status = m_cameraManager->GetCameraStatus(m_selectedCameraId);
      std::string statusMsg = "RunPageUI: Camera " + m_selectedCameraId + " - Connected: " +
        (status.connected ? std::string("true") : std::string("false")) +
        ", Grabbing: " + (status.grabbing ? std::string("true") : std::string("false"));
      m_logger->LogInfo(statusMsg);

      // Force connect and start grabbing if not already
      if (!status.connected) {
        m_logger->LogInfo("RunPageUI: Connecting camera: " + m_selectedCameraId);
        bool connected = m_cameraManager->ConnectCamera(m_selectedCameraId);
        std::string connectMsg = "RunPageUI: Camera connection result: " +
          (connected ? std::string("success") : std::string("failed"));
        m_logger->LogInfo(connectMsg);

        if (connected) {
          status = m_cameraManager->GetCameraStatus(m_selectedCameraId);
          std::string postConnectMsg = "RunPageUI: Post-connect status - Connected: " +
            (status.connected ? std::string("true") : std::string("false"));
          m_logger->LogInfo(postConnectMsg);
        }
      }

      if (status.connected && !status.grabbing) {
        m_logger->LogInfo("RunPageUI: Starting grabbing for: " + m_selectedCameraId);

        // Try StartGrabbingWithBroadcast first, then fall back to StartGrabbing
        bool grabbingStarted = false;
        try {
          grabbingStarted = m_cameraManager->StartGrabbingWithBroadcast(m_selectedCameraId);
          std::string grabMsg = "RunPageUI: StartGrabbingWithBroadcast result: " +
            (grabbingStarted ? std::string("success") : std::string("failed"));
          m_logger->LogInfo(grabMsg);
        }
        catch (...) {
          m_logger->LogInfo("RunPageUI: StartGrabbingWithBroadcast not available, trying StartGrabbing");
          grabbingStarted = m_cameraManager->StartGrabbing(m_selectedCameraId);
          std::string grabMsg = "RunPageUI: StartGrabbing result: " +
            (grabbingStarted ? std::string("success") : std::string("failed"));
          m_logger->LogInfo(grabMsg);
        }

        if (grabbingStarted) {
          status = m_cameraManager->GetCameraStatus(m_selectedCameraId);
          std::string postGrabMsg = "RunPageUI: Post-grabbing status - Grabbing: " +
            (status.grabbing ? std::string("true") : std::string("false"));
          m_logger->LogInfo(postGrabMsg);
        }
      }

      // Initialize camera feeds
      m_logger->LogInfo("RunPageUI: Initializing camera feeds...");
      InitializeCameraFeed();
      InitializeEmbeddedCameraFeed();

      m_cameraSystemInitialized = true;
      m_logger->LogInfo("RunPageUI: Camera system initialization complete");

      // Final status check
      status = m_cameraManager->GetCameraStatus(m_selectedCameraId);
      std::string finalMsg = "RunPageUI: Final camera status - Connected: " +
        (status.connected ? std::string("true") : std::string("false")) +
        ", Grabbing: " + (status.grabbing ? std::string("true") : std::string("false"));
      m_logger->LogInfo(finalMsg);

    }
    else {
      m_logger->LogWarning("RunPageUI: No cameras available");
      m_cameraSystemInitialized = false;
    }
  }
  else {
    m_logger->LogInfo("RunPageUI: Camera manager cleared");
    m_cameraSystemInitialized = false;
  }
}

// SIMPLE FIX: InitializeCameraFeed with manual status update
void RunPageUI::InitializeCameraFeed() {
  if (m_selectedCameraId.empty() || !m_cameraManager) {
    m_cameraSystemInitialized = false;
    UpdateStatus("Cannot initialize camera feed: no camera selected or manager unavailable");
    m_logger->LogError("RunPageUI: InitializeCameraFeed failed - no camera or manager");
    return;
  }

  m_logger->LogInfo("RunPageUI: InitializeCameraFeed starting for: " + m_selectedCameraId);

  // Clear existing subscription
  ClearCameraFeed();

  // Add delay after camera starts grabbing
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Create new subscriber
  m_cameraSubscriber = std::make_shared<LiveVideoSubscriber>(m_selectedCameraId);
  m_logger->LogInfo("RunPageUI: Created LiveVideoSubscriber with ID: " + m_cameraSubscriber->GetSubscriberId());

  // Subscribe to the broadcasting system
  m_cameraManager->SubscribeToFrames(m_cameraSubscriber);
  m_logger->LogInfo("RunPageUI: SubscribeToFrames called");

  // Start broadcast system if not already active
  m_cameraManager->StartBroadcastSystem();
  m_logger->LogInfo("RunPageUI: StartBroadcastSystem called");

  // REPLACE with this single line:
  m_cameraSubscriber->InitializeStatusFromManager(m_cameraManager);

  // Give subscriber a moment to process the status update
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  m_cameraSystemInitialized = true;
  m_logger->LogInfo("RunPageUI: Camera feed initialized for: " + m_selectedCameraId);
  UpdateStatus("Camera feed initialized for: " + m_selectedCameraId);

  // Debug: Check subscriber status after manual update
  std::string subscriberConnectedMsg = "RunPageUI: Subscriber camera connected: " +
    (m_cameraSubscriber->IsCameraConnected() ? std::string("true") : std::string("false"));
  m_logger->LogInfo(subscriberConnectedMsg);

  std::string subscriberGrabbingMsg = "RunPageUI: Subscriber camera grabbing: " +
    (m_cameraSubscriber->IsCameraGrabbing() ? std::string("true") : std::string("false"));
  m_logger->LogInfo(subscriberGrabbingMsg);



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


std::unique_ptr<SequenceStep> RunPageUI::BuildSelectedProcess() {
  // NEW: Recipe-based building
  if (m_usingRecipe) {
    ProcessInstance* instance = GetSelectedRecipeInstance();
    if (!instance) {
      UpdateStatus("Selected recipe instance not found", true);
      m_logger->LogError("Could not find recipe instance: " + m_selectedProcess);
      return nullptr;
    }

    return BuildFromRecipeInstance(instance);
  }

  // Original registry-based process building (non-configurable)
  if (ProcessRegistry::GetInstance().HasProcess(m_selectedProcess)) {
    if (m_promptUI) {
      auto process = ProcessRegistry::GetInstance().BuildProcess(
        m_selectedProcess, m_machineOps, *m_promptUI);
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

void RunPageUI::InitializeEmbeddedCameraFeed() {
  if (!m_cameraManager || !m_cameraSystemInitialized) {
    return;
  }

  // Simply reuse the main subscriber instead of creating a new one
  m_embeddedCameraSubscriber = m_cameraSubscriber;
  m_logger->LogInfo("RunPageUI: Embedded camera feed sharing main subscriber");
}

// NEW: Clear embedded camera feed
void RunPageUI::ClearEmbeddedCameraFeed() {
  if (m_embeddedCameraSubscriber && m_cameraManager) {
    m_cameraManager->UnsubscribeFromFrames(m_embeddedCameraSubscriber->GetSubscriberId());
  }
  m_embeddedCameraSubscriber.reset();
}

void RunPageUI::RenderEmbeddedCameraFeed(const ImVec2& canvasSize) {
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();
  UpdateCameraTexture();

  if (m_textureInitialized && m_textureWidth > 0 && m_textureHeight > 0) {
    // Calculate display size
    float aspectRatio = (float)m_textureWidth / (float)m_textureHeight;
    float displayWidth = canvasSize.x;
    float displayHeight = displayWidth / aspectRatio;

    if (displayHeight > canvasSize.y) {
      displayHeight = canvasSize.y;
      displayWidth = displayHeight * aspectRatio;
    }

    // Center the display
    float offsetX = (canvasSize.x - displayWidth) * 0.5f;
    float offsetY = (canvasSize.y - displayHeight) * 0.5f;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);

    // ZOOM IMPLEMENTATION: Calculate UV coordinates for cropping
    float uvMinX = 0.0f, uvMinY = 0.0f, uvMaxX = 1.0f, uvMaxY = 1.0f;

    if (m_panel1ZoomLevel < 1.0f) {
      // Calculate crop region (centered)
      float cropFactor = (1.0f - m_panel1ZoomLevel) / 2.0f;

      uvMinX = cropFactor;
      uvMinY = cropFactor;
      uvMaxX = 1.0f - cropFactor;
      uvMaxY = 1.0f - cropFactor;
    }

    // Use ImGui::Image with UV coordinates
    ImGui::Image(
      (ImTextureID)(intptr_t)m_cameraTextureID,
      ImVec2(displayWidth, displayHeight),
      ImVec2(uvMinX, uvMinY),  // UV min
      ImVec2(uvMaxX, uvMaxY)   // UV max
    );

    // Optional: Show zoom indicator in corner
    if (m_panel1ZoomLevel < 1.0f) {
      ImDrawList* drawList = ImGui::GetForegroundDrawList();
      ImVec2 textPos = ImVec2(canvasPos.x + 10, canvasPos.y + 10);
      char zoomText[32];
      snprintf(zoomText, sizeof(zoomText), "Zoom: %.0f%%", m_panel1ZoomLevel * 100);

      // Draw background for text
      ImVec2 textSize = ImGui::CalcTextSize(zoomText);
      drawList->AddRectFilled(
        ImVec2(textPos.x - 2, textPos.y - 2),
        ImVec2(textPos.x + textSize.x + 2, textPos.y + textSize.y + 2),
        IM_COL32(0, 0, 0, 180),
        3.0f
      );

      // Draw text
      drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), zoomText);
    }
  }
  else {
    // Show placeholder
    std::string message = "Waiting for video frames...";
    if (m_embeddedCameraSubscriber) {
      message += "\nFrames: " + std::to_string(m_embeddedCameraSubscriber->GetTotalFramesReceived());
    }
    RenderCameraPlaceholder(canvasSize, message);
  }
}



// Add this auto-recovery logic to your DebugCameraFrameFlow() method
void RunPageUI::DebugCameraFrameFlow() {
  if (!m_cameraSubscriber || !m_cameraManager) {
    return;
  }

  static uint64_t lastFrameCount = 0;
  static auto lastCheckTime = std::chrono::steady_clock::now();
  static int noFrameWarningCount = 0;

  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCheckTime).count();

  // Check every 2 seconds
  if (elapsed >= 2000) {
    uint64_t currentFrameCount = m_cameraSubscriber->GetTotalFramesReceived();

    // ADD THIS DEBUG LINE:
    //std::cout << "[DEBUG FLOW CHECK] Subscriber frame count: " << currentFrameCount
    //  << " (last was: " << lastFrameCount << ")" << std::endl;

    // Check if frames are still flowing
    if (currentFrameCount == lastFrameCount && currentFrameCount > 0) {
      noFrameWarningCount++;
      m_logger->LogWarning("RunPageUI: Frame flow stopped - stuck at " +
        std::to_string(currentFrameCount) + " frames (warning #" +
        std::to_string(noFrameWarningCount) + ")");

      // AUTO-RECOVERY: Restart broadcast system after 3 warnings (6 seconds)
      if (noFrameWarningCount >= 3) {
        m_logger->LogInfo("RunPageUI: Auto-recovering broadcast system...");

        // Restart broadcast system
        m_cameraManager->StopBroadcastSystem();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        m_cameraManager->StartBroadcastSystem();
        m_cameraManager->SubscribeToFrames(m_cameraSubscriber);

        noFrameWarningCount = 0; // Reset counter
        UpdateStatus("Auto-recovered camera broadcast system");
      }

    }
    else if (currentFrameCount > lastFrameCount) {
      // Frames are flowing normally
      if (noFrameWarningCount > 0) {
        m_logger->LogInfo("RunPageUI: Frame flow resumed - now at " +
          std::to_string(currentFrameCount) + " frames");
        noFrameWarningCount = 0;
      }
    }

    lastFrameCount = currentFrameCount;
    lastCheckTime = now;
  }
}



// RunPageUI.cpp - Fixed RenderConfigurableTab() to prevent infinite loading

void RunPageUI::RenderConfigurableTab() {
  using namespace UAA3ProcessBuilders;

  // Header
  ImGui::Text("Process Configuration Editor");
  ImGui::Separator();

  // Check if selected process is configurable
  bool isConfigurable = (m_selectedProcess.find("_Configurable") != std::string::npos);

  if (!isConfigurable) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
      "Select a configurable process to edit settings");
    ImGui::Spacing();
    ImGui::TextWrapped("Configurable processes have '_Configurable' in their name");
    return;
  }

  // Initialize if needed
  if (!m_processConfigUI) {
    m_processConfigUI = std::make_unique<ProcessConfigUI>();
    m_logger->LogInfo("Created ProcessConfigUI instance");
  }

  // CRITICAL FIX: Only sync when process actually changed
  // Track the last synced process to avoid repeated calls
  static std::string lastSyncedProcess = "";

  if (lastSyncedProcess != m_selectedProcess) {
    // Process changed - sync the configuration
    m_processConfigUI->setCurrentProcess(m_selectedProcess);
    lastSyncedProcess = m_selectedProcess;
    m_logger->LogInfo("Synced ProcessConfigUI with: " + m_selectedProcess);
  }

  // Render the configuration UI embedded (pass true for embedded)
  if (m_processConfigUI) {
    static auto lastTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - lastTime).count();
    lastTime = now;

    // Call with embedded = true
    m_processConfigUI->render(deltaTime, true);

    // Get updated configuration for use when building the process
    m_currentProcessConfig = m_processConfigUI->getConfig();
  }
}


// RunPageUI.cpp - Add these helper methods at the end of the file

// ============================================================================
// Helper: Update spec threshold from input buffer
// ============================================================================
void RunPageUI::UpdateSpecThreshold() {
  float value = 0.0f;
  try {
    value = std::stof(m_specInputBuffer);
  }
  catch (...) {
    return;  // Invalid input, don't update
  }

  // Convert to Amps based on selected unit
  float multiplier = GetUnitMultiplier(m_specUnit);
  m_specThreshold = value * multiplier;
}

// ============================================================================
// Helper: Get unit multiplier for conversion to Amps
// ============================================================================
float RunPageUI::GetUnitMultiplier(const std::string& unit) {
  if (unit == "pA") return 1e-12f;  // picoamps
  if (unit == "nA") return 1e-9f;   // nanoamps
  if (unit == "uA") return 1e-6f;   // microamps
  if (unit == "mA") return 1e-3f;   // milliamps
  if (unit == "A") return 1.0f;     // amps
  return 1e-6f;  // Default to microamps
}

// ============================================================================
// Helper: Get unit index for combo box
// ============================================================================
int RunPageUI::GetUnitIndex(const std::string& unit) {
  if (unit == "pA") return 0;
  if (unit == "nA") return 1;
  if (unit == "uA") return 2;
  if (unit == "mA") return 3;
  if (unit == "A") return 4;
  return 2;  // Default to uA (index 2)
}

// ============================================================================
// Helper: Format current value with appropriate units
// ============================================================================
void RunPageUI::FormatCurrentValue(float value, char* buffer, size_t bufferSize) {
  float absValue = std::abs(value);

  if (absValue == 0.0f) {
    snprintf(buffer, bufferSize, "0.00 A");
  }
  else if (absValue < 1e-9f) {
    snprintf(buffer, bufferSize, "%.2f pA", value * 1e12f);
  }
  else if (absValue < 1e-6f) {
    snprintf(buffer, bufferSize, "%.2f nA", value * 1e9f);
  }
  else if (absValue < 1e-3f) {
    snprintf(buffer, bufferSize, "%.2f µA", value * 1e6f);
  }
  else if (absValue < 1.0f) {
    snprintf(buffer, bufferSize, "%.3f mA", value * 1e3f);
  }
  else {
    snprintf(buffer, bufferSize, "%.3f A", value);
  }
}




void RunPageUI::RenderActionTab() {
  // Camera Group
  ImGui::Text("Camera Group");
  ImGui::Separator();

  ImGui::Spacing();

  // Center the button
  float buttonWidth = 160.0f;
  float windowWidth = ImGui::GetContentRegionAvail().x;
  float centerOffset = (windowWidth - buttonWidth) * 0.5f;

  if (centerOffset > 0) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centerOffset);
  }

  // Use the fancy button - it returns true when clicked
  if (RenderFancyCameraButton()) {
    CaptureAllCameraFrames();
  }

  // ADD CROSSHAIR TOGGLE BUTTON
  ImGui::Spacing();
  ImGui::Spacing();

  // Center the crosshair toggle button
  if (centerOffset > 0) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centerOffset);
  }

  // Crosshair toggle button with color based on state
  if (m_showCrosshair) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
  }

  const char* crosshairText = m_showCrosshair ?
    reinterpret_cast<const char*>(u8"🎯 Crosshair ON") :
    reinterpret_cast<const char*>(u8"🎯 Crosshair OFF");

  if (ImGui::Button(crosshairText, ImVec2(buttonWidth, 35))) {
    m_showCrosshair = !m_showCrosshair;
    UpdateStatus(m_showCrosshair ? "Crosshair enabled" : "Crosshair disabled");

    // Debug log to verify state change
    m_logger->LogInfo("Crosshair toggled to: " + std::string(m_showCrosshair ? "ON" : "OFF"));


  }

  ImGui::PopStyleColor(2);



  // ADD ZOOM CONTROLS
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::Text("Camera Zoom");
  ImGui::Spacing();

  // Create 4 zoom buttons in a 2x2 grid
  float zoomButtonWidth = (buttonWidth - 5) / 2.0f;  // Two buttons per row with spacing

  // Center the zoom controls
  if (centerOffset > 0) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centerOffset);
  }

  // First row: 25% and 50%
  bool is25 = (m_panel1ZoomLevel == 0.25f);
  if (is25) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
  }
  if (ImGui::Button("25%", ImVec2(zoomButtonWidth, 30))) {
    m_panel1ZoomLevel = 0.25f;
    UpdateStatus("Camera zoom: 25% (cropped to center)");
  }
  if (is25) ImGui::PopStyleColor();

  ImGui::SameLine();

  bool is50 = (m_panel1ZoomLevel == 0.50f);
  if (is50) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
  }
  if (ImGui::Button("50%", ImVec2(zoomButtonWidth, 30))) {
    m_panel1ZoomLevel = 0.50f;
    UpdateStatus("Camera zoom: 50% (cropped to center)");
  }
  if (is50) ImGui::PopStyleColor();

  // Second row: 75% and 100%
  if (centerOffset > 0) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centerOffset);
  }

  bool is75 = (m_panel1ZoomLevel == 0.75f);
  if (is75) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
  }
  if (ImGui::Button("75%", ImVec2(zoomButtonWidth, 30))) {
    m_panel1ZoomLevel = 0.75f;
    UpdateStatus("Camera zoom: 75% (cropped to center)");
  }
  if (is75) ImGui::PopStyleColor();

  ImGui::SameLine();

  bool is100 = (m_panel1ZoomLevel == 1.0f);
  if (is100) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));  // Green for default
  }
  if (ImGui::Button("100%", ImVec2(zoomButtonWidth, 30))) {
    m_panel1ZoomLevel = 1.0f;
    UpdateStatus("Camera zoom: 100% (full frame)");
  }
  if (is100) ImGui::PopStyleColor();

  // Show current zoom level
  ImGui::Spacing();
  if (centerOffset > 0) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centerOffset);
  }
  ImGui::Text("Current: %.0f%%", m_panel1ZoomLevel * 100.0f);






  // Show animated progress indicator when capturing
  if (m_captureInProgress) {
    ImGui::Spacing();

    // Center progress bar
    float progressWidth = 180.0f;
    float progressOffset = (windowWidth - progressWidth) * 0.5f;
    if (progressOffset > 0) {
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + progressOffset);
    }

    // Animated progress bar
    static float progress = 0.0f;
    progress += ImGui::GetIO().DeltaTime * 0.3f;
    if (progress > 1.0f) progress = 0.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
    ImGui::ProgressBar(progress, ImVec2(progressWidth, 6));
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
  }

  // Status message section
  if (!m_lastCaptureStatus.empty()) {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Status with appropriate icon
    if (m_lastCaptureStatus.find("Success") != std::string::npos) {
      ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
        reinterpret_cast<const char*>(u8"✅ Status"));
    }
    else if (m_lastCaptureStatus.find("Error") != std::string::npos) {
      ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f),
        reinterpret_cast<const char*>(u8"❌ Status"));
    }
    else {
      ImGui::Text(reinterpret_cast<const char*>(u8"ℹ️ Status"));
    }

    ImGui::TextWrapped("%s", m_lastCaptureStatus.c_str());
  }
}




void RunPageUI::CaptureAllCameraFrames() {
  m_captureInProgress = true;
  m_lastCaptureStatus = "Capturing images from all cameras...";

  // Get camera manager from AppContext
  auto& context = AppContext::GetInstance();
  auto* cameraManager = context.GetCameraManager();

  if (!cameraManager) {
    m_lastCaptureStatus = "Error: Camera manager not available";
    m_captureInProgress = false;
    return;
  }

  // Use the built-in CaptureImageAll method
  bool success = cameraManager->CaptureImageAll();

  if (success) {
    // Get the count of cameras for status message
    size_t cameraCount = cameraManager->GetCameraCount();
    m_lastCaptureStatus = "Successfully captured images from " +
      std::to_string(cameraCount) + " camera(s)";

    // Optional: Show where images were saved
    std::string outputDir = cameraManager->GetImageOutputDirectory();
    m_lastCaptureStatus += "\nImages saved to: " + outputDir;
  }
  else {
    m_lastCaptureStatus = "Failed to capture images from some cameras";
  }

  m_captureInProgress = false;

  // Log the result
  if (m_logger) {
    m_logger->LogInfo(m_lastCaptureStatus);
  }
}


bool RunPageUI::RenderFancyCameraButton() {
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  ImVec2 pos = ImGui::GetCursorScreenPos();

  float width = 160.0f;
  float height = 40.0f;
  float rounding = 15.0f;

  // Button rectangle using ImVec2 for min and max
  ImVec2 bb_min = pos;
  ImVec2 bb_max = ImVec2(pos.x + width, pos.y + height);

  // Check interaction
  bool hovered = ImGui::IsMouseHoveringRect(bb_min, bb_max);
  bool clicked = hovered && ImGui::IsMouseClicked(0);

  // Colors
  ImU32 col_bg = hovered ?
    IM_COL32(51, 153, 255, 255) : // Hover: brighter blue
    IM_COL32(41, 128, 230, 255);  // Normal: blue

  if (m_captureInProgress) {
    col_bg = IM_COL32(128, 128, 128, 255); // Gray when disabled
    clicked = false; // Disable clicks when capturing
  }

  // Draw rounded rectangle
  drawList->AddRectFilled(bb_min, bb_max, col_bg, rounding);

  // Draw border
  drawList->AddRect(bb_min, bb_max,
    IM_COL32(255, 255, 255, 80), rounding, 0, 2.0f);

  // Add subtle gradient effect (optional)
  if (hovered && !m_captureInProgress) {
    drawList->AddRectFilled(bb_min,
      ImVec2(bb_max.x, bb_min.y + height * 0.5f),
      IM_COL32(255, 255, 255, 20), rounding);
  }

  // Draw text centered
  const char* text = m_captureInProgress ?
    reinterpret_cast<const char*>(u8"📷 Capturing...") :
    reinterpret_cast<const char*>(u8"📷 Take Photos");

  ImVec2 textSize = ImGui::CalcTextSize(text);
  ImVec2 textPos(
    pos.x + (width - textSize.x) * 0.5f,
    pos.y + (height - textSize.y) * 0.5f
  );

  drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), text);

  // Advance cursor
  ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + height + 5));

  return clicked;
}

void RunPageUI::LoadSpecFromDatabase() {
  auto& settings = AppSettings::getInstance();

  // === LOAD SPEC THRESHOLD ===
  if (!settings.exists("ui_settings", "spec_threshold_ua")) {
    bool created = settings.createFloat("ui_settings", "spec_threshold_ua", 800.0f);
    if (created) {
      m_logger->LogInfo("Created default spec threshold: 800uA");
    }
    else {
      m_logger->LogError("Failed to create default spec threshold");
      m_specThreshold = 800e-6f;
      strcpy_s(m_specInputBuffer, sizeof(m_specInputBuffer), "800");
      m_specUnit = "uA";
      return;
    }
  }

  auto specValue = settings.getFloat("ui_settings", "spec_threshold_ua");
  if (specValue.has_value()) {
    m_specThreshold = specValue.value() * 1e-6f;
    snprintf(m_specInputBuffer, sizeof(m_specInputBuffer), "%.0f", specValue.value());
    m_logger->LogInfo("Loaded spec threshold: " + std::to_string(specValue.value()) + "uA");
  }

  // === LOAD SPEC UNIT ===
  if (!settings.exists("ui_settings", "spec_unit")) {
    settings.createString("ui_settings", "spec_unit", "uA");
    m_logger->LogInfo("Created default spec unit: uA");
  }

  auto unitValue = settings.getString("ui_settings", "spec_unit");
  if (unitValue.has_value()) {
    m_specUnit = unitValue.value();
    m_logger->LogInfo("Loaded spec unit: " + m_specUnit);
  }

  // === LOAD THRESHOLDS ===
  if (!settings.exists("ui_settings", "pass_threshold")) {
    settings.createFloat("ui_settings", "pass_threshold", 100.0f);
  }
  auto passValue = settings.getFloat("ui_settings", "pass_threshold");
  if (passValue.has_value()) {
    m_thresholds.pass = passValue.value();
    m_logger->LogInfo("Loaded pass threshold: " + std::to_string(m_thresholds.pass) + "%");
  }

  if (!settings.exists("ui_settings", "fail_threshold")) {
    settings.createFloat("ui_settings", "fail_threshold", 95.0f);
  }
  auto failValue = settings.getFloat("ui_settings", "fail_threshold");
  if (failValue.has_value()) {
    m_thresholds.needWork = failValue.value();
    m_logger->LogInfo("Loaded fail threshold: " + std::to_string(m_thresholds.needWork) + "%");
  }

  if (!settings.exists("ui_settings", "exceptional_threshold")) {
    settings.createFloat("ui_settings", "exceptional_threshold", 110.0f);
  }
  auto exceptionalValue = settings.getFloat("ui_settings", "exceptional_threshold");
  if (exceptionalValue.has_value()) {
    m_thresholds.excellent = exceptionalValue.value();
    m_logger->LogInfo("Loaded exceptional threshold: " + std::to_string(m_thresholds.excellent) + "%");
  }

  m_logger->LogInfo("All spec settings loaded/refreshed from database");
}


void RunPageUI::SaveSpecToDatabase() {
  auto& settings = AppSettings::getInstance();

  float specInUA = m_specThreshold * 1e6f;
  m_logger->LogInfo("SaveSpecToDatabase: Attempting to save " + std::to_string(specInUA) + "uA");

  // Check if variable exists
  if (settings.exists("ui_settings", "spec_threshold_ua")) {
    // Try to read as float to check type compatibility
    auto currentValue = settings.getFloat("ui_settings", "spec_threshold_ua");
    if (!currentValue.has_value()) {
      // Variable exists but wrong type - delete and recreate
      m_logger->LogInfo("SaveSpecToDatabase: Variable has wrong type (BLOB), deleting and recreating as float");
      settings.deleteVariable("ui_settings", "spec_threshold_ua");

      bool created = settings.createFloat("ui_settings", "spec_threshold_ua", specInUA);
      if (created) {
        m_lastCaptureStatus = "Spec threshold saved: " + std::to_string((int)specInUA) + "uA";
        m_logger->LogInfo("SaveSpecToDatabase: Successfully recreated as float type");
      }
      else {
        m_lastCaptureStatus = "Error: Failed to recreate spec threshold";
        m_logger->LogError("SaveSpecToDatabase: Failed to recreate as float");
      }
      return;
    }
  }

  // Variable either doesn't exist or is correct type
  bool success = false;
  if (!settings.exists("ui_settings", "spec_threshold_ua")) {
    success = settings.createFloat("ui_settings", "spec_threshold_ua", specInUA);
    m_logger->LogInfo("SaveSpecToDatabase: Created new float variable");
  }
  else {
    success = settings.setFloat("ui_settings", "spec_threshold_ua", specInUA);
    m_logger->LogInfo("SaveSpecToDatabase: Updated existing float variable");
  }

  if (success) {
    m_lastCaptureStatus = "Spec threshold saved: " + std::to_string((int)specInUA) + "uA";
    m_logger->LogInfo("Saved spec threshold: " + std::to_string(specInUA) + "uA");
  }
  else {
    m_lastCaptureStatus = "Error: Failed to save spec threshold";
    m_logger->LogError("SaveSpecToDatabase: Operation failed");
  }
}

void RunPageUI::SaveAllSpecSettingsToDatabase() {
  auto& settings = AppSettings::getInstance();
  bool allSuccess = true;

  // Save spec threshold
  float specInUA = m_specThreshold * 1e6f;
  if (!settings.setFloat("ui_settings", "spec_threshold_ua", specInUA)) {
    allSuccess = false;
    m_logger->LogError("Failed to save spec threshold");
  }

  // Save spec unit
  if (!settings.setString("ui_settings", "spec_unit", m_specUnit)) {
    allSuccess = false;
    m_logger->LogError("Failed to save spec unit");
  }

  // Save pass threshold
  if (!settings.setFloat("ui_settings", "pass_threshold", m_passThreshold)) {
    allSuccess = false;
    m_logger->LogError("Failed to save pass threshold");
  }

  // Save fail threshold
  if (!settings.setFloat("ui_settings", "fail_threshold", m_failThreshold)) {
    allSuccess = false;
    m_logger->LogError("Failed to save fail threshold");
  }

  // Save exceptional threshold
  if (!settings.setFloat("ui_settings", "exceptional_threshold", m_exceptionalThreshold)) {
    allSuccess = false;
    m_logger->LogError("Failed to save exceptional threshold");
  }

  if (allSuccess) {
    m_lastCaptureStatus = "All spec settings saved successfully";
    m_logger->LogInfo("Saved all spec settings: " + std::to_string((int)specInUA) +
      m_specUnit + ", Pass:" + std::to_string(m_passThreshold) +
      "%, Fail:" + std::to_string(m_failThreshold) +
      "%, Exceptional:" + std::to_string(m_exceptionalThreshold) + "%");
  }
  else {
    m_lastCaptureStatus = "Error: Failed to save some spec settings";
    m_logger->LogError("Failed to save some spec settings");
  }
}

const char* RunPageUI::GetStatusText(float percentage) {
  if (percentage >= m_exceptionalThreshold) return "Exceptional";
  if (percentage >= m_passThreshold) return "Pass";
  if (percentage >= m_failThreshold) return "Needs Work";
  return "Fail";
}

// Add this method
void RunPageUI::RefreshSettingsFromDatabase() {
  // Reload all spec settings from database
  LoadSpecFromDatabase();

  // Update any UI elements that depend on these settings
  UpdateStatus("Settings refreshed from database");

  m_logger->LogInfo("RunPageUI: Refreshed settings from SettingsEditor changes");
}


// Add to RunPageUI.cpp

void RunPageUI::ShowRecipeLoadDialog() {
  if (m_showRecipeLoadDialog) {
    ImGui::OpenPopup("Load Recipe");
    m_showRecipeLoadDialog = false;
  }

  if (ImGui::BeginPopupModal("Load Recipe", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Select a recipe to load for execution:");
    ImGui::Separator();

    auto availableRecipes = GetAvailableRecipeFiles();

    if (availableRecipes.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
        "No recipes found in recipes/ directory.");
      ImGui::Spacing();
      ImGui::Text("Create recipes in the Recipe Management page.");
    }
    else {
      ImGui::BeginChild("RecipeList", ImVec2(300, 200), true);

      for (const auto& recipe : availableRecipes) {
        bool isSelected = (m_selectedRecipeFile == recipe);
        if (ImGui::RadioButton(recipe.c_str(), isSelected)) {
          m_selectedRecipeFile = recipe;
        }
      }

      ImGui::EndChild();
    }

    ImGui::Separator();

    bool canLoad = !m_selectedRecipeFile.empty();

    if (canLoad) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
    }
    else {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    }

    if (ImGui::Button("Load", ImVec2(80, 30)) && canLoad) {
      if (LoadRecipeFromFile(m_selectedRecipeFile)) {
        UpdateStatus("Loaded recipe: " + m_loadedRecipe.name);
        ImGui::CloseCurrentPopup();
      }
      else {
        UpdateStatus("Failed to load recipe: " + m_selectedRecipeFile, true);
      }
    }

    if (canLoad) {
      ImGui::PopStyleColor(2);
    }
    else {
      ImGui::PopStyleVar();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(80, 30))) {
      m_selectedRecipeFile = "";
      ImGui::CloseCurrentPopup();
    }

    if (!m_selectedRecipeFile.empty()) {
      ImGui::Separator();
      ImGui::Text("Selected: %s", m_selectedRecipeFile.c_str());
    }

    ImGui::EndPopup();
  }
}

std::vector<std::string> RunPageUI::GetAvailableRecipeFiles() const {
  std::vector<std::string> recipes;

  if (!std::filesystem::exists(m_recipesDirectory)) {
    return recipes;
  }

  for (const auto& entry : std::filesystem::directory_iterator(m_recipesDirectory)) {
    if (entry.is_regular_file() && entry.path().extension() == ".json") {
      std::string filename = entry.path().stem().string();
      recipes.push_back(filename);
    }
  }

  std::sort(recipes.begin(), recipes.end());
  return recipes;
}

bool RunPageUI::LoadRecipeFromFile(const std::string& filename) {
  try {
    std::string filepath = m_recipesDirectory + filename + ".json";

    if (!std::filesystem::exists(filepath)) {
      m_logger->LogError("Recipe file not found: " + filepath);
      return false;
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
      m_logger->LogError("Failed to open recipe file: " + filepath);
      return false;
    }

    nlohmann::json recipeJson;
    file >> recipeJson;
    file.close();

    if (DeserializeRecipe(recipeJson)) {
      m_loadedRecipe.filename = filename;
      m_loadedRecipe.loadedTime = std::time(nullptr);
      m_usingRecipe = true;

      // Auto-select first instance if available
      if (!m_loadedRecipe.instances.empty()) {
        m_selectedProcess = m_loadedRecipe.instances[0].GetUIDisplayName();
        ExtractSelectedProcessOperations();
      }

      m_logger->LogInfo("Loaded recipe: " + m_loadedRecipe.name +
        " with " + std::to_string(m_loadedRecipe.instances.size()) +
        " instances");
      return true;
    }

    return false;

  }
  catch (const std::exception& e) {
    m_logger->LogError("Error loading recipe: " + std::string(e.what()));
    return false;
  }
}

bool RunPageUI::DeserializeRecipe(const nlohmann::json& recipeJson) {
  try {
    // Clear current recipe
    m_loadedRecipe.instances.clear();

    // Load recipe name
    if (recipeJson.contains("name")) {
      m_loadedRecipe.name = recipeJson["name"];
    }

    // Load process instances
    if (recipeJson.contains("processInstances") && recipeJson["processInstances"].is_array()) {
      for (const auto& instanceJson : recipeJson["processInstances"]) {
        ProcessInstance instance(
          instanceJson["instanceId"],
          instanceJson["processType"]
        );

        // Load display name
        if (instanceJson.contains("displayName")) {
          instance.displayName = instanceJson["displayName"];
        }

        // Load nickname
        if (instanceJson.contains("nickname")) {
          instance.nickname = instanceJson["nickname"];
        }

        // Load parameters
        if (instanceJson.contains("parameters") && !instanceJson["parameters"].is_null()) {
          for (const auto& param : instanceJson["parameters"].items()) {
            instance.parameters[param.key()] = param.value();
          }
        }

        m_loadedRecipe.instances.push_back(instance);
      }
    }

    return true;

  }
  catch (const std::exception& e) {
    m_logger->LogError("Error deserializing recipe: " + std::string(e.what()));
    return false;
  }
}

std::vector<std::string> RunPageUI::GetRecipeInstanceDisplayNames() const {
  std::vector<std::string> names;
  names.reserve(m_loadedRecipe.instances.size());

  for (const auto& instance : m_loadedRecipe.instances) {
    names.push_back(instance.GetUIDisplayName());
  }

  return names;
}

ProcessInstance* RunPageUI::GetSelectedRecipeInstance() {
  for (auto& instance : m_loadedRecipe.instances) {
    if (instance.GetUIDisplayName() == m_selectedProcess) {
      return &instance;
    }
  }
  return nullptr;
}

std::unique_ptr<SequenceStep> RunPageUI::BuildFromRecipeInstance(ProcessInstance* instance) {
  if (!instance) {
    return nullptr;
  }

  m_logger->LogInfo("Building from recipe: " + instance->GetUIDisplayName() +
    " (Type: " + instance->processType + ")");

  // Log parameters for debugging
  m_logger->LogInfo("Recipe parameters (" + std::to_string(instance->parameters.size()) + "):");
  for (const auto& [key, value] : instance->parameters) {
    m_logger->LogInfo("  " + key + " = " + value);
  }

  if (!ProcessRegistry::GetInstance().HasProcess(instance->processType)) {
    UpdateStatus("Process type not found: " + instance->processType, true);
    return nullptr;
  }

  if (!m_promptUI) {
    UpdateStatus("UserPromptUI not available", true);
    return nullptr;
  }

  // THIS IS THE KEY CHANGE - use BuildProcessWithParameters
  auto process = ProcessRegistry::GetInstance().BuildProcessWithParameters(
    instance->processType,
    m_machineOps,
    *m_promptUI,
    instance->parameters  // Pass the recipe parameters!
  );

  if (process) {
    UpdateStatus("Built: " + instance->GetUIDisplayName());
    m_logger->LogInfo("Successfully built with recipe parameters");
    return process;
  }
  else {
    UpdateStatus("Failed to build: " + instance->processType, true);
    return nullptr;
  }
}