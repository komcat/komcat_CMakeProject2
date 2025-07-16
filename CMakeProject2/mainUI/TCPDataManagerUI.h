// TCPDataManagerUI.h
#pragma once

#include "include/data/data_client_manager.h"
#include <memory>

class TCPDataManagerUI {
private:
  std::unique_ptr<DataClientManager> m_dataClientManager;
  bool m_isInitialized = false;

public:
  TCPDataManagerUI();
  ~TCPDataManagerUI();

  // Initialize the data client manager
  bool Initialize(const std::string& configPath = "DataServerConfig.json");

  // Update the manager (call this every frame)
  void Update();

  // Render the UI
  void Render();

  // Check if initialized
  bool IsInitialized() const { return m_isInitialized; }

  // Get access to the underlying manager (if needed)
  DataClientManager* GetManager() { return m_dataClientManager.get(); }
};