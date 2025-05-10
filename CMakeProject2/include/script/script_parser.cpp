// script_parser.cpp
#include "include/script/script_parser.h"
#include "include/ProcessBuilders.h" // For MockUserInteractionManager
#include "include/MockUserInteractionManager.h"
#include <sstream>
#include <algorithm>

ScriptParser::ScriptParser() : m_currentLine(0), m_uiManager(nullptr) {}

ScriptParser::~ScriptParser() {}

std::string ScriptParser::TrimString(const std::string& str) {
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

std::unique_ptr<SequenceStep> ScriptParser::ParseScript(
  const std::string& script,
  MachineOperations& machineOps,
  UserInteractionManager* uiManager,
  const std::string& sequenceName)
{
  // Reset state
  ClearErrors();
  m_currentLine = 0;
  m_variables.clear();
  m_procedures.clear();
  m_uiManager = uiManager;
  m_processedScript.clear();

  // Create the sequence
  auto sequence = std::make_unique<SequenceStep>(sequenceName, machineOps);

  // Step 1: Preprocess the script to extract procedures and handle includes
  std::vector<std::string> lines = PreprocessScript(script);

  // Step 2: Process each line and create operations
  std::vector<std::shared_ptr<SequenceOperation>> operations;

  for (const std::string& line : lines) {
    m_currentLine++;

    // Skip empty lines and comments
    if (line.empty() || line[0] == '#') {
      continue;
    }

    try {
      auto operation = ParseLine(line, machineOps);
      if (operation) {
        operations.push_back(operation);

        // Also build the processed script for logging/debugging
        m_processedScript += line + "\n";
      }
    }
    catch (const std::exception& e) {
      AddError(e.what(), m_currentLine);
      // Continue parsing to find more errors
    }
  }

  // Step 3: Validate control structures (ensure matching if/endif, for/endfor, etc.)
  if (!HasErrors() && !ValidateControlStructures(operations)) {
    // ValidateControlStructures should add specific errors, so we don't need to add more here
    return nullptr;
  }

  // Step 4: If no errors, add all operations to the sequence
  if (!HasErrors()) {
    for (const auto& op : operations) {
      sequence->AddOperation(op);
    }
    return sequence;
  }

  return nullptr;
}

std::vector<std::string> ScriptParser::PreprocessScript(const std::string& script) {
  std::vector<std::string> lines;
  std::istringstream stream(script);
  std::string line;

  // Read all lines
  while (std::getline(stream, line)) {
    // Trim whitespace
    line = TrimString(line);
    lines.push_back(line);
  }

  // Extract procedures into separate maps
  ExtractProcedures(lines);

  return lines;
}

void ScriptParser::ExtractProcedures(std::vector<std::string>& lines) {
  // Track if we're currently capturing a procedure
  bool inProcedure = false;
  std::string currentProcedure;
  std::vector<std::string> procedureLines;
  std::vector<size_t> linesToRemove;

  for (size_t i = 0; i < lines.size(); i++) {
    const std::string& line = lines[i];

    if (line.empty() || line[0] == '#') {
      continue; // Skip comments and empty lines
    }

    // Check for procedure definition start (without using regex)
    std::string upperLine = line;
    std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(), ::toupper);

    if (upperLine.find("DEFINE PROCEDURE") == 0) {
      // Extract procedure name
      size_t nameStart = upperLine.find_first_not_of(" \t", 17); // After "DEFINE PROCEDURE"
      if (nameStart == std::string::npos) {
        AddError("Invalid procedure definition, missing name", i + 1);
        continue;
      }

      size_t nameEnd = line.find("(", nameStart);
      if (nameEnd == std::string::npos) {
        AddError("Invalid procedure definition, missing ()", i + 1);
        continue;
      }

      if (inProcedure) {
        AddError("Nested procedure definitions are not allowed", i + 1);
        continue;
      }

      inProcedure = true;
      currentProcedure = TrimString(line.substr(nameStart, nameEnd - nameStart));
      procedureLines.clear();
      linesToRemove.push_back(i);
    }
    // Check for procedure end
    else if (line == "END") {
      if (!inProcedure) {
        AddError("END without DEFINE PROCEDURE", i + 1);
        continue;
      }

      // Store the procedure
      m_procedures[currentProcedure] = procedureLines;

      inProcedure = false;
      currentProcedure.clear();
      linesToRemove.push_back(i);
    }
    // If we're in a procedure, collect the lines
    else if (inProcedure) {
      procedureLines.push_back(line);
      linesToRemove.push_back(i);
    }
  }

  // Check if we have an unclosed procedure
  if (inProcedure) {
    AddError("Unclosed procedure: " + currentProcedure, lines.size());
  }

  // Remove procedure definitions from the main script
  // Remove from back to front to avoid shifting indices
  std::sort(linesToRemove.begin(), linesToRemove.end(), std::greater<size_t>());
  for (size_t idx : linesToRemove) {
    lines.erase(lines.begin() + idx);
  }
}

