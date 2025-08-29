// SPDOperations.cpp

// Prevent winsock conflicts before any includes
#ifdef _WIN32
#define _WINSOCKAPI_   // Prevent winsock.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "SPDOperations.h"
#include "include/machine_operations.h"
#include <chrono>

// ============================================================================
// SPD_ReadCurrentVoltageOperation Implementation
// ============================================================================

bool SPD_ReadCurrentVoltageOperation::Execute(MachineOperations& ops) {
  // Generate caller context
  std::string callerContext = "SPD_ReadCurrentVoltageOperation";
  if (!m_deviceName.empty()) {
    callerContext += "_" + m_deviceName;
  }
  if (!m_label.empty()) {
    callerContext += "_" + m_label;
  }

  // Start operation tracking
  std::string opId;
  std::shared_ptr<OperationResultsManager> resultsManager = ops.GetResultsManager();
  if (resultsManager && m_storeResults) {
    std::map<std::string, std::string> parameters;
    parameters["device_name"] = m_deviceName;
    parameters["label"] = m_label;
    parameters["store_results"] = m_storeResults ? "true" : "false";

    opId = resultsManager->StartOperation("SPD_ReadCurrentVoltageOperation", m_deviceName,
      callerContext, "", parameters);
  }

  ops.LogInfo("Starting SPD current/voltage reading" +
    (m_deviceName.empty() ? "" : " from " + m_deviceName) +
    (m_label.empty() ? "" : " (" + m_label + ")"));

  // Create SPD operations instance
  SPD_Ops spdOps;

  // Check if SPD is available
  if (!spdOps.IsAvailable()) {
    ops.LogError("SPD_ReadCurrentVoltageOperation: SPD power supplies not available");
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", "SPD not available");
    }
    return false;
  }

  // Read measurements - use specific device if provided
  bool success;
  if (!m_deviceName.empty()) {
    success = spdOps.ReadCurrentAndVoltageFromDevice(m_deviceName, m_voltages, m_currents);
  }
  else {
    success = spdOps.ReadCurrentAndVoltage(m_voltages, m_currents);
  }

  if (success) {
    ops.LogInfo("SPD_ReadCurrentVoltageOperation: Successfully read " +
      std::to_string(m_voltages.size()) + " voltage/current pairs");

    if (!m_voltages.empty() && !m_currents.empty()) {
      // Use device name or default prefix for GlobalDataStore keys
      std::string devicePrefix = m_deviceName.empty() ? "SPD1" : m_deviceName;
      ops.SetDataValue(devicePrefix + "-Voltage", m_voltages[0], "SPD_ReadCurrentVoltageOperation");
      ops.SetDataValue(devicePrefix + "-Current", m_currents[0], "SPD_ReadCurrentVoltageOperation");

      ops.LogInfo("Stored in GlobalDataStore: " + devicePrefix + "-Voltage=" +
        std::to_string(m_voltages[0]) + "V, " + devicePrefix + "-Current=" +
        std::to_string(m_currents[0]) + "A");
    }

    // Store results in operation tracking
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
    ops.LogError("SPD_ReadCurrentVoltageOperation failed: " + spdOps.GetLastError());
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", spdOps.GetLastError());
    }
  }

  return success;
}

