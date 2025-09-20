#pragma once

#include "SequenceStep.h"
#include "SPD_Ops.h"
#include <memory>
#include <vector>
/**
 * @brief Read current and voltage measurements from SPD power supplies
 */
class SPD_ReadCurrentVoltageOperation : public SequenceOperation {
public:
  /**
   * @brief Constructor
   * @param deviceName Name of SPD device to read from (empty = first available)
   * @param storeResults If true, store results in operation tracking system
   * @param label Optional label for identification
   */
  SPD_ReadCurrentVoltageOperation(const std::string& deviceName = "",
    bool storeResults = true,
    const std::string& label = "")
    : m_deviceName(deviceName), m_storeResults(storeResults), m_label(label) {
  }

  bool Execute(MachineOperations& ops) override;
  std::string GetDescription() const override;

  // Access to last readings (valid after successful Execute)
  const std::vector<double>& GetVoltages() const { return m_voltages; }
  const std::vector<double>& GetCurrents() const { return m_currents; }

private:
  std::string m_deviceName;  // NEW
  bool m_storeResults;
  std::string m_label;
  std::vector<double> m_voltages;
  std::vector<double> m_currents;
};

/**
 * @brief Enable or disable SPD power supply outputs
 */
class SPD_EnablePowerOperation : public SequenceOperation {
public:
  /**
   * @brief Constructor
   * @param enable true to enable outputs, false to disable
   * @param deviceName Name of SPD device (empty = all devices)
   * @param delayMs Optional delay after operation (ms)
   */
  SPD_EnablePowerOperation(bool enable, const std::string& deviceName = "", int delayMs = 500)
    : m_enable(enable), m_deviceName(deviceName), m_delayMs(delayMs) {
  }

  bool Execute(MachineOperations& ops) override;
  std::string GetDescription() const override;

private:
  bool m_enable;
  std::string m_deviceName;  // NEW
  int m_delayMs;
};

/**
 * @brief Set SPD power supplies to Constant Voltage (CV) mode
 */
class SPD_SetConstantVoltageOperation : public SequenceOperation {
public:
  /**
   * @brief Constructor
   * @param voltage Target voltage in volts
   * @param currentLimit Current compliance/limit in amps
   * @param deviceName Name of SPD device (empty = all devices)
   * @param label Optional label for identification
   */
  SPD_SetConstantVoltageOperation(double voltage, double currentLimit,
    const std::string& deviceName = "",
    const std::string& label = "")
    : m_voltage(voltage), m_currentLimit(currentLimit), m_deviceName(deviceName), m_label(label) {
  }

  bool Execute(MachineOperations& ops) override;
  std::string GetDescription() const override;

  // Getters for parameters
  double GetVoltage() const { return m_voltage; }
  double GetCurrentLimit() const { return m_currentLimit; }
  const std::string& GetDeviceName() const { return m_deviceName; }

private:
  double m_voltage;
  double m_currentLimit;
  std::string m_deviceName;  // NEW
  std::string m_label;
};

/**
 * @brief Set SPD power supplies to Constant Current (CC) mode
 */
class SPD_SetConstantCurrentOperation : public SequenceOperation {
public:
  /**
   * @brief Constructor
   * @param current Target current in amps
   * @param voltageLimit Voltage compliance/limit in volts
   * @param deviceName Name of SPD device (empty = all devices)
   * @param label Optional label for identification
   */
  SPD_SetConstantCurrentOperation(double current, double voltageLimit,
    const std::string& deviceName = "",
    const std::string& label = "")
    : m_current(current), m_voltageLimit(voltageLimit), m_deviceName(deviceName), m_label(label) {
  }

  bool Execute(MachineOperations& ops) override;
  std::string GetDescription() const override;

  // Getters for parameters
  double GetCurrent() const { return m_current; }
  double GetVoltageLimit() const { return m_voltageLimit; }
  const std::string& GetDeviceName() const { return m_deviceName; }

private:
  double m_current;
  double m_voltageLimit;
  std::string m_deviceName;  // NEW
  std::string m_label;
};

/**
 * @brief Perform voltage sweep on SPD power supply
 */
class SPD_VoltageSweepOperation : public SequenceOperation {
public:
  SPD_VoltageSweepOperation(double startV, double stopV, int steps,
    double currentLimit, const std::string& deviceName = "",
    double delayMs = 100.0, const std::string& label = "")
    : m_startV(startV), m_stopV(stopV), m_steps(steps),
    m_currentLimit(currentLimit), m_deviceName(deviceName),
    m_delayMs(delayMs), m_label(label) {
  }

  bool Execute(MachineOperations& ops) override;
  std::string GetDescription() const override;

private:
  double m_startV, m_stopV, m_currentLimit, m_delayMs;
  int m_steps;
  std::string m_deviceName;  // NEW
  std::string m_label;
};

/**
 * @brief Perform current sweep on SPD power supply
 */
class SPD_CurrentSweepOperation : public SequenceOperation {
public:
  SPD_CurrentSweepOperation(double startA, double stopA, int steps,
    double voltageLimit, const std::string& deviceName = "",
    double delayMs = 100.0, const std::string& label = "")
    : m_startA(startA), m_stopA(stopA), m_steps(steps),
    m_voltageLimit(voltageLimit), m_deviceName(deviceName),
    m_delayMs(delayMs), m_label(label) {
  }

  bool Execute(MachineOperations& ops) override;
  std::string GetDescription() const override;

private:
  double m_startA, m_stopA, m_voltageLimit, m_delayMs;
  int m_steps;
  std::string m_deviceName;  // NEW
  std::string m_label;
};