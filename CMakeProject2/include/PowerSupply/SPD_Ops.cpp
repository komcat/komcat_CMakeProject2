// SPD_Ops.cpp
#include "SPD_Ops.h"

SPD_Ops::SPD_Ops()
  : m_spdManager(nullptr), m_logger(nullptr) {

  // Get SPD manager from AppContext
  AppContext& context = AppContext::GetInstance();
  m_spdManager = context.GetSPDPowerSupply();
  m_logger = Logger::GetInstance();

  if (!m_spdManager) {
    LogError("SPD_Ops: SPDPowerSupplyManager not available from AppContext");
  }
  else {
    LogInfo("SPD_Ops: Successfully connected to SPDPowerSupplyManager");
  }
}

bool SPD_Ops::IsAvailable() const {
  if (!m_spdManager) {
    return false;
  }
  return m_spdManager->GetConnectedDeviceCount() > 0;
}

int SPD_Ops::GetConnectedDeviceCount() const {
  if (!m_spdManager) {
    return 0;
  }
  return m_spdManager->GetConnectedCount();
}

bool SPD_Ops::ReadVoltages(std::vector<double>& voltages) {
  voltages.clear();

  if (!m_spdManager) {
    LogError("ReadVoltages: SPD manager not available");
    return false;
  }

  try {
    // Get device names and read from each
    auto deviceNames = m_spdManager->GetDeviceNames();
    if (deviceNames.empty()) {
      LogError("ReadVoltages: No devices available");
      return false;
    }

    bool success = true;
    for (const auto& deviceName : deviceNames) {
      double voltage = 0.0;
      if (m_spdManager->ReadVoltage(deviceName, 1, voltage)) { // Channel 1
        voltages.push_back(voltage);
        LogInfo("ReadVoltages: " + deviceName + " = " + std::to_string(voltage) + "V");
      }
      else {
        LogError("ReadVoltages: Failed to read from " + deviceName);
        success = false;
      }
    }

    return success;
  }
  catch (const std::exception& e) {
    LogError("ReadVoltages: Exception - " + std::string(e.what()));
    return false;
  }
}

bool SPD_Ops::ReadCurrents(std::vector<double>& currents) {
  currents.clear();

  if (!m_spdManager) {
    LogError("ReadCurrents: SPD manager not available");
    return false;
  }

  try {
    auto deviceNames = m_spdManager->GetDeviceNames();
    if (deviceNames.empty()) {
      LogError("ReadCurrents: No devices available");
      return false;
    }

    bool success = true;
    for (const auto& deviceName : deviceNames) {
      double current = 0.0;
      if (m_spdManager->ReadCurrent(deviceName, 1, current)) { // Channel 1
        currents.push_back(current);
        LogInfo("ReadCurrents: " + deviceName + " = " + std::to_string(current) + "A");
      }
      else {
        LogError("ReadCurrents: Failed to read from " + deviceName);
        success = false;
      }
    }

    return success;
  }
  catch (const std::exception& e) {
    LogError("ReadCurrents: Exception - " + std::string(e.what()));
    return false;
  }
}

bool SPD_Ops::ReadCurrentAndVoltage(std::vector<double>& voltages, std::vector<double>& currents) {
  voltages.clear();
  currents.clear();

  if (!m_spdManager) {
    LogError("ReadCurrentAndVoltage: SPD manager not available");
    return false;
  }

  try {
    auto deviceNames = m_spdManager->GetDeviceNames();
    if (deviceNames.empty()) {
      LogError("ReadCurrentAndVoltage: No devices available");
      return false;
    }

    bool success = true;
    for (const auto& deviceName : deviceNames) {
      double voltage = 0.0, current = 0.0;

      bool voltageOk = m_spdManager->ReadVoltage(deviceName, 1, voltage);
      bool currentOk = m_spdManager->ReadCurrent(deviceName, 1, current);

      if (voltageOk && currentOk) {
        voltages.push_back(voltage);
        currents.push_back(current);
        LogInfo("ReadCurrentAndVoltage: " + deviceName + " = " +
          std::to_string(voltage) + "V, " + std::to_string(current) + "A");
      }
      else {
        LogError("ReadCurrentAndVoltage: Failed to read from " + deviceName);
        success = false;
      }
    }

    return success;
  }
  catch (const std::exception& e) {
    LogError("ReadCurrentAndVoltage: Exception - " + std::string(e.what()));
    return false;
  }
}

bool SPD_Ops::SetOutputsEnabled(bool enable) {
  if (!m_spdManager) {
    LogError("SetOutputsEnabled: SPD manager not available");
    return false;
  }

  try {
    bool result = m_spdManager->SetAllOutputs(enable);
    if (result) {
      LogInfo("SetOutputsEnabled: " + std::string(enable ? "Enabled" : "Disabled") + " all outputs");
    }
    else {
      LogError("SetOutputsEnabled: Failed to " + std::string(enable ? "enable" : "disable") + " outputs");
    }
    return result;
  }
  catch (const std::exception& e) {
    LogError("SetOutputsEnabled: Exception - " + std::string(e.what()));
    return false;
  }
}

