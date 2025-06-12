#include "include/SMU/keithley2400_client.h"
#include "include/logger.h"
#include "imgui.h"
#include <sstream>
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
    if (send(m_socket, jsonStr.c_str(), jsonStr.length(), 0) == SOCKET_ERROR) {
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
    catch (const std::exception& e) {
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
    catch (const std::exception& e) {
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
    catch (const std::exception& e) {
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
    catch (const std::exception& e) {
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
    catch (const std::exception& e) {
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
    catch (const std::exception& e) {
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
  catch (const std::exception& e) {
    m_lastError = "Failed to parse measurement response";
    return false;
  }
}

bool Keithley2400Client::VoltageSweep(double start, double stop, int steps, double compliance, double delay,
  std::vector<VoltageSweepResult>& results) {
  json data;
  data["start"] = start;
  data["stop"] = stop;
  data["steps"] = steps;
  data["compliance"] = compliance;
  data["delay"] = delay;

  std::string response;
  bool result = SendJsonCommand("voltage_sweep", data.dump(), &response);

  if (result) {
    try {
      json jsonResponse = json::parse(response);
      if (jsonResponse["status"] == "success" && jsonResponse.contains("data")) {
        results.clear();
        auto sweepData = jsonResponse["data"];

        for (const auto& point : sweepData) {
          VoltageSweepResult sweepResult;
          sweepResult.setVoltage = point.value("set_voltage", 0.0);
          sweepResult.measuredVoltage = point.value("measured_voltage", 0.0);
          sweepResult.measuredCurrent = point.value("measured_current", 0.0);
          sweepResult.timestamp = std::chrono::steady_clock::now();
          results.push_back(sweepResult);
        }

        Logger::GetInstance()->LogInfo("Keithley2400Client: Voltage sweep completed with " +
          std::to_string(results.size()) + " points");
        return true;
      }
      else {
        m_lastError = jsonResponse.value("message", "Unknown error");
        return false;
      }
    }
    catch (const std::exception& e) {
      m_lastError = "Failed to parse voltage sweep response";
      return false;
    }
  }

  Logger::GetInstance()->LogError("Keithley2400Client: Failed to perform voltage sweep - " + m_lastError);
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

  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
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

      // Voltage sweep section
      ImGui::Text("Voltage Sweep:");

      // Start voltage with manual input
      static float sweepStartFloat = static_cast<float>(m_uiState.sweepStart);
      if (ImGui::SliderFloat("Start (V)", &sweepStartFloat, -20.0f, 20.0f, "%.2f")) {
        m_uiState.sweepStart = static_cast<double>(sweepStartFloat);
      }
      ImGui::SameLine();
      ImGui::PushItemWidth(80);
      if (ImGui::InputFloat("##SweepStartInput", &sweepStartFloat, 0.0f, 0.0f, "%.2f")) {
        sweepStartFloat = (std::max)(-20.0f, (std::min)(20.0f, sweepStartFloat));
        m_uiState.sweepStart = static_cast<double>(sweepStartFloat);
      }
      ImGui::PopItemWidth();

      // Stop voltage with manual input
      static float sweepStopFloat = static_cast<float>(m_uiState.sweepStop);
      if (ImGui::SliderFloat("Stop (V)", &sweepStopFloat, -20.0f, 20.0f, "%.2f")) {
        m_uiState.sweepStop = static_cast<double>(sweepStopFloat);
      }
      ImGui::SameLine();
      ImGui::PushItemWidth(80);
      if (ImGui::InputFloat("##SweepStopInput", &sweepStopFloat, 0.0f, 0.0f, "%.2f")) {
        sweepStopFloat = (std::max)(-20.0f, (std::min)(20.0f, sweepStopFloat));
        m_uiState.sweepStop = static_cast<double>(sweepStopFloat);
      }
      ImGui::PopItemWidth();

      // Steps with manual input
      ImGui::SliderInt("Steps", &m_uiState.sweepSteps, 2, 100);
      ImGui::SameLine();
      ImGui::PushItemWidth(80);
      ImGui::InputInt("##SweepStepsInput", &m_uiState.sweepSteps, 0, 0);
      m_uiState.sweepSteps = (std::max)(2, (std::min)(100, m_uiState.sweepSteps)); // Clamp
      ImGui::PopItemWidth();

      // Compliance with manual input
      static float sweepComplianceFloat = static_cast<float>(m_uiState.sweepCompliance);
      if (ImGui::SliderFloat("Compliance (A)", &sweepComplianceFloat, 0.001f, 1.0f, "%.3f")) {
        m_uiState.sweepCompliance = static_cast<double>(sweepComplianceFloat);
      }
      ImGui::SameLine();
      ImGui::PushItemWidth(80);
      if (ImGui::InputFloat("##SweepComplianceInput", &sweepComplianceFloat, 0.0f, 0.0f, "%.3f")) {
        sweepComplianceFloat = (std::max)(0.001f, (std::min)(1.0f, sweepComplianceFloat));
        m_uiState.sweepCompliance = static_cast<double>(sweepComplianceFloat);
      }
      ImGui::PopItemWidth();

      // Delay with manual input
      static float sweepDelayFloat = static_cast<float>(m_uiState.sweepDelay);
      if (ImGui::SliderFloat("Delay (s)", &sweepDelayFloat, 0.01f, 1.0f, "%.3f")) {
        m_uiState.sweepDelay = static_cast<double>(sweepDelayFloat);
      }
      ImGui::SameLine();
      ImGui::PushItemWidth(80);
      if (ImGui::InputFloat("##SweepDelayInput", &sweepDelayFloat, 0.0f, 0.0f, "%.3f")) {
        sweepDelayFloat = (std::max)(0.01f, (std::min)(1.0f, sweepDelayFloat));
        m_uiState.sweepDelay = static_cast<double>(sweepDelayFloat);
      }
      ImGui::PopItemWidth();

      // Complete corrected section:
      if (ImGui::Button("Perform Voltage Sweep")) {
        std::vector<VoltageSweepResult> sweepResults;
        if (VoltageSweep(m_uiState.sweepStart, m_uiState.sweepStop, m_uiState.sweepSteps,
          m_uiState.sweepCompliance, m_uiState.sweepDelay, sweepResults)) {
          Logger::GetInstance()->LogInfo("Voltage sweep completed with " + std::to_string(sweepResults.size()) + " points");
          // Could store results for plotting
        }
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
        float minV = *(std::min_element)(voltageData.begin(), voltageData.end());
        float maxV = *(std::max_element)(voltageData.begin(), voltageData.end());
        float margin = (std::max)(0.1f, (maxV - minV) * 0.1f);

        ImGui::PlotLines("Voltage (V)", voltageData.data(), static_cast<int>(voltageData.size()),
          0, nullptr, minV - margin, maxV + margin, ImVec2(760, 100));

        float minI = *(std::min_element)(currentData.begin(), currentData.end());
        float maxI = *(std::max_element)(currentData.begin(), currentData.end());
        float currentMargin = (std::max)(0.1f, (maxI - minI) * 0.1f);

        ImGui::PlotLines("Current (mA)", currentData.data(), static_cast<int>(currentData.size()),
          0, nullptr, minI - currentMargin, maxI + currentMargin, ImVec2(760, 100));
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