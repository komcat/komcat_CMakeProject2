// siglent_power_supply.cpp
#include "siglent_power_supply.h"
#include <iostream>
#include <sstream>
#include <regex>

SiglentPowerSupply::SiglentPowerSupply(const std::string& resource_string)
  : m_resourceString(resource_string)
  , m_deviceInfoCached(false)
  , m_sweepRunning(false)
  , m_sweepStopRequested(false)
  , m_sweepProgress(0.0f) {
  m_spd = std::make_unique<PowerSupply::SPDPowerSupply>(resource_string);

  // Initialize default voltage/current settings
  m_lastVoltageSettings[1] = 5.0f;  // Default 5V
  m_lastCurrentSettings[1] = 1.0f;  // Default 1A
}

SiglentPowerSupply::~SiglentPowerSupply() {
  if (IsSweepRunning()) {
    StopSweep();
  }
  if (IsConnected()) {
    Disconnect();
  }
}

IPowerSupplyDevice::DeviceInfo SiglentPowerSupply::GetDeviceInfo() const {
  if (!m_deviceInfoCached && IsConnected()) {
    std::string idn = m_spd->getInstrumentID();
    if (!idn.empty()) {
      const_cast<SiglentPowerSupply*>(this)->ParseDeviceInfo(idn);
      m_deviceInfoCached = true;
    }
  }
  return m_deviceInfo;
}

void SiglentPowerSupply::ParseDeviceInfo(const std::string& idn) {
  // Example IDN: "Siglent Technologies,SPD1305X,SPD13DCQ7R0719,1.0.1.16R4"
  // Format: Manufacturer,Model,SerialNumber,FirmwareVersion

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
  m_deviceInfo.id = 0; // Could use a hash of serial number if needed
}

bool SiglentPowerSupply::Connect() {
  bool result = m_spd->connect(m_resourceString);
  if (result) {
    m_deviceInfoCached = false; // Refresh device info on new connection
  }
  return result;
}

bool SiglentPowerSupply::Disconnect() {
  if (IsSweepRunning()) {
    StopSweep();
  }
  m_spd->disconnect();
  m_deviceInfoCached = false;
  return true;
}

bool SiglentPowerSupply::IsConnected() const {
  return m_spd->isConnected();
}

bool SiglentPowerSupply::SetVoltage(float voltage, int channel) {
  if (!IsConnected()) return false;

  // Store the setting for mode switching
  m_lastVoltageSettings[channel] = voltage;

  return m_spd->setVoltage(channel, static_cast<double>(voltage));
}

bool SiglentPowerSupply::SetCurrent(float current, int channel) {
  if (!IsConnected()) return false;

  // Store the setting for mode switching
  m_lastCurrentSettings[channel] = current;

  return m_spd->setCurrent(channel, static_cast<double>(current));
}

bool SiglentPowerSupply::TurnOn(int channel) {
  if (!IsConnected()) return false;
  return m_spd->setOutput(channel, true);
}

bool SiglentPowerSupply::TurnOff(int channel) {
  if (!IsConnected()) return false;
  return m_spd->setOutput(channel, false);
}

bool SiglentPowerSupply::SetModeConstantVoltage(int channel) {
  if (!IsConnected()) return false;

  // Get current settings or use defaults
  float voltage = m_lastVoltageSettings.count(channel) ?
    m_lastVoltageSettings[channel] : 5.0f;
  float currentLimit = m_lastCurrentSettings.count(channel) ?
    m_lastCurrentSettings[channel] : 1.0f;

  return m_spd->setConstantVoltageMode(channel, voltage, currentLimit);
}

bool SiglentPowerSupply::SetModeConstantCurrent(int channel) {
  if (!IsConnected()) return false;

  // Get current settings or use defaults
  float current = m_lastCurrentSettings.count(channel) ?
    m_lastCurrentSettings[channel] : 1.0f;
  float voltageLimit = m_lastVoltageSettings.count(channel) ?
    m_lastVoltageSettings[channel] : 5.0f;

  return m_spd->setConstantCurrentMode(channel, current, voltageLimit);
}

float SiglentPowerSupply::ReadVoltage(int channel) const {
  if (!IsConnected()) return 0.0f;

  auto voltage = m_spd->getVoltage(channel);
  return voltage.has_value() ? static_cast<float>(voltage.value()) : 0.0f;
}

float SiglentPowerSupply::ReadCurrent(int channel) const {
  if (!IsConnected()) return 0.0f;

  auto current = m_spd->getCurrent(channel);
  return current.has_value() ? static_cast<float>(current.value()) : 0.0f;
}

IPowerSupplyDevice::Measurement SiglentPowerSupply::ReadVoltageCurrent(int channel) const {
  Measurement meas;
  meas.voltage = ReadVoltage(channel);
  meas.current = ReadCurrent(channel);
  return meas;
}

bool SiglentPowerSupply::StartSweep(const SweepConfig& config) {
  std::lock_guard<std::mutex> lock(m_sweepMutex);

  if (m_sweepRunning) {
    std::cerr << "Sweep already in progress" << std::endl;
    return false;
  }

  if (!IsConnected()) {
    std::cerr << "Device not connected" << std::endl;
    return false;
  }

  // ADD THIS BLOCK - Clean up any previous thread
  if (m_sweepThread && m_sweepThread->joinable()) {
    m_sweepThread->join();
  }

  m_sweepRunning = true;
  m_sweepStopRequested = false;
  m_sweepProgress = 0.0f;
  m_currentSweepConfig = config;
  m_currentSweepResult.config = config;
  m_currentSweepResult.completed = false;
  m_currentSweepResult.errorMessage.clear();
  m_currentSweepResult.measurements.clear();
  m_currentSweepResult.sweepValues.clear();

  // Start sweep in a separate thread
  m_sweepThread = std::make_unique<std::thread>(
    &SiglentPowerSupply::RunSweepThread, this, config);

  return true;
}

