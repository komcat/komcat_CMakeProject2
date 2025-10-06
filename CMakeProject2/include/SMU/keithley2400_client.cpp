#include "include/SMU/keithley2400_client.h"
#include "include/logger.h"
#include "imgui.h"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>
// Add this include at the top of keithley2400_client.cpp
#include "include/data/global_data_store.h"
using json = nlohmann::json;

Keithley2400Client::Keithley2400Client()
  : m_socket(INVALID_SOCKET)
  , m_serverPort(0)
  , m_isConnected(false)
  , m_isPolling(false)
  , m_pollingIntervalMs(250)
  , m_showWindow(true)
  , m_name("Keithley 2400 Controller")
{
#ifdef _WIN32
  // Initialize Winsock for Windows
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    Logger::GetInstance()->LogError("Keithley2400Client: WSAStartup failed");
  }
#endif

  // Initialize latest reading
  m_latestReading = {};
  m_latestReading.timestamp = std::chrono::steady_clock::now();

  // Print data store keys that will be used
  Logger* logger = Logger::GetInstance();
  logger->LogInfo("Keithley2400Client: Initialized - Data store keys will be:");
  logger->LogInfo("  - " + m_name + "-Voltage");
  logger->LogInfo("  - " + m_name + "-Current");
  logger->LogInfo("  - " + m_name + "-Resistance");
  logger->LogInfo("  - " + m_name + "-Power");
  logger->LogInfo("  - GPIB-Current (legacy key for current)");

  Logger::GetInstance()->LogInfo("Keithley2400Client: Initialized");
}
Keithley2400Client::~Keithley2400Client() {
  StopPolling();
  Disconnect();

#ifdef _WIN32
  WSACleanup();
#endif
  Logger::GetInstance()->LogInfo("Keithley2400Client: Destroyed");
}

bool Keithley2400Client::Connect(const std::string& ip, int port) {
  Logger* logger = Logger::GetInstance();

  if (m_isConnected) {
    Disconnect();
  }

  m_serverIp = ip;
  m_serverPort = port;

  // Create socket
  m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (m_socket == INVALID_SOCKET) {
    m_lastError = "Error creating socket";
    logger->LogError("Keithley2400Client: " + m_lastError);
    return false;
  }

  // Setup server address
  sockaddr_in serverAddr;
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(port);

  if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) <= 0) {
    m_lastError = "Invalid address: " + ip;
    logger->LogError("Keithley2400Client: " + m_lastError);
    closesocket(m_socket);
    m_socket = INVALID_SOCKET;
    return false;
  }

  // Connect to server
  if (connect(m_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
    m_lastError = "Connection failed to " + ip + ":" + std::to_string(port);
    logger->LogError("Keithley2400Client: " + m_lastError);
    closesocket(m_socket);
    m_socket = INVALID_SOCKET;
    return false;
  }

  m_isConnected = true;
  logger->LogInfo("Keithley2400Client: Connected to " + ip + ":" + std::to_string(port));

  // Get initial status
  std::string instrumentId, outputState, sourceFunction;
  GetStatus(instrumentId, outputState, sourceFunction);

  // Start polling by default
  // //DO NOT START POLLING
  //StartPolling();



  return true;
}

void Keithley2400Client::Disconnect() {
  if (!m_isConnected) {
    return;
  }

  StopPolling();

  if (m_socket != INVALID_SOCKET) {
    closesocket(m_socket);
    m_socket = INVALID_SOCKET;
  }

  m_isConnected = false;
  Logger::GetInstance()->LogInfo("Keithley2400Client: Disconnected");
}

bool Keithley2400Client::IsConnected() const {
  return m_isConnected;
}

bool Keithley2400Client::SendJsonCommand(const std::string& type, const std::string& data, std::string* response) {
  if (!m_isConnected) {
    m_lastError = "Not connected to server";
    return false;
  }

  try {
    // Create JSON command
    json command;
    command["type"] = type;

    if (!data.empty()) {
      command["data"] = json::parse(data);
    }

    std::string jsonStr = command.dump();
    //Logger::GetInstance()->LogInfo("Sending: " + jsonStr); // ADD THIS LINE

    // Send command
    auto length = jsonStr.length();
    if (length > INT_MAX) {
      m_lastError = "Message too large to send";
      return false;
    }

    if (send(m_socket, jsonStr.c_str(), static_cast<int>(length), 0) == SOCKET_ERROR) {
      m_lastError = "Failed to send command";
      return false;
    }

    // Receive response if requested
    if (response) {
      char buffer[4096] = { 0 };
      int bytesReceived = recv(m_socket, buffer, sizeof(buffer) - 1, 0);

      if (bytesReceived <= 0) {
        m_lastError = "No response received";
        return false;
      }

      *response = std::string(buffer, bytesReceived);
    }

    return true;
  }
  catch (const std::exception& e) {
    m_lastError = "JSON error: " + std::string(e.what());
    return false;
  }
}

