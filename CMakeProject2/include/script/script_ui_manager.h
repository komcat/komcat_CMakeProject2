// script_ui_manager.h
#pragma once
#include "include/ProcessBuilders.h"
#include <mutex>
#include <condition_variable>
#include <atomic>

class ScriptUIManager : public UserInteractionManager {
public:
  ScriptUIManager() : m_waitingForConfirmation(false), m_autoConfirm(false) {}

  bool WaitForConfirmation(const std::string& message) override {
    if (m_autoConfirm) {
      return true;
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    m_lastMessage = message;
    m_waitingForConfirmation = true;

    // Wait for user response
    m_cv.wait(lock, [this] { return !m_waitingForConfirmation; });
    return m_lastResult;
  }

  void ConfirmationReceived(bool confirmed) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastResult = confirmed;
    m_waitingForConfirmation = false;
    m_cv.notify_one();
  }

  bool IsWaitingForConfirmation() const { return m_waitingForConfirmation; }
  const std::string& GetLastMessage() const { return m_lastMessage; }
  void SetAutoConfirm(bool autoConfirm) { m_autoConfirm = autoConfirm; }

private:
  std::atomic<bool> m_waitingForConfirmation;
  std::atomic<bool> m_autoConfirm;
  bool m_lastResult = false;
  std::string m_lastMessage;
  std::mutex m_mutex;
  std::condition_variable m_cv;
};