#include "PowerSupplyOps.h"

PowerSupplyOps::PowerSupplyOps(IPowerSupplyManager* manager)
  : m_manager(manager), m_logger(Logger::GetInstance()) {

  if (!m_manager) {
    LogError("PowerSupplyOps: Manager is null");
  }
  else {
    LogInfo("PowerSupplyOps: Initialized with " +
      std::to_string(m_manager->GetDeviceCount()) + " devices");
  }
}

bool PowerSupplyOps::IsAvailable() const {
  if (!m_manager) return false;
  return m_manager->GetDeviceCount() > 0;
}

int PowerSupplyOps::GetConnectedDeviceCount() const {
  if (!m_manager) return 0;

  int connected = 0;
  for (const auto& deviceId : m_manager->GetDeviceIds()) {
    if (m_manager->IsDeviceConnected(deviceId)) {
      connected++;
    }
  }
  return connected;
}

bool PowerSupplyOps::ReadVoltages(std::vector<float>& voltages) {
  voltages.clear();
  if (!m_manager) {
    LogError("ReadVoltages: Manager not available");
    return false;
  }

  try {
    auto deviceIds = m_manager->GetDeviceIds();
    bool success = true;

    for (const auto& deviceId : deviceIds) {
      float voltage = m_manager->ReadVoltage(deviceId);
      voltages.push_back(voltage);
      LogInfo("ReadVoltages: " + deviceId + " = " + std::to_string(voltage) + "V");
    }
    return success;
  }
  catch (const std::exception& e) {
    LogError("ReadVoltages: Exception - " + std::string(e.what()));
    return false;
  }
}

bool PowerSupplyOps::ReadCurrents(std::vector<float>& currents) {
  currents.clear();
  if (!m_manager) {
    LogError("ReadCurrents: Manager not available");
    return false;
  }

  try {
    auto deviceIds = m_manager->GetDeviceIds();
    bool success = true;

    for (const auto& deviceId : deviceIds) {
      float current = m_manager->ReadCurrent(deviceId);
      currents.push_back(current);
      LogInfo("ReadCurrents: " + deviceId + " = " + std::to_string(current) + "A");
    }
    return success;
  }
  catch (const std::exception& e) {
    LogError("ReadCurrents: Exception - " + std::string(e.what()));
    return false;
  }
}

bool PowerSupplyOps::ReadMeasurements(std::vector<IPowerSupplyDevice::Measurement>& measurements) {
  measurements.clear();
  if (!m_manager) {
    LogError("ReadMeasurements: Manager not available");
    return false;
  }

  try {
    auto allMeasurements = m_manager->ReadAllMeasurements();
    for (const auto& [deviceId, measurement] : allMeasurements) {
      measurements.push_back(measurement);
      LogInfo("ReadMeasurements: " + deviceId + " = " +
        std::to_string(measurement.voltage) + "V, " +
        std::to_string(measurement.current) + "A");
    }
    return true;
  }
  catch (const std::exception& e) {
    LogError("ReadMeasurements: Exception - " + std::string(e.what()));
    return false;
  }
}

bool PowerSupplyOps::SetOutputsEnabled(bool enable) {
  if (!m_manager) {
    LogError("SetOutputsEnabled: Manager not available");
    return false;
  }

  try {
    auto result = enable ? m_manager->TurnOnAll() : m_manager->TurnOffAll();
    if (result.successCount > 0) {
      LogInfo("SetOutputsEnabled: " + std::string(enable ? "Enabled" : "Disabled") +
        " " + std::to_string(result.successCount) + " outputs");
    }
    return result.failureCount == 0;
  }
  catch (const std::exception& e) {
    LogError("SetOutputsEnabled: Exception - " + std::string(e.what()));
    return false;
  }
}