bool Keithley2400Client::ResetInstrument() {
  std::string response;
  bool result = SendJsonCommand("reset", "", &response);

  if (result) {
    try {
      json jsonResponse = json::parse(response);
      if (jsonResponse["status"] == "success") {
        Logger::GetInstance()->LogInfo("Keithley2400Client: " + jsonResponse["message"].get<std::string>());
        return true;
      }
      else {
        m_lastError = jsonResponse["message"];
        return false;
      }
    }
    catch (const std::exception& ) {
      m_lastError = "Failed to parse reset response";
      return false;
    }
  }

  Logger::GetInstance()->LogError("Keithley2400Client: Failed to reset instrument - " + m_lastError);
  return false;
}

//auto start polling
bool Keithley2400Client::SetOutput(bool enable) {
  json data;
  data["state"] = enable ? "ON" : "OFF";

  std::string response;
  bool result = SendJsonCommand("output", data.dump(), &response);

  if (result) {
    try {
      json jsonResponse = json::parse(response);
      if (jsonResponse["status"] == "success") {
        m_uiState.outputEnabled = enable;
        m_uiState.outputStatus = enable ? "ON" : "OFF";

        // Auto-start/stop polling based on output state
        if (enable) {
          if (!m_isPolling) {
            StartPolling(m_pollingIntervalMs);
            Logger::GetInstance()->LogInfo("Keithley2400Client: Auto-started polling (output ON)");
          }
        }
        else {
          if (m_isPolling) {
            StopPolling();
            Logger::GetInstance()->LogInfo("Keithley2400Client: Auto-stopped polling (output OFF)");
          }
        }

        Logger::GetInstance()->LogInfo("Keithley2400Client: " + jsonResponse["message"].get<std::string>());
        return true;
      }
      else {
        m_lastError = jsonResponse["message"];
        return false;
      }
    }
    catch (const std::exception& ) {
      m_lastError = "Failed to parse output response";
      return false;
    }
  }

  Logger::GetInstance()->LogError("Keithley2400Client: Failed to set output - " + m_lastError);
  return false;
}
bool Keithley2400Client::GetStatus(std::string& instrumentId, std::string& outputState, std::string& sourceFunction) {
  std::string response;
  bool result = SendJsonCommand("get_status", "", &response);

  if (result) {
    try {
      json jsonResponse = json::parse(response);
      if (jsonResponse["status"] == "success" && jsonResponse.contains("data")) {
        auto data = jsonResponse["data"];
        instrumentId = data.value("instrument", "Unknown");
        outputState = data.value("output", "OFF");
        sourceFunction = data.value("source_function", "VOLT");

        // Update UI state
        m_uiState.instrumentStatus = instrumentId;
        m_uiState.outputStatus = outputState;
        m_uiState.sourceFunction = sourceFunction;
        m_uiState.outputEnabled = (outputState == "ON");

        return true;
      }
      else {
        m_lastError = jsonResponse.value("message", "Unknown error");
        return false;
      }
    }
    catch (const std::exception& ) {
      m_lastError = "Failed to parse status response";
      return false;
    }
  }

  return false;
}

bool Keithley2400Client::SetupVoltageSource(double voltage, double compliance, const std::string& range) {
  json data;
  data["voltage"] = voltage;
  data["compliance"] = compliance;
  data["range"] = range;

  std::string response;
  bool result = SendJsonCommand("setup_voltage_source", data.dump(), &response);

  if (result) {
    try {
      json jsonResponse = json::parse(response);
      if (jsonResponse["status"] == "success") {
        m_uiState.sourceMode = 0; // Voltage mode
        m_uiState.voltageSetpoint = voltage;
        m_uiState.compliance = compliance;
        Logger::GetInstance()->LogInfo("Keithley2400Client: " + jsonResponse["message"].get<std::string>());
        return true;
      }
      else {
        m_lastError = jsonResponse["message"];
        return false;
      }
    }
    catch (const std::exception& ) {
      m_lastError = "Failed to parse voltage source response";
      return false;
    }
  }

  Logger::GetInstance()->LogError("Keithley2400Client: Failed to setup voltage source - " + m_lastError);
  return false;
}

bool Keithley2400Client::SetupCurrentSource(double current, double compliance, const std::string& range) {
  json data;
  data["current"] = current;
  data["compliance"] = compliance;
  data["range"] = range;

  std::string response;
  bool result = SendJsonCommand("setup_current_source", data.dump(), &response);

  if (result) {
    try {
      json jsonResponse = json::parse(response);
      if (jsonResponse["status"] == "success") {
        m_uiState.sourceMode = 1; // Current mode
        m_uiState.currentSetpoint = current;
        m_uiState.compliance = compliance;
        Logger::GetInstance()->LogInfo("Keithley2400Client: " + jsonResponse["message"].get<std::string>());
        return true;
      }
      else {
        m_lastError = jsonResponse["message"];
        return false;
      }
    }
    catch (const std::exception& e) {
      m_lastError = "Failed to parse current source response";
      return false;
    }
  }

  Logger::GetInstance()->LogError("Keithley2400Client: Failed to setup current source - " + m_lastError);
  return false;
}