bool ScriptParser::ValidateControlStructures(const std::vector<std::shared_ptr<SequenceOperation>>& operations) {
  std::stack<ControlStructureInfo> controlStack;
  std::map<FlowControlOperation::Type, std::string> typeNames = {
      {FlowControlOperation::Type::If, "IF"},
      {FlowControlOperation::Type::For, "FOR"},
      {FlowControlOperation::Type::While, "WHILE"}
  };

  for (size_t i = 0; i < operations.size(); i++) {
    auto flowOp = std::dynamic_pointer_cast<FlowControlOperation>(operations[i]);
    if (!flowOp) {
      continue; // Not a flow control operation
    }

    FlowControlOperation::Type type = flowOp->GetType();

    // Opening control structures
    if (type == FlowControlOperation::Type::If ||
      type == FlowControlOperation::Type::For ||
      type == FlowControlOperation::Type::While) {
      ControlStructureInfo info = {
          type,
          flowOp->GetLineNumber(),
          flowOp->GetCondition()
      };
      controlStack.push(info);
    }
    // Else needs a matching If
    else if (type == FlowControlOperation::Type::Else) {
      if (controlStack.empty() || controlStack.top().type != FlowControlOperation::Type::If) {
        AddError("ELSE without matching IF", flowOp->GetLineNumber());
        return false;
      }
      // No change to stack, Else is part of the same If structure
    }
    // Closing control structures
    else if (type == FlowControlOperation::Type::EndIf) {
      if (controlStack.empty() || controlStack.top().type != FlowControlOperation::Type::If) {
        AddError("ENDIF without matching IF", flowOp->GetLineNumber());
        return false;
      }
      controlStack.pop();
    }
    else if (type == FlowControlOperation::Type::EndFor) {
      if (controlStack.empty() || controlStack.top().type != FlowControlOperation::Type::For) {
        AddError("ENDFOR without matching FOR", flowOp->GetLineNumber());
        return false;
      }
      controlStack.pop();
    }
    else if (type == FlowControlOperation::Type::EndWhile) {
      if (controlStack.empty() || controlStack.top().type != FlowControlOperation::Type::While) {
        AddError("ENDWHILE without matching WHILE", flowOp->GetLineNumber());
        return false;
      }
      controlStack.pop();
    }
  }

  // Check if all control structures were closed
  if (!controlStack.empty()) {
    FlowControlOperation::Type unclosedType = controlStack.top().type;
    AddError("Unclosed " + typeNames[unclosedType] + " statement", operations.size());
    return false;
  }

  return true;
}

std::vector<std::string> ScriptParser::TokenizeLine(const std::string& line) {
  std::vector<std::string> tokens;
  std::string token;
  bool inQuotes = false;

  for (char c : line) {
    if (c == '"') {
      inQuotes = !inQuotes;
      token += c;
    }
    else if (c == ' ' && !inQuotes) {
      if (!token.empty()) {
        tokens.push_back(token);
        token.clear();
      }
    }
    else {
      token += c;
    }
  }

  if (!token.empty()) {
    tokens.push_back(token);
  }

  return tokens;
}

