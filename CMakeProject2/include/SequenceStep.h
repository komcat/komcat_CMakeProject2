#pragma once

#include "ProcessStep.h"
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <fstream>           // Add this for file operations
#include <nlohmann/json.hpp> // Add this for JSON operations
#include <iostream>          // Add this for console output
#include <iomanip>           // Add this for formatting
#include "Programming/UserPromptUI.h"
#include <numeric>      // for std::accumulate
#include <algorithm>    // for std::min_element, std::max_element
#include <cmath>        // for std::pow, std::sqrt
#include <limits>       // for std::numeric_limits


// Represents an operation in a sequence
class SequenceOperation {
public:
  virtual ~SequenceOperation() = default;
  virtual bool Execute(MachineOperations& ops) = 0;
  virtual std::string GetDescription() const = 0;
};


// Move device to node operation - UPDATED with caller context
class MoveToNodeOperation : public SequenceOperation {
public:
  MoveToNodeOperation(const std::string& deviceName, const std::string& graphName,
    const std::string& nodeId)
    : m_deviceName(deviceName), m_graphName(graphName), m_nodeId(nodeId) {
  }

  bool Execute(MachineOperations& ops) override {
    // Generate caller context with operation details
    std::string callerContext = "MoveToNodeOperation_" + m_deviceName + "_to_" + m_nodeId;

    // Pass caller context to MachineOperations
    return ops.MoveDeviceToNode(m_deviceName, m_graphName, m_nodeId, true, callerContext);
  }

  std::string GetDescription() const override {
    return "Move " + m_deviceName + " to node " + m_nodeId + " in graph " + m_graphName;
  }

private:
  std::string m_deviceName;
  std::string m_graphName;
  std::string m_nodeId;
};


// Set output operation - UPDATED with caller context
class SetOutputOperation : public SequenceOperation {
public:
  SetOutputOperation(const std::string& deviceName, int pin, bool state, int delayMs = 200)
    : m_deviceName(deviceName), m_pin(pin), m_state(state), m_delayMs(delayMs) {
  }

  bool Execute(MachineOperations& ops) override {
    // Generate caller context with operation details
    std::string callerContext = "SetOutputOperation_" + m_deviceName + "_pin" +
      std::to_string(m_pin) + "_" + (m_state ? "ON" : "OFF");

    // Pass caller context to MachineOperations
    bool result = ops.SetOutput(m_deviceName, m_pin, m_state, callerContext);

    // Add a delay after setting the output
    if (result && m_delayMs > 0) {
      ops.Wait(m_delayMs);
    }

    return result;
  }

  std::string GetDescription() const override {
    return "Set output " + m_deviceName + " pin " + std::to_string(m_pin) +
      " to " + (m_state ? "ON" : "OFF") +
      " (delay: " + std::to_string(m_delayMs) + "ms)";
  }

private:
  std::string m_deviceName;
  int m_pin;
  bool m_state;
  int m_delayMs;
};



// Clear output operation with configurable delay
class ClearOutputOperation : public SequenceOperation {
public:
  ClearOutputOperation(const std::string& deviceName, int pin, int delayMs = 200)
    : m_deviceName(deviceName), m_pin(pin), m_delayMs(delayMs) {
  }

  bool Execute(MachineOperations& ops) override {
    // Generate caller context with operation details
    std::string callerContext = "ClearOutputOperation_" + m_deviceName + "_pin" +
      std::to_string(m_pin);

    // Pass caller context to MachineOperations (clear = false)
    bool result = ops.SetOutput(m_deviceName, m_pin, false, callerContext);

    // Add a delay after clearing the output
    if (result && m_delayMs > 0) {
      ops.Wait(m_delayMs);
    }

    return result;
  }

  std::string GetDescription() const override {
    return "Clear output " + m_deviceName + " pin " + std::to_string(m_pin) +
      " (delay: " + std::to_string(m_delayMs) + "ms)";
  }

private:
  std::string m_deviceName;
  int m_pin;
  int m_delayMs;
};

// Alternative: Dedicated ClearOutput method approach
// If you prefer a separate method instead of using SetOutput(false)

class ClearOutputOperationDedicated : public SequenceOperation {
public:
  ClearOutputOperationDedicated(const std::string& deviceName, int pin, int delayMs = 200)
    : m_deviceName(deviceName), m_pin(pin), m_delayMs(delayMs) {
  }

  bool Execute(MachineOperations& ops) override {
    // Generate caller context with operation details
    std::string callerContext = "ClearOutputOperationDedicated_" + m_deviceName + "_pin" +
      std::to_string(m_pin);

    // Use dedicated ClearOutput method (you'd need to implement this)
    bool result = ops.ClearOutput(m_deviceName, m_pin, callerContext);

    // Add a delay after clearing the output
    if (result && m_delayMs > 0) {
      ops.Wait(m_delayMs);
    }

    return result;
  }

  std::string GetDescription() const override {
    return "Clear output " + m_deviceName + " pin " + std::to_string(m_pin) +
      " (delay: " + std::to_string(m_delayMs) + "ms)";
  }

private:
  std::string m_deviceName;
  int m_pin;
  int m_delayMs;
};


// Sequence step - executes a sequence of operations
class SequenceStep : public ProcessStep {
public:
  SequenceStep(const std::string& name, MachineOperations& machineOps);

  // Add operations to the sequence
  void AddOperation(std::shared_ptr<SequenceOperation> operation);

  // Execute the sequence
  bool Execute() override;

  // Get the operations for this sequence (added for ProcessBuilder)
  const std::vector<std::shared_ptr<SequenceOperation>>& GetOperations() const {
    return m_operations;
  }
  void PrintSequencePlan() const;
private:
  std::vector<std::shared_ptr<SequenceOperation>> m_operations;

};

// Wait operation
class WaitOperation : public SequenceOperation {
public:
  WaitOperation(int milliseconds)
    : m_milliseconds(milliseconds) {
  }

  bool Execute(MachineOperations& ops) override {
    ops.Wait(m_milliseconds);
    return true;
  }

  std::string GetDescription() const override {
    return "Wait for " + std::to_string(m_milliseconds) + " ms";
  }

private:
  int m_milliseconds;
};


// Enhanced ExtendSlideOperation with complete tracking
class ExtendSlideOperation : public SequenceOperation {
public:
  ExtendSlideOperation(const std::string& slideName, bool waitForCompletion = true, int timeoutMs = 5000)
    : m_slideName(slideName), m_waitForCompletion(waitForCompletion), m_timeoutMs(timeoutMs) {
  }

  bool Execute(MachineOperations& ops) override {
    // Generate descriptive caller context for COMPLETE operation
    std::string callerContext = "ExtendSlideOperation_Complete_" + m_slideName;

    // 1. Start COMPLETE operation tracking
    std::string opId;
    auto overallStartTime = std::chrono::steady_clock::now();

    auto resultsManager = ops.GetResultsManager();
    if (resultsManager) {
      std::map<std::string, std::string> parameters = {
          {"slide_name", m_slideName},
          {"wait_for_completion", m_waitForCompletion ? "true" : "false"},
          {"timeout_ms", std::to_string(m_timeoutMs)},
          {"operation_type", "complete_extend_with_tracking"}
      };

      opId = resultsManager->StartOperation("CompleteExtendSlide", m_slideName, callerContext, "", parameters);
    }

    // 2. Get initial state
    SlideState initialState = ops.GetSlideState(m_slideName);
    auto extendStartTime = std::chrono::steady_clock::now();

    ops.LogInfo("ExtendSlideOperation: Starting complete extend tracking for " + m_slideName +
      " (initial state: " + std::to_string(static_cast<int>(initialState)) + ")");

    // 3. Execute the extend operation (this will create a separate ExtendSlide operation)
    bool extendSuccess = ops.ExtendSlide(m_slideName, m_waitForCompletion, m_timeoutMs, callerContext + "_ExtendSlide");

    auto operationEndTime = std::chrono::steady_clock::now();

    // 4. Get final state
    SlideState finalState = ops.GetSlideState(m_slideName);

    // 5. Calculate timing
    auto totalElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(operationEndTime - overallStartTime).count();
    auto extendElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(operationEndTime - extendStartTime).count();

    // 6. Determine operation result
    bool operationSuccess = false;
    std::string resultReason;

    if (!extendSuccess) {
      resultReason = "extend_operation_failed";
    }
    else if (!m_waitForCompletion) {
      operationSuccess = true;
      resultReason = "extend_initiated_no_wait";
    }
    else if (finalState == SlideState::EXTENDED) {
      operationSuccess = true;
      resultReason = "successfully_extended";
    }
    else if (finalState == SlideState::P_ERROR) {
      resultReason = "slide_error_state";
    }
    else if (finalState == SlideState::MOVING) {
      resultReason = "still_moving_timeout";
    }
    else {
      resultReason = "unexpected_final_state";
    }

    // 7. Store comprehensive results
    if (resultsManager && !opId.empty()) {
      resultsManager->StoreResult(opId, "total_operation_time_ms", std::to_string(totalElapsedMs));
      resultsManager->StoreResult(opId, "actual_extend_time_ms", std::to_string(extendElapsedMs));
      resultsManager->StoreResult(opId, "initial_state", std::to_string(static_cast<int>(initialState)));
      resultsManager->StoreResult(opId, "final_state", std::to_string(static_cast<int>(finalState)));
      resultsManager->StoreResult(opId, "initial_state_name", SlideStateToString(initialState));
      resultsManager->StoreResult(opId, "final_state_name", SlideStateToString(finalState));
      resultsManager->StoreResult(opId, "operation_result", resultReason);
      resultsManager->StoreResult(opId, "state_changed", (initialState != finalState) ? "true" : "false");

      if (operationSuccess) {
        ops.LogInfo("ExtendSlideOperation: Successfully completed - " + resultReason);
        resultsManager->EndOperation(opId, "success");
      }
      else {
        ops.LogError("ExtendSlideOperation: Failed - " + resultReason);
        resultsManager->EndOperation(opId, "failed", resultReason);
      }

      // Performance metrics
      if (initialState != SlideState::EXTENDED && finalState == SlideState::EXTENDED) {
        resultsManager->StoreResult(opId, "actual_movement_time_ms", std::to_string(extendElapsedMs));
      }
    }

    return operationSuccess;
  }

  std::string GetDescription() const override {
    return "Complete extend slide " + m_slideName +
      (m_waitForCompletion ? " (wait for completion)" : " (no wait)") +
      " with full tracking";
  }

private:
  std::string m_slideName;
  bool m_waitForCompletion;
  int m_timeoutMs;

  // Helper function to convert slide state to string
  std::string SlideStateToString(SlideState state) const {
    switch (state) {
    case SlideState::EXTENDED: return "EXTENDED";
    case SlideState::RETRACTED: return "RETRACTED";
    case SlideState::MOVING: return "MOVING";
    case SlideState::P_ERROR: return "ERROR";
    default: return "UNKNOWN";
    }
  }
};

// Enhanced RetractSlideOperation with complete tracking
class RetractSlideOperation : public SequenceOperation {
public:
  RetractSlideOperation(const std::string& slideName, bool waitForCompletion = true, int timeoutMs = 5000)
    : m_slideName(slideName), m_waitForCompletion(waitForCompletion), m_timeoutMs(timeoutMs) {
  }

  bool Execute(MachineOperations& ops) override {
    // Generate descriptive caller context for COMPLETE operation
    std::string callerContext = "RetractSlideOperation_Complete_" + m_slideName;

    // 1. Start COMPLETE operation tracking
    std::string opId;
    auto overallStartTime = std::chrono::steady_clock::now();

    auto resultsManager = ops.GetResultsManager();
    if (resultsManager) {
      std::map<std::string, std::string> parameters = {
          {"slide_name", m_slideName},
          {"wait_for_completion", m_waitForCompletion ? "true" : "false"},
          {"timeout_ms", std::to_string(m_timeoutMs)},
          {"operation_type", "complete_retract_with_tracking"}
      };

      opId = resultsManager->StartOperation("CompleteRetractSlide", m_slideName, callerContext, "", parameters);
    }

    // 2. Get initial state
    SlideState initialState = ops.GetSlideState(m_slideName);
    auto retractStartTime = std::chrono::steady_clock::now();

    ops.LogInfo("RetractSlideOperation: Starting complete retract tracking for " + m_slideName +
      " (initial state: " + std::to_string(static_cast<int>(initialState)) + ")");

    // 3. Execute the retract operation (this will create a separate RetractSlide operation)
    bool retractSuccess = ops.RetractSlide(m_slideName, m_waitForCompletion, m_timeoutMs, callerContext + "_RetractSlide");

    auto operationEndTime = std::chrono::steady_clock::now();

    // 4. Get final state
    SlideState finalState = ops.GetSlideState(m_slideName);

    // 5. Calculate timing
    auto totalElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(operationEndTime - overallStartTime).count();
    auto retractElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(operationEndTime - retractStartTime).count();

    // 6. Determine operation result
    bool operationSuccess = false;
    std::string resultReason;

    if (!retractSuccess) {
      resultReason = "retract_operation_failed";
    }
    else if (!m_waitForCompletion) {
      operationSuccess = true;
      resultReason = "retract_initiated_no_wait";
    }
    else if (finalState == SlideState::RETRACTED) {
      operationSuccess = true;
      resultReason = "successfully_retracted";
    }
    else if (finalState == SlideState::P_ERROR) {
      resultReason = "slide_error_state";
    }
    else if (finalState == SlideState::MOVING) {
      resultReason = "still_moving_timeout";
    }
    else {
      resultReason = "unexpected_final_state";
    }

    // 7. Store comprehensive results
    if (resultsManager && !opId.empty()) {
      resultsManager->StoreResult(opId, "total_operation_time_ms", std::to_string(totalElapsedMs));
      resultsManager->StoreResult(opId, "actual_retract_time_ms", std::to_string(retractElapsedMs));
      resultsManager->StoreResult(opId, "initial_state", std::to_string(static_cast<int>(initialState)));
      resultsManager->StoreResult(opId, "final_state", std::to_string(static_cast<int>(finalState)));
      resultsManager->StoreResult(opId, "initial_state_name", SlideStateToString(initialState));
      resultsManager->StoreResult(opId, "final_state_name", SlideStateToString(finalState));
      resultsManager->StoreResult(opId, "operation_result", resultReason);
      resultsManager->StoreResult(opId, "state_changed", (initialState != finalState) ? "true" : "false");

      if (operationSuccess) {
        ops.LogInfo("RetractSlideOperation: Successfully completed - " + resultReason);
        resultsManager->EndOperation(opId, "success");
      }
      else {
        ops.LogError("RetractSlideOperation: Failed - " + resultReason);
        resultsManager->EndOperation(opId, "failed", resultReason);
      }

      // Performance metrics
      if (initialState != SlideState::RETRACTED && finalState == SlideState::RETRACTED) {
        resultsManager->StoreResult(opId, "actual_movement_time_ms", std::to_string(retractElapsedMs));
      }
    }

    return operationSuccess;
  }

