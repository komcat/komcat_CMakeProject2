#pragma once

#include "SequenceStep.h"
#include "machine_operations.h"
#include "Programming/UserPromptUI.h"
#include <string>
#include <map>
#include <mutex>
#include <memory>
#include <functional>
#include <chrono>
#include <atomic>

/// <summary>
/// Storage manager for user input values across operations
/// Maintains a map of named inputs that persist through the sequence
/// </summary>
class UserInputStorage {
public:
  static UserInputStorage& GetInstance() {
    static UserInputStorage instance;
    return instance;
  }

  void StoreInput(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_inputs[key] = value;
    m_timestamps[key] = std::chrono::steady_clock::now();
  }

  std::string GetInput(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_inputs.find(key);
    return (it != m_inputs.end()) ? it->second : "";
  }

  bool HasInput(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_inputs.find(key) != m_inputs.end();
  }

  void ClearInput(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_inputs.erase(key);
    m_timestamps.erase(key);
  }

  void ClearAllInputs() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_inputs.clear();
    m_timestamps.clear();
  }

  std::map<std::string, std::string> GetAllInputs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_inputs;
  }

private:
  UserInputStorage() = default;
  mutable std::mutex m_mutex;
  std::map<std::string, std::string> m_inputs;
  std::map<std::string, std::chrono::steady_clock::time_point> m_timestamps;
};

/// <summary>
/// Operation that prompts user for text input and stores it persistently
/// Input remains available to all subsequent operations in the sequence
/// </summary>
class UserInputOperation : public SequenceOperation {
public:
  /// <summary>
  /// Creates a user input operation
  /// </summary>
  /// <param name="storageKey">Unique key to store/retrieve this input</param>
  /// <param name="title">Dialog title</param>
  /// <param name="prompt">Prompt message for the user</param>
  /// <param name="promptUI">Reference to UserPromptUI</param>
  /// <param name="defaultValue">Default value in input field</param>
  /// <param name="required">If true, empty input is not allowed</param>
  UserInputOperation(
    const std::string& storageKey,
    const std::string& title,
    const std::string& prompt,
    UserPromptUI& promptUI,
    const std::string& defaultValue = "",
    bool required = false,
    int timeoutSeconds = 3600)
    : m_storageKey(storageKey),
    m_title(title),
    m_prompt(prompt),
    m_promptUI(promptUI),
    m_defaultValue(defaultValue),
    m_required(required),
    m_timeoutSeconds(timeoutSeconds),
    m_completed(false) {
  }

  bool Execute(MachineOperations& ops) override;
  std::string GetDescription() const override;

private:
  std::string m_storageKey;
  std::string m_title;
  std::string m_prompt;
  UserPromptUI& m_promptUI;
  std::string m_defaultValue;
  bool m_required;
  int m_timeoutSeconds;

  // Runtime state
  std::atomic<bool> m_completed{ false };
  bool m_userConfirmed{ false };
  std::string m_inputText;
};

/// <summary>
/// Operation that uses previously stored user input
/// </summary>
class UseStoredInputOperation : public SequenceOperation {
public:
  UseStoredInputOperation(
    const std::string& storageKey,
    std::function<bool(MachineOperations&, const std::string&)> action)
    : m_storageKey(storageKey), m_action(action) {
  }

  bool Execute(MachineOperations& ops) override;
  std::string GetDescription() const override;

private:
  std::string m_storageKey;
  std::function<bool(MachineOperations&, const std::string&)> m_action;
};

/// <summary>
/// Operation to clear stored user inputs
/// </summary>
class ClearUserInputOperation : public SequenceOperation {
public:
  // Clear specific input
  explicit ClearUserInputOperation(const std::string& storageKey)
    : m_storageKey(storageKey), m_clearAll(false) {
  }

  // Clear all inputs
  ClearUserInputOperation()
    : m_clearAll(true) {
  }

  bool Execute(MachineOperations& ops) override;
  std::string GetDescription() const override;

private:
  std::string m_storageKey;
  bool m_clearAll;
};

/// <summary>
/// Operation to log all current stored inputs
/// </summary>
class LogStoredInputsOperation : public SequenceOperation {
public:
  bool Execute(MachineOperations& ops) override;
  std::string GetDescription() const override;
};