std::shared_ptr<SequenceOperation> ScriptParser::ParseLine(const std::string& line, MachineOperations& machineOps) {
  auto tokens = TokenizeLine(line);
  if (tokens.empty()) {
    return nullptr;
  }

  std::string command = tokens[0];
  std::transform(command.begin(), command.end(), command.begin(), ::toupper);

  // Check for flow control statements
  if (command == "IF" || command == "ELSE" || command == "ENDIF" ||
    command == "FOR" || command == "ENDFOR" ||
    command == "WHILE" || command == "ENDWHILE") {
    return ParseFlowControl(tokens);
  }
  // Check for procedure calls
  else if (command == "CALL") {
    return ParseProcedureCall(tokens);
  }
  // Handle variable operations
  else if (command == "SET") {
    return ParseVariableOperation(tokens);
  }
  // Handle existing command types
  else if (command == "MOVE" || command == "MOVE_TO_POINT" || command == "MOVE_RELATIVE") {
    return ParseMoveCommand(tokens, machineOps);
  }
  else if (command == "SET_OUTPUT" || command == "READ_INPUT" || command == "CLEAR_LATCH") {
    return ParseOutputCommand(tokens, machineOps);
  }
  else if (command == "EXTEND_SLIDE" || command == "RETRACT_SLIDE") {
    return ParsePneumaticCommand(tokens, machineOps);
  }
  else if (command == "LASER_ON" || command == "LASER_OFF" || command == "TEC_ON" ||
    command == "TEC_OFF" || command == "SET_LASER_CURRENT" ||
    command == "SET_TEC_TEMPERATURE" || command == "WAIT_FOR_TEMPERATURE") {
    return ParseLaserCommand(tokens, machineOps);
  }
  else if (command == "RUN_SCAN") {
    return ParseScanCommand(tokens, machineOps);
  }
  else if (command == "WAIT" || command == "PROMPT" || command == "PRINT") {
    return ParseUtilityCommand(tokens, machineOps);
  }

  throw std::runtime_error("Unknown command: " + tokens[0]);
}

// Flow control parsing implementation
std::shared_ptr<SequenceOperation> ScriptParser::ParseFlowControl(const std::vector<std::string>& tokens) {
  std::string command = tokens[0];
  std::transform(command.begin(), command.end(), command.begin(), ::toupper);

  if (command == "IF") {
    if (tokens.size() < 2) {
      throw std::runtime_error("IF statement requires a condition");
    }

    // Combine tokens after IF as the condition
    std::string condition;
    for (size_t i = 1; i < tokens.size(); ++i) {
      if (i > 1) condition += " ";
      condition += tokens[i];
    }

    return std::make_shared<FlowControlOperation>(
      FlowControlOperation::Type::If, condition, m_currentLine);
  }
  else if (command == "ELSE") {
    return std::make_shared<FlowControlOperation>(
      FlowControlOperation::Type::Else, "", m_currentLine);
  }
  else if (command == "ENDIF") {
    return std::make_shared<FlowControlOperation>(
      FlowControlOperation::Type::EndIf, "", m_currentLine);
  }
  else if (command == "FOR") {
    if (tokens.size() < 6 || tokens[2] != "=" || tokens[4] != "TO") {
      throw std::runtime_error("Invalid FOR syntax. Expected: FOR $var = start TO end");
    }

    std::string variable = tokens[1];
    std::string startExpr = tokens[3];
    std::string endExpr = tokens[5];
    std::string stepExpr = (tokens.size() > 7 && tokens[6] == "STEP") ? tokens[7] : "1";

    // Format: "variable|start|end|step"
    std::string condition = variable + "|" + startExpr + "|" + endExpr + "|" + stepExpr;

    return std::make_shared<FlowControlOperation>(
      FlowControlOperation::Type::For, condition, m_currentLine);
  }
  else if (command == "ENDFOR") {
    return std::make_shared<FlowControlOperation>(
      FlowControlOperation::Type::EndFor, "", m_currentLine);
  }
  else if (command == "WHILE") {
    if (tokens.size() < 2) {
      throw std::runtime_error("WHILE statement requires a condition");
    }

    // Combine tokens after WHILE as the condition
    std::string condition;
    for (size_t i = 1; i < tokens.size(); ++i) {
      if (i > 1) condition += " ";
      condition += tokens[i];
    }

    return std::make_shared<FlowControlOperation>(
      FlowControlOperation::Type::While, condition, m_currentLine);
  }
  else if (command == "ENDWHILE") {
    return std::make_shared<FlowControlOperation>(
      FlowControlOperation::Type::EndWhile, "", m_currentLine);
  }

  throw std::runtime_error("Unknown flow control command: " + command);
}

std::shared_ptr<SequenceOperation> ScriptParser::ParseVariableOperation(const std::vector<std::string>& tokens) {
  if (tokens.size() < 4 || tokens[2] != "=") {
    throw std::runtime_error("Invalid variable assignment. Expected: SET $variable = expression");
  }

  std::string varName = tokens[1];
  if (!IsVariable(varName)) {
    throw std::runtime_error("Variable name must start with $: " + varName);
  }

  // Combine the remaining tokens as the expression
  std::string expression;
  for (size_t i = 3; i < tokens.size(); i++) {
    if (i > 3) expression += " ";
    expression += tokens[i];
  }

  return std::make_shared<VariableOperation>(varName, expression);
}

