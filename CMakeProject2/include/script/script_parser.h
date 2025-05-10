// script_parser.h
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stack>
#include "include/SequenceStep.h"
#include "include/machine_operations.h"

// Forward declaration
class UserInteractionManager;

class ScriptParser {
public:
  // Custom operation for flow control structures
  class FlowControlOperation : public SequenceOperation {
  public:
    enum class Type {
      If,
      Else,
      EndIf,
      For,
      EndFor,
      While,
      EndWhile
    };

    FlowControlOperation(Type type, const std::string& condition = "", int lineNumber = 0)
      : m_type(type), m_condition(condition), m_lineNumber(lineNumber) {
    }

    bool Execute(MachineOperations& ops) override;
    std::string GetDescription() const override;

    Type GetType() const { return m_type; }
    const std::string& GetCondition() const { return m_condition; }
    int GetLineNumber() const { return m_lineNumber; }

  private:
    Type m_type;
    std::string m_condition;
    int m_lineNumber;
  };

  // Custom operation for variable assignments
  class VariableOperation : public SequenceOperation {
  public:
    VariableOperation(const std::string& name, const std::string& expression)
      : m_name(name), m_expression(expression) {
    }

    bool Execute(MachineOperations& ops) override;
    std::string GetDescription() const override;

    const std::string& GetName() const { return m_name; }
    const std::string& GetExpression() const { return m_expression; }

  private:
    std::string m_name;
    std::string m_expression;
  };

  // Custom operation for procedure calls
  class ProcedureCallOperation : public SequenceOperation {
  public:
    ProcedureCallOperation(const std::string& name) : m_name(name) {}

    bool Execute(MachineOperations& ops) override;
    std::string GetDescription() const override;

    const std::string& GetName() const { return m_name; }

  private:
    std::string m_name;
  };

  ScriptParser();
  ~ScriptParser();

  // Parse full script into a sequence step
  std::unique_ptr<SequenceStep> ParseScript(
    const std::string& script,
    MachineOperations& machineOps,
    UserInteractionManager* uiManager = nullptr,
    const std::string& sequenceName = "Script");

  // Check script for syntax errors without executing
  bool ValidateScript(const std::string& script, std::vector<std::string>& errors);

  // Get error information
  bool HasErrors() const { return !m_errors.empty(); }
  const std::vector<std::string>& GetErrors() const { return m_errors; }

  // Get the processed script with procedure calls expanded
  std::string GetProcessedScript() const { return m_processedScript; }

  // Get access to procedures map
  const std::map<std::string, std::vector<std::string>>& GetProcedures() const { return m_procedures; }

private:
  // String utilities
  std::string TrimString(const std::string& str);

  // Preprocessing methods
  std::vector<std::string> PreprocessScript(const std::string& script);
  void ExtractProcedures(std::vector<std::string>& lines);

  // Tokenize a line into command and parameters
  std::vector<std::string> TokenizeLine(const std::string& line);

  // Parse a single line into an operation
  std::shared_ptr<SequenceOperation> ParseLine(const std::string& line, MachineOperations& machineOps);

  // Parse specific command types
  std::shared_ptr<SequenceOperation> ParseMoveCommand(const std::vector<std::string>& tokens, MachineOperations& machineOps);
  std::shared_ptr<SequenceOperation> ParseOutputCommand(const std::vector<std::string>& tokens, MachineOperations& machineOps);
  std::shared_ptr<SequenceOperation> ParsePneumaticCommand(const std::vector<std::string>& tokens, MachineOperations& machineOps);
  std::shared_ptr<SequenceOperation> ParseLaserCommand(const std::vector<std::string>& tokens, MachineOperations& machineOps);
  std::shared_ptr<SequenceOperation> ParseScanCommand(const std::vector<std::string>& tokens, MachineOperations& machineOps);
  std::shared_ptr<SequenceOperation> ParseUtilityCommand(const std::vector<std::string>& tokens, MachineOperations& machineOps);
  std::shared_ptr<SequenceOperation> ParseFlowControl(const std::vector<std::string>& tokens);
  std::shared_ptr<SequenceOperation> ParseVariableOperation(const std::vector<std::string>& tokens);
  std::shared_ptr<SequenceOperation> ParseProcedureCall(const std::vector<std::string>& tokens);

  // Control structure validation
  bool ValidateControlStructures(const std::vector<std::shared_ptr<SequenceOperation>>& operations);

  // Variable handling
  void SetVariable(const std::string& name, double value);
  double GetVariable(const std::string& name, double defaultValue = 0.0);
  bool IsVariable(const std::string& token) const;
  double EvaluateExpression(const std::string& expression);
  std::string ReplaceVariables(const std::string& expression);

  // Error handling
  void AddError(const std::string& error, int lineNumber);
  void ClearErrors();

  // State
  std::map<std::string, double> m_variables;
  std::map<std::string, std::vector<std::string>> m_procedures;
  std::vector<std::string> m_errors;
  int m_currentLine;
  UserInteractionManager* m_uiManager;
  std::string m_processedScript;

  // Control structure tracking
  struct ControlStructureInfo {
    FlowControlOperation::Type type;
    int lineNumber;
    std::string condition; // For loops and whiles
  };
  std::stack<ControlStructureInfo> m_controlStructures;
};