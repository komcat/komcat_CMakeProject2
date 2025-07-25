#include "RunPageUI.h"
#include "imgui.h"
#include <chrono>
#include <iostream>
#include <algorithm>

// UPDATE RunPageUI.cpp - Constructor
RunPageUI::RunPageUI(MachineOperations& machineOps)
  : m_machineOps(machineOps),
  m_uiManager(std::make_unique<MockUserInteractionManager>()),
  m_stopRequested(false),
  m_pauseRequested(false)
{
  m_logger = Logger::GetInstance();

  // NEW: Initialize filter manager
  m_filterManager = std::make_unique<ProcessFilterManager>();

  // Set up callback for when filters change
  m_filterManager->SetOnFilterChangedCallback([this]() {
    OnFilterChanged();
  });

  m_logger->LogInfo("RunPageUI: Initialized with process filtering support");
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

// NEW: Render single-line running status
void RunPageUI::RenderRunningStatus() {
  // Calculate the available width for the status bar
  float availableWidth = ImGui::GetContentRegionAvail().x;
  float statusHeight = 60.0f; // Height for the status bar

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
  float textX = (availableWidth - textSize.x) * 0.5f;
  if (textX > 0) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textX);
  }

  ImGui::Text("%s", statusText.c_str());

  ImGui::PopStyleColor(); // Pop text color
  ImGui::SetWindowFontScale(1.0f); // Reset font scale

  // Move cursor past the status bar
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 15.0f); // Bottom padding
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

// UPDATE: RenderColumn2 - Simplified for custom presets only
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

  // UAA3 system status
  ImGui::Text("UAA3 System Status:");
  ImGui::Indent();

  if (m_promptUI) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Modern UserPromptUI Available");
    ImGui::Text("  - UAA3 sequences enabled");
    ImGui::Text("  - Enhanced user prompts");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "⚠ UserPromptUI Not Available");
    ImGui::Text("  - Using legacy sequences only");
  }

  ImGui::Unindent();
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

    std::string status = autoConfirmValue ? "enabled" : "disabled";
    UpdateStatus("Auto-confirm " + status + " for all sequences");
  }

  // Enhanced tooltip
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("Auto-confirm affects:");
    if (m_promptUI) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "✓ UAA3 sequences (UserPromptUI)");
      ImGui::Text("  - 3 second auto-confirm delay");
    }
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "✓ Legacy sequences");
    ImGui::EndTooltip();
  }

  // Auto-confirm delay setting
  if (m_autoConfirm && m_promptUI) {
    ImGui::Spacing();
    ImGui::Text("Auto-confirm Settings:");
    ImGui::Indent();

    float delay = m_promptUI->GetAutoConfirmDelay();
    if (ImGui::SliderFloat("Delay (seconds)", &delay, 1.0f, 10.0f, "%.1f")) {
      m_promptUI->SetAutoConfirmDelay(delay);
      UpdateStatus("Auto-confirm delay set to " + std::to_string(delay) + " seconds");
    }

    ImGui::Unindent();
  }

  ImGui::Separator();

  // Device connection status
  if (ImGui::CollapsingHeader("Device Status")) {
    std::vector<std::string> devices = { "gantry-main", "hex-left", "hex-right" };

    for (const auto& deviceName : devices) {
      bool isConnected = m_machineOps.IsDeviceConnected(deviceName);
      ImGui::Text("%s: %s", deviceName.c_str(),
        isConnected ? "Connected" : "Not Connected");
    }
  }

  // Process type information
  if (ImGui::CollapsingHeader("Process Information")) {
    if (!m_selectedProcess.empty()) {
      ImGui::Text("Selected: %s", m_selectedProcess.c_str());

      if (m_selectedProcess.find("UAA3_") == 0) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "Type: UAA3 Modern");
        ImGui::Text("• Uses UserPromptUI");
        ImGui::Text("• Enhanced safety checks");
        if (m_autoConfirm) {
          ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "• Auto-confirm enabled");
        }
      }
      else {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Type: Legacy");
        ImGui::Text("• Uses MockUserInteractionManager");
        if (m_autoConfirm) {
          ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "• Auto-confirm enabled");
        }
      }
    }
  }
}

void RunPageUI::RenderColumn3() {
  ImGui::Text("Status & Information");
  ImGui::Separator();

  // Process filter status at top
  auto currentList = GetCurrentProcessList();
  auto totalList = m_filterManager->GetAllAvailableProcesses();

  ImGui::Text("Process Filter Status:");
  ImGui::Text("Showing %zu of %zu sequences", currentList.size(), totalList.size());

  if (currentList.size() < totalList.size()) {
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Some processes are hidden by filters");
  }

  ImGui::Separator();

  // Progress information 
  if (m_processRunning) {
    ImGui::Text("Process: %s", m_selectedProcess.c_str());
    ImGui::ProgressBar(m_progress, ImVec2(-1, 0));

    if (m_processPaused) {
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "⏸ PAUSED");
      ImGui::Text("Click RESUME to continue execution");
    }
    else {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "▶ RUNNING");
      ImGui::Text("Process is executing normally");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Process Controls:");
    ImGui::BulletText("PAUSE: Temporarily halt execution");
    ImGui::BulletText("RESUME: Continue from pause point");
    ImGui::BulletText("STOP: Terminate process completely");
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "⏹ IDLE");
    ImGui::Text("No process running");

    ImGui::Spacing();
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

  // Status Messages at bottom of Column3
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

