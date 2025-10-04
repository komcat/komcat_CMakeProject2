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

// Core Pick Only Operation
class CorePick : public SequenceOperation {
public:
  CorePick(const std::string& deviceName,
    double speed,
    const std::string& pickNode,
    bool enableCameraView = false,
    const std::string& cameraGantryDevice = "",
    const std::string& cameraViewNode = "")
    : m_deviceName(deviceName)
    , m_speed(speed)
    , m_pickNode(pickNode)
    , m_enableCameraView(enableCameraView)
    , m_cameraGantryDevice(cameraGantryDevice)
    , m_cameraViewNode(cameraViewNode) {
  }

  bool Execute(MachineOperations& ops) override {
    ops.LogInfo("=== CorePick Operation ===");
    ops.LogInfo("Device: " + m_deviceName);
    ops.LogInfo("Speed: " + std::to_string(m_speed));
    ops.LogInfo("Pick Node: " + m_pickNode);

    if (m_enableCameraView) {
      ops.LogInfo("Camera View Enabled");
      ops.LogInfo("Camera Gantry: " + m_cameraGantryDevice);
      ops.LogInfo("Camera View Node: " + m_cameraViewNode);

      ops.LogInfo("Moving camera to view position...");
      ops.Wait(1000);
    }

    ops.LogInfo("Moving to pick position: " + m_pickNode);
    ops.Wait(1500);
    ops.LogInfo("Picking item...");
    ops.Wait(500);

    ops.LogInfo("CorePick operation completed successfully");


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





    return true;
  }

  std::string GetDescription() const override {
    return "Core Pick: " + m_deviceName + " @ " + m_pickNode;
  }

private:
  std::string m_deviceName;
  double m_speed;
  std::string m_pickNode;
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