std::shared_ptr<SequenceOperation> ScriptParser::ParseProcedureCall(const std::vector<std::string>& tokens) {
  if (tokens.size() < 2) {
    throw std::runtime_error("Invalid procedure call. Expected: CALL procedureName()");
  }

  // Extract procedure name, removing the ()
  std::string procName = tokens[1];
  if (procName.find("(") != std::string::npos) {
    procName = procName.substr(0, procName.find("("));
  }

  // Check if the procedure exists
  if (m_procedures.find(procName) == m_procedures.end()) {
    throw std::runtime_error("Procedure not defined: " + procName);
  }

  return std::make_shared<ProcedureCallOperation>(procName);
}

// Example implementation for movement commands
std::shared_ptr<SequenceOperation> ScriptParser::ParseMoveCommand(const std::vector<std::string>& tokens, MachineOperations& machineOps) {
  std::string command = tokens[0];
  std::transform(command.begin(), command.end(), command.begin(), ::toupper);

  if (command == "MOVE") {
    if (tokens.size() < 6) {
      throw std::runtime_error("Invalid MOVE command syntax. Expected: MOVE <device> TO <node> IN <graph>");
    }

    std::string deviceName = tokens[1];
    if (tokens[2] != "TO") {
      throw std::runtime_error("Expected 'TO' in MOVE command");
    }

    std::string nodeId = tokens[3];

    if (tokens[4] != "IN") {
      throw std::runtime_error("Expected 'IN' in MOVE command");
    }

    std::string graphName = tokens[5];

    return std::make_shared<MoveToNodeOperation>(deviceName, graphName, nodeId);
  }
  else if (command == "MOVE_TO_POINT") {
    if (tokens.size() < 3) {
      throw std::runtime_error("Invalid MOVE_TO_POINT command syntax. Expected: MOVE_TO_POINT <device> <position>");
    }

    std::string deviceName = tokens[1];
    std::string positionName = tokens[2];

    return std::make_shared<MoveToPointNameOperation>(deviceName, positionName);
  }

  throw std::runtime_error("Unrecognized move command: " + command);
}

// Example implementation for laser commands
std::shared_ptr<SequenceOperation> ScriptParser::ParseLaserCommand(const std::vector<std::string>& tokens, MachineOperations& machineOps) {
  std::string command = tokens[0];
  std::transform(command.begin(), command.end(), command.begin(), ::toupper);

  if (command == "LASER_ON") {
    std::string laserName = (tokens.size() > 1) ? tokens[1] : "";
    return std::make_shared<LaserOnOperation>(laserName);
  }
  else if (command == "LASER_OFF") {
    std::string laserName = (tokens.size() > 1) ? tokens[1] : "";
    return std::make_shared<LaserOffOperation>(laserName);
  }
  else if (command == "SET_LASER_CURRENT") {
    if (tokens.size() < 2) {
      throw std::runtime_error("Invalid SET_LASER_CURRENT command syntax. Expected: SET_LASER_CURRENT <current> [laser_name]");
    }

    float current = std::stof(tokens[1]);
    std::string laserName = (tokens.size() > 2) ? tokens[2] : "";

    return std::make_shared<SetLaserCurrentOperation>(current, laserName);
  }

  throw std::runtime_error("Unrecognized laser command: " + command);
}

// Utility commands implementation
std::shared_ptr<SequenceOperation> ScriptParser::ParseUtilityCommand(const std::vector<std::string>& tokens, MachineOperations& machineOps) {
  std::string command = tokens[0];
  std::transform(command.begin(), command.end(), command.begin(), ::toupper);

  if (command == "WAIT") {
    if (tokens.size() < 2) {
      throw std::runtime_error("Invalid WAIT command syntax. Expected: WAIT <milliseconds>");
    }

    int milliseconds = std::stoi(tokens[1]);
    return std::make_shared<WaitOperation>(milliseconds);
  }
  else if (command == "PROMPT") {
    if (tokens.size() < 2) {
      throw std::runtime_error("Invalid PROMPT command syntax. Expected: PROMPT <message>");
    }

    // Combine remaining tokens as the message
    std::string message;
    for (size_t i = 1; i < tokens.size(); i++) {
      if (i > 1) message += " ";
      message += tokens[i];
    }

    // Use the provided UI manager or create a mock one if none was provided
    if (m_uiManager) {
      return std::make_shared<UserConfirmOperation>(message, *m_uiManager);
    }
    else {
      static MockUserInteractionManager mockUiManager;
      return std::make_shared<UserConfirmOperation>(message, mockUiManager);
    }
  }

  throw std::runtime_error("Unrecognized utility command: " + command);
}