std::string SPD_ReadCurrentVoltageOperation::GetDescription() const {
  std::string desc = "Read SPD current/voltage measurements";
  if (!m_deviceName.empty()) {
    desc += " from " + m_deviceName;
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
// SPD_EnablePowerOperation Implementation
// ============================================================================

bool SPD_EnablePowerOperation::Execute(MachineOperations& ops) {
  std::string callerContext = "SPD_EnablePowerOperation_" +
    std::string(m_enable ? "ON" : "OFF");
  if (!m_deviceName.empty()) {
    callerContext += "_" + m_deviceName;
  }

  // Start operation tracking
  std::string opId;
  std::shared_ptr<OperationResultsManager> resultsManager = ops.GetResultsManager();
  if (resultsManager) {
    std::map<std::string, std::string> parameters;
    parameters["enable"] = m_enable ? "true" : "false";
    parameters["device_name"] = m_deviceName;
    parameters["delay_ms"] = std::to_string(m_delayMs);

    opId = resultsManager->StartOperation("SPD_EnablePowerOperation", m_deviceName,
      callerContext, "", parameters);
  }

  ops.LogInfo("SPD Power operation: " + std::string(m_enable ? "Enabling" : "Disabling") +
    " outputs" + (m_deviceName.empty() ? "" : " on " + m_deviceName));

  // Create SPD operations instance
  SPD_Ops spdOps;

  if (!spdOps.IsAvailable()) {
    ops.LogError("SPD_EnablePowerOperation: SPD power supplies not available");
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", "SPD not available");
    }
    return false;
  }

  // Enable/disable outputs - specific device or all
  bool success;
  if (!m_deviceName.empty()) {
    success = spdOps.SetDeviceOutputEnabled(m_deviceName, m_enable);
  }
  else {
    success = spdOps.SetOutputsEnabled(m_enable);
  }

  if (success) {
    ops.LogInfo("SPD_EnablePowerOperation: Successfully " +
      std::string(m_enable ? "enabled" : "disabled") + " SPD outputs");

    // Add delay after operation if requested
    if (m_delayMs > 0) {
      ops.LogInfo("SPD_EnablePowerOperation: Waiting " + std::to_string(m_delayMs) + "ms");
      ops.Wait(m_delayMs);
    }

    if (resultsManager && !opId.empty()) {
      std::map<std::string, std::string> results;
      results["outputs_enabled"] = m_enable ? "true" : "false";
      results["target_device"] = m_deviceName;
      results["device_count"] = std::to_string(spdOps.GetConnectedDeviceCount());
      resultsManager->EndOperation(opId, "completed", "Power operation successful", results);
    }
  }
  else {
    ops.LogError("SPD_EnablePowerOperation failed: " + spdOps.GetLastError());
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", spdOps.GetLastError());
    }
  }

  return success;
}

std::string SPD_EnablePowerOperation::GetDescription() const {
  std::string desc = std::string(m_enable ? "Enable" : "Disable") + " SPD power outputs";
  if (!m_deviceName.empty()) {
    desc += " on " + m_deviceName;
  }
  if (m_delayMs > 0) {
    desc += " (delay: " + std::to_string(m_delayMs) + "ms)";
  }
  return desc;
}

// ============================================================================
// SPD_SetConstantVoltageOperation Implementation
// ============================================================================

bool SPD_SetConstantVoltageOperation::Execute(MachineOperations& ops) {
  std::string callerContext = "SPD_SetConstantVoltageOperation_" +
    std::to_string(m_voltage) + "V_" + std::to_string(m_currentLimit) + "A";
  if (!m_deviceName.empty()) {
    callerContext += "_" + m_deviceName;
  }
  if (!m_label.empty()) {
    callerContext += "_" + m_label;
  }

  // Start operation tracking
  std::string opId;
  std::shared_ptr<OperationResultsManager> resultsManager = ops.GetResultsManager();
  if (resultsManager) {
    std::map<std::string, std::string> parameters;
    parameters["voltage"] = std::to_string(m_voltage);
    parameters["current_limit"] = std::to_string(m_currentLimit);
    parameters["device_name"] = m_deviceName;
    parameters["label"] = m_label;

    opId = resultsManager->StartOperation("SPD_SetConstantVoltageOperation", m_deviceName,
      callerContext, "", parameters);
  }

  ops.LogInfo("Setting SPD to CV mode: " + std::to_string(m_voltage) + "V, " +
    std::to_string(m_currentLimit) + "A limit" +
    (m_deviceName.empty() ? "" : " on " + m_deviceName) +
    (m_label.empty() ? "" : " (" + m_label + ")"));

  // Create SPD operations instance
  SPD_Ops spdOps;

  if (!spdOps.IsAvailable()) {
    ops.LogError("SPD_SetConstantVoltageOperation: SPD power supplies not available");
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", "SPD not available");
    }
    return false;
  }

  // Set CV mode - specific device or all
  bool success;
  if (!m_deviceName.empty()) {
    success = spdOps.SetDeviceConstantVoltageMode(m_deviceName, m_voltage, m_currentLimit);
  }
  else {
    success = spdOps.SetConstantVoltageMode(m_voltage, m_currentLimit);
  }

  if (success) {
    ops.LogInfo("SPD_SetConstantVoltageOperation: Successfully set CV mode");

    if (resultsManager && !opId.empty()) {
      std::map<std::string, std::string> results;
      results["mode"] = "CV";
      results["voltage"] = std::to_string(m_voltage);
      results["current_limit"] = std::to_string(m_currentLimit);
      results["target_device"] = m_deviceName;
      results["device_count"] = std::to_string(spdOps.GetConnectedDeviceCount());
      resultsManager->EndOperation(opId, "completed", "CV mode set successfully", results);
    }
  }
  else {
    ops.LogError("SPD_SetConstantVoltageOperation failed: " + spdOps.GetLastError());
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", spdOps.GetLastError());
    }
  }

  return success;
}

