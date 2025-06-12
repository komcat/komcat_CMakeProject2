#pragma once

#include <string>
#include <memory>
#include <map>
#include <vector>
#include "keithley2400_client.h"

// Forward declaration
class Logger;

// Manager class for Keithley 2400 clients
class Keithley2400Manager {
public:
  Keithley2400Manager();
  ~Keithley2400Manager();

  // Initialize from JSON config
  bool Initialize(const std::string& configFile = "");

  // Add a client with the given name and connection details
  bool AddClient(const std::string& name, const std::string& ip, int port);

  // Get a client by name
  Keithley2400Client* GetClient(const std::string& name);

  // Remove a client by name
  bool RemoveClient(const std::string& name);

  // Connect all clients
  bool ConnectAll();

  // Disconnect all clients
  void DisconnectAll();

  // Get list of all client names
  std::vector<std::string> GetClientNames() const;

  // Synchronized operations across all clients
  bool SetAllOutputs(bool enable);
  bool ResetAllInstruments();
  void StartAllPolling(int intervalMs = 1000);
  void StopAllPolling();

  // Get aggregated data
  struct AggregatedData {
    int connectedCount;
    int totalCount;
    double totalVoltage;
    double totalCurrent;
    double totalPower;
    std::vector<std::pair<std::string, Keithley2400Reading>> readings;
  };
  AggregatedData GetAggregatedData() const;

  // UI rendering
  void RenderUI();
  void ToggleWindow();
  bool IsVisible() const;
  const std::string& GetName() const;

private:
  Logger* m_logger;
  std::map<std::string, std::unique_ptr<Keithley2400Client>> m_clients;
  std::map<std::string, int> m_clientPollingIntervals;  // name -> polling interval
  // Store connection information for each client
  std::map<std::string, std::pair<std::string, int>> m_clientConnections; // name -> (ip, port)

  bool m_showWindow;
  std::string m_name;

  // Rest of the existing private members...
  struct ManagerUIState {
    bool autoConnect = true;
    int defaultPollingInterval = 1000;
    std::string lastConfigFile = "";

    // Bulk operation settings
    double bulkVoltage = 0.0;
    double bulkCurrent = 0.001;
    double bulkCompliance = 0.1;
    int bulkSourceMode = 0; // 0 = voltage, 1 = current
  } m_uiState;

  // Helper methods
  void LoadDefaultConfiguration();
  bool SaveConfiguration(const std::string& filename);
  bool LoadConfiguration(const std::string& filename);


};