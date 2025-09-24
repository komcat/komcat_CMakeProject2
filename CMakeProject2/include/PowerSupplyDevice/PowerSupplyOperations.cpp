#include "PowerSupplyOperations.h"
#include "PowerSupplyDevice/PowerSupplyManager.h"
#include "include/machine_operations.h"

#include <chrono>

// ============================================================================
// PS_ReadMeasurementOperation Implementation
// ============================================================================

bool PS_ReadMeasurementOperation::Execute(MachineOperations& ops) {
  std::string callerContext = "PS_ReadMeasurementOperation";
  if (!m_deviceId.empty()) {
    callerContext += "_" + m_deviceId;
  }
  if (!m_label.empty()) {
    callerContext += "_" + m_label;
  }

  // Start operation tracking
  std::string opId;
  std::shared_ptr<OperationResultsManager> resultsManager = ops.GetResultsManager();
  if (resultsManager && m_storeResults) {
    std::map<std::string, std::string> parameters;
    parameters["device_id"] = m_deviceId;
    parameters["label"] = m_label;
    parameters["store_results"] = m_storeResults ? "true" : "false";

    opId = resultsManager->StartOperation("PS_ReadMeasurementOperation", m_deviceId,
      callerContext, "", parameters);
  }

  ops.LogInfo("Starting power supply measurement reading" +
    (m_deviceId.empty() ? "" : " from " + m_deviceId) +
    (m_label.empty() ? "" : " (" + m_label + ")"));

  // Get power supply manager from ops (not AppContext)
  auto psManager = ops.GetPowerSupplyManager();

  if (!psManager) {
    ops.LogError("PS_ReadMeasurementOperation: Power supply manager not available");
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", "Manager not available");
    }
    return false;
  }

  PowerSupplyOps psOps(psManager);

  if (!psOps.IsAvailable()) {
    ops.LogError("PS_ReadMeasurementOperation: No power supplies available");
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", "No devices available");
    }
    return false;
  }

  // Read measurements
  bool success;
  if (!m_deviceId.empty()) {
    // Read specific device
    float voltage = psManager->ReadVoltage(m_deviceId);
    float current = psManager->ReadCurrent(m_deviceId);
    m_voltages.push_back(voltage);
    m_currents.push_back(current);
    success = true;
  }
  else {
    // Read all devices
    std::vector<IPowerSupplyDevice::Measurement> measurements;
    success = psOps.ReadMeasurements(measurements);
    for (const auto& meas : measurements) {
      m_voltages.push_back(meas.voltage);
      m_currents.push_back(meas.current);
    }
  }

  if (success) {
    ops.LogInfo("PS_ReadMeasurementOperation: Successfully read " +
      std::to_string(m_voltages.size()) + " measurements");

    if (!m_voltages.empty() && !m_currents.empty()) {
      std::string devicePrefix = m_deviceId.empty() ? "PS1" : m_deviceId;
      ops.SetDataValue(devicePrefix + "-Voltage", m_voltages[0], "PS_ReadMeasurementOperation");
      ops.SetDataValue(devicePrefix + "-Current", m_currents[0], "PS_ReadMeasurementOperation");

      ops.LogInfo("Stored in GlobalDataStore: " + devicePrefix + "-Voltage=" +
        std::to_string(m_voltages[0]) + "V, " + devicePrefix + "-Current=" +
        std::to_string(m_currents[0]) + "A");
    }

    if (resultsManager && m_storeResults && !opId.empty()) {
      std::map<std::string, std::string> results;
      for (size_t i = 0; i < m_voltages.size() && i < m_currents.size(); ++i) {
        results["voltage_" + std::to_string(i)] = std::to_string(m_voltages[i]);
        results["current_" + std::to_string(i)] = std::to_string(m_currents[i]);
      }
      results["device_count"] = std::to_string(m_voltages.size());
      resultsManager->EndOperation(opId, "completed", "Measurements successful", results);
    }
  }
  else {
    ops.LogError("PS_ReadMeasurementOperation failed: " + psOps.GetLastError());
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", psOps.GetLastError());
    }
  }

  return success;
}

