#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <chrono>
#include <mutex>
#include <map>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

class GlobalDataStore;

/**
 * @brief C++ client for SiPhOG TCP server
 * Connects to Python SiPhOG server and stores data in GlobalDataStore
 */
class SiPhOGClient {
public:
  /**
   * @brief Constructor
   * @param host Server hostname/IP (default: "127.0.0.1")
   * @param port Server port (default: 65432)
   * @param channelPrefix Prefix for GlobalDataStore channels (default: "SiPhOG-")
   */
  SiPhOGClient(const std::string& host = "127.0.0.1",
    int port = 65432,
    const std::string& channelPrefix = "SiPhOG-");

  /**
   * @brief Destructor - ensures clean shutdown
   */
  ~SiPhOGClient();

  /**
   * @brief Connect to the SiPhOG server
   * @return true if connection successful
   */
  bool Connect();

  /**
   * @brief Disconnect from server
   */
  void Disconnect();

  /**
   * @brief Start data collection in background thread
   * @return true if started successfully
   */
  bool StartDataCollection();

  /**
   * @brief Stop data collection
   */
  void StopDataCollection();

  /**
   * @brief Check if connected to server
   */
  bool IsConnected() const { return m_connected; }

  /**
   * @brief Check if data collection is active
   */
  bool IsDataCollectionActive() const { return m_dataCollectionActive; }

  /**
   * @brief Get connection statistics
   */
  struct ConnectionStats {
    uint64_t totalMessages = 0;
    uint64_t validMessages = 0;
    uint64_t errorMessages = 0;
    double averageRate = 0.0;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastMessageTime;
  };

  ConnectionStats GetStats() const;

  /**
   * @brief Set callback for connection status updates
   * @param callback Function called when connection status changes
   */
  void SetStatusCallback(std::function<void(const std::string&)> callback);

  /**
   * @brief Get last received data values
   * @return Map of channel names to values
   */
  std::map<std::string, float> GetLatestData() const;

  /**
   * @brief Enable/disable debug output
   */
  void SetDebugMode(bool enable) { m_debugMode = enable; }

private:
  // Network configuration
  std::string m_host;
  int m_port;
  std::string m_channelPrefix;

  // Socket and connection
  SOCKET m_socket;
  std::atomic<bool> m_connected;
  std::atomic<bool> m_dataCollectionActive;
  std::atomic<bool> m_shouldStop;

  // Threading
  std::thread m_dataThread;
  mutable std::mutex m_dataMutex;
  mutable std::mutex m_statsMutex;

  // Data storage
  GlobalDataStore* m_dataStore;
  std::map<std::string, float> m_latestData;

  // Statistics
  ConnectionStats m_stats;

  // Callbacks
  std::function<void(const std::string&)> m_statusCallback;

  // Configuration
  bool m_debugMode;

  // Data keys matching Python server
  std::vector<std::string> m_dataKeys;

  /**
   * @brief Initialize data keys
   */
  void InitializeDataKeys();

  /**
   * @brief Initialize network (Windows-specific)
   */
  bool InitializeNetwork();

  /**
   * @brief Cleanup network (Windows-specific)
   */
  void CleanupNetwork();

  /**
   * @brief Main data collection loop (runs in separate thread)
   */
  void DataCollectionLoop();

  /**
   * @brief Parse CSV data from server
   * @param dataStr CSV string from server
   * @return true if parsing successful
   */
  bool ParseData(const std::string& dataStr);

  /**
   * @brief Store parsed data in GlobalDataStore
   * @param values Vector of float values matching m_dataKeys
   */
  void StoreData(const std::vector<float>& values);

  /**
   * @brief Update connection statistics
   * @param messageReceived true if valid message received
   */
  void UpdateStats(bool messageReceived);

  /**
   * @brief Send status update via callback
   * @param status Status message
   */
  void SendStatusUpdate(const std::string& status);

  /**
   * @brief Log debug message
   * @param message Debug message
   */
  void DebugLog(const std::string& message);
};