  std::string GetDescription() const override {
    return "Complete retract slide " + m_slideName +
      (m_waitForCompletion ? " (wait for completion)" : " (no wait)") +
      " with full tracking";
  }

private:
  std::string m_slideName;
  bool m_waitForCompletion;
  int m_timeoutMs;

  // Helper function to convert slide state to string
  std::string SlideStateToString(SlideState state) const {
    switch (state) {
    case SlideState::EXTENDED: return "EXTENDED";
    case SlideState::RETRACTED: return "RETRACTED";
    case SlideState::MOVING: return "MOVING";
    case SlideState::P_ERROR: return "ERROR";
    default: return "UNKNOWN";
    }
  }
};


// Add these to SequenceStep.h

// Laser On operation
class LaserOnOperation : public SequenceOperation {
public:
  LaserOnOperation(const std::string& laserName = "")
    : m_laserName(laserName) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.LaserOn(m_laserName);
  }

  std::string GetDescription() const override {
    return "Turn laser ON" + (m_laserName.empty() ? "" : " for " + m_laserName);
  }

private:
  std::string m_laserName;
};

// Laser Off operation
class LaserOffOperation : public SequenceOperation {
public:
  LaserOffOperation(const std::string& laserName = "")
    : m_laserName(laserName) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.LaserOff(m_laserName);
  }

  std::string GetDescription() const override {
    return "Turn laser OFF" + (m_laserName.empty() ? "" : " for " + m_laserName);
  }

private:
  std::string m_laserName;
};

// TEC On operation
class TECOnOperation : public SequenceOperation {
public:
  TECOnOperation(const std::string& laserName = "")
    : m_laserName(laserName) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.TECOn(m_laserName);
  }

  std::string GetDescription() const override {
    return "Turn TEC ON" + (m_laserName.empty() ? "" : " for " + m_laserName);
  }

private:
  std::string m_laserName;
};

// TEC Off operation
class TECOffOperation : public SequenceOperation {
public:
  TECOffOperation(const std::string& laserName = "")
    : m_laserName(laserName) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.TECOff(m_laserName);
  }

  std::string GetDescription() const override {
    return "Turn TEC OFF" + (m_laserName.empty() ? "" : " for " + m_laserName);
  }

private:
  std::string m_laserName;
};

// Set Laser Current operation
class SetLaserCurrentOperation : public SequenceOperation {
public:
  SetLaserCurrentOperation(float current, const std::string& laserName = "")
    : m_current(current), m_laserName(laserName) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.SetLaserCurrent(m_current, m_laserName);
  }

  std::string GetDescription() const override {
    return "Set laser current to " + std::to_string(m_current) + "A" +
      (m_laserName.empty() ? "" : " for " + m_laserName);
  }

private:
  float m_current;
  std::string m_laserName;
};

// Set TEC Temperature operation
class SetTECTemperatureOperation : public SequenceOperation {
public:
  SetTECTemperatureOperation(float temperature, const std::string& laserName = "")
    : m_temperature(temperature), m_laserName(laserName) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.SetTECTemperature(m_temperature, m_laserName);
  }

  std::string GetDescription() const override {
    return "Set TEC temperature to " + std::to_string(m_temperature) + "C" +
      (m_laserName.empty() ? "" : " for " + m_laserName);
  }

private:
  float m_temperature;
  std::string m_laserName;
};

// Wait for Temperature operation
class WaitForLaserTemperatureOperation : public SequenceOperation {
public:
  WaitForLaserTemperatureOperation(float targetTemp, float tolerance = 0.5f,
    int timeoutMs = 30000, const std::string& laserName = "")
    : m_targetTemp(targetTemp), m_tolerance(tolerance),
    m_timeoutMs(timeoutMs), m_laserName(laserName) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.WaitForLaserTemperature(m_targetTemp, m_tolerance, m_timeoutMs, m_laserName);
  }

  std::string GetDescription() const override {
    return "Wait for laser temperature to stabilize at " + std::to_string(m_targetTemp) + "C (±" +
      std::to_string(m_tolerance) + "C)" +
      (m_laserName.empty() ? "" : " for " + m_laserName);
  }

private:
  float m_targetTemp;
  float m_tolerance;
  int m_timeoutMs;
  std::string m_laserName;
};



// 1. UPDATE THE LEGACY MoveToPointNameOperation class:
class MoveToPointNameOperation : public SequenceOperation {
public:
  MoveToPointNameOperation(const std::string& deviceName, const std::string& positionName)
    : m_deviceName(deviceName), m_positionName(positionName) {
  }

  bool Execute(MachineOperations& ops) override {
    // Generate caller context - FOLLOWING EXISTING PATTERN
    std::string callerContext = "MoveToPointNameOperation_" + m_deviceName + "_to_" + m_positionName;

    // Pass caller context to MachineOperations  
    return ops.MoveToPointName(m_deviceName, m_positionName, true, callerContext);
  }

  std::string GetDescription() const override {
    return "Move " + m_deviceName + " to named position " + m_positionName;
  }

private:
  std::string m_deviceName;
  std::string m_positionName;
};


// SCANNING OPERATIONS - NEW ADDITIONS

// Start Scan operation

// UPDATED StartScanOperation with caller context
class StartScanOperation : public SequenceOperation {
public:
  StartScanOperation(const std::string& deviceName, const std::string& dataChannel,
    const std::vector<double>& stepSizes, int settlingTimeMs,
    const std::vector<std::string>& axesToScan = { "Z", "X", "Y" })
    : m_deviceName(deviceName), m_dataChannel(dataChannel),
    m_stepSizes(stepSizes), m_settlingTimeMs(settlingTimeMs),
    m_axesToScan(axesToScan) {
  }

  bool Execute(MachineOperations& ops) override {
    // Generate descriptive caller context
    std::string callerContext = "StartScanOperation_" + m_deviceName + "_" + m_dataChannel;

    // Call machine operations with tracking
    return ops.StartScan(m_deviceName, m_dataChannel, m_stepSizes, m_settlingTimeMs, m_axesToScan, callerContext);
  }

  std::string GetDescription() const override {
    std::string axesStr;
    for (size_t i = 0; i < m_axesToScan.size(); ++i) {
      axesStr += m_axesToScan[i];
      if (i < m_axesToScan.size() - 1) axesStr += ", ";
    }

    std::string stepsStr;
    for (size_t i = 0; i < m_stepSizes.size(); ++i) {
      stepsStr += std::to_string(m_stepSizes[i] * 1000) + " µm";
      if (i < m_stepSizes.size() - 1) stepsStr += ", ";
    }

    return "Start scan on " + m_deviceName + " using " + m_dataChannel +
      " channel, scanning " + axesStr + " axes with steps " + stepsStr;
  }

private:
  std::string m_deviceName;
  std::string m_dataChannel;
  std::vector<double> m_stepSizes;
  int m_settlingTimeMs;
  std::vector<std::string> m_axesToScan;
};

// UPDATED StopScanOperation with caller context
class StopScanOperation : public SequenceOperation {
public:
  StopScanOperation(const std::string& deviceName)
    : m_deviceName(deviceName) {
  }

  bool Execute(MachineOperations& ops) override {
    // Generate descriptive caller context
    std::string callerContext = "StopScanOperation_" + m_deviceName;

    // Call machine operations with tracking
    return ops.StopScan(m_deviceName, callerContext);
  }

  std::string GetDescription() const override {
    return "Stop scan on " + m_deviceName;
  }

private:
  std::string m_deviceName;
};

// Wait For Scan Completion operation
class WaitForScanCompletionOperation : public SequenceOperation {
public:
  WaitForScanCompletionOperation(const std::string& deviceName, int timeoutMs = 1800000)
    : m_deviceName(deviceName), m_timeoutMs(timeoutMs) {
  }

  bool Execute(MachineOperations& ops) override {
    auto startTime = std::chrono::steady_clock::now();
    auto endTime = startTime + std::chrono::milliseconds(m_timeoutMs);

    // Wait until the scan is no longer active or timeout
    while (ops.IsScanActive(m_deviceName)) {
      // Check if we've exceeded the timeout
      if (std::chrono::steady_clock::now() > endTime) {
        // Timeout occurred, stop the scan
        std::string callerContext = "WaitForScanCompletionOperation_" + m_deviceName + "_timeout";
        ops.StopScan(m_deviceName, callerContext);
        return false;
      }

      // Sleep to avoid high CPU usage
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return true;
  }

  std::string GetDescription() const override {
    return "Wait for scan completion on " + m_deviceName +
      " (timeout: " + std::to_string(m_timeoutMs / 1000) + " seconds)";
  }

private:
  std::string m_deviceName;
  int m_timeoutMs;
};





class RunScanOperation : public SequenceOperation {
public:
  RunScanOperation(
    const std::string& deviceName,
    const std::string& dataChannel,
    const std::vector<double>& stepSizes = { 0.002, 0.001, 0.0005 },
    int settlingTimeMs = 300,
    const std::vector<std::string>& axesToScan = { "Z", "X", "Y" },
    int timeoutMs = 1800000)
    : m_deviceName(deviceName),
    m_dataChannel(dataChannel),
    m_stepSizes(stepSizes),
    m_settlingTimeMs(settlingTimeMs),
    m_axesToScan(axesToScan),
    m_timeoutMs(timeoutMs) {
  }

  bool Execute(MachineOperations& ops) override {
    // Generate descriptive caller context for COMPLETE scan operation
    std::string callerContext = "RunScanOperation_Complete_" + m_deviceName + "_" + m_dataChannel;

    // 1. Start COMPLETE scan tracking (not just StartScan)
    std::string opId;
    auto overallStartTime = std::chrono::steady_clock::now();

    auto resultsManager = ops.GetResultsManager();
    if (resultsManager) {
      std::map<std::string, std::string> parameters = {
          {"device_name", m_deviceName},
          {"data_channel", m_dataChannel},
          {"settling_time_ms", std::to_string(m_settlingTimeMs)},
          {"timeout_ms", std::to_string(m_timeoutMs)},
          {"axes_count", std::to_string(m_axesToScan.size())},
          {"steps_count", std::to_string(m_stepSizes.size())}
      };

      // Add step sizes and axes to parameters
      for (size_t i = 0; i < m_stepSizes.size() && i < 3; ++i) {
        parameters["step_size_" + std::to_string(i)] = std::to_string(m_stepSizes[i]);
      }
      for (size_t i = 0; i < m_axesToScan.size() && i < 3; ++i) {
        parameters["axis_" + std::to_string(i)] = m_axesToScan[i];
      }

      opId = resultsManager->StartOperation("CompleteScan", m_deviceName, callerContext, "", parameters);
    }

    // 2. Start the scan with tracking (this will create a separate StartScan operation)
    if (!ops.StartScan(m_deviceName, m_dataChannel, m_stepSizes, m_settlingTimeMs, m_axesToScan, callerContext + "_Start")) {
      // End complete scan tracking with failure
      if (resultsManager && !opId.empty()) {
        auto endTime = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - overallStartTime).count();

        resultsManager->StoreResult(opId, "total_scan_time_ms", std::to_string(elapsedMs));
        resultsManager->StoreResult(opId, "scan_result", "failed_to_start");
        resultsManager->EndOperation(opId, "failed", "Failed to start scan");
      }
      return false;
    }

    // 3. Wait for completion with progress tracking
    auto scanStartTime = std::chrono::steady_clock::now();
    auto endTime = scanStartTime + std::chrono::milliseconds(m_timeoutMs);

    bool scanCompleted = false;
    bool timedOut = false;

    while (ops.IsScanActive(m_deviceName)) {
      if (std::chrono::steady_clock::now() > endTime) {
        // Timeout - stop the scan
        std::string stopContext = callerContext + "_timeout_stop";
        ops.StopScan(m_deviceName, stopContext);
        timedOut = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    auto overallEndTime = std::chrono::steady_clock::now();
    auto totalElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(overallEndTime - overallStartTime).count();
    auto scanElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(overallEndTime - scanStartTime).count();

    // 4. Get scan results
    double peakValue = 0.0;
    PositionStruct peakPosition;
    bool hasPeak = ops.GetScanPeak(m_deviceName, peakValue, peakPosition);

    // 5. Store comprehensive results
    if (resultsManager && !opId.empty()) {
      resultsManager->StoreResult(opId, "total_scan_time_ms", std::to_string(totalElapsedMs));
      resultsManager->StoreResult(opId, "actual_scan_time_ms", std::to_string(scanElapsedMs));

      if (timedOut) {
        resultsManager->StoreResult(opId, "scan_result", "timeout");
        resultsManager->StoreResult(opId, "timeout_after_ms", std::to_string(m_timeoutMs));
        resultsManager->EndOperation(opId, "failed", "Scan timed out after " + std::to_string(m_timeoutMs / 1000) + " seconds");
      }
      else if (hasPeak) {
        resultsManager->StoreResult(opId, "scan_result", "success");
        resultsManager->StoreResult(opId, "peak_value", std::to_string(peakValue));
        resultsManager->StoreResult(opId, "peak_position_x", std::to_string(peakPosition.x));
        resultsManager->StoreResult(opId, "peak_position_y", std::to_string(peakPosition.y));
        resultsManager->StoreResult(opId, "peak_position_z", std::to_string(peakPosition.z));
        resultsManager->StoreResult(opId, "scan_status", ops.GetScanStatus(m_deviceName));
        resultsManager->EndOperation(opId, "success");
        scanCompleted = true;
      }
      else {
        resultsManager->StoreResult(opId, "scan_result", "completed_no_peak");
        resultsManager->StoreResult(opId, "scan_status", ops.GetScanStatus(m_deviceName));
        resultsManager->EndOperation(opId, "failed", "Scan completed but no peak found");
      }
    }

    return scanCompleted && !timedOut;
  }

  std::string GetDescription() const override {
    std::string axesStr;
    for (size_t i = 0; i < m_axesToScan.size(); ++i) {
      axesStr += m_axesToScan[i];
      if (i < m_axesToScan.size() - 1) axesStr += ", ";
    }

    std::string stepsStr;
    for (size_t i = 0; i < m_stepSizes.size(); ++i) {
      stepsStr += std::to_string(m_stepSizes[i] * 1000) + " µm";
      if (i < m_stepSizes.size() - 1) stepsStr += ", ";
    }

    return "Run complete scan on " + m_deviceName + " using " + m_dataChannel +
      " over " + axesStr + " axes with " + stepsStr + " steps (with full tracking)";
  }

private:
  std::string m_deviceName;
  std::string m_dataChannel;
  std::vector<double> m_stepSizes;
  int m_settlingTimeMs;
  std::vector<std::string> m_axesToScan;
  int m_timeoutMs;
};




// BlockingRunScanOperation (complete scan with full tracking from start to finish)
class BlockingRunScanOperation : public SequenceOperation {
public:
  BlockingRunScanOperation(
    const std::string& deviceName,
    const std::string& dataChannel,
    const std::vector<double>& stepSizes = { 0.002, 0.001, 0.0005 },
    int settlingTimeMs = 300,
    const std::vector<std::string>& axesToScan = { "Z", "X", "Y" })
    : m_deviceName(deviceName),
    m_dataChannel(dataChannel),
    m_stepSizes(stepSizes),
    m_settlingTimeMs(settlingTimeMs),
    m_axesToScan(axesToScan) {
  }