std::string SPD_SetConstantVoltageOperation::GetDescription() const {
  std::string desc = "Set SPD CV mode: " + std::to_string(m_voltage) + "V, " +
    std::to_string(m_currentLimit) + "A limit";
  if (!m_deviceName.empty()) {
    desc += " on " + m_deviceName;
  }
  if (!m_label.empty()) {
    desc += " (" + m_label + ")";
  }
  return desc;
}

// ============================================================================
// SPD_SetConstantCurrentOperation Implementation  
// ============================================================================

bool SPD_SetConstantCurrentOperation::Execute(MachineOperations& ops) {
  std::string callerContext = "SPD_SetConstantCurrentOperation_" +
    std::to_string(m_current) + "A_" + std::to_string(m_voltageLimit) + "V";
  if (!m_deviceName.empty()) {
    callerContext += "_" + m_deviceName;
  }
  if (!m_label.empty()) {
    callerContext += "_" + m_label;
  }

  // Start operation tracking
  std::string opId;
  std::shared_ptr<OperationResultsManager> resultsManager = ops.GetResultsManager();
  if (resultsManager) {
    std::map<std::string, std::string> parameters;
    parameters["current"] = std::to_string(m_current);
    parameters["voltage_limit"] = std::to_string(m_voltageLimit);
    parameters["device_name"] = m_deviceName;
    parameters["label"] = m_label;

    opId = resultsManager->StartOperation("SPD_SetConstantCurrentOperation", m_deviceName,
      callerContext, "", parameters);
  }

  ops.LogInfo("Setting SPD to CC mode: " + std::to_string(m_current) + "A, " +
    std::to_string(m_voltageLimit) + "V limit" +
    (m_deviceName.empty() ? "" : " on " + m_deviceName) +
    (m_label.empty() ? "" : " (" + m_label + ")"));

  // Create SPD operations instance
  SPD_Ops spdOps;

  if (!spdOps.IsAvailable()) {
    ops.LogError("SPD_SetConstantCurrentOperation: SPD power supplies not available");
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", "SPD not available");
    }
    return false;
  }

  // Set CC mode - specific device or all
  bool success;
  if (!m_deviceName.empty()) {
    success = spdOps.SetDeviceConstantCurrentMode(m_deviceName, m_current, m_voltageLimit);
  }
  else {
    success = spdOps.SetConstantCurrentMode(m_current, m_voltageLimit);
  }

  if (success) {
    ops.LogInfo("SPD_SetConstantCurrentOperation: Successfully set CC mode");

    if (resultsManager && !opId.empty()) {
      std::map<std::string, std::string> results;
      results["mode"] = "CC";
      results["current"] = std::to_string(m_current);
      results["voltage_limit"] = std::to_string(m_voltageLimit);
      results["target_device"] = m_deviceName;
      results["device_count"] = std::to_string(spdOps.GetConnectedDeviceCount());
      resultsManager->EndOperation(opId, "completed", "CC mode set successfully", results);
    }
  }
  else {
    ops.LogError("SPD_SetConstantCurrentOperation failed: " + spdOps.GetLastError());
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", spdOps.GetLastError());
    }
  }

  return success;
}

