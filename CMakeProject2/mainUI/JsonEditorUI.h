// JsonEditorUI.h
#pragma once

#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include "../mainUI/MenuManager_uaa3.h"
#include "imgui.h"

class JsonEditorUI : public IImguiUI {
public:
  JsonEditorUI();
  ~JsonEditorUI() = default;

  // IImguiUI interface implementation
  void Render() override;
  void Show() override { m_visible = true; }
  void Hide() override { m_visible = false; }
  bool IsVisible() const override { return m_visible; }
  const std::string& GetName() const override { return m_name; }
  void Toggle() override { m_visible = !m_visible; }

  // Font setting
  void SetMonospacedFont(ImFont* font) { m_monospacedFont = font; }

private:
  // UI state
  bool m_visible = false;
  std::string m_name = "JSON Editor";

  // Font
  ImFont* m_monospacedFont = nullptr;

  // File management
  std::vector<std::string> m_jsonFiles;
  int m_selectedFileIndex = -1;
  std::string m_currentFilePath;

  // Editor content
  std::string m_editorContent;
  std::string m_originalContent;  // Store original content to track changes
  bool m_contentModified = false;

  // Line tracking
  std::vector<std::string> m_currentLines;
  std::vector<std::string> m_originalLines;
  std::set<int> m_modifiedLines;  // Track which lines have changed

  // UI layout
  float m_leftPanelWidth = 250.0f;
  bool m_showLineNumbers = true;
  bool m_highlightChanges = true;

  // Helper methods
  void ScanForJsonFiles();
  void LoadJsonFile(const std::string& filepath);
  void SaveJsonFile();
  void SaveJsonFileAs();
  void CreateNewFile();
  void UpdateLineTracking();
  std::vector<std::string> SplitIntoLines(const std::string& text);

  // UI rendering methods
  void RenderFileList();
  void RenderEditor();
  void RenderButtons();

  // Status messages
  std::string m_statusMessage;
  float m_statusMessageTimer = 0.0f;
  void SetStatusMessage(const std::string& msg);
};