void ScriptParser::AddError(const std::string& error, int lineNumber) {
  std::stringstream ss;
  ss << "Line " << lineNumber << ": " << error;
  m_errors.push_back(ss.str());
}

void ScriptParser::ClearErrors() {
  m_errors.clear();
}

void ScriptParser::SetVariable(const std::string& name, double value) {
  m_variables[name] = value;
}

double ScriptParser::GetVariable(const std::string& name, double defaultValue) {
  auto it = m_variables.find(name);
  if (it != m_variables.end()) {
    return it->second;
  }
  return defaultValue;
}

bool ScriptParser::IsVariable(const std::string& token) const {
  return !token.empty() && token[0] == '$';
}

// FlowControlOperation implementation
bool ScriptParser::FlowControlOperation::Execute(MachineOperations& ops) {
  // Flow control operations don't directly do anything during execution
  // They're handled by the script executor's control flow logic
  return true;
}

std::string ScriptParser::FlowControlOperation::GetDescription() const {
  switch (m_type) {
  case Type::If:
    return "If " + m_condition;
  case Type::Else:
    return "Else";
  case Type::EndIf:
    return "EndIf";
  case Type::For:
    return "For " + m_condition;
  case Type::EndFor:
    return "EndFor";
  case Type::While:
    return "While " + m_condition;
  case Type::EndWhile:
    return "EndWhile";
  default:
    return "Unknown flow control";
  }
}

// VariableOperation implementation
bool ScriptParser::VariableOperation::Execute(MachineOperations& ops) {
  // Variable operations typically update a variable value
  // In a real implementation, this would modify the script executor's variable state
  ops.LogInfo("Setting variable " + m_name + " = " + m_expression);
  return true;
}

std::string ScriptParser::VariableOperation::GetDescription() const {
  return "Set " + m_name + " = " + m_expression;
}

// ProcedureCallOperation implementation
bool ScriptParser::ProcedureCallOperation::Execute(MachineOperations& ops) {
  // The actual execution will be handled by expanding the procedure in the script executor
  ops.LogInfo("Calling procedure: " + m_name);
  return true;
}

std::string ScriptParser::ProcedureCallOperation::GetDescription() const {
  return "Call procedure: " + m_name;
}

// Simple expression evaluation - would need to be expanded for real use
double ScriptParser::EvaluateExpression(const std::string& expression) {
  // Just a placeholder for a more sophisticated expression evaluator
  try {
    return std::stod(expression);
  }
  catch (...) {
    throw std::runtime_error("Invalid expression: " + expression);
  }
}

std::string ScriptParser::ReplaceVariables(const std::string& expression) {
  // A simple variable replacement - in a real implementation,
  // you'd want to use a proper expression parser
  std::string result = expression;
  size_t pos = 0;

  // Find all "$varname" patterns and replace them with their values
  while ((pos = result.find("$", pos)) != std::string::npos) {
    // If $ is the last character, it's not a valid variable
    if (pos == result.length() - 1) break;

    // Find the end of the variable name (first non-alphanumeric character after $)
    size_t endPos = pos + 1;
    while (endPos < result.length() &&
      (isalnum(result[endPos]) || result[endPos] == '_')) {
      endPos++;
    }

    // Extract the variable name including $
    std::string varName = result.substr(pos, endPos - pos);

    // Get variable value
    double value = GetVariable(varName, 0.0);

    // Convert to string and replace
    std::string valueStr = std::to_string(value);

    // Remove trailing zeros from floating point representation
    valueStr.erase(valueStr.find_last_not_of('0') + 1, std::string::npos);
    if (valueStr.back() == '.') {
      valueStr.pop_back();
    }

    result.replace(pos, varName.length(), valueStr);

    // Move past this replacement
    pos += valueStr.length();
  }

  return result;
}

