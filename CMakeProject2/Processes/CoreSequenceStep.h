#pragma once
#include "ProcessStep.h"
#include "SequenceStep.h"
#include "IDisplayOutput.h"
#include "global_data_store.h"
#include <chrono>
#include <ctime>
#include <iostream>
#include <string>

class MachineOperations; // Forward declaration

class DoSomethingB : public SequenceOperation {
public:
  DoSomethingB(std::string task) {
    m_task = task;
  }

  bool Execute(MachineOperations& ops) override {
    ops.LogInfo(" B is doing: " + m_task);
    // Simulate work
    ops.Wait(500);
    return true; // Indicate success
  }

  std::string GetDescription() const override {
    return "Perform " + m_task + " by B";
  }
private:
  std::string m_task;
};


class PrintOutValue : public SequenceOperation {
private:
  IDisplayOutput* displayOutput;
  std::string channelName;
  std::string unit;
  bool addTimestamp;
  bool useSIFormat;

public:
  PrintOutValue(IDisplayOutput* output,
    const std::string& channel,
    const std::string& unitType = "A",
    bool timestamp = true,
    bool siFormat = true)
    : displayOutput(output)
    , channelName(channel)
    , unit(unitType)
    , addTimestamp(timestamp)
    , useSIFormat(siFormat) {
  }

  bool Execute(MachineOperations& ops) override {
    if (!displayOutput) return false;

    // FIX 1: GetInstance() with capital G and I
    GlobalDataStore* dataStore = GlobalDataStore::GetInstance();
    if (!dataStore) return false;

    // FIX 2: Use GetValue() not getDouble()
    float value = dataStore->GetValue(channelName, 0.0f);

    std::string formattedValue;

    if (useSIFormat) {
      formattedValue = FormatWithSI(value, unit);
    }
    else {
      // Raw decimal with 12 places
      char buffer[64];
      snprintf(buffer, sizeof(buffer), "%.12f %s", value, unit.c_str());
      formattedValue = std::string(buffer);
    }

    std::string message = channelName + ": " + formattedValue;

    if (addTimestamp) {
      auto now = std::chrono::system_clock::now();
      auto time = std::chrono::system_clock::to_time_t(now);
      std::tm tm;
      localtime_s(&tm, &time);
      char timeStr[100];
      std::strftime(timeStr, sizeof(timeStr), "[%Y-%m-%d %H:%M:%S] ", &tm);
      message = std::string(timeStr) + message;
    }

    displayOutput->displayText(message);
    return true;
  }

  std::string GetDescription() const override {
    return "Print value: " + channelName +
      (useSIFormat ? " (SI format)" : " (raw decimal)");
  }

private:
  std::string FormatWithSI(double value, const std::string& baseUnit) {
    double absValue = std::abs(value);
    char buffer[64];

    if (absValue == 0.0) {
      snprintf(buffer, sizeof(buffer), "0.00 %s", baseUnit.c_str());
    }
    else if (absValue < 1e-12) {
      snprintf(buffer, sizeof(buffer), "%.3e %s", value, baseUnit.c_str());
    }
    else if (absValue < 1e-9) {
      // pico
      snprintf(buffer, sizeof(buffer), "%.2f p%s", value * 1e12, baseUnit.c_str());
    }
    else if (absValue < 1e-6) {
      // nano
      snprintf(buffer, sizeof(buffer), "%.2f n%s", value * 1e9, baseUnit.c_str());
    }
    else if (absValue < 1e-3) {
      // micro
      snprintf(buffer, sizeof(buffer), "%.2f µ%s", value * 1e6, baseUnit.c_str());
    }
    else if (absValue < 1.0) {
      // milli
      snprintf(buffer, sizeof(buffer), "%.3f m%s", value * 1e3, baseUnit.c_str());
    }
    else if (absValue < 1e3) {
      // base unit
      snprintf(buffer, sizeof(buffer), "%.3f %s", value, baseUnit.c_str());
    }
    else if (absValue < 1e6) {
      // kilo
      snprintf(buffer, sizeof(buffer), "%.3f k%s", value * 1e-3, baseUnit.c_str());
    }
    else if (absValue < 1e9) {
      // mega
      snprintf(buffer, sizeof(buffer), "%.3f M%s", value * 1e-6, baseUnit.c_str());
    }
    else {
      snprintf(buffer, sizeof(buffer), "%.3e %s", value, baseUnit.c_str());
    }

    return std::string(buffer);
  }
};



