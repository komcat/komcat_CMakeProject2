#pragma once

#include <string>
#include <vector>
#include <map>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define SOCKET_TYPE SOCKET
#define INVALID_SOCKET_VALUE INVALID_SOCKET
#define SOCKET_ERROR_VALUE SOCKET_ERROR
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET_TYPE int
#define INVALID_SOCKET_VALUE -1
#define SOCKET_ERROR_VALUE -1
#define closesocket close
#endif

// Data structure to hold SIPHOG measurement data
struct SIPHOGData {
  float sledCurrent;      // SLED_Current (mA)
  float photoCurrent;     // Photo Current (uA)  
  float sledTemp;         // SLED_Temp (C)
  float targetSagPwr;     // Target SAG_PWR (V)
  float sagPwr;           // SAG_PWR (V)
  float tecCurrent;       // TEC_Current (mA)
  std::chrono::steady_clock::time_point timestamp;

  // Default constructor
  SIPHOGData() : sledCurrent(0.0f), photoCurrent(0.0f), sledTemp(0.0f),
    targetSagPwr(0.0f), sagPwr(0.0f), tecCurrent(0.0f),
    timestamp(std::chrono::steady_clock::now()) {
  }
};

class SIPHOGClient {
public:
  SIPHOGClient();
  ~SIPHOGClient();

  // Connection management
  bool Connect(const std::string& ip = "127.0.0.1", int port = 65432);
  void Disconnect();
  bool IsConnected() const;

  // Data collection
  void StartPolling();
  void StopPolling();
  bool IsPolling() const;

  // Data access
  SIPHOGData GetLatestData() const;
  std::vector<SIPHOGData> GetDataBuffer() const;
  void ClearDataBuffer();

  // Configuration
  void SetPollingInterval(int intervalMs);
  int GetPollingInterval() const;
  void SetMaxBufferSize(size_t maxSize);

  // UI
  void RenderUI();
  void SetShowWindow(bool show);
  bool GetShowWindow() const;
  void ToggleWindow(); // Required for VerticalToolbarMenu integration
  bool IsVisible() const; // Required for VerticalToolbarMenu integration

  // Statistics
  struct DataStats {
    float min, max, avg;
  };
  std::map<std::string, DataStats> GetStatistics() const;

  // Error handling
  std::string GetLastError() const;

private:
  // Network members
  SOCKET_TYPE m_socket;
  std::string m_serverIp;
  int m_serverPort;
  std::atomic<bool> m_isConnected;
  std::string m_lastError;

  // Data collection members
  std::atomic<bool> m_isPolling;
  std::thread m_pollingThread;
  int m_pollingIntervalMs;

  // Data storage
  SIPHOGData m_latestData;
  std::vector<SIPHOGData> m_dataBuffer;
  size_t m_maxBufferSize;
  mutable std::mutex m_dataMutex;

  // UI members
  bool m_showWindow;
  std::string m_name;

  // Private methods for new polling implementation
  void PollingThreadFunction();
  bool FetchLatestData();
  bool ParseAndStoreMessage(const std::string& message);
  void StoreDataInGlobalStore(const SIPHOGData& data);
  void AddToBuffer(const SIPHOGData& data);

  // Data keys (same as Python client)
  static const std::vector<std::string> DATA_KEYS;
};