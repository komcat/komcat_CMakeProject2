// BlockSequenceConverter.h
#pragma once

#include "MachineBlockUI.h"
#include "include/SequenceStep.h"
#include "include/machine_operations.h"
#include <memory>
#include <string>
#include <functional>
#include "UserPromptUI.h"
class BlockSequenceConverter {
public:
  BlockSequenceConverter(MachineOperations& machineOps, UserPromptUI* promptUI = nullptr)
    : m_machineOps(machineOps), m_promptUI(promptUI) {
  }

  // NEW: Progress callback type for real-time feedback
  using ProgressCallback = std::function<void(int blockId, const std::string& blockName,
    const std::string& status, const std::string& details)>;

  // NEW: Set progress callback for real-time block execution feedback
  void SetProgressCallback(ProgressCallback callback) { m_progressCallback = callback; }

  // Convert machine blocks to executable sequence
  std::unique_ptr<SequenceStep> ConvertBlocksToSequence(
    const std::vector<MachineBlock*>& executionOrder,
    const std::string& sequenceName = "Generated Sequence");

private:
  MachineOperations& m_machineOps;
  UserPromptUI* m_promptUI;  // NEW: Reference to prompt UI

  
  // NEW: Progress callback member
  ProgressCallback m_progressCallback;

  // Convert individual block types to sequence operations
  std::shared_ptr<SequenceOperation> ConvertMoveNodeBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertWaitBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertSetOutputBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertClearOutputBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertExtendSlideBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertRetractSlideBlock(const MachineBlock& block);
  // NEW: Convert laser and TEC block types to sequence operations
  std::shared_ptr<SequenceOperation> ConvertSetLaserCurrentBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertLaserOnBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertLaserOffBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertSetTECTemperatureBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertTECOnBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertTECOffBlock(const MachineBlock& block);
  // NEW: Convert prompt blocks
  std::shared_ptr<SequenceOperation> ConvertPromptBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertMoveToPositionBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertMoveRelativeAxisBlock(const MachineBlock& block);
  // NEW: Keithley converter method declarations
  std::shared_ptr<SequenceOperation> ConvertKeithleyResetBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertKeithleySetOutputBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertKeithleyVoltageSourceBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertKeithleyCurrentSourceBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertKeithleyReadVoltageBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertKeithleyReadCurrentBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertKeithleyReadResistanceBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertKeithleySendCommandBlock(const MachineBlock& block);

  // Helper methods
  std::string GetParameterValue(const MachineBlock& block, const std::string& paramName);
  int GetParameterValueAsInt(const MachineBlock& block, const std::string& paramName, int defaultValue = 0);
  bool GetParameterValueAsBool(const MachineBlock& block, const std::string& paramName, bool defaultValue = false);


};