bool SPD_Ops::AreOutputsEnabled() const {
  if (!m_spdManager) {
    return false;
  }

  try {
    auto deviceNames = m_spdManager->GetDeviceNames();
    if (deviceNames.empty()) {
      return false;
    }

    // Check first device as representative
    bool outputEnabled = false;
    bool result = m_spdManager->IsOutputEnabled(deviceNames[0], 1, outputEnabled);
    return result && outputEnabled;
  }
  catch (const std::exception& e) {
    LogError("AreOutputsEnabled: Exception - " + std::string(e.what()));
    return false;
  }
}

bool SPD_Ops::SetConstantVoltageMode(double voltage, double currentLimit) {
  if (!m_spdManager) {
    LogError("SetConstantVoltageMode: SPD manager not available");
    return false;
  }

  try {
    bool result = m_spdManager->SetConstantVoltageMode(voltage, currentLimit);
    if (result) {
      LogInfo("SetConstantVoltageMode: Set CV mode to " + std::to_string(voltage) +
        "V with " + std::to_string(currentLimit) + "A limit");
    }
    else {
      LogError("SetConstantVoltageMode: Failed to set CV mode");
    }
    return result;
  }
  catch (const std::exception& e) {
    LogError("SetConstantVoltageMode: Exception - " + std::string(e.what()));
    return false;
  }
}

bool SPD_Ops::SetConstantCurrentMode(double current, double voltageLimit) {
  if (!m_spdManager) {
    LogError("SetConstantCurrentMode: SPD manager not available");
    return false;
  }

  try {
    bool result = m_spdManager->SetConstantCurrentMode(current, voltageLimit);
    if (result) {
      LogInfo("SetConstantCurrentMode: Set CC mode to " + std::to_string(current) +
        "A with " + std::to_string(voltageLimit) + "V limit");
    }
    else {
      LogError("SetConstantCurrentMode: Failed to set CC mode");
    }
    return result;
  }
  catch (const std::exception& e) {
    LogError("SetConstantCurrentMode: Exception - " + std::string(e.what()));
    return false;
  }
}

void SPD_Ops::LogError(const std::string& message) const {
  m_lastError = message;
  if (m_logger) {
    m_logger->LogError(message);
  }
}

void SPD_Ops::LogInfo(const std::string& message) const {
  if (m_logger) {
    m_logger->LogInfo(message);
  }
}

bool SPD_Ops::PerformVoltageSweep(double startV, double stopV, int steps,
  double currentLimit, double delayMs,
  std::vector<SPDSweepResult>& results) {
  if (!m_spdManager) {
    LogError("PerformVoltageSweep: SPD manager not available");
    return false;
  }

  auto deviceNames = m_spdManager->GetDeviceNames();
  if (deviceNames.empty()) {
    LogError("PerformVoltageSweep: No devices available");
    return false;
  }

  // Use first device, channel 1
  bool success = m_spdManager->PerformVoltageSweep(deviceNames[0], 1,
    startV, stopV, steps, currentLimit, delayMs, results);

  if (success) {
    LogInfo("PerformVoltageSweep: Completed " + std::to_string(results.size()) + " points");
  }
  else {
    LogError("PerformVoltageSweep: Sweep failed on " + deviceNames[0]);
  }

  return success;
}

bool SPD_Ops::PerformCurrentSweep(double startA, double stopA, int steps,
  double voltageLimit, double delayMs,
  std::vector<SPDSweepResult>& results) {
  if (!m_spdManager) {
    LogError("PerformCurrentSweep: SPD manager not available");
    return false;
  }

  auto deviceNames = m_spdManager->GetDeviceNames();
  if (deviceNames.empty()) {
    LogError("PerformCurrentSweep: No devices available");
    return false;
  }

  // Use first device, channel 1
  bool success = m_spdManager->PerformCurrentSweep(deviceNames[0], 1,
    startA, stopA, steps, voltageLimit, delayMs, results);

  if (success) {
    LogInfo("PerformCurrentSweep: Completed " + std::to_string(results.size()) + " points");
  }
  else {
    LogError("PerformCurrentSweep: Sweep failed on " + deviceNames[0]);
  }

  return success;
}


bool SPD_Ops::ReadCurrentAndVoltageFromDevice(const std::string& deviceName,
  std::vector<double>& voltages,
  std::vector<double>& currents) {
  voltages.clear();
  currents.clear();

  if (!m_spdManager) {
    LogError("ReadCurrentAndVoltageFromDevice: SPD manager not available");
    return false;
  }

  if (deviceName.empty()) {
    LogError("ReadCurrentAndVoltageFromDevice: Device name is required");
    return false;
  }

  try {
    double voltage = 0.0, current = 0.0;

    bool voltageOk = m_spdManager->ReadVoltage(deviceName, 1, voltage);
    bool currentOk = m_spdManager->ReadCurrent(deviceName, 1, current);

    if (voltageOk && currentOk) {
      voltages.push_back(voltage);
      currents.push_back(current);
      LogInfo("ReadCurrentAndVoltageFromDevice: " + deviceName + " = " +
        std::to_string(voltage) + "V, " + std::to_string(current) + "A");
      return true;
    }
    else {
      LogError("ReadCurrentAndVoltageFromDevice: Failed to read from " + deviceName);
      return false;
    }
  }
  catch (const std::exception& e) {
    LogError("ReadCurrentAndVoltageFromDevice: Exception - " + std::string(e.what()));
    return false;
  }
}

