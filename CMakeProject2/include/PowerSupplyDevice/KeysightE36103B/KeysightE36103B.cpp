// KeysightE36103B.cpp
#include "KeysightE36103B.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <visa.h>

KeysightE36103B::KeysightE36103B(const std::string& resource_string)
  : m_resourceString(resource_string)
  , m_defaultRM(VI_NULL)
  , m_instr(VI_NULL)
  , m_connected(false)
  , m_debugMode(false)
  , m_deviceInfoCached(false)
  , m_sweepRunning(false)
  , m_sweepStopRequested(false)
  , m_sweepProgress(0.0f) {

  // Set default device info for E36103B
  m_deviceInfo.name = "Keysight E36103B";
  m_deviceInfo.model = "E36103B";
  m_deviceInfo.id = 0;
}

KeysightE36103B::~KeysightE36103B() {
  // First stop any running sweep
  if (IsSweepRunning()) {
    StopSweep();
  }

  // Make sure sweep thread is properly joined
  if (m_sweepThread && m_sweepThread->joinable()) {
    m_sweepStopRequested = true;  // Signal stop
    m_sweepThread->join();  // Wait for thread to finish
  }

  // Clean up the thread pointer
  m_sweepThread.reset();

  // Then disconnect
  if (IsConnected()) {
    Disconnect();
  }
}

IPowerSupplyDevice::DeviceInfo KeysightE36103B::GetDeviceInfo() const {
  if (!m_deviceInfoCached && IsConnected()) {
    std::string idn = SendQuery("*IDN?");
    if (!idn.empty()) {
      const_cast<KeysightE36103B*>(this)->ParseDeviceInfo(idn);
      m_deviceInfoCached = true;
    }
  }
  return m_deviceInfo;
}

void KeysightE36103B::ParseDeviceInfo(const std::string& idn) {
  // Example: "Keysight Technologies,E36103B,MY58200108,1.0.0"
  std::stringstream ss(idn);
  std::string manufacturer, model, serial, firmware;

  std::getline(ss, manufacturer, ',');
  std::getline(ss, model, ',');
  std::getline(ss, serial, ',');
  std::getline(ss, firmware, ',');

  m_deviceInfo.name = manufacturer + " " + model;
  m_deviceInfo.model = model;
  m_deviceInfo.serialNumber = serial;
  m_deviceInfo.firmwareVersion = firmware;
}

bool KeysightE36103B::Connect() {
  if (m_resourceString.empty()) {
    std::cerr << "Resource string not set" << std::endl;
    return false;
  }

  std::lock_guard<std::mutex> lock(m_visaMutex);

  if (m_connected) {
    return true;
  }

  // Open VISA resource manager
  ViStatus status = viOpenDefaultRM(&m_defaultRM);
  if (status < VI_SUCCESS) {
    std::cerr << "Failed to open VISA resource manager: 0x" << std::hex << status << std::endl;
    return false;
  }

  // Open instrument session
  status = viOpen(m_defaultRM, m_resourceString.c_str(), VI_NULL, VI_NULL, &m_instr);
  if (status < VI_SUCCESS) {
    std::cerr << "Failed to open instrument: " << m_resourceString << " (0x" << std::hex << status << ")" << std::endl;
    viClose(m_defaultRM);
    m_defaultRM = VI_NULL;
    return false;
  }

  // Configure timeout (5 seconds)
  viSetAttribute(m_instr, VI_ATTR_TMO_VALUE, VISA_TIMEOUT);

  // Set termination character for reading
  viSetAttribute(m_instr, VI_ATTR_TERMCHAR_EN, VI_TRUE);
  viSetAttribute(m_instr, VI_ATTR_TERMCHAR, '\n');

  m_connected = true;
  m_deviceInfoCached = false;

  // Initialize device - don't use SendCommand here to avoid locking
  std::string cmd = "*RST\n";
  ViUInt32 bytesWritten;
  viWrite(m_instr, reinterpret_cast<ViBuf>(const_cast<char*>(cmd.c_str())),
    static_cast<ViUInt32>(cmd.length()), &bytesWritten);

  cmd = "*CLS\n";
  viWrite(m_instr, reinterpret_cast<ViBuf>(const_cast<char*>(cmd.c_str())),
    static_cast<ViUInt32>(cmd.length()), &bytesWritten);

  cmd = "SYST:REM\n";
  viWrite(m_instr, reinterpret_cast<ViBuf>(const_cast<char*>(cmd.c_str())),
    static_cast<ViUInt32>(cmd.length()), &bytesWritten);

  if (m_debugMode) {
    // Use direct VISA call for IDN query to avoid deadlock
    cmd = "*IDN?\n";
    viWrite(m_instr, reinterpret_cast<ViBuf>(const_cast<char*>(cmd.c_str())),
      static_cast<ViUInt32>(cmd.length()), &bytesWritten);

    char buffer[256];
    ViUInt32 bytesRead;
    status = viRead(m_instr, reinterpret_cast<ViBuf>(buffer), sizeof(buffer) - 1, &bytesRead);
    if (status == VI_SUCCESS || status == VI_SUCCESS_TERM_CHAR) {
      buffer[bytesRead] = '\0';
      std::cout << "Connected to: " << buffer << std::endl;
    }
  }

  return true;
}

