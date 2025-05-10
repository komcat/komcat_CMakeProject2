// script_executor.cpp
#include "include/script/script_executor.h"
#include <sstream>
#include <algorithm>
#include <chrono>

ScriptExecutor::ScriptExecutor(MachineOperations& machineOps)
  : m_machineOps(machineOps),
  m_state(ExecutionState::Idle),
  m_pauseRequested(false),
  m_stopRequested(false),
  m_currentLine(0),
  m_totalLines(0)
{
}

ScriptExecutor::~ScriptExecutor() {
  Stop();
}

std::string ScriptExecutor::TrimString(const std::string& str) {
  if (str.empty()) {
    return str;
  }

  // Find first non-whitespace character
  size_t start = str.find_first_not_of(" \t\r\n");

  // If the string is all whitespace
  if (start == std::string::npos) {
    return "";
  }

  // Find last non-whitespace character
  size_t end = str.find_last_not_of(" \t\r\n");

  // Return the trimmed substring
  return str.substr(start, end - start + 1);
}

bool ScriptExecutor::ExecuteScript(const std::string& script, bool startImmediately) {
  // Don't start a new execution if one is already running
  if (m_state == ExecutionState::Running || m_state == ExecutionState::Paused) {
    LogError("Cannot execute a new script while another is running");
    return false;
  }

  // Clear previous state
  m_script = script;
  m_log.clear();
  m_errors.clear();
  m_currentLine = 0;
  m_totalLines = 0;
  m_currentOperation = "";
  m_pauseRequested = false;
  m_stopRequested = false;
  m_variables.clear();

  // Count total lines (excluding comments and empty lines)
  std::istringstream stream(script);
  std::string line;
  while (std::getline(stream, line)) {
    // Use the TrimString function instead of regex_replace
    line = TrimString(line);
    if (!line.empty() && line[0] != '#') {
      m_totalLines++;
    }
  }

  // Parse the script
  try {
    m_sequence = m_parser.ParseScript(script, m_machineOps, nullptr, "UserScript");
    if (!m_sequence) {
      m_state = ExecutionState::Error;
      // Copy parser errors
      m_errors = m_parser.GetErrors();
      for (const auto& error : m_errors) {
        LogError(error);
      }

      if (m_executionCallback) {
        m_executionCallback(m_state);
      }

      return false;
    }
  }
  catch (const std::exception& e) {
    m_state = ExecutionState::Error;
    LogError(std::string("Script parsing error: ") + e.what());

    if (m_executionCallback) {
      m_executionCallback(m_state);
    }

    return false;
  }

  Log("Script parsed successfully");

  if (startImmediately) {
    Start();
  }

  return true;
}

void ScriptExecutor::Start() {
  if (m_state == ExecutionState::Running) {
    return; // Already running
  }

  if (!m_sequence) {
    LogError("No script to execute");
    return;
  }

  // Ensure we're not already executing
  Stop();

  // Update state and start execution thread
  m_state = ExecutionState::Running;
  m_pauseRequested = false;
  m_stopRequested = false;

  Log("Starting script execution");

  if (m_executionCallback) {
    m_executionCallback(m_state);
  }

  m_executionThread = std::thread(&ScriptExecutor::ExecuteScriptInternal, this);
}

void ScriptExecutor::Pause() {
  if (m_state == ExecutionState::Running) {
    m_pauseRequested = true;
    Log("Pausing script execution");
  }
}

void ScriptExecutor::Resume() {
  if (m_state == ExecutionState::Paused) {
    m_pauseRequested = false;
    m_state = ExecutionState::Running;
    Log("Resuming script execution");

    if (m_executionCallback) {
      m_executionCallback(m_state);
    }
  }
}

void ScriptExecutor::Stop() {
  if (m_state == ExecutionState::Running || m_state == ExecutionState::Paused) {
    m_stopRequested = true;
    m_pauseRequested = false; // Clear pause if set

    // Wait for thread to finish
    if (m_executionThread.joinable()) {
      m_executionThread.join();
    }

    m_state = ExecutionState::Idle;
    Log("Script execution stopped");

    if (m_executionCallback) {
      m_executionCallback(m_state);
    }
  }
}

