// MachineOperationsExtended.cpp
#include "MachineOperationsExtended.h"
#include <iostream>
#include <sstream>

bool MachineOperationsExtended::MoveToPositionOnSystem(
  const std::string& deviceName,
  double localX, double localY, double localZ,
  const CoordinateSystem& system,
  bool waitForCompletion) {

  if (!system.IsValid()) {
    if (m_logger) {
      m_logger->LogError("MachineOperationsExtended: Invalid coordinate system '" +
        system.GetName() + "'");
    }
    return false;
  }

  // Convert local to machine coordinates
  PositionStruct localPos = { localX, localY, localZ, 0, 0, 0 };
  PositionStruct machinePos = system.TransformToMachine(localPos);

  if (m_logger) {
    std::ostringstream msg;
    msg << "MachineOperationsExtended: Moving " << deviceName
      << " to system '" << system.GetName()
      << "' position (" << localX << ", " << localY << ", " << localZ << ")";
    m_logger->LogInfo(msg.str());

    msg.str("");
    msg << "MachineOperationsExtended: Machine coordinates: ("
      << machinePos.x << ", " << machinePos.y << ", " << machinePos.z << ")";
    m_logger->LogInfo(msg.str());
  }

  // Use base class method to move
  return MoveDeviceToPosition(deviceName, machinePos, waitForCompletion);
}

bool MachineOperationsExtended::MoveRelativeOnSystem(
  const std::string& deviceName,
  double distance,
  char axis,
  const CoordinateSystem& system,
  bool waitForCompletion) {

  if (!system.IsValid()) {
    if (m_logger) {
      m_logger->LogError("MachineOperationsExtended: Invalid coordinate system");
    }
    return false;
  }

  // Get current position in system coordinates
  PositionStruct currentLocal;
  if (!GetPositionOnSystem(deviceName, system, currentLocal)) {
    return false;
  }

  // Apply relative movement
  PositionStruct targetLocal = currentLocal;
  switch (axis) {
  case 'X': case 'x':
    targetLocal.x += distance;
    break;
  case 'Y': case 'y':
    targetLocal.y += distance;
    break;
  case 'Z': case 'z':
    targetLocal.z += distance;
    break;
  default:
    if (m_logger) {
      m_logger->LogError("MachineOperationsExtended: Invalid axis: " +
        std::string(1, axis));
    }
    return false;
  }

  if (m_logger) {
    std::ostringstream msg;
    msg << "MachineOperationsExtended: Moving " << deviceName
      << " relative " << distance << "mm along system "
      << axis << "-axis";
    m_logger->LogInfo(msg.str());
  }

  return MoveToPositionOnSystem(deviceName,
    targetLocal.x, targetLocal.y, targetLocal.z,
    system, waitForCompletion);
}

bool MachineOperationsExtended::GetPositionOnSystem(
  const std::string& deviceName,
  const CoordinateSystem& system,
  PositionStruct& localPos) {

  if (!system.IsValid()) {
    if (m_logger) {
      m_logger->LogError("MachineOperationsExtended: Invalid coordinate system");
    }
    return false;
  }

  // Get current machine position
  PositionStruct machinePos;
  if (!GetDeviceCurrentPosition(deviceName, machinePos)) {
    if (m_logger) {
      m_logger->LogError("MachineOperationsExtended: Failed to get current position for " +
        deviceName);
    }
    return false;
  }

  // Transform to local coordinates
  localPos = system.TransformToLocal(machinePos);

  if (m_logger) {
    std::ostringstream msg;
    msg << "MachineOperationsExtended: " << deviceName
      << " position in system '" << system.GetName()
      << "': (" << localPos.x << ", " << localPos.y << ", " << localPos.z << ")";
    m_logger->LogInfo(msg.str());
  }

  return true;
}

bool MachineOperationsExtended::MoveToAxisPositionOnSystem(
  const std::string& deviceName,
  char axis,
  double targetValue,
  const CoordinateSystem& system,
  bool waitForCompletion) {

  // Get current position in system
  PositionStruct currentLocal;
  if (!GetPositionOnSystem(deviceName, system, currentLocal)) {
    return false;
  }

  // Update only the specified axis
  PositionStruct targetLocal = currentLocal;
  switch (axis) {
  case 'X': case 'x':
    targetLocal.x = targetValue;
    break;
  case 'Y': case 'y':
    targetLocal.y = targetValue;
    break;
  case 'Z': case 'z':
    targetLocal.z = targetValue;
    break;
  default:
    if (m_logger) {
      m_logger->LogError("MachineOperationsExtended: Invalid axis: " +
        std::string(1, axis));
    }
    return false;
  }

  return MoveToPositionOnSystem(deviceName,
    targetLocal.x, targetLocal.y, targetLocal.z,
    system, waitForCompletion);
}

bool MachineOperationsExtended::MoveToSystemOrigin(
  const std::string& deviceName,
  const CoordinateSystem& system,
  bool waitForCompletion) {

  if (m_logger) {
    m_logger->LogInfo("MachineOperationsExtended: Moving " + deviceName +
      " to origin of system '" + system.GetName() + "'");
  }

  return MoveToPositionOnSystem(deviceName, 0, 0, 0, system, waitForCompletion);
}