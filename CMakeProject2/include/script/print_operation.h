#pragma once

#include "include/SequenceStep.h"
#include "include/machine_operations.h"
#include <string>
#include <vector>

// PrintOperation - handles the PRINT command
class PrintOperation : public SequenceOperation {
public:
  PrintOperation(const std::string& message)
    : m_message(message) {
  }

  bool Execute(MachineOperations& ops) override {
    // Don't process variables here - let the executor handle it
    ops.LogInfo("PRINT: " + m_message);

    // IMPORTANT: Don't call print handler here
    // The script executor will handle variable substitution and calling the print handler
    // This prevents duplicate messages and ensures variables are replaced correctly

    return true;
  }

  std::string GetDescription() const override {
    return "Print: " + m_message;
  }

  // Set a custom print handler
  static void SetPrintHandler(std::function<void(const std::string&)> handler) {
    m_printHandler = handler;
  }

  // Get the raw message (for executor to process)
  const std::string& GetMessage() const { return m_message; }

private:
  std::string m_message;

  // Global print handler for all PRINT operations - inline for C++17
  inline static std::function<void(const std::string&)> m_printHandler = nullptr;
};