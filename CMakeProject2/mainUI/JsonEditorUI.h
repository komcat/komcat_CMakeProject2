// JsonEditorUI.h
#pragma once

#include <string>
#include <vector>
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

private:
  // UI state
  bool m_visible = false;
  std::string m_name = "JSON Editor";

  // File management
  std::vector<std::string> m_jsonFiles;
  int m_selectedFileIndex = -1;
  std::string m_currentFilePath;

  // Editor content
  std::string m_editorContent;
  bool m_contentModified = false;

  // UI layout
  float m_leftPanelWidth = 250.0f;

  // Helper methods
  void ScanForJsonFiles();
  void LoadJsonFile(const std::string& filepath);
  void SaveJsonFile();
  void SaveJsonFileAs();
  void CreateNewFile();

  // UI rendering methods
  void RenderFileList();
  void RenderEditor();
  void RenderButtons();

  // Status messages
  std::string m_statusMessage;
  float m_statusMessageTimer = 0.0f;
  void SetStatusMessage(const std::string& msg);
};