  bool Execute(MachineOperations& ops) override {
    // Generate descriptive caller context for COMPLETE scan operation
    std::string callerContext = "BlockingRunScanOperation_" + m_deviceName + "_" + m_dataChannel;

    // 1. Start COMPLETE scan tracking (not just PerformScan)
    std::string opId;
    auto overallStartTime = std::chrono::steady_clock::now();

    auto resultsManager = ops.GetResultsManager();
    if (resultsManager) {
      std::map<std::string, std::string> parameters = {
          {"device_name", m_deviceName},
          {"data_channel", m_dataChannel},
          {"settling_time_ms", std::to_string(m_settlingTimeMs)},
          {"axes_count", std::to_string(m_axesToScan.size())},
          {"steps_count", std::to_string(m_stepSizes.size())},
          {"scan_type", "blocking_complete_scan"}
      };

      // Add step sizes and axes to parameters
      for (size_t i = 0; i < m_stepSizes.size() && i < 3; ++i) {
        parameters["step_size_" + std::to_string(i)] = std::to_string(m_stepSizes[i]);
      }
      for (size_t i = 0; i < m_axesToScan.size() && i < 3; ++i) {
        parameters["axis_" + std::to_string(i)] = m_axesToScan[i];
      }

      opId = resultsManager->StartOperation("BlockingCompleteScan", m_deviceName, callerContext, "", parameters);
    }

    ops.LogInfo("BlockingRunScanOperation: Starting complete scan tracking for " + m_deviceName);

    // 2. Execute the blocking scan (this will create a separate PerformScan operation)
    auto scanStartTime = std::chrono::steady_clock::now();
    bool scanSuccess = ops.PerformScan(m_deviceName, m_dataChannel, m_stepSizes, m_settlingTimeMs, m_axesToScan, callerContext + "_PerformScan");
    auto scanEndTime = std::chrono::steady_clock::now();

    // 3. Calculate timing
    auto totalElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(scanEndTime - overallStartTime).count();
    auto scanElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(scanEndTime - scanStartTime).count();

    // 4. Get scan results
    double peakValue = 0.0;
    PositionStruct peakPosition;
    bool hasPeak = ops.GetScanPeak(m_deviceName, peakValue, peakPosition);
    std::string scanStatus = ops.GetScanStatus(m_deviceName);

    // 5. Store comprehensive results
    if (resultsManager && !opId.empty()) {
      resultsManager->StoreResult(opId, "total_operation_time_ms", std::to_string(totalElapsedMs));
      resultsManager->StoreResult(opId, "actual_scan_time_ms", std::to_string(scanElapsedMs));
      resultsManager->StoreResult(opId, "scan_final_status", scanStatus);

      if (scanSuccess) {
        if (hasPeak) {
          resultsManager->StoreResult(opId, "scan_result", "success_with_peak");
          resultsManager->StoreResult(opId, "peak_value", std::to_string(peakValue));
          resultsManager->StoreResult(opId, "peak_position_x", std::to_string(peakPosition.x));
          resultsManager->StoreResult(opId, "peak_position_y", std::to_string(peakPosition.y));
          resultsManager->StoreResult(opId, "peak_position_z", std::to_string(peakPosition.z));
          resultsManager->StoreResult(opId, "peak_position_u", std::to_string(peakPosition.u));
          resultsManager->StoreResult(opId, "peak_position_v", std::to_string(peakPosition.v));
          resultsManager->StoreResult(opId, "peak_position_w", std::to_string(peakPosition.w));

          ops.LogInfo("BlockingRunScanOperation: Scan completed successfully with peak value " + std::to_string(peakValue));
          resultsManager->EndOperation(opId, "success");
        }
        else {
          resultsManager->StoreResult(opId, "scan_result", "success_no_peak");
          ops.LogWarning("BlockingRunScanOperation: Scan completed but no peak found");
          resultsManager->EndOperation(opId, "success"); // Still success, just no peak
        }
      }
      else {
        resultsManager->StoreResult(opId, "scan_result", "failed");
        ops.LogError("BlockingRunScanOperation: Scan failed");
        resultsManager->EndOperation(opId, "failed", "Blocking scan operation failed");
      }

      // Additional performance metrics
      if (totalElapsedMs > 0) {
        resultsManager->StoreResult(opId, "scan_efficiency_pct", std::to_string((scanElapsedMs * 100) / totalElapsedMs));
      }
    }

    return scanSuccess;
  }

  std::string GetDescription() const override {
    std::string axesStr;
    for (size_t i = 0; i < m_axesToScan.size(); ++i) {
      axesStr += m_axesToScan[i];
      if (i < m_axesToScan.size() - 1) axesStr += ", ";
    }

    std::string stepsStr;
    for (size_t i = 0; i < m_stepSizes.size(); ++i) {
      stepsStr += std::to_string(m_stepSizes[i] * 1000) + " µm";
      if (i < m_stepSizes.size() - 1) stepsStr += ", ";
    }

    return "Blocking complete scan on " + m_deviceName + " using " + m_dataChannel +
      " over " + axesStr + " axes with " + stepsStr + " steps (full tracking)";
  }

private:
  std::string m_deviceName;
  std::string m_dataChannel;
  std::vector<double> m_stepSizes;
  int m_settlingTimeMs;
  std::vector<std::string> m_axesToScan;
};

// Keep the original PerformScanOperation for simple cases (just delegates to PerformScan)
class PerformScanOperation : public SequenceOperation {
public:
  PerformScanOperation(
    const std::string& deviceName,
    const std::string& dataChannel,
    const std::vector<double>& stepSizes = { 0.002, 0.001, 0.0005 },
    int settlingTimeMs = 300,
    const std::vector<std::string>& axesToScan = { "Z", "X", "Y" })
    : m_deviceName(deviceName),
    m_dataChannel(dataChannel),
    m_stepSizes(stepSizes),
    m_settlingTimeMs(settlingTimeMs),
    m_axesToScan(axesToScan) {
  }

  bool Execute(MachineOperations& ops) override {
    // Generate descriptive caller context
    std::string callerContext = "PerformScanOperation_" + m_deviceName + "_" + m_dataChannel;

    // Simple delegation to machine operations (only PerformScan tracking)
    return ops.PerformScan(m_deviceName, m_dataChannel, m_stepSizes, m_settlingTimeMs, m_axesToScan, callerContext);
  }

  std::string GetDescription() const override {
    std::string axesStr;
    for (size_t i = 0; i < m_axesToScan.size(); ++i) {
      axesStr += m_axesToScan[i];
      if (i < m_axesToScan.size() - 1) axesStr += ", ";
    }

    std::string stepsStr;
    for (size_t i = 0; i < m_stepSizes.size(); ++i) {
      stepsStr += std::to_string(m_stepSizes[i] * 1000) + " µm";
      if (i < m_stepSizes.size() - 1) stepsStr += ", ";
    }

    return "Perform scan on " + m_deviceName + " using " + m_dataChannel +
      " over " + axesStr + " axes with " + stepsStr + " steps (basic tracking)";
  }

private:
  std::string m_deviceName;
  std::string m_dataChannel;
  std::vector<double> m_stepSizes;
  int m_settlingTimeMs;
  std::vector<std::string> m_axesToScan;
};


// Add this to SequenceStep.h after the other operation classes
class ParallelDeviceMovementOperation : public SequenceOperation {
public:
  // Constructor for named position movements
  ParallelDeviceMovementOperation(
    std::vector<std::pair<std::string, std::string>> devicePositions,
    const std::string& description = "Parallel Device Movement")
    : m_devicePositions(std::move(devicePositions)), m_description(description) {
  }

  bool Execute(MachineOperations& ops) override {
    // Start all movements in non-blocking mode
    for (const auto& [deviceName, positionName] : m_devicePositions) {
      ops.LogInfo("Starting movement of " + deviceName + " to position " + positionName);
      if (!ops.MoveToPointName(deviceName, positionName, false)) {
        ops.LogError("Failed to start movement for device " + deviceName);
        return false;
      }
    }

    // Wait for all devices to complete their movements
    bool allSucceeded = true;
    for (const auto& [deviceName, positionName] : m_devicePositions) {
      ops.LogInfo("Waiting for " + deviceName + " to complete movement");
      if (!ops.WaitForDeviceMotionCompletion(deviceName,30000)) {
        ops.LogError("Timeout waiting for device " + deviceName + " to complete movement");
        allSucceeded = false;
        // Continue waiting for other devices even if one fails
      }
    }

    return allSucceeded;
  }

  std::string GetDescription() const override {
    return m_description;
  }

private:
  std::vector<std::pair<std::string, std::string>> m_devicePositions;
  std::string m_description;
};


// Initialize Camera operation
class InitializeCameraOperation : public SequenceOperation {
public:
  InitializeCameraOperation() {}

  bool Execute(MachineOperations& ops) override {
    return ops.InitializeCamera();
  }

  std::string GetDescription() const override {
    return "Initialize camera";
  }
};

// Connect Camera operation
class ConnectCameraOperation : public SequenceOperation {
public:
  ConnectCameraOperation() {}

  bool Execute(MachineOperations& ops) override {
    return ops.ConnectCamera();
  }

  std::string GetDescription() const override {
    return "Connect to camera";
  }
};

// Start Camera Grabbing operation
class StartCameraGrabbingOperation : public SequenceOperation {
public:
  StartCameraGrabbingOperation() {}

  bool Execute(MachineOperations& ops) override {
    return ops.StartCameraGrabbing();
  }

  std::string GetDescription() const override {
    return "Start camera grabbing";
  }
};

// Stop Camera Grabbing operation
class StopCameraGrabbingOperation : public SequenceOperation {
public:
  StopCameraGrabbingOperation() {}

  bool Execute(MachineOperations& ops) override {
    return ops.StopCameraGrabbing();
  }

  std::string GetDescription() const override {
    return "Stop camera grabbing";
  }
};

// Capture Image operation
class CaptureImageOperation : public SequenceOperation {
public:
  CaptureImageOperation(const std::string& filename = "")
    : m_filename(filename) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.CaptureImageToFile(m_filename);
  }

  std::string GetDescription() const override {
    return "Capture image" + (m_filename.empty() ? "" : " to " + m_filename);
  }

private:
  std::string m_filename;
};

// Wait for Camera Initialization operation (combines initialize and connect)
class WaitForCameraReadyOperation : public SequenceOperation {
public:
  WaitForCameraReadyOperation(int timeoutMs = 5000)
    : m_timeoutMs(timeoutMs) {
  }

