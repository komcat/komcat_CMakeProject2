#pragma once

#include "include/PowerSupplyDevice/IPowerSupplyManager.h"
#include "include/logger.h"
#include <string>
#include <vector>
#include <memory>

/**
 * @brief Power Supply Operations utility class
 *
 * Provides simplified interface to power supply operations using IPowerSupplyManager.
 * Handles common power supply tasks like reading measurements, enabling/disabling outputs,
 * and performing sweeps.
 */
class PowerSupplyOps {
public:
  /**
   * @brief Constructor - takes a power supply manager reference
   */
  PowerSupplyOps(IPowerSupplyManager* manager);  // Change to raw pointer

  ~PowerSupplyOps() = default;

  // === Status Checks ===
  bool IsAvailable() const;
  int GetConnectedDeviceCount() const;

  // === Reading Operations ===
  bool ReadVoltages(std::vector<float>& voltages);
  bool ReadCurrents(std::vector<float>& currents);
  bool ReadMeasurements(std::vector<IPowerSupplyDevice::Measurement>& measurements);

  // === Output Control ===
  bool SetOutputsEnabled(bool enable);
  bool SetDeviceOutputEnabled(const std::string& deviceId, bool enable);

  // === Mode Setting ===
  bool SetConstantVoltageMode(float voltage, float currentLimit);
  bool SetConstantCurrentMode(float current, float voltageLimit);

  // === Sweep Operations ===
  bool PerformVoltageSweep(const std::string& deviceId,
    float startV, float stopV, float stepSize,
    float currentLimit, int delayMs,
    IPowerSupplyDevice::SweepResult& result);

  bool PerformCurrentSweep(const std::string& deviceId,
    float startA, float stopA, float stepSize,
    float voltageLimit, int delayMs,
    IPowerSupplyDevice::SweepResult& result);

  // === Utility ===
  const std::string& GetLastError() const { return m_lastError; }
  void ClearError() { m_lastError.clear(); }

private:
  IPowerSupplyManager* m_manager;  // Change to raw pointer
  mutable std::string m_lastError;
  Logger* m_logger;

  void LogError(const std::string& message) const;
  void LogInfo(const std::string& message) const;
};