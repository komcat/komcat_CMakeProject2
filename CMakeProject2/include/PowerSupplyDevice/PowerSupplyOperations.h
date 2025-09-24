#pragma once

#include "SequenceStep.h"
#include "PowerSupplyOps.h"
#include <memory>
#include <vector>

/**
 * @brief Read voltage and current from power supplies
 */
class PS_ReadMeasurementOperation : public SequenceOperation {
public:
  PS_ReadMeasurementOperation(const std::string& deviceId = "",
    bool storeResults = true,
    const std::string& label = "")
    : m_deviceId(deviceId), m_storeResults(storeResults), m_label(label) {
  }

  bool Execute(MachineOperations& ops) override;
  std::string GetDescription() const override;

  const std::vector<float>& GetVoltages() const { return m_voltages; }
  const std::vector<float>& GetCurrents() const { return m_currents; }

private:
  std::string m_deviceId;
  bool m_storeResults;
  std::string m_label;
  std::vector<float> m_voltages;
  std::vector<float> m_currents;
};

/**
 * @brief Enable/disable power supply outputs
 */
class PS_EnableOutputOperation : public SequenceOperation {
public:
  PS_EnableOutputOperation(bool enable, const std::string& deviceId = "", int delayMs = 500)
    : m_enable(enable), m_deviceId(deviceId), m_delayMs(delayMs) {
  }

  bool Execute(MachineOperations& ops) override;
  std::string GetDescription() const override;

private:
  bool m_enable;
  std::string m_deviceId;
  int m_delayMs;
};

/**
 * @brief Set power supply to constant voltage mode
 */
class PS_SetVoltageOperation : public SequenceOperation {
public:
  PS_SetVoltageOperation(float voltage, float currentLimit = 1.0f,
    const std::string& deviceId = "",
    const std::string& label = "")
    : m_voltage(voltage), m_currentLimit(currentLimit),
    m_deviceId(deviceId), m_label(label) {
  }

  bool Execute(MachineOperations& ops) override;
  std::string GetDescription() const override;

private:
  float m_voltage;
  float m_currentLimit;
  std::string m_deviceId;
  std::string m_label;
};

/**
 * @brief Set power supply to constant current mode
 */
class PS_SetCurrentOperation : public SequenceOperation {
public:
  PS_SetCurrentOperation(float current, float voltageLimit = 5.0f,
    const std::string& deviceId = "",
    const std::string& label = "")
    : m_current(current), m_voltageLimit(voltageLimit),
    m_deviceId(deviceId), m_label(label) {
  }

  bool Execute(MachineOperations& ops) override;
  std::string GetDescription() const override;

private:
  float m_current;
  float m_voltageLimit;
  std::string m_deviceId;
  std::string m_label;
};

/**
 * @brief Perform voltage sweep
 */
class PS_VoltageSweepOperation : public SequenceOperation {
public:
  PS_VoltageSweepOperation(float startV, float stopV, float stepSize,
    float currentLimit, const std::string& deviceId,
    int delayMs = 100, const std::string& label = "")
    : m_startV(startV), m_stopV(stopV), m_stepSize(stepSize),
    m_currentLimit(currentLimit), m_deviceId(deviceId),
    m_delayMs(delayMs), m_label(label) {
  }

  bool Execute(MachineOperations& ops) override;
  std::string GetDescription() const override;

private:
  float m_startV, m_stopV, m_stepSize, m_currentLimit;
  std::string m_deviceId;
  int m_delayMs;
  std::string m_label;
};