std::string PS_ReadMeasurementOperation::GetDescription() const {
  std::string desc = "Read power supply measurements";
  if (!m_deviceId.empty()) {
    desc += " from " + m_deviceId;
  }
  if (!m_label.empty()) {
    desc += " (" + m_label + ")";
  }
  if (m_storeResults) {
    desc += " [stored]";
  }
  return desc;
}

// ============================================================================
// PS_EnableOutputOperation Implementation
// ============================================================================

bool PS_EnableOutputOperation::Execute(MachineOperations& ops) {
  std::string callerContext = "PS_EnableOutputOperation_" +
    std::string(m_enable ? "ON" : "OFF");
  if (!m_deviceId.empty()) {
    callerContext += "_" + m_deviceId;
  }

  std::string opId;
  std::shared_ptr<OperationResultsManager> resultsManager = ops.GetResultsManager();
  if (resultsManager) {
    std::map<std::string, std::string> parameters;
    parameters["enable"] = m_enable ? "true" : "false";
    parameters["device_id"] = m_deviceId;
    parameters["delay_ms"] = std::to_string(m_delayMs);

    opId = resultsManager->StartOperation("PS_EnableOutputOperation", m_deviceId,
      callerContext, "", parameters);
  }

  ops.LogInfo("Power supply operation: " + std::string(m_enable ? "Enabling" : "Disabling") +
    " outputs" + (m_deviceId.empty() ? "" : " on " + m_deviceId));

  // Get power supply manager from ops (not AppContext)
  auto psManager = ops.GetPowerSupplyManager();

  if (!psManager) {
    ops.LogError("PS_EnableOutputOperation: Manager not available");
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", "Manager not available");
    }
    return false;
  }

  PowerSupplyOps psOps(psManager);

  bool success;
  if (!m_deviceId.empty()) {
    success = psOps.SetDeviceOutputEnabled(m_deviceId, m_enable);
  }
  else {
    success = psOps.SetOutputsEnabled(m_enable);
  }

  if (success) {
    ops.LogInfo("PS_EnableOutputOperation: Successfully " +
      std::string(m_enable ? "enabled" : "disabled") + " outputs");

    if (m_delayMs > 0) {
      ops.LogInfo("PS_EnableOutputOperation: Waiting " + std::to_string(m_delayMs) + "ms");
      ops.Wait(m_delayMs);
    }

    if (resultsManager && !opId.empty()) {
      std::map<std::string, std::string> results;
      results["outputs_enabled"] = m_enable ? "true" : "false";
      results["target_device"] = m_deviceId;
      results["device_count"] = std::to_string(psOps.GetConnectedDeviceCount());
      resultsManager->EndOperation(opId, "completed", "Power operation successful", results);
    }
  }
  else {
    ops.LogError("PS_EnableOutputOperation failed: " + psOps.GetLastError());
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", psOps.GetLastError());
    }
  }

  return success;
}


std::string PS_EnableOutputOperation::GetDescription() const {
  std::string desc = std::string(m_enable ? "Enable" : "Disable") + " power supply outputs";
  if (!m_deviceId.empty()) {
    desc += " on " + m_deviceId;
  }
  if (m_delayMs > 0) {
    desc += " (delay: " + std::to_string(m_delayMs) + "ms)";
  }
  return desc;
}

// ============================================================================
// PS_SetVoltageOperation Implementation
// ============================================================================