bool Keithley2400Client::SendWriteCommand(const std::string& command) {
  json data;
  data["command"] = command;

  std::string response;
  bool result = SendJsonCommand("write", data.dump(), &response);

  if (result) {
    try {
      json jsonResponse = json::parse(response);
      if (jsonResponse["status"] == "success") {
        Logger::GetInstance()->LogInfo("Keithley2400Client: Write command executed: " + command);
        return true;
      }
      else {
        m_lastError = jsonResponse["message"];
        return false;
      }
    }
    catch (const std::exception& ) {
      m_lastError = "Failed to parse write response";
      return false;
    }
  }

  return false;
}

bool Keithley2400Client::SendQueryCommand(const std::string& command, std::string& response) {
  json data;
  data["command"] = command;

  std::string jsonResponse;
  bool result = SendJsonCommand("query", data.dump(), &jsonResponse);

  if (result) {
    try {
      json parsed = json::parse(jsonResponse);
      if (parsed["status"] == "success" && parsed.contains("data")) {
        response = parsed["data"];
        return true;
      }
      else {
        m_lastError = parsed.value("message", "Unknown error");
        return false;
      }
    }
    catch (const std::exception& ) {
      m_lastError = "Failed to parse query response";
      return false;
    }
  }

  return false;
}

bool Keithley2400Client::ReadMeasurement(Keithley2400Reading& reading) {
  std::string response;
  bool result = SendJsonCommand("read", "", &response);

  if (result) {
    return ParseMeasurement(response, reading);
  }

  return false;
}

bool Keithley2400Client::ParseMeasurement(const std::string& jsonResponse, Keithley2400Reading& reading) {
  try {
    json parsed = json::parse(jsonResponse);
    if (parsed["status"] == "success" && parsed.contains("data")) {
      auto data = parsed["data"];
      reading.voltage = data.value("voltage", 0.0);
      reading.current = data.value("current", 0.0);
      reading.resistance = data.value("resistance", 0.0);
      reading.power = data.value("power", 0.0);
      reading.timestamp = std::chrono::steady_clock::now();
      return true;
    }
    else {
      m_lastError = parsed.value("message", "Unknown error");
      return false;
    }
  }
  catch (const std::exception& ) {
    m_lastError = "Failed to parse measurement response";
    return false;
  }
}

// Fix the VoltageSweep method in keithley2400_client.cpp

bool Keithley2400Client::VoltageSweep(double start, double stop, int steps, double compliance, double delay,
  std::vector<VoltageSweepResult>& results) {

  Logger* logger = Logger::GetInstance();
  logger->LogInfo("Keithley2400Client: Starting voltage sweep from " +
    std::to_string(start) + "V to " + std::to_string(stop) + "V with " +
    std::to_string(steps) + " steps");

  json data;
  data["start"] = start;
  data["stop"] = stop;
  data["steps"] = steps;
  data["compliance"] = compliance;
  data["delay"] = delay;

  std::string response;
  bool result = SendJsonCommand("voltage_sweep", data.dump(), &response);

  if (result) {
    logger->LogInfo("Keithley2400Client: Received sweep response, parsing...");

    try {
      json jsonResponse = json::parse(response);

      if (jsonResponse["status"] == "success" && jsonResponse.contains("data")) {
        results.clear();
        auto sweepData = jsonResponse["data"];

        logger->LogInfo("Keithley2400Client: Parsing " + std::to_string(sweepData.size()) + " sweep points");

        for (const auto& point : sweepData) {
          VoltageSweepResult sweepResult;
          sweepResult.setVoltage = point.value("set_voltage", 0.0);
          sweepResult.measuredVoltage = point.value("measured_voltage", 0.0);
          sweepResult.measuredCurrent = point.value("measured_current", 0.0);
          sweepResult.timestamp = std::chrono::steady_clock::now();
          results.push_back(sweepResult);
        }

        logger->LogInfo("Keithley2400Client: Voltage sweep completed successfully with " +
          std::to_string(results.size()) + " points");
        return true;
      }
      else {
        m_lastError = jsonResponse.value("message", "Unknown error in sweep response");
        logger->LogError("Keithley2400Client: Sweep failed - " + m_lastError);
        return false;
      }
    }
    catch (const std::exception& e) {
      m_lastError = "Failed to parse voltage sweep response: " + std::string(e.what());
      logger->LogError("Keithley2400Client: " + m_lastError);
      return false;
    }
  }

  logger->LogError("Keithley2400Client: Failed to send voltage sweep command - " + m_lastError);
  return false;
}