bool KeysightE36103B::Disconnect() {
  if (IsSweepRunning()) {
    StopSweep();
  }

  std::lock_guard<std::mutex> lock(m_visaMutex);

  if (m_connected) {
    // Directly send output off command instead of calling TurnOff() to avoid deadlock
    if (m_instr != VI_NULL) {
      std::string cmd = "OUTP OFF\n";
      ViUInt32 bytesWritten;
      viWrite(m_instr, reinterpret_cast<ViBuf>(const_cast<char*>(cmd.c_str())),
        static_cast<ViUInt32>(cmd.length()), &bytesWritten);
    }

    if (m_instr != VI_NULL) {
      viClose(m_instr);
      m_instr = VI_NULL;
    }

    if (m_defaultRM != VI_NULL) {
      viClose(m_defaultRM);
      m_defaultRM = VI_NULL;
    }

    m_connected = false;
    m_deviceInfoCached = false;
  }
  return true;
}

bool KeysightE36103B::IsConnected() const {
  return m_connected && (m_instr != VI_NULL);
}

bool KeysightE36103B::SetVoltage(float voltage, int channel) {
  if (!IsConnected()) return false;

  // E36103B is single channel, ignore channel parameter
  if (voltage < 0 || voltage > MAX_VOLTAGE) {
    std::cerr << "Voltage out of range (0-" << MAX_VOLTAGE << "V)" << std::endl;
    return false;
  }

  std::stringstream cmd;
  cmd << "VOLT " << std::fixed << std::setprecision(3) << voltage;
  return SendCommand(cmd.str());
}

bool KeysightE36103B::SetCurrent(float current, int channel) {
  if (!IsConnected()) return false;

  if (current < 0 || current > MAX_CURRENT) {
    std::cerr << "Current out of range (0-" << MAX_CURRENT << "A)" << std::endl;
    return false;
  }

  std::stringstream cmd;
  cmd << "CURR " << std::fixed << std::setprecision(3) << current;
  return SendCommand(cmd.str());
}

bool KeysightE36103B::TurnOn(int channel) {
  if (!IsConnected()) return false;
  return SendCommand("OUTP ON");
}

bool KeysightE36103B::TurnOff(int channel) {
  if (!IsConnected()) return false;
  return SendCommand("OUTP OFF");
}

bool KeysightE36103B::SetModeConstantVoltage(int channel) {
  // E36103B automatically switches between CV/CC based on load
  // Set current limit high to ensure CV mode
  if (!IsConnected()) return false;
  return SetCurrent(MAX_CURRENT, channel);
}