bool SPD_Ops::SetDeviceOutputEnabled(const std::string& deviceName, bool enable) {
  if (!m_spdManager) {
    LogError("SetDeviceOutputEnabled: SPD manager not available");
    return false;
  }

  if (deviceName.empty()) {
    LogError("SetDeviceOutputEnabled: Device name is required");
    return false;
  }

  try {
    bool result = m_spdManager->SetOutput(deviceName, 1, enable);
    if (result) {
      LogInfo("SetDeviceOutputEnabled: " + std::string(enable ? "Enabled" : "Disabled") +
        " output on " + deviceName);
    }
    else {
      LogError("SetDeviceOutputEnabled: Failed to " +
        std::string(enable ? "enable" : "disable") + " output on " + deviceName);
    }
    return result;
  }
  catch (const std::exception& e) {
    LogError("SetDeviceOutputEnabled: Exception - " + std::string(e.what()));
    return false;
  }
}

bool SPD_Ops::SetDeviceConstantVoltageMode(const std::string& deviceName,
  double voltage, double currentLimit) {
  if (!m_spdManager) {
    LogError("SetDeviceConstantVoltageMode: SPD manager not available");
    return false;
  }

  if (deviceName.empty()) {
    LogError("SetDeviceConstantVoltageMode: Device name is required");
    return false;
  }

  try {
    bool result = m_spdManager->SetConstantVoltageMode(deviceName, voltage, currentLimit);
    if (result) {
      LogInfo("SetDeviceConstantVoltageMode: Set CV mode on " + deviceName + " to " +
        std::to_string(voltage) + "V with " + std::to_string(currentLimit) + "A limit");
    }
    else {
      LogError("SetDeviceConstantVoltageMode: Failed to set CV mode on " + deviceName);
    }
    return result;
  }
  catch (const std::exception& e) {
    LogError("SetDeviceConstantVoltageMode: Exception - " + std::string(e.what()));
    return false;
  }
}

bool SPD_Ops::SetDeviceConstantCurrentMode(const std::string& deviceName,
  double current, double voltageLimit) {
  if (!m_spdManager) {
    LogError("SetDeviceConstantCurrentMode: SPD manager not available");
    return false;
  }

  if (deviceName.empty()) {
    LogError("SetDeviceConstantCurrentMode: Device name is required");
    return false;
  }

  try {
    bool result = m_spdManager->SetConstantCurrentMode(deviceName, current, voltageLimit);
    if (result) {
      LogInfo("SetDeviceConstantCurrentMode: Set CC mode on " + deviceName + " to " +
        std::to_string(current) + "A with " + std::to_string(voltageLimit) + "V limit");
    }
    else {
      LogError("SetDeviceConstantCurrentMode: Failed to set CC mode on " + deviceName);
    }
    return result;
  }
  catch (const std::exception& e) {
    LogError("SetDeviceConstantCurrentMode: Exception - " + std::string(e.what()));
    return false;
  }
}

bool SPD_Ops::PerformDeviceVoltageSweep(const std::string& deviceName,
  double startV, double stopV, int steps,
  double currentLimit, double delayMs,
  std::vector<SPDSweepResult>& results) {
  if (!m_spdManager) {
    LogError("PerformDeviceVoltageSweep: SPD manager not available");
    return false;
  }

  if (deviceName.empty()) {
    LogError("PerformDeviceVoltageSweep: Device name is required");
    return false;
  }

  bool success = m_spdManager->PerformVoltageSweep(deviceName, 1,
    startV, stopV, steps, currentLimit, delayMs, results);

  if (success) {
    LogInfo("PerformDeviceVoltageSweep: Completed " + std::to_string(results.size()) +
      " points on " + deviceName);
  }
  else {
    LogError("PerformDeviceVoltageSweep: Sweep failed on " + deviceName);
  }

  return success;
}

bool SPD_Ops::PerformDeviceCurrentSweep(const std::string& deviceName,
  double startA, double stopA, int steps,
  double voltageLimit, double delayMs,
  std::vector<SPDSweepResult>& results) {
  if (!m_spdManager) {
    LogError("PerformDeviceCurrentSweep: SPD manager not available");
    return false;
  }

  if (deviceName.empty()) {
    LogError("PerformDeviceCurrentSweep: Device name is required");
    return false;
  }

  bool success = m_spdManager->PerformCurrentSweep(deviceName, 1,
    startA, stopA, steps, voltageLimit, delayMs, results);

  if (success) {
    LogInfo("PerformDeviceCurrentSweep: Completed " + std::to_string(results.size()) +
      " points on " + deviceName);
  }
  else {
    LogError("PerformDeviceCurrentSweep: Sweep failed on " + deviceName);
  }

  return success;
}