bool Keithley2400Client::CurrentSweep(double start, double stop, int steps, double compliance, double delay,
  std::vector<CurrentSweepResult>& results) {

  Logger* logger = Logger::GetInstance();
  logger->LogInfo("Keithley2400Client: Starting current sweep from " +
    std::to_string(start) + "A to " + std::to_string(stop) + "A with " +
    std::to_string(steps) + " steps");

  json data;
  data["start"] = start;
  data["stop"] = stop;
  data["steps"] = steps;
  data["compliance"] = compliance;
  data["delay"] = delay;

  std::string response;
  bool result = SendJsonCommand("current_sweep", data.dump(), &response);

  if (result) {
    logger->LogInfo("Keithley2400Client: Received current sweep response, parsing...");

    try {
      json jsonResponse = json::parse(response);

      if (jsonResponse["status"] == "success" && jsonResponse.contains("data")) {
        results.clear();
        auto sweepData = jsonResponse["data"];

        logger->LogInfo("Keithley2400Client: Parsing " + std::to_string(sweepData.size()) + " sweep points");

        for (const auto& point : sweepData) {
          CurrentSweepResult sweepResult;
          sweepResult.setCurrent = point.value("set_current", 0.0);
          sweepResult.measuredVoltage = point.value("measured_voltage", 0.0);
          sweepResult.measuredCurrent = point.value("measured_current", 0.0);
          sweepResult.resistance = point.value("resistance", 0.0);
          sweepResult.power = point.value("power", 0.0);
          sweepResult.timestamp = std::chrono::steady_clock::now();
          results.push_back(sweepResult);
        }

        logger->LogInfo("Keithley2400Client: Current sweep completed successfully with " +
          std::to_string(results.size()) + " points");
        return true;
      }
      else {
        m_lastError = jsonResponse.value("message", "Unknown error in current sweep response");
        logger->LogError("Keithley2400Client: Current sweep failed - " + m_lastError);
        return false;
      }
    }
    catch (const std::exception& e) {
      m_lastError = "Failed to parse current sweep response: " + std::string(e.what());
      logger->LogError("Keithley2400Client: " + m_lastError);
      return false;
    }
  }

  logger->LogError("Keithley2400Client: Failed to send current sweep command - " + m_lastError);
  return false;
}



Keithley2400Reading Keithley2400Client::GetLatestReading() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  return m_latestReading;
}

double Keithley2400Client::GetVoltage() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  return m_latestReading.voltage;
}

double Keithley2400Client::GetCurrent() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  return m_latestReading.current;
}

double Keithley2400Client::GetResistance() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  return m_latestReading.resistance;
}

double Keithley2400Client::GetPower() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  return m_latestReading.power;
}

void Keithley2400Client::StartPolling(int intervalMs) {
  if (m_isPolling) {
    return;
  }

  m_pollingIntervalMs = intervalMs;
  m_isPolling = true;
  m_pollingThread = std::thread(&Keithley2400Client::PollingThread, this);

  Logger::GetInstance()->LogInfo("Keithley2400Client: Started polling thread with interval " +
    std::to_string(m_pollingIntervalMs) + "ms");
}

void Keithley2400Client::StopPolling() {
  if (!m_isPolling) {
    return;
  }

  m_isPolling = false;

  if (m_pollingThread.joinable()) {
    m_pollingThread.join();
  }

  Logger::GetInstance()->LogInfo("Keithley2400Client: Stopped polling thread");
}

bool Keithley2400Client::IsPolling() const {
  return m_isPolling;
}  // <-- ADD THIS METHOD


void Keithley2400Client::PollingThread() {
  Logger* logger = Logger::GetInstance();
  logger->LogInfo("Keithley2400Client: Polling thread started");

  while (m_isPolling && m_isConnected) {
    Keithley2400Reading reading;
    if (ReadMeasurement(reading)) {
      {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_latestReading = reading;
        m_readingHistory.push_back(reading);

        // Limit history size
        if (m_readingHistory.size() > MAX_HISTORY_SIZE) {
          m_readingHistory.pop_front();
        }

        // NEW: Store in GlobalDataStore
        GlobalDataStore* dataStore = GlobalDataStore::GetInstance();

        // Use client name as prefix for unique IDs
        std::string baseId = m_name; // Or use a simpler ID like "GPIB-Current"

        // Store individual measurements
        dataStore->SetValue(baseId + "-Voltage", static_cast<float>(reading.voltage));
        dataStore->SetValue(baseId + "-Current", static_cast<float>(reading.current));
        dataStore->SetValue(baseId + "-Resistance", static_cast<float>(reading.resistance));
        dataStore->SetValue(baseId + "-Power", static_cast<float>(reading.power));




      }
    }
    else {
      logger->LogWarning("Keithley2400Client: Failed to read measurement - " + m_lastError);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(m_pollingIntervalMs));
  }

  logger->LogInfo("Keithley2400Client: Polling thread stopped");
}