class CorePickPlace : public SequenceOperation {
public:
  CorePickPlace(
    const std::string& deviceName,
    float speed,
    const std::string& pickNode,
    const std::string& placeNode,
    bool enableCameraView,
    const std::string& cameraGantry,
    const std::string& cameraViewPickNode,
    const std::string& cameraViewPlaceNode,
    const std::string& graphName,
    const std::string& gripperOutputDevice,
    const std::string& gripperPinName,      // Changed to pin name
    float gripperHoldDelay)
    : m_deviceName(deviceName),
    m_speed(speed),
    m_pickNode(pickNode),
    m_placeNode(placeNode),
    m_enableCameraView(enableCameraView),
    m_cameraGantry(cameraGantry),
    m_cameraViewPickNode(cameraViewPickNode),
    m_cameraViewPlaceNode(cameraViewPlaceNode),
    m_graphName(graphName),
    m_gripperOutputDevice(gripperOutputDevice),
    m_gripperPinName(gripperPinName),
    m_gripperHoldDelay(gripperHoldDelay) {
  }

  bool Execute(MachineOperations& ops) override {
    ops.LogInfo("=== CorePickPlace Operation ===");
    ops.LogInfo("Device: " + m_deviceName);
    ops.LogInfo("Graph: " + m_graphName);
    ops.LogInfo("Gripper Device: " + m_gripperOutputDevice);
    ops.LogInfo("Gripper Pin: " + m_gripperPinName);

    // === PICK SEQUENCE ===

    if (m_enableCameraView) {
      ops.LogInfo("Moving camera to PICK view: " + m_cameraViewPickNode);
      if (!ops.MoveToNode(m_cameraGantry, m_graphName, m_cameraViewPickNode,true,"CorePickPlace")) {
        ops.LogError("Failed to move camera to pick view");
        return false;
      }
      ops.Wait(500);
    }

    ops.LogInfo("Moving to pick position: " + m_pickNode);
    if (!ops.MoveToNode(m_deviceName, m_graphName, m_pickNode, true, "CorePickPlace")) {
      ops.LogError("Failed to move to pick position");
      return false;
    }
    ops.Wait(500);

    // Activate gripper using pin name
    ops.LogInfo("Activating gripper: " + m_gripperPinName);
    if (!ops.SetOutputByName(m_gripperOutputDevice, m_gripperPinName, true)) {
      ops.LogError("Failed to activate gripper");
      return false;
    }
    ops.Wait(200);

    // Release and re-grip cycle
    ops.LogInfo("Grip verification cycle");
    if (!ops.SetOutputByName(m_gripperOutputDevice, m_gripperPinName, false)) {
      ops.LogError("Failed to release gripper");
      return false;
    }
    ops.Wait(static_cast<int>(m_gripperHoldDelay * 1000));

    if (!ops.SetOutputByName(m_gripperOutputDevice, m_gripperPinName, true)) {
      ops.LogError("Failed to re-grip");
      return false;
    }
    ops.Wait(500);

    // === PLACE SEQUENCE ===

    if (m_enableCameraView) {
      ops.LogInfo("Moving camera to PLACE view: " + m_cameraViewPlaceNode);
      if (!ops.MoveToNode(m_cameraGantry, m_graphName, m_cameraViewPlaceNode, true, "CorePickPlace")) {
        ops.LogError("Failed to move camera to place view");
        return false;
      }
      ops.Wait(500);
    }

    ops.LogInfo("Moving to place position: " + m_placeNode);
    if (!ops.MoveToNode(m_deviceName, m_graphName, m_placeNode, true, "CorePickPlace")) {
      ops.LogError("Failed to move to place position");
      return false;
    }
    ops.Wait(500);

    // Release gripper using pin name
    ops.LogInfo("Releasing gripper");
    if (!ops.SetOutputByName(m_gripperOutputDevice, m_gripperPinName, false)) {
      ops.LogError("Failed to release gripper");
      return false;
    }
    ops.Wait(300);

    ops.LogInfo("CorePickPlace completed successfully");
    return true;
  }

