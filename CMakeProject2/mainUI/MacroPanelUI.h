// MacroPanelUI.h
#pragma once

#include "imgui.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
// Add these includes at the top
#include <filesystem>



// Forward declarations ONLY - no includes in header
class MacroManager;
class MachineBlockUI;
class CameraManager;               // Forward declaration only
class MacroPanelCameraHandler;     // Forward declaration only

struct MacroProgramItem {
  int id;
  bool selected = false;
  std::string name;
  std::string filePath;
  bool canRun = true;
};

class MacroPanelUI {
public:
  MacroPanelUI(CameraManager* cameraManager = nullptr);  // Updated constructor
  ~MacroPanelUI(); // Need explicit destructor for std::unique_ptr with forward declaration

  void RenderMacroPanel();  // RENAMED: was RenderUI() - avoids conflict with MachineBlockUI
  void SetMacroManager(MacroManager* macroManager);
  void SetMachineBlockUI(MachineBlockUI* blockUI);

  // Camera management
  void SetCameraManager(CameraManager* cameraManager);

  // Control functions (remove ToggleWindow and IsVisible since we're embedded)
  void SetVisible(bool visible) { m_showWindow = visible; }
  bool IsVisible() const { return m_showWindow; }

private:
  bool m_showWindow = true;
  MacroManager* m_macroManager = nullptr;
  MachineBlockUI* m_blockUI = nullptr;

  // Camera handler - clean separation of concerns
  std::unique_ptr<MacroPanelCameraHandler> m_cameraHandler;

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

  // UI Rendering Methods - RENAMED to avoid conflicts with MachineBlockUI
  void RenderMacroLeftPanel();    // 33% - Macro List & Controls
  void RenderMacroMiddlePanel();  // 25% - Program Items Table
  void RenderMacroRightPanel();   // 42% - Properties & Actions

  // Left Panel Methods
  void RenderMacroList();
  void RenderMacroControls();
  void RenderNewMacroSection();
  void RenderLoadSaveSection();

  // Middle Panel Methods
  void RenderProgramTable();
  void RenderTableHeader();
  void RenderProgramRow(int rowIndex, MacroProgramItem& item);
  void RenderProgramRowColumns(int rowIndex, MacroProgramItem& item); // New column-based method
  void RenderExecutionControls();
  void RenderProgramRowColumnsSafe(int rowIndex, MacroProgramItem& item);
  void RenderProgramRowSimple(int rowIndex, MacroProgramItem& item);
  void RenderProgramTableVertical();

  // Right Panel Methods
  void RenderMacroProperties();
  void RenderAvailablePrograms();
  void RenderExecutionStatus();
  void RenderMacroVisualization(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize);

  // Helper Methods
  void RefreshMacroList();
  void RefreshProgramItems();
  void RefreshAvailablePrograms();
  std::vector<std::string> GetAvailableMacros();
  std::vector<std::string> GetAvailablePrograms();

  // Execution Methods
  void ExecuteSelectedPrograms();
  void ExecuteSingleProgram(const std::string& programName);
  void ExecuteNextSelectedProgram(); // NEW METHOD - sequential execution
  void ExecuteSelectedProgramsClean(); // NEW METHOD - simple clean approach
  void PlayExecution();
  void PauseExecution();
  void StopExecution();

  // Add these member variables in the private section:
  std::vector<std::string> m_selectedProgramsQueue; // Queue of selected programs to execute
  int m_currentExecutionIndex = 0; // Current index in the execution queue


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

  // File dialog methods
  void RenderFileDialog();
  void RenderLoadDialog();
  void RenderSaveDialog();
  void RefreshDirectoryListing();
  void NavigateToDirectory(const std::string& path);
  bool IsJsonFile(const std::filesystem::directory_entry& entry);

};