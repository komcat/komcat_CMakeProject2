// script_editor_ui.h
#pragma once

#include "include/script/script_executor.h"
#include "include/machine_operations.h"
#include "include/ui/VerticalToolbarMenu.h"
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <map>

class ScriptEditorUI : public IHierarchicalTogglableUI {
public:
  ScriptEditorUI(MachineOperations& machineOps);
  ~ScriptEditorUI();

  // Render the UI
  void RenderUI();

  // IHierarchicalTogglableUI implementation
  bool IsVisible() const override { return m_isVisible; }
  void ToggleWindow() override { m_isVisible = !m_isVisible; }
  const std::string& GetName() const override { return m_name; }
  bool HasChildren() const override { return false; }
  const std::vector<std::shared_ptr<IHierarchicalTogglableUI>>& GetChildren() const override { return m_children; }

  // Original visibility methods
  void SetVisible(bool visible) { m_isVisible = visible; }
  void ToggleVisibility() { m_isVisible = !m_isVisible; }

  // Script management
  bool LoadScript(const std::string& filename);
  bool SaveScript(const std::string& filename);
  void SetScript(const std::string& script);
  const std::string& GetScript() const { return m_script; }

  // Sample scripts for quick access
  void AddSampleScript(const std::string& name, const std::string& script);
  // File dialog methods
  void ShowOpenDialog();
  void ShowSaveDialog();
  void RenderFileDialog();
private:
  // Helper method to trim whitespace from strings
  std::string TrimString(const std::string& str);
  std::string m_name;
  std::vector<std::shared_ptr<IHierarchicalTogglableUI>> m_children;
  // UI sections
  void RenderEditorSection();
  void RenderControlSection();
  void RenderLogSection();
  void RenderCommandHelpSection();

  // Command help system
  void InitializeCommandHelp();
  struct CommandHelp {
    std::string syntax;
    std::string description;
    std::string example;
  };
  std::map<std::string, CommandHelp> m_commandHelp;

  // Script execution handler
  void OnScriptExecutionStateChanged(ScriptExecutor::ExecutionState state);
  void OnScriptLog(const std::string& message);

  // UI state
  bool m_isVisible;
  std::string m_script;
  bool m_showCommandHelp;

  // Sample scripts
  std::map<std::string, std::string> m_sampleScripts;

  // Execution state
  MachineOperations& m_machineOps;
  ScriptExecutor m_executor;

  // Status display
  std::string m_statusMessage;
  std::chrono::steady_clock::time_point m_statusExpiry;

  // Buffer for script editor
  static constexpr size_t EDITOR_BUFFER_SIZE = 65536;
  char m_editorBuffer[EDITOR_BUFFER_SIZE];

  // File management
  std::string m_currentFilePath;
  bool m_showFileDialog = false;
  bool m_isOpenDialog = true;
  char m_filePathBuffer[256] = "";

  std::vector<std::string> m_recentFiles;
  const size_t MAX_RECENT_FILES = 5;

  void AddToRecentFiles(const std::string& filepath);
};