void ScriptExecutor::ExecuteScriptInternal() {
  try {
    // Execute each operation in the sequence
    const auto& operations = m_sequence->GetOperations();

    for (size_t i = 0; i < operations.size() && !m_stopRequested; i++) {
      // Handle pause request
      while (m_pauseRequested && !m_stopRequested) {
        if (m_state != ExecutionState::Paused) {
          m_state = ExecutionState::Paused;
          if (m_executionCallback) {
            m_executionCallback(m_state);
          }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      // If we were paused and now resuming, update state
      if (m_state == ExecutionState::Paused) {
        m_state = ExecutionState::Running;
        if (m_executionCallback) {
          m_executionCallback(m_state);
        }
      }

      // Skip the rest if stopping
      if (m_stopRequested) {
        break;
      }

      // Update current line and operation
      m_currentLine = i + 1;
      m_currentOperation = operations[i]->GetDescription();

      Log("Executing: " + m_currentOperation);

      // Check for flow control operations
      auto flowOp = std::dynamic_pointer_cast<ScriptParser::FlowControlOperation>(operations[i]);
      if (flowOp) {
        ExecuteFlowControl(flowOp.get(), i);
        continue;
      }

      // Check for variable operations
      auto varOp = std::dynamic_pointer_cast<ScriptParser::VariableOperation>(operations[i]);
      if (varOp) {
        // Set the variable
        try {
          double value = EvaluateExpression(varOp->GetExpression());
          SetVariable(varOp->GetName(), value);
          Log("Set variable " + varOp->GetName() + " = " + std::to_string(value));
        }
        catch (const std::exception& e) {
          LogError("Error evaluating expression: " + std::string(e.what()));
          m_state = ExecutionState::Error;
          if (m_executionCallback) {
            m_executionCallback(m_state);
          }
          break;
        }
        continue;
      }

      // Execute the operation
      bool success = operations[i]->Execute(m_machineOps);

      if (!success) {
        LogError("Operation failed: " + m_currentOperation);
        m_state = ExecutionState::Error;

        if (m_executionCallback) {
          m_executionCallback(m_state);
        }

        break;
      }
    }

    // If we completed normally (not stopped or error)
    if (!m_stopRequested && m_state != ExecutionState::Error) {
      m_state = ExecutionState::Completed;
      Log("Script execution completed successfully");

      if (m_executionCallback) {
        m_executionCallback(m_state);
      }
    }
  }
  catch (const std::exception& e) {
    LogError(std::string("Error during script execution: ") + e.what());
    m_state = ExecutionState::Error;

    if (m_executionCallback) {
      m_executionCallback(m_state);
    }
  }
}

// Flow control execution methods
void ScriptExecutor::ExecuteFlowControl(const ScriptParser::FlowControlOperation* flowOp, size_t& index) {
  auto type = flowOp->GetType();

  switch (type) {
  case ScriptParser::FlowControlOperation::Type::If:
  {
    bool condition = EvaluateCondition(flowOp->GetCondition());
    Log("Evaluating IF condition: " + flowOp->GetCondition() + " = " +
      (condition ? "TRUE" : "FALSE"));

    if (!condition) {
      // Find matching ELSE or ENDIF
      size_t elseIndex = index;
      const auto& operations = m_sequence->GetOperations();

      // Search forward for ELSE or ENDIF
      bool foundElse = false;
      for (size_t i = index + 1; i < operations.size(); i++) {
        auto nextFlowOp = std::dynamic_pointer_cast<ScriptParser::FlowControlOperation>(operations[i]);
        if (nextFlowOp) {
          auto nextType = nextFlowOp->GetType();
          if (nextType == ScriptParser::FlowControlOperation::Type::Else) {
            elseIndex = i;
            foundElse = true;
            break;
          }
          else if (nextType == ScriptParser::FlowControlOperation::Type::EndIf) {
            elseIndex = i;
            break;
          }
        }
      }

      if (foundElse) {
        // Skip to ELSE
        index = elseIndex;
      }
      else {
        // Skip to ENDIF
        index = elseIndex;
      }
    }
    // If condition is true, just continue with the next statement
  }
  break;

  case ScriptParser::FlowControlOperation::Type::Else:
  {
    // Find matching ENDIF and skip to it
    size_t endifIndex = FindMatchingEnd(index,
      ScriptParser::FlowControlOperation::Type::If,
      ScriptParser::FlowControlOperation::Type::EndIf);

    index = endifIndex;
  }
  break;

  case ScriptParser::FlowControlOperation::Type::EndIf:
    // Nothing to do, just continue
    break;

  case ScriptParser::FlowControlOperation::Type::For:
  {
    // Find matching ENDFOR
    size_t endForIndex = FindMatchingEnd(index,
      ScriptParser::FlowControlOperation::Type::For,
      ScriptParser::FlowControlOperation::Type::EndFor);

    // Process the FOR loop
    ProcessForLoop(flowOp->GetCondition(), index, endForIndex);
  }
  break;

  case ScriptParser::FlowControlOperation::Type::EndFor:
    // EndFor is handled in ProcessForLoop
    break;

  case ScriptParser::FlowControlOperation::Type::While:
  {
    // Find matching ENDWHILE
    size_t endWhileIndex = FindMatchingEnd(index,
      ScriptParser::FlowControlOperation::Type::While,
      ScriptParser::FlowControlOperation::Type::EndWhile);

    // Process the WHILE loop
    ProcessWhileLoop(flowOp->GetCondition(), index, endWhileIndex);
  }
  break;

  case ScriptParser::FlowControlOperation::Type::EndWhile:
    // EndWhile is handled in ProcessWhileLoop
    break;
  }
}

void ScriptExecutor::ProcessForLoop(const std::string& condition, size_t& index, size_t endForIndex) {
  // Parse FOR loop parameters (variable|start|end|step)
  size_t pos1 = condition.find('|');
  size_t pos2 = condition.find('|', pos1 + 1);
  size_t pos3 = condition.find('|', pos2 + 1);

  if (pos1 == std::string::npos || pos2 == std::string::npos || pos3 == std::string::npos) {
    LogError("Invalid FOR loop format: " + condition);
    m_state = ExecutionState::Error;
    return;
  }

  std::string varName = condition.substr(0, pos1);
  std::string startExpr = condition.substr(pos1 + 1, pos2 - pos1 - 1);
  std::string endExpr = condition.substr(pos2 + 1, pos3 - pos2 - 1);
  std::string stepExpr = condition.substr(pos3 + 1);

  // Evaluate start/end/step expressions
  double start = EvaluateExpression(startExpr);
  double end = EvaluateExpression(endExpr);
  double step = EvaluateExpression(stepExpr);

  if (step == 0) {
    LogError("FOR loop step cannot be zero");
    m_state = ExecutionState::Error;
    return;
  }

  // Set initial loop variable
  SetVariable(varName, start);

  // Save the FOR statement index
  size_t forIndex = index;

  // Loop until condition is false or loop is broken
  while ((step > 0 && GetVariable(varName) <= end) ||
    (step < 0 && GetVariable(varName) >= end)) {

    // Check for stop request
    if (m_stopRequested) break;

    // Handle pause request
    while (m_pauseRequested && !m_stopRequested) {
      if (m_state != ExecutionState::Paused) {
        m_state = ExecutionState::Paused;
        if (m_executionCallback) {
          m_executionCallback(m_state);
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Log current loop iteration
    Log("FOR loop: " + varName + " = " + std::to_string(GetVariable(varName)));

    // Execute loop body (statements between FOR and ENDFOR)
    for (size_t i = forIndex + 1; i < endForIndex && !m_stopRequested; i++) {
      // Update current line and operation
      m_currentLine = i + 1;
      m_currentOperation = m_sequence->GetOperations()[i]->GetDescription();

      Log("Executing: " + m_currentOperation);

      // Handle nested flow control
      auto nestedFlowOp = std::dynamic_pointer_cast<ScriptParser::FlowControlOperation>(
        m_sequence->GetOperations()[i]);

      if (nestedFlowOp) {
        size_t tempIndex = i;
        ExecuteFlowControl(nestedFlowOp.get(), tempIndex);
        i = tempIndex;
        continue;
      }

      // Handle variable operations
      auto varOp = std::dynamic_pointer_cast<ScriptParser::VariableOperation>(
        m_sequence->GetOperations()[i]);

      if (varOp) {
        try {
          double value = EvaluateExpression(varOp->GetExpression());
          SetVariable(varOp->GetName(), value);
        }
        catch (const std::exception& e) {
          LogError("Error in variable operation: " + std::string(e.what()));
          m_state = ExecutionState::Error;
          index = endForIndex;
          return;
        }
        continue;
      }

      // Execute the operation
      bool success = m_sequence->GetOperations()[i]->Execute(m_machineOps);
      if (!success) {
        LogError("Operation failed: " + m_currentOperation);
        m_state = ExecutionState::Error;

        if (m_executionCallback) {
          m_executionCallback(m_state);
        }

        index = endForIndex;
        return;
      }
    }

    // Increment the loop counter
    SetVariable(varName, GetVariable(varName) + step);

    // If we need to stop, break out of the loop
    if (m_stopRequested || m_state == ExecutionState::Error) {
      break;
    }
  }

  // Set the index to after ENDFOR
  index = endForIndex;
}

void ScriptExecutor::ProcessWhileLoop(const std::string& condition, size_t& index, size_t endWhileIndex) {
  // Save the WHILE statement index
  size_t whileIndex = index;

  // Evaluate condition initially
  bool keepLooping = EvaluateCondition(condition);

  // Loop until condition is false or loop is broken
  while (keepLooping) {
    // Check for stop request
    if (m_stopRequested) break;

    // Handle pause request
    while (m_pauseRequested && !m_stopRequested) {
      if (m_state != ExecutionState::Paused) {
        m_state = ExecutionState::Paused;
        if (m_executionCallback) {
          m_executionCallback(m_state);
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Log current loop iteration
    Log("WHILE condition: " + condition + " = TRUE");

    // Execute loop body (statements between WHILE and ENDWHILE)
    for (size_t i = whileIndex + 1; i < endWhileIndex && !m_stopRequested; i++) {
      // Update current line and operation
      m_currentLine = i + 1;
      m_currentOperation = m_sequence->GetOperations()[i]->GetDescription();

      Log("Executing: " + m_currentOperation);

      // Handle nested flow control
      auto nestedFlowOp = std::dynamic_pointer_cast<ScriptParser::FlowControlOperation>(
        m_sequence->GetOperations()[i]);

      if (nestedFlowOp) {
        size_t tempIndex = i;
        ExecuteFlowControl(nestedFlowOp.get(), tempIndex);
        i = tempIndex;
        continue;
      }

      // Handle variable operations
      auto varOp = std::dynamic_pointer_cast<ScriptParser::VariableOperation>(
        m_sequence->GetOperations()[i]);

      if (varOp) {
        try {
          double value = EvaluateExpression(varOp->GetExpression());
          SetVariable(varOp->GetName(), value);
        }
        catch (const std::exception& e) {
          LogError("Error in variable operation: " + std::string(e.what()));
          m_state = ExecutionState::Error;
          index = endWhileIndex;
          return;
        }
        continue;
      }

      // Execute the operation
      bool success = m_sequence->GetOperations()[i]->Execute(m_machineOps);
      if (!success) {
        LogError("Operation failed: " + m_currentOperation);
        m_state = ExecutionState::Error;

        if (m_executionCallback) {
          m_executionCallback(m_state);
        }

        index = endWhileIndex;
        return;
      }
    }

    // Re-evaluate condition at the end of each loop iteration
    keepLooping = EvaluateCondition(condition);

    // If we need to stop, break out of the loop
    if (m_stopRequested || m_state == ExecutionState::Error) {
      break;
    }
  }

  // Log condition evaluation when exiting the loop
  if (!keepLooping) {
    Log("WHILE condition: " + condition + " = FALSE, exiting loop");
  }

  // Set the index to after ENDWHILE
  index = endWhileIndex;
}

size_t ScriptExecutor::FindMatchingEnd(size_t startIndex,
  ScriptParser::FlowControlOperation::Type startType,
  ScriptParser::FlowControlOperation::Type endType)
{
  const auto& operations = m_sequence->GetOperations();
  int nestLevel = 1; // Start with nest level 1 (we're inside a block)

  for (size_t i = startIndex + 1; i < operations.size(); i++) {
    auto flowOp = std::dynamic_pointer_cast<ScriptParser::FlowControlOperation>(operations[i]);
    if (!flowOp) continue;

    auto type = flowOp->GetType();

    // Found another start of the same type - increase nesting level
    if (type == startType) {
      nestLevel++;
    }
    // Found an end of our type - decrease nesting level
    else if (type == endType) {
      nestLevel--;

      // If we've found the matching end, return its index
      if (nestLevel == 0) {
        return i;
      }
    }
  }

  // Should never get here if script validation was done properly
  LogError("Failed to find matching end for control structure");
  m_state = ExecutionState::Error;
  return operations.size() - 1; // Return the last operation as a fallback
}

bool ScriptExecutor::EvaluateCondition(const std::string& condition) {
  // This is a very simplified condition evaluator
  // In a real implementation, you would need a proper expression parser

  // Trim the condition
  std::string trimmedCondition = TrimString(condition);

  // Handle basic comparison operations
  size_t posEq = trimmedCondition.find("==");
  size_t posNeq = trimmedCondition.find("!=");
  size_t posLt = trimmedCondition.find("<");
  size_t posGt = trimmedCondition.find(">");
  size_t posLte = trimmedCondition.find("<=");
  size_t posGte = trimmedCondition.find(">=");

  // Check for equality
  if (posEq != std::string::npos) {
    std::string left = TrimString(trimmedCondition.substr(0, posEq));
    std::string right = TrimString(trimmedCondition.substr(posEq + 2));

    double leftVal = EvaluateExpression(left);
    double rightVal = EvaluateExpression(right);

    return leftVal == rightVal;
  }
  // Check for inequality
  else if (posNeq != std::string::npos) {
    std::string left = TrimString(trimmedCondition.substr(0, posNeq));
    std::string right = TrimString(trimmedCondition.substr(posNeq + 2));

    double leftVal = EvaluateExpression(left);
    double rightVal = EvaluateExpression(right);

    return leftVal != rightVal;
  }
  // Check for less than or equal
  else if (posLte != std::string::npos) {
    std::string left = TrimString(trimmedCondition.substr(0, posLte));
    std::string right = TrimString(trimmedCondition.substr(posLte + 2));

    double leftVal = EvaluateExpression(left);
    double rightVal = EvaluateExpression(right);

    return leftVal <= rightVal;
  }
  // Check for greater than or equal
  else if (posGte != std::string::npos) {
    std::string left = TrimString(trimmedCondition.substr(0, posGte));
    std::string right = TrimString(trimmedCondition.substr(posGte + 2));

    double leftVal = EvaluateExpression(left);
    double rightVal = EvaluateExpression(right);

    return leftVal >= rightVal;
  }
  // Check for less than
  else if (posLt != std::string::npos) {
    std::string left = TrimString(trimmedCondition.substr(0, posLt));
    std::string right = TrimString(trimmedCondition.substr(posLt + 1));

    double leftVal = EvaluateExpression(left);
    double rightVal = EvaluateExpression(right);

    return leftVal < rightVal;
  }
  // Check for greater than
  else if (posGt != std::string::npos) {
    std::string left = TrimString(trimmedCondition.substr(0, posGt));
    std::string right = TrimString(trimmedCondition.substr(posGt + 1));

    double leftVal = EvaluateExpression(left);
    double rightVal = EvaluateExpression(right);

    return leftVal > rightVal;
  }

  // If no comparison operator found, evaluate as a boolean expression
  try {
    double result = EvaluateExpression(trimmedCondition);
    return result != 0.0; // Non-zero is true
  }
  catch (const std::exception& e) {
    LogError("Error evaluating condition: " + std::string(e.what()));
    return false;
  }
}

void ScriptExecutor::SetVariable(const std::string& name, double value) {
  m_variables[name] = value;
}

double ScriptExecutor::GetVariable(const std::string& name, double defaultValue) {
  auto it = m_variables.find(name);
  if (it != m_variables.end()) {
    return it->second;
  }
  return defaultValue;
}

double ScriptExecutor::EvaluateExpression(const std::string& expression) {
  // This is a very simplified expression evaluator
  // In a real implementation, you would need a proper expression parser

  // Try to replace variables
  std::string processedExpr = expression;
  size_t pos = 0;

  // Find all "$varname" patterns and replace with their values
  while ((pos = processedExpr.find("$", pos)) != std::string::npos) {
    // If $ is the last character, it's not a valid variable
    if (pos == processedExpr.length() - 1) break;

    // Find the end of the variable name (first non-alphanumeric character after $)
    size_t endPos = pos + 1;
    while (endPos < processedExpr.length() &&
      (isalnum(processedExpr[endPos]) || processedExpr[endPos] == '_')) {
      endPos++;
    }

    // Extract the variable name including $
    std::string varName = processedExpr.substr(pos, endPos - pos);

    // Get variable value
    double value = GetVariable(varName, 0.0);

    // Convert to string and replace
    std::string valueStr = std::to_string(value);

    // Remove trailing zeros from floating point representation
    valueStr.erase(valueStr.find_last_not_of('0') + 1, std::string::npos);
    if (valueStr.back() == '.') {
      valueStr.pop_back();
    }

    processedExpr.replace(pos, varName.length(), valueStr);

    // Move past this replacement
    pos += valueStr.length();
  }

  // For simple expressions, just evaluate directly
  // In a real implementation, you would use an expression parser library
  try {
    return std::stod(processedExpr);
  }
  catch (...) {
    throw std::runtime_error("Invalid expression: " + expression);
  }
}

void ScriptExecutor::Log(const std::string& message) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_log.push_back(message);

  if (m_logCallback) {
    m_logCallback(message);
  }
}

void ScriptExecutor::LogError(const std::string& error) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_errors.push_back(error);
  m_log.push_back("ERROR: " + error);

  if (m_logCallback) {
    m_logCallback("ERROR: " + error);
  }
}