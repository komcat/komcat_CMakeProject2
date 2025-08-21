// include/ops/BaseOperations.h
#pragma once

#include "IOperations.h"
#include "include/logger.h"
#include <string>
#include <atomic>
#include <chrono>

/**
 * @brief Base implementation class providing common functionality for all *Ops classes
 *
 * This class implements the standard parts of IOperations that are common across
 * all operation types, reducing code duplication and ensuring consistency.
 */
class BaseOperations : public IOperations {
protected:
  Logger* m_logger = nullptr;
  std::string m_lastError;
  std::atomic<bool> m_initialized{ false };
  std::chrono::steady_clock::time_point m_initTime;

public:
  BaseOperations() : m_logger(Logger::GetInstance()) {}
  virtual ~BaseOperations() = default;

  // === IOperations Implementation ===
  bool IsInitialized() const override {
    return m_initialized.load();
  }

  std::string GetLastError() const override {
    return m_lastError;
  }

  void SetLogger(Logger* logger) override {
    if (logger) {
      m_logger = logger;
    }
  }

  // === Common Logging Helpers ===
  void LogInfo(const std::string& message) const {
    if (m_logger) {
      m_logger->LogInfo(GetOperationType() + ": " + message);
    }
  }

  void LogWarning(const std::string& message) const {
    if (m_logger) {
      m_logger->LogWarning(GetOperationType() + ": " + message);
    }
  }

  void LogError(const std::string& message) const {
    if (m_logger) {
      m_logger->LogError(GetOperationType() + ": " + message);
    }
  }

  // === Status and Diagnostics ===
  /**
   * @brief Get the time when this operations class was initialized
   * @return Time point of initialization
   */
  std::chrono::steady_clock::time_point GetInitTime() const {
    return m_initTime;
  }

  /**
   * @brief Get how long this operations class has been running
   * @return Duration since initialization
   */
  std::chrono::milliseconds GetUptime() const {
    if (!m_initialized.load()) return std::chrono::milliseconds(0);
    return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - m_initTime);
  }

protected:
  // === Helper Methods for Derived Classes ===

  /**
   * @brief Set error message and log it
   * @param error Error message to set and log
   */
  void SetError(const std::string& error) {
    m_lastError = error;
    LogError(error);
  }

  /**
   * @brief Mark initialization as complete
   */
  void SetInitialized() {
    m_initTime = std::chrono::steady_clock::now();
    m_initialized.store(true);
    LogInfo("Initialized successfully");
  }

  /**
   * @brief Mark as shutdown
   */
  void SetShutdown() {
    m_initialized.store(false);
    LogInfo("Shutdown complete");
  }

  /**
   * @brief Clear the last error
   */
  void ClearError() {
    m_lastError.clear();
  }

  /**
   * @brief Check if operations class is initialized, set error if not
   * @param operation Name of operation being attempted
   * @return true if initialized
   */
  bool CheckInitialized(const std::string& operation = "") const {
    if (!m_initialized.load()) {
      std::string msg = GetOperationType() + " not initialized";
      if (!operation.empty()) {
        msg += " (attempted: " + operation + ")";
      }
      // Can't call SetError here since it's const, so just log
      if (m_logger) {
        m_logger->LogError(msg);
      }
      return false;
    }
    return true;
  }
};