  bool Execute(MachineOperations& ops) override {
    // First initialize if not already initialized
    if (!ops.IsCameraInitialized()) {
      if (!ops.InitializeCamera()) {
        return false;
      }
    }

    // Then connect if not already connected
    if (!ops.IsCameraConnected()) {
      if (!ops.ConnectCamera()) {
        return false;
      }
    }

    // Wait for the camera to be truly ready with a timeout
    auto startTime = std::chrono::steady_clock::now();
    auto endTime = startTime + std::chrono::milliseconds(m_timeoutMs);

    while (std::chrono::steady_clock::now() < endTime) {
      if (ops.IsCameraConnected()) {
        return true;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Timeout expired
    return false;
  }

  std::string GetDescription() const override {
    return "Wait for camera to be ready (timeout: " + std::to_string(m_timeoutMs) + "ms)";
  }

private:
  int m_timeoutMs;
};

// Add this after the other operation classes in SequenceStep.h
// REPLACE existing MoveRelativeOperation class in SequenceStep.h:
class MoveRelativeOperation : public SequenceOperation {
public:
  MoveRelativeOperation(const std::string& deviceName, const std::string& axis, double distance)
    : m_deviceName(deviceName), m_axis(axis), m_distance(distance) {
  }

  bool Execute(MachineOperations& ops) override {
    // Generate caller context with operation details - FOLLOWING EXISTING PATTERN
    std::string callerContext = "MoveRelativeOperation_" + m_deviceName + "_" + m_axis +
      "_" + std::to_string(m_distance);

    // Pass caller context to MachineOperations
    return ops.MoveRelative(m_deviceName, m_axis, m_distance, true, callerContext);
  }

  std::string GetDescription() const override {
    return "Move " + m_deviceName + " relative on " + m_axis + " axis by " + std::to_string(m_distance);
  }

private:
  std::string m_deviceName;
  std::string m_axis;
  double m_distance;
};


/// <summary>
/// Enhanced operation that periodically reads and logs a data value from the GlobalDataStore
/// for a specified duration and at specified intervals, with comprehensive tracking and statistics.
/// </summary>
class PeriodicMonitorDataValueOperation : public SequenceOperation {
public:
  /// <summary>
  /// Creates a new operation to periodically monitor a data value with tracking.
  /// </summary>
  /// <param name="dataId">The identifier of the data channel to monitor.</param>
  /// <param name="durationMs">The total duration to monitor in milliseconds.</param>
  /// <param name="intervalMs">The interval between readings in milliseconds.</param>
  /// <param name="enableIntermediateLogging">Whether to log every reading (default: false).</param>
  /// <param name="logEveryN">Log every N readings when intermediate logging is enabled (default: 10).</param>
  /// <param name="enableThresholdAlerts">Whether to check threshold violations (default: false).</param>
  /// <param name="minThreshold">Minimum threshold value (default: -1e-6 for nano-scale values).</param>
  /// <param name="maxThreshold">Maximum threshold value (default: 1e-6 for nano-scale values).</param>
  PeriodicMonitorDataValueOperation(const std::string& dataId, int durationMs, int intervalMs,
    bool enableIntermediateLogging = false, int logEveryN = 10,
    bool enableThresholdAlerts = false, double minThreshold = -1e-6, double maxThreshold = 1e-6)
    : m_dataId(dataId), m_durationMs(durationMs), m_intervalMs(intervalMs),
    m_enableIntermediateLogging(enableIntermediateLogging), m_logEveryN(logEveryN),
    m_enableThresholdAlerts(enableThresholdAlerts),
    m_minThreshold(minThreshold), m_maxThreshold(maxThreshold) {
  }

  /// <summary>
  /// Simple constructor for basic monitoring (no intermediate logging, no thresholds).
  /// </summary>
  static std::unique_ptr<PeriodicMonitorDataValueOperation> CreateBasic(
    const std::string& dataId, int durationMs, int intervalMs) {
    return std::make_unique<PeriodicMonitorDataValueOperation>(dataId, durationMs, intervalMs);
  }

  /// <summary>
  /// Constructor for monitoring with intermediate logging enabled.
  /// </summary>
  static std::unique_ptr<PeriodicMonitorDataValueOperation> CreateWithLogging(
    const std::string& dataId, int durationMs, int intervalMs, int logEveryN = 10) {
    return std::make_unique<PeriodicMonitorDataValueOperation>(
      dataId, durationMs, intervalMs, true, logEveryN);
  }

  /// <summary>
  /// Constructor for monitoring with threshold alerts enabled.
  /// </summary>
  static std::unique_ptr<PeriodicMonitorDataValueOperation> CreateWithThresholds(
    const std::string& dataId, int durationMs, int intervalMs,
    double minThreshold, double maxThreshold) {
    return std::make_unique<PeriodicMonitorDataValueOperation>(
      dataId, durationMs, intervalMs, false, 10, true, minThreshold, maxThreshold);
  }

  /// <summary>
  /// Constructor for full-featured monitoring with both logging and thresholds.
  /// </summary>
  static std::unique_ptr<PeriodicMonitorDataValueOperation> CreateFullFeatured(
    const std::string& dataId, int durationMs, int intervalMs,
    int logEveryN, double minThreshold, double maxThreshold) {
    return std::make_unique<PeriodicMonitorDataValueOperation>(
      dataId, durationMs, intervalMs, true, logEveryN, true, minThreshold, maxThreshold);
  }

  /// <summary>
  /// Constructor specifically for nano-scale measurements with appropriate thresholds.
  /// </summary>
  static std::unique_ptr<PeriodicMonitorDataValueOperation> CreateForNanoScale(
    const std::string& dataId, int durationMs, int intervalMs,
    double minNano = 0.0005, double maxNano = 0.15) {
    // Convert nano values to actual thresholds (assuming nano means 1e-9 units)
    double minThreshold = minNano * 1e-9;
    double maxThreshold = maxNano * 1e-9;
    return std::make_unique<PeriodicMonitorDataValueOperation>(
      dataId, durationMs, intervalMs, false, 10, true, minThreshold, maxThreshold);
  }

  /// <summary>
  /// Executes the operation with comprehensive tracking and statistics.
  /// </summary>
  /// <param name="ops">Reference to MachineOperations for accessing the data store.</param>
  /// <returns>True if the monitoring completed successfully, false otherwise.</returns>
  bool Execute(MachineOperations& ops) override {
    // 1. Start operation tracking
    std::string opId;
    auto startTime = std::chrono::steady_clock::now();

    if (ops.GetResultsManager()) {
      std::map<std::string, std::string> parameters = {
        {"dataId", m_dataId},
        {"durationMs", std::to_string(m_durationMs)},
        {"intervalMs", std::to_string(m_intervalMs)},
        {"enableIntermediateLogging", m_enableIntermediateLogging ? "true" : "false"},
        {"logEveryN", std::to_string(m_logEveryN)},
        {"enableThresholdAlerts", m_enableThresholdAlerts ? "true" : "false"},
        {"minThreshold", std::to_string(m_minThreshold)},
        {"maxThreshold", std::to_string(m_maxThreshold)}
      };
      opId = ops.GetResultsManager()->StartOperation("PeriodicMonitor", m_dataId,
        "PeriodicMonitorDataValueOperation", "", parameters);
    }

    ops.LogInfo("Starting periodic monitoring of " + m_dataId +
      " for " + std::to_string(m_durationMs / 1000) + " seconds");

    // 2. Initialize tracking variables
    std::vector<double> readings;
    std::vector<std::chrono::steady_clock::time_point> readingTimes;
    int successfulReads = 0;
    int failedReads = 0;
    int thresholdViolations = 0;
    double startValue = std::numeric_limits<double>::quiet_NaN();
    double endValue = std::numeric_limits<double>::quiet_NaN();
    bool hasValidReading = false;

    auto endTime = startTime + std::chrono::milliseconds(m_durationMs);
    int readingCount = 0;

    // 3. Monitoring loop
    while (std::chrono::steady_clock::now() < endTime) {
      auto readingStartTime = std::chrono::steady_clock::now();

      try {
        double value = static_cast<double>(ops.ReadDataValue(m_dataId));
        readingCount++;

        // Store reading and timestamp
        readings.push_back(value);
        readingTimes.push_back(readingStartTime);
        successfulReads++;
        hasValidReading = true;

        // Store start value (first successful reading)
        if (successfulReads == 1) {
          startValue = value;
        }
        endValue = value; // Always update end value with latest reading

        // ALWAYS log each reading with proper precision for nano-scale values
        // Use scientific notation if value is very small (< 1e-6) or use high precision
        std::ostringstream valueStream;
        if (std::abs(value) < 1e-6 && value != 0.0) {
          valueStream << std::scientific << std::setprecision(6) << value;
        }
        else {
          valueStream << std::fixed << std::setprecision(12) << value;
        }
        ops.LogInfo("Read value from " + m_dataId + ": " + valueStream.str());

        // Check threshold violations if enabled
        if (m_enableThresholdAlerts) {
          if (value < m_minThreshold || value > m_maxThreshold) {
            thresholdViolations++;

            // Use scientific notation for very small values
            std::ostringstream valueStr, minStr, maxStr;
            valueStr << std::scientific << std::setprecision(6) << value;
            minStr << std::scientific << std::setprecision(6) << m_minThreshold;
            maxStr << std::scientific << std::setprecision(6) << m_maxThreshold;

            ops.LogWarning("Threshold violation in " + m_dataId + ": " +
              valueStr.str() + " (limits: " + minStr.str() +
              " to " + maxStr.str() + ")");
          }
        }

        // Additional intermediate logging if enabled (different from the always-on logging above)
        if (m_enableIntermediateLogging && (readingCount % m_logEveryN == 0)) {
          std::ostringstream summaryStream;
          if (std::abs(value) < 1e-6 && value != 0.0) {
            summaryStream << std::scientific << std::setprecision(3) << value;
          }
          else {
            summaryStream << std::fixed << std::setprecision(9) << value;
          }
          ops.LogInfo("Reading #" + std::to_string(readingCount) + " - " +
            m_dataId + ": " + summaryStream.str());
        }

      }
      catch (const std::exception& e) {
        failedReads++;
        ops.LogError("Failed to read " + m_dataId + ": " + std::string(e.what()));
      }

      // Sleep for the interval duration
      std::this_thread::sleep_for(std::chrono::milliseconds(m_intervalMs));
    }

    // 4. Calculate statistics
    auto actualEndTime = std::chrono::steady_clock::now();
    auto totalElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
      actualEndTime - startTime).count();

    // Calculate reading statistics if we have valid data
    double minValue = std::numeric_limits<double>::quiet_NaN();
    double maxValue = std::numeric_limits<double>::quiet_NaN();
    double avgValue = std::numeric_limits<double>::quiet_NaN();
    double valueRange = std::numeric_limits<double>::quiet_NaN();
    double stdDeviation = std::numeric_limits<double>::quiet_NaN();
    double actualIntervalMs = std::numeric_limits<double>::quiet_NaN();

    if (!readings.empty()) {
      // Min/Max/Average
      minValue = *std::min_element(readings.begin(), readings.end());
      maxValue = *std::max_element(readings.begin(), readings.end());

      double sum = std::accumulate(readings.begin(), readings.end(), 0.0);
      avgValue = sum / readings.size();
      valueRange = maxValue - minValue;

      // Standard deviation (using double precision for nano-scale values)
      double variance = 0.0;
      for (double reading : readings) {
        variance += std::pow(reading - avgValue, 2);
      }
      variance /= readings.size();
      stdDeviation = std::sqrt(variance);

      // Calculate actual average interval between readings
      if (readingTimes.size() > 1) {
        auto totalReadingTime = std::chrono::duration_cast<std::chrono::milliseconds>(
          readingTimes.back() - readingTimes.front()).count();
        actualIntervalMs = static_cast<double>(totalReadingTime) / (readingTimes.size() - 1);
      }
    }

    // 5. Store comprehensive results
    if (ops.GetResultsManager() && !opId.empty()) {
      // Fix: Explicitly specify the type of `resultsManager`  
      std::shared_ptr<OperationResultsManager> resultsManager = ops.GetResultsManager();
      // Basic operation info (using scientific notation for very small values)
      auto formatValue = [](double val) -> std::string {
        if (std::isnan(val)) return "NaN";
        if (std::abs(val) < 1e-6) {  // For nano-scale values
          std::ostringstream ss;
          ss << std::scientific << std::setprecision(6) << val;
          return ss.str();
        }
        else {
          return std::to_string(val);
        }
      };

      resultsManager->StoreResult(opId, "start_value", formatValue(startValue));
      resultsManager->StoreResult(opId, "end_value", formatValue(endValue));
      resultsManager->StoreResult(opId, "successful_reads", std::to_string(successfulReads));
      resultsManager->StoreResult(opId, "failed_reads", std::to_string(failedReads));
      resultsManager->StoreResult(opId, "total_elapsed_ms", std::to_string(totalElapsedMs));

      // Statistical data
      resultsManager->StoreResult(opId, "min_value", formatValue(minValue));
      resultsManager->StoreResult(opId, "max_value", formatValue(maxValue));
      resultsManager->StoreResult(opId, "avg_value", formatValue(avgValue));
      resultsManager->StoreResult(opId, "value_range", formatValue(valueRange));
      resultsManager->StoreResult(opId, "std_deviation", formatValue(stdDeviation));
      resultsManager->StoreResult(opId, "actual_interval_ms", std::isnan(actualIntervalMs) ? "NaN" : std::to_string(actualIntervalMs));

      // Threshold monitoring results
      if (m_enableThresholdAlerts) {
        resultsManager->StoreResult(opId, "threshold_violations", std::to_string(thresholdViolations));
        resultsManager->StoreResult(opId, "violation_rate",
          successfulReads > 0 ? std::to_string((double)thresholdViolations / successfulReads * 100.0) + "%" : "N/A");
      }

      // Determine success/failure
      bool operationSuccess = hasValidReading && (failedReads == 0 || successfulReads > failedReads);

      if (operationSuccess) {
        resultsManager->EndOperation(opId, "success");
      }
      else {
        std::string errorMsg = "Monitoring failed - ";
        if (!hasValidReading) {
          errorMsg += "no successful readings";
        }
        else {
          errorMsg += "too many failed reads (" + std::to_string(failedReads) +
            " failed, " + std::to_string(successfulReads) + " succeeded)";
        }
        resultsManager->EndOperation(opId, "failed", errorMsg);
      }
    }

    // 6. Final logging with scientific notation for nano-scale values
    if (hasValidReading) {
      auto formatForLog = [](double val) -> std::string {
        if (std::isnan(val)) return "N/A";
        if (std::abs(val) < 1e-6) {  // For nano-scale values
          std::ostringstream ss;
          ss << std::scientific << std::setprecision(3) << val;
          return ss.str();
        }
        else {
          return std::to_string(val);
        }
      };

      ops.LogInfo("Completed periodic monitoring of " + m_dataId +
        " - Readings: " + std::to_string(successfulReads) +
        " successful, " + std::to_string(failedReads) + " failed" +
        " | Range: " + formatForLog(minValue) +
        " to " + formatForLog(maxValue) +
        " | Avg: " + formatForLog(avgValue));
    }
    else {
      ops.LogError("Periodic monitoring of " + m_dataId + " failed - no successful readings");
    }

    return hasValidReading && (failedReads == 0 || successfulReads > failedReads);
  }

  /// <summary>
  /// Gets a human-readable description of this operation.
  /// </summary>
  /// <returns>A string describing the monitoring operation.</returns>
  std::string GetDescription() const override {
    std::string desc = "Monitor " + m_dataId + " for " +
      std::to_string(m_durationMs / 1000) + " seconds";

    if (m_enableIntermediateLogging) {
      desc += " (log every " + std::to_string(m_logEveryN) + ")";
    }

    if (m_enableThresholdAlerts) {
      desc += " (alerts: " + std::to_string(m_minThreshold) +
        " to " + std::to_string(m_maxThreshold) + ")";
    }

    return desc;
  }

private:
  std::string m_dataId;           ///< The identifier of the data channel to monitor
  int m_durationMs;               ///< The total duration to monitor in milliseconds
  int m_intervalMs;               ///< The interval between readings in milliseconds
  bool m_enableIntermediateLogging; ///< Whether to log every N readings
  int m_logEveryN;                ///< Log every N readings when intermediate logging is enabled
  bool m_enableThresholdAlerts;   ///< Whether to check threshold violations
  double m_minThreshold;          ///< Minimum threshold value (double precision for nano-scale)
  double m_maxThreshold;          ///< Maximum threshold value (double precision for nano-scale)
};




/// <summary>
/// Operation that reads a single data value from the GlobalDataStore and logs it.
/// Useful for capturing instantaneous readings at specific points in a sequence.
/// </summary>
class ReadAndLogDataValueOperation : public SequenceOperation {
public:
  /// <summary>
  /// Creates a new operation to read and log a data value.
  /// </summary>
  /// <param name="dataId">The identifier of the data channel to read.</param>
  /// <param name="description">Optional custom description for the log message.</param>
  ReadAndLogDataValueOperation(const std::string& dataId, const std::string& description = "")
    : m_dataId(dataId), m_description(description) {
  }

  /// <summary>
  /// Executes the operation, reading and logging the specified data value.
  /// </summary>
  /// <param name="ops">Reference to MachineOperations for accessing the data store.</param>
  /// <returns>True if the read and log succeeded, false otherwise.</returns>
  bool Execute(MachineOperations& ops) override {
    float value = ops.ReadDataValue(m_dataId);
    std::string message = m_description.empty() ?
      m_dataId + " value: " + std::to_string(value) :
      m_description + ": " + std::to_string(value);
    ops.LogInfo(message);
    return true;
  }

