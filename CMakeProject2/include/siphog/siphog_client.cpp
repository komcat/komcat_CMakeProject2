#include "include/siphog/siphog_client.h"
#include "include/logger.h"
#include "include/data/global_data_store.h"
#include "imgui.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <mutex>

// Data keys from the server (same order as server sends) - matching Python client
const std::vector<std::string> SIPHOGClient::DATA_KEYS = {
    "SLED_Current (mA)",
    "Photo Current (uA)",
    "SLED_Temp (C)",
    "Target SAG_PWR (V)",
    "SAG_PWR (V)",
    "TEC_Current (mA)"
};

SIPHOGClient::SIPHOGClient()
  : m_socket(INVALID_SOCKET_VALUE)
  , m_serverPort(0)
  , m_isConnected(false)
  , m_isPolling(false)
  , m_pollingIntervalMs(1000)  // 1 second default, matching Python client
  , m_maxBufferSize(1000)      // Same as Python client
  , m_showWindow(true)
  , m_name("SIPHOG Controller")
{
#ifdef _WIN32
  // Initialize Winsock for Windows
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    Logger::GetInstance()->LogError("SIPHOGClient: WSAStartup failed");
  }
#endif

  // Initialize latest data
  m_latestData = SIPHOGData();

  // Print data store keys that will be used
  Logger* logger = Logger::GetInstance();
  logger->LogInfo("SIPHOGClient: Initialized - Data store keys will be:");
  for (const auto& key : DATA_KEYS) {
    logger->LogInfo("  - " + key);
  }

  Logger::GetInstance()->LogInfo("SIPHOGClient: Initialized");
}

SIPHOGClient::~SIPHOGClient() {
  StopPolling();
  Disconnect();

#ifdef _WIN32
  WSACleanup();
#endif
  Logger::GetInstance()->LogInfo("SIPHOGClient: Destroyed");
}

bool SIPHOGClient::Connect(const std::string& ip, int port) {
  Logger* logger = Logger::GetInstance();

  if (m_isConnected) {
    Disconnect();
  }

  m_serverIp = ip;
  m_serverPort = port;

  // Create socket
  m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (m_socket == INVALID_SOCKET_VALUE) {
    m_lastError = "Error creating socket";
    logger->LogError("SIPHOGClient: " + m_lastError);
    return false;
  }

  // Setup server address
  sockaddr_in serverAddr;
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(port);

  if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) <= 0) {
    m_lastError = "Invalid address: " + ip;
    logger->LogError("SIPHOGClient: " + m_lastError);
    closesocket(m_socket);
    m_socket = INVALID_SOCKET_VALUE;
    return false;
  }

  // Connect to server
  if (connect(m_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR_VALUE) {
    m_lastError = "Connection failed to " + ip + ":" + std::to_string(port);
    logger->LogError("SIPHOGClient: " + m_lastError);
    closesocket(m_socket);
    m_socket = INVALID_SOCKET_VALUE;
    return false;
  }

  m_isConnected = true;
  logger->LogInfo("SIPHOGClient: Connected to " + ip + ":" + std::to_string(port));

  // Start polling by default (like Python client)
  StartPolling();

  return true;
}

void SIPHOGClient::Disconnect() {
  if (!m_isConnected) {
    return;
  }

  StopPolling();

  if (m_socket != INVALID_SOCKET_VALUE) {
    closesocket(m_socket);
    m_socket = INVALID_SOCKET_VALUE;
  }

  m_isConnected = false;
  Logger::GetInstance()->LogInfo("SIPHOGClient: Disconnected");
}

bool SIPHOGClient::IsConnected() const {
  return m_isConnected;
}

void SIPHOGClient::StartPolling() {
  if (m_isPolling || !m_isConnected) {
    return;
  }

  m_isPolling = true;
  m_pollingThread = std::thread(&SIPHOGClient::PollingThreadFunction, this);
  Logger::GetInstance()->LogInfo("SIPHOGClient: Started polling");
}

void SIPHOGClient::StopPolling() {
  if (!m_isPolling) {
    return;
  }

  m_isPolling = false;

  if (m_pollingThread.joinable()) {
    m_pollingThread.join();
  }

  Logger::GetInstance()->LogInfo("SIPHOGClient: Stopped polling");
}

bool SIPHOGClient::IsPolling() const {
  return m_isPolling;
}

SIPHOGData SIPHOGClient::GetLatestData() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  return m_latestData;
}

