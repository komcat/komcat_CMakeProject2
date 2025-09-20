// keithley2400_operations.cpp
#include "include/SMU/keithley2400_operations.h"
#include "include/SMU/keithley2400_manager.h"
#include "include/SMU/keithley2400_client.h"
#include "include/logger.h"
#include <thread>  // Add this include at the top if not already there
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


bool Keithley2400Operations::VoltageSweep(double startVoltage, double stopVoltage, int steps,
  double currentCompliance, double delayMs, const std::string& clientName) {

  m_logger->LogInfo("Keithley2400Operations: Starting simple voltage sweep from " +
    std::to_string(startVoltage) + "V to " + std::to_string(stopVoltage) + "V, " +
    std::to_string(steps) + " steps");

  // Get client
  Keithley2400Client* client = GetClient(clientName);
  if (!client) {
    m_logger->LogError("Keithley2400Operations: No client available for voltage sweep");
    return false;
  }

  // Validate parameters
  if (steps <= 1) {
    m_logger->LogError("Keithley2400Operations: Invalid step count: " + std::to_string(steps));
    return false;
  }

  try {
    // Calculate voltage step size
    double voltageStep = (stopVoltage - startVoltage) / (steps - 1);

    m_logger->LogInfo("Keithley2400Operations: Voltage step size: " + std::to_string(voltageStep) + "V");

    // Perform sweep
    for (int i = 0; i < steps; ++i) {
      double voltage = startVoltage + (i * voltageStep);

      // Setup voltage source for this step
      if (!SetupVoltageSource(voltage, currentCompliance, "AUTO", clientName)) {
        m_logger->LogError("Keithley2400Operations: Failed to setup voltage at step " + std::to_string(i));
        return false;
      }

      // Enable output
      if (!SetOutput(true, clientName)) {
        m_logger->LogError("Keithley2400Operations: Failed to enable output at step " + std::to_string(i));
        return false;
      }

      // Wait for settling
      if (delayMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(delayMs)));
      }

      // Read and log measurements
      double current, resistance;
      if (ReadCurrent(current, clientName) && ReadResistance(resistance, clientName)) {
        m_logger->LogInfo("Keithley2400Operations: Step " + std::to_string(i + 1) + "/" +
          std::to_string(steps) + " - V=" + std::to_string(voltage) + "V, I=" +
          std::to_string(current) + "A, R=" + std::to_string(resistance) + "Ohm");
      }
    }

    // Disable output for safety
    SetOutput(false, clientName);
    m_logger->LogInfo("Keithley2400Operations: Voltage sweep completed successfully");
    return true;

  }
  catch (const std::exception& e) {
    m_logger->LogError("Keithley2400Operations: Exception during voltage sweep: " + std::string(e.what()));
    SetOutput(false, clientName); // Safety disable
    return false;
  }
}

bool Keithley2400Operations::CurrentSweep(double startCurrent, double stopCurrent, int steps,
  double voltageCompliance, double delayMs, const std::string& clientName) {

  m_logger->LogInfo("Keithley2400Operations: Starting simple current sweep from " +
    std::to_string(startCurrent) + "A to " + std::to_string(stopCurrent) + "A, " +
    std::to_string(steps) + " steps");

  // Get client
  Keithley2400Client* client = GetClient(clientName);
  if (!client) {
    m_logger->LogError("Keithley2400Operations: No client available for current sweep");
    return false;
  }

  // Validate parameters
  if (steps <= 1) {
    m_logger->LogError("Keithley2400Operations: Invalid step count: " + std::to_string(steps));
    return false;
  }

  try {
    // Calculate current step size
    double currentStep = (stopCurrent - startCurrent) / (steps - 1);

    m_logger->LogInfo("Keithley2400Operations: Current step size: " + std::to_string(currentStep) + "A");

    // Perform sweep
    for (int i = 0; i < steps; ++i) {
      double current = startCurrent + (i * currentStep);

      // Setup current source for this step
      if (!SetupCurrentSource(current, voltageCompliance, "AUTO", clientName)) {
        m_logger->LogError("Keithley2400Operations: Failed to setup current at step " + std::to_string(i));
        return false;
      }

      // Enable output
      if (!SetOutput(true, clientName)) {
        m_logger->LogError("Keithley2400Operations: Failed to enable output at step " + std::to_string(i));
        return false;
      }

      // Wait for settling
      if (delayMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(delayMs)));
      }

      // Read and log measurements
      double voltage, resistance;
      if (ReadVoltage(voltage, clientName) && ReadResistance(resistance, clientName)) {
        m_logger->LogInfo("Keithley2400Operations: Step " + std::to_string(i + 1) + "/" +
          std::to_string(steps) + " - I=" + std::to_string(current) + "A, V=" +
          std::to_string(voltage) + "V, R=" + std::to_string(resistance) + "Ohm");
      }
    }

    // Disable output for safety
    SetOutput(false, clientName);
    m_logger->LogInfo("Keithley2400Operations: Current sweep completed successfully");
    return true;

  }
  catch (const std::exception& e) {
    m_logger->LogError("Keithley2400Operations: Exception during current sweep: " + std::string(e.what()));
    SetOutput(false, clientName); // Safety disable
    return false;
  }
}