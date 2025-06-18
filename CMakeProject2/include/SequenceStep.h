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



// Represents an operation in a sequence
class SequenceOperation {
public:
  virtual ~SequenceOperation() = default;
  virtual bool Execute(MachineOperations& ops) = 0;
  virtual std::string GetDescription() const = 0;
};

// Move device to node operation
class MoveToNodeOperation : public SequenceOperation {
public:
  MoveToNodeOperation(const std::string& deviceName, const std::string& graphName,
    const std::string& nodeId)
    : m_deviceName(deviceName), m_graphName(graphName), m_nodeId(nodeId) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.MoveDeviceToNode(m_deviceName, m_graphName, m_nodeId, true);
  }

  std::string GetDescription() const override {
    return "Move " + m_deviceName + " to node " + m_nodeId + " in graph " + m_graphName;
  }

private:
  std::string m_deviceName;
  std::string m_graphName;
  std::string m_nodeId;
};

// Set output operation with configurable delay
class SetOutputOperation : public SequenceOperation {
public:
  SetOutputOperation(const std::string& deviceName, int pin, bool state, int delayMs = 200)
    : m_deviceName(deviceName), m_pin(pin), m_state(state), m_delayMs(delayMs) {
  }

  bool Execute(MachineOperations& ops) override {
    bool result = ops.SetOutput(m_deviceName, m_pin, m_state);

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

// Retract slide operation
class RetractSlideOperation : public SequenceOperation {
public:
  RetractSlideOperation(const std::string& slideName)
    : m_slideName(slideName) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.RetractSlide(m_slideName, true);
  }

  std::string GetDescription() const override {
    return "Retract pneumatic slide " + m_slideName;
  }

private:
  std::string m_slideName;
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

// Add this to SequenceStep.h with the other operation classes
class MoveToPointNameOperation : public SequenceOperation {
public:
  MoveToPointNameOperation(const std::string& deviceName, const std::string& positionName)
    : m_deviceName(deviceName), m_positionName(positionName) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.MoveToPointName(m_deviceName, m_positionName, true);
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
    return ops.StartScan(m_deviceName, m_dataChannel, m_stepSizes, m_settlingTimeMs, m_axesToScan);
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

// Stop Scan operation
class StopScanOperation : public SequenceOperation {
public:
  StopScanOperation(const std::string& deviceName)
    : m_deviceName(deviceName) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.StopScan(m_deviceName);
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
        ops.StopScan(m_deviceName);
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




// Scan operation with automatic wait for completion
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
    // 1. Start the scan
    if (!ops.StartScan(m_deviceName, m_dataChannel, m_stepSizes, m_settlingTimeMs, m_axesToScan)) {
      return false;
    }

    // 2. Wait for completion
    auto startTime = std::chrono::steady_clock::now();
    auto endTime = startTime + std::chrono::milliseconds(m_timeoutMs);

    while (ops.IsScanActive(m_deviceName)) {
      if (std::chrono::steady_clock::now() > endTime) {
        ops.StopScan(m_deviceName);
        return false;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Note: The scanning algorithm automatically moves to the peak position,
    // so we don't need to do that explicitly here.
    return true;
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

    return "Run scan on " + m_deviceName + " using " + m_dataChannel +
      " over " + axesStr + " axes with " + stepsStr +
      " steps (auto-moves to peak)";
  }

private:
  std::string m_deviceName;
  std::string m_dataChannel;
  std::vector<double> m_stepSizes;
  int m_settlingTimeMs;
  std::vector<std::string> m_axesToScan;
  int m_timeoutMs;
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
class MoveRelativeOperation : public SequenceOperation {
public:
  MoveRelativeOperation(const std::string& deviceName, const std::string& axis, double distance)
    : m_deviceName(deviceName), m_axis(axis), m_distance(distance) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.MoveRelative(m_deviceName, m_axis, m_distance, true);
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
/// Operation that periodically reads and logs a data value from the GlobalDataStore
/// for a specified duration and at specified intervals.
/// </summary>
class PeriodicMonitorDataValueOperation : public SequenceOperation {
public:
  /// <summary>
  /// Creates a new operation to periodically monitor a data value.
  /// </summary>
  /// <param name="dataId">The identifier of the data channel to monitor.</param>
  /// <param name="durationMs">The total duration to monitor in milliseconds.</param>
  /// <param name="intervalMs">The interval between readings in milliseconds.</param>
  PeriodicMonitorDataValueOperation(const std::string& dataId, int durationMs, int intervalMs)
    : m_dataId(dataId), m_durationMs(durationMs), m_intervalMs(intervalMs) {
  }

  /// <summary>
  /// Executes the operation, reading and logging the specified data value at regular intervals.
  /// </summary>
  /// <param name="ops">Reference to MachineOperations for accessing the data store.</param>
  /// <returns>True if the monitoring completed successfully, false otherwise.</returns>
  bool Execute(MachineOperations& ops) override {
    auto startTime = std::chrono::steady_clock::now();
    auto endTime = startTime + std::chrono::milliseconds(m_durationMs);

    ops.LogInfo("Starting periodic monitoring of " + m_dataId +
      " for " + std::to_string(m_durationMs / 1000) + " seconds");

    while (std::chrono::steady_clock::now() < endTime) {
      float value = ops.ReadDataValue(m_dataId);
      ops.LogInfo(m_dataId + " value: " + std::to_string(value));

      // Sleep for the interval duration
      std::this_thread::sleep_for(std::chrono::milliseconds(m_intervalMs));
    }

    ops.LogInfo("Completed periodic monitoring of " + m_dataId);
    return true;
  }

  /// <summary>
  /// Gets a human-readable description of this operation.
  /// </summary>
  /// <returns>A string describing the monitoring operation.</returns>
  std::string GetDescription() const override {
    return "Monitor " + m_dataId + " for " +
      std::to_string(m_durationMs / 1000) + " seconds";
  }

private:
  std::string m_dataId;  ///< The identifier of the data channel to monitor
  int m_durationMs;      ///< The total duration to monitor in milliseconds
  int m_intervalMs;      ///< The interval between readings in milliseconds
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

// ===================================================================
// CORRECTED UserPromptOperation for SequenceStep.h
// ===================================================================

class UserPromptOperation : public SequenceOperation {
public:
  UserPromptOperation(const std::string& title, const std::string& message, UserPromptUI& promptUI)
    : m_title(title), m_message(message), m_promptUI(promptUI), m_completed(false), m_result(PromptResult::PENDING) {
  }

  // FIXED: Remove "UserPromptOperation::" - it's already inside the class
  bool Execute(MachineOperations& ops) override {
    ops.LogInfo("Requesting user prompt: " + m_title);

    // Reset completion state
    m_completed = false;
    m_result = PromptResult::PENDING;

    // Use RequestPrompt (thread-safe) instead of ShowPrompt
    m_promptUI.RequestPrompt(m_title, m_message, [this](PromptResult result) {
      m_result = result;
      m_completed = true;
    });

    // Wait for user response with timeout
    int timeoutSeconds = 3600; // 30 second timeout
    int waitedMs = 0;
    while (!m_completed && waitedMs < (timeoutSeconds * 1000)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      waitedMs += 100;
    }

    if (!m_completed) {
      ops.LogWarning("User prompt timed out after " + std::to_string(timeoutSeconds) + " seconds");
      return false;
    }

    // Handle result
    switch (m_result) {
    case PromptResult::YES:
      ops.LogInfo("User confirmed YES - continuing execution");
      return true;
    case PromptResult::NO:
      ops.LogWarning("User selected NO - stopping execution");
      return false;
    case PromptResult::CANCELLED:
      ops.LogWarning("User cancelled prompt - stopping execution");
      return false;
    default:
      ops.LogError("Unknown prompt result - stopping execution");
      return false;
    }
  }

  std::string GetDescription() const override {
    return "User prompt: " + m_title;
  }

private:
  std::string m_title;
  std::string m_message;
  UserPromptUI& m_promptUI;
  std::atomic<bool> m_completed;
  std::atomic<PromptResult> m_result;
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
      bool moveSuccess = ops.MoveRelative(m_deviceName, "Z", m_zOffset, true);
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