bool PS_SetVoltageOperation::Execute(MachineOperations& ops) {
  std::string callerContext = "PS_SetVoltageOperation_" +
    std::to_string(m_voltage) + "V_" + std::to_string(m_currentLimit) + "A";
  if (!m_deviceId.empty()) {
    callerContext += "_" + m_deviceId;
  }

  std::string opId;
  std::shared_ptr<OperationResultsManager> resultsManager = ops.GetResultsManager();
  if (resultsManager) {
    std::map<std::string, std::string> parameters;
    parameters["voltage"] = std::to_string(m_voltage);
    parameters["current_limit"] = std::to_string(m_currentLimit);
    parameters["device_id"] = m_deviceId;
    parameters["label"] = m_label;

    opId = resultsManager->StartOperation("PS_SetVoltageOperation", m_deviceId,
      callerContext, "", parameters);
  }

  ops.LogInfo("Setting power supply to " + std::to_string(m_voltage) + "V, " +
    std::to_string(m_currentLimit) + "A limit" +
    (m_deviceId.empty() ? "" : " on " + m_deviceId));

  auto psManager = ops.GetPowerSupplyManager();

  if (!psManager) {
    ops.LogError("PS_SetVoltageOperation: Manager not available");
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", "Manager not available");
    }
    return false;
  }

  bool success;
  if (!m_deviceId.empty()) {
    success = psManager->SetVoltage(m_deviceId, m_voltage) &&
      psManager->SetCurrent(m_deviceId, m_currentLimit);
  }
  else {
    auto result = psManager->SetVoltageAll(m_voltage);
    success = (result.failureCount == 0);
  }

  if (success) {
    ops.LogInfo("PS_SetVoltageOperation: Successfully set voltage");

    if (resultsManager && !opId.empty()) {
      std::map<std::string, std::string> results;
      results["voltage"] = std::to_string(m_voltage);
      results["current_limit"] = std::to_string(m_currentLimit);
      results["target_device"] = m_deviceId;
      resultsManager->EndOperation(opId, "completed", "Voltage set successfully", results);
    }
  }
  else {
    ops.LogError("PS_SetVoltageOperation failed");
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", "Failed to set voltage");
    }
  }

  return success;
}

std::string PS_SetVoltageOperation::GetDescription() const {
  std::string desc = "Set voltage: " + std::to_string(m_voltage) + "V, " +
    std::to_string(m_currentLimit) + "A limit";
  if (!m_deviceId.empty()) {
    desc += " on " + m_deviceId;
  }
  if (!m_label.empty()) {
    desc += " (" + m_label + ")";
  }
  return desc;
}

// ============================================================================
// PS_SetCurrentOperation Implementation
// ============================================================================

bool PS_SetCurrentOperation::Execute(MachineOperations& ops) {
  std::string callerContext = "PS_SetCurrentOperation_" +
    std::to_string(m_current) + "A_" + std::to_string(m_voltageLimit) + "V";
  if (!m_deviceId.empty()) {
    callerContext += "_" + m_deviceId;
  }

  std::string opId;
  std::shared_ptr<OperationResultsManager> resultsManager = ops.GetResultsManager();
  if (resultsManager) {
    std::map<std::string, std::string> parameters;
    parameters["current"] = std::to_string(m_current);
    parameters["voltage_limit"] = std::to_string(m_voltageLimit);
    parameters["device_id"] = m_deviceId;
    parameters["label"] = m_label;

    opId = resultsManager->StartOperation("PS_SetCurrentOperation", m_deviceId,
      callerContext, "", parameters);
  }

  ops.LogInfo("Setting power supply to " + std::to_string(m_current) + "A, " +
    std::to_string(m_voltageLimit) + "V limit" +
    (m_deviceId.empty() ? "" : " on " + m_deviceId));

  auto psManager = ops.GetPowerSupplyManager();

  if (!psManager) {
    ops.LogError("PS_SetCurrentOperation: Manager not available");
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", "Manager not available");
    }
    return false;
  }

  bool success;
  if (!m_deviceId.empty()) {
    success = psManager->SetCurrent(m_deviceId, m_current) &&
      psManager->SetVoltage(m_deviceId, m_voltageLimit);
  }
  else {
    auto result = psManager->SetCurrentAll(m_current);
    success = (result.failureCount == 0);
  }

  if (success) {
    ops.LogInfo("PS_SetCurrentOperation: Successfully set current");

    if (resultsManager && !opId.empty()) {
      std::map<std::string, std::string> results;
      results["current"] = std::to_string(m_current);
      results["voltage_limit"] = std::to_string(m_voltageLimit);
      results["target_device"] = m_deviceId;
      resultsManager->EndOperation(opId, "completed", "Current set successfully", results);
    }
  }
  else {
    ops.LogError("PS_SetCurrentOperation failed");
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", "Failed to set current");
    }
  }

  return success;
}

