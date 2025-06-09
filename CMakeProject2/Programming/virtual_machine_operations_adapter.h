// virtual_machine_operations_adapter.h
// Simplified adapter that wraps VirtualMachineOperations with MachineOperations interface
#pragma once

#include "virtual_machine_operations.h"
#include "include/logger.h"
#include "include/motions/MotionTypes.h"  // Use proper MotionTypes.h
#include <memory>

// Forward declaration
class MachineOperations;

// Simplified Virtual Machine Operations - uses composition instead of inheritance
class VirtualMachineOperationsAdapter {
private:
  std::unique_ptr<VirtualMachineOperations> m_virtualOps;
  Logger* m_logger;

public:
  VirtualMachineOperationsAdapter()
    : m_virtualOps(std::make_unique<VirtualMachineOperations>())
  {
    m_logger = Logger::GetInstance();
    m_logger->LogInfo("VirtualMachineOperationsAdapter: Initialized with virtual operations");

    // Run a quick test to verify virtual operations work
    m_virtualOps->RunVirtualTest();
  }

  ~VirtualMachineOperationsAdapter() {
    m_logger->LogInfo("VirtualMachineOperationsAdapter: Shutting down");
  }

  // Motion control methods - delegate to virtual operations
  bool MoveDeviceToNode(const std::string& deviceName, const std::string& graphName,
    const std::string& targetNodeId, bool blocking = true) {
    m_logger->LogInfo("VirtualAdapter: Delegating MoveDeviceToNode to virtual operations");
    return m_virtualOps->MoveDeviceToNode(deviceName, graphName, targetNodeId, blocking);
  }


  bool MovePathFromTo(const std::string& deviceName, const std::string& graphName,
    const std::string& startNodeId, const std::string& endNodeId,
    bool blocking = true) {
    m_logger->LogInfo("VirtualAdapter: Delegating MovePathFromTo to virtual operations");
    return m_virtualOps->MovePathFromTo(deviceName, graphName, startNodeId, endNodeId, blocking);
  }

  bool MoveToPointName(const std::string& deviceName, const std::string& positionName, bool blocking = true) {
    m_logger->LogInfo("VirtualAdapter: Delegating MoveToPointName to virtual operations");
    return m_virtualOps->MoveToPointName(deviceName, positionName, blocking);
  }

  bool MoveRelative(const std::string& deviceName, const std::string& axis,
    double distance, bool blocking = true) {
    m_logger->LogInfo("VirtualAdapter: Delegating MoveRelative to virtual operations");
    return m_virtualOps->MoveRelative(deviceName, axis, distance, blocking);
  }

  // IO control methods
  bool SetOutput(const std::string& deviceName, int outputPin, bool state) {
    m_logger->LogInfo("VirtualAdapter: Delegating SetOutput to virtual operations");
    return m_virtualOps->SetOutput(deviceName, outputPin, state);
  }

  bool SetOutput(int deviceId, int outputPin, bool state) {
    m_logger->LogInfo("VirtualAdapter: Delegating SetOutput(deviceId) to virtual operations");
    return m_virtualOps->SetOutput(deviceId, outputPin, state);
  }

  bool ReadInput(const std::string& deviceName, int inputPin, bool& state) {
    m_logger->LogInfo("VirtualAdapter: Delegating ReadInput to virtual operations");
    return m_virtualOps->ReadInput(deviceName, inputPin, state);
  }

  bool ReadInput(int deviceId, int inputPin, bool& state) {
    m_logger->LogInfo("VirtualAdapter: Delegating ReadInput(deviceId) to virtual operations");
    return m_virtualOps->ReadInput(deviceId, inputPin, state);
  }

  // Pneumatic control methods
  bool ExtendSlide(const std::string& slideName, bool waitForCompletion = true, int timeoutMs = 5000) {
    m_logger->LogInfo("VirtualAdapter: Delegating ExtendSlide to virtual operations");
    return m_virtualOps->ExtendSlide(slideName, waitForCompletion, timeoutMs);
  }

  bool RetractSlide(const std::string& slideName, bool waitForCompletion = true, int timeoutMs = 5000) {
    m_logger->LogInfo("VirtualAdapter: Delegating RetractSlide to virtual operations");
    return m_virtualOps->RetractSlide(slideName, waitForCompletion, timeoutMs);
  }

