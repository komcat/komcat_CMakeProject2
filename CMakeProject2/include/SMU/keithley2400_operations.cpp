// keithley2400_operations.cpp
#include "include/SMU/keithley2400_operations.h"
#include "include/SMU/keithley2400_manager.h"
#include "include/SMU/keithley2400_client.h"
#include "include/logger.h"

Keithley2400Operations::Keithley2400Operations(Keithley2400Manager& manager)
  : m_manager(manager)
{
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("Keithley2400Operations: Initialized");
}

Keithley2400Operations::~Keithley2400Operations() {
  m_logger->LogInfo("Keithley2400Operations: Destroyed");
}

Keithley2400Client* Keithley2400Operations::GetClient(const std::string& clientName) {
  if (clientName.empty()) {
    // No specific client requested, get the first available one
    auto clientNames = m_manager.GetClientNames();
    if (clientNames.empty()) {
      m_logger->LogError("Keithley2400Operations: No clients available");
      return nullptr;
    }
    return m_manager.GetClient(clientNames[0]);
  }
  else {
    // Get the named client
    return m_manager.GetClient(clientName);
  }
}

bool Keithley2400Operations::ResetInstrument(const std::string& clientName) {
  m_logger->LogInfo("Keithley2400Operations: Resetting instrument" +
    (clientName.empty() ? "" : " for " + clientName));

  Keithley2400Client* client = GetClient(clientName);
  if (!client) {
    return false;
  }

  return client->ResetInstrument();
}

bool Keithley2400Operations::SetOutput(bool enable, const std::string& clientName) {
  m_logger->LogInfo("Keithley2400Operations: " + std::string(enable ? "Enabling" : "Disabling") +
    " output" + (clientName.empty() ? "" : " for " + clientName));

  Keithley2400Client* client = GetClient(clientName);
  if (!client) {
    return false;
  }

  return client->SetOutput(enable);
}

bool Keithley2400Operations::GetStatus(std::string& instrumentId, std::string& outputState,
  std::string& sourceFunction, const std::string& clientName) {
  m_logger->LogInfo("Keithley2400Operations: Getting status" +
    (clientName.empty() ? "" : " for " + clientName));

  Keithley2400Client* client = GetClient(clientName);
  if (!client) {
    return false;
  }

  return client->GetStatus(instrumentId, outputState, sourceFunction);
}

bool Keithley2400Operations::SetupVoltageSource(double voltage, double compliance,
  const std::string& range, const std::string& clientName) {
  m_logger->LogInfo("Keithley2400Operations: Setting up voltage source " +
    std::to_string(voltage) + "V, compliance " + std::to_string(compliance) + "A" +
    (clientName.empty() ? "" : " for " + clientName));

  Keithley2400Client* client = GetClient(clientName);
  if (!client) {
    return false;
  }

  return client->SetupVoltageSource(voltage, compliance, range);
}

bool Keithley2400Operations::SetupCurrentSource(double current, double compliance,
  const std::string& range, const std::string& clientName) {
  m_logger->LogInfo("Keithley2400Operations: Setting up current source " +
    std::to_string(current) + "A, compliance " + std::to_string(compliance) + "V" +
    (clientName.empty() ? "" : " for " + clientName));

  Keithley2400Client* client = GetClient(clientName);
  if (!client) {
    return false;
  }

  return client->SetupCurrentSource(current, compliance, range);
}

bool Keithley2400Operations::ReadVoltage(double& voltage, const std::string& clientName) {
  Keithley2400Client* client = GetClient(clientName);
  if (!client) {
    return false;
  }

  voltage = client->GetVoltage();
  return true;
}

bool Keithley2400Operations::ReadCurrent(double& current, const std::string& clientName) {
  Keithley2400Client* client = GetClient(clientName);
  if (!client) {
    return false;
  }

  current = client->GetCurrent();
  return true;
}

bool Keithley2400Operations::ReadResistance(double& resistance, const std::string& clientName) {
  Keithley2400Client* client = GetClient(clientName);
  if (!client) {
    return false;
  }

  resistance = client->GetResistance();
  return true;
}

bool Keithley2400Operations::ReadPower(double& power, const std::string& clientName) {
  Keithley2400Client* client = GetClient(clientName);
  if (!client) {
    return false;
  }

  power = client->GetPower();
  return true;
}

bool Keithley2400Operations::SendWriteCommand(const std::string& command, const std::string& clientName) {
  m_logger->LogInfo("Keithley2400Operations: Sending write command: " + command +
    (clientName.empty() ? "" : " for " + clientName));

  Keithley2400Client* client = GetClient(clientName);
  if (!client) {
    return false;
  }

  return client->SendWriteCommand(command);
}

bool Keithley2400Operations::SendQueryCommand(const std::string& command, std::string& response,
  const std::string& clientName) {
  m_logger->LogInfo("Keithley2400Operations: Sending query command: " + command +
    (clientName.empty() ? "" : " for " + clientName));

  Keithley2400Client* client = GetClient(clientName);
  if (!client) {
    return false;
  }

  return client->SendQueryCommand(command, response);
}

bool Keithley2400Operations::IsOutputEnabled(const std::string& clientName) {
  std::string instrumentId, outputState, sourceFunction;
  if (GetStatus(instrumentId, outputState, sourceFunction, clientName)) {
    return (outputState == "ON" || outputState == "1");
  }
  return false;
}

std::string Keithley2400Operations::GetLastError(const std::string& clientName) {
  Keithley2400Client* client = GetClient(clientName);
  if (!client) {
    return "No client available";
  }

  return client->GetLastError();
}