// UPDATED: BuildSelectedProcess with UAA3 support
std::unique_ptr<SequenceStep> RunPageUI::BuildSelectedProcess() {
  // Legacy sequences (using MockUserInteractionManager)
  if (m_selectedProcess == "Initialization") {
    return ProcessBuilders::BuildInitializationSequence(m_machineOps);
  }
  else if (m_selectedProcess == "InitializationParallel") {
    return ProcessBuilders::BuildInitializationSequenceParallel(m_machineOps);
  }
  else if (m_selectedProcess == "Probing") {
    return ProcessBuilders::BuildProbingSequence(m_machineOps, *m_uiManager);
  }
  else if (m_selectedProcess == "PickPlaceLeftLens") {
    return ProcessBuilders::BuildPickPlaceLeftLensSequence(m_machineOps, *m_uiManager);
  }
  else if (m_selectedProcess == "PickPlaceRightLens") {
    return ProcessBuilders::BuildPickPlaceRightLensSequence(m_machineOps, *m_uiManager);
  }
  else if (m_selectedProcess == "UVCuring") {
    return ProcessBuilders::BuildUVCuringSequence(m_machineOps, *m_uiManager);
  }
  else if (m_selectedProcess == "RejectLeftLens") {
    return ProcessBuilders::RejectLeftLensSequence(m_machineOps, *m_uiManager);
  }
  else if (m_selectedProcess == "RejectRightLens") {
    return ProcessBuilders::RejectRightLensSequence(m_machineOps, *m_uiManager);
  }
  else if (m_selectedProcess == "NeedleCalibration") {
    return ProcessBuilders::BuildNeedleXYCalibrationSequenceEnhanced(m_machineOps, *m_uiManager);
  }
  else if (m_selectedProcess == "DispenseCalibration1") {
    return ProcessBuilders::BuildDispenseCalibrationSequence(m_machineOps, *m_uiManager);
  }
  else if (m_selectedProcess == "DispenseCalibration2") {
    return ProcessBuilders::BuildDispenseCalibration2Sequence(m_machineOps, *m_uiManager);
  }
  else if (m_selectedProcess == "DispenseEpoxy1") {
    return ProcessBuilders::BuildDispenseEpoxy1Sequence(m_machineOps, *m_uiManager);
  }
  else if (m_selectedProcess == "DispenseEpoxy2") {
    return ProcessBuilders::BuildDispenseEpoxy2Sequence(m_machineOps, *m_uiManager);
  }

  // NEW: UAA3 Modern sequences (using UserPromptUI)
  else if (m_selectedProcess == "UAA3_ModernProbing") {
    if (m_promptUI) {
      return UAA3ProcessBuilders::BuildModernProbingSequence(m_machineOps, *m_promptUI);
    }
    else {
      // Fallback to legacy if UserPromptUI not available
      UpdateStatus("UserPromptUI not available, using legacy probing", true);
      return ProcessBuilders::BuildProbingSequence(m_machineOps, *m_uiManager);
    }
  }
  

  // Default fallback
  return ProcessBuilders::BuildInitializationSequence(m_machineOps);
}

void RunPageUI::ProcessThreadFunc(const std::string& processName) {
  try {
    auto sequence = BuildSelectedProcess();
    if (!sequence) {
      UpdateStatus("Failed to build process sequence", true);
      m_processRunning = false;
      return;
    }

    const auto& operations = sequence->GetOperations();
    size_t totalOps = operations.size();

    for (size_t i = 0; i < totalOps && !m_stopRequested; ++i) {
      // Handle pause
      while (m_pauseRequested && !m_stopRequested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      if (m_stopRequested) break;

      UpdateStatus("Executing: " + operations[i]->GetDescription());

      bool success = operations[i]->Execute(m_machineOps);
      if (!success && !m_stopRequested) {
        UpdateStatus("Operation failed: " + operations[i]->GetDescription(), true);
        break;
      }

      m_progress = static_cast<float>(i + 1) / static_cast<float>(totalOps);
    }

    if (m_stopRequested) {
      UpdateStatus("Process stopped by user");
    }
    else {
      UpdateStatus("Process completed successfully");
    }

  }
  catch (const std::exception& e) {
    UpdateStatus("Process error: " + std::string(e.what()), true);
  }

  m_processRunning = false;
  m_processPaused = false;
  m_progress = 0.0f;
}

// NEW: Get current filtered process list
std::vector<std::string> RunPageUI::GetCurrentProcessList() const {
  return m_filterManager->GetFilteredProcessList();
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