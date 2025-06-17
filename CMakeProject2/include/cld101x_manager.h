#pragma once

#include <string>
#include <memory>
#include <map>
#include <vector>
#include "cld101x_client.h"

// Forward declaration
class Logger;
// Store connection info for reconnection - Add this
struct ClientConnectionInfo {
  std::string ip;
  int port;
  bool autoConnect;
};
// Manager class for CLD101x clients
class CLD101xManager {
public:
  CLD101xManager();
  ~CLD101xManager();

  // Initialize from JSON config (for future use)
  bool Initialize(const std::string& configFile = "");

  // Add a client with the given name and connection details
  bool AddClient(const std::string& name, const std::string& ip, int port);

  // Get a client by name
  CLD101xClient* GetClient(const std::string& name);

  // Remove a client by name
  bool RemoveClient(const std::string& name);

  // Connect all clients
  bool ConnectAll();

  // Disconnect all clients
  void DisconnectAll();

  // CONNECTION STATUS METHODS - Add these
  bool IsConnected(const std::string& clientName) const;
  bool IsAnyConnected() const;
  bool AreAllConnected() const;
  int GetConnectedCount() const;

  // RECONNECTION METHODS - Add these
  bool ReconnectClient(const std::string& clientName);
  bool ReconnectAll();
  bool ConnectClient(const std::string& clientName);


  // Get list of all client names
  std::vector<std::string> GetClientNames() const;

  // UI rendering
  void RenderUI();
  void ToggleWindow();
  bool IsVisible() const;
  const std::string& GetName() const;

private:
  Logger* m_logger;
  std::map<std::string, std::unique_ptr<CLD101xClient>> m_clients;
  bool m_showWindow;
  std::string m_name;


  std::map<std::string, ClientConnectionInfo> m_connectionInfo;
};