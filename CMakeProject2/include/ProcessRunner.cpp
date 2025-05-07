#include "ProcessBuilders.h"
#include "MockUserInteractionManager.h"
#include "logger.h"
#include <thread>
#include <memory>
#include <iostream>

// Simple class to manage running processes
class ProcessRunner {
public:
  ProcessRunner(MachineOperations& machineOps)
    : m_machineOps(machineOps),
    m_uiManager(std::make_unique<MockUserInteractionManager>()),
    m_running(false),
    m_lastResult(false)
  {
    Logger::GetInstance()->LogInfo("ProcessRunner: Initialized");
  }

  ~ProcessRunner() {
    // Ensure any running process is stopped cleanly
    Stop();
  }

  // Run a specific process (runs in a separate thread)
  bool RunProcess(const std::string& processName) {
    if (m_running) {
      Logger::GetInstance()->LogError("ProcessRunner: Already running a process");
      return false;
    }

    m_running = true;
    m_lastResult = false;

    // Build the appropriate process sequence
    std::unique_ptr<SequenceStep> sequence;

    if (processName == "Initialization") {
      sequence = ProcessBuilders::BuildInitializationSequence(m_machineOps);
    }
    else if (processName == "Probing") {
      sequence = ProcessBuilders::BuildProbingSequence(m_machineOps, *m_uiManager);
    }
    else if (processName == "PickPlaceLeftLens") {
      sequence = ProcessBuilders::BuildPickPlaceLeftLensSequence(m_machineOps, *m_uiManager);
    }
    else if (processName == "PickPlaceRightLens") {
      sequence = ProcessBuilders::BuildPickPlaceRightLensSequence(m_machineOps, *m_uiManager);
    }
    else if (processName == "UVCuring") {
      sequence = ProcessBuilders::BuildUVCuringSequence(m_machineOps, *m_uiManager);
    }
    else if (processName == "CompleteProcess") {
      sequence = ProcessBuilders::BuildCompleteProcessSequence(m_machineOps, *m_uiManager);
    }
    else {
      Logger::GetInstance()->LogError("ProcessRunner: Unknown process: " + processName);
      m_running = false;
      return false;
    }

    // Set completion callback
    sequence->SetCompletionCallback([this](bool success) {
      m_lastResult = success;
      m_running = false;
      Logger::GetInstance()->LogInfo("ProcessRunner: Process completed with " +
        std::string(success ? "success" : "failure"));
    });

    // Run the sequence in a new thread
    m_processThread = std::thread([sequence = std::move(sequence)]() {
      sequence->Execute();
    });

    // Detach the thread so it can continue running independently
    m_processThread.detach();

    return true;
  }

  // Stop the current process
  void Stop() {
    if (!m_running) {
      return;
    }

    // In a real implementation, we would signal the process to stop gracefully
    // For this example, we'll just wait for it to finish
    Logger::GetInstance()->LogInfo("ProcessRunner: Waiting for process to complete...");

    while (m_running) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  // Check if a process is running
  bool IsRunning() const {
    return m_running;
  }

  // Get the result of the last process
  bool GetLastResult() const {
    return m_lastResult;
  }

  // Confirm the current user interaction (for testing)
  void ConfirmUserInteraction(bool confirm = true) {
    if (m_uiManager->IsWaitingForConfirmation()) {
      m_uiManager->ConfirmationReceived(confirm);
    }
  }

  // Set auto-confirm mode (for testing)
  void SetAutoConfirm(bool autoConfirm) {
    m_uiManager->SetAutoConfirm(autoConfirm);
  }

private:
  MachineOperations& m_machineOps;
  std::unique_ptr<MockUserInteractionManager> m_uiManager;
  std::thread m_processThread;
  bool m_running;
  bool m_lastResult;
};

// Example usage (could be in main.cpp or similar)
void ProcessRunnerExample() {
  // This is just an example and should be replaced with proper initialization
  // in your actual application. The code below will not compile as-is
  // because we don't have access to the actual implementations.

  // Comment out or replace these lines in your actual implementation
  /*
  // Create dependencies
  MotionConfigManager configManager("motion_config.json");
  MotionControlLayer motionLayer(configManager, piManager, acsControllerManager);
  PIControllerManager piManager;
  EziIOManager ioManager;
  PneumaticManager pneumaticManager;

  // Create machine operations with correct parameters
  MachineOperations machineOps(motionLayer, piManager, ioManager, pneumaticManager);

  // Create the process runner
  ProcessRunner runner(machineOps);

  // Enable auto-confirm for testing
  runner.SetAutoConfirm(true);

  // Run the initialization process
  std::cout << "Running initialization process..." << std::endl;
  runner.RunProcess("Initialization");

  // Wait for the process to complete
  while (runner.IsRunning()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // Check the result
  if (runner.GetLastResult()) {
      std::cout << "Initialization successful!" << std::endl;

      // Run the next process, etc.
      // runner.RunProcess("Probing");
  }
  else {
      std::cout << "Initialization failed!" << std::endl;
  }

  // Process example complete
  std::cout << "Process example complete" << std::endl;
  */

  // Instead, use this placeholder to indicate how to use the ProcessRunner
  std::cout << "ProcessRunner usage example:" << std::endl;
  std::cout << "1. Create a ProcessRunner with your MachineOperations instance" << std::endl;
  std::cout << "2. Call ProcessRunner::RunProcess() with the process name" << std::endl;
  std::cout << "3. Wait for completion or handle user interactions" << std::endl;
}