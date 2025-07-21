// MacroPanelUI.h - Cleaned and optimized
#pragma once

#include "imgui.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <filesystem>
#include "Programming/UserPromptUI.h"
#include "ProgramCardRenderer.h"
// Forward declarations ONLY - no includes in header
class MacroManager;
class MachineBlockUI;
class CameraManager;
class MacroPanelCameraHandler;

struct MacroProgramItem {
  int id;
  bool selected = false;
  std::string name;
  std::string filePath;
  bool canRun = true;
};

class MacroPanelUI {
public:
  MacroPanelUI(CameraManager* cameraManager = nullptr);
  ~MacroPanelUI();

  void RenderMacroPanel();
  void SetMacroManager(MacroManager* macroManager);
  void SetMachineBlockUI(MachineBlockUI* blockUI);
  void SetCameraManager(CameraManager* cameraManager);
  void SetPromptUI(UserPromptUI* promptUI) { m_promptUI = promptUI; }

  void SetVisible(bool visible) { m_showWindow = visible; }
  bool IsVisible() const { return m_showWindow; }

private:
  // Core references
  bool m_showWindow = true;
  MacroManager* m_macroManager = nullptr;
  MachineBlockUI* m_blockUI = nullptr;
  UserPromptUI* m_promptUI = nullptr;
  std::unique_ptr<MacroPanelCameraHandler> m_cameraHandler;
  std::unique_ptr<ProgramCardRenderer> m_cardRenderer;
  // UI State
  std::string m_currentMacroName;
  std::vector<MacroProgramItem> m_programItems;
  bool m_isExecuting = false;
  bool m_isPaused = false;
  int m_currentProgramIndex = -1;

  // Input buffers
  char m_macroNameBuffer[256] = "";
  char m_macroDescBuffer[512] = "";
  int m_selectedMacroIndex = 0;
  int m_selectedProgramIndex = 0;

  // Data
  std::vector<std::string> m_availableMacros;
  std::vector<std::string> m_availablePrograms;

  // File dialog state
  bool m_showLoadDialog = false;
  bool m_showSaveDialog = false;
  std::string m_currentDirectory = "macros";
  std::vector<std::filesystem::directory_entry> m_directoryEntries;
  int m_selectedFileIndex = -1;
  char m_saveFileName[256] = "";

  // === UI RENDERING METHODS ===
  void RenderMacroLeftPanel();
  void RenderMacroMiddlePanel();
  void RenderMacroRightPanel();

  // Left Panel Methods
  void RenderMacroList();
  void RenderMacroControls();
  void RenderNewMacroSection();
  void RenderLoadSaveSection();

  // Middle Panel Methods
  void RenderProgramTable();
  void RenderTableHeader();
  void RenderProgramRow(int rowIndex, MacroProgramItem& item);
  void RenderExecutionControls();

  // Right Panel Methods
  void RenderMacroProperties();
  void RenderAvailablePrograms();
  void RenderExecutionStatus();
  void RenderMacroVisualization(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize);

  // === HELPER METHODS ===
  void RefreshMacroList();
  void RefreshProgramItems();
  void RefreshAvailablePrograms();
  std::vector<std::string> GetAvailableMacros();
  std::vector<std::string> GetAvailablePrograms();

  // === EXECUTION METHODS ===
  void ExecuteSelectedPrograms();
  void ExecuteSingleProgram(const std::string& programName);
  void PlayExecution();
  void PauseExecution();
  void StopExecution();

  // === FILE DIALOG METHODS ===
  void RenderFileDialog();
  void RenderLoadDialog();
  void RenderSaveDialog();
  void RefreshDirectoryListing();
  void NavigateToDirectory(const std::string& path);
  bool IsJsonFile(const std::filesystem::directory_entry& entry);

  // === UTILITY METHODS ===
  void RenderPrompts();

  // NEW METHOD: Synchronize UI state with MacroManager
  void SyncExecutionState();

  void RenderEnhancedExecutionControls();
  void RenderProgramCards();
  void RemoveProgramFromMacro(int index);
};