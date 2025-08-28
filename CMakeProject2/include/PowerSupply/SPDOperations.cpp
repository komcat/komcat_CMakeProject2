// SPDOperations.cpp

// Prevent winsock conflicts before any includes
#ifdef _WIN32
#define _WINSOCKAPI_   // Prevent winsock.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// If you need networking, include winsock2 here
// #include <winsock2.h>
// #include <ws2tcpip.h>
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
  if (!m_label.empty()) {
    callerContext += "_" + m_label;
  }

  // Start operation tracking
  std::string opId;
  auto startTime = std::chrono::steady_clock::now();

  std::shared_ptr<OperationResultsManager> resultsManager = ops.GetResultsManager();
  if (resultsManager && m_storeResults) {
    std::map<std::string, std::string> parameters;
    parameters["label"] = m_label;
    parameters["store_results"] = m_storeResults ? "true" : "false";

    opId = resultsManager->StartOperation("SPD_ReadCurrentVoltageOperation", "",
      callerContext, "", parameters);
  }

  ops.LogInfo("Starting SPD current/voltage reading" +
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

  // Read measurements
  bool success = spdOps.ReadCurrentAndVoltage(m_voltages, m_currents);

  if (success) {
    ops.LogInfo("SPD_ReadCurrentVoltageOperation: Successfully read " +
      std::to_string(m_voltages.size()) + " voltage/current pairs");

    // Store results if requested
    if (resultsManager && m_storeResults && !opId.empty()) {
      std::map<std::string, std::string> results;

      for (size_t i = 0; i < m_voltages.size() && i < m_currents.size(); ++i) {
        results["voltage_" + std::to_string(i)] = std::to_string(m_voltages[i]);
        results["current_" + std::to_string(i)] = std::to_string(m_currents[i]);
      }
      results["device_count"] = std::to_string(m_voltages.size());

      resultsManager->EndOperation(opId, "completed", "Measurements successful");
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

  // Start operation tracking
  std::string opId;
  std::shared_ptr<OperationResultsManager> resultsManager = ops.GetResultsManager();
  if (resultsManager) {
    std::map<std::string, std::string> parameters;
    parameters["enable"] = m_enable ? "true" : "false";
    parameters["delay_ms"] = std::to_string(m_delayMs);

    opId = resultsManager->StartOperation("SPD_EnablePowerOperation", "",
      callerContext, "", parameters);
  }

  ops.LogInfo("SPD Power operation: " + std::string(m_enable ? "Enabling" : "Disabling") +
    " outputs");

  // Create SPD operations instance
  SPD_Ops spdOps;

  if (!spdOps.IsAvailable()) {
    ops.LogError("SPD_EnablePowerOperation: SPD power supplies not available");
    if (resultsManager && !opId.empty()) {
      resultsManager->EndOperation(opId, "failed", "SPD not available");
    }
    return false;
  }

  // Enable/disable outputs
  bool success = spdOps.SetOutputsEnabled(m_enable);

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
      results["device_count"] = std::to_string(spdOps.GetConnectedDeviceCount());
      resultsManager->EndOperation(opId, "completed", "Power operation successful");
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
    std::to_string(m_voltage) + "V_" +
    std::to_string(m_currentLimit) + "A";
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
    parameters["label"] = m_label;

    opId = resultsManager->StartOperation("SPD_SetConstantVoltageOperation", "",
      callerContext, "", parameters);
  }

  ops.LogInfo("Setting SPD to CV mode: " + std::to_string(m_voltage) + "V, " +
    std::to_string(m_currentLimit) + "A limit" +
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

  // Set CV mode
  bool success = spdOps.SetConstantVoltageMode(m_voltage, m_currentLimit);

  if (success) {
    ops.LogInfo("SPD_SetConstantVoltageOperation: Successfully set CV mode");

    if (resultsManager && !opId.empty()) {
      std::map<std::string, std::string> results;
      results["mode"] = "CV";
      results["voltage"] = std::to_string(m_voltage);
      results["current_limit"] = std::to_string(m_currentLimit);
      results["device_count"] = std::to_string(spdOps.GetConnectedDeviceCount());
      resultsManager->EndOperation(opId, "completed", "CV mode set successfully");
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
    std::to_string(m_current) + "A_" +
    std::to_string(m_voltageLimit) + "V";
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
    parameters["label"] = m_label;

    opId = resultsManager->StartOperation("SPD_SetConstantCurrentOperation", "",
      callerContext, "", parameters);
  }

  ops.LogInfo("Setting SPD to CC mode: " + std::to_string(m_current) + "A, " +
    std::to_string(m_voltageLimit) + "V limit" +
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

  // Set CC mode
  bool success = spdOps.SetConstantCurrentMode(m_current, m_voltageLimit);

  if (success) {
    ops.LogInfo("SPD_SetConstantCurrentOperation: Successfully set CC mode");

    if (resultsManager && !opId.empty()) {
      std::map<std::string, std::string> results;
      results["mode"] = "CC";
      results["current"] = std::to_string(m_current);
      results["voltage_limit"] = std::to_string(m_voltageLimit);
      results["device_count"] = std::to_string(spdOps.GetConnectedDeviceCount());
      resultsManager->EndOperation(opId, "completed", "CC mode set successfully");
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
  if (!m_label.empty()) {
    desc += " (" + m_label + ")";
  }
  return desc;
}