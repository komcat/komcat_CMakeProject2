#pragma once

#include "SiPhOGDataProvider.h"
#include "include/data/global_data_store.h"
#include <memory>
#include <string>

/**
 * @brief Manager class for SiPhOG data provider integration with GlobalDataStore
 * Handles the publisher-subscriber pattern initialization and lifecycle
 */
class SiPhOGManager {
public:
  SiPhOGManager(const std::string& host = "127.0.0.1", int port = 65432);
  ~SiPhOGManager();

  // Lifecycle management
  bool Initialize();
  void Shutdown();
  bool IsInitialized() const { return m_initialized; }

  // Connection management
  bool Connect();
  void Disconnect();
  bool IsConnected() const;

  // Data collection
  bool StartDataCollection(int intervalMs = 0); // 0 = use default/continuous
  void StopDataCollection();
  bool IsDataCollectionActive() const;

  // Configuration
  void SetDebugMode(bool enable);
  void SetChannelPrefix(const std::string& prefix) { m_channelPrefix = prefix; }

  // Status and statistics
  SiPhOGDataProvider::Stats GetStats() const;
  std::string GetConnectionStatus() const;

  // Access to underlying provider (for debug UI etc.)
  SiPhOGDataProvider* GetProvider() const { return m_provider.get(); }

private:
  // Configuration
  std::string m_host;
  int m_port;
  std::string m_channelPrefix;

  // Components
  std::unique_ptr<SiPhOGDataProvider> m_provider;
  GlobalDataStore* m_dataStore;

  // State
  bool m_initialized;
  bool m_subscribed;
  bool m_debugMode;

  // Methods
  bool SubscribeToGlobalDataStore();
  void UnsubscribeFromGlobalDataStore();
};