  /// <summary>
  /// Gets a human-readable description of this operation.
  /// </summary>
  /// <returns>A string describing the read and log operation.</returns>
  std::string GetDescription() const override {
    return "Read and log " + m_dataId;
  }

private:
  std::string m_dataId;      ///< The identifier of the data channel to read
  std::string m_description; ///< Optional custom description for the log message
};

/// <summary>
/// Operation that reads the current laser power setting and logs it.
/// </summary>
class ReadAndLogLaserCurrentOperation : public SequenceOperation {
public:
  /// <summary>
  /// Creates a new operation to read and log the laser current.
  /// </summary>
  /// <param name="laserName">Optional name of the laser device if multiple lasers are present.</param>
  /// <param name="description">Optional custom description for the log message.</param>
  ReadAndLogLaserCurrentOperation(const std::string& laserName = "", const std::string& description = "")
    : m_laserName(laserName), m_description(description) {
  }

  /// <summary>
  /// Executes the operation, reading and logging the current laser power setting.
  /// </summary>
  /// <param name="ops">Reference to MachineOperations for accessing the laser controller.</param>
  /// <returns>True if the read and log succeeded, false otherwise.</returns>
  bool Execute(MachineOperations& ops) override {
    float current = ops.GetLaserCurrent(m_laserName);
    std::string message = m_description.empty() ?
      "Laser current" + (m_laserName.empty() ? "" : " for " + m_laserName) + ": " + std::to_string(current) + "A" :
      m_description + ": " + std::to_string(current) + "A";
    ops.LogInfo(message);
    return true;
  }

  /// <summary>
  /// Gets a human-readable description of this operation.
  /// </summary>
  /// <returns>A string describing the read and log operation.</returns>
  std::string GetDescription() const override {
    return "Read and log laser current" + (m_laserName.empty() ? "" : " for " + m_laserName);
  }

private:
  std::string m_laserName;   ///< Optional name of the laser device
  std::string m_description; ///< Optional custom description for the log message
};

/// <summary>
/// Operation that reads the current laser temperature and logs it.
/// </summary>
class ReadAndLogLaserTemperatureOperation : public SequenceOperation {
public:
  /// <summary>
  /// Creates a new operation to read and log the laser temperature.
  /// </summary>
  /// <param name="laserName">Optional name of the laser device if multiple lasers are present.</param>
  /// <param name="description">Optional custom description for the log message.</param>
  ReadAndLogLaserTemperatureOperation(const std::string& laserName = "", const std::string& description = "")
    : m_laserName(laserName), m_description(description) {
  }

  /// <summary>
  /// Executes the operation, reading and logging the current laser temperature.
  /// </summary>
  /// <param name="ops">Reference to MachineOperations for accessing the laser controller.</param>
  /// <returns>True if the read and log succeeded, false otherwise.</returns>
  bool Execute(MachineOperations& ops) override {
    float temperature = ops.GetLaserTemperature(m_laserName);
    std::string message = m_description.empty() ?
      "Laser temperature" + (m_laserName.empty() ? "" : " for " + m_laserName) + ": " + std::to_string(temperature) + "°C" :
      m_description + ": " + std::to_string(temperature) + "°C";
    ops.LogInfo(message);
    return true;
  }

  /// <summary>
  /// Gets a human-readable description of this operation.
  /// </summary>
  /// <returns>A string describing the read and log operation.</returns>
  std::string GetDescription() const override {
    return "Read and log laser temperature" + (m_laserName.empty() ? "" : " for " + m_laserName);
  }

private:
  std::string m_laserName;   ///< Optional name of the laser device
  std::string m_description; ///< Optional custom description for the log message
};


// ADD these new operation classes to SequenceStep.h:

// Apply Camera Exposure for Node operation
class ApplyCameraExposureForNodeOperation : public SequenceOperation {
public:
  ApplyCameraExposureForNodeOperation(const std::string& nodeId)
    : m_nodeId(nodeId) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.ApplyCameraExposureForNode(m_nodeId);
  }

  std::string GetDescription() const override {
    return "Apply camera exposure settings for node " + m_nodeId;
  }

private:
  std::string m_nodeId;
};

// Apply Default Camera Exposure operation
class ApplyDefaultCameraExposureOperation : public SequenceOperation {
public:
  ApplyDefaultCameraExposureOperation() {}

  bool Execute(MachineOperations& ops) override {
    return ops.ApplyDefaultCameraExposure();
  }

  std::string GetDescription() const override {
    return "Apply default camera exposure settings";
  }
};

// Enable/Disable Auto Camera Exposure operation
class SetAutoExposureOperation : public SequenceOperation {
public:
  SetAutoExposureOperation(bool enabled)
    : m_enabled(enabled) {
  }

  bool Execute(MachineOperations& ops) override {
    ops.SetAutoExposureEnabled(m_enabled);
    return true;
  }

  std::string GetDescription() const override {
    return std::string("Set automatic camera exposure ") + (m_enabled ? "ON" : "OFF");
  }

private:
  bool m_enabled;
};


// SequenceStep.h - Add these new operation classes for needle calibration

#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>
#include <iomanip>

// Calculate Needle Offset operation
class CalculateNeedleOffsetOperation : public SequenceOperation {
public:
  CalculateNeedleOffsetOperation(const std::string& deviceName,
    const std::string& pos1Label,
    const std::string& pos2Label)
    : m_deviceName(deviceName), m_pos1Label(pos1Label), m_pos2Label(pos2Label) {
  }

  bool Execute(MachineOperations& ops) override {
    // Get stored positions
    PositionStruct pos1, pos2;
    if (!ops.GetStoredPosition(m_pos1Label, pos1)) {
      ops.LogError("Failed to get stored position: " + m_pos1Label);
      return false;
    }

    if (!ops.GetStoredPosition(m_pos2Label, pos2)) {
      ops.LogError("Failed to get stored position: " + m_pos2Label);
      return false;
    }

    // Calculate needle offset = pos2 - pos1
    double offsetX = pos2.x - pos1.x;
    double offsetY = pos2.y - pos1.y;
    double offsetZ = 0.0; // Always 0 for needle calibration as requested

    // Display the calculated offset
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "\n=== NEEDLE OFFSET CALCULATION ===" << std::endl;
    std::cout << "Position 1 (before dot): X=" << pos1.x << ", Y=" << pos1.y << ", Z=" << pos1.z << std::endl;
    std::cout << "Position 2 (after adjustment): X=" << pos2.x << ", Y=" << pos2.y << ", Z=" << pos2.z << std::endl;
    std::cout << "Calculated Needle Offset:" << std::endl;
    std::cout << "  X offset: " << offsetX << " mm" << std::endl;
    std::cout << "  Y offset: " << offsetY << " mm" << std::endl;
    std::cout << "  Z offset: " << offsetZ << " mm (fixed)" << std::endl;
    std::cout << "=================================" << std::endl;

    // Log the calculation
    ops.LogInfo("Needle offset calculated: X=" + std::to_string(offsetX) +
      ", Y=" + std::to_string(offsetY) + ", Z=" + std::to_string(offsetZ));

    return true;
  }

  std::string GetDescription() const override {
    return "Calculate needle offset from positions " + m_pos1Label + " and " + m_pos2Label;
  }

private:
  std::string m_deviceName;
  std::string m_pos1Label;
  std::string m_pos2Label;
};

// Save Needle Offset operation
class SaveNeedleOffsetOperation : public SequenceOperation {
public:
  SaveNeedleOffsetOperation(const std::string& deviceName,
    const std::string& pos1Label,
    const std::string& pos2Label)
    : m_deviceName(deviceName), m_pos1Label(pos1Label), m_pos2Label(pos2Label) {
  }

  bool Execute(MachineOperations& ops) override {
    try {
      // Get stored positions
      PositionStruct pos1, pos2;
      if (!ops.GetStoredPosition(m_pos1Label, pos1)) {
        ops.LogError("Failed to get stored position: " + m_pos1Label);
        return false;
      }

      if (!ops.GetStoredPosition(m_pos2Label, pos2)) {
        ops.LogError("Failed to get stored position: " + m_pos2Label);
        return false;
      }

      // Calculate needle offset = pos2 - pos1
      double offsetX = pos2.x - pos1.x;
      double offsetY = pos2.y - pos1.y;
      double offsetZ = 0.0; // Always 0 for needle calibration

      // Load existing camera_to_object_offset.json
      std::string configPath = "camera_to_object_offset.json";
      nlohmann::json config;

      std::ifstream configFile(configPath);
      if (configFile.is_open()) {
        configFile >> config;
        configFile.close();
        ops.LogInfo("Loaded existing camera offset configuration");
      }
      else {
        ops.LogWarning("Could not load existing config, creating new one");
        // Create basic structure if file doesn't exist
        config = {
            {"camera_center", {
                {"description", "Reference point (0,0,0) for all offset measurements"},
                {"coordinates", {{"x", 0}, {"y", 0}, {"z", 0}}}
            }},
            {"hardware_offsets", nlohmann::json::object()},
            {"calibration_info", {
                {"coordinate_system", "right_handed"},
                {"origin", "camera_optical_center"},
                {"x_axis", "horizontal_left"},
                {"y_axis", "horizontal_toward"},
                {"z_axis", "vertical_up"},
                {"precision", "±0.1mm"},
                {"calibration_method", "automatic_needle_calibration"}
            }}
        };
      }

      // Get current timestamp
      auto now = std::chrono::system_clock::now();
      auto time_t = std::chrono::system_clock::to_time_t(now);
      std::tm tm;
      gmtime_s(&tm, &time_t);

      char timestamp[32];
      std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &tm);

      // Update needle offset in config
      config["hardware_offsets"]["needle"] = {
          {"description", "Offset from camera center to needle tip"},
          {"coordinates", {
              {"x", offsetX},
              {"y", offsetY},
              {"z", offsetZ}
          }},
          {"units", "mm"},
          {"last_calibrated", std::string(timestamp)},
          {"calibration_method", "automatic_dispensing_calibration"}
      };

      // Save updated config
      std::ofstream outFile(configPath);
      if (outFile.is_open()) {
        outFile << config.dump(2); // Pretty print with 2-space indentation
        outFile.close();

        ops.LogInfo("Successfully saved needle offset to " + configPath);
        ops.LogInfo("Needle offset: X=" + std::to_string(offsetX) +
          ", Y=" + std::to_string(offsetY) + ", Z=" + std::to_string(offsetZ));

        std::cout << "\n=== CONFIGURATION SAVED ===" << std::endl;
        std::cout << "Needle offset saved to: " << configPath << std::endl;
        std::cout << "X offset: " << std::fixed << std::setprecision(6) << offsetX << " mm" << std::endl;
        std::cout << "Y offset: " << offsetY << " mm" << std::endl;
        std::cout << "Z offset: " << offsetZ << " mm" << std::endl;
        std::cout << "Calibration timestamp: " << timestamp << std::endl;
        std::cout << "============================" << std::endl;

        return true;
      }
      else {
        ops.LogError("Failed to open config file for writing: " + configPath);
        return false;
      }

    }
    catch (const std::exception& e) {
      ops.LogError("Exception while saving needle offset: " + std::string(e.what()));
      return false;
    }
  }

  std::string GetDescription() const override {
    return "Save calculated needle offset to camera_to_object_offset.json";
  }

private:
  std::string m_deviceName;
  std::string m_pos1Label;
  std::string m_pos2Label;
};

// Load Camera Offset Config operation (utility for reading current values)
class LoadCameraOffsetConfigOperation : public SequenceOperation {
public:
  LoadCameraOffsetConfigOperation() {}

  bool Execute(MachineOperations& ops) override {
    try {
      std::string configPath = "camera_to_object_offset.json";
      std::ifstream configFile(configPath);

      if (!configFile.is_open()) {
        ops.LogWarning("Camera offset config file not found: " + configPath);
        return false;
      }

      nlohmann::json config;
      configFile >> config;
      configFile.close();

      ops.LogInfo("Successfully loaded camera offset configuration");

      // Display current needle offset if it exists
      if (config.contains("hardware_offsets") &&
        config["hardware_offsets"].contains("needle")) {

        auto needle = config["hardware_offsets"]["needle"];
        auto coords = needle["coordinates"];

        std::cout << "\n=== CURRENT NEEDLE OFFSET ===" << std::endl;
        std::cout << "X offset: " << coords["x"] << " mm" << std::endl;
        std::cout << "Y offset: " << coords["y"] << " mm" << std::endl;
        std::cout << "Z offset: " << coords["z"] << " mm" << std::endl;

        if (needle.contains("last_calibrated")) {
          std::cout << "Last calibrated: " << needle["last_calibrated"] << std::endl;
        }
        std::cout << "==============================" << std::endl;
      }
      else {
        std::cout << "\n=== NO EXISTING NEEDLE OFFSET FOUND ===" << std::endl;
      }

      return true;

    }
    catch (const std::exception& e) {
      ops.LogError("Exception while loading camera offset config: " + std::string(e.what()));
      return false;
    }
  }

  std::string GetDescription() const override {
    return "Load and display current camera offset configuration";
  }
};



// SequenceStep.h - Add these missing operation classes

// Clear Stored Positions operation
class ClearStoredPositionsOperation : public SequenceOperation {
public:
  ClearStoredPositionsOperation(const std::string& deviceNameFilter = "")
    : m_deviceNameFilter(deviceNameFilter) {
  }

  bool Execute(MachineOperations& ops) override {
    ops.ClearStoredPositions(m_deviceNameFilter);
    return true;
  }

  std::string GetDescription() const override {
    return m_deviceNameFilter.empty() ?
      "Clear all stored positions" :
      "Clear stored positions for device '" + m_deviceNameFilter + "'";
  }

private:
  std::string m_deviceNameFilter;
};

// Display Needle Offset operation
class DisplayNeedleOffsetOperation : public SequenceOperation {
public:
  DisplayNeedleOffsetOperation() {}