// Add these stub implementations for ParseOutputCommand, ParsePneumaticCommand,
// and ParseScanCommand until you implement them
std::shared_ptr<SequenceOperation> ScriptParser::ParseOutputCommand(
  const std::vector<std::string>& tokens, MachineOperations& machineOps)
{
  std::string command = tokens[0];
  std::transform(command.begin(), command.end(), command.begin(), ::toupper);

  if (command == "SET_OUTPUT") {
    if (tokens.size() < 4) {
      throw std::runtime_error("Invalid SET_OUTPUT syntax. Expected: SET_OUTPUT <device> <pin> <ON|OFF>");
    }

    std::string device = tokens[1];
    int pin = std::stoi(tokens[2]);

    std::string stateStr = tokens[3];
    std::transform(stateStr.begin(), stateStr.end(), stateStr.begin(), ::toupper);
    bool state = (stateStr == "ON" || stateStr == "TRUE" || stateStr == "1");

    int delay = (tokens.size() > 4) ? std::stoi(tokens[4]) : 200;

    return std::make_shared<SetOutputOperation>(device, pin, state, delay);
  }

  // Add implementations for READ_INPUT and CLEAR_LATCH

  throw std::runtime_error("Unrecognized output command: " + command);
}

std::shared_ptr<SequenceOperation> ScriptParser::ParsePneumaticCommand(
  const std::vector<std::string>& tokens, MachineOperations& machineOps)
{
  std::string command = tokens[0];
  std::transform(command.begin(), command.end(), command.begin(), ::toupper);

  if (command == "EXTEND_SLIDE") {
    if (tokens.size() < 2) {
      throw std::runtime_error("Invalid EXTEND_SLIDE syntax. Expected: EXTEND_SLIDE <slide_name>");
    }

    std::string slideName = tokens[1];
    return std::make_shared<ExtendSlideOperation>(slideName);
  }
  else if (command == "RETRACT_SLIDE") {
    if (tokens.size() < 2) {
      throw std::runtime_error("Invalid RETRACT_SLIDE syntax. Expected: RETRACT_SLIDE <slide_name>");
    }

    std::string slideName = tokens[1];
    return std::make_shared<RetractSlideOperation>(slideName);
  }

  throw std::runtime_error("Unrecognized pneumatic command: " + command);
}

std::shared_ptr<SequenceOperation> ScriptParser::ParseScanCommand(
  const std::vector<std::string>& tokens, MachineOperations& machineOps)
{
  std::string command = tokens[0];
  std::transform(command.begin(), command.end(), command.begin(), ::toupper);

  if (command == "RUN_SCAN") {
    if (tokens.size() < 4) {
      throw std::runtime_error("Invalid RUN_SCAN syntax. Expected: RUN_SCAN <device> <channel> <step_sizes>");
    }

    std::string device = tokens[1];
    std::string channel = tokens[2];

    // Parse step sizes (comma-separated)
    std::string stepsStr = tokens[3];
    std::vector<double> steps;

    // Simple comma-separated parsing
    size_t start = 0;
    size_t end = stepsStr.find(',');
    while (end != std::string::npos) {
      steps.push_back(std::stod(stepsStr.substr(start, end - start)));
      start = end + 1;
      end = stepsStr.find(',', start);
    }
    steps.push_back(std::stod(stepsStr.substr(start)));

    // Default settling time
    int settlingTime = (tokens.size() > 4) ? std::stoi(tokens[4]) : 300;

    // Default axes
    std::vector<std::string> axes = { "Z", "X", "Y" };

    return std::make_shared<RunScanOperation>(device, channel, steps, settlingTime, axes);
  }

  throw std::runtime_error("Unrecognized scan command: " + command);
}

bool ScriptParser::ValidateScript(const std::string& script, std::vector<std::string>& errors) {
  // Parse the script, but don't execute it
  MockUserInteractionManager dummyUiManager;
  MachineOperations* dummyOps = nullptr; // We won't execute, so this is ok

  ClearErrors();

  // Preprocess the script
  try {
    std::vector<std::string> lines = PreprocessScript(script);

    // Process each line
    m_currentLine = 0;
    std::vector<std::shared_ptr<SequenceOperation>> operations;

    for (const std::string& line : lines) {
      m_currentLine++;

      // Skip empty lines and comments
      if (line.empty() || line[0] == '#') {
        continue;
      }

      try {
        // We'll parse but not create actual operations
        TokenizeLine(line);
        // Just check if we can tokenize each line
      }
      catch (const std::exception& e) {
        AddError(e.what(), m_currentLine);
      }
    }

    // Check control structures
    if (!ValidateControlStructures(operations)) {
      // Errors were added by ValidateControlStructures
    }
  }
  catch (const std::exception& e) {
    AddError(e.what(), m_currentLine);
  }

  // Copy the errors
  errors = m_errors;

  // Return true if validation succeeded (no errors)
  return !HasErrors();
}