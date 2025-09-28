// SettingsEditorUI.h
#pragma once

#include "IImguiWindowUI.h"
#include "AppSettings.h"
#include "imgui.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>  // Add this for std::function

class Logger;

struct SettingsParameter {
  std::string name;
  std::string displayName;
  std::string type; // "float", "string", "bool", "int"
  std::string value;
  std::string unit; // For display purposes
  float minValue = 0.0f;
  float maxValue = 1000.0f;
};

struct SettingsCategory {
  std::string name;
  std::string displayName;
  std::vector<SettingsParameter> parameters;
};

class SettingsEditorUI : public IImguiWindowUI {
public:
  // Add callback type
  using OnSettingsChangedCallback = std::function<void()>;

  SettingsEditorUI();
  ~SettingsEditorUI() override = default;

  // IImguiWindowUI interface implementation
  void Render() override;
  void Show() override;
  void Hide() override;
  bool IsVisible() const override;
  const std::string& GetName() const override;

  // Add callback setter
  void SetOnSettingsChangedCallback(OnSettingsChangedCallback callback) {
    m_onSettingsChanged = callback;
  }

private:
  bool m_isVisible = false;
  Logger* m_logger;
  std::string m_windowName = "Settings Editor";

  // Add callback member
  OnSettingsChangedCallback m_onSettingsChanged;

  // UI state
  std::vector<SettingsCategory> m_categories;
  int m_selectedCategoryIndex = 0;
  std::string m_statusMessage;

  // Methods
  void InitializeCategories();
  void LoadCategoryFromDatabase(SettingsCategory& category);
  void SaveCategoryToDatabase(const SettingsCategory& category);
  void SaveAllCategories();

  void RenderLeftPanel();
  void RenderRightPanel();
  void RenderParameterInput(SettingsParameter& param);

  void UpdateStatus(const std::string& message, bool isError = false);
};