  std::string GetDescription() const override {
    return "Core Pick&Place: " + m_deviceName + " (" + m_pickNode + " -> " + m_placeNode + ")";
  }

private:
  std::string m_deviceName;
  float m_speed;
  std::string m_pickNode;
  std::string m_placeNode;
  bool m_enableCameraView;
  std::string m_cameraGantry;
  std::string m_cameraViewPickNode;
  std::string m_cameraViewPlaceNode;
  std::string m_graphName;
  std::string m_gripperOutputDevice;
  std::string m_gripperPinName;      // Now a name instead of number
  float m_gripperHoldDelay;
};



/* actual node

hex-right
pick
  node_5245
see pick
  node_4209

  gripper right  "IOBottom", 2

place
  node_5263
see place
 node_4209



*/

// Core Pick Only Operation
class CorePick : public SequenceOperation {
public:
  CorePick(
    const std::string& deviceName,
    const std::string& graphName,
    const std::string& pickNode,
    const std::string& cameraGantry,
    const std::string& cameraViewNode,
    const std::string& gripperOutputDevice,
    const std::string& gripperPinName,
    float gripperHoldDelay,
    float speed,
    bool enableCameraView,
    bool slowDownOnApproach,
    float approachDistance,
    float speedOnApproach)
    : m_deviceName(deviceName),
    m_graphName(graphName),
    m_pickNode(pickNode),
    m_cameraGantry(cameraGantry),
    m_cameraViewNode(cameraViewNode),
    m_gripperOutputDevice(gripperOutputDevice),
    m_gripperPinName(gripperPinName),
    m_gripperHoldDelay(gripperHoldDelay),
    m_speed(speed),
    m_enableCameraView(enableCameraView),
    m_slowDownOnApproach(slowDownOnApproach),
    m_approachDistance(approachDistance),
    m_speedOnApproach(speedOnApproach) {
  }

  bool Execute(MachineOperations& ops) override {
    ops.LogInfo("=== CorePick Operation ===");
    ops.LogInfo("Device: " + m_deviceName);
    ops.LogInfo("Graph: " + m_graphName);
    ops.LogInfo("Pick Node: " + m_pickNode);
    ops.LogInfo("Gripper Device: " + m_gripperOutputDevice);
    ops.LogInfo("Gripper Pin: " + m_gripperPinName);
    ops.LogInfo("Speed: " + std::to_string(m_speed));

    // Move camera to view position
    if (m_enableCameraView) {
      ops.LogInfo("Moving camera to view position: " + m_cameraViewNode);
      if (!ops.MoveToNode(m_cameraGantry, m_graphName, m_cameraViewNode,true,"CorePick")) {
        ops.LogError("Failed to move camera to view position");
        return false;
      }
      ops.Wait(500);
    }

    // Move to pick position
    ops.LogInfo("Moving to pick position: " + m_pickNode);
    if (!ops.MoveToNode(m_deviceName, m_graphName, m_pickNode, true, "CorePick")) {
      ops.LogError("Failed to move to pick position");
      return false;
    }
    ops.Wait(500);

    // Activate gripper
    ops.LogInfo("Activating gripper: " + m_gripperPinName);
    if (!ops.SetOutputByName(m_gripperOutputDevice, m_gripperPinName, true)) {
      ops.LogError("Failed to activate gripper");
      return false;
    }
    ops.Wait(200);

    // Grip verification cycle
    ops.LogInfo("Grip verification cycle");
    if (!ops.SetOutputByName(m_gripperOutputDevice, m_gripperPinName, false)) {
      ops.LogError("Failed to release gripper");
      return false;
    }
    ops.Wait(static_cast<int>(m_gripperHoldDelay * 1000));

    if (!ops.SetOutputByName(m_gripperOutputDevice, m_gripperPinName, true)) {
      ops.LogError("Failed to re-grip");
      return false;
    }
    ops.Wait(500);

    ops.LogInfo("CorePick completed successfully");
    return true;
  }

  std::string GetDescription() const override {
    return "Core Pick: " + m_deviceName + " @ " + m_pickNode;
  }

private:
  std::string m_deviceName;
  std::string m_graphName;
  std::string m_pickNode;
  std::string m_cameraGantry;
  std::string m_cameraViewNode;
  std::string m_gripperOutputDevice;
  std::string m_gripperPinName;
  float m_gripperHoldDelay;
  float m_speed;
  bool m_enableCameraView;
  bool m_slowDownOnApproach;
  float m_approachDistance;
  float m_speedOnApproach;
};

