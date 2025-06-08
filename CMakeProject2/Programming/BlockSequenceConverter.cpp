// BlockSequenceConverter.cpp
#include "BlockSequenceConverter.h"
#include "include/ProcessBuilders.h" // For WaitOperation and SetOutputOperation
#include <algorithm>
#include <cctype>

std::unique_ptr<SequenceStep> BlockSequenceConverter::ConvertBlocksToSequence(
  const std::vector<MachineBlock*>& executionOrder,
  const std::string& sequenceName) {

  auto sequence = std::make_unique<SequenceStep>(sequenceName, m_machineOps);

  m_machineOps.LogInfo("Converting " + std::to_string(executionOrder.size()) +
    " blocks to sequence operations");

  for (const auto& block : executionOrder) {
    std::shared_ptr<SequenceOperation> operation = nullptr;

    switch (block->type) {
    case BlockType::START:
      // START blocks don't generate operations, just log
      m_machineOps.LogInfo("Starting sequence: " + GetParameterValue(*block, "program_name"));
      continue;

    case BlockType::END:
      // END blocks don't generate operations, just log
      m_machineOps.LogInfo("Ending sequence with cleanup: " +
        GetParameterValue(*block, "cleanup"));
      continue;

    case BlockType::MOVE_NODE:
      operation = ConvertMoveNodeBlock(*block);
      break;

    case BlockType::WAIT:
      operation = ConvertWaitBlock(*block);
      break;

    case BlockType::SET_OUTPUT:
      operation = ConvertSetOutputBlock(*block);
      break;

    case BlockType::CLEAR_OUTPUT:
      operation = ConvertClearOutputBlock(*block);
      break;

    default:
      m_machineOps.LogWarning("Unknown block type encountered: " +
        std::to_string(static_cast<int>(block->type)));
      continue;
    }

    if (operation) {
      sequence->AddOperation(operation);
      m_machineOps.LogInfo("Added operation: " + operation->GetDescription());
    }
  }

  return sequence;
}

std::shared_ptr<SequenceOperation> BlockSequenceConverter::ConvertMoveNodeBlock(const MachineBlock& block) {
  std::string deviceName = GetParameterValue(block, "device_name");
  std::string graphName = GetParameterValue(block, "graph_name");
  std::string nodeId = GetParameterValue(block, "node_id");

  if (deviceName.empty() || graphName.empty() || nodeId.empty()) {
    m_machineOps.LogError("MOVE_NODE block missing required parameters");
    return nullptr;
  }

  return std::make_shared<MoveToNodeOperation>(deviceName, graphName, nodeId);
}

std::shared_ptr<SequenceOperation> BlockSequenceConverter::ConvertWaitBlock(const MachineBlock& block) {
  int milliseconds = GetParameterValueAsInt(block, "milliseconds", 1000);

  if (milliseconds <= 0) {
    m_machineOps.LogWarning("Invalid wait time, using default 1000ms");
    milliseconds = 1000;
  }

  return std::make_shared<WaitOperation>(milliseconds);
}

std::shared_ptr<SequenceOperation> BlockSequenceConverter::ConvertSetOutputBlock(const MachineBlock& block) {
  std::string deviceName = GetParameterValue(block, "device_name");
  int pin = GetParameterValueAsInt(block, "pin", 0);
  bool state = GetParameterValueAsBool(block, "state", true);
  int delayMs = GetParameterValueAsInt(block, "delay_ms", 200);

  if (deviceName.empty()) {
    m_machineOps.LogError("SET_OUTPUT block missing device_name parameter");
    return nullptr;
  }

  return std::make_shared<SetOutputOperation>(deviceName, pin, state, delayMs);
}

std::shared_ptr<SequenceOperation> BlockSequenceConverter::ConvertClearOutputBlock(const MachineBlock& block) {
  std::string deviceName = GetParameterValue(block, "device_name");
  int pin = GetParameterValueAsInt(block, "pin", 0);
  int delayMs = GetParameterValueAsInt(block, "delay_ms", 200);

  if (deviceName.empty()) {
    m_machineOps.LogError("CLEAR_OUTPUT block missing device_name parameter");
    return nullptr;
  }

  // Clear output is just set output with state = false
  return std::make_shared<SetOutputOperation>(deviceName, pin, false, delayMs);
}

std::string BlockSequenceConverter::GetParameterValue(const MachineBlock& block, const std::string& paramName) {
  for (const auto& param : block.parameters) {
    if (param.name == paramName) {
      return param.value;
    }
  }
  return "";
}

int BlockSequenceConverter::GetParameterValueAsInt(const MachineBlock& block, const std::string& paramName, int defaultValue) {
  std::string value = GetParameterValue(block, paramName);
  if (value.empty()) {
    return defaultValue;
  }

  try {
    return std::stoi(value);
  }
  catch (const std::exception&) {
    m_machineOps.LogWarning("Failed to convert parameter '" + paramName + "' to int, using default");
    return defaultValue;
  }
}

bool BlockSequenceConverter::GetParameterValueAsBool(const MachineBlock& block, const std::string& paramName, bool defaultValue) {
  std::string value = GetParameterValue(block, paramName);
  if (value.empty()) {
    return defaultValue;
  }

  // Convert string to lowercase for comparison
  std::transform(value.begin(), value.end(), value.begin(), ::tolower);

  if (value == "true" || value == "1" || value == "yes" || value == "on") {
    return true;
  }
  else if (value == "false" || value == "0" || value == "no" || value == "off") {
    return false;
  }

  m_machineOps.LogWarning("Failed to convert parameter '" + paramName + "' to bool, using default");
  return defaultValue;
}