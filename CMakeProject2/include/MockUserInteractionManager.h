#pragma once

#include "ProcessBuilders.h"
#include "logger.h"
#include <mutex>
#include <condition_variable>
#include <functional>
#include <iostream>

// Mock implementation of UserInteractionManager for testing
class MockUserInteractionManager : public UserInteractionManager {
public:
  MockUserInteractionManager()
    : m_autoConfirm(false),
    m_waitingForConfirmation(false),
    m_lastMessage("")
  {
  }

  // Wait for user confirmation with a message
  bool WaitForConfirmation(const std::string& message) override {
    Logger::GetInstance()->LogInfo("UI: " + message);

    m_lastMessage = message;

    if (m_autoConfirm) {
      Logger::GetInstance()->LogInfo("UI: Auto-confirming");
      return true;
    }

    // Set up state for waiting
    m_waitingForConfirmation = true;

    // Use synchronization primitives to wait for confirmation
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this] { return !m_waitingForConfirmation; });

    // Return the result (set by ConfirmationReceived)
    return m_lastResult;
  }

  // Enable auto-confirmation for testing
  void SetAutoConfirm(bool autoConfirm) {
    m_autoConfirm = autoConfirm;
  }

  // Called by UI when user confirms or cancels
  void ConfirmationReceived(bool confirmed) {
    if (m_waitingForConfirmation) {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_lastResult = confirmed;
      m_waitingForConfirmation = false;
      m_cv.notify_one();

      Logger::GetInstance()->LogInfo("UI: User " +
        std::string(confirmed ? "confirmed" : "canceled"));
    }
  }

  // Check if waiting for confirmation
  bool IsWaitingForConfirmation() const {
    return m_waitingForConfirmation;
  }

  // Get the last message shown to user
  std::string GetLastMessage() const {
    return m_lastMessage;
  }

private:
  bool m_autoConfirm;
  bool m_waitingForConfirmation;
  bool m_lastResult;
  std::string m_lastMessage;

  std::mutex m_mutex;
  std::condition_variable m_cv;
};