// Core Place Only Operation
class CorePlace : public SequenceOperation {
public:
  CorePlace(
    const std::string& deviceName,
    const std::string& graphName,
    const std::string& placeNode,
    const std::string& cameraGantry,
    const std::string& cameraViewNode,
    const std::string& gripperOutputDevice,
    const std::string& gripperPinName,
    float gripperHoldDelay,
    float speed,
    bool enableCameraView,
    bool slowDownOnApproach,
    float approachDistance,
    float speedOnApproach)
    : m_deviceName(deviceName),
    m_graphName(graphName),
    m_placeNode(placeNode),
    m_cameraGantry(cameraGantry),
    m_cameraViewNode(cameraViewNode),
    m_gripperOutputDevice(gripperOutputDevice),
    m_gripperPinName(gripperPinName),
    m_gripperHoldDelay(gripperHoldDelay),
    m_speed(speed),
    m_enableCameraView(enableCameraView),
    m_slowDownOnApproach(slowDownOnApproach),
    m_approachDistance(approachDistance),
    m_speedOnApproach(speedOnApproach) {
  }

  bool Execute(MachineOperations& ops) override {
    ops.LogInfo("=== CorePlace Operation ===");
    ops.LogInfo("Device: " + m_deviceName);
    ops.LogInfo("Graph: " + m_graphName);
    ops.LogInfo("Place Node: " + m_placeNode);
    ops.LogInfo("Gripper Device: " + m_gripperOutputDevice);
    ops.LogInfo("Gripper Pin: " + m_gripperPinName);
    ops.LogInfo("Speed: " + std::to_string(m_speed));

    // Move camera to view position
    if (m_enableCameraView) {
      ops.LogInfo("Moving camera to view position: " + m_cameraViewNode);
      if (!ops.MoveToNode(m_cameraGantry, m_graphName, m_cameraViewNode, true, "CorePlace")) {
        ops.LogError("Failed to move camera to view position");
        return false;
      }
      ops.Wait(500);
    }

    // Move to place position
    ops.LogInfo("Moving to place position: " + m_placeNode);
    if (!ops.MoveToNode(m_deviceName, m_graphName, m_placeNode, true, "CorePlace")) {
      ops.LogError("Failed to move to place position");
      return false;
    }
    ops.Wait(500);

    // Release gripper
    ops.LogInfo("Releasing gripper: " + m_gripperPinName);
    if (!ops.SetOutputByName(m_gripperOutputDevice, m_gripperPinName, false)) {
      ops.LogError("Failed to release gripper");
      return false;
    }
    ops.Wait(300);

    ops.LogInfo("CorePlace completed successfully");
    return true;
  }

  std::string GetDescription() const override {
    return "Core Place: " + m_deviceName + " -> " + m_placeNode;
  }

private:
  std::string m_deviceName;
  std::string m_graphName;
  std::string m_placeNode;
  std::string m_cameraGantry;
  std::string m_cameraViewNode;
  std::string m_gripperOutputDevice;
  std::string m_gripperPinName;
  float m_gripperHoldDelay;
  float m_speed;
  bool m_enableCameraView;
  bool m_slowDownOnApproach;
  float m_approachDistance;
  float m_speedOnApproach;
};


// Core UV Only Operation
class CoreUV : public SequenceOperation {
public:
  CoreUV(
    const std::string& deviceName,
    const std::string& graphName,
    const std::string& uvNode,
    const std::string& pneumaticUVDevice,
    const std::string& ioDevice,
    const std::string& uvTriggerPinName,
    float uvDurationSeconds,
    float speed,
    bool fineAlignmentEnable1,
    const std::string& fineAlignmentDevice1,
    const std::string& feedBackChannelName1,
    bool fineAlignmentEnable2,
    const std::string& fineAlignmentDevice2,
    const std::string& feedBackChannelName2)
    : m_deviceName(deviceName),
    m_graphName(graphName),
    m_uvNode(uvNode),
    m_pneumaticUVDevice(pneumaticUVDevice),
    m_ioDevice(ioDevice),
    m_uvTriggerPinName(uvTriggerPinName),
    m_uvDurationSeconds(uvDurationSeconds),
    m_speed(speed),
    m_fineAlignmentEnable1(fineAlignmentEnable1),
    m_fineAlignmentDevice1(fineAlignmentDevice1),
    m_feedBackChannelName1(feedBackChannelName1),
    m_fineAlignmentEnable2(fineAlignmentEnable2),
    m_fineAlignmentDevice2(fineAlignmentDevice2),
    m_feedBackChannelName2(feedBackChannelName2) {
  }

