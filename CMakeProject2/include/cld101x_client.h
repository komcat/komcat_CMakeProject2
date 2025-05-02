#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <deque>
#include <chrono>

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

class CLD101xClient {
public:
  CLD101xClient();
  ~CLD101xClient();

  // Connect to CLD101x server
  bool Connect(const std::string& ip, int port);

  // Disconnect from server
  void Disconnect();

  // Check if connected
  bool IsConnected() const;

  // Control methods
  bool SetLaserCurrent(float current); // Set laser current in Amperes
  bool SetTECTemperature(float temperature); // Set TEC temperature in Celsius
  bool LaserOn(); // Turn laser on
  bool LaserOff(); // Turn laser off
  bool TECOn(); // Turn TEC on
  bool TECOff(); // Turn TEC off

  // Measurement methods
  float GetTemperature() const; // Get the latest temperature reading (C)
  float GetLaserCurrent() const; // Get the latest laser current reading (A)

  // UI rendering
  void RenderUI();
  void ToggleWindow(); // Toggle UI visibility
  bool IsVisible() const;
  const std::string& GetName() const;

  // Start/stop background polling
  void StartPolling(int intervalMs = 500); // Default to 500ms as per requirements
  void StopPolling();

private:
  // Send command to the server
  bool SendCommand(const std::string& command, std::string* response = nullptr);

  // Background polling thread
  void PollingThread();

  // Socket variables
  SOCKET m_socket;
  std::string m_serverIp;
  int m_serverPort;
  std::atomic<bool> m_isConnected;

  // Latest readings
  mutable std::mutex m_dataMutex;
  float m_currentTemperature;
  float m_currentLaserCurrent;
  std::string m_lastError;

  // Polling thread
  std::thread m_pollingThread;
  std::atomic<bool> m_isPolling;
  int m_pollingIntervalMs;

  // UI state
  bool m_showWindow;
  std::string m_name;

  // Stats and history
  std::deque<std::pair<std::chrono::steady_clock::time_point, float>> m_temperatureHistory;
  std::deque<std::pair<std::chrono::steady_clock::time_point, float>> m_currentHistory;
  static constexpr size_t MAX_HISTORY_SIZE = 300; // ~5 minutes at 1Hz polling
};