std::vector<SIPHOGData> SIPHOGClient::GetDataBuffer() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  return m_dataBuffer;
}

void SIPHOGClient::ClearDataBuffer() {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  m_dataBuffer.clear();
}

void SIPHOGClient::SetPollingInterval(int intervalMs) {
  m_pollingIntervalMs = (std::max)(100, intervalMs); // Minimum 100ms
}

int SIPHOGClient::GetPollingInterval() const {
  return m_pollingIntervalMs;
}

void SIPHOGClient::SetMaxBufferSize(size_t maxSize) {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  m_maxBufferSize = maxSize;

  // Trim buffer if needed
  if (m_dataBuffer.size() > m_maxBufferSize) {
    m_dataBuffer.erase(m_dataBuffer.begin(),
      m_dataBuffer.begin() + (m_dataBuffer.size() - m_maxBufferSize));
  }
}

std::string SIPHOGClient::GetLastError() const {
  return m_lastError;
}

void SIPHOGClient::SetShowWindow(bool show) {
  m_showWindow = show;
}

bool SIPHOGClient::GetShowWindow() const {
  return m_showWindow;
}

void SIPHOGClient::ToggleWindow() {
  m_showWindow = !m_showWindow;
}

bool SIPHOGClient::IsVisible() const {
  return m_showWindow;
}

std::map<std::string, SIPHOGClient::DataStats> SIPHOGClient::GetStatistics() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  std::map<std::string, DataStats> stats;

  if (m_dataBuffer.empty()) {
    return stats;
  }

  // Calculate statistics for each data key
  std::vector<float> sledCurrentValues, photoCurrentValues, sledTempValues;
  std::vector<float> targetSagPwrValues, sagPwrValues, tecCurrentValues;

  for (const auto& data : m_dataBuffer) {
    sledCurrentValues.push_back(data.sledCurrent);
    photoCurrentValues.push_back(data.photoCurrent);
    sledTempValues.push_back(data.sledTemp);
    targetSagPwrValues.push_back(data.targetSagPwr);
    sagPwrValues.push_back(data.sagPwr);
    tecCurrentValues.push_back(data.tecCurrent);
  }

  auto calculateStats = [](const std::vector<float>& values) -> DataStats {
    DataStats stats;
    stats.min = *std::min_element(values.begin(), values.end());
    stats.max = *std::max_element(values.begin(), values.end());
    stats.avg = std::accumulate(values.begin(), values.end(), 0.0f) / values.size();
    return stats;
  };

  stats[DATA_KEYS[0]] = calculateStats(sledCurrentValues);
  stats[DATA_KEYS[1]] = calculateStats(photoCurrentValues);
  stats[DATA_KEYS[2]] = calculateStats(sledTempValues);
  stats[DATA_KEYS[3]] = calculateStats(targetSagPwrValues);
  stats[DATA_KEYS[4]] = calculateStats(sagPwrValues);
  stats[DATA_KEYS[5]] = calculateStats(tecCurrentValues);

  return stats;
}

void SIPHOGClient::PollingThreadFunction() {
  Logger::GetInstance()->LogInfo("SIPHOGClient: Polling thread started");

  while (m_isPolling && m_isConnected) {
    if (FetchLatestData()) {
      // Successfully got fresh data
    }

    // Sleep for the specified interval before next poll
    std::this_thread::sleep_for(std::chrono::milliseconds(m_pollingIntervalMs));
  }

  Logger::GetInstance()->LogInfo("SIPHOGClient: Polling thread stopped");
}

