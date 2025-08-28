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
   * @param storeResults If true, store results in operation tracking system
   * @param label Optional label for identification
   */
  SPD_ReadCurrentVoltageOperation(bool storeResults = true, const std::string& label = "")
    : m_storeResults(storeResults), m_label(label) {
  }

  bool Execute(MachineOperations& ops) override;
  std::string GetDescription() const override;

  // Access to last readings (valid after successful Execute)
  const std::vector<double>& GetVoltages() const { return m_voltages; }
  const std::vector<double>& GetCurrents() const { return m_currents; }

private:
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
   * @param delayMs Optional delay after operation (ms)
   */
  SPD_EnablePowerOperation(bool enable, int delayMs = 500)
    : m_enable(enable), m_delayMs(delayMs) {
  }

  bool Execute(MachineOperations& ops) override;
  std::string GetDescription() const override;

private:
  bool m_enable;
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
   * @param label Optional label for identification
   */
  SPD_SetConstantVoltageOperation(double voltage, double currentLimit, const std::string& label = "")
    : m_voltage(voltage), m_currentLimit(currentLimit), m_label(label) {
  }

  bool Execute(MachineOperations& ops) override;
  std::string GetDescription() const override;

  // Getters for parameters
  double GetVoltage() const { return m_voltage; }
  double GetCurrentLimit() const { return m_currentLimit; }

private:
  double m_voltage;
  double m_currentLimit;
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
   * @param label Optional label for identification
   */
  SPD_SetConstantCurrentOperation(double current, double voltageLimit, const std::string& label = "")
    : m_current(current), m_voltageLimit(voltageLimit), m_label(label) {
  }

  bool Execute(MachineOperations& ops) override;
  std::string GetDescription() const override;

  // Getters for parameters
  double GetCurrent() const { return m_current; }
  double GetVoltageLimit() const { return m_voltageLimit; }

private:
  double m_current;
  double m_voltageLimit;
  std::string m_label;
};

// ============================================================================
// Usage Examples
// ============================================================================

/*
// Example 1: Enable power supplies
sequence->AddOperation(std::make_shared<SPD_EnablePowerOperation>(true, 1000)); // Enable with 1s delay

// Example 2: Set CV mode - 3.3V with 0.5A current limit
sequence->AddOperation(std::make_shared<SPD_SetConstantVoltageOperation>(3.3, 0.5, "3V3_Supply"));

// Example 3: Read measurements and store results
sequence->AddOperation(std::make_shared<SPD_ReadCurrentVoltageOperation>(true, "PowerCheck"));

// Example 4: Set CC mode - 100mA with 10V voltage limit
sequence->AddOperation(std::make_shared<SPD_SetConstantCurrentOperation>(0.1, 10.0, "100mA_Test"));

// Example 5: Disable power supplies
sequence->AddOperation(std::make_shared<SPD_EnablePowerOperation>(false, 0)); // Disable immediately

// Example 6: Complete power supply sequence
sequence->AddOperation(std::make_shared<SPD_SetConstantVoltageOperation>(5.0, 1.0, "5V_Rail"));
sequence->AddOperation(std::make_shared<SPD_EnablePowerOperation>(true, 500));
sequence->AddOperation(std::make_shared<SPD_ReadCurrentVoltageOperation>(true, "Initial_Check"));
// ... do other operations ...
sequence->AddOperation(std::make_shared<SPD_ReadCurrentVoltageOperation>(true, "Final_Check"));
sequence->AddOperation(std::make_shared<SPD_EnablePowerOperation>(false, 0));
*/