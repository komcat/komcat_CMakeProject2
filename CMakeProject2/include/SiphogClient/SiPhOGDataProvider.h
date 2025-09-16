#pragma once

#include "include/data/IDataProvider.h"
#include "SocketWrapper.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <chrono>
#include <memory>

/**
 * @brief SiPhOG data provider that implements IDataProvider interface
 * Connects to Python SiPhOG server and publishes data through callbacks
 */
class SiPhOGDataProvider : public IDataProvider {
public:
  SiPhOGDataProvider(const std::string& host = "127.0.0.1", int port = 65432);
  ~SiPhOGDataProvider();

  // IDataProvider interface
  std::string GetProviderName() const override;
  bool StartDataCollection(int intervalMs) override;
  void StopDataCollection() override;
  bool IsDataCollectionActive() const override;
  void SetDataUpdateCallback(std::function<void(const std::string&, const std::string&)> callback) override;
  std::vector<std::string> GetDeviceNames() const override;
  std::map<std::string, std::string> GetChannelSuffixes() const override;

  // SiPhOG-specific methods
  bool Connect();
  void Disconnect();
  bool IsConnected() const;

  // Configuration
  void SetDebugMode(bool enable) { m_debugMode = enable; }

  // Statistics
  struct Stats {
    uint64_t totalMessages = 0;
    uint64_t validMessages = 0;
    uint64_t errorMessages = 0;
    double currentRate = 0.0;
    std::chrono::steady_clock::time_point startTime;
  };

  Stats GetStats() const;

private:
  // Network configuration
  std::string m_host;
  int m_port;
  std::unique_ptr<SocketWrapper> m_socket;

  // Connection state
  std::atomic<bool> m_connected;
  std::atomic<bool> m_dataCollectionActive;
  std::atomic<bool> m_shouldStop;

  // Threading
  std::thread m_dataThread;
  mutable std::mutex m_statsMutex;
  mutable std::mutex m_callbackMutex;

  // Data callback
  std::function<void(const std::string&, const std::string&)> m_dataCallback;

  // Statistics
  Stats m_stats;

  // Configuration
  bool m_debugMode;

  // Data keys matching Python server output
  std::vector<std::string> m_dataKeys;

  // Methods
  void InitializeDataKeys();
  void DataCollectionLoop();
  bool ParseAndPublishData(const std::string& csvLine);
  void UpdateStats(bool messageReceived);
  void PublishDataUpdate(const std::map<std::string, float>& values);
  void DebugLog(const std::string& message);
};