  bool Execute(MachineOperations& ops) override {
    try {
      std::string configPath = "camera_to_object_offset.json";
      std::ifstream configFile(configPath);

      if (!configFile.is_open()) {
        ops.LogWarning("Camera offset config file not found: " + configPath);
        std::cout << "\n=== NO EXISTING NEEDLE OFFSET FOUND ===" << std::endl;
        std::cout << "This will be the first needle calibration." << std::endl;
        std::cout << "========================================" << std::endl;
        return true;
      }

      nlohmann::json config;
      configFile >> config;
      configFile.close();

      ops.LogInfo("Successfully loaded camera offset configuration");

      // Display current needle offset if it exists
      if (config.contains("hardware_offsets") &&
        config["hardware_offsets"].contains("needle")) {

        auto needle = config["hardware_offsets"]["needle"];
        auto coords = needle["coordinates"];

        std::cout << "\n=== CURRENT NEEDLE OFFSET ===" << std::endl;
        std::cout << "X offset: " << std::fixed << std::setprecision(6) << coords["x"] << " mm" << std::endl;
        std::cout << "Y offset: " << coords["y"] << " mm" << std::endl;
        std::cout << "Z offset: " << coords["z"] << " mm" << std::endl;

        if (needle.contains("last_calibrated")) {
          std::cout << "Last calibrated: " << needle["last_calibrated"] << std::endl;
        }
        if (needle.contains("calibration_method")) {
          std::cout << "Method: " << needle["calibration_method"] << std::endl;
        }
        std::cout << "==============================" << std::endl;

        ops.LogInfo("Current needle offset: X=" + std::to_string((double)coords["x"]) +
          ", Y=" + std::to_string((double)coords["y"]) +
          ", Z=" + std::to_string((double)coords["z"]));
      }
      else {
        std::cout << "\n=== NO EXISTING NEEDLE OFFSET FOUND ===" << std::endl;
        std::cout << "This will be the first needle calibration." << std::endl;
        std::cout << "========================================" << std::endl;
        ops.LogInfo("No existing needle offset found in configuration");
      }

      return true;

    }
    catch (const std::exception& e) {
      ops.LogError("Exception while loading camera offset config: " + std::string(e.what()));
      std::cout << "\n=== ERROR READING CONFIG ===" << std::endl;
      std::cout << "Could not read existing configuration." << std::endl;
      std::cout << "Will proceed with new calibration." << std::endl;
      std::cout << "=============================" << std::endl;
      return true; // Continue with calibration even if we can't read existing config
    }
  }

  std::string GetDescription() const override {
    return "Display current needle offset from camera configuration";
  }
};

// Log Position Distance operation  
class LogPositionDistanceOperation : public SequenceOperation {
public:
  LogPositionDistanceOperation(const std::string& deviceName, const std::string& storedLabel,
    const std::string& description = "")
    : m_deviceName(deviceName), m_storedLabel(storedLabel), m_description(description) {
  }

  bool Execute(MachineOperations& ops) override {
    double distance = ops.CalculateDistanceFromStored(m_deviceName, m_storedLabel);

    if (distance < 0) {
      ops.LogWarning("Could not calculate distance from stored position '" + m_storedLabel + "'");
      return true; // Non-critical operation, continue sequence
    }

    std::string message = m_description.empty() ?
      "Distance from stored position '" + m_storedLabel + "': " + std::to_string(distance) + " mm" :
      m_description + ": " + std::to_string(distance) + " mm";

    ops.LogInfo(message);

    // Also display to console for user visibility
    std::cout << ">>> " << message << std::endl;

    return true;
  }

  std::string GetDescription() const override {
    return "Log distance from " + m_deviceName + " to stored position '" + m_storedLabel + "'";
  }

private:
  std::string m_deviceName;
  std::string m_storedLabel;
  std::string m_description;
};

// SequenceStep.h - Add this missing operation class

// Capture Current Position operation
class CapturePositionOperation : public SequenceOperation {
public:
  CapturePositionOperation(const std::string& deviceName, const std::string& label)
    : m_deviceName(deviceName), m_label(label) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.CaptureCurrentPosition(m_deviceName, m_label);
  }

  std::string GetDescription() const override {
    return "Capture current position of " + m_deviceName + " as '" + m_label + "'";
  }

private:
  std::string m_deviceName;
  std::string m_label;
};



// Problem: ImGui::OpenPopup() is being called from background thread
// Solution: Use a flag-based approach instead of direct ImGui calls

/// <summary>
/// Enhanced operation that displays a user prompt with comprehensive tracking and statistics.
/// Tracks user response time, timeout handling, and operation success metrics.
/// </summary>
class UserPromptOperation : public SequenceOperation {
public:
  /// <summary>
  /// Creates a new user prompt operation with tracking.
  /// </summary>
  /// <param name="title">The title of the prompt dialog.</param>
  /// <param name="message">The message to display to the user.</param>
  /// <param name="promptUI">Reference to the UserPromptUI for displaying the prompt.</param>
  /// <param name="timeoutSeconds">Timeout in seconds (default: 3600 = 1 hour).</param>
  /// <param name="enableResponseTimeTracking">Whether to track detailed response timing (default: true).</param>
  UserPromptOperation(const std::string& title, const std::string& message, UserPromptUI& promptUI,
    int timeoutSeconds = 3600, bool enableResponseTimeTracking = true)
    : m_title(title), m_message(message), m_promptUI(promptUI),
    m_timeoutSeconds(timeoutSeconds), m_enableResponseTimeTracking(enableResponseTimeTracking),
    m_completed(false), m_result(PromptResult::PENDING) {
  }

  /// <summary>
  /// Simple constructor for basic prompts (1 hour timeout, response tracking enabled).
  /// </summary>
  static std::unique_ptr<UserPromptOperation> CreateBasic(
    const std::string& title, const std::string& message, UserPromptUI& promptUI) {
    return std::make_unique<UserPromptOperation>(title, message, promptUI);
  }

  /// <summary>
  /// Constructor for prompts with custom timeout.
  /// </summary>
  static std::unique_ptr<UserPromptOperation> CreateWithTimeout(
    const std::string& title, const std::string& message, UserPromptUI& promptUI,
    int timeoutSeconds) {
    return std::make_unique<UserPromptOperation>(title, message, promptUI, timeoutSeconds);
  }

  /// <summary>
  /// Constructor for prompts with minimal tracking (no response time tracking).
  /// </summary>
  static std::unique_ptr<UserPromptOperation> CreateMinimalTracking(
    const std::string& title, const std::string& message, UserPromptUI& promptUI,
    int timeoutSeconds = 3600) {
    return std::make_unique<UserPromptOperation>(title, message, promptUI, timeoutSeconds, false);
  }

  /// <summary>
  /// Executes the user prompt with comprehensive tracking.
  /// </summary>
  /// <param name="ops">Reference to MachineOperations for logging and results tracking.</param>
  /// <returns>True if user confirmed (YES), false for NO/CANCEL/TIMEOUT.</returns>
  bool Execute(MachineOperations& ops) override {
    // 1. Start operation tracking
    std::string opId;
    auto startTime = std::chrono::steady_clock::now();

    std::shared_ptr<OperationResultsManager> resultsManager = ops.GetResultsManager();
    if (resultsManager) {
      std::map<std::string, std::string> parameters = {
        {"title", m_title},
        {"message_length", std::to_string(m_message.length())},
        {"timeout_seconds", std::to_string(m_timeoutSeconds)},
        {"enableResponseTimeTracking", m_enableResponseTimeTracking ? "true" : "false"}
      };
      opId = resultsManager->StartOperation("UserPrompt", m_title,
        "UserPromptOperation", "", parameters);
    }

    ops.LogInfo("Requesting user prompt: " + m_title);

    // 2. Initialize tracking variables
    auto promptDisplayTime = std::chrono::steady_clock::now();
    auto userResponseTime = std::chrono::steady_clock::time_point{};
    bool timedOut = false;
    bool userResponded = false;
    int pollCount = 0;

    // Reset completion state
    m_completed = false;
    m_result = PromptResult::PENDING;

    // 3. Display prompt using RequestPrompt (thread-safe)
    m_promptUI.RequestPrompt(m_title, m_message, [this, &userResponseTime, &userResponded](PromptResult result) {
      userResponseTime = std::chrono::steady_clock::now();
      userResponded = true;
      m_result = result;
      m_completed = true;
    });

    // 4. Wait for user response with timeout and tracking
    int timeoutMs = m_timeoutSeconds * 1000;
    int waitedMs = 0;
    const int pollIntervalMs = 100;

    while (!m_completed && waitedMs < timeoutMs) {
      std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
      waitedMs += pollIntervalMs;
      pollCount++;

      // Optional: Log periodic status for very long prompts
      if (m_enableResponseTimeTracking && m_timeoutSeconds >= 300 && // 5+ minute timeout
        waitedMs % 30000 == 0) { // Every 30 seconds
        ops.LogInfo("Still waiting for user response to: " + m_title +
          " (" + std::to_string(waitedMs / 1000) + "s elapsed)");
      }
    }

    // 5. Handle timeout
    if (!m_completed) {
      timedOut = true;
      ops.LogWarning("User prompt timed out after " + std::to_string(m_timeoutSeconds) + " seconds");
    }

    // 6. Calculate timing statistics
    auto endTime = std::chrono::steady_clock::now();
    auto totalElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
      endTime - startTime).count();

    float responseTimeMs = std::numeric_limits<float>::quiet_NaN();
    float promptToResponseMs = std::numeric_limits<float>::quiet_NaN();

    if (userResponded && m_enableResponseTimeTracking) {
      responseTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        userResponseTime - startTime).count();
      promptToResponseMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        userResponseTime - promptDisplayTime).count();
    }

    // 7. Determine operation success and result
    bool operationSuccess = false;
    std::string resultString = "unknown";
    std::string userAction = "none";

    if (timedOut) {
      resultString = "timeout";
      userAction = "timeout";
    }
    else {
      switch (m_result) {
      case PromptResult::YES:
        resultString = "yes";
        userAction = "confirmed";
        operationSuccess = true;
        ops.LogInfo("User confirmed YES - continuing execution");
        break;
      case PromptResult::NO:
        resultString = "no";
        userAction = "declined";
        ops.LogWarning("User selected NO - stopping execution");
        break;
      case PromptResult::CANCELLED:
        resultString = "cancelled";
        userAction = "cancelled";
        ops.LogWarning("User cancelled prompt - stopping execution");
        break;
      default:
        resultString = "unknown";
        userAction = "unknown";
        ops.LogError("Unknown prompt result - stopping execution");
        break;
      }
    }

    // 8. Store comprehensive results
    if (resultsManager && !opId.empty()) {
      // Basic operation info
      resultsManager->StoreResult(opId, "user_response", resultString);
      resultsManager->StoreResult(opId, "user_action", userAction);
      resultsManager->StoreResult(opId, "total_elapsed_ms", std::to_string(totalElapsedMs));
      resultsManager->StoreResult(opId, "timed_out", timedOut ? "true" : "false");
      resultsManager->StoreResult(opId, "user_responded", userResponded ? "true" : "false");

      // Timing statistics (if enabled and available)
      if (m_enableResponseTimeTracking) {
        resultsManager->StoreResult(opId, "response_time_ms",
          std::isnan(responseTimeMs) ? "NaN" : std::to_string(responseTimeMs));
        resultsManager->StoreResult(opId, "prompt_to_response_ms",
          std::isnan(promptToResponseMs) ? "NaN" : std::to_string(promptToResponseMs));
        resultsManager->StoreResult(opId, "poll_count", std::to_string(pollCount));

        // Response speed categorization
        if (!std::isnan(responseTimeMs)) {
          std::string responseSpeed;
          if (responseTimeMs < 5000) responseSpeed = "fast";           // < 5 seconds
          else if (responseTimeMs < 30000) responseSpeed = "normal";   // 5-30 seconds
          else if (responseTimeMs < 300000) responseSpeed = "slow";    // 30s-5min
          else responseSpeed = "very_slow";                            // > 5 minutes

          resultsManager->StoreResult(opId, "response_speed", responseSpeed);
        }
      }

      // Timeout utilization (how much of the timeout was used)
      if (m_timeoutSeconds > 0) {
        float timeoutUtilization = (float)totalElapsedMs / (m_timeoutSeconds * 1000) * 100.0f;
        resultsManager->StoreResult(opId, "timeout_utilization_percent",
          std::to_string(timeoutUtilization));
      }

      // End operation with appropriate status
      if (timedOut) {
        resultsManager->EndOperation(opId, "timeout",
          "User prompt timed out after " + std::to_string(m_timeoutSeconds) + " seconds");
      }
      else if (operationSuccess) {
        resultsManager->EndOperation(opId, "success");
      }
      else {
        resultsManager->EndOperation(opId, "user_declined",
          "User " + userAction + " the prompt");
      }
    }

    // 9. Final logging with summary
    if (m_enableResponseTimeTracking && userResponded) {
      ops.LogInfo("User prompt completed: " + m_title +
        " - Response: " + resultString +
        " | Time: " + (std::isnan(responseTimeMs) ? "N/A" : std::to_string((int)responseTimeMs)) + "ms");
    }
    else {
      ops.LogInfo("User prompt completed: " + m_title + " - Response: " + resultString);
    }

    return operationSuccess;
  }

  /// <summary>
  /// Gets a human-readable description of this operation.
  /// </summary>
  /// <returns>A string describing the user prompt operation.</returns>
  std::string GetDescription() const override {
    std::string desc = "User prompt: " + m_title;

    if (m_timeoutSeconds != 3600) { // Non-default timeout
      if (m_timeoutSeconds >= 60) {
        desc += " (timeout: " + std::to_string(m_timeoutSeconds / 60) + "m)";
      }
      else {
        desc += " (timeout: " + std::to_string(m_timeoutSeconds) + "s)";
      }
    }

    if (!m_enableResponseTimeTracking) {
      desc += " (minimal tracking)";
    }

    return desc;
  }

private:
  std::string m_title;              ///< The title of the prompt dialog
  std::string m_message;            ///< The message to display to the user
  UserPromptUI& m_promptUI;         ///< Reference to the UserPromptUI
  int m_timeoutSeconds;             ///< Timeout in seconds
  bool m_enableResponseTimeTracking; ///< Whether to track detailed response timing

  // Runtime state (thread-safe)
  std::atomic<bool> m_completed;    ///< Whether the prompt has been completed
  std::atomic<PromptResult> m_result; ///< The user's response result
};


// Add this to SequenceStep.h or create a new file:
class MockUserPromptOperation : public SequenceOperation {
public:
  MockUserPromptOperation(const std::string& title, const std::string& message)
    : m_title(title), m_message(message) {
  }

  bool Execute(MachineOperations& ops) override {
    ops.LogInfo("MOCK PROMPT: " + m_title);
    ops.LogInfo("Message: " + m_message);
    ops.LogInfo("Auto-answering YES for testing (implement real UserPromptUI for interactive prompts)");

    // For now, always return true (YES) so execution continues
    // TODO: Replace with real UserPromptUI implementation
    return true;
  }

  std::string GetDescription() const override {
    return "Mock user prompt: " + m_title;
  }

private:
  std::string m_title;
  std::string m_message;
};


