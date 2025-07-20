// MacroPanelUI.h
#pragma once

#include "imgui.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

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
  void PlayExecution();
  void PauseExecution();
  void StopExecution();

  // Data
  std::vector<std::string> m_availableMacros;
  std::vector<std::string> m_availablePrograms;

  // File dialog state
  bool m_showLoadDialog = false;
  std::vector<std::string> m_availableMacroFiles;
  int m_selectedFileIndex = 0;
  char m_loadDialogSearchBuffer[256] = "";

  // Add these method declarations:
  void RenderLoadDialog();
  void RefreshMacroFiles();
  std::vector<std::string> GetMacroFilesFromDirectory();

};