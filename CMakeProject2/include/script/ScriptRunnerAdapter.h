// ScriptRunnerAdapter.h
#pragma once

#include "include/ui/VerticalToolbarMenu.h"
#include "include/script/script_runner.h"

// Adapter for ScriptRunner that implements the IHierarchicalTogglableUI interface
class ScriptRunnerAdapter : public IHierarchicalTogglableUI {
public:
  ScriptRunnerAdapter(ScriptRunner& runner, const std::string& name)
    : m_runner(runner), m_name(name), m_children() {
  }

  bool IsVisible() const override {
    return m_runner.IsVisible();
  }

  void ToggleWindow() override {
    m_runner.ToggleWindow();
  }

  const std::string& GetName() const override {
    return m_name;
  }

  bool HasChildren() const override {
    return false;
  }

  const std::vector<std::shared_ptr<IHierarchicalTogglableUI>>& GetChildren() const override {
    return m_children;
  }

private:
  ScriptRunner& m_runner;
  std::string m_name;
  std::vector<std::shared_ptr<IHierarchicalTogglableUI>> m_children;
};

// Helper function to create a ScriptRunnerAdapter
inline std::shared_ptr<IHierarchicalTogglableUI> CreateScriptRunnerAdapter(
  ScriptRunner& runner, const std::string& name) {
  return std::make_shared<ScriptRunnerAdapter>(runner, name);
}

