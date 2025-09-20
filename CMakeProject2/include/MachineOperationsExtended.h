// MachineOperationsExtended.h
#pragma once

#include "include/machine_operations.h"
#include "include/CoordinateSystem/CoordinateSystem.h"
#include <memory>

/**
 * @brief Extended machine operations with coordinate system support
 *
 * Adds ability to work with multiple coordinate systems for movement commands
 */
class MachineOperationsExtended : public MachineOperations {
public:
  using MachineOperations::MachineOperations; // Inherit constructors

  /**
   * @brief Move device to position in specified coordinate system
   * @param deviceName Device to move (e.g., "gantry-main")
   * @param localX X position in system coordinates
   * @param localY Y position in system coordinates
   * @param localZ Z position in system coordinates
   * @param system The coordinate system to use
   * @param waitForCompletion Wait for movement to complete
   * @return True if movement command successful
   */
  bool MoveToPositionOnSystem(
    const std::string& deviceName,
    double localX, double localY, double localZ,
    const CoordinateSystem& system,
    bool waitForCompletion = true
  );

  /**
   * @brief Move relative to current position along system axis
   * @param deviceName Device to move
   * @param distance Distance to move (negative for reverse)
   * @param axis Which axis ('X', 'Y', or 'Z')
   * @param system The coordinate system to use
   * @param waitForCompletion Wait for movement to complete
   * @return True if movement command successful
   */
  bool MoveRelativeOnSystem(
    const std::string& deviceName,
    double distance,
    char axis,
    const CoordinateSystem& system,
    bool waitForCompletion = true
  );

  /**
   * @brief Get current device position in system coordinates
   * @param deviceName Device to query
   * @param system The coordinate system to use
   * @param localPos Output position in system coordinates
   * @return True if position retrieved successfully
   */
  bool GetPositionOnSystem(
    const std::string& deviceName,
    const CoordinateSystem& system,
    PositionStruct& localPos
  );

  /**
   * @brief Move along a system axis to specific coordinate
   * @param deviceName Device to move
   * @param axis Which axis ('X', 'Y', or 'Z')
   * @param targetValue Target coordinate value on that axis
   * @param system The coordinate system to use
   * @param waitForCompletion Wait for movement to complete
   * @return True if movement command successful
   */
  bool MoveToAxisPositionOnSystem(
    const std::string& deviceName,
    char axis,
    double targetValue,
    const CoordinateSystem& system,
    bool waitForCompletion = true
  );

  /**
   * @brief Move to system origin (0, 0, 0)
   */
  bool MoveToSystemOrigin(
    const std::string& deviceName,
    const CoordinateSystem& system,
    bool waitForCompletion = true
  );

private:
  Logger* m_logger = Logger::GetInstance();
};