bool SIPHOGClient::FetchLatestData() {
  if (!m_isConnected) {
    return false;
  }

  try {
    // Set a short timeout for non-blocking receive
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000; // 500ms timeout

    if (setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO,
      (const char*)&timeout, sizeof(timeout)) < 0) {
      m_lastError = "Failed to set socket timeout";
      return false;
    }

    // Read data until we get a complete message or timeout
    std::string tempBuffer;
    char buffer[1024];

    auto startTime = std::chrono::steady_clock::now();
    const auto maxWaitTime = std::chrono::milliseconds(500); // Max 500ms to get a complete message

    while (std::chrono::steady_clock::now() - startTime < maxWaitTime) {
      int bytesReceived = recv(m_socket, buffer, sizeof(buffer) - 1, 0);

      if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        tempBuffer += std::string(buffer);

        // Check if we have a complete message
        size_t startPos = tempBuffer.find("$START,");
        if (startPos != std::string::npos) {
          size_t endPos = tempBuffer.find(",END$", startPos);
          if (endPos != std::string::npos) {
            // Found complete message, extract and parse it
            size_t dataStart = startPos + 7; // Length of "$START,"
            size_t dataLength = endPos - dataStart;
            std::string completeMessage = tempBuffer.substr(dataStart, dataLength);

            // Only log errors, not normal operation
            // Logger::GetInstance()->LogInfo("SIPHOGClient: Fetched fresh data: '" + completeMessage + "'");

            if (ParseAndStoreMessage(completeMessage)) {
              return true; // Successfully parsed and stored
            }
          }
        }
      }
      else if (bytesReceived == 0) {
        m_lastError = "Server disconnected";
        Logger::GetInstance()->LogWarning("SIPHOGClient: Server disconnected");
        m_isConnected = false;
        return false;
      }
      else {
        // Check if it's a timeout or actual error
#ifdef _WIN32
        int error = WSAGetLastError();
        if (error == WSAETIMEDOUT) {
          break; // Timeout, stop trying
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          break; // Timeout, stop trying
        }
#endif
        m_lastError = "Error receiving data";
        return false;
      }
    }

    // If we get here, we either timed out or didn't get a complete message
    // Logger::GetInstance()->LogInfo("SIPHOGClient: No complete message received in this poll cycle");
    return false;

  }
  catch (const std::exception& e) {
    m_lastError = "Exception in FetchLatestData: " + std::string(e.what());
    Logger::GetInstance()->LogError("SIPHOGClient: " + m_lastError);
    return false;
  }
}

bool SIPHOGClient::ParseAndStoreMessage(const std::string& message) {
  try {
    // Parse the message
    std::vector<std::string> tokens;
    std::stringstream ss(message);
    std::string token;

    while (std::getline(ss, token, ',')) {
      token.erase(token.find_last_not_of(" \t\r\n") + 1);
      token.erase(0, token.find_first_not_of(" \t\r\n"));
      if (!token.empty()) {
        tokens.push_back(token);
      }
    }

    // We should have exactly 6 tokens for SIPHOG data
    if (tokens.size() != DATA_KEYS.size()) {
      Logger::GetInstance()->LogWarning("SIPHOGClient: Expected " +
        std::to_string(DATA_KEYS.size()) + " tokens, got " + std::to_string(tokens.size()));
      return false;
    }

    // Convert to values
    std::vector<double> values;
    for (const auto& token : tokens) {
      try {
        double value = std::stod(token);
        values.push_back(value);
      }
      catch (const std::exception& e) {
        Logger::GetInstance()->LogWarning("SIPHOGClient: Failed to parse token '" + token + "': " + e.what());
        return false;
      }
    }

    // Sanity check on the values
    if (values[0] < 0 || values[0] > 300) return false;     // SLED_Current (mA): 0-300
    if (values[1] < 0 || values[1] > 3000) return false;    // Photo Current (uA): 0-3000  
    if (values[2] < 0 || values[2] > 100) return false;     // SLED_Temp (C): 0 to 100
    if (std::abs(values[3]) > 3.3) return false;            // Target SAG_PWR (V): ±3.3V
    if (std::abs(values[4]) > 3.3) return false;            // SAG_PWR (V): ±3.3V  
    if (values[5] < -200 || values[5] > 200) return false;  // TEC_Current (mA): -200 to 200

    // Create data structure
    SIPHOGData data;
    data.timestamp = std::chrono::steady_clock::now();
    data.sledCurrent = static_cast<float>(values[0]);    // SLED_Current (mA)
    data.photoCurrent = static_cast<float>(values[1]);   // Photo Current (uA)
    data.sledTemp = static_cast<float>(values[2]);       // SLED_Temp (C)
    data.targetSagPwr = static_cast<float>(values[3]);   // Target SAG_PWR (V)
    data.sagPwr = static_cast<float>(values[4]);         // SAG_PWR (V)
    data.tecCurrent = static_cast<float>(values[5]);     // TEC_Current (mA)

    // Store in buffer and global store
    AddToBuffer(data);
    StoreDataInGlobalStore(data);

    // Only log errors, not normal successful operations
    // Logger::GetInstance()->LogInfo("SIPHOGClient: Successfully parsed and stored...");

    return true;

  }
  catch (const std::exception& e) {
    Logger::GetInstance()->LogWarning("SIPHOGClient: Exception parsing message: " + std::string(e.what()));
    return false;
  }
}