  // Scanning methods
  bool StartScan(const std::string& deviceName, const std::string& scanProfile = "default") {
    m_logger->LogInfo("VirtualAdapter: Delegating StartScan to virtual operations");
    return m_virtualOps->StartScan(deviceName, scanProfile);
  }

  bool StopScan(const std::string& deviceName) {
    m_logger->LogInfo("VirtualAdapter: Delegating StopScan to virtual operations");
    return m_virtualOps->StopScan(deviceName);
  }

  // Camera methods
  bool InitializeCamera() {
    m_logger->LogInfo("VirtualAdapter: Delegating InitializeCamera to virtual operations");
    return m_virtualOps->InitializeCamera();
  }

  bool ConnectCamera() {
    m_logger->LogInfo("VirtualAdapter: Delegating ConnectCamera to virtual operations");
    return m_virtualOps->ConnectCamera();
  }

  bool CaptureImageToFile(const std::string& filename = "") {
    m_logger->LogInfo("VirtualAdapter: Delegating CaptureImageToFile to virtual operations");
    return m_virtualOps->CaptureImageToFile(filename);
  }

  bool StartCameraGrabbing() {
    m_logger->LogInfo("VirtualAdapter: Delegating StartCameraGrabbing to virtual operations");
    return m_virtualOps->StartCameraGrabbing();
  }

  bool StopCameraGrabbing() {
    m_logger->LogInfo("VirtualAdapter: Delegating StopCameraGrabbing to virtual operations");
    return m_virtualOps->StopCameraGrabbing();
  }

  // Status methods
  bool IsDeviceMoving(const std::string& deviceName) {
    return m_virtualOps->IsDeviceMoving(deviceName);
  }

  bool WaitForDeviceMotionCompletion(const std::string& deviceName, int timeoutMs) {
    m_logger->LogInfo("VirtualAdapter: Delegating WaitForDeviceMotionCompletion to virtual operations");
    return m_virtualOps->WaitForDeviceMotionCompletion(deviceName, timeoutMs);
  }

  // Position methods
  std::string GetDeviceCurrentNode(const std::string& deviceName, const std::string& graphName) {
    return m_virtualOps->GetDeviceCurrentNode(deviceName, graphName);
  }

  std::string GetDeviceCurrentPositionName(const std::string& deviceName) {
    return m_virtualOps->GetDeviceCurrentPositionName(deviceName);
  }

  bool GetDeviceCurrentPosition(const std::string& deviceName, PositionStruct& position) {
    return m_virtualOps->GetDeviceCurrentPosition(deviceName, position);
  }

  // Utility methods
  void Wait(int milliseconds) {
    m_virtualOps->Wait(milliseconds);
  }

  float ReadDataValue(const std::string& dataId, float defaultValue = 0.0f) {
    return m_virtualOps->ReadDataValue(dataId, defaultValue);
  }

  // Device connection checks - always return true for virtual mode
  bool IsDeviceConnected(const std::string& deviceName) const {
    m_logger->LogInfo("VirtualAdapter: Device " + deviceName + " is virtually connected");
    return true; // All devices are "connected" in virtual mode
  }

  // Camera status methods for virtual mode
  bool IsCameraInitialized() const {
    return true; // Always initialized in virtual mode
  }

  bool IsCameraConnected() const {
    return true; // Always connected in virtual mode
  }

  bool IsCameraGrabbing() const {
    return false; // Not grabbing by default
  }

  // Provide access to the underlying virtual operations for testing
  VirtualMachineOperations* GetVirtualOperations() {
    return m_virtualOps.get();
  }

  // Logging methods using virtual adapter prefix
  void LogInfo(const std::string& message) const {
    if (m_logger) {
      m_logger->LogInfo("VirtualAdapter: " + message);
    }
  }

  void LogWarning(const std::string& message) const {
    if (m_logger) {
      m_logger->LogWarning("VirtualAdapter: " + message);
    }
  }

  void LogError(const std::string& message) const {
    if (m_logger) {
      m_logger->LogError("VirtualAdapter: " + message);
    }
  }
};