std::string SPD_SetConstantCurrentOperation::GetDescription() const {
  std::string desc = "Set SPD CC mode: " + std::to_string(m_current) + "A, " +
    std::to_string(m_voltageLimit) + "V limit";
  if (!m_deviceName.empty()) {
    desc += " on " + m_deviceName;
  }
  if (!m_label.empty()) {
    desc += " (" + m_label + ")";
  }
  return desc;
}

// ============================================================================
// SPD_VoltageSweepOperation Implementation
// ============================================================================

bool SPD_VoltageSweepOperation::Execute(MachineOperations& ops) {
  // Generate caller context
  std::string callerContext = "SPD_VoltageSweepOperation_" +
    std::to_string(m_startV) + "V_to_" + std::to_string(m_stopV) + "V";
  if (!m_deviceName.empty()) {
    callerContext += "_" + m_deviceName;
  }
  if (!m_label.empty()) {
    callerContext += "_" + m_label;
  }

  // Start operation tracking
  std::string opId;
  std::shared_ptr<OperationResultsManager> resultsManager = ops.GetResultsManager();
  if (resultsManager) {
    std::map<std::string, std::string> parameters;
    parameters["start_voltage"] = std::to_string(m_startV);
    parameters["stop_voltage"] = std::to_string(m_stopV);
    parameters["steps"] = std::to_string(m_steps);
    parameters["current_limit"] = std::to_string(m_currentLimit);
    parameters["device_name"] = m_deviceName;
    parameters["delay_ms"] = std::to_string(m_delayMs);
    parameters["label"] = m_label;

    opId = resultsManager->StartOperation("SPD_VoltageSweepOperation", m_deviceName,
      callerContext, "", parameters);
  }

  ops.LogInfo("Starting SPD voltage sweep: " + std::to_string(m_startV) + "V to " +
    std::to_string(m_stopV) + "V, " + std::to_string(m_steps) + " steps" +
    (m_deviceName.empty() ? "" : " on " + m_deviceName) +
    (m_label.empty() ? "" : " (" + m_label + ")"));

  SPD_Ops spdOps;
  if (!spdOps.IsAvailable()) {
    ops.LogError("SPD_VoltageSweepOperation: SPD not available");
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", "SPD not available");
    }
    return false;
  }

  std::vector<SPDSweepResult> results;
  bool success;
  if (!m_deviceName.empty()) {
    success = spdOps.PerformDeviceVoltageSweep(m_deviceName, m_startV, m_stopV, m_steps,
      m_currentLimit, m_delayMs, results);
  }
  else {
    success = spdOps.PerformVoltageSweep(m_startV, m_stopV, m_steps,
      m_currentLimit, m_delayMs, results);
  }

  if (success) {
    ops.LogInfo("SPD voltage sweep completed: " + std::to_string(results.size()) + " points");

    // Store individual sweep results
    if (resultsManager && !opId.empty()) {
      std::map<std::string, std::string> sweepResults;
      sweepResults["sweep_points"] = std::to_string(results.size());
      sweepResults["target_device"] = m_deviceName;

      // Add individual data points
      for (size_t i = 0; i < results.size(); ++i) {
        const auto& point = results[i];
        std::string pointPrefix = "point_" + std::to_string(i) + "_";

        sweepResults[pointPrefix + "set_voltage"] = std::to_string(point.setValue);
        sweepResults[pointPrefix + "measured_voltage"] = std::to_string(point.measuredVoltage);
        sweepResults[pointPrefix + "measured_current"] = std::to_string(point.measuredCurrent);
      }

      resultsManager->EndOperation(opId, "completed", "Voltage sweep successful", sweepResults);
    }
  }
  else {
    ops.LogError("SPD voltage sweep failed: " + spdOps.GetLastError());
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", spdOps.GetLastError());
    }
  }

  return success;
}

