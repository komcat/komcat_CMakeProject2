#include "cld101x_client.h"
#include "logger.h"
#include "imgui.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

CLD101xClient::CLD101xClient()
  : m_socket(INVALID_SOCKET)
  , m_serverPort(0)
  , m_isConnected(false)
  , m_currentTemperature(0.0f)
  , m_currentLaserCurrent(0.0f)
  , m_lastUpdateTime(std::chrono::steady_clock::now())
  , m_isPolling(false)
  , m_pollingIntervalMs(500)  // Default to 500ms
  , m_showWindow(true)
  , m_name("CLD101x Controller")

  , m_globalDataStore(nullptr)          // ADD THIS
  , m_enableGlobalDataStore(false)      // ADD THIS
  , m_devicePrefix("CLD101x")           // ADD THIS
{
  // Initialize Global Data Store
  m_globalDataStore = GlobalDataStore::GetInstance();

#ifdef _WIN32
  // Initialize Winsock for Windows
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    Logger::GetInstance()->LogError("CLD101xClient: WSAStartup failed");
  }
#endif
  Logger::GetInstance()->LogInfo("CLD101xClient: Initialized");
}

CLD101xClient::~CLD101xClient() {
  StopPolling();
  Disconnect();

#ifdef _WIN32
  // Cleanup Winsock for Windows
  WSACleanup();
#endif
  Logger::GetInstance()->LogInfo("CLD101xClient: Destroyed");
}

// ENHANCED Connect method in cld101x_client.cpp
bool CLD101xClient::Connect(const std::string& ip, int port) {
  Logger* logger = Logger::GetInstance();

  // If already connected, disconnect first
  if (m_isConnected) {
    Disconnect();
  }

  m_serverIp = ip;
  m_serverPort = port;

  // Create socket
  m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (m_socket == INVALID_SOCKET) {
    logger->LogError("CLD101xClient: Error creating socket");
    return false;
  }

  // Setup server address
  sockaddr_in serverAddr;
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(port);

  // Convert IP address from string to binary form
  if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) <= 0) {
    logger->LogError("CLD101xClient: Invalid address: " + ip);
    closesocket(m_socket);
    m_socket = INVALID_SOCKET;
    return false;
  }

  // Connect to server
  if (connect(m_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
    logger->LogError("CLD101xClient: Connection failed to " + ip + ":" + std::to_string(port));
    closesocket(m_socket);
    m_socket = INVALID_SOCKET;
    return false;
  }

  m_isConnected = true;
  logger->LogInfo("CLD101xClient: Connected to " + ip + ":" + std::to_string(port));

  // **NEW: Sync hardware status immediately after connection**
  try {
    // Clear status cache to force fresh reads
    m_lastStatusQuery = std::chrono::steady_clock::time_point{};

    // Sync hardware status before starting polling
    SyncHardwareStatus();
    logger->LogInfo("CLD101xClient: Initial hardware status - Laser: " +
      std::string(m_cachedLaserStatus ? "ON" : "OFF") +
      ", TEC: " + std::string(m_cachedTECStatus ? "ON" : "OFF"));
  }
  catch (const std::exception& e) {
    logger->LogWarning("CLD101xClient: Failed to sync initial hardware status: " + std::string(e.what()));
  }

  // Start polling automatically with default interval
  StartPolling(m_pollingIntervalMs);

  return true;
}
void CLD101xClient::Disconnect() {
  if (!m_isConnected) {
    return;
  }

  // Stop polling first
  StopPolling();

  // Close socket
  if (m_socket != INVALID_SOCKET) {
    closesocket(m_socket);
    m_socket = INVALID_SOCKET;
  }

  m_isConnected = false;
  Logger::GetInstance()->LogInfo("CLD101xClient: Disconnected");
}

bool CLD101xClient::IsConnected() const {
  return m_isConnected;
}

// NEW: Get cached readings (thread-safe, for UI)
float CLD101xClient::GetLatestTemperature() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  return m_currentTemperature;
}

float CLD101xClient::GetLatestLaserCurrent() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  return m_currentLaserCurrent;
}

std::chrono::steady_clock::time_point CLD101xClient::GetLastUpdateTime() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  return m_lastUpdateTime;
}