  bool Execute(MachineOperations& ops) override {
    ops.LogInfo("=== CoreUV Operation ===");
    ops.LogInfo("Device: " + m_deviceName);
    ops.LogInfo("Graph: " + m_graphName);
    ops.LogInfo("UV Node: " + m_uvNode);
    ops.LogInfo("UV Duration: " + std::to_string(m_uvDurationSeconds) + " seconds");

    // Move to UV position
    ops.LogInfo("Moving to UV position: " + m_uvNode);
    if (!ops.MoveToNode(m_deviceName, m_graphName, m_uvNode, true, "CoreUV")) {
      ops.LogError("Failed to move to UV position");
      return false;
    }
    ops.Wait(500);

    // Extend UV Head pneumatic
    ops.LogInfo("Extending UV head: " + m_pneumaticUVDevice);
    if (!ops.ExtendSlide(m_pneumaticUVDevice)) {
      ops.LogError("Failed to extend UV head");
      return false;
    }
    ops.Wait(500);

    // Fine Alignment 1 (if enabled)
    if (m_fineAlignmentEnable1) {
      ops.LogInfo("Performing fine alignment 1: " + m_fineAlignmentDevice1);
      // Perform scan with step sizes 0.0002, 0.0001 on axes Z, X, Y
      if (!ops.Runscan(m_fineAlignmentDevice1, m_feedBackChannelName1,
        { 0.0002, 0.0001 }, 300, { "Z", "X", "Y" }, "CoreUV")) {
        ops.LogError("Fine alignment 1 failed");
        return false;
      }
      ops.Wait(300);
    }

    // Fine Alignment 2 (if enabled)
    if (m_fineAlignmentEnable2) {
      ops.LogInfo("Performing fine alignment 2: " + m_fineAlignmentDevice2);
      // Perform scan with step sizes 0.0002, 0.0001 on axes Z, X, Y
      if (!ops.Runscan(m_fineAlignmentDevice2, m_feedBackChannelName2,
        { 0.0002, 0.0001 }, 300, { "Z", "X", "Y" }, "CoreUV")) {
        ops.LogError("Fine alignment 2 failed");
        return false;
      }
      ops.Wait(300);
    }

    // Trigger UV - Clear then Set (toggle pattern)
    ops.LogInfo("Triggering UV: " + m_uvTriggerPinName);
    if (!ops.SetOutputByName(m_ioDevice, m_uvTriggerPinName, false)) {
      ops.LogError("Failed to clear UV trigger");
      return false;
    }
    ops.Wait(50);

    if (!ops.SetOutputByName(m_ioDevice, m_uvTriggerPinName, true)) {
      ops.LogError("Failed to set UV trigger");
      return false;
    }
    ops.Wait(150);

    // UV curing duration
    int durationMs = static_cast<int>(m_uvDurationSeconds * 1000);
    ops.LogInfo("UV curing for " + std::to_string(m_uvDurationSeconds) + " seconds...");
    ops.Wait(durationMs);

    // Retract UV Head
    ops.LogInfo("Retracting UV head");
    if (!ops.RetractSlide(m_pneumaticUVDevice)) {
      ops.LogError("Failed to retract UV head");
      return false;
    }
    ops.Wait(500);

    ops.LogInfo("CoreUV completed successfully");
    return true;
  }

  std::string GetDescription() const override {
    return "Core UV: " + m_deviceName + " @ " + m_uvNode +
      " (" + std::to_string(m_uvDurationSeconds) + "s)";
  }

private:
  std::string m_deviceName;
  std::string m_graphName;
  std::string m_uvNode;
  std::string m_pneumaticUVDevice;
  std::string m_ioDevice;
  std::string m_uvTriggerPinName;
  float m_uvDurationSeconds;
  float m_speed;
  bool m_fineAlignmentEnable1;
  std::string m_fineAlignmentDevice1;
  std::string m_feedBackChannelName1;
  bool m_fineAlignmentEnable2;
  std::string m_fineAlignmentDevice2;
  std::string m_feedBackChannelName2;
};