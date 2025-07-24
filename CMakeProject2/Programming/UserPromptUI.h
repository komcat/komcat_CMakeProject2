// UserPromptUI.h
#pragma once

#include <imgui.h>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>

enum class PromptResult {
  PENDING,   // Waiting for user response
  YES,       // User confirmed YES
  NO,        // User confirmed NO
  CANCELLED  // User closed/cancelled
};

class UserPromptUI {
public:
  UserPromptUI();
  ~UserPromptUI();

  // Main render function
  void Render();

  // Show prompt and wait for user response
  void ShowPrompt(const std::string& title, const std::string& message,
    std::function<void(PromptResult)> callback);
  // REQUIRED: Add this method if it doesn't exist
  void RequestPrompt(const std::string& title, const std::string& message,
    std::function<void(PromptResult)> callback);

  // Control visibility
  void Show() { m_isVisible = true; }
  void Hide() { m_isVisible = false; }
  bool IsVisible() const { return m_isVisible; }

  // Check if prompt is active
  bool IsPromptActive() const { return m_isPromptActive; }

  // Get current result (thread-safe)
  PromptResult GetResult() const;

  // Reset for new prompt
  void Reset();

  // NEW: Auto-confirm functionality
  void SetAutoConfirm(bool autoConfirm) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_autoConfirm = autoConfirm;
  }
  bool GetAutoConfirm() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_autoConfirm;
  }

  // NEW: Set auto-confirm delay (in seconds)
  void SetAutoConfirmDelay(float delaySeconds) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_autoConfirmDelay = delaySeconds;
  }
  float GetAutoConfirmDelay() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_autoConfirmDelay;
  }

private:
  bool m_isVisible;
  bool m_isPromptActive;
  bool m_promptRequested;  // Flag for thread-safe requests

  std::string m_title;
  std::string m_message;
  std::atomic<PromptResult> m_result;
  std::function<void(PromptResult)> m_callback;

  // NEW: Auto-confirm members
  bool m_autoConfirm;
  float m_autoConfirmDelay;    // Delay in seconds before auto-confirm
  float m_promptStartTime;     // When the prompt was first shown
  bool m_autoConfirmTriggered; // Prevent multiple auto-confirms

  // Thread safety
  mutable std::mutex m_mutex;

  // UI styling
  void SetupPromptStyling();
  void RestoreDefaultStyling();
  void SetupPromptStylingDark();

  // Button handlers
  void OnYesClicked();
  void OnNoClicked();
  void OnCancelClicked();

  // NEW: Auto-confirm helpers
  void CheckAutoConfirm();
  float GetCurrentTime() const;
};