// Existing methods for compatibility
float CLD101xClient::GetTemperature() const {
  return GetLatestTemperature();
}

float CLD101xClient::GetLaserCurrent() const {
  return GetLatestLaserCurrent();
}

// Primary SendCommand method with pointer parameter (existing)
bool CLD101xClient::SendCommand(const std::string& command, std::string* response) {
  if (!m_isConnected) {
    m_lastError = "Not connected to server";
    return false;
  }

  // Clear any previous error
  m_lastError.clear();

  // Check for overflow before casting
  if (command.length() > static_cast<size_t>(INT_MAX)) {
    m_lastError = "Command too large to send";
    return false;
  }

  if (send(m_socket, command.c_str(), static_cast<int>(command.length()), 0) == SOCKET_ERROR) {
    m_lastError = "Failed to send command";
    return false;
  }

  // If response is requested, wait for it
  if (response) {
    char buffer[1024] = { 0 };
    int bytesReceived = recv(m_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytesReceived <= 0) {
      m_lastError = "No response received";
      return false;
    }

    // Set response
    *response = std::string(buffer, bytesReceived);
  }

  return true;
}

// Overloaded SendCommand method with reference parameter (new for UI compatibility)
bool CLD101xClient::SendCommand(const std::string& command, std::string& response) {
  return SendCommand(command, &response);
}

bool CLD101xClient::SetLaserCurrent(float current) {
  // Format command with precision to 3 decimal places
  std::ostringstream cmd;
  cmd << "SET_LASER_CURRENT " << std::fixed << std::setprecision(3) << current;

  std::string response;
  bool result = SendCommand(cmd.str(), &response);

  if (result) {
    Logger::GetInstance()->LogInfo("CLD101xClient: " + response);
  }
  else {
    Logger::GetInstance()->LogError("CLD101xClient: Failed to set laser current - " + m_lastError);
  }

  return result;
}

bool CLD101xClient::SetTECTemperature(float temperature) {
  // Format command with precision to 2 decimal places
  std::ostringstream cmd;
  cmd << "SET_TEC_TEMPERATURE " << std::fixed << std::setprecision(2) << temperature;

  std::string response;
  bool result = SendCommand(cmd.str(), &response);

  if (result) {
    Logger::GetInstance()->LogInfo("CLD101xClient: " + response);
  }
  else {
    Logger::GetInstance()->LogError("CLD101xClient: Failed to set TEC temperature - " + m_lastError);
  }

  return result;
}

bool CLD101xClient::LaserOn() {
  std::string response;
  bool result = SendCommand("LASER_ON", &response);

  if (result) {
    Logger::GetInstance()->LogInfo("CLD101xClient: " + response);
  }
  else {
    Logger::GetInstance()->LogError("CLD101xClient: Failed to turn laser on - " + m_lastError);
  }

  return result;
}

bool CLD101xClient::LaserOff() {
  std::string response;
  bool result = SendCommand("LASER_OFF", &response);

  if (result) {
    Logger::GetInstance()->LogInfo("CLD101xClient: " + response);
  }
  else {
    Logger::GetInstance()->LogError("CLD101xClient: Failed to turn laser off - " + m_lastError);
  }

  return result;
}

bool CLD101xClient::TECOn() {
  std::string response;
  bool result = SendCommand("TEC_ON", &response);

  if (result) {
    Logger::GetInstance()->LogInfo("CLD101xClient: " + response);
  }
  else {
    Logger::GetInstance()->LogError("CLD101xClient: Failed to turn TEC on - " + m_lastError);
  }

  return result;
}

bool CLD101xClient::TECOff() {
  std::string response;
  bool result = SendCommand("TEC_OFF", &response);

  if (result) {
    Logger::GetInstance()->LogInfo("CLD101xClient: " + response);
  }
  else {
    Logger::GetInstance()->LogError("CLD101xClient: Failed to turn TEC off - " + m_lastError);
  }

  return result;
}