void SIPHOGClient::StoreDataInGlobalStore(const SIPHOGData& data) {
  // Store each value in the global data store using the same keys as Python client
  GlobalDataStore* store = GlobalDataStore::GetInstance();

  store->SetValue(DATA_KEYS[0], data.sledCurrent);    // SLED_Current (mA)
  store->SetValue(DATA_KEYS[1], data.photoCurrent);   // Photo Current (uA)
  store->SetValue(DATA_KEYS[2], data.sledTemp);       // SLED_Temp (C)
  store->SetValue(DATA_KEYS[3], data.targetSagPwr);   // Target SAG_PWR (V)
  store->SetValue(DATA_KEYS[4], data.sagPwr);         // SAG_PWR (V)
  store->SetValue(DATA_KEYS[5], data.tecCurrent);     // TEC_Current (mA)
}

void SIPHOGClient::AddToBuffer(const SIPHOGData& data) {
  std::lock_guard<std::mutex> lock(m_dataMutex);

  m_latestData = data;
  m_dataBuffer.push_back(data);

  // Keep buffer size manageable (same as Python client)
  if (m_dataBuffer.size() > m_maxBufferSize) {
    m_dataBuffer.erase(m_dataBuffer.begin());
  }
}

void SIPHOGClient::RenderUI() {
  if (!m_showWindow) {
    return;
  }

  ImGui::Begin(m_name.c_str(), &m_showWindow);

  // Connection status
  ImGui::Text("Connection Status: %s", m_isConnected ? "Connected" : "Disconnected");

  if (m_isConnected) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "●");
    ImGui::Text("Server: %s:%d", m_serverIp.c_str(), m_serverPort);
  }

  ImGui::Separator();

  // Connection controls
  static char ipBuffer[64] = "127.0.0.1";
  static int portBuffer = 65432;

  ImGui::InputText("IP Address", ipBuffer, sizeof(ipBuffer));
  ImGui::InputInt("Port", &portBuffer);

  if (!m_isConnected) {
    if (ImGui::Button("Connect")) {
      Connect(std::string(ipBuffer), portBuffer);
    }
  }
  else {
    if (ImGui::Button("Disconnect")) {
      Disconnect();
    }
  }

  ImGui::Separator();

  // Polling controls
  ImGui::Text("Polling: %s", m_isPolling ? "Active" : "Stopped");

  int intervalMs = m_pollingIntervalMs;
  if (ImGui::SliderInt("Interval (ms)", &intervalMs, 100, 5000)) {
    SetPollingInterval(intervalMs);
  }

  if (m_isConnected) {
    if (!m_isPolling) {
      if (ImGui::Button("Start Polling")) {
        StartPolling();
      }
    }
    else {
      if (ImGui::Button("Stop Polling")) {
        StopPolling();
      }
    }
  }

  ImGui::Separator();

  // Latest data display
  ImGui::Text("Latest Data:");
  if (m_isConnected && m_isPolling) {
    SIPHOGData latest = GetLatestData();

    ImGui::Text("%-25s: %10.3f", DATA_KEYS[0].c_str(), latest.sledCurrent);
    ImGui::Text("%-25s: %10.3f", DATA_KEYS[1].c_str(), latest.photoCurrent);
    ImGui::Text("%-25s: %10.3f", DATA_KEYS[2].c_str(), latest.sledTemp);
    ImGui::Text("%-25s: %10.3f", DATA_KEYS[3].c_str(), latest.targetSagPwr);
    ImGui::Text("%-25s: %10.3f", DATA_KEYS[4].c_str(), latest.sagPwr);
    ImGui::Text("%-25s: %10.3f", DATA_KEYS[5].c_str(), latest.tecCurrent);
  }
  else {
    ImGui::Text("No data available");
  }

  ImGui::Separator();

  // Buffer info
  {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    ImGui::Text("Buffer: %zu/%zu samples", m_dataBuffer.size(), m_maxBufferSize);
  }

  if (ImGui::Button("Clear Buffer")) {
    ClearDataBuffer();
  }

  // Error display
  if (!m_lastError.empty()) {
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Last Error:");
    ImGui::TextWrapped("%s", m_lastError.c_str());
  }

  ImGui::End();
}