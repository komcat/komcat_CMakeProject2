// include/ops/IOperations.h
#pragma once

#include <string>
#include <vector>

class Logger;

/**
 * @brief Base interface for all operations classes
 *
 * This interface defines the minimum requirements that all *Ops classes must implement.
 * It ensures consistent lifecycle management, error handling, and device discovery
 * across all operation types (Motion, Laser, SMU, Camera, etc.)
 */
class IOperations {
public:
  virtual ~IOperations() = default;

  // === Core Lifecycle ===
  /**
   * @brief Initialize the operations class and its dependencies
   * @return true if initialization successful, false otherwise
   */
  virtual bool Initialize() = 0;

  /**
   * @brief Check if the operations class is properly initialized
   * @return true if initialized and ready for use
   */
  virtual bool IsInitialized() const = 0;

  /**
   * @brief Shutdown and cleanup the operations class
   */
  virtual void Shutdown() = 0;

  // === Error Handling ===
  /**
   * @brief Get the last error message from this operations class
   * @return String describing the last error, empty if no error
   */
  virtual std::string GetLastError() const = 0;

  // === Device Management ===
  /**
   * @brief Get list of devices this operations class can control
   * @return Vector of device names/identifiers
   */
  virtual std::vector<std::string> GetAvailableDevices() const = 0;

  /**
   * @brief Check if a specific device is connected and ready
   * @param deviceName Name of the device to check
   * @return true if device is connected and operational
   */
  virtual bool IsDeviceConnected(const std::string& deviceName) const = 0;

  // === Optional Capabilities ===
  /**
   * @brief Perform self-test to verify operations class functionality
   * @return true if self-test passes
   * @note Default implementation returns true (no test)
   */
  virtual bool SelfTest() { return true; }

  /**
   * @brief Set the logger instance for this operations class
   * @param logger Pointer to logger instance
   */
  virtual void SetLogger(Logger* logger) = 0;

  // === Operation Type Identification ===
  /**
   * @brief Get the type name of this operations class
   * @return String identifying the operation type (e.g., "Motion", "Laser", "SMU")
   */
  virtual std::string GetOperationType() const = 0;
};