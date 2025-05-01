#pragma once

#include "include/ui/ToolbarMenu.h"
#include "include/data/data_client_manager.h"

// Adapter for DataClientManager
class DataClientManagerAdapter : public ITogglableUI {
public:
  DataClientManagerAdapter(DataClientManager& manager, const std::string& name)
    : m_manager(manager), m_name(name), m_isVisible(true) {
  }

  bool IsVisible() const override {
    return m_isVisible;
  }

  void ToggleWindow() override {
    m_isVisible = !m_isVisible;
  }

  const std::string& GetName() const override {
    return m_name;
  }

private:
  DataClientManager& m_manager;
  std::string m_name;
  bool m_isVisible;
};

// Helper function to create the adapter
std::shared_ptr<ITogglableUI> CreateDataClientManagerAdapter(DataClientManager& manager, const std::string& name) {
  return std::make_shared<DataClientManagerAdapter>(manager, name);
}