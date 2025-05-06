#include "ProcessControlPanel.h"
#include "imgui.h"
#include <chrono>
#include <iostream>

ProcessControlPanel::ProcessControlPanel(MachineOperations& machineOps)
  : m_machineOps(machineOps),
  m_uiManager(std::make_unique<MockUserInteractionManager>()),
  m_stopRequested(false)
{
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("ProcessControlPanel: Initialized");
}

ProcessControlPanel::~ProcessControlPanel() {
  // Ensure any running process is stopped cleanly
  StopProcess();
  m_logger->LogInfo("ProcessControlPanel: Destroyed");
}

void ProcessControlPanel::RenderUI() {
  if (!m_showWindow) return;

  // Get display size for setting the initial window size
  ImGuiIO& io = ImGui::GetIO();
  ImVec2 displaySize = io.DisplaySize;

  // Set initial window size to 75% width and 65% height, but only on first use
  ImGui::SetNextWindowSize(ImVec2(displaySize.x * 0.75f, displaySize.y * 0.65f), ImGuiCond_FirstUseEver);

  // Begin window with resizable flag only - no auto resize
  ImGui::Begin("Process Control Panel", &m_showWindow);

  // Title with larger font
  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("Process Control");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Separator();

  // Create a two-column layout
  const float windowWidth = ImGui::GetContentRegionAvail().x;
  const float columnWidth = windowWidth * 0.48f; // Each column gets almost half the window
  const float buttonWidth = columnWidth - 10.0f;
  const float buttonHeight = 40.0f;

  // Left column - Process buttons
  ImGui::BeginChild("ProcessButtons", ImVec2(columnWidth, 350), true, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);
  ImGui::Text("Available Processes:");
  ImGui::Separator();

  // Style for process selection buttons
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

  for (const auto& process : m_availableProcesses) {
    // Highlight the selected process with a green button
    if (m_selectedProcess == process) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 0.2f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.8f, 0.3f, 1.0f));
    }
    else {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    }

    // Create a button for each process
    if (ImGui::Button(process.c_str(), ImVec2(buttonWidth, buttonHeight))) {
      m_selectedProcess = process;
      // If we're not already running a process, start this one right away
      if (!m_processRunning) {
        StartProcess(process);
      }
    }

    ImGui::PopStyleColor(2);

    // Spacing between buttons
    ImGui::Spacing();
  }

  ImGui::PopStyleVar();
  ImGui::EndChild();

  ImGui::SameLine();

  // Right column - Process description
  ImGui::BeginChild("ProcessDescription", ImVec2(columnWidth, 350), true, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);
  ImGui::Text("Process Description:");
  ImGui::Separator();

  if (m_selectedProcess == "Initialization") {
    ImGui::TextWrapped("Initializes all hardware by moving devices to home positions, releasing grippers, and setting up pneumatics.");
    ImGui::Spacing();

    float bulletSpacing = 24.0f; // Increased spacing for better readability
    float indent = 10.0f;

    // Styled bullets
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Moves gantry, hex-left, and hex-right to safe positions");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Releases left and right grippers");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Retracts pneumatic slides");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Activates base vacuum");
  }
  else if (m_selectedProcess == "Probing") {
    ImGui::TextWrapped("Positions the gantry to inspect key components and waits for user confirmation at each step.");
    ImGui::Spacing();

    float bulletSpacing = 24.0f;
    float indent = 10.0f;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Moves gantry to sled position");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Waits for user to confirm sled position");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Moves gantry to PIC position");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Waits for user to confirm PIC position");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Returns to safe position");
  }
  else if (m_selectedProcess == "PickPlaceLeftLens") {
    ImGui::TextWrapped("Controls hex-left to pick up a lens and place it in position.");
    ImGui::Spacing();

    float bulletSpacing = 24.0f;
    float indent = 10.0f;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Moves to approach position");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Picks up lens with gripper");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Moves to placement position");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Places lens");
  }
  else if (m_selectedProcess == "PickPlaceRightLens") {
    ImGui::TextWrapped("Controls hex-right to pick up a lens and place it in position.");
    ImGui::Spacing();

    float bulletSpacing = 24.0f;
    float indent = 10.0f;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Moves to approach position");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Picks up lens with gripper");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Moves to placement position");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Places lens");
  }
  else if (m_selectedProcess == "UVCuring") {
    ImGui::TextWrapped("Positions the UV device and performs the curing process.");
    ImGui::Spacing();

    float bulletSpacing = 24.0f;
    float indent = 10.0f;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Moves gantry to UV position");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Extends UV head");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Activates UV light for the curing period");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Retracts UV head");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Releases grippers and returns to safe position");
  }
  else if (m_selectedProcess == "CompleteProcess") {
    ImGui::TextWrapped("Performs the complete end-to-end assembly process, including all the steps above in sequence.");
    ImGui::Spacing();

    float bulletSpacing = 24.0f;
    float indent = 10.0f;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("System initialization");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Component probing and inspection");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Left lens pick and placement");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Right lens pick and placement");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("UV curing");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bulletSpacing - ImGui::GetTextLineHeight());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "•"); ImGui::SameLine();
    ImGui::Text("Return to safe positions");
  }

  ImGui::EndChild();

  ImGui::Separator();

  // Status section
  ImGui::Text("Status: %s", m_statusMessage.c_str());

  // Progress bar (if process is running)
  if (m_processRunning) {
    ImGui::ProgressBar(m_progress, ImVec2(-1, 0));
  }

  // Auto-confirm for user interactions
  bool autoConfirmValue = m_autoConfirm; // Copy to a local variable for ImGui
  if (ImGui::Checkbox("Auto-confirm User Interactions", &autoConfirmValue)) {
    // Update the class member when changed
    m_autoConfirm = autoConfirmValue;
    m_uiManager->SetAutoConfirm(m_autoConfirm);
  }

  // Start/Stop button at the bottom
  if (m_processRunning) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); // Red button
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));

    if (ImGui::Button("Stop Process", ImVec2(-1, 50))) {
      StopProcess();
    }

    ImGui::PopStyleColor(3);

    // If process is running and user interactions are needed, show controls
    if (m_uiManager->IsWaitingForConfirmation() && !m_autoConfirm) {
      ImGui::Text("User confirmation needed: %s", m_uiManager->GetLastMessage().c_str());

      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 0.2f, 1.0f)); // Green button
      if (ImGui::Button("Confirm", ImVec2(120, 0))) {
        m_uiManager->ConfirmationReceived(true);
      }
      ImGui::PopStyleColor();

      ImGui::SameLine();

      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f)); // Red button
      if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        m_uiManager->ConfirmationReceived(false);
      }
      ImGui::PopStyleColor();
    }
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 0.2f, 1.0f)); // Green button
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.8f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.6f, 0.1f, 1.0f));

    if (ImGui::Button("Start Process", ImVec2(-1, 50))) {
      StartProcess(m_selectedProcess);
    }

    ImGui::PopStyleColor(3);
  }

  ImGui::End();
}