std::string SPD_VoltageSweepOperation::GetDescription() const {
  std::string desc = "SPD voltage sweep: " + std::to_string(m_startV) + "V to " +
    std::to_string(m_stopV) + "V (" + std::to_string(m_steps) + " steps)";
  if (!m_deviceName.empty()) {
    desc += " on " + m_deviceName;
  }
  if (!m_label.empty()) {
    desc += " (" + m_label + ")";
  }
  return desc;
}

// ============================================================================
// SPD_CurrentSweepOperation Implementation
// ============================================================================

bool SPD_CurrentSweepOperation::Execute(MachineOperations& ops) {
  // Generate caller context
  std::string callerContext = "SPD_CurrentSweepOperation_" +
    std::to_string(m_startA) + "A_to_" + std::to_string(m_stopA) + "A";
  if (!m_deviceName.empty()) {
    callerContext += "_" + m_deviceName;
  }
  if (!m_label.empty()) {
    callerContext += "_" + m_label;
  }

  // Start operation tracking
  std::string opId;
  std::shared_ptr<OperationResultsManager> resultsManager = ops.GetResultsManager();
  if (resultsManager) {
    std::map<std::string, std::string> parameters;
    parameters["start_current"] = std::to_string(m_startA);
    parameters["stop_current"] = std::to_string(m_stopA);
    parameters["steps"] = std::to_string(m_steps);
    parameters["voltage_limit"] = std::to_string(m_voltageLimit);
    parameters["device_name"] = m_deviceName;
    parameters["delay_ms"] = std::to_string(m_delayMs);
    parameters["label"] = m_label;

    opId = resultsManager->StartOperation("SPD_CurrentSweepOperation", m_deviceName,
      callerContext, "", parameters);
  }

  ops.LogInfo("Starting SPD current sweep: " + std::to_string(m_startA) + "A to " +
    std::to_string(m_stopA) + "A, " + std::to_string(m_steps) + " steps" +
    (m_deviceName.empty() ? "" : " on " + m_deviceName) +
    (m_label.empty() ? "" : " (" + m_label + ")"));

  SPD_Ops spdOps;
  if (!spdOps.IsAvailable()) {
    ops.LogError("SPD_CurrentSweepOperation: SPD not available");
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", "SPD not available");
    }
    return false;
  }

  std::vector<SPDSweepResult> results;
  bool success;
  if (!m_deviceName.empty()) {
    success = spdOps.PerformDeviceCurrentSweep(m_deviceName, m_startA, m_stopA, m_steps,
      m_voltageLimit, m_delayMs, results);
  }
  else {
    success = spdOps.PerformCurrentSweep(m_startA, m_stopA, m_steps,
      m_voltageLimit, m_delayMs, results);
  }

  if (success) {
    ops.LogInfo("SPD current sweep completed: " + std::to_string(results.size()) + " points");

    // Store individual sweep results
    if (resultsManager && !opId.empty()) {
      std::map<std::string, std::string> sweepResults;
      sweepResults["sweep_points"] = std::to_string(results.size());
      sweepResults["target_device"] = m_deviceName;

      // Add individual data points
      for (size_t i = 0; i < results.size(); ++i) {
        const auto& point = results[i];
        std::string pointPrefix = "point_" + std::to_string(i) + "_";

        sweepResults[pointPrefix + "set_current"] = std::to_string(point.setValue);
        sweepResults[pointPrefix + "measured_voltage"] = std::to_string(point.measuredVoltage);
        sweepResults[pointPrefix + "measured_current"] = std::to_string(point.measuredCurrent);
      }

      resultsManager->EndOperation(opId, "completed", "Current sweep successful", sweepResults);
    }
  }
  else {
    ops.LogError("SPD current sweep failed: " + spdOps.GetLastError());
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", spdOps.GetLastError());
    }
  }

  return success;
}

std::string SPD_CurrentSweepOperation::GetDescription() const {
  std::string desc = "SPD current sweep: " + std::to_string(m_startA) + "A to " +
    std::to_string(m_stopA) + "A (" + std::to_string(m_steps) + " steps)";
  if (!m_deviceName.empty()) {
    desc += " on " + m_deviceName;
  }
  if (!m_label.empty()) {
    desc += " (" + m_label + ")";
  }
  return desc;
}