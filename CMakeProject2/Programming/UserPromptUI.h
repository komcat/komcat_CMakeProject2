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

  // Thread-safe prompt request
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

  // Auto-confirm functionality
  void SetAutoConfirm(bool autoConfirm) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_autoConfirm = autoConfirm;
  }
  bool GetAutoConfirm() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_autoConfirm;
  }

  // Set auto-confirm delay (in seconds)
  void SetAutoConfirmDelay(float delaySeconds) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_autoConfirmDelay = delaySeconds;
  }
  float GetAutoConfirmDelay() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_autoConfirmDelay;
  }

  // ===== NEW TEXT INPUT FUNCTIONALITY =====

  /// <summary>
  /// Request text input from user with callback (basic version)
  /// </summary>
  void RequestInput(const std::string& title, const std::string& message,
    const std::string& defaultValue,
    std::function<void(const std::string&, bool)> callback);

  /// <summary>
  /// Request text input from user (thread-safe version for sequences)
  /// </summary>
  void RequestTextInput(const std::string& title,
    const std::string& message,
    const std::string& defaultValue,
    std::function<void(bool, const std::string&)> callback);

  /// <summary>
  /// Check if text input is currently requested
  /// </summary>
  bool IsTextInputRequested() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_textInputRequested;
  }

  /// <summary>
  /// Get text input details for rendering
  /// </summary>
  void GetTextInputDetails(std::string& title, std::string& message,
    std::string& defaultValue, std::string& buffer) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    title = m_inputTitle;
    message = m_inputMessage;
    defaultValue = m_inputDefaultValue;
    buffer = m_inputBuffer;
  }

  /// <summary>
  /// Complete text input with result
  /// </summary>
  void CompleteTextInput(bool accepted, const std::string& value);

private:
  bool m_isVisible;
  bool m_isPromptActive;
  bool m_promptRequested;  // Flag for thread-safe requests

  std::string m_title;
  std::string m_message;
  std::atomic<PromptResult> m_result;
  std::function<void(PromptResult)> m_callback;

  // Auto-confirm members
  bool m_autoConfirm;
  float m_autoConfirmDelay;    // Delay in seconds before auto-confirm
  float m_promptStartTime;     // When the prompt was first shown
  bool m_autoConfirmTriggered; // Prevent multiple auto-confirms

  // Window sizing members
  bool m_windowSizeCalculated;  // Track if size was calculated
  ImVec2 m_calculatedWindowSize; // Store calculated size

  // Thread safety
  mutable std::mutex m_mutex;

  // ===== TEXT INPUT MEMBERS =====
  bool m_showInputPrompt = false;
  bool m_textInputRequested = false;  // Thread-safe flag for text input
  std::string m_inputTitle;
  std::string m_inputMessage;
  std::string m_inputDefaultValue;
  std::string m_inputBuffer;
  std::function<void(const std::string&, bool)> m_inputCallback;  // Original callback
  std::function<void(bool, const std::string&)> m_textInputCallback;  // New callback format

  // UI styling
  void SetupPromptStyling();
  void RestoreDefaultStyling();
  void SetupPromptStylingDark();

  // Button handlers
  void OnYesClicked();
  void OnNoClicked();
  void OnCancelClicked();

  // Auto-confirm helpers
  void CheckAutoConfirm();
  float GetCurrentTime() const;

  // Window sizing helper
  void CalculateWindowSize();

  // Text input rendering
  void RenderTextInputDialog();
};