void ProcessControlPanel::StartProcess(const std::string& processName) {
  if (m_processRunning) {
    m_logger->LogWarning("ProcessControlPanel: Process already running");
    return;
  }

  m_processRunning = true;
  m_progress = 0.0f;
  m_stopRequested = false;

  UpdateStatus("Starting process: " + processName);

  // Start the process in a separate thread
  m_processThread = std::thread(&ProcessControlPanel::ProcessThreadFunc, this, processName);
  m_processThread.detach(); // Let it run independently
}

void ProcessControlPanel::StopProcess() {
  if (!m_processRunning) {
    return;
  }

  UpdateStatus("Stopping process...");
  m_stopRequested = true;

  // Wait for the process to complete (with a timeout)
  int timeoutCounter = 0;
  while (m_processRunning && timeoutCounter < 100) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    timeoutCounter++;
  }

  // If it's still running, we assume it's hung and just set the flag
  if (m_processRunning) {
    m_processRunning = false;
    UpdateStatus("Process forcibly terminated", true);
  }
  else {
    UpdateStatus("Process stopped");
  }
}

void ProcessControlPanel::UpdateStatus(const std::string& message, bool isError) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_statusMessage = message;

  if (isError) {
    m_logger->LogError("ProcessControlPanel: " + message);
  }
  else {
    m_logger->LogInfo("ProcessControlPanel: " + message);
  }
}