class MoveToPositionOperation : public SequenceOperation {
public:
  MoveToPositionOperation(const std::string& controllerName, const std::string& positionName, bool blocking = true)
    : m_controllerName(controllerName), m_positionName(positionName), m_blocking(blocking) {
  }

  bool Execute(MachineOperations& ops) override {
    // Validate position name exists
    if (m_positionName.empty()) {
      return false;
    }

    return ops.MoveToPointName(m_controllerName, m_positionName, m_blocking);
  }

  std::string GetDescription() const override {
    return "Move " + m_controllerName + " to position '" + m_positionName + "'";
  }

private:
  std::string m_controllerName;
  std::string m_positionName;
  bool m_blocking;
};

class MoveRelativeAxisOperation : public SequenceOperation {
public:
  MoveRelativeAxisOperation(const std::string& controllerName, const std::string& axisName,
    double distanceMm, bool blocking = true)
    : m_controllerName(controllerName), m_axisName(axisName),
    m_distanceMm(distanceMm), m_blocking(blocking) {
  }

  bool Execute(MachineOperations& ops) override {
    // Validate axis name
    if (m_axisName.empty()) {
      return false;
    }

    return ops.MoveRelative(m_controllerName, m_axisName, m_distanceMm, m_blocking);
  }

  std::string GetDescription() const override {
    return "Move " + m_controllerName + " relative on " + m_axisName +
      " axis by " + std::to_string(m_distanceMm) + "mm";
  }

private:
  std::string m_controllerName;
  std::string m_axisName;
  double m_distanceMm;
  bool m_blocking;
};

// Add these Keithley 2400 operation classes to SequenceStep.h:

// Reset Keithley instrument operation
class ResetKeithleyOperation : public SequenceOperation {
public:
  ResetKeithleyOperation(const std::string& clientName = "")
    : m_clientName(clientName) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.SMU_ResetInstrument(m_clientName);
  }

  std::string GetDescription() const override {
    return "Reset Keithley instrument" + (m_clientName.empty() ? "" : " (" + m_clientName + ")");
  }

private:
  std::string m_clientName;
};

// Set Keithley output operation
class SetKeithleyOutputOperation : public SequenceOperation {
public:
  SetKeithleyOutputOperation(bool enable, const std::string& clientName = "")
    : m_enable(enable), m_clientName(clientName) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.SMU_SetOutput(m_enable, m_clientName);
  }

  std::string GetDescription() const override {
    return std::string(m_enable ? "Enable" : "Disable") + " Keithley output" +
      (m_clientName.empty() ? "" : " (" + m_clientName + ")");
  }

private:
  bool m_enable;
  std::string m_clientName;
};

// Setup Keithley voltage source operation
class SetupKeithleyVoltageSourceOperation : public SequenceOperation {
public:
  SetupKeithleyVoltageSourceOperation(double voltage, double compliance = 0.1,
    const std::string& range = "AUTO",
    const std::string& clientName = "")
    : m_voltage(voltage), m_compliance(compliance), m_range(range), m_clientName(clientName) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.SMU_SetupVoltageSource(m_voltage, m_compliance, m_range, m_clientName);
  }

  std::string GetDescription() const override {
    return "Setup Keithley voltage source: " + std::to_string(m_voltage) + "V, " +
      std::to_string(m_compliance) + "A compliance" +
      (m_clientName.empty() ? "" : " (" + m_clientName + ")");
  }

private:
  double m_voltage;
  double m_compliance;
  std::string m_range;
  std::string m_clientName;
};

// Setup Keithley current source operation
class SetupKeithleyCurrentSourceOperation : public SequenceOperation {
public:
  SetupKeithleyCurrentSourceOperation(double current, double compliance = 10.0,
    const std::string& range = "AUTO",
    const std::string& clientName = "")
    : m_current(current), m_compliance(compliance), m_range(range), m_clientName(clientName) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.SMU_SetupCurrentSource(m_current, m_compliance, m_range, m_clientName);
  }

  std::string GetDescription() const override {
    return "Setup Keithley current source: " + std::to_string(m_current) + "A, " +
      std::to_string(m_compliance) + "V compliance" +
      (m_clientName.empty() ? "" : " (" + m_clientName + ")");
  }

private:
  double m_current;
  double m_compliance;
  std::string m_range;
  std::string m_clientName;
};

// Read Keithley voltage operation
class ReadKeithleyVoltageOperation : public SequenceOperation {
public:
  ReadKeithleyVoltageOperation(const std::string& clientName = "")
    : m_clientName(clientName) {
  }

  bool Execute(MachineOperations& ops) override {
    double voltage;
    if (ops.SMU_ReadVoltage(voltage, m_clientName)) {
      ops.LogInfo("Keithley voltage reading: " + std::to_string(voltage) + "V" +
        (m_clientName.empty() ? "" : " (" + m_clientName + ")"));
      return true;
    }
    return false;
  }

  std::string GetDescription() const override {
    return "Read Keithley voltage" + (m_clientName.empty() ? "" : " (" + m_clientName + ")");
  }

private:
  std::string m_clientName;
};

// Read Keithley current operation
class ReadKeithleyCurrentOperation : public SequenceOperation {
public:
  ReadKeithleyCurrentOperation(const std::string& clientName = "")
    : m_clientName(clientName) {
  }

  bool Execute(MachineOperations& ops) override {
    double current;
    if (ops.SMU_ReadCurrent(current, m_clientName)) {
      ops.LogInfo("Keithley current reading: " + std::to_string(current) + "A" +
        (m_clientName.empty() ? "" : " (" + m_clientName + ")"));
      return true;
    }
    return false;
  }

  std::string GetDescription() const override {
    return "Read Keithley current" + (m_clientName.empty() ? "" : " (" + m_clientName + ")");
  }

private:
  std::string m_clientName;
};

// Read Keithley resistance operation
class ReadKeithleyResistanceOperation : public SequenceOperation {
public:
  ReadKeithleyResistanceOperation(const std::string& clientName = "")
    : m_clientName(clientName) {
  }

  bool Execute(MachineOperations& ops) override {
    double resistance;
    if (ops.SMU_ReadResistance(resistance, m_clientName)) {
      ops.LogInfo("Keithley resistance reading: " + std::to_string(resistance) + " Ohms" +
        (m_clientName.empty() ? "" : " (" + m_clientName + ")"));
      return true;
    }
    return false;
  }

  std::string GetDescription() const override {
    return "Read Keithley resistance" + (m_clientName.empty() ? "" : " (" + m_clientName + ")");
  }

private:
  std::string m_clientName;
};

// Send Keithley SCPI command operation
class SendKeithleyCommandOperation : public SequenceOperation {
public:
  SendKeithleyCommandOperation(const std::string& command, const std::string& clientName = "")
    : m_command(command), m_clientName(clientName) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.SMU_SendCommand(m_command, m_clientName);
  }

  std::string GetDescription() const override {
    return "Send Keithley command: " + m_command +
      (m_clientName.empty() ? "" : " (" + m_clientName + ")");
  }

private:
  std::string m_command;
  std::string m_clientName;
};



// Save Current Position to Config operation
class SaveCurrentPositionToConfigOperation : public SequenceOperation {
public:
  SaveCurrentPositionToConfigOperation(const std::string& deviceName, const std::string& positionName)
    : m_deviceName(deviceName), m_positionName(positionName) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.SaveCurrentPositionToConfig(m_deviceName, m_positionName);
  }

  std::string GetDescription() const override {
    return "Save current position of " + m_deviceName + " to config as '" + m_positionName + "'";
  }

private:
  std::string m_deviceName;
  std::string m_positionName;
};

// Apply Needle Offset and Move operation
class ApplyNeedleOffsetAndMoveOperation : public SequenceOperation {
public:
  ApplyNeedleOffsetAndMoveOperation(const std::string& deviceName, const std::string& storedPositionLabel)
    : m_deviceName(deviceName), m_storedPositionLabel(storedPositionLabel) {
  }

  bool Execute(MachineOperations& ops) override {
    try {
      // Get the stored camera position
      PositionStruct cameraPosition;
      if (!ops.GetStoredPosition(m_storedPositionLabel, cameraPosition)) {
        ops.LogError("Failed to get stored camera position: " + m_storedPositionLabel);
        return false;
      }

      // Load needle offset from camera_to_object_offset.json
      std::string configPath = "camera_to_object_offset.json";
      std::ifstream configFile(configPath);
      if (!configFile.is_open()) {
        ops.LogError("Cannot open camera offset config file: " + configPath);
        return false;
      }

      nlohmann::json config;
      configFile >> config;
      configFile.close();

      // Check if needle offset exists
      if (!config.contains("hardware_offsets") ||
        !config["hardware_offsets"].contains("needle")) {
        ops.LogError("Needle offset not found in configuration");
        return false;
      }

      auto needleCoords = config["hardware_offsets"]["needle"]["coordinates"];
      double offsetX = needleCoords["x"];
      double offsetY = needleCoords["y"];
      double offsetZ = needleCoords["z"];

      ops.LogInfo("Applying needle offset: X=" + std::to_string(offsetX) +
        ", Y=" + std::to_string(offsetY) +
        ", Z=" + std::to_string(offsetZ));

      // Calculate needle position = camera position - needle offset
      // The offset in config represents camera-to-needle offset, so we need to invert it
      // to move from camera position to where needle should be positioned
      PositionStruct needlePosition = cameraPosition;
      needlePosition.x -= offsetX;  // CHANGED: subtract instead of add
      needlePosition.y -= offsetY;  // CHANGED: subtract instead of add
      needlePosition.z -= offsetZ;  // CHANGED: subtract instead of add

      ops.LogInfo("Moving from camera position to needle position:");
      ops.LogInfo("Camera: X=" + std::to_string(cameraPosition.x) +
        ", Y=" + std::to_string(cameraPosition.y) +
        ", Z=" + std::to_string(cameraPosition.z));
      ops.LogInfo("Needle: X=" + std::to_string(needlePosition.x) +
        ", Y=" + std::to_string(needlePosition.y) +
        ", Z=" + std::to_string(needlePosition.z));

      // Calculate the relative movement needed
      double deltaX = needlePosition.x - cameraPosition.x;
      double deltaY = needlePosition.y - cameraPosition.y;
      double deltaZ = needlePosition.z - cameraPosition.z;

      ops.LogInfo("Relative movement: deltaX=" + std::to_string(deltaX) +
        ", deltaY=" + std::to_string(deltaY) +
        ", deltaZ=" + std::to_string(deltaZ));

      // Apply the moves sequentially
      bool success = true;
      if (std::abs(deltaX) > 0.001) { // Only move if significant difference
        success &= ops.MoveRelative(m_deviceName, "X", deltaX, true);
      }
      if (std::abs(deltaY) > 0.001) {
        success &= ops.MoveRelative(m_deviceName, "Y", deltaY, true);
      }
      if (std::abs(deltaZ) > 0.001) {
        success &= ops.MoveRelative(m_deviceName, "Z", deltaZ, true);
      }

      if (success) {
        ops.LogInfo("Successfully moved to needle position");
      }
      else {
        ops.LogError("Failed to move to needle position");
      }

      return success;
    }
    catch (const std::exception& e) {
      ops.LogError("Exception while applying needle offset: " + std::string(e.what()));
      return false;
    }
  }

  std::string GetDescription() const override {
    return "Apply needle offset and move " + m_deviceName + " from camera to needle position";
  }

private:
  std::string m_deviceName;
  std::string m_storedPositionLabel;
};


// Create Safe Dispense Position operation
class CreateSafeDispensePositionOperation : public SequenceOperation {
public:
  CreateSafeDispensePositionOperation(const std::string& deviceName,
    const std::string& sourcePositionName,
    const std::string& safePositionName,
    double zOffset)
    : m_deviceName(deviceName), m_sourcePositionName(sourcePositionName),
    m_safePositionName(safePositionName), m_zOffset(zOffset) {
  }

  bool Execute(MachineOperations& ops) override {
    try {
      // Get current position (which should be the dispense position just saved)
      PositionStruct currentPosition;
      if (!ops.GetDeviceCurrentPosition(m_deviceName, currentPosition)) {
        ops.LogError("Failed to get current position for " + m_deviceName);
        return false;
      }

      // Create safe position by modifying Z coordinate
      PositionStruct safePosition = currentPosition;
      safePosition.z += m_zOffset; // m_zOffset should be negative (-0.5) to move up

      ops.LogInfo("Creating safe dispense position '" + m_safePositionName + "':");
      ops.LogInfo("Source position: X=" + std::to_string(currentPosition.x) +
        ", Y=" + std::to_string(currentPosition.y) +
        ", Z=" + std::to_string(currentPosition.z));
      ops.LogInfo("Safe position: X=" + std::to_string(safePosition.x) +
        ", Y=" + std::to_string(safePosition.y) +
        ", Z=" + std::to_string(safePosition.z) +
        " (Z offset: " + std::to_string(m_zOffset) + ")");

      // Move to safe position first
      std::string callerContext = "CreateSafeDispensePositionOperation_" + m_deviceName + "_Z_" + std::to_string(m_zOffset);
      bool moveSuccess = ops.MoveRelative(m_deviceName, "Z", m_zOffset, true, callerContext);
      if (!moveSuccess) {
        ops.LogError("Failed to move to safe position");
        return false;
      }

      // Save the safe position to config
      bool saveSuccess = ops.SaveCurrentPositionToConfig(m_deviceName, m_safePositionName);
      if (!saveSuccess) {
        ops.LogError("Failed to save safe position to config");
        return false;
      }

      ops.LogInfo("Successfully created and saved safe dispense position '" + m_safePositionName + "'");
      return true;

    }
    catch (const std::exception& e) {
      ops.LogError("Exception while creating safe dispense position: " + std::string(e.what()));
      return false;
    }
  }

  std::string GetDescription() const override {
    return "Create safe dispense position '" + m_safePositionName +
      "' from '" + m_sourcePositionName + "' with Z offset " + std::to_string(m_zOffset);
  }

private:
  std::string m_deviceName;
  std::string m_sourcePositionName;
  std::string m_safePositionName;
  double m_zOffset;
};


// Add this to SequenceStep.h with the other operation classes

/// <summary>
/// Enhanced blocking move operation that explicitly waits for motion completion
/// This operation provides stronger guarantees that movement is complete before
/// continuing to the next sequence step.
/// </summary>
// UPDATE BlockingMoveToPointNameOperation in SequenceStep.h:
class BlockingMoveToPointNameOperation : public SequenceOperation {
public:
  BlockingMoveToPointNameOperation(const std::string& deviceName,
    const std::string& positionName,
    int timeoutMs = 30000)
    : m_deviceName(deviceName), m_positionName(positionName), m_timeoutMs(timeoutMs) {
  }

