#pragma once

#include "ProcessBuilders.h"
#include "machine_operations.h"
#include "MockUserInteractionManager.h"
#include "logger.h"
#include "include/ui/ToolbarMenu.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>

// Class to provide a UI for selecting and running processes
class ProcessControlPanel : public ITogglableUI {
public:
  ProcessControlPanel(MachineOperations& machineOps);
  ~ProcessControlPanel();

  // ITogglableUI implementation
  bool IsVisible() const override { return m_showWindow; }
  void ToggleWindow() override { m_showWindow = !m_showWindow; }
  const std::string& GetName() const override { return m_windowTitle; }

  // Render the UI
  void RenderUI();

private:
  // References to managers
  MachineOperations& m_machineOps;
  std::unique_ptr<MockUserInteractionManager> m_uiManager;
  Logger* m_logger;

  // UI state
  bool m_showWindow = true;
  std::string m_windowTitle = "Process Control";
  std::string m_statusMessage = "Ready";
  std::string m_selectedProcess = "Initialization";
  bool m_processRunning = false;
  float m_progress = 0.0f;

  // Process handling
  std::thread m_processThread;
  std::atomic<bool> m_stopRequested;
  std::mutex m_mutex;
  std::atomic<bool> m_autoConfirm;

  // Available processes list
  std::vector<std::string> m_availableProcesses = {
      "Initialization",
      "Probing",
      "PickPlaceLeftLens",
      "PickPlaceRightLens",
      "UVCuring",
      "CompleteProcess"
  };

  // Methods
  void StartProcess(const std::string& processName);
  void StopProcess();
  void UpdateStatus(const std::string& message, bool isError = false);

  // Returns appropriate sequence for the selected process
  std::unique_ptr<SequenceStep> BuildSelectedProcess();

  // Thread function to run the process
  void ProcessThreadFunc(const std::string& processName);
};