bool KeysightE36103B::SetModeConstantCurrent(int channel) {
  // E36103B automatically switches between CV/CC based on load
  // Set voltage limit high to ensure CC mode
  if (!IsConnected()) return false;
  return SetVoltage(MAX_VOLTAGE, channel);
}

float KeysightE36103B::ReadVoltage(int channel) const {
  if (!IsConnected()) return 0.0f;

  std::string response = SendQuery("MEAS:VOLT?");
  try {
    return std::stof(response);
  }
  catch (...) {
    if (m_debugMode) {
      std::cerr << "Failed to parse voltage: " << response << std::endl;
    }
    return 0.0f;
  }
}

float KeysightE36103B::ReadCurrent(int channel) const {
  if (!IsConnected()) return 0.0f;

  std::string response = SendQuery("MEAS:CURR?");
  try {
    return std::stof(response);
  }
  catch (...) {
    if (m_debugMode) {
      std::cerr << "Failed to parse current: " << response << std::endl;
    }
    return 0.0f;
  }
}

IPowerSupplyDevice::Measurement KeysightE36103B::ReadVoltageCurrent(int channel) const {
  Measurement meas;

  if (!IsConnected()) {
    meas.voltage = 0.0f;
    meas.current = 0.0f;
    return meas;
  }

  meas.voltage = ReadVoltage(channel);
  meas.current = ReadCurrent(channel);

  return meas;
}

bool KeysightE36103B::StartSweep(const SweepConfig& config) {
  std::lock_guard<std::mutex> lock(m_sweepMutex);

  if (m_sweepRunning) {
    std::cerr << "Sweep already in progress" << std::endl;
    return false;
  }

  if (!IsConnected()) {
    std::cerr << "Device not connected" << std::endl;
    return false;
  }

  // Clean up any previous thread
  if (m_sweepThread) {
    if (m_sweepThread->joinable()) {
      m_sweepThread->join();
    }
    m_sweepThread.reset();  // Explicitly reset the pointer
  }

  m_sweepRunning = true;
  m_sweepStopRequested = false;
  m_sweepProgress = 0.0f;
  m_currentSweepResult.config = config;
  m_currentSweepResult.completed = false;
  m_currentSweepResult.errorMessage.clear();
  m_currentSweepResult.measurements.clear();
  m_currentSweepResult.sweepValues.clear();

  // Start sweep in separate thread
  m_sweepThread = std::make_unique<std::thread>(
    &KeysightE36103B::RunSweepThread, this, config);

  return true;
}

bool KeysightE36103B::StopSweep() {
  // First check if sweep is running without lock
  if (!m_sweepRunning) {
    return true;
  }

  // Set stop flag
  m_sweepStopRequested = true;

  // Join thread if it exists (without holding mutex)
  if (m_sweepThread && m_sweepThread->joinable()) {
    m_sweepThread->join();
  }

  // Now update status with lock
  {
    std::lock_guard<std::mutex> lock(m_sweepMutex);
    m_sweepRunning = false;
    m_currentSweepResult.completed = false;
    m_currentSweepResult.errorMessage = "Sweep stopped by user";
  }

  // Clean up thread pointer
  m_sweepThread.reset();

  return true;
}

bool KeysightE36103B::IsSweepRunning() const {
  return m_sweepRunning.load();
}

float KeysightE36103B::GetSweepProgress() const {
  return m_sweepProgress.load();
}

IPowerSupplyDevice::SweepResult KeysightE36103B::GetSweepResults() const {
  std::lock_guard<std::mutex> lock(m_sweepMutex);
  return m_currentSweepResult;
}

IPowerSupplyDevice::SweepResult KeysightE36103B::ExecuteSweepBlocking(const SweepConfig& config) {
  if (!StartSweep(config)) {
    SweepResult result;
    result.completed = false;
    result.errorMessage = "Failed to start sweep";
    return result;
  }

  // Wait for completion
  while (m_sweepRunning) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  return GetSweepResults();
}