std::unique_ptr<SequenceStep> ProcessControlPanel::BuildSelectedProcess() {
  if (m_selectedProcess == "Initialization") {
    return ProcessBuilders::BuildInitializationSequence(m_machineOps);
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
    return ProcessBuilders::BuildUVCuringSequence(m_machineOps);
  }
  else if (m_selectedProcess == "CompleteProcess") {
    return ProcessBuilders::BuildCompleteProcessSequence(m_machineOps, *m_uiManager);
  }

  // Default fallback
  return ProcessBuilders::BuildInitializationSequence(m_machineOps);
}

void ProcessControlPanel::ProcessThreadFunc(const std::string& processName) {
  try {
    // Build the process
    std::unique_ptr<SequenceStep> sequence = BuildSelectedProcess();
    if (!sequence) {
      UpdateStatus("Failed to create process: " + processName, true);
      m_processRunning = false;
      return;
    }

    // Number of operations to track progress
    const auto& operations = sequence->GetOperations();
    size_t totalOperations = operations.size();
    size_t completedOperations = 0;

    // Set up step completion tracking
    bool processSuccess = false;
    bool processComplete = false;

    sequence->SetCompletionCallback([&](bool success) {
      processSuccess = success;
      processComplete = true;
    });

    // Create a progress tracking wrapper class for operations
    class ProgressTrackingOperation : public SequenceOperation {
    public:
      ProgressTrackingOperation(std::shared_ptr<SequenceOperation> original,
        ProcessControlPanel* panel, size_t& completed, size_t total)
        : m_original(original), m_panel(panel),
        m_completedOps(completed), m_totalOps(total) {
      }

      bool Execute(MachineOperations& ops) override {
        m_panel->UpdateStatus("Executing: " + GetDescription());
        bool result = m_original->Execute(ops);
        m_completedOps++;
        // Update progress (thread-safe because atomic)
        m_panel->m_progress = static_cast<float>(m_completedOps) / m_totalOps;
        return result && !m_panel->m_stopRequested;
      }

      std::string GetDescription() const override {
        return m_original->GetDescription();
      }

    private:
      std::shared_ptr<SequenceOperation> m_original;
      ProcessControlPanel* m_panel;
      size_t& m_completedOps;
      size_t m_totalOps;
    };

    // Replace each operation with a tracking wrapper
    std::vector<std::shared_ptr<SequenceOperation>> trackedOperations;
    for (const auto& operation : operations) {
      trackedOperations.push_back(
        std::make_shared<ProgressTrackingOperation>(
          operation, this, completedOperations, totalOperations
        )
      );
    }

    // Create a new sequence with the tracked operations
    auto trackingSequence = std::make_unique<SequenceStep>("Tracking:" + processName, m_machineOps);
    for (const auto& op : trackedOperations) {
      trackingSequence->AddOperation(op);
    }

    // Set the completion callback on the new sequence
    trackingSequence->SetCompletionCallback([&](bool success) {
      processSuccess = success;
      processComplete = true;
    });

    // Replace the original sequence with our tracking sequence
    sequence = std::move(trackingSequence);

    // Execute the sequence
    UpdateStatus("Executing process: " + processName);
    sequence->Execute();

    // Wait for completion callback or timeout
    int timeoutMs = 60000; // 60 second timeout
    auto startTime = std::chrono::steady_clock::now();

    while (!processComplete) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      // Check for timeout
      auto currentTime = std::chrono::steady_clock::now();
      auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        currentTime - startTime).count();

      if (elapsedMs > timeoutMs || m_stopRequested) {
        UpdateStatus("Process timed out or was stopped", true);
        break;
      }
    }

    // Update status based on process result
    if (processComplete) {
      if (processSuccess) {
        UpdateStatus("Process completed successfully");
      }
      else {
        UpdateStatus("Process failed", true);
      }
    }
  }
  catch (const std::exception& e) {
    UpdateStatus("Exception during process execution: " + std::string(e.what()), true);
  }
  catch (...) {
    UpdateStatus("Unknown exception during process execution", true);
  }

  // Always mark process as not running when we exit
  m_processRunning = false;
  m_progress = 0.0f;
}