bool SiglentPowerSupply::StopSweep() {
  // Set stop flag under lock
  {
    std::lock_guard<std::mutex> lock(m_sweepMutex);

    if (!m_sweepRunning) {
      return true;
    }

    m_sweepStopRequested = true;
  }

  // Join thread WITHOUT lock to avoid deadlock
  if (m_sweepThread && m_sweepThread->joinable()) {
    m_sweepThread->join();
  }

  // Update state under lock
  {
    std::lock_guard<std::mutex> lock(m_sweepMutex);
    m_sweepRunning = false;
    m_currentSweepResult.completed = false;
    m_currentSweepResult.errorMessage = "Sweep stopped by user";
  }

  return true;
}


bool SiglentPowerSupply::IsSweepRunning() const {
  return m_sweepRunning.load();
}

float SiglentPowerSupply::GetSweepProgress() const {
  return m_sweepProgress.load();
}

IPowerSupplyDevice::SweepResult SiglentPowerSupply::GetSweepResults() const {
  std::lock_guard<std::mutex> lock(m_sweepMutex);
  return m_currentSweepResult;
}

IPowerSupplyDevice::SweepResult SiglentPowerSupply::ExecuteSweepBlocking(const SweepConfig& config) {
  SweepResult result;
  result.config = config;
  result.completed = false;

  if (!IsConnected()) {
    result.errorMessage = "Device not connected";
    return result;
  }

  // Calculate number of steps
  float range = std::abs(config.endValue - config.startValue);
  int steps = static_cast<int>(range / config.stepSize) + 1;

  // Prepare vectors
  std::vector<PowerSupply::SPDSweepResult> spdResults;

  bool success = false;

  if (config.mode == SweepConfig::Mode::CONSTANT_VOLTAGE) {
    // Use default current limit if not set
    float currentLimit = m_lastCurrentSettings.count(config.channel) ?
      m_lastCurrentSettings[config.channel] : 1.0f;

    success = m_spd->voltageSweep(
      config.channel,
      config.startValue,
      config.endValue,
      steps,
      currentLimit,
      config.delayMs,
      spdResults
    );
  }
  else { // CONSTANT_CURRENT mode
    // Use default voltage limit if not set
    float voltageLimit = m_lastVoltageSettings.count(config.channel) ?
      m_lastVoltageSettings[config.channel] : 5.0f;

    success = m_spd->currentSweep(
      config.channel,
      config.startValue,
      config.endValue,
      steps,
      voltageLimit,
      config.delayMs,
      spdResults
    );
  }

  if (success) {
    // Convert SPD results to interface format
    for (const auto& spdResult : spdResults) {
      Measurement meas;
      meas.voltage = static_cast<float>(spdResult.measuredVoltage);
      meas.current = static_cast<float>(spdResult.measuredCurrent);
      result.measurements.push_back(meas);
      result.sweepValues.push_back(static_cast<float>(spdResult.setValue));
    }
    result.completed = true;
  }
  else {
    result.errorMessage = "Sweep execution failed";
  }

  return result;
}

void SiglentPowerSupply::RunSweepThread(const SweepConfig& config) {
  // Calculate number of steps
  float range = std::abs(config.endValue - config.startValue);
  int totalSteps = static_cast<int>(range / config.stepSize) + 1;
  float stepValue = config.startValue;
  float stepIncrement = (config.endValue > config.startValue) ?
    config.stepSize : -config.stepSize;

  // Clear previous results
  {
    std::lock_guard<std::mutex> lock(m_sweepMutex);
    m_currentSweepResult.measurements.clear();
    m_currentSweepResult.sweepValues.clear();
  }

  // Perform sweep
  for (int step = 0; step < totalSteps && !m_sweepStopRequested; ++step) {
    // Set the value based on mode
    bool setSuccess = false;
    if (config.mode == SweepConfig::Mode::CONSTANT_VOLTAGE) {
      setSuccess = m_spd->setVoltage(config.channel, stepValue);
    }
    else {
      setSuccess = m_spd->setCurrent(config.channel, stepValue);
    }

    if (!setSuccess) {
      std::lock_guard<std::mutex> lock(m_sweepMutex);
      m_currentSweepResult.errorMessage = "Failed to set sweep value";
      break;
    }

    // Make sure output is on
    m_spd->setOutput(config.channel, true);

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

void SiglentPowerSupply::SetResourceString(const std::string& resource) {
  m_resourceString = resource;
  if (m_spd) {
    // If we need to reconnect with new resource string
    if (IsConnected()) {
      Disconnect();
      Connect();
    }
  }
}

std::string SiglentPowerSupply::GetResourceString() const {
  return m_resourceString;
}

void SiglentPowerSupply::SetDebugMode(bool enable) {
  if (m_spd) {
    m_spd->SetDebug(enable);
  }
}



bool SiglentPowerSupply::IsOutputOn(int channel) const {
  if (!IsConnected()) return false;

  auto state = m_spd->getOutputState(channel);
  return state.has_value() ? state.value() : false;
}