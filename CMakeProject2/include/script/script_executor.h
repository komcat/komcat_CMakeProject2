// script_executor.h
#pragma once

#include "include/script/script_parser.h"
#include "include/SequenceStep.h"
#include "include/machine_operations.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <map>

class ScriptExecutor {
public:
  enum class ExecutionState {
    Idle,
    Running,
    Paused,
    Completed,
    Error
  };

  // Callback types
  using ExecutionCallback = std::function<void(ExecutionState)>;
  using LogCallback = std::function<void(const std::string&)>;

  ScriptExecutor(MachineOperations& machineOps);
  ~ScriptExecutor();

  // Execute a script
  bool ExecuteScript(const std::string& script, bool startImmediately = true);

  // Control execution
  void Start();
  void Pause();
  void Resume();
  void Stop();

  // Execution status
  ExecutionState GetState() const { return m_state; }
  int GetCurrentLine() const { return m_currentLine; }
  int GetTotalLines() const { return m_totalLines; }
  float GetProgress() const { return (m_totalLines > 0) ? (float)m_currentLine / (float)m_totalLines : 0.0f; }
  const std::string& GetCurrentOperation() const { return m_currentOperation; }
  const std::vector<std::string>& GetLog() const { return m_log; }
  const std::vector<std::string>& GetErrors() const { return m_errors; }

  // Callbacks
  void SetExecutionCallback(ExecutionCallback callback) { m_executionCallback = callback; }
  void SetLogCallback(LogCallback callback) { m_logCallback = callback; }
  void SetUIManager(UserInteractionManager* uiManager) { m_uiManager = uiManager; }
private:
  // Helper method to trim whitespace from strings
  std::string TrimString(const std::string& str);

  // Execution methods for control flow
  void ExecuteScriptInternal();
  bool EvaluateCondition(const std::string& condition);
  void ExecuteFlowControl(const ScriptParser::FlowControlOperation* flowOp, size_t& index);
  void ProcessForLoop(const std::string& condition, size_t& index, size_t endForIndex);
  void ProcessWhileLoop(const std::string& condition, size_t& index, size_t endWhileIndex);

  // Find the matching end control structure
  size_t FindMatchingEnd(size_t startIndex,
    ScriptParser::FlowControlOperation::Type startType,
    ScriptParser::FlowControlOperation::Type endType);

  // Variable and procedure handling
  void SetVariable(const std::string& name, double value);
  double GetVariable(const std::string& name, double defaultValue = 0.0);
  double EvaluateExpression(const std::string& expression);

  // Logging helpers
  void Log(const std::string& message);
  void LogError(const std::string& error);

  // Execution state
  MachineOperations& m_machineOps;
  ScriptParser m_parser;
  std::unique_ptr<SequenceStep> m_sequence;
  std::string m_script;

  // Thread control
  std::thread m_executionThread;
  std::atomic<ExecutionState> m_state;
  std::atomic<bool> m_pauseRequested;
  std::atomic<bool> m_stopRequested;
  std::mutex m_mutex;

  // Execution tracking
  int m_currentLine;
  int m_totalLines;
  std::string m_currentOperation;

  // Variables
  std::map<std::string, double> m_variables;

  // Logging
  std::vector<std::string> m_log;
  std::vector<std::string> m_errors;

  // Callbacks
  ExecutionCallback m_executionCallback;
  LogCallback m_logCallback;

  UserInteractionManager* m_uiManager = nullptr;
};