void KeysightE36103B::RunSweepThread(const SweepConfig& config) {
  // Calculate number of steps
  float range = std::abs(config.endValue - config.startValue);
  int totalSteps = static_cast<int>(range / config.stepSize) + 1;
  float stepValue = config.startValue;
  float stepIncrement = (config.endValue > config.startValue) ?
    config.stepSize : -config.stepSize;

  // Make sure output is on
  TurnOn();

  for (int step = 0; step < totalSteps && !m_sweepStopRequested; ++step) {
    // Set the value based on mode
    bool setSuccess = false;
    if (config.mode == SweepConfig::Mode::CONSTANT_VOLTAGE) {
      setSuccess = SetVoltage(stepValue, config.channel);
    }
    else {
      setSuccess = SetCurrent(stepValue, config.channel);
    }

    if (!setSuccess) {
      std::lock_guard<std::mutex> lock(m_sweepMutex);
      m_currentSweepResult.errorMessage = "Failed to set sweep value";
      break;
    }

    // Wait for settling
    if (config.delayMs > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(config.delayMs));
    }

    // Read measurements
    Measurement meas = ReadVoltageCurrent(config.channel);

    // Store results
    {
      std::lock_guard<std::mutex> lock(m_sweepMutex);
      m_currentSweepResult.measurements.push_back(meas);
      m_currentSweepResult.sweepValues.push_back(stepValue);
    }

    // Update progress
    m_sweepProgress = static_cast<float>(step + 1) / totalSteps;

    // Move to next step
    stepValue += stepIncrement;
  }

  // Mark completion
  {
    std::lock_guard<std::mutex> lock(m_sweepMutex);
    if (!m_sweepStopRequested && m_currentSweepResult.errorMessage.empty()) {
      m_currentSweepResult.completed = true;
    }
  }

  m_sweepRunning = false;
  m_sweepProgress = 1.0f;
}

void KeysightE36103B::SetResourceString(const std::string& resource) {
  m_resourceString = resource;
  if (IsConnected()) {
    Disconnect();
    Connect();
  }
}

std::string KeysightE36103B::GetResourceString() const {
  return m_resourceString;
}

bool KeysightE36103B::IsOutputOn() const {
  if (!IsConnected()) return false;

  std::string response = SendQuery("OUTP?");
  return (response == "1" || response == "ON");
}

bool KeysightE36103B::SetOVP(float voltage) {
  if (!IsConnected()) return false;

  if (voltage < OVP_MIN || voltage > OVP_MAX) {
    std::cerr << "OVP voltage out of range (" << OVP_MIN << "-" << OVP_MAX << "V)" << std::endl;
    return false;
  }

  std::stringstream cmd;
  cmd << "VOLT:PROT " << std::fixed << std::setprecision(3) << voltage;
  return SendCommand(cmd.str());
}

bool KeysightE36103B::SetOCP(float current) {
  if (!IsConnected()) return false;

  float maxOCP = MAX_CURRENT * 1.1f;  // Allow 10% over max
  if (current < 0 || current > maxOCP) {
    std::cerr << "OCP current out of range (0-" << maxOCP << "A)" << std::endl;
    return false;
  }

  std::stringstream cmd;
  cmd << "CURR:PROT " << std::fixed << std::setprecision(3) << current;
  return SendCommand(cmd.str());
}

bool KeysightE36103B::ClearProtection() {
  if (!IsConnected()) return false;
  return SendCommand("OUTP:PROT:CLE");
}

std::string KeysightE36103B::GetErrorString() const {
  if (!IsConnected()) return "Not connected";
  return SendQuery("SYST:ERR?");
}

void KeysightE36103B::SetRemoteSense(bool enable) {
  if (!IsConnected()) return;

  std::stringstream cmd;
  cmd << "VOLT:SENS " << (enable ? "EXT" : "INT");
  SendCommand(cmd.str());
}

