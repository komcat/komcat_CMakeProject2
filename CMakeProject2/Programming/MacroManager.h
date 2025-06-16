#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <thread>
#include <nlohmann/json.hpp>
#include "FeedbackUI.h"
#include "MacroEditState.h"
// Forward declaration
class MachineBlockUI;

struct SavedProgram {
  std::string name;
  std::string filePath;
  std::string description;
};

struct MacroProgram {
  int id;
  std::string name;
  std::string description;
  std::vector<SavedProgram> programs;
};

class MacroManager {
public:
  MacroManager();
  ~MacroManager();

  // Program management
  void AddProgram(const std::string& programName, const std::string& filePath);
  void ScanForPrograms();
  std::vector<std::string> GetProgramNames() const;

  // Macro creation and editing
  bool CreateMacro(const std::string& macroName, const std::string& description = "");
  bool AddProgramToMacro(const std::string& macroName, const std::string& programName);
  bool RemoveProgramFromMacro(const std::string& macroName, int index);
  bool DeleteMacro(const std::string& macroName);

  // Macro execution
  void ExecuteMacro(const std::string& macroName, std::function<void(bool)> callback = nullptr);
  void ExecuteSingleProgram(const std::string& programName);
  void StopExecution();
  bool IsExecuting() const;
  std::string GetCurrentMacro() const;

  // File operations
  void SaveMacro(const std::string& macroName, const std::string& filePath);
  void LoadMacro(const std::string& filePath);

  // Getters
  std::vector<std::string> GetMacroNames() const;
  MacroProgram* GetMacro(const std::string& macroName);

  // Dependencies
  void SetMachineBlockUI(MachineBlockUI* blockUI);

  // UI Methods
  void RenderUI();

  void ToggleWindow() { m_showWindow = !m_showWindow; }
  bool IsVisible() const { return m_showWindow; }


  void InitializeFeedbackUI();
  // New methods for text-based feedback
  void AddExecutionLog(const std::string& message);
  std::string GetCurrentTimeString();
  void ClearExecutionLog();
private:
  bool m_showWindow = false;
  std::map<std::string, SavedProgram> m_savedPrograms;
  std::map<std::string, MacroProgram> m_macros;

  MachineBlockUI* m_blockUI = nullptr;

  bool m_isExecuting;
  std::string m_currentMacro;
  int m_nextMacroId;

  // Debug and execution state
  int m_currentProgramIndex;
  std::string m_currentExecutingProgram;
  bool m_debugMode;
  bool m_forceRescanMacros;

  void ScanForMacroFiles(std::vector<std::string>& availableMacroFiles);
  void RenderDebugSection();
  void RenderExecutionStatus();
  void RenderCreateMacroSection();
  void RenderEditMacrosSection();
  void RenderLoadMacroSection();
  void RenderAvailableProgramsSection();
  void RenderEmbeddedFeedbackSection();
  // NEW: Add this member variable
  std::atomic<bool> m_stopRequested{ false };  // Graceful stop flag

  std::unique_ptr<FeedbackUI> m_macroFeedbackUI;  // Separate feedback UI for macro manager
  bool m_showEmbeddedFeedback = false;
  int m_maxLogLines = 20;  // Keep last 20 log entries

  std::vector<std::string> m_executionLog;

  // Thread-safe logging
  std::mutex m_logMutex;
  std::vector<std::string> m_pendingLogs;  // Messages from execution thread
  std::vector<std::string> m_displayLogs;  // Messages for UI display


  void ProcessPendingLogs();
  void ExecuteMacroWithIndices(const std::string& macroName, const std::vector<int>& indices);

  // NEW: Edit mode state for each macro
  std::map<std::string, bool> m_editModeStates;
  std::map<std::string, MacroEditState> m_macroEditStates;
  std::map<std::string, int> m_selectedProgramIndices; // For add dropdown

  // NEW: Edit mode management
  bool IsEditMode(const std::string& macroName) const;
  void SetEditMode(const std::string& macroName, bool editMode);
  MacroEditState& GetEditState(const std::string& macroName);

};