void Keithley2400Client::RenderUI() {
  if (!m_showWindow) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);
  if (ImGui::Begin(m_name.c_str(), &m_showWindow)) {
    // Connection status
    if (!m_isConnected) {
      static char ipBuffer[64] = "127.0.0.101";
      static int port = 8888;

      ImGui::Text("Status: Disconnected");
      ImGui::InputText("IP Address", ipBuffer, sizeof(ipBuffer));
      ImGui::InputInt("Port", &port);

      if (ImGui::Button("Connect")) {
        Connect(ipBuffer, port);
      }
    }
    else {
      ImGui::Text("Status: Connected to %s:%d", m_serverIp.c_str(), m_serverPort);
      ImGui::Text("Instrument: %s", m_uiState.instrumentStatus.c_str());

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
        static int interval = 250;
        ImGui::InputInt("Interval (ms)", &interval);
        ImGui::SameLine();
        if (ImGui::Button("Start Polling")) {
          StartPolling(interval);
        }
      }

      ImGui::Separator();

      // Current readings
      auto reading = GetLatestReading();
      ImGui::Text("Latest Readings:");
      ImGui::Text("  Voltage: %.6f V", reading.voltage);
      ImGui::Text("  Current: %.9f A (%.3f mA)", reading.current, reading.current * 1000.0);
      ImGui::Text("  Resistance: %.2f Ohms", reading.resistance);
      ImGui::Text("  Power: %.9f W", reading.power);

      ImGui::Separator();

      // Source mode selection
      ImGui::Text("Source Mode:");
      const char* sourceModes[] = { "Voltage Source", "Current Source" };
      ImGui::Combo("Mode", &m_uiState.sourceMode, sourceModes, IM_ARRAYSIZE(sourceModes));

      if (m_uiState.sourceMode == 0) {
        // Voltage source controls
        ImGui::Text("Voltage Source Controls:");

        // Voltage slider and manual input
        static float voltageFloat = static_cast<float>(m_uiState.voltageSetpoint);
        if (ImGui::SliderFloat("Voltage (V)", &voltageFloat, -20.0f, 20.0f, "%.3f")) {
          m_uiState.voltageSetpoint = static_cast<double>(voltageFloat);
        }
        ImGui::SameLine();
        ImGui::PushItemWidth(80);
        if (ImGui::InputFloat("##VoltageInput", &voltageFloat, 0.0f, 0.0f, "%.3f")) {
          // Clamp to valid range
          voltageFloat = (std::max)(-20.0f, (std::min)(20.0f, voltageFloat));
          m_uiState.voltageSetpoint = static_cast<double>(voltageFloat);
        }
        ImGui::PopItemWidth();

        // Compliance slider and manual input
        static float complianceFloat = static_cast<float>(m_uiState.compliance);
        if (ImGui::SliderFloat("Current Compliance (A)", &complianceFloat, 0.001f, 1.0f, "%.3f")) {
          m_uiState.compliance = static_cast<double>(complianceFloat);
        }
        ImGui::SameLine();
        ImGui::PushItemWidth(80);
        if (ImGui::InputFloat("##ComplianceInput", &complianceFloat, 0.0f, 0.0f, "%.3f")) {
          // Clamp to valid range
          complianceFloat = (std::max)(0.001f, (std::min)(1.0f, complianceFloat));
          m_uiState.compliance = static_cast<double>(complianceFloat);
        }
        ImGui::PopItemWidth();

        if (ImGui::Button("Setup Voltage Source")) {
          SetupVoltageSource(m_uiState.voltageSetpoint, m_uiState.compliance);
        }
      }
      else {
        // Current source controls
        ImGui::Text("Current Source Controls:");

        // Current slider and manual input
        static float currentFloat = static_cast<float>(m_uiState.currentSetpoint);
        if (ImGui::SliderFloat("Current (A)", &currentFloat, 0.0f, 1.0f, "%.6f")) {
          m_uiState.currentSetpoint = static_cast<double>(currentFloat);
        }
        ImGui::SameLine();
        ImGui::PushItemWidth(80);
        if (ImGui::InputFloat("##CurrentInput", &currentFloat, 0.0f, 0.0f, "%.6f")) {
          // Clamp to valid range
          currentFloat = (std::max)(0.0f, (std::min)(1.0f, currentFloat));
          m_uiState.currentSetpoint = static_cast<double>(currentFloat);
        }
        ImGui::PopItemWidth();

        // Voltage compliance slider and manual input
        static float complianceFloatV = static_cast<float>(m_uiState.compliance);
        if (ImGui::SliderFloat("Voltage Compliance (V)", &complianceFloatV, 1.0f, 200.0f, "%.1f")) {
          m_uiState.compliance = static_cast<double>(complianceFloatV);
        }
        ImGui::SameLine();
        ImGui::PushItemWidth(80);
        if (ImGui::InputFloat("##VComplianceInput", &complianceFloatV, 0.0f, 0.0f, "%.1f")) {
          // Clamp to valid range
          complianceFloatV = (std::max)(1.0f, (std::min)(200.0f, complianceFloatV));
          m_uiState.compliance = static_cast<double>(complianceFloatV);
        }
        ImGui::PopItemWidth();

        if (ImGui::Button("Setup Current Source")) {
          SetupCurrentSource(m_uiState.currentSetpoint, m_uiState.compliance);
        }
      }

      ImGui::Separator();

      // Output control
      ImGui::Text("Output Control:");
      ImGui::Text("Output Status: %s", m_uiState.outputStatus.c_str());

      if (ImGui::Button("Output ON")) {
        SetOutput(true);
      }
      ImGui::SameLine();
      if (ImGui::Button("Output OFF")) {
        SetOutput(false);
      }
      ImGui::SameLine();
      if (ImGui::Button("Reset Instrument")) {
        ResetInstrument();
      }

      ImGui::Separator();

      // ENHANCED: Tabbed sweep interface
      ImGui::Text("Measurement Sweeps:");

      if (ImGui::BeginTabBar("SweepTypeTabs", ImGuiTabBarFlags_None)) {

        // VOLTAGE SWEEP TAB
        if (ImGui::BeginTabItem("⚡ Voltage Sweep")) {
          ImGui::Spacing();

          // Voltage sweep parameter controls
          static float sweepStartFloat = static_cast<float>(m_uiState.sweepStart);
          if (ImGui::SliderFloat("Start (V)", &sweepStartFloat, -20.0f, 20.0f, "%.2f")) {
            m_uiState.sweepStart = static_cast<double>(sweepStartFloat);
          }
          ImGui::SameLine();
          ImGui::PushItemWidth(80);
          if (ImGui::InputFloat("##VSweepStartInput", &sweepStartFloat, 0.0f, 0.0f, "%.2f")) {
            sweepStartFloat = (std::max)(-20.0f, (std::min)(20.0f, sweepStartFloat));
            m_uiState.sweepStart = static_cast<double>(sweepStartFloat);
          }
          ImGui::PopItemWidth();

          static float sweepStopFloat = static_cast<float>(m_uiState.sweepStop);
          if (ImGui::SliderFloat("Stop (V)", &sweepStopFloat, -20.0f, 20.0f, "%.2f")) {
            m_uiState.sweepStop = static_cast<double>(sweepStopFloat);
          }
          ImGui::SameLine();
          ImGui::PushItemWidth(80);
          if (ImGui::InputFloat("##VSweepStopInput", &sweepStopFloat, 0.0f, 0.0f, "%.2f")) {
            sweepStopFloat = (std::max)(-20.0f, (std::min)(20.0f, sweepStopFloat));
            m_uiState.sweepStop = static_cast<double>(sweepStopFloat);
          }
          ImGui::PopItemWidth();

          ImGui::SliderInt("Steps", &m_uiState.sweepSteps, 2, 100);
          ImGui::SameLine();
          ImGui::PushItemWidth(80);
          ImGui::InputInt("##VSweepStepsInput", &m_uiState.sweepSteps, 0, 0);
          m_uiState.sweepSteps = (std::max)(2, (std::min)(100, m_uiState.sweepSteps));
          ImGui::PopItemWidth();

          static float sweepComplianceFloat = static_cast<float>(m_uiState.sweepCompliance);
          if (ImGui::SliderFloat("Current Compliance (A)", &sweepComplianceFloat, 0.001f, 1.0f, "%.3f")) {
            m_uiState.sweepCompliance = static_cast<double>(sweepComplianceFloat);
          }
          ImGui::SameLine();
          ImGui::PushItemWidth(80);
          if (ImGui::InputFloat("##VSweepComplianceInput", &sweepComplianceFloat, 0.0f, 0.0f, "%.3f")) {
            sweepComplianceFloat = (std::max)(0.001f, (std::min)(1.0f, sweepComplianceFloat));
            m_uiState.sweepCompliance = static_cast<double>(sweepComplianceFloat);
          }
          ImGui::PopItemWidth();

          static float sweepDelayFloat = static_cast<float>(m_uiState.sweepDelay);
          if (ImGui::SliderFloat("Delay (s)", &sweepDelayFloat, 0.01f, 1.0f, "%.3f")) {
            m_uiState.sweepDelay = static_cast<double>(sweepDelayFloat);
          }
          ImGui::SameLine();
          ImGui::PushItemWidth(80);
          if (ImGui::InputFloat("##VSweepDelayInput", &sweepDelayFloat, 0.0f, 0.0f, "%.3f")) {
            sweepDelayFloat = (std::max)(0.01f, (std::min)(1.0f, sweepDelayFloat));
            m_uiState.sweepDelay = static_cast<double>(sweepDelayFloat);
          }
          ImGui::PopItemWidth();

          ImGui::Spacing();

          if (ImGui::Button("🚀 Perform Voltage Sweep", ImVec2(-1, 40))) {
            Logger::GetInstance()->LogInfo("Starting voltage sweep from " +
              std::to_string(m_uiState.sweepStart) + "V to " +
              std::to_string(m_uiState.sweepStop) + "V");

            std::vector<VoltageSweepResult> sweepResults;
            if (VoltageSweep(m_uiState.sweepStart, m_uiState.sweepStop, m_uiState.sweepSteps,
              m_uiState.sweepCompliance, m_uiState.sweepDelay, sweepResults)) {

              Logger::GetInstance()->LogInfo("Voltage sweep completed with " + std::to_string(sweepResults.size()) + " points");

              // Print results to console
              std::cout << "\n" << std::string(70, '=') << std::endl;
              std::cout << "           VOLTAGE SWEEP RESULTS" << std::endl;
              std::cout << std::string(70, '=') << std::endl;
              std::cout << "Points: " << sweepResults.size() << std::endl;
              std::cout << "Range:  " << m_uiState.sweepStart << "V to " << m_uiState.sweepStop << "V" << std::endl;
              std::cout << std::string(70, '-') << std::endl;

              std::cout << std::left
                << std::setw(6) << "Step"
                << std::setw(12) << "Set V"
                << std::setw(15) << "Measured V"
                << std::setw(15) << "Current (A)"
                << std::setw(15) << "Current (mA)"
                << "Power (mW)" << std::endl;
              std::cout << std::string(70, '-') << std::endl;

              for (size_t i = 0; i < sweepResults.size(); ++i) {
                const auto& point = sweepResults[i];
                double powerMW = point.measuredVoltage * point.measuredCurrent * 1000.0;

                std::cout << std::left
                  << std::setw(6) << (i + 1)
                  << std::setw(12) << std::fixed << std::setprecision(3) << point.setVoltage
                  << std::setw(15) << std::fixed << std::setprecision(6) << point.measuredVoltage
                  << std::setw(15) << std::scientific << std::setprecision(3) << point.measuredCurrent
                  << std::setw(15) << std::fixed << std::setprecision(6) << (point.measuredCurrent * 1000.0)
                  << std::fixed << std::setprecision(6) << powerMW << std::endl;
              }
              std::cout << std::string(70, '=') << "\n" << std::endl;
            }
          }

          ImGui::EndTabItem();
        }

        // CURRENT SWEEP TAB
        if (ImGui::BeginTabItem("🔋 Current Sweep")) {
          ImGui::Spacing();

          // Current sweep controls
          static float currentSweepStart = 0.0f;
          static float currentSweepStop = 0.001f;
          static int currentSweepSteps = 11;
          static float currentSweepCompliance = 10.0f;
          static float currentSweepDelay = 0.1f;

          if (ImGui::SliderFloat("Start (A)", &currentSweepStart, -1.0f, 1.0f, "%.6f")) {
            // Value updated by slider
          }
          ImGui::SameLine();
          ImGui::PushItemWidth(80);
          if (ImGui::InputFloat("##CurrentSweepStartInput", &currentSweepStart, 0.0f, 0.0f, "%.6f")) {
            currentSweepStart = (std::max)(-1.0f, (std::min)(1.0f, currentSweepStart));
          }
          ImGui::PopItemWidth();

          if (ImGui::SliderFloat("Stop (A)", &currentSweepStop, -1.0f, 1.0f, "%.6f")) {
            // Value updated by slider
          }
          ImGui::SameLine();
          ImGui::PushItemWidth(80);
          if (ImGui::InputFloat("##CurrentSweepStopInput", &currentSweepStop, 0.0f, 0.0f, "%.6f")) {
            currentSweepStop = (std::max)(-1.0f, (std::min)(1.0f, currentSweepStop));
          }
          ImGui::PopItemWidth();

          ImGui::SliderInt("Steps##CSteps", &currentSweepSteps, 2, 100);
          ImGui::SameLine();
          ImGui::PushItemWidth(80);
          ImGui::InputInt("##CurrentSweepStepsInput", &currentSweepSteps, 0, 0);
          currentSweepSteps = (std::max)(2, (std::min)(100, currentSweepSteps));
          ImGui::PopItemWidth();

          if (ImGui::SliderFloat("Voltage Compliance (V)", &currentSweepCompliance, 1.0f, 200.0f, "%.1f")) {
            // Value updated by slider
          }
          ImGui::SameLine();
          ImGui::PushItemWidth(80);
          if (ImGui::InputFloat("##CurrentSweepComplianceInput", &currentSweepCompliance, 0.0f, 0.0f, "%.1f")) {
            currentSweepCompliance = (std::max)(1.0f, (std::min)(200.0f, currentSweepCompliance));
          }
          ImGui::PopItemWidth();

          if (ImGui::SliderFloat("Delay (s)##CDelay", &currentSweepDelay, 0.01f, 1.0f, "%.3f")) {
            // Value updated by slider
          }
          ImGui::SameLine();
          ImGui::PushItemWidth(80);
          if (ImGui::InputFloat("##CurrentSweepDelayInput", &currentSweepDelay, 0.0f, 0.0f, "%.3f")) {
            currentSweepDelay = (std::max)(0.01f, (std::min)(1.0f, currentSweepDelay));
          }
          ImGui::PopItemWidth();

          ImGui::Spacing();

          if (ImGui::Button("🚀 Perform Current Sweep", ImVec2(-1, 40))) {
            Logger::GetInstance()->LogInfo("Starting current sweep from " +
              std::to_string(currentSweepStart) + "A to " +
              std::to_string(currentSweepStop) + "A");

            std::vector<CurrentSweepResult> currentSweepResults;
            if (CurrentSweep(currentSweepStart, currentSweepStop, currentSweepSteps,
              currentSweepCompliance, currentSweepDelay, currentSweepResults)) {

              Logger::GetInstance()->LogInfo("Current sweep completed with " + std::to_string(currentSweepResults.size()) + " points");

              // Print results to console
              std::cout << "\n" << std::string(80, '=') << std::endl;
              std::cout << "           CURRENT SWEEP RESULTS" << std::endl;
              std::cout << std::string(80, '=') << std::endl;
              std::cout << "Points: " << currentSweepResults.size() << std::endl;
              std::cout << "Range:  " << currentSweepStart << "A to " << currentSweepStop << "A" << std::endl;
              std::cout << std::string(80, '-') << std::endl;

              std::cout << std::left
                << std::setw(6) << "Step"
                << std::setw(15) << "Set I (A)"
                << std::setw(15) << "Measured V"
                << std::setw(15) << "Measured I"
                << std::setw(15) << "Resistance"
                << "Power (mW)" << std::endl;
              std::cout << std::string(80, '-') << std::endl;

              for (size_t i = 0; i < currentSweepResults.size(); ++i) {
                const auto& point = currentSweepResults[i];
                double powerMW = point.power * 1000.0;

                std::cout << std::left
                  << std::setw(6) << (i + 1)
                  << std::setw(15) << std::scientific << std::setprecision(3) << point.setCurrent
                  << std::setw(15) << std::fixed << std::setprecision(6) << point.measuredVoltage
                  << std::setw(15) << std::scientific << std::setprecision(3) << point.measuredCurrent
                  << std::setw(15) << std::scientific << std::setprecision(3) << point.resistance
                  << std::fixed << std::setprecision(6) << powerMW << std::endl;
              }

              std::cout << std::string(80, '-') << std::endl;

              // Calculate and display summary statistics
              double maxVoltage = 0.0, minVoltage = 0.0;
              double avgResistance = 0.0;
              int validPoints = 0;

              for (const auto& point : currentSweepResults) {
                maxVoltage = (std::max)(maxVoltage, std::abs(point.measuredVoltage));
                minVoltage = (std::min)(minVoltage, std::abs(point.measuredVoltage));

                if (point.resistance > 0 && point.resistance < 1e10) {
                  avgResistance += point.resistance;
                  validPoints++;
                }
              }

              if (validPoints > 0) {
                avgResistance /= validPoints;
              }

              std::cout << "SUMMARY:" << std::endl;
              std::cout << "  Current range: " << std::scientific << std::setprecision(3)
                << currentSweepStart << "A to " << currentSweepStop << "A" << std::endl;
              std::cout << "  Voltage range: " << std::fixed << std::setprecision(6)
                << minVoltage << "V to " << maxVoltage << "V" << std::endl;

              if (validPoints > 0) {
                std::cout << "  Average resistance: " << std::scientific << std::setprecision(3)
                  << avgResistance << " Ohms" << std::endl;
              }

              std::cout << std::string(80, '=') << "\n" << std::endl;
            }
          }

          ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
      }

      ImGui::Separator();

      // History plots
      ImGui::Text("Measurement History");

      std::vector<float> voltageData, currentData;
      {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        voltageData.reserve(m_readingHistory.size());
        currentData.reserve(m_readingHistory.size());

        for (const auto& reading : m_readingHistory) {
          voltageData.push_back(static_cast<float>(reading.voltage));
          currentData.push_back(static_cast<float>(reading.current * 1000.0)); // Convert to mA
        }
      }

      if (!voltageData.empty()) {
        float minV = *(std::min_element(voltageData.begin(), voltageData.end()));
        float maxV = *(std::max_element(voltageData.begin(), voltageData.end()));
        float margin = (std::max)(0.1f, (maxV - minV) * 0.1f);

        ImGui::PlotLines("Voltage (V)", voltageData.data(), static_cast<int>(voltageData.size()),
          0, nullptr, minV - margin, maxV + margin, ImVec2(850, 100));

        float minI = *(std::min_element(currentData.begin(), currentData.end()));
        float maxI = *(std::max_element(currentData.begin(), currentData.end()));
        float currentMargin = (std::max)(0.1f, (maxI - minI) * 0.1f);

        ImGui::PlotLines("Current (mA)", currentData.data(), static_cast<int>(currentData.size()),
          0, nullptr, minI - currentMargin, maxI + currentMargin, ImVec2(850, 100));
      }
      else {
        ImGui::Text("No measurement data available yet");
      }

      // Raw SCPI command interface
      ImGui::Separator();
      ImGui::Text("Raw SCPI Commands:");

      static char scpiCommand[256] = "";
      ImGui::InputText("Command", scpiCommand, sizeof(scpiCommand));

      if (ImGui::Button("Send Write Command")) {
        if (strlen(scpiCommand) > 0) {
          SendWriteCommand(scpiCommand);
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Send Query Command")) {
        if (strlen(scpiCommand) > 0) {
          std::string response;
          if (SendQueryCommand(scpiCommand, response)) {
            Logger::GetInstance()->LogInfo("Query response: " + response);
          }
        }
      }

      // Show last error if any
      if (!m_lastError.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Last Error: %s", m_lastError.c_str());
      }
    }
  }
  ImGui::End();
}


void Keithley2400Client::ToggleWindow() {
  m_showWindow = !m_showWindow;
}

bool Keithley2400Client::IsVisible() const {
  return m_showWindow;
}

const std::string& Keithley2400Client::GetName() const {
  return m_name;
}

void Keithley2400Client::SetName(const std::string& name) {
  m_name = name;
}

const std::string& Keithley2400Client::GetLastError() const {
  return m_lastError;
}