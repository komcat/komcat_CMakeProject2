#pragma once

#include "ProcessBuilders.h"
#include "uaa3ProcessBuilders.h"  // NEW: Include UAA3 modern sequences
#include "machine_operations.h"
#include "MockUserInteractionManager.h"
#include "Programming/UserPromptUI.h"  // NEW: Include UserPromptUI
#include "logger.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>

// Run page UI class for 3-column layout
class RunPageUI {
public:
  RunPageUI(MachineOperations& machineOps);
  ~RunPageUI();

  // Render the run page UI
  void RenderUI();

  // NEW: Add UserPromptUI support for modern UAA3 sequences
  void SetUserPromptUI(UserPromptUI* promptUI) { m_promptUI = promptUI; }

private:
  // References to managers
  MachineOperations& m_machineOps;
  std::unique_ptr<MockUserInteractionManager> m_uiManager;
  Logger* m_logger;

  // NEW: UserPromptUI for modern sequences
  UserPromptUI* m_promptUI = nullptr;

  // UI state
  std::string m_statusMessage = "Ready";
  std::string m_selectedProcess = "Initialization";
  bool m_processRunning = false;
  bool m_processPaused = false;
  float m_progress = 0.0f;

  // Process handling
  std::thread m_processThread;
  std::atomic<bool> m_stopRequested;
  std::atomic<bool> m_pauseRequested;
  std::mutex m_mutex;
  std::atomic<bool> m_autoConfirm;

  // UPDATED: Available processes list with UAA3 modern sequences
  std::vector<std::string> m_availableProcesses = {
      "Initialization",
      "InitializationParallel",

      // Legacy sequences (using MockUserInteractionManager)
      "Probing",                    // Legacy version
      "PickPlaceLeftLens",
      "PickPlaceRightLens",
      "UVCuring",
      "RejectLeftLens",
      "RejectRightLens",
      "NeedleCalibration",          // Legacy version
      "DispenseCalibration1",
      "DispenseCalibration2",
      "DispenseEpoxy1",
      "DispenseEpoxy2",

      // NEW: UAA3 Modern sequences (using UserPromptUI)
      "UAA3_ModernProbing",         // Clean modern probing
      "UAA3_EnhancedProbing",       // Detailed probing with safety checks
      "UAA3_QuickProbing",          // Minimal interaction probing
      "UAA3_ModernNeedleCalib"      // Modern needle calibration
  };

  // Status messages for scrolling display
  std::vector<std::string> m_statusHistory;
  static const size_t MAX_STATUS_HISTORY = 100;

  // Methods
  void StartProcess(const std::string& processName);
  void PauseProcess();
  void StopProcess();
  void UpdateStatus(const std::string& message, bool isError = false);

  // UI rendering methods
  void RenderColumn1();
  void RenderColumn2();
  void RenderColumn3();
  void RenderControlButtons();
  void RenderStatusArea();
  void RenderProcessButtons();

  // Process management
  std::unique_ptr<SequenceStep> BuildSelectedProcess();
  void ProcessThreadFunc(const std::string& processName);
};