#pragma once
#include "ProcessStep.h"
#include "SequenceStep.h"
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

// Core Pick and Place Operation
class CorePickPlace : public SequenceOperation {
public:
  CorePickPlace(const std::string& deviceName,
    double speed,
    const std::string& pickNode,
    const std::string& placeNode,
    bool enableCameraView = false,
    const std::string& cameraGantryDevice = "",
    const std::string& cameraViewNode = "")
    : m_deviceName(deviceName)
    , m_speed(speed)
    , m_pickNode(pickNode)
    , m_placeNode(placeNode)
    , m_enableCameraView(enableCameraView)
    , m_cameraGantryDevice(cameraGantryDevice)
    , m_cameraViewNode(cameraViewNode) {
  }

  bool Execute(MachineOperations& ops) override {
    ops.LogInfo("=== CorePickPlace Operation ===");
    ops.LogInfo("Device: " + m_deviceName);
    ops.LogInfo("Speed: " + std::to_string(m_speed));
    ops.LogInfo("Pick Node: " + m_pickNode);
    ops.LogInfo("Place Node: " + m_placeNode);

    if (m_enableCameraView) {
      ops.LogInfo("Camera View Enabled");
      ops.LogInfo("Camera Gantry: " + m_cameraGantryDevice);
      ops.LogInfo("Camera View Node: " + m_cameraViewNode);

      // Simulate camera positioning
      ops.LogInfo("Moving camera to view position...");
      ops.Wait(1000);
    }

    // Simulate pick operation
    ops.LogInfo("Moving to pick position: " + m_pickNode);
    ops.Wait(1500);
    ops.LogInfo("Picking item...");
    ops.Wait(500);

    // Simulate place operation
    ops.LogInfo("Moving to place position: " + m_placeNode);
    ops.Wait(1500);
    ops.LogInfo("Placing item...");
    ops.Wait(500);

    ops.LogInfo("CorePickPlace operation completed successfully");
    return true;
  }

  std::string GetDescription() const override {
    return "Core Pick&Place: " + m_deviceName + " (" + m_pickNode + " -> " + m_placeNode + ")";
  }

private:
  std::string m_deviceName;
  double m_speed;
  std::string m_pickNode;
  std::string m_placeNode;
  bool m_enableCameraView;
  std::string m_cameraGantryDevice;
  std::string m_cameraViewNode;
};

// Core Place Only Operation
class CorePlace : public SequenceOperation {
public:
  CorePlace(const std::string& deviceName,
    double speed,
    const std::string& placeNode,
    bool enableCameraView = false,
    const std::string& cameraGantryDevice = "",
    const std::string& cameraViewNode = "")
    : m_deviceName(deviceName)
    , m_speed(speed)
    , m_placeNode(placeNode)
    , m_enableCameraView(enableCameraView)
    , m_cameraGantryDevice(cameraGantryDevice)
    , m_cameraViewNode(cameraViewNode) {
  }

  bool Execute(MachineOperations& ops) override {
    ops.LogInfo("=== CorePlace Operation ===");
    ops.LogInfo("Device: " + m_deviceName);
    ops.LogInfo("Speed: " + std::to_string(m_speed));
    ops.LogInfo("Place Node: " + m_placeNode);

    if (m_enableCameraView) {
      ops.LogInfo("Camera View Enabled");
      ops.LogInfo("Camera Gantry: " + m_cameraGantryDevice);
      ops.LogInfo("Camera View Node: " + m_cameraViewNode);

      ops.LogInfo("Moving camera to view position...");
      ops.Wait(1000);
    }

    ops.LogInfo("Moving to place position: " + m_placeNode);
    ops.Wait(1500);
    ops.LogInfo("Placing item...");
    ops.Wait(500);

    ops.LogInfo("CorePlace operation completed successfully");
    return true;
  }

  std::string GetDescription() const override {
    return "Core Place: " + m_deviceName + " -> " + m_placeNode;
  }

private:
  std::string m_deviceName;
  double m_speed;
  std::string m_placeNode;
  bool m_enableCameraView;
  std::string m_cameraGantryDevice;
  std::string m_cameraViewNode;
};