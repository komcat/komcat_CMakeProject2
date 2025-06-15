// keithley2400_operations.h
#pragma once

#include <string>

// Forward declarations
class Keithley2400Manager;
class Keithley2400Client;
class Logger;

class Keithley2400Operations {
public:
  Keithley2400Operations(Keithley2400Manager& manager);
  ~Keithley2400Operations();

  // Basic control methods
  bool ResetInstrument(const std::string& clientName = "");
  bool SetOutput(bool enable, const std::string& clientName = "");
  bool GetStatus(std::string& instrumentId, std::string& outputState,
    std::string& sourceFunction, const std::string& clientName = "");

  // Source configuration
  bool SetupVoltageSource(double voltage, double compliance = 0.1,
    const std::string& range = "AUTO", const std::string& clientName = "");
  bool SetupCurrentSource(double current, double compliance = 10.0,
    const std::string& range = "AUTO", const std::string& clientName = "");

  // Measurement methods
  bool ReadVoltage(double& voltage, const std::string& clientName = "");
  bool ReadCurrent(double& current, const std::string& clientName = "");
  bool ReadResistance(double& resistance, const std::string& clientName = "");
  bool ReadPower(double& power, const std::string& clientName = "");

  // Raw SCPI commands
  bool SendWriteCommand(const std::string& command, const std::string& clientName = "");
  bool SendQueryCommand(const std::string& command, std::string& response,
    const std::string& clientName = "");

  // Convenience methods
  bool IsOutputEnabled(const std::string& clientName = "");
  std::string GetLastError(const std::string& clientName = "");

private:
  Keithley2400Manager& m_manager;
  Logger* m_logger;

  // Helper method to get a client, either by name or default
  Keithley2400Client* GetClient(const std::string& clientName);
};