// script_runner.h
#pragma once

#include "include/script/script_executor.h"
#include "include/machine_operations.h"
#include "include/ui/VerticalToolbarMenu.h"
#include "include/script/script_print_viewer.h"
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <chrono>
#include <map>

class ScriptRunner : public IHierarchicalTogglableUI {
public:
  // Slot info structure
  struct SlotInfo {
    std::string scriptPath;      // Path to .aas file
    std::string displayName;     // Custom display name for the slot
    std::string description;     // Description of what the script does
    bool enabled;               // Whether the slot is enabled
    bool isExecuting;           // Whether currently executing
    std::unique_ptr<ScriptExecutor> executor;
    std::string lastError;
    ScriptExecutor::ExecutionState lastState;

    SlotInfo() : enabled(false), isExecuting(false),
      lastState(ScriptExecutor::ExecutionState::Idle) {
    }
  };

  ScriptRunner(MachineOperations& machineOps, ScriptPrintViewer* printViewer);
  ~ScriptRunner();

  // Render the UI
  void RenderUI();

  // IHierarchicalTogglableUI implementation
  bool IsVisible() const override { return m_isVisible; }
  void ToggleWindow() override { m_isVisible = !m_isVisible; }
  const std::string& GetName() const override { return m_name; }
  bool HasChildren() const override { return false; }
  const std::vector<std::shared_ptr<IHierarchicalTogglableUI>>& GetChildren() const override { return m_children; }

  // Script slot management
  void LoadConfiguration();
  void SaveConfiguration();
  void AssignScriptToSlot(int slotIndex, const std::string& scriptPath, const std::string& displayName);
  void ClearSlot(int slotIndex);
  void ExecuteSlot(int slotIndex);
  void StopSlot(int slotIndex);

  // Configuration file path
  static constexpr const char* CONFIG_FILE = "scripts/script_runner_config.json";
  static constexpr int NUM_SLOTS = 20;

  // Get the number of slots to display
  int GetVisibleSlotCount() const { return m_visibleSlotCount; }

private:
  // UI rendering helpers
  void RenderSlot(int slotIndex);
  void RenderEditDialog();
  void RefreshFileList();

  // Execution handling
  void OnSlotExecutionStateChanged(int slotIndex, ScriptExecutor::ExecutionState state);
  void OnSlotLog(int slotIndex, const std::string& message);

  // File operations
  bool LoadScriptContent(const std::string& scriptPath, std::string& content);
  std::vector<std::string> GetScriptFiles(const std::string& directory = "scripts");

  // State
  bool m_isVisible;
  std::string m_name;
  std::vector<std::shared_ptr<IHierarchicalTogglableUI>> m_children;

  MachineOperations& m_machineOps;
  std::array<SlotInfo, NUM_SLOTS> m_slots;

  // UI state
  bool m_showEditDialog;
  int m_editingSlotIndex;
  char m_editNameBuffer[256];
  char m_editPathBuffer[512];
  char m_editDescriptionBuffer[256];
  std::vector<std::string> m_availableScripts;
  int m_visibleSlotCount;
  bool m_showSettings;

  // Execution tracking
  std::map<int, std::chrono::steady_clock::time_point> m_executionStartTimes;

  // Execution log tracking for each slot
  std::map<int, std::vector<std::string>> m_executionLogs;

  // Current operation for each slot
  std::map<int, std::string> m_currentOperations;

  // Selected log slot for display
  int m_selectedLogSlot = -1;

  // Progress panel rendering
  void RenderProgressPanel();

  ScriptPrintViewer* m_printViewer;  // <-- ADD THIS LINE

  // ... existing members ...
  std::mutex m_printMutex;  // Add this line

};