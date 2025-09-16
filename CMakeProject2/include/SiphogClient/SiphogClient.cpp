#include "SiPhOGClient.h"
#include "include/data/global_data_store.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>

// Include socket headers in .cpp file only to avoid conflicts
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Prevent inclusion of old winsock.h
#define _WINSOCKAPI_
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#define closesocket close
#endif

SiPhOGClient::SiPhOGClient(const std::string& host, int port, const std::string& channelPrefix)
  : m_host(host)
  , m_port(port)
  , m_channelPrefix(channelPrefix)
  , m_socket(INVALID_SOCKET)
  , m_connected(false)
  , m_dataCollectionActive(false)
  , m_shouldStop(false)
  , m_dataStore(nullptr)
  , m_debugMode(false)
{
  InitializeDataKeys();
  m_dataStore = GlobalDataStore::GetInstance();

  if (m_debugMode) {
    DebugLog("SiPhOGClient created - Host: " + m_host + ", Port: " + std::to_string(m_port));
  }
}

SiPhOGClient::~SiPhOGClient() {
  StopDataCollection();
  Disconnect();
  CleanupNetwork();

  if (m_debugMode) {
    DebugLog("SiPhOGClient destroyed");
  }
}

void SiPhOGClient::InitializeDataKeys() {
  m_dataKeys = {
      "SLED_Current (mA)",
      "Photo Current (uA)",
      "SLED_Temp (C)",
      "Target SAG_PWR (V)",
      "SAG_PWR (V)",
      "TEC_Current (mA)"
  };
}

bool SiPhOGClient::InitializeNetwork() {
#ifdef _WIN32
  WSADATA wsaData;
  int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (result != 0) {
    SendStatusUpdate("Failed to initialize Winsock: " + std::to_string(result));
    return false;
  }
#endif
  return true;
}

void SiPhOGClient::CleanupNetwork() {
#ifdef _WIN32
  WSACleanup();
#endif
}

bool SiPhOGClient::Connect() {
  if (m_connected) {
    DebugLog("Already connected to server");
    return true;
  }

  if (!InitializeNetwork()) {
    return false;
  }

  // Create socket
  m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (m_socket == INVALID_SOCKET) {
    SendStatusUpdate("Failed to create socket");
    CleanupNetwork();
    return false;
  }

  // Configure server address
  sockaddr_in serverAddr;
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(m_port);

#ifdef _WIN32
  inet_pton(AF_INET, m_host.c_str(), &serverAddr.sin_addr);
#else
  inet_aton(m_host.c_str(), &serverAddr.sin_addr);
#endif

  SendStatusUpdate("Connecting to " + m_host + ":" + std::to_string(m_port) + "...");

  // Connect to server
  if (connect(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
    SendStatusUpdate("Failed to connect to server");
    closesocket(m_socket);
    m_socket = INVALID_SOCKET;
    CleanupNetwork();
    return false;
  }

  // Set socket timeout for non-blocking receive
#ifdef _WIN32
  DWORD timeout = 100; // 100ms timeout
  setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 100000; // 100ms timeout
  setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#endif

  m_connected = true;

  // Initialize statistics
  {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats = ConnectionStats();
    m_stats.startTime = std::chrono::steady_clock::now();
  }

  SendStatusUpdate("✅ Connected successfully!");
  DebugLog("Socket connected successfully");

  return true;
}

void SiPhOGClient::Disconnect() {
  if (!m_connected) {
    return;
  }

  m_connected = false;

  if (m_socket != INVALID_SOCKET) {
    closesocket(m_socket);
    m_socket = INVALID_SOCKET;
  }

  SendStatusUpdate("Disconnected from server");
  DebugLog("Socket disconnected");
}

bool SiPhOGClient::StartDataCollection() {
  if (m_dataCollectionActive) {
    DebugLog("Data collection already active");
    return true;
  }

  if (!m_connected) {
    SendStatusUpdate("Cannot start data collection - not connected");
    return false;
  }

  m_shouldStop = false;
  m_dataCollectionActive = true;

  // Start data collection thread
  m_dataThread = std::thread(&SiPhOGClient::DataCollectionLoop, this);

  SendStatusUpdate("🚀 Data collection started");
  DebugLog("Data collection thread started");

  return true;
}

void SiPhOGClient::StopDataCollection() {
  if (!m_dataCollectionActive) {
    return;
  }

  m_shouldStop = true;
  m_dataCollectionActive = false;

  // Wait for thread to finish
  if (m_dataThread.joinable()) {
    m_dataThread.join();
  }

  SendStatusUpdate("🛑 Data collection stopped");
  DebugLog("Data collection thread stopped");
}

void SiPhOGClient::DataCollectionLoop() {
  std::string buffer;
  char recvBuffer[1024];

  SendStatusUpdate("📊 Starting data monitoring...");

  while (!m_shouldStop && m_connected) {
    try {
      // Receive data from server
      int bytesReceived = recv(m_socket, recvBuffer, sizeof(recvBuffer) - 1, 0);

      if (bytesReceived > 0) {
        recvBuffer[bytesReceived] = '\0';
        buffer += std::string(recvBuffer);

        // Process complete messages
        size_t pos;
        while ((pos = buffer.find_first_of("\n,")) != std::string::npos) {
          std::string line;

          if (buffer[pos] == '\n') {
            line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
          }
          else {
            // Check if we have enough values for a complete message
            std::vector<std::string> parts;
            std::stringstream ss(buffer);
            std::string item;

            while (std::getline(ss, item, ',')) {
              parts.push_back(item);
              if (parts.size() >= m_dataKeys.size()) {
                break;
              }
            }

            if (parts.size() >= m_dataKeys.size()) {
              // We have enough values
              line = "";
              for (size_t i = 0; i < m_dataKeys.size(); ++i) {
                if (i > 0) line += ",";
                line += parts[i];
              }

              // Remove processed data from buffer
              size_t consumed = 0;
              for (size_t i = 0; i < m_dataKeys.size(); ++i) {
                consumed += parts[i].length();
                if (i < m_dataKeys.size() - 1) consumed += 1; // comma
              }
              if (consumed < buffer.length()) {
                buffer.erase(0, consumed + 1); // +1 for trailing comma
              }
              else {
                buffer.clear();
              }
            }
            else {
              break; // Not enough data yet
            }
          }

          // Process the line if we got one
          if (!line.empty()) {
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);

            if (!line.empty() && line.find(',') != std::string::npos) {
              bool success = ParseData(line);
              UpdateStats(success);

              if (m_debugMode && success) {
                static int debugCounter = 0;
                if (++debugCounter % 50 == 1) {
                  DebugLog("Processed message: " + line.substr(0, 50) + "...");
                }
              }
            }
          }
        }

      }
      else if (bytesReceived == 0) {
        // Server disconnected
        SendStatusUpdate("⚠️ Server disconnected");
        m_connected = false;
        break;

      }
      else {
        // Error or timeout
#ifdef _WIN32
        int error = WSAGetLastError();
        if (error != WSAETIMEDOUT && error != WSAEWOULDBLOCK) {
#else
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
#endif
          SendStatusUpdate("❌ Socket error during receive");
          m_connected = false;
          break;
        }
        // Timeout is normal, continue loop
        }

      // Small delay to prevent busy waiting
      std::this_thread::sleep_for(std::chrono::milliseconds(1));

      }
    catch (const std::exception& e) {
      SendStatusUpdate("❌ Exception in data loop: " + std::string(e.what()));
      break;
    }
    }

  m_dataCollectionActive = false;
  DebugLog("Data collection loop exited");
  }