std::string KeysightE36103B::SendQuery(const std::string& cmd) const {
  if (!m_connected || m_instr == VI_NULL) {
    return "";
  }

  std::lock_guard<std::mutex> lock(m_visaMutex);

  try {
    // Send query
    std::string command = cmd + "\n";
    ViUInt32 bytesWritten;

    ViStatus status = viWrite(m_instr,
      reinterpret_cast<ViBuf>(const_cast<char*>(command.c_str())),
      static_cast<ViUInt32>(command.length()),
      &bytesWritten);

    if (status != VI_SUCCESS) {
      if (m_debugMode) {
        std::cerr << "Failed to send query: 0x" << std::hex << status << std::endl;
      }
      return "";
    }

    // Read response
    char buffer[1024];
    ViUInt32 bytesRead;

    status = viRead(m_instr,
      reinterpret_cast<ViBuf>(buffer),
      sizeof(buffer) - 1,
      &bytesRead);

    if (status != VI_SUCCESS && status != VI_SUCCESS_TERM_CHAR) {
      if (m_debugMode) {
        std::cerr << "Failed to read response: 0x" << std::hex << status << std::endl;
      }
      return "";
    }

    buffer[bytesRead] = '\0';

    // Remove trailing whitespace
    std::string result(buffer);
    result.erase(result.find_last_not_of(" \n\r\t") + 1);

    return result;

  }
  catch (const std::exception& e) {
    if (m_debugMode) {
      std::cerr << "Exception during query: " << e.what() << std::endl;
    }
    return "";
  }
}

bool KeysightE36103B::SendCommand(const std::string& cmd) const {
  if (!m_connected || m_instr == VI_NULL) {
    return false;
  }

  std::lock_guard<std::mutex> lock(m_visaMutex);

  try {
    std::string command = cmd + "\n";
    ViUInt32 bytesWritten;

    ViStatus status = viWrite(m_instr,
      reinterpret_cast<ViBuf>(const_cast<char*>(command.c_str())),
      static_cast<ViUInt32>(command.length()),
      &bytesWritten);

    if (status != VI_SUCCESS) {
      if (m_debugMode) {
        std::cerr << "Failed to send command: 0x" << std::hex << status << std::endl;
      }
      return false;
    }

    // Don't call CheckError() here to avoid potential recursion
    // Just return success based on write operation
    return bytesWritten == command.length();

  }
  catch (const std::exception& e) {
    if (m_debugMode) {
      std::cerr << "Exception sending command: " << e.what() << std::endl;
    }
    return false;
  }
}

bool KeysightE36103B::CheckError() const {
  // This method should not lock mutex or call SendQuery to avoid recursion
  if (!m_connected || m_instr == VI_NULL) {
    return false;
  }

  // Direct VISA calls without mutex lock
  std::string cmd = "SYST:ERR?\n";
  ViUInt32 bytesWritten;

  ViStatus status = viWrite(m_instr,
    reinterpret_cast<ViBuf>(const_cast<char*>(cmd.c_str())),
    static_cast<ViUInt32>(cmd.length()),
    &bytesWritten);

  if (status != VI_SUCCESS) {
    return false;
  }

  char buffer[256];
  ViUInt32 bytesRead;

  status = viRead(m_instr,
    reinterpret_cast<ViBuf>(buffer),
    sizeof(buffer) - 1,
    &bytesRead);

  if (status != VI_SUCCESS && status != VI_SUCCESS_TERM_CHAR) {
    return false;
  }

  buffer[bytesRead] = '\0';
  std::string error(buffer);

  if (error.find("No error") == std::string::npos && error.find("+0") == std::string::npos) {
    if (m_debugMode) {
      std::cerr << "Device error: " << error << std::endl;
    }
    return true;  // There is an error
  }
  return false;  // No error
}