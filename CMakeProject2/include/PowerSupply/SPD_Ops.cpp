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