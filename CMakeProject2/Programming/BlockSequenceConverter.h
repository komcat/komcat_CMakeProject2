// BlockSequenceConverter.h
#pragma once

#include "MachineBlockUI.h"
#include "include/SequenceStep.h"
#include "include/machine_operations.h"
#include <memory>
#include <string>

class BlockSequenceConverter {
public:
  BlockSequenceConverter(MachineOperations& machineOps)
    : m_machineOps(machineOps) {
  }

  // Convert machine blocks to executable sequence
  std::unique_ptr<SequenceStep> ConvertBlocksToSequence(
    const std::vector<MachineBlock*>& executionOrder,
    const std::string& sequenceName = "Generated Sequence");

private:
  MachineOperations& m_machineOps;

  // Convert individual block types to sequence operations
  std::shared_ptr<SequenceOperation> ConvertMoveNodeBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertWaitBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertSetOutputBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertClearOutputBlock(const MachineBlock& block);

  // Helper methods
  std::string GetParameterValue(const MachineBlock& block, const std::string& paramName);
  int GetParameterValueAsInt(const MachineBlock& block, const std::string& paramName, int defaultValue = 0);
  bool GetParameterValueAsBool(const MachineBlock& block, const std::string& paramName, bool defaultValue = false);
};