bool PowerSupplyOps::SetDeviceOutputEnabled(const std::string& deviceId, bool enable) {
  if (!m_manager) {
    LogError("SetDeviceOutputEnabled: Manager not available");
    return false;
  }

  try {
    bool result = enable ? m_manager->TurnOn(deviceId) : m_manager->TurnOff(deviceId);
    if (result) {
      LogInfo("SetDeviceOutputEnabled: " + std::string(enable ? "Enabled" : "Disabled") +
        " output on " + deviceId);
    }
    return result;
  }
  catch (const std::exception& e) {
    LogError("SetDeviceOutputEnabled: Exception - " + std::string(e.what()));
    return false;
  }
}

bool PowerSupplyOps::SetConstantVoltageMode(float voltage, float currentLimit) {
  if (!m_manager) {
    LogError("SetConstantVoltageMode: Manager not available");
    return false;
  }

  try {
    auto result = m_manager->SetVoltageAll(voltage);
    if (result.failureCount > 0) {
      LogError("SetConstantVoltageMode: Failed on some devices");
      return false;
    }

    LogInfo("SetConstantVoltageMode: Set CV mode to " + std::to_string(voltage) +
      "V on " + std::to_string(result.successCount) + " devices");
    return true;
  }
  catch (const std::exception& e) {
    LogError("SetConstantVoltageMode: Exception - " + std::string(e.what()));
    return false;
  }
}

bool PowerSupplyOps::SetConstantCurrentMode(float current, float voltageLimit) {
  if (!m_manager) {
    LogError("SetConstantCurrentMode: Manager not available");
    return false;
  }

  try {
    auto result = m_manager->SetCurrentAll(current);
    if (result.failureCount > 0) {
      LogError("SetConstantCurrentMode: Failed on some devices");
      return false;
    }

    LogInfo("SetConstantCurrentMode: Set CC mode to " + std::to_string(current) +
      "A on " + std::to_string(result.successCount) + " devices");
    return true;
  }
  catch (const std::exception& e) {
    LogError("SetConstantCurrentMode: Exception - " + std::string(e.what()));
    return false;
  }
}

bool PowerSupplyOps::PerformVoltageSweep(const std::string& deviceId,
  float startV, float stopV, float stepSize,
  float currentLimit, int delayMs,
  IPowerSupplyDevice::SweepResult& result) {
  if (!m_manager) {
    LogError("PerformVoltageSweep: Manager not available");
    return false;
  }

  IPowerSupplyDevice::SweepConfig config;
  config.mode = IPowerSupplyDevice::SweepConfig::Mode::CONSTANT_VOLTAGE;
  config.startValue = startV;
  config.endValue = stopV;
  config.stepSize = stepSize;
  config.delayMs = delayMs;

  result = m_manager->ExecuteSweepBlocking(deviceId, config);

  if (result.completed) {
    LogInfo("PerformVoltageSweep: Completed " +
      std::to_string(result.measurements.size()) + " points on " + deviceId);
    return true;
  }
  else {
    LogError("PerformVoltageSweep: Failed - " + result.errorMessage);
    return false;
  }
}

bool PowerSupplyOps::PerformCurrentSweep(const std::string& deviceId,
  float startA, float stopA, float stepSize,
  float voltageLimit, int delayMs,
  IPowerSupplyDevice::SweepResult& result) {
  if (!m_manager) {
    LogError("PerformCurrentSweep: Manager not available");
    return false;
  }

  IPowerSupplyDevice::SweepConfig config;
  config.mode = IPowerSupplyDevice::SweepConfig::Mode::CONSTANT_CURRENT;
  config.startValue = startA;
  config.endValue = stopA;
  config.stepSize = stepSize;
  config.delayMs = delayMs;

  result = m_manager->ExecuteSweepBlocking(deviceId, config);

  if (result.completed) {
    LogInfo("PerformCurrentSweep: Completed " +
      std::to_string(result.measurements.size()) + " points on " + deviceId);
    return true;
  }
  else {
    LogError("PerformCurrentSweep: Failed - " + result.errorMessage);
    return false;
  }
}

void PowerSupplyOps::LogError(const std::string& message) const {
  m_lastError = message;
  if (m_logger) {
    m_logger->LogError(message);
  }
}

void PowerSupplyOps::LogInfo(const std::string& message) const {
  if (m_logger) {
    m_logger->LogInfo(message);
  }
}