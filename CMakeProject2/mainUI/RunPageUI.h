#pragma once

#include "ProcessBuilders.h"
#include "uaa3_process_builders.h"  // Include UAA3 modern sequences
#include "machine_operations.h"
#include "MockUserInteractionManager.h"
#include "Programming/UserPromptUI.h"  // Include UserPromptUI
#include "ProcessFilterManager.h"  // Simple custom preset filter manager
#include "include/ui/OperationsDisplayUI.h"  // Add OperationsDisplayUI
#include "logger.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <chrono>

// Run page UI class for 3-column layout
class RunPageUI {
public:
  RunPageUI(MachineOperations& machineOps);
  ~RunPageUI();

  // Render the run page UI
  void RenderUI();
  void SetUserPromptUI(UserPromptUI* promptUI) { m_promptUI = promptUI; }

private:
  // References to managers
  MachineOperations& m_machineOps;
  std::unique_ptr<MockUserInteractionManager> m_uiManager;
  Logger* m_logger;

  // UserPromptUI for modern sequences
  UserPromptUI* m_promptUI = nullptr;

  // Operations Display UI for detail results
  std::unique_ptr<OperationsDisplayUI> m_operationsDisplayUI;

  // UI state
  std::string m_statusMessage = "Ready";
  std::string m_selectedProcess = "Initialization";
  bool m_processRunning = false;
  bool m_processPaused = false;
  float m_progress = 0.0f;

  // Column3 tab state
  int m_column3TabIndex = 0; // 0 = Status, 1 = Detail Results

  // Process handling
  std::thread m_processThread;
  std::atomic<bool> m_stopRequested;
  std::atomic<bool> m_pauseRequested;
  std::mutex m_mutex;
  std::atomic<bool> m_autoConfirm;

  // Simple filter management
  std::unique_ptr<ProcessFilterManager> m_filterManager;
  bool m_showFilterWindow = false;

  // NEW: Completed steps tracking
  struct CompletedProcess {
    std::string processName;
    std::string dateTime;
    std::string duration;
  };
  std::vector<CompletedProcess> m_completedSteps;
  static const size_t MAX_COMPLETED_STEPS = 50;
  std::chrono::steady_clock::time_point m_processStartTime;

  // Filter integration methods
  void ShowFilterConfiguration() { m_showFilterWindow = true; }
  std::vector<std::string> GetCurrentProcessList() const;
  void OnFilterChanged();

  // Status messages for scrolling display
  std::vector<std::string> m_statusHistory;
  static const size_t MAX_STATUS_HISTORY = 100;

  // Methods
  void StartProcess(const std::string& processName);
  void PauseProcess();
  void ResumeProcess();
  void StopProcess();
  void UpdateStatus(const std::string& message, bool isError = false);

  // NEW: Completed steps management
  void AddCompletedStep(const std::string& stepName, const std::string& duration);
  void ClearCompletedSteps();

  // UI rendering methods
  void RenderColumn1();
  void RenderColumn2();
  void RenderColumn3();
  void RenderStatusTab();          // NEW: Status tab content
  void RenderDetailResultsTab();   // NEW: Detail results tab content
  void RenderControlButtons();
  void RenderStatusArea();
  void RenderProcessButtons();
  void RenderRunningStatus();
  void RenderCompletedSteps();     // NEW: Render completed steps list

  // Process management
  std::unique_ptr<SequenceStep> BuildSelectedProcess();
  void ProcessThreadFunc(const std::string& processName);
};