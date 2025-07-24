#include "RunPageUI.h"
#include "imgui.h"
#include <chrono>
#include <iostream>
#include <algorithm>

RunPageUI::RunPageUI(MachineOperations& machineOps)
  : m_machineOps(machineOps),
  m_uiManager(std::make_unique<MockUserInteractionManager>()),
  m_stopRequested(false),
  m_pauseRequested(false)
{
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("RunPageUI: Initialized with UAA3 modern sequence support");
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
}

void RunPageUI::RenderColumn1() {
  ImGui::Text("Process Control");
  ImGui::Separator();

  // Render control buttons at the top
  RenderControlButtons();

  ImGui::Spacing();
  ImGui::Separator();

  // Render status area (300px height)
  RenderStatusArea();

  ImGui::Spacing();
  ImGui::Separator();

  // Render process step buttons
  RenderProcessButtons();
}

// UPDATE RunPageUI.cpp - RenderColumn2 method with auto-confirm integration

void RunPageUI::RenderColumn2() {
  ImGui::Text("Feature Panel 2");
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

  // UPDATED: Auto-confirm checkbox with UserPromptUI integration
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

    // Log the change
    std::string status = autoConfirmValue ? "enabled" : "disabled";
    UpdateStatus("Auto-confirm " + status + " for all sequences");
  }

  // Enhanced tooltip showing which systems are affected
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("Auto-confirm affects:");
    if (m_promptUI) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "✓ UAA3 sequences (UserPromptUI)");
      ImGui::Text("  - 3 second auto-confirm delay");
      ImGui::Text("  - Visual countdown display");
    }
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "✓ Legacy sequences (MockUserInteractionManager)");
    ImGui::Text("  - Immediate auto-confirm");
    ImGui::EndTooltip();
  }

  // NEW: Auto-confirm delay setting (only when auto-confirm is enabled)
  if (m_autoConfirm && m_promptUI) {
    ImGui::Spacing();
    ImGui::Text("Auto-confirm Settings:");
    ImGui::Indent();

    // Delay slider
    float delay = m_promptUI->GetAutoConfirmDelay();
    if (ImGui::SliderFloat("Delay (seconds)", &delay, 1.0f, 10.0f, "%.1f")) {
      m_promptUI->SetAutoConfirmDelay(delay);
      UpdateStatus("Auto-confirm delay set to " + std::to_string(delay) + " seconds");
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("How long to wait before automatically confirming UAA3 prompts");
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

      // Determine process type
      if (m_selectedProcess.find("UAA3_") == 0) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "Type: UAA3 Modern");
        ImGui::Text("• Uses UserPromptUI");
        ImGui::Text("• Enhanced safety checks");
        ImGui::Text("• Detailed user guidance");
        if (m_autoConfirm) {
          ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "• Auto-confirm enabled");
        }
      }
      else {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Type: Legacy");
        ImGui::Text("• Uses MockUserInteractionManager");
        ImGui::Text("• Basic user prompts");
        if (m_autoConfirm) {
          ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "• Auto-confirm enabled");
        }
      }
    }
  }

  // NEW: Auto-confirm status display
  if (ImGui::CollapsingHeader("Auto-Confirm Status")) {
    ImGui::Text("Legacy sequences: %s", m_autoConfirm ? "Enabled" : "Disabled");

    if (m_promptUI) {
      bool uaa3AutoConfirm = m_promptUI->GetAutoConfirm();
      ImGui::Text("UAA3 sequences: %s", uaa3AutoConfirm ? "Enabled" : "Disabled");

      if (uaa3AutoConfirm) {
        float delay = m_promptUI->GetAutoConfirmDelay();
        ImGui::Text("Delay: %.1f seconds", delay);

        // Show if there's a mismatch
        if (uaa3AutoConfirm != m_autoConfirm) {
          ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "⚠ Settings not synced");
          if (ImGui::Button("Sync Settings")) {
            m_promptUI->SetAutoConfirm(m_autoConfirm);
            UpdateStatus("Auto-confirm settings synchronized");
          }
        }
      }
    }
    else {
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "UAA3 sequences: N/A");
    }
  }
}

