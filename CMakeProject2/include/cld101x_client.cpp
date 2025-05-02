#include "include/cld101x_client.h"
#include "include/logger.h"
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
  , m_isPolling(false)
  , m_pollingIntervalMs(1000)
  , m_showWindow(true)
  , m_name("CLD101x Controller")
{
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

  // Start polling by default
  StartPolling();

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

bool CLD101xClient::SendCommand(const std::string& command, std::string* response) {
  if (!m_isConnected) {
    m_lastError = "Not connected to server";
    return false;
  }

  // Send the command
  if (send(m_socket, command.c_str(), command.length(), 0) == SOCKET_ERROR) {
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

float CLD101xClient::GetTemperature() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  return m_currentTemperature;
}

float CLD101xClient::GetLaserCurrent() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  return m_currentLaserCurrent;
}

void CLD101xClient::StartPolling(int intervalMs) {
  // Don't start if already polling
  if (m_isPolling) {
    return;
  }

  // Set polling interval (force to 500ms as per requirements)
  m_pollingIntervalMs = 500; // Override the parameter to always use 500ms

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

void CLD101xClient::PollingThread() {
  Logger* logger = Logger::GetInstance();
  logger->LogInfo("CLD101xClient: Polling thread started");

  while (m_isPolling && m_isConnected) {
    // Get current time for history
    auto now = std::chrono::steady_clock::now();

    // Read temperature
    std::string tempResponse;
    if (SendCommand("READ_TEC_TEMPERATURE", &tempResponse)) {
      // Parse temperature value - format: "Current TEC temperature [C]: XX.XX"
      size_t pos = tempResponse.find(": ");
      if (pos != std::string::npos) {
        try {
          float temp = std::stof(tempResponse.substr(pos + 2));

          // Update temperature
          {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            m_currentTemperature = temp;
            m_temperatureHistory.push_back({ now, temp });

            // Limit history size
            if (m_temperatureHistory.size() > MAX_HISTORY_SIZE) {
              m_temperatureHistory.pop_front();
            }
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
      // Parse current value - format: "Current laser current [A]: X.XXX"
      size_t pos = currentResponse.find(": ");
      if (pos != std::string::npos) {
        try {
          float current = std::stof(currentResponse.substr(pos + 2));

          // Update current
          {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            m_currentLaserCurrent = current;
            m_currentHistory.push_back({ now, current });

            // Limit history size
            if (m_currentHistory.size() > MAX_HISTORY_SIZE) {
              m_currentHistory.pop_front();
            }
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

void CLD101xClient::RenderUI() {
  if (!m_showWindow) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
  if (ImGui::Begin(m_name.c_str(), &m_showWindow)) {
    // Connection status and controls
    if (!m_isConnected) {
      // IP and port input fields
      static char ipBuffer[64] = "127.0.0.88"; // Default from your server
      static int port = 65432; // Default from your server

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

      // Polling controls
      if (m_isPolling) {
        if (ImGui::Button("Stop Polling")) {
          StopPolling();
        }
        ImGui::SameLine();
        ImGui::Text("Polling every %d ms", m_pollingIntervalMs);
      }
      else {
        // Fixed polling interval of 500ms
        static int interval = 500;
        ImGui::Text("Interval: 500 ms");
        ImGui::SameLine();
        if (ImGui::Button("Start Polling")) {
          StartPolling(interval);
        }
      }

      ImGui::Separator();

      // Current readings section
      ImGui::Text("Current Temperature: %.2f C", GetTemperature());
      ImGui::Text("Current Laser Current: %.3f A", GetLaserCurrent());

      ImGui::Separator();

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