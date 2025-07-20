// MacroPanelUI.h
#pragma once

#include "imgui.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

// Forward declarations
class MacroManager;
class MachineBlockUI;

struct MacroProgramItem {
  int id;
  bool selected = false;
  std::string name;
  std::string filePath;
  bool canRun = true;
};

class MacroPanelUI {
public:
  MacroPanelUI();
  ~MacroPanelUI() = default;

  void RenderUI();
  void SetMacroManager(MacroManager* macroManager);
  void SetMachineBlockUI(MachineBlockUI* blockUI);

  // Control functions (remove ToggleWindow and IsVisible since we're embedded)
  void SetVisible(bool visible) { m_showWindow = visible; }
  bool IsVisible() const { return m_showWindow; }

private:
  bool m_showWindow = true;
  MacroManager* m_macroManager = nullptr;
  MachineBlockUI* m_blockUI = nullptr;

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

  // UI Rendering Methods
  void RenderLeftPanel();   // 25% - Macro List & Controls
  void RenderMiddlePanel(); // 50% - Program Items Table
  void RenderRightPanel();  // 25% - Properties & Actions

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
};