// ALSO UPDATE: StartProcess method to sync auto-confirm on sequence start
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
void RunPageUI::RenderColumn3() {
  ImGui::Text("Feature Panel 3");
  ImGui::Separator();

  // Progress information
  if (m_processRunning) {
    ImGui::Text("Process: %s", m_selectedProcess.c_str());
    ImGui::ProgressBar(m_progress, ImVec2(-1, 0));

    if (m_processPaused) {
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "PAUSED");
    }
    else {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "RUNNING");
    }
  }

  ImGui::Spacing();
  ImGui::Separator();

  // NEW: UAA3 Sequence Recommendations
  ImGui::Text("UAA3 Sequence Recommendations:");
  ImGui::Separator();

  if (ImGui::CollapsingHeader("Probing Sequences")) {
    ImGui::BulletText("UAA3_QuickProbing: Development/Testing");
    ImGui::BulletText("UAA3_ModernProbing: Standard Operations");
    ImGui::BulletText("UAA3_EnhancedProbing: Production Use");
    ImGui::BulletText("Probing: Legacy Compatibility");
  }

  if (ImGui::CollapsingHeader("Usage Guidelines")) {
    ImGui::TextWrapped("• UAA3 sequences require UserPromptUI setup");
    ImGui::TextWrapped("• Enhanced sequences include safety timeouts");
    ImGui::TextWrapped("• Legacy sequences remain for compatibility");
    ImGui::TextWrapped("• Modern sequences provide better error handling");
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

  // Pause button (yellow when process running)
  if (m_processRunning && !m_processPaused) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.7f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.5f, 0.0f, 1.0f));
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
  }

  if (ImGui::Button("PAUSE", ImVec2(buttonWidth, buttonHeight))) {
    if (m_processRunning) {
      PauseProcess();
    }
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

void RunPageUI::RenderStatusArea() {
  ImGui::Text("Status Messages");

  // Status area with scrolling (300px height)
  ImGui::BeginChild("StatusArea", ImVec2(0, 300), true, ImGuiWindowFlags_HorizontalScrollbar);

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

void RunPageUI::RenderProcessButtons() {
  ImGui::Text("Process Steps");

  // Process buttons with vertical layout
  ImGui::BeginChild("ProcessButtons", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

  const float buttonWidth = ImGui::GetContentRegionAvail().x * 0.95f;
  const float buttonHeight = 35.0f;

  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

  for (const auto& process : m_availableProcesses) {
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
      displayName = "🔧 " + process;  // Modern tool icon
    }

    if (ImGui::Button(displayName.c_str(), ImVec2(buttonWidth, buttonHeight))) {
      m_selectedProcess = process;
      // Auto-start if not running
      if (!m_processRunning) {
        StartProcess(process);
      }
    }

    // Enhanced tooltips
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::Text("Process: %s", process.c_str());
      if (isUAA3) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "UAA3 Modern Sequence");
        ImGui::Text("• Uses UserPromptUI");
        ImGui::Text("• Enhanced safety checks");
        ImGui::Text("• Detailed user guidance");
        if (!m_promptUI) {
          ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "⚠ Requires UserPromptUI setup");
        }
      }
      else {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Legacy Sequence");
        ImGui::Text("• Uses MockUserInteractionManager");
        ImGui::Text("• Basic user prompts");
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

  ImGui::PopStyleVar();
  ImGui::EndChild();
}


void RunPageUI::PauseProcess() {
  if (!m_processRunning || m_processPaused) {
    return;
  }

  m_processPaused = true;
  m_pauseRequested = true;
  UpdateStatus("Process paused");
}

void RunPageUI::StopProcess() {
  if (!m_processRunning) {
    return;
  }

  UpdateStatus("Stopping process...");
  m_stopRequested = true;

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

  m_processPaused = false;
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
  else if (m_selectedProcess == "UAA3_EnhancedProbing") {
    if (m_promptUI) {
      return UAA3ProcessBuilders::BuildEnhancedProbingSequence(m_machineOps, *m_promptUI);
    }
    else {
      UpdateStatus("UserPromptUI not available, using legacy probing", true);
      return ProcessBuilders::BuildProbingSequence(m_machineOps, *m_uiManager);
    }
  }
  else if (m_selectedProcess == "UAA3_QuickProbing") {
    if (m_promptUI) {
      return UAA3ProcessBuilders::BuildQuickProbingSequence(m_machineOps, *m_promptUI);
    }
    else {
      UpdateStatus("UserPromptUI not available, using legacy probing", true);
      return ProcessBuilders::BuildProbingSequence(m_machineOps, *m_uiManager);
    }
  }
  else if (m_selectedProcess == "UAA3_ModernNeedleCalib") {
    if (m_promptUI) {
      return UAA3ProcessBuilders::BuildModernNeedleCalibrationSequence(m_machineOps, *m_promptUI);
    }
    else {
      UpdateStatus("UserPromptUI not available, using legacy needle calibration", true);
      return ProcessBuilders::BuildNeedleXYCalibrationSequenceEnhanced(m_machineOps, *m_uiManager);
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