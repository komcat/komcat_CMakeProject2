#pragma once

#include "AppContext.h"
#include "include/PowerSupply/SPDPowerSupplyManager.h"
#include "include/logger.h"
#include <string>
#include <vector>

/**
 * @brief SPD Power Supply Operations utility class
 *
 * This class provides a simplified interface to SPD power supply operations
 * using AppContext.GetSPDPowerSupply(). It handles common power supply tasks
 * like reading measurements, enabling/disabling outputs, and setting modes.
 */
class SPD_Ops {
public:
  /**
   * @brief Constructor - gets SPD manager reference from AppContext
   */
  SPD_Ops();

  /**
   * @brief Destructor
   */
  ~SPD_Ops() = default;

  // === Status Checks ===

  /**
   * @brief Check if SPD manager is available and ready
   * @return true if SPD manager is available
   */
  bool IsAvailable() const;

  /**
   * @brief Get number of connected devices
   * @return Number of connected SPD devices
   */
  int GetConnectedDeviceCount() const;

  // === Reading Operations ===

  /**
   * @brief Read current voltage from all connected devices
   * @param voltages Vector to store voltage readings (cleared first)
   * @return true if readings successful
   */
  bool ReadVoltages(std::vector<double>& voltages);

  /**
   * @brief Read current from all connected devices
   * @param currents Vector to store current readings (cleared first)
   * @return true if readings successful
   */
  bool ReadCurrents(std::vector<double>& currents);

  /**
   * @brief Read both voltage and current from all devices
   * @param voltages Vector to store voltage readings
   * @param currents Vector to store current readings
   * @return true if readings successful
   */
  bool ReadCurrentAndVoltage(std::vector<double>& voltages, std::vector<double>& currents);

  // === Output Control ===

  /**
   * @brief Enable/disable all power supply outputs
   * @param enable true to enable outputs, false to disable
   * @return true if operation successful
   */
  bool SetOutputsEnabled(bool enable);

  /**
   * @brief Get output enable status from first available device
   * @return true if outputs are enabled (or unknown if no devices)
   */
  bool AreOutputsEnabled() const;

  // === Mode Setting Operations ===

  /**
   * @brief Set Constant Voltage (CV) mode on all devices
   * @param voltage Target voltage in volts
   * @param currentLimit Current compliance/limit in amps
   * @return true if operation successful on all connected devices
   */
  bool SetConstantVoltageMode(double voltage, double currentLimit);

  /**
   * @brief Set Constant Current (CC) mode on all devices
   * @param current Target current in amps
   * @param voltageLimit Voltage compliance/limit in volts
   * @return true if operation successful on all connected devices
   */
  bool SetConstantCurrentMode(double current, double voltageLimit);

  // === Utility Methods ===

  /**
   * @brief Get last error message from operations
   * @return Last error message string
   */
  const std::string& GetLastError() const { return m_lastError; }

  /**
   * @brief Clear last error message
   */
  void ClearError() { m_lastError.clear(); }



  // === Sweep Operations ===

/**
 * @brief Perform voltage sweep on first available device
 * @param startV Starting voltage in volts
 * @param stopV Ending voltage in volts
 * @param steps Number of steps in sweep
 * @param currentLimit Current compliance/limit in amps
 * @param delayMs Delay between steps in milliseconds
 * @param results Vector to store sweep results
 * @return true if sweep successful
 */
  bool PerformVoltageSweep(double startV, double stopV, int steps,
    double currentLimit, double delayMs,
    std::vector<SPDSweepResult>& results);

  /**
   * @brief Perform current sweep on first available device
   * @param startA Starting current in amps
   * @param stopA Ending current in amps
   * @param steps Number of steps in sweep
   * @param voltageLimit Voltage compliance/limit in volts
   * @param delayMs Delay between steps in milliseconds
   * @param results Vector to store sweep results
   * @return true if sweep successful
   */
  bool PerformCurrentSweep(double startA, double stopA, int steps,
    double voltageLimit, double delayMs,
    std::vector<SPDSweepResult>& results);


  // === Device-Specific Operations ===

/**
 * @brief Read voltage and current from a specific device
 * @param deviceName Name of the device to read from
 * @param voltages Vector to store voltage readings
 * @param currents Vector to store current readings
 * @return true if readings successful
 */
  bool ReadCurrentAndVoltageFromDevice(const std::string& deviceName,
    std::vector<double>& voltages,
    std::vector<double>& currents);

  /**
   * @brief Enable/disable output on a specific device
   * @param deviceName Name of the device
   * @param enable true to enable, false to disable
   * @return true if operation successful
   */
  bool SetDeviceOutputEnabled(const std::string& deviceName, bool enable);

  /**
   * @brief Set CV mode on a specific device
   * @param deviceName Name of the device
   * @param voltage Target voltage in volts
   * @param currentLimit Current compliance/limit in amps
   * @return true if operation successful
   */
  bool SetDeviceConstantVoltageMode(const std::string& deviceName,
    double voltage, double currentLimit);

  /**
   * @brief Set CC mode on a specific device
   * @param deviceName Name of the device
   * @param current Target current in amps
   * @param voltageLimit Voltage compliance/limit in volts
   * @return true if operation successful
   */
  bool SetDeviceConstantCurrentMode(const std::string& deviceName,
    double current, double voltageLimit);

  /**
   * @brief Perform voltage sweep on a specific device
   * @param deviceName Name of the device
   * @param startV Starting voltage
   * @param stopV Ending voltage
   * @param steps Number of steps
   * @param currentLimit Current limit
   * @param delayMs Delay between steps
   * @param results Vector to store results
   * @return true if successful
   */
  bool PerformDeviceVoltageSweep(const std::string& deviceName,
    double startV, double stopV, int steps,
    double currentLimit, double delayMs,
    std::vector<SPDSweepResult>& results);

  /**
   * @brief Perform current sweep on a specific device
   * @param deviceName Name of the device
   * @param startA Starting current
   * @param stopA Ending current
   * @param steps Number of steps
   * @param voltageLimit Voltage limit
   * @param delayMs Delay between steps
   * @param results Vector to store results
   * @return true if successful
   */
  bool PerformDeviceCurrentSweep(const std::string& deviceName,
    double startA, double stopA, int steps,
    double voltageLimit, double delayMs,
    std::vector<SPDSweepResult>& results);


private:
  SPDPowerSupplyManager* m_spdManager;
  mutable std::string m_lastError;
  Logger* m_logger;

  /**
   * @brief Log error message and store in m_lastError
   * @param message Error message to log and store
   */
  void LogError(const std::string& message) const;

  /**
   * @brief Log info message
   * @param message Info message to log
   */
  void LogInfo(const std::string& message) const;
};