std::string PS_SetCurrentOperation::GetDescription() const {
  std::string desc = "Set current: " + std::to_string(m_current) + "A, " +
    std::to_string(m_voltageLimit) + "V limit";
  if (!m_deviceId.empty()) {
    desc += " on " + m_deviceId;
  }
  if (!m_label.empty()) {
    desc += " (" + m_label + ")";
  }
  return desc;
}

// ============================================================================
// PS_VoltageSweepOperation Implementation
// ============================================================================

bool PS_VoltageSweepOperation::Execute(MachineOperations& ops) {
  std::string callerContext = "PS_VoltageSweepOperation_" +
    std::to_string(m_startV) + "V_to_" + std::to_string(m_stopV) + "V";
  if (!m_deviceId.empty()) {
    callerContext += "_" + m_deviceId;
  }
  if (!m_label.empty()) {
    callerContext += "_" + m_label;
  }

  std::string opId;
  std::shared_ptr<OperationResultsManager> resultsManager = ops.GetResultsManager();
  if (resultsManager) {
    std::map<std::string, std::string> parameters;
    parameters["start_voltage"] = std::to_string(m_startV);
    parameters["stop_voltage"] = std::to_string(m_stopV);
    parameters["step_size"] = std::to_string(m_stepSize);
    parameters["current_limit"] = std::to_string(m_currentLimit);
    parameters["device_id"] = m_deviceId;
    parameters["delay_ms"] = std::to_string(m_delayMs);
    parameters["label"] = m_label;

    opId = resultsManager->StartOperation("PS_VoltageSweepOperation", m_deviceId,
      callerContext, "", parameters);
  }

  ops.LogInfo("Starting power supply voltage sweep: " + std::to_string(m_startV) + "V to " +
    std::to_string(m_stopV) + "V, step: " + std::to_string(m_stepSize) + "V" +
    " on " + m_deviceId +
    (m_label.empty() ? "" : " (" + m_label + ")"));

  auto psManager = ops.GetPowerSupplyManager();

  if (!psManager) {
    ops.LogError("PS_VoltageSweepOperation: Manager not available");
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", "Manager not available");
    }
    return false;
  }

  PowerSupplyOps psOps(psManager);

  if (!psOps.IsAvailable()) {
    ops.LogError("PS_VoltageSweepOperation: No devices available");
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", "No devices available");
    }
    return false;
  }

  IPowerSupplyDevice::SweepResult result;
  bool success = psOps.PerformVoltageSweep(m_deviceId, m_startV, m_stopV,
    m_stepSize, m_currentLimit,
    m_delayMs, result);

  if (success) {
    ops.LogInfo("PS voltage sweep completed: " + std::to_string(result.measurements.size()) + " points");

    if (resultsManager && !opId.empty()) {
      std::map<std::string, std::string> sweepResults;
      sweepResults["sweep_points"] = std::to_string(result.measurements.size());
      sweepResults["target_device"] = m_deviceId;

      // Add individual data points (limit to first 10 to avoid huge logs)
      size_t pointsToLog = (std::min)(result.measurements.size(), size_t(10));
      for (size_t i = 0; i < pointsToLog; ++i) {
        std::string pointPrefix = "point_" + std::to_string(i) + "_";
        sweepResults[pointPrefix + "voltage"] = std::to_string(result.measurements[i].voltage);
        sweepResults[pointPrefix + "current"] = std::to_string(result.measurements[i].current);
      }

      resultsManager->EndOperation(opId, "completed", "Voltage sweep successful", sweepResults);
    }
  }
  else {
    ops.LogError("PS voltage sweep failed: " + psOps.GetLastError());
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", psOps.GetLastError());
    }
  }

  return success;
}

std::string PS_VoltageSweepOperation::GetDescription() const {
  std::string desc = "PS voltage sweep: " + std::to_string(m_startV) + "V to " +
    std::to_string(m_stopV) + "V (step: " + std::to_string(m_stepSize) + "V)";
  if (!m_deviceId.empty()) {
    desc += " on " + m_deviceId;
  }
  if (!m_label.empty()) {
    desc += " (" + m_label + ")";
  }
  return desc;
}