  bool Execute(MachineOperations& ops) override {
    ops.LogInfo("Starting BLOCKING move of " + m_deviceName + " to position '" + m_positionName + "'");

    // Generate caller context - FOLLOWING EXISTING PATTERN
    std::string callerContext = "BlockingMoveToPointNameOperation_" + m_deviceName + "_to_" + m_positionName;

    // Step 1: Start the move (with blocking=true) - NOW WITH CALLER CONTEXT
    bool moveStarted = ops.MoveToPointName(m_deviceName, m_positionName, true, callerContext);
    if (!moveStarted) {
      ops.LogError("Failed to start move operation for " + m_deviceName);
      return false;
    }

    ops.LogInfo("Move command sent, now explicitly waiting for motion completion...");

    // Step 2: Explicitly wait for motion completion with timeout
    bool motionCompleted = ops.WaitForDeviceMotionCompletion(m_deviceName, m_timeoutMs);
    if (!motionCompleted) {
      ops.LogError("Timeout waiting for " + m_deviceName + " motion to complete after " +
        std::to_string(m_timeoutMs) + "ms");
      return false;
    }

    // Step 3: Small additional settling delay for mechanical systems
    ops.Wait(100);  // 100ms settling time

    ops.LogInfo("BLOCKING move of " + m_deviceName + " to '" + m_positionName + "' completed successfully");
    return true;
  }

  std::string GetDescription() const override {
    return "BLOCKING move " + m_deviceName + " to named position '" + m_positionName +
      "' (timeout: " + std::to_string(m_timeoutMs) + "ms)";
  }

private:
  std::string m_deviceName;
  std::string m_positionName;
  int m_timeoutMs;
};

/// <summary>
/// Non-blocking move operation for cases where you want to start multiple moves in parallel
/// </summary>
// UPDATE NonBlockingMoveToPointNameOperation in SequenceStep.h:
class NonBlockingMoveToPointNameOperation : public SequenceOperation {
public:
  NonBlockingMoveToPointNameOperation(const std::string& deviceName,
    const std::string& positionName)
    : m_deviceName(deviceName), m_positionName(positionName) {
  }

  bool Execute(MachineOperations& ops) override {
    ops.LogInfo("Starting NON-BLOCKING move of " + m_deviceName + " to position '" + m_positionName + "'");

    // Generate caller context - FOLLOWING EXISTING PATTERN
    std::string callerContext = "NonBlockingMoveToPointNameOperation_" + m_deviceName + "_to_" + m_positionName;

    // Start the move with blocking=false - NOW WITH CALLER CONTEXT
    bool moveStarted = ops.MoveToPointName(m_deviceName, m_positionName, false, callerContext);
    if (!moveStarted) {
      ops.LogError("Failed to start non-blocking move operation for " + m_deviceName);
      return false;
    }

    ops.LogInfo("Non-blocking move command sent for " + m_deviceName + " - continuing to next operation");
    return true;
  }

  std::string GetDescription() const override {
    return "NON-BLOCKING move " + m_deviceName + " to named position '" + m_positionName + "'";
  }

private:
  std::string m_deviceName;
  std::string m_positionName;
};



/// <summary>
/// Wait for motion completion operation - useful after non-blocking moves
/// </summary>
class WaitForMotionCompletionOperation : public SequenceOperation {
public:
  /// <summary>
  /// Creates a new wait for motion completion operation
  /// </summary>
  /// <param name="deviceName">Name of the device to wait for</param>
  /// <param name="timeoutMs">Maximum time to wait (default: 30 seconds)</param>
  WaitForMotionCompletionOperation(const std::string& deviceName, int timeoutMs = 30000)
    : m_deviceName(deviceName), m_timeoutMs(timeoutMs) {
  }

  /// <summary>
  /// Waits for the specified device to complete its motion
  /// </summary>
  /// <param name="ops">Reference to MachineOperations</param>
  /// <returns>True if motion completed within timeout, false otherwise</returns>
  bool Execute(MachineOperations& ops) override {
    ops.LogInfo("Waiting for motion completion of " + m_deviceName +
      " (timeout: " + std::to_string(m_timeoutMs) + "ms)");

    bool completed = ops.WaitForDeviceMotionCompletion(m_deviceName, m_timeoutMs);

    if (completed) {
      ops.LogInfo("Motion completed for " + m_deviceName);
    }
    else {
      ops.LogError("Timeout waiting for motion completion of " + m_deviceName);
    }

    return completed;
  }

  /// <summary>
  /// Gets a description of this operation for logging and debugging
  /// </summary>
  /// <returns>Human-readable description</returns>
  std::string GetDescription() const override {
    return "Wait for motion completion of " + m_deviceName +
      " (timeout: " + std::to_string(m_timeoutMs) + "ms)";
  }

private:
  std::string m_deviceName; ///< Name of the device to wait for
  int m_timeoutMs;          ///< Timeout in milliseconds
};


// ReadInput operation with internal storage (RECOMMENDED for sequences)
class ReadInputOperation : public SequenceOperation {
public:
  ReadInputOperation(const std::string& deviceName, int inputPin)
    : m_deviceName(deviceName), m_inputPin(inputPin), m_lastState(false) {
  }

  bool Execute(MachineOperations& ops) override {
    // Generate descriptive caller context
    std::string callerContext = "ReadInputOperation_" + m_deviceName + "_pin" +
      std::to_string(m_inputPin);

    // Call machine operations with tracking
    return ops.ReadInput(m_deviceName, m_inputPin, m_lastState, callerContext);
  }

  std::string GetDescription() const override {
    return "Read input " + m_deviceName + " pin " + std::to_string(m_inputPin) +
      " (last state: " + (m_lastState ? "HIGH" : "LOW") + ")";
  }

  // Getter methods for accessing the read state
  bool GetLastState() const { return m_lastState; }
  bool IsHigh() const { return m_lastState; }
  bool IsLow() const { return !m_lastState; }
  std::string GetStateString() const { return m_lastState ? "HIGH" : "LOW"; }

private:
  std::string m_deviceName;
  int m_inputPin;
  bool m_lastState;  // Internal storage for the state
};

// ClearLatch operation with tracking
class ClearLatchOperation : public SequenceOperation {
public:
  ClearLatchOperation(const std::string& deviceName, int inputPin)
    : m_deviceName(deviceName), m_inputPin(inputPin) {
  }

  bool Execute(MachineOperations& ops) override {
    // Generate descriptive caller context
    std::string callerContext = "ClearLatchOperation_" + m_deviceName + "_pin" +
      std::to_string(m_inputPin);

    // Call machine operations with tracking
    return ops.ClearLatch(m_deviceName, m_inputPin, callerContext);
  }

  std::string GetDescription() const override {
    return "Clear latch " + m_deviceName + " pin " + std::to_string(m_inputPin);
  }

private:
  std::string m_deviceName;
  int m_inputPin;
};



/// <summary>
/// Store current device speed operation - captures current speed for later restoration
/// </summary>
class StoreCurrentSpeedOperation : public SequenceOperation {
public:
  StoreCurrentSpeedOperation(const std::string& deviceName, const std::string& storageKey = "")
    : m_deviceName(deviceName), m_storageKey(storageKey.empty() ? deviceName + "_stored_speed" : storageKey) {
  }

  bool Execute(MachineOperations& ops) override {
    std::string callerContext = "StoreCurrentSpeedOperation_" + m_deviceName + "_" + m_storageKey;

    // Get current speed from device (you'll need to implement this)
    double currentSpeed = 0.0;
    bool success = ops.GetDeviceSpeed(m_deviceName, currentSpeed, callerContext);

    if (success) {
      // Store the speed in a static map for later retrieval
      GetStoredSpeeds()[m_storageKey] = currentSpeed;
      ops.LogInfo("Stored current speed " + std::to_string(currentSpeed) +
        " mm/s for device " + m_deviceName + " with key '" + m_storageKey + "'");
    }
    else {
      ops.LogWarning("Failed to get current speed for " + m_deviceName + ", using default");
      // Store default speeds based on controller type
      double defaultSpeed = ops.IsDevicePIController(m_deviceName) ? 10.0 : 30.0;
      GetStoredSpeeds()[m_storageKey] = defaultSpeed;
      ops.LogInfo("Stored default speed " + std::to_string(defaultSpeed) +
        " mm/s for device " + m_deviceName + " with key '" + m_storageKey + "'");
    }

    return true;
  }

  std::string GetDescription() const override {
    return "Store current speed for device " + m_deviceName + " (key: " + m_storageKey + ")";
  }

  // Static method to access stored speeds
  static std::map<std::string, double>& GetStoredSpeeds() {
    static std::map<std::string, double> storedSpeeds;
    return storedSpeeds;
  }

private:
  std::string m_deviceName;
  std::string m_storageKey;
};

/// <summary>
/// Set device speed operation
/// </summary>
class SetDeviceSpeedOperation : public SequenceOperation {
public:
  SetDeviceSpeedOperation(const std::string& deviceName, double speed)
    : m_deviceName(deviceName), m_speed(speed) {
  }

  bool Execute(MachineOperations& ops) override {
    std::string callerContext = "SetDeviceSpeedOperation_" + m_deviceName + "_" + std::to_string(m_speed);

    ops.LogInfo("Setting speed for device " + m_deviceName + " to " + std::to_string(m_speed) + " mm/s");

    return ops.SetDeviceSpeed(m_deviceName, m_speed, callerContext);
  }

  std::string GetDescription() const override {
    return "Set speed for device " + m_deviceName + " to " + std::to_string(m_speed) + " mm/s";
  }

private:
  std::string m_deviceName;
  double m_speed;
};

/// <summary>
/// Restore previously stored device speed operation
/// </summary>
class RestoreStoredSpeedOperation : public SequenceOperation {
public:
  RestoreStoredSpeedOperation(const std::string& deviceName, const std::string& storageKey = "")
    : m_deviceName(deviceName), m_storageKey(storageKey.empty() ? deviceName + "_stored_speed" : storageKey) {
  }

  bool Execute(MachineOperations& ops) override {
    std::string callerContext = "RestoreStoredSpeedOperation_" + m_deviceName + "_" + m_storageKey;

    auto& storedSpeeds = StoreCurrentSpeedOperation::GetStoredSpeeds();
    auto it = storedSpeeds.find(m_storageKey);

    if (it != storedSpeeds.end()) {
      double storedSpeed = it->second;
      ops.LogInfo("Restoring stored speed " + std::to_string(storedSpeed) +
        " mm/s for device " + m_deviceName + " from key '" + m_storageKey + "'");

      return ops.SetDeviceSpeed(m_deviceName, storedSpeed, callerContext);
    }
    else {
      ops.LogError("No stored speed found for key '" + m_storageKey + "' - cannot restore");
      return false;
    }
  }

  std::string GetDescription() const override {
    return "Restore stored speed for device " + m_deviceName + " (key: " + m_storageKey + ")";
  }

private:
  std::string m_deviceName;
  std::string m_storageKey;
};



// Add these new sequence operation classes to SequenceStep.h:

// ACS Run Buffer operation with tracking
class ACSRunBufferOperation : public SequenceOperation {
public:
  ACSRunBufferOperation(const std::string& deviceName, int bufferNumber, const std::string& labelName = "")
    : m_deviceName(deviceName), m_bufferNumber(bufferNumber), m_labelName(labelName) {
  }

  bool Execute(MachineOperations& ops) override {
    // Generate descriptive caller context
    std::string callerContext = "ACSRunBufferOperation_" + m_deviceName + "_buffer" +
      std::to_string(m_bufferNumber) +
      (m_labelName.empty() ? "" : "_" + m_labelName);

    // Call machine operations with tracking
    return ops.acsc_RunBuffer(m_deviceName, m_bufferNumber, m_labelName, callerContext);
  }

  std::string GetDescription() const override {
    return "Run ACS buffer " + std::to_string(m_bufferNumber) + " on " + m_deviceName +
      (m_labelName.empty() ? "" : " from label " + m_labelName);
  }

private:
  std::string m_deviceName;
  int m_bufferNumber;
  std::string m_labelName;
};

// ACS Stop Buffer operation with tracking
class ACSStopBufferOperation : public SequenceOperation {
public:
  ACSStopBufferOperation(const std::string& deviceName, int bufferNumber)
    : m_deviceName(deviceName), m_bufferNumber(bufferNumber) {
  }

  bool Execute(MachineOperations& ops) override {
    // Generate descriptive caller context
    std::string callerContext = "ACSStopBufferOperation_" + m_deviceName + "_buffer" +
      std::to_string(m_bufferNumber);

    // Call machine operations with tracking
    return ops.acsc_StopBuffer(m_deviceName, m_bufferNumber, callerContext);
  }

  std::string GetDescription() const override {
    return "Stop ACS buffer " + std::to_string(m_bufferNumber) + " on " + m_deviceName;
  }

private:
  std::string m_deviceName;
  int m_bufferNumber;
};

// ACS Stop All Buffers operation with tracking
class ACSStopAllBuffersOperation : public SequenceOperation {
public:
  ACSStopAllBuffersOperation(const std::string& deviceName)
    : m_deviceName(deviceName) {
  }

  bool Execute(MachineOperations& ops) override {
    // Generate descriptive caller context
    std::string callerContext = "ACSStopAllBuffersOperation_" + m_deviceName;

    // Call machine operations with tracking
    return ops.acsc_StopAllBuffers(m_deviceName, callerContext);
  }

  std::string GetDescription() const override {
    return "Stop all ACS buffers on " + m_deviceName;
  }

private:
  std::string m_deviceName;
};

// ACS Buffer Status Check operation
class ACSBufferStatusOperation : public SequenceOperation {
public:
  ACSBufferStatusOperation(const std::string& deviceName, int bufferNumber)
    : m_deviceName(deviceName), m_bufferNumber(bufferNumber), m_isRunning(false) {
  }

  bool Execute(MachineOperations& ops) override {
    // Check buffer status (no caller context needed for status queries)
    m_isRunning = ops.acsc_IsBufferRunning(m_deviceName, m_bufferNumber);
    return true; // Status check always succeeds
  }

  std::string GetDescription() const override {
    return "Check ACS buffer " + std::to_string(m_bufferNumber) + " status on " + m_deviceName +
      " (status: " + (m_isRunning ? "RUNNING" : "STOPPED") + ")";
  }

  // Getter methods for accessing the status
  bool IsRunning() const { return m_isRunning; }
  bool IsStopped() const { return !m_isRunning; }
  std::string GetStatusString() const { return m_isRunning ? "RUNNING" : "STOPPED"; }

private:
  std::string m_deviceName;
  int m_bufferNumber;
  bool m_isRunning;
};