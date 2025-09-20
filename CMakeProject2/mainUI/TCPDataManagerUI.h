// TCPDataManagerUI.h
#pragma once

#include "include/data/data_client_manager.h"
#include <memory>
#include <unordered_map>
#include <string>

// Structure to hold server display information from DataServerConfig.json
struct ServerDisplayInfo {
  std::string id;
  std::string name;
  std::string description;
  std::string host;
  int port;
  bool autoConnect;
  std::string unit;
  bool displayUnitSuffix;
  bool logData;

  ServerDisplayInfo() : port(0), autoConnect(false), displayUnitSuffix(false), logData(false) {}
};

class TCPDataManagerUI {
private:
  DataClientManager* m_dataClientManager = nullptr; // Changed to raw pointer
  bool m_isInitialized = false;

  // Store server configuration information for proper display
  std::unordered_map<std::string, ServerDisplayInfo> m_serverConfigs;

  // Helper methods
  void LoadServerConfig();
  void RenderServerCard(const ServerDisplayInfo& serverInfo, int clientIndex);

public:
  TCPDataManagerUI();
  ~TCPDataManagerUI();

  // Initialize with external data client manager
  bool Initialize(DataClientManager* dataClientManager);

  // Update the manager (call this every frame)
  void Update();

  // Render the UI
  void Render();

  // Check if initialized
  bool IsInitialized() const { return m_isInitialized; }

  // Get access to the underlying manager (if needed)
  DataClientManager* GetManager() { return m_dataClientManager; }
};