bool SiPhOGClient::ParseData(const std::string & dataStr) {
  try {
    std::vector<float> values;
    std::stringstream ss(dataStr);
    std::string item;

    // Split by commas and convert to float
    while (std::getline(ss, item, ',') && values.size() < m_dataKeys.size()) {
      // Remove whitespace
      item.erase(0, item.find_first_not_of(" \t"));
      item.erase(item.find_last_not_of(" \t") + 1);

      if (!item.empty()) {
        float value = std::stof(item);
        values.push_back(value);
      }
    }

    // Check if we have the expected number of values
    if (values.size() == m_dataKeys.size()) {
      StoreData(values);
      return true;
    }
    else {
      if (m_debugMode) {
        DebugLog("Invalid data format - expected " + std::to_string(m_dataKeys.size()) +
          " values, got " + std::to_string(values.size()));
      }
      return false;
    }

  }
  catch (const std::exception& e) {
    if (m_debugMode) {
      DebugLog("Parse error: " + std::string(e.what()) + " for data: " + dataStr);
    }
    return false;
  }
}

void SiPhOGClient::StoreData(const std::vector<float>&values) {
  if (!m_dataStore || values.size() != m_dataKeys.size()) {
    return;
  }

  // Store in GlobalDataStore with channel prefix
  for (size_t i = 0; i < values.size(); ++i) {
    std::string channelName = m_channelPrefix + m_dataKeys[i];
    m_dataStore->SetValue(channelName, values[i]);
  }

  // Update local latest data cache
  {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    for (size_t i = 0; i < values.size(); ++i) {
      m_latestData[m_dataKeys[i]] = values[i];
    }
  }

  if (m_debugMode) {
    static int storeCounter = 0;
    if (++storeCounter % 100 == 1) {
      DebugLog("Stored data: SLED_Current=" + std::to_string(values[0]) +
        "mA, SAG_PWR=" + std::to_string(values[4]) + "V");
    }
  }
}

void SiPhOGClient::UpdateStats(bool messageReceived) {
  std::lock_guard<std::mutex> lock(m_statsMutex);

  auto now = std::chrono::steady_clock::now();
  m_stats.totalMessages++;

  if (messageReceived) {
    m_stats.validMessages++;
    m_stats.lastMessageTime = now;
  }
  else {
    m_stats.errorMessages++;
  }

  // Calculate average rate
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_stats.startTime);
  if (duration.count() > 0) {
    m_stats.averageRate = (double)m_stats.validMessages / (duration.count() / 1000.0);
  }
}

SiPhOGClient::ConnectionStats SiPhOGClient::GetStats() const {
  std::lock_guard<std::mutex> lock(m_statsMutex);
  return m_stats;
}

void SiPhOGClient::SetStatusCallback(std::function<void(const std::string&)> callback) {
  m_statusCallback = callback;
}

std::map<std::string, float> SiPhOGClient::GetLatestData() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  return m_latestData;
}

void SiPhOGClient::SendStatusUpdate(const std::string & status) {
  if (m_statusCallback) {
    m_statusCallback(status);
  }

  if (m_debugMode) {
    DebugLog("STATUS: " + status);
  }
}

void SiPhOGClient::DebugLog(const std::string & message) {
  if (m_debugMode) {
    auto now = std::chrono::steady_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(
      std::chrono::system_clock::now());

    std::cout << "[DEBUG SiPhOGClient "
      << std::put_time(std::localtime(&time_t), "%H:%M:%S")
      << "] " << message << std::endl;
  }
}