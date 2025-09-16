#pragma once

#include "mainUI/MenuManager_uaa3.h"
#include "SocketWrapper.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <chrono>
#include <deque>
#include <memory>

/**
 * @brief Simple SiPhOG client for debug testing - no GlobalDataStore dependency
 * Implements IImguiUI interface for integration with menu system
 * Uses SocketWrapper to avoid winsock conflicts
 */
class SiPhOGClientDebugUI : public IImguiUI {
public:
  struct DataPoint {
    std::chrono::steady_clock::time_point timestamp;
    std::map<std::string, float> values;
    std::string rawData;
    bool parseSuccess;
  };

  struct ConnectionStats {
    uint64_t totalMessages = 0;
    uint64_t validMessages = 0;
    uint64_t errorMessages = 0;
    double currentRate = 0.0;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastMessageTime;
    bool isConnected = false;
    bool isCollecting = false;
  };

public:
  SiPhOGClientDebugUI();
  ~SiPhOGClientDebugUI();

  // IImguiUI interface implementation
  void Render() override;
  void Show() override;
  void Hide() override;
  bool IsVisible() const override;
  const std::string& GetName() const override;

  // Connection management
  bool Connect(const std::string& host, int port);
  void Disconnect();
  bool StartDataCollection();
  void StopDataCollection();

  // Status
  bool IsConnected() const { return m_connected; }
  bool IsCollecting() const { return m_dataCollectionActive; }

  // Data access for UI
  ConnectionStats GetStats() const;
  std::vector<DataPoint> GetRecentData(size_t count = 100) const;
  DataPoint GetLatestData() const;
  std::string GetConnectionStatus() const;

  // Legacy method for direct UI rendering (kept for compatibility)
  void RenderDebugUI();

private:
  // Network using wrapper
  std::string m_host;
  int m_port;
  std::unique_ptr<SocketWrapper> m_socket;
  std::atomic<bool> m_connected;
  std::atomic<bool> m_dataCollectionActive;
  std::atomic<bool> m_shouldStop;

  // Threading
  std::thread m_dataThread;
  mutable std::mutex m_dataMutex;
  mutable std::mutex m_statsMutex;

  // Data storage
  std::deque<DataPoint> m_dataHistory;
  static const size_t MAX_HISTORY = 1000;

  // Statistics
  ConnectionStats m_stats;

  // Data keys
  std::vector<std::string> m_dataKeys;

  // UI state
  bool m_isVisible;
  std::string m_windowName;
  char m_hostBuffer[256];
  int m_portBuffer;
  bool m_autoScroll;
  bool m_showRawData;
  bool m_showParseErrors;

  // Methods
  void InitializeDataKeys();
  void DataCollectionLoop();
  bool ParseData(const std::string& dataStr, std::map<std::string, float>& values);
  void AddDataPoint(const std::string& rawData, const std::map<std::string, float>& values, bool parseSuccess);
  void UpdateStats(bool messageReceived);
  void RenderConnectionPanel();
  void RenderDataPanel();
  void RenderStatsPanel();
  void RenderRawDataPanel();
};