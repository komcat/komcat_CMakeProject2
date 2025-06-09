// virtual_machine_operations.h
// Mock implementation of MachineOperations for testing block programming
#pragma once

#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <map>

// Use the proper MotionTypes.h header
#include "include/motions/MotionTypes.h"


// Virtual Machine Operations - Mock Implementation
class VirtualMachineOperations {
private:
  std::map<std::string, PositionStruct> m_currentPositions;
  std::map<std::string, bool> m_deviceStates;
  std::map<std::string, bool> m_outputStates;
  std::map<std::string, bool> m_inputStates;
  std::map<std::string, std::string> m_slideStates; // "extended", "retracted", "moving"
  std::random_device m_rd;
  std::mt19937 m_gen;

  void log(const std::string& message) {
    std::cout << "[VIRTUAL] " << message << std::endl;
  }

public:
  VirtualMachineOperations() : m_gen(m_rd()) {
    // Initialize some default states
    m_deviceStates["scanner"] = true;
    m_deviceStates["camera"] = true;
    m_slideStates["slide1"] = "retracted";
    m_slideStates["slide2"] = "retracted";
  }

  // Motion control methods - MOCK IMPLEMENTATIONS
  bool MoveDeviceToNode(const std::string& deviceName, const std::string& graphName,
    const std::string& targetNodeId, bool blocking = true) {
    log("Moving " + deviceName + " to node " + targetNodeId + " in graph " + graphName);

    if (blocking) {
      // Simulate movement time
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Mock success/failure (95% success rate)
    bool success = (m_gen() % 100) < 95;
    log(success ? "Movement completed successfully" : "Movement failed");
    return success;
  }

  bool MovePathFromTo(const std::string& deviceName, const std::string& graphName,
    const std::string& startNodeId, const std::string& endNodeId,
    bool blocking = true) {
    log("Moving " + deviceName + " from " + startNodeId + " to " + endNodeId);

    if (blocking) {
      std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }

    bool success = (m_gen() % 100) < 92;
    log(success ? "Path movement completed" : "Path movement failed");
    return success;
  }

  bool MoveToPointName(const std::string& deviceName, const std::string& positionName, bool blocking = true) {
    log("Moving " + deviceName + " to position " + positionName);

    // Update mock position
    PositionStruct newPos;
    if (positionName == "home") {
      newPos = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    }
    else if (positionName == "scan_start") {
      newPos = { 100.0, 50.0, 25.0, 0.0, 0.0, 0.0 };
    }
    else if (positionName == "pickup") {
      newPos = { 150.0, 75.0, 10.0, 0.0, 0.0, 90.0 };
    }
    else {
      // Random position
      newPos = {
          static_cast<double>(m_gen() % 200),
          static_cast<double>(m_gen() % 200),
          static_cast<double>(m_gen() % 50),
          0.0, 0.0, 0.0
      };
    }

    m_currentPositions[deviceName] = newPos;

    if (blocking) {
      std::this_thread::sleep_for(std::chrono::milliseconds(600));
    }

    log("Moved " + deviceName + " to position (" +
      std::to_string(newPos.x) + ", " + std::to_string(newPos.y) + ", " + std::to_string(newPos.z) + ")");
    return true;
  }

  bool MoveRelative(const std::string& deviceName, const std::string& axis, double distance, bool blocking = true) {
    log("Moving " + deviceName + " relative " + std::to_string(distance) + "mm on " + axis + " axis");

    // Update current position
    if (m_currentPositions.find(deviceName) == m_currentPositions.end()) {
      m_currentPositions[deviceName] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    }

    auto& pos = m_currentPositions[deviceName];
    if (axis == "X") pos.x += distance;
    else if (axis == "Y") pos.y += distance;
    else if (axis == "Z") pos.z += distance;

    if (blocking) {
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    return true;
  }

  // IO control methods - MOCK IMPLEMENTATIONS
  bool SetOutput(const std::string& deviceName, int outputPin, bool state) {
    std::string key = deviceName + "_out_" + std::to_string(outputPin);
    m_outputStates[key] = state;
    log("Set output " + std::to_string(outputPin) + " on " + deviceName + " to " + (state ? "HIGH" : "LOW"));
    return true;
  }

  bool SetOutput(int deviceId, int outputPin, bool state) {
    return SetOutput("device_" + std::to_string(deviceId), outputPin, state);
  }

  bool ReadInput(const std::string& deviceName, int inputPin, bool& state) {
    std::string key = deviceName + "_in_" + std::to_string(inputPin);

    // Simulate random input states or use predefined ones
    if (m_inputStates.find(key) == m_inputStates.end()) {
      m_inputStates[key] = (m_gen() % 2) == 1;
    }

    state = m_inputStates[key];
    log("Read input " + std::to_string(inputPin) + " on " + deviceName + ": " + (state ? "HIGH" : "LOW"));
    return true;
  }

  bool ReadInput(int deviceId, int inputPin, bool& state) {
    return ReadInput("device_" + std::to_string(deviceId), inputPin, state);
  }

  // Pneumatic control methods - MOCK IMPLEMENTATIONS
  bool ExtendSlide(const std::string& slideName, bool waitForCompletion = true, int timeoutMs = 5000) {
    log("Extending slide " + slideName);
    m_slideStates[slideName] = "moving";

    if (waitForCompletion) {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      m_slideStates[slideName] = "extended";
      log("Slide " + slideName + " extended successfully");
    }

    return true;
  }

  bool RetractSlide(const std::string& slideName, bool waitForCompletion = true, int timeoutMs = 5000) {
    log("Retracting slide " + slideName);
    m_slideStates[slideName] = "moving";

    if (waitForCompletion) {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      m_slideStates[slideName] = "retracted";
      log("Slide " + slideName + " retracted successfully");
    }

    return true;
  }

  // Scanning methods - MOCK IMPLEMENTATIONS
  bool StartScan(const std::string& deviceName, const std::string& scanProfile = "default") {
    log("Starting scan on " + deviceName + " with profile " + scanProfile);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Simulate scan time
    log("Scan completed on " + deviceName);
    return true;
  }

  bool StopScan(const std::string& deviceName) {
    log("Stopping scan on " + deviceName);
    return true;
  }

  // Camera control methods - MOCK IMPLEMENTATIONS
  bool InitializeCamera() {
    log("Initializing camera");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    return true;
  }

  bool ConnectCamera() {
    log("Connecting to camera");
    return true;
  }

  bool CaptureImageToFile(const std::string& filename = "") {
    std::string actualFilename = filename.empty() ? "capture_" + std::to_string(m_gen() % 1000) + ".jpg" : filename;
    log("Capturing image to file: " + actualFilename);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return true;
  }

  bool StartCameraGrabbing() {
    log("Starting camera grabbing");
    return true;
  }

  bool StopCameraGrabbing() {
    log("Stopping camera grabbing");
    return true;
  }

  // Status methods - MOCK IMPLEMENTATIONS
  bool IsDeviceMoving(const std::string& deviceName) {
    // Randomly return true/false to simulate movement
    bool moving = (m_gen() % 10) == 0; // 10% chance of being in motion
    if (moving) log(deviceName + " is currently moving");
    return moving;
  }

  bool WaitForDeviceMotionCompletion(const std::string& deviceName, int timeoutMs) {
    log("Waiting for " + deviceName + " motion completion (timeout: " + std::to_string(timeoutMs) + "ms)");
    std::this_thread::sleep_for(std::chrono::milliseconds((std::min)(500, timeoutMs)));
    return true;
  }

  // Position methods - MOCK IMPLEMENTATIONS
  std::string GetDeviceCurrentNode(const std::string& deviceName, const std::string& graphName) {
    std::string nodeNames[] = { "home", "node1", "node2", "scan_pos", "pickup_pos" };
    std::string currentNode = nodeNames[m_gen() % 5];
    log(deviceName + " is currently at node: " + currentNode);
    return currentNode;
  }

  std::string GetDeviceCurrentPositionName(const std::string& deviceName) {
    std::string posNames[] = { "home", "scan_start", "pickup", "dropoff", "park" };
    std::string currentPos = posNames[m_gen() % 5];
    log(deviceName + " is currently at position: " + currentPos);
    return currentPos;
  }

  bool GetDeviceCurrentPosition(const std::string& deviceName, PositionStruct& position) {
    if (m_currentPositions.find(deviceName) != m_currentPositions.end()) {
      position = m_currentPositions[deviceName];
    }
    else {
      // Return a random position
      position = {
          static_cast<double>(m_gen() % 200),
          static_cast<double>(m_gen() % 200),
          static_cast<double>(m_gen() % 50),
          0.0, 0.0, 0.0
      };
      m_currentPositions[deviceName] = position;
    }

    log("Current position of " + deviceName + ": (" +
      std::to_string(position.x) + ", " + std::to_string(position.y) + ", " + std::to_string(position.z) + ")");
    return true;
  }

  // Utility methods - MOCK IMPLEMENTATIONS
  void Wait(int milliseconds) {
    log("Waiting for " + std::to_string(milliseconds) + " ms");
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
  }

  float ReadDataValue(const std::string& dataId, float defaultValue = 0.0f) {
    // Return mock sensor data
    float mockValue = defaultValue;
    if (dataId == "temperature") {
      mockValue = 23.5f + (static_cast<float>(m_gen() % 100) / 50.0f); // 23.5-25.5°C
    }
    else if (dataId == "pressure") {
      mockValue = 1013.25f + (static_cast<float>(m_gen() % 100) / 10.0f); // Around 1013 mbar
    }
    else if (dataId == "scan_result") {
      mockValue = static_cast<float>(m_gen() % 100) / 100.0f; // 0.0-1.0
    }
    else {
      mockValue = static_cast<float>(m_gen() % 1000) / 100.0f; // Random 0-10
    }

    log("Read data value '" + dataId + "': " + std::to_string(mockValue));
    return mockValue;
  }

  // Quick test method to verify virtual operations work
  void RunVirtualTest() {
    log("=== VIRTUAL MACHINE OPERATIONS TEST ===");

    // Test motion
    MoveToPointName("scanner", "home", true);
    MoveRelative("scanner", "X", 50.0, true);

    // Test IO
    SetOutput("scanner", 1, true);
    bool inputState;
    ReadInput("scanner", 1, inputState);

    // Test pneumatics
    ExtendSlide("slide1", true);
    RetractSlide("slide1", true);

    // Test camera
    InitializeCamera();
    CaptureImageToFile();

    // Test scanning
    StartScan("scanner", "profile1");

    // Test data
    float temp = ReadDataValue("temperature");
    float pressure = ReadDataValue("pressure");

    log("=== VIRTUAL TEST COMPLETED ===");
  }
};

// Example usage:
/*
int main() {
    VirtualMachineOperations virtualOps;

    // Run a quick test
    virtualOps.RunVirtualTest();

    // Use in your block programming tests
    virtualOps.MoveToPointName("scanner", "home");
    virtualOps.StartScan("scanner", "default");
    virtualOps.ExtendSlide("slide1");

    return 0;
}
*/