void CLD101xClient::StartPolling(int intervalMs) {
  // Don't start if already polling
  if (m_isPolling) {
    Logger::GetInstance()->LogInfo("CLD101xClient: Polling already active");
    return;
  }

  // Don't start if not connected
  if (!m_isConnected) {
    Logger::GetInstance()->LogWarning("CLD101xClient: Cannot start polling - not connected");
    return;
  }

  // Set polling interval (clamp between 100ms and 5000ms)
  m_pollingIntervalMs = (std::max)(100, (std::min)(intervalMs, 5000));

  // Start polling thread
  m_isPolling = true;
  m_pollingThread = std::thread(&CLD101xClient::PollingThread, this);

  Logger::GetInstance()->LogInfo("CLD101xClient: Started polling thread with interval " +
    std::to_string(m_pollingIntervalMs) + "ms");
}

void CLD101xClient::StopPolling() {
  // Don't stop if not polling
  if (!m_isPolling) {
    return;
  }

  // Stop the polling thread
  m_isPolling = false;

  // Join the thread if it's still running
  if (m_pollingThread.joinable()) {
    m_pollingThread.join();
  }

  Logger::GetInstance()->LogInfo("CLD101xClient: Stopped polling thread");
}


// UPDATE the PollingThread method to publish to Global Data Store:
void CLD101xClient::PollingThread() {
  Logger* logger = Logger::GetInstance();
  logger->LogInfo("CLD101xClient: Polling thread started with " + std::to_string(m_pollingIntervalMs) + "ms interval");

  while (m_isPolling && m_isConnected) {
    auto now = std::chrono::steady_clock::now();

    // Read temperature
    std::string tempResponse;
    if (SendCommand("READ_TEC_TEMPERATURE", &tempResponse)) {
      size_t pos = tempResponse.find(": ");
      if (pos != std::string::npos) {
        try {
          float temp = std::stof(tempResponse.substr(pos + 2));

          // Update local cache
          {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            m_currentTemperature = temp;
            m_lastUpdateTime = now;
            m_temperatureHistory.push_back({ now, temp });

            if (m_temperatureHistory.size() > MAX_HISTORY_SIZE) {
              m_temperatureHistory.pop_front();
            }
          }

          // PUBLISH TO GLOBAL DATA STORE
          if (m_enableGlobalDataStore && m_globalDataStore) {
            m_globalDataStore->SetValue(m_devicePrefix + "-Temperature", temp);
          }

        }
        catch (const std::exception& e) {
          logger->LogWarning("CLD101xClient: Failed to parse temperature - " +
            std::string(e.what()) + " - Raw response: " + tempResponse);
        }
      }
    }
    else {
      logger->LogWarning("CLD101xClient: Failed to read temperature - " + m_lastError);
    }

    // Short delay between commands
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Read laser current
    std::string currentResponse;
    if (SendCommand("READ_LASER_CURRENT", &currentResponse)) {
      size_t pos = currentResponse.find(": ");
      if (pos != std::string::npos) {
        try {
          float current = std::stof(currentResponse.substr(pos + 2));

          // Update local cache
          {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            m_currentLaserCurrent = current;
            m_currentHistory.push_back({ now, current });

            if (m_currentHistory.size() > MAX_HISTORY_SIZE) {
              m_currentHistory.pop_front();
            }
          }

          // PUBLISH TO GLOBAL DATA STORE
          if (m_enableGlobalDataStore && m_globalDataStore) {
            m_globalDataStore->SetValue(m_devicePrefix + "-LaserCurrent", current);
          }

        }
        catch (const std::exception& e) {
          logger->LogWarning("CLD101xClient: Failed to parse current - " +
            std::string(e.what()) + " - Raw response: " + currentResponse);
        }
      }
    }
    else {
      logger->LogWarning("CLD101xClient: Failed to read laser current - " + m_lastError);
    }

    // Wait for next polling interval
    std::this_thread::sleep_for(std::chrono::milliseconds(m_pollingIntervalMs));
  }

  logger->LogInfo("CLD101xClient: Polling thread stopped");
}


