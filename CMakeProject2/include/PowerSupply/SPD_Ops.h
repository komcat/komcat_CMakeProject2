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