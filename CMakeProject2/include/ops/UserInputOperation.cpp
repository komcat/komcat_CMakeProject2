#include "UserInputOperations.h"
#include <thread>
#include <iostream>

bool UserInputOperation::Execute(MachineOperations& ops) {
  ops.LogInfo("Requesting user input for: " + m_storageKey);

  // Check if we already have a stored value for this key
  auto& storage = UserInputStorage::GetInstance();
  std::string existingValue = storage.GetInput(m_storageKey);

  // Use existing value as default if available
  std::string actualDefault = !existingValue.empty() ? existingValue : m_defaultValue;

  // Reset state
  m_completed = false;
  m_userConfirmed = false;
  m_inputText.clear();

  // Request text input from user
  m_promptUI.RequestTextInput(m_title, m_prompt, actualDefault,
    [this](bool confirmed, const std::string& text) {
    m_userConfirmed = confirmed;
    m_inputText = text;
    m_completed = true;
  });

  // Wait for response with timeout
  auto startTime = std::chrono::steady_clock::now();
  while (!m_completed) {
    auto elapsed = std::chrono::steady_clock::now() - startTime;
    if (elapsed > std::chrono::seconds(m_timeoutSeconds)) {
      ops.LogWarning("User input timed out for: " + m_storageKey);
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // Handle user response
  if (!m_userConfirmed) {
    ops.LogInfo("User cancelled input for: " + m_storageKey);
    return false;
  }

  // Validate if required
  if (m_required && m_inputText.empty()) {
    ops.LogError("Required input was empty for: " + m_storageKey);
    return false;
  }

  // Store the input persistently
  storage.StoreInput(m_storageKey, m_inputText);

  // Log the input
  ops.LogInfo("User input stored for '" + m_storageKey + "': " + m_inputText);

  // Also store in results manager for tracking
  auto resultsManager = ops.GetResultsManager();
  if (resultsManager) {
    resultsManager->StoreResult("UserInput_" + m_storageKey, "value", m_inputText);
    resultsManager->StoreResult("UserInput_" + m_storageKey, "timestamp",
      std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
  }

  return true;
}

std::string UserInputOperation::GetDescription() const {
  std::string desc = "User input: " + m_storageKey;
  if (m_required) desc += " (required)";
  return desc;
}

bool UseStoredInputOperation::Execute(MachineOperations& ops) {
  auto& storage = UserInputStorage::GetInstance();

  if (!storage.HasInput(m_storageKey)) {
    ops.LogError("No stored input found for key: " + m_storageKey);
    return false;
  }

  std::string value = storage.GetInput(m_storageKey);
  ops.LogInfo("Using stored input '" + m_storageKey + "': " + value);

  // Execute the action with the stored value
  return m_action(ops, value);
}

std::string UseStoredInputOperation::GetDescription() const {
  return "Use stored input: " + m_storageKey;
}

bool ClearUserInputOperation::Execute(MachineOperations& ops) {
  auto& storage = UserInputStorage::GetInstance();

  if (m_clearAll) {
    storage.ClearAllInputs();
    ops.LogInfo("Cleared all stored user inputs");
  }
  else {
    storage.ClearInput(m_storageKey);
    ops.LogInfo("Cleared stored input: " + m_storageKey);
  }

  return true;
}

std::string ClearUserInputOperation::GetDescription() const {
  return m_clearAll ? "Clear all user inputs" : "Clear input: " + m_storageKey;
}

bool LogStoredInputsOperation::Execute(MachineOperations& ops) {
  auto& storage = UserInputStorage::GetInstance();
  auto inputs = storage.GetAllInputs();

  ops.LogInfo("=== Stored User Inputs ===");
  if (inputs.empty()) {
    ops.LogInfo("No stored inputs");
  }
  else {
    for (const auto& [key, value] : inputs) {
      ops.LogInfo("  " + key + " = " + value);
    }
  }
  ops.LogInfo("========================");

  return true;
}

std::string LogStoredInputsOperation::GetDescription() const {
  return "Log all stored inputs";
}