// Existing UI code stays the same...
void CLD101xClient::RenderUI() {
  if (!m_showWindow) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
  if (ImGui::Begin(m_name.c_str(), &m_showWindow)) {
    // Connection status and controls
    if (!m_isConnected) {
      // IP and port input fields
      static char ipBuffer[64] = "127.0.0.11"; // Updated to match server IP
      static int port = 65432;

      ImGui::Text("Status: Disconnected");
      ImGui::InputText("IP Address", ipBuffer, sizeof(ipBuffer));
      ImGui::InputInt("Port", &port);

      if (ImGui::Button("Connect")) {
        Connect(ipBuffer, port);
      }
    }
    else {
      ImGui::Text("Status: Connected to %s:%d", m_serverIp.c_str(), m_serverPort);

      if (ImGui::Button("Disconnect")) {
        Disconnect();
      }

      ImGui::SameLine();

      // Enhanced polling controls
      if (m_isPolling) {
        if (ImGui::Button("Stop Polling")) {
          StopPolling();
        }
        ImGui::SameLine();
        ImGui::Text("Polling every %d ms", m_pollingIntervalMs);

        // Show last update time
        auto lastUpdate = GetLastUpdateTime();
        auto now = std::chrono::steady_clock::now();
        auto ageSec = std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate).count();
        ImGui::Text("Last update: %lds ago", ageSec);
      }
      else {
        // Configurable polling interval
        static int interval = 500;
        ImGui::SliderInt("Interval (ms)", &interval, 100, 2000);
        ImGui::SameLine();
        if (ImGui::Button("Start Polling")) {
          StartPolling(interval);
        }
      }

      ImGui::Separator();

      // Current readings section with data age indicator
      ImGui::Text("Current Temperature: %.2f C", GetLatestTemperature());
      ImGui::Text("Current Laser Current: %.3f A", GetLatestLaserCurrent());

      ImGui::Separator();

      // Rest of the existing UI code...
      // Controls section
      ImGui::Text("Laser Control:");
      // Button to turn laser on
      if (ImGui::Button("Laser ON")) {
        LaserOn();
      }
      ImGui::SameLine();
      // Button to turn laser off
      if (ImGui::Button("Laser OFF")) {
        LaserOff();
      }

      // Current control
      static float currentSetpoint = 0.15f;
      static int currentMA = static_cast<int>(currentSetpoint * 1000.0f); // Convert A to mA

      // Slider in mA units with max 280mA
      ImGui::SliderInt("Laser Current (mA)", &currentMA, 0, 280);
      currentSetpoint = currentMA / 1000.0f; // Convert back to A
      ImGui::SameLine();
      ImGui::Text("(%.3f A)", currentSetpoint);

      // Current dropdown selector
      static const char* currentOptions[] = { "110 mA", "120 mA", "130 mA", "140 mA", "150 mA",
                                             "160 mA", "170 mA", "180 mA", "190 mA", "200 mA",
                                             "210 mA", "220 mA", "230 mA", "240 mA", "250 mA" };
      static int currentIndex = 4; // Default to 150 mA

      if (ImGui::Combo("Preset Current", &currentIndex, currentOptions, IM_ARRAYSIZE(currentOptions))) {
        // Extract value from string (e.g., "150 mA" -> 150)
        int presetMA = std::stoi(currentOptions[currentIndex]);
        currentMA = presetMA;
        currentSetpoint = currentMA / 1000.0f;
      }

      if (ImGui::Button("Set Current")) {
        SetLaserCurrent(currentSetpoint);
      }

      ImGui::Separator();

      ImGui::Text("TEC Control:");
      // Button to turn TEC on
      if (ImGui::Button("TEC ON")) {
        TECOn();
      }
      ImGui::SameLine();
      // Button to turn TEC off
      if (ImGui::Button("TEC OFF")) {
        TECOff();
      }

      // Temperature control
      static float tempSetpoint = 25.0f;
      static int tempInt = static_cast<int>(tempSetpoint);

      // Snap to integer values between 20C and 30C
      ImGui::SliderInt("TEC Temperature (C)", &tempInt, 20, 30);
      tempSetpoint = static_cast<float>(tempInt);

      if (ImGui::Button("Set Temperature")) {
        SetTECTemperature(tempSetpoint);
      }

      ImGui::Separator();

      // Manual command interface for debugging
      ImGui::Text("Manual Command Interface:");
      static char commandBuffer[256] = "";
      ImGui::InputText("SCPI Command", commandBuffer, sizeof(commandBuffer));

      if (ImGui::Button("Send Command")) {
        if (strlen(commandBuffer) > 0) {
          std::string response;
          if (SendCommand(commandBuffer, &response)) {
            Logger::GetInstance()->LogInfo("Manual command response: " + response);
          }
          else {
            Logger::GetInstance()->LogError("Manual command failed: " + m_lastError);
          }
        }
      }

      ImGui::Separator();

      // Graphs section
      ImGui::Text("Temperature History");

      // Get latest data for graph
      std::vector<float> tempData;
      std::vector<float> currentData;

      {
        std::lock_guard<std::mutex> lock(m_dataMutex);

        // Copy temperature data
        tempData.reserve(m_temperatureHistory.size());
        for (const auto& [time, temp] : m_temperatureHistory) {
          tempData.push_back(temp);
        }

        // Copy current data
        currentData.reserve(m_currentHistory.size());
        for (const auto& [time, current] : m_currentHistory) {
          currentData.push_back(current);
        }
      }

      // Temperature plot
      if (!tempData.empty()) {
        float minTemp = *std::min_element(tempData.begin(), tempData.end());
        float maxTemp = *std::max_element(tempData.begin(), tempData.end());

        // Add a small margin to min/max for better visualization
        float margin = (std::max)(0.1f, (maxTemp - minTemp) * 0.1f);
        minTemp = std::floor(minTemp - margin);
        maxTemp = std::ceil(maxTemp + margin);

        // Plot the temperature graph
        ImGui::PlotLines("##temp", tempData.data(), static_cast<int>(tempData.size()),
          0, nullptr, minTemp, maxTemp, ImVec2(580, 100));
        ImGui::Text("Min: %.2f C   Max: %.2f C", minTemp, maxTemp);
      }
      else {
        ImGui::Text("No temperature data available yet");
      }

      ImGui::Text("Laser Current History");

      // Current plot
      if (!currentData.empty()) {
        float minCurrent = *std::min_element(currentData.begin(), currentData.end());
        float maxCurrent = *std::max_element(currentData.begin(), currentData.end());

        // Add a small margin to min/max for better visualization
        float margin = (std::max)(0.01f, (maxCurrent - minCurrent) * 0.1f);
        minCurrent = std::floor(minCurrent * 100 - margin * 100) / 100.0f;
        maxCurrent = std::ceil(maxCurrent * 100 + margin * 100) / 100.0f;

        // Ensure we don't have negative current
        minCurrent = (std::max)(0.0f, minCurrent);

        // Plot the current graph
        ImGui::PlotLines("##current", currentData.data(), static_cast<int>(currentData.size()),
          0, nullptr, minCurrent, maxCurrent, ImVec2(580, 100));
        ImGui::Text("Min: %.3f A   Max: %.3f A", minCurrent, maxCurrent);
      }
      else {
        ImGui::Text("No current data available yet");
      }
    }
  }
  ImGui::End();
}

void CLD101xClient::ToggleWindow() {
  m_showWindow = !m_showWindow;
}

bool CLD101xClient::IsVisible() const {
  return m_showWindow;
}

const std::string& CLD101xClient::GetName() const {
  return m_name;
}


// Add these new methods to cld101x_client.cpp:

void CLD101xClient::EnableGlobalDataStore(bool enable, const std::string& devicePrefix) {
  m_enableGlobalDataStore = enable;
  m_devicePrefix = devicePrefix;

  if (enable && m_globalDataStore) {
    Logger::GetInstance()->LogInfo("CLD101xClient: Enabled Global Data Store with prefix: " + devicePrefix);

    // Immediately publish current values if connected
    if (m_isConnected) {
      std::lock_guard<std::mutex> lock(m_dataMutex);
      m_globalDataStore->SetValue(m_devicePrefix + "-Temperature", m_currentTemperature);
      m_globalDataStore->SetValue(m_devicePrefix + "-LaserCurrent", m_currentLaserCurrent);
      Logger::GetInstance()->LogInfo("CLD101xClient: Published initial values to Global Data Store");
    }
  }
  else {
    Logger::GetInstance()->LogInfo("CLD101xClient: Disabled Global Data Store");
  }
}

void CLD101xClient::DisableGlobalDataStore() {
  m_enableGlobalDataStore = false;
  Logger::GetInstance()->LogInfo("CLD101xClient: Global Data Store disabled");
}


// ============================================================================
// COMPREHENSIVE FIX for CLD101x Status Synchronization Issues
// ============================================================================

// 1. ENHANCED cld101x_client.cpp - Fix status query methods with better error handling

bool CLD101xClient::GetLaserStatus() {
  if (!m_isConnected) {
    return false;
  }

  // Check if cache is still valid
  auto now = std::chrono::steady_clock::now();
  if (now - m_lastStatusQuery < STATUS_CACHE_TIMEOUT) {
    return m_cachedLaserStatus;
  }

  std::string response;
  bool success = false;

  // Try multiple command variations for better compatibility
  std::vector<std::string> commands = {
    "OUTPUT1:STATE?",      // Standard SCPI
    "OUTPut1:STATe?",      // Alternative case
    "output1:state?"       // Lowercase version
  };

  for (const auto& cmd : commands) {
    if (SendCommand(cmd, &response)) {
      success = true;
      break;
    }
    // Short delay between attempts
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  if (success) {
    // Parse response with robust handling
    std::string trimmedResponse = response;

    // Remove all whitespace and control characters
    trimmedResponse.erase(
      std::remove_if(trimmedResponse.begin(), trimmedResponse.end(),
        [](unsigned char c) { return std::isspace(c) || std::iscntrl(c); }),
      trimmedResponse.end()
    );

    // Convert to uppercase for consistent comparison
    std::transform(trimmedResponse.begin(), trimmedResponse.end(),
      trimmedResponse.begin(), ::toupper);

    // Check for various possible responses
    bool isOn = (trimmedResponse == "ON" ||
      trimmedResponse == "1" ||
      trimmedResponse == "TRUE" ||
      trimmedResponse.find("ON") != std::string::npos);

    m_cachedLaserStatus = isOn;
    m_lastStatusQuery = now;

    //Logger::GetInstance()->LogInfo("CLD101xClient: Laser status query successful: '" +
    //  response + "' (cleaned: '" + trimmedResponse + "') -> " +
    //  (m_cachedLaserStatus ? "ON" : "OFF"));

    return m_cachedLaserStatus;
  }
  else {
    Logger::GetInstance()->LogWarning("CLD101xClient: Failed to query laser status with all commands: " + m_lastError);

    // SMART FALLBACK: Infer status from current reading
    if (m_isPolling) {
      float current = GetLatestLaserCurrent();
      bool inferredStatus = (current > 0.001f); // If current > 1mA, laser is probably on

      Logger::GetInstance()->LogInfo("CLD101xClient: Inferring laser status from current: " +
        std::to_string(current) + "A -> " +
        (inferredStatus ? "ON" : "OFF"));

      m_cachedLaserStatus = inferredStatus;
      m_lastStatusQuery = now;
      return m_cachedLaserStatus;
    }

    return false; // Default to OFF if we can't determine
  }
}


bool CLD101xClient::GetTECStatus() {
  if (!m_isConnected) {
    return false;
  }

  // Check if cache is still valid
  auto now = std::chrono::steady_clock::now();
  if (now - m_lastStatusQuery < STATUS_CACHE_TIMEOUT) {
    return m_cachedTECStatus;
  }

  std::string response;
  bool success = false;

  // Try multiple command variations for TEC
  std::vector<std::string> commands = {
    "OUTPUT2:STATE?",      // Standard SCPI
    "OUTPut2:STATe?",      // Alternative case
    "output2:state?"       // Lowercase version
  };

  for (const auto& cmd : commands) {
    if (SendCommand(cmd, &response)) {
      success = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  if (success) {
    // Parse response with robust handling
    std::string trimmedResponse = response;

    // Remove all whitespace and control characters
    trimmedResponse.erase(
      std::remove_if(trimmedResponse.begin(), trimmedResponse.end(),
        [](unsigned char c) { return std::isspace(c) || std::iscntrl(c); }),
      trimmedResponse.end()
    );

    // Convert to uppercase
    std::transform(trimmedResponse.begin(), trimmedResponse.end(),
      trimmedResponse.begin(), ::toupper);

    bool isOn = (trimmedResponse == "ON" ||
      trimmedResponse == "1" ||
      trimmedResponse == "TRUE" ||
      trimmedResponse.find("ON") != std::string::npos);

    m_cachedTECStatus = isOn;
    m_lastStatusQuery = now;

    Logger::GetInstance()->LogInfo("CLD101xClient: TEC status query successful: '" +
      response + "' (cleaned: '" + trimmedResponse + "') -> " +
      (m_cachedTECStatus ? "ON" : "OFF"));

    return m_cachedTECStatus;
  }
  else {
    Logger::GetInstance()->LogWarning("CLD101xClient: Failed to query TEC status with all commands: " + m_lastError);

    // **UPDATED: More conservative TEC inference**
    // Based on your log, it seems the hardware status query is actually working correctly
    // The issue might be that we're over-correcting with temperature inference

    Logger::GetInstance()->LogInfo("CLD101xClient: Using fallback TEC status inference");

    // For now, default to OFF if hardware query fails
    // We'll rely on the cross-validation in SyncHardwareStatus to catch real mismatches
    m_cachedTECStatus = false;
    m_lastStatusQuery = now;

    Logger::GetInstance()->LogInfo("CLD101xClient: TEC status defaulted to OFF (hardware query failed)");
    return false;
  }
}

// ============================================================================
// UPDATED SyncHardwareStatus with improved TEC validation logic
// Replace your existing SyncHardwareStatus() method:
// ============================================================================

// ============================================================================
// FIXED SyncHardwareStatus - Use fresh polling data, not stale cache
// Replace your SyncHardwareStatus method in cld101x_client.cpp:
// ============================================================================

void CLD101xClient::SyncHardwareStatus() {
  if (!m_isConnected) {
    return;
  }

  Logger::GetInstance()->LogInfo("CLD101xClient: Syncing hardware status with cross-validation...");

  // Force cache refresh
  m_lastStatusQuery = std::chrono::steady_clock::time_point{};

  try {
    // **FIX 1: Get FRESH measurements for validation**
    float currentReading = 0.0f;
    float tempReading = 0.0f;
    float setpointReading = 25.0f;

    // Force fresh readings instead of using potentially stale cache
    if (m_isPolling) {
      // Get fresh readings from polling (these should be most recent)
      currentReading = GetLatestLaserCurrent();
      tempReading = GetLatestTemperature();

      Logger::GetInstance()->LogInfo("CLD101xClient: Using polling data - Current: " +
        std::to_string(currentReading) + "A, Temp: " +
        std::to_string(tempReading) + "°C");
    }
    else {
      // If not polling, get direct readings
      std::string currentResponse, tempResponse;
      if (SendCommand("READ_LASER_CURRENT", &currentResponse)) {
        size_t pos = currentResponse.find(": ");
        if (pos != std::string::npos) {
          try {
            currentReading = std::stof(currentResponse.substr(pos + 2));
          }
          catch (...) {}
        }
      }

      if (SendCommand("READ_TEC_TEMPERATURE", &tempResponse)) {
        size_t pos = tempResponse.find(": ");
        if (pos != std::string::npos) {
          try {
            tempReading = std::stof(tempResponse.substr(pos + 2));
          }
          catch (...) {}
        }
      }

      Logger::GetInstance()->LogInfo("CLD101xClient: Using direct readings - Current: " +
        std::to_string(currentReading) + "A, Temp: " +
        std::to_string(tempReading) + "°C");
    }

    // Try to get TEC setpoint for better validation
    std::string setpointResponse;
    if (SendCommand("source2:temperature:spoint?", &setpointResponse)) {
      try {
        size_t pos = setpointResponse.find(": ");
        if (pos != std::string::npos) {
          setpointReading = std::stof(setpointResponse.substr(pos + 2));
        }
        else {
          setpointReading = std::stof(setpointResponse);
        }
      }
      catch (const std::exception& e) {
        Logger::GetInstance()->LogWarning("CLD101xClient: Failed to parse setpoint during sync - using 25°C");
        setpointReading = 25.0f;
      }
    }

    // Query hardware status
    bool laserStatusQuery = GetLaserStatus();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    bool tecStatusQuery = GetTECStatus();

    // **FIX 2: More conservative cross-validation - don't auto-correct immediately**
    bool laserCrossCheck = true;
    bool tecCrossCheck = true;

    // **LASER VALIDATION: Only correct if measurement is very clear**
    if (laserStatusQuery && currentReading < 0.001f) {
      Logger::GetInstance()->LogWarning("CLD101xClient: Laser status mismatch - Shows ON but current is " +
        std::to_string(currentReading) + "A");
      laserCrossCheck = false;

      // **CONSERVATIVE: Only auto-correct if current is really zero**
      if (currentReading < 0.0001f) {
        m_cachedLaserStatus = false;
        Logger::GetInstance()->LogInfo("CLD101xClient: Correcting laser status to OFF (current is zero)");
      }
    }
    else if (!laserStatusQuery && currentReading > 0.01f) {
      Logger::GetInstance()->LogWarning("CLD101xClient: Laser status mismatch - Shows OFF but current is " +
        std::to_string(currentReading) + "A");
      laserCrossCheck = false;

      // **CONSERVATIVE: Only auto-correct if current is significantly high**
      if (currentReading > 0.05f) {
        m_cachedLaserStatus = true;
        Logger::GetInstance()->LogInfo("CLD101xClient: Correcting laser status to ON (high current detected)");
      }
    }

    // **TEC VALIDATION: Just log, don't auto-correct**
    float tempDifference = std::abs(tempReading - setpointReading);

    Logger::GetInstance()->LogInfo("CLD101xClient: TEC analysis - Status: " +
      std::string(tecStatusQuery ? "ON" : "OFF") +
      ", Temp: " + std::to_string(tempReading) + "°C" +
      ", Setpoint: " + std::to_string(setpointReading) + "°C" +
      ", Diff: " + std::to_string(tempDifference) + "°C");

    // **NO AUTO-CORRECTION for TEC - trust hardware status**
    // The TEC behavior seems complex, so let's not second-guess the hardware

    Logger::GetInstance()->LogInfo("CLD101xClient: Hardware status sync complete:");
    Logger::GetInstance()->LogInfo("  Laser: " + std::string(m_cachedLaserStatus ? "ON" : "OFF") +
      " (Current: " + std::to_string(currentReading) + "A)" +
      (laserCrossCheck ? "" : " [MISMATCH DETECTED]"));
    Logger::GetInstance()->LogInfo("  TEC: " + std::string(m_cachedTECStatus ? "ON" : "OFF") +
      " (Temp: " + std::to_string(tempReading) + "°C, Setpoint: " +
      std::to_string(setpointReading) + "°C, Diff: " +
      std::to_string(tempDifference) + "°C)");
  }
  catch (const std::exception& e) {
    Logger::GetInstance()->LogError("CLD101xClient: Hardware status sync failed: " + std::string(e.what()));
  }
}

void CLD101xClient::AnalyzeTECBehavior() {
  if (!m_isConnected || !m_isPolling) {
    return;
  }

  Logger::GetInstance()->LogInfo("CLD101xClient: === TEC BEHAVIOR ANALYSIS ===");

  // Get current readings
  float currentTemp = GetLatestTemperature();
  bool currentTECStatus = GetTECStatus();

  // Try to get setpoint
  std::string setpointResponse;
  float setpoint = 25.0f;
  if (SendCommand("source2:temperature:spoint?", &setpointResponse)) {
    try {
      size_t pos = setpointResponse.find(": ");
      if (pos != std::string::npos) {
        setpoint = std::stof(setpointResponse.substr(pos + 2));
      }
      else {
        setpoint = std::stof(setpointResponse);
      }
    }
    catch (...) {
      setpoint = 25.0f;
    }
  }

  float tempDiff = currentTemp - setpoint;

  Logger::GetInstance()->LogInfo("  Current Status: " + std::string(currentTECStatus ? "ON" : "OFF"));
  Logger::GetInstance()->LogInfo("  Current Temp: " + std::to_string(currentTemp) + "°C");
  Logger::GetInstance()->LogInfo("  Setpoint: " + std::to_string(setpoint) + "°C");
  Logger::GetInstance()->LogInfo("  Difference: " + std::to_string(tempDiff) + "°C " +
    (tempDiff > 0 ? "(ABOVE setpoint)" : "(BELOW setpoint)"));
  Logger::GetInstance()->LogInfo("================================");
}