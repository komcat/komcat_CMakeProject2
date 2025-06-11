// BlockSequenceConverter.cpp
#include "BlockSequenceConverter.h"
#include "include/ProcessBuilders.h" // For WaitOperation and SetOutputOperation
#include <algorithm>
#include <cctype>

// ===================================================================
// Progress Tracking Operation - Internal implementation for real-time feedback
// ===================================================================
class ProgressTrackingOperation : public SequenceOperation {
public:
  ProgressTrackingOperation(
    std::shared_ptr<SequenceOperation> wrappedOperation,
    int blockId,
    const std::string& blockName,
    std::function<void(int, const std::string&, const std::string&, const std::string&)> progressCallback)
    : m_wrappedOperation(wrappedOperation)
    , m_blockId(blockId)
    , m_blockName(blockName)
    , m_progressCallback(progressCallback) {
  }

  bool Execute(MachineOperations& ops) override {
    // Notify execution start
    if (m_progressCallback) {
      m_progressCallback(m_blockId, m_blockName, "Processing",
        "Hardware executing: " + m_blockName);
    }

    // Execute the actual hardware operation
    bool success = m_wrappedOperation->Execute(ops);

    // Notify execution completion
    if (m_progressCallback) {
      std::string status = success ? "Complete" : "Failed";
      std::string details = success ? "Hardware operation completed successfully" : "Hardware operation failed";
      m_progressCallback(m_blockId, m_blockName, status, details);
    }

    return success;
  }

  std::string GetDescription() const override {
    return m_wrappedOperation->GetDescription();
  }

private:
  std::shared_ptr<SequenceOperation> m_wrappedOperation;
  int m_blockId;
  std::string m_blockName;
  std::function<void(int, const std::string&, const std::string&, const std::string&)> m_progressCallback;
};

// ===================================================================
// Main conversion method with real-time progress tracking
// ===================================================================
std::unique_ptr<SequenceStep> BlockSequenceConverter::ConvertBlocksToSequence(
  const std::vector<MachineBlock*>& executionOrder,
  const std::string& sequenceName) {

  auto sequence = std::make_unique<SequenceStep>(sequenceName, m_machineOps);

  m_machineOps.LogInfo("Converting " + std::to_string(executionOrder.size()) +
    " blocks to sequence operations with real-time feedback");

  for (const auto& block : executionOrder) {
    std::shared_ptr<SequenceOperation> operation = nullptr;

    switch (block->type) {
    case BlockType::START:
      // START blocks don't generate hardware operations, just log
      m_machineOps.LogInfo("Starting sequence: " + GetParameterValue(*block, "program_name"));

      // Notify progress callback that START block is processed
      if (m_progressCallback) {
        m_progressCallback(block->id, block->label, "Complete", "START block processed - program starting");
      }
      continue;

    case BlockType::END:
      // END blocks don't generate hardware operations, just log
      m_machineOps.LogInfo("Ending sequence with cleanup: " +
        GetParameterValue(*block, "cleanup"));

      // Notify progress callback that END block is processed
      if (m_progressCallback) {
        m_progressCallback(block->id, block->label, "Complete", "END block processed - program ending");
      }
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
    case BlockType::EXTEND_SLIDE:    // NEW
      operation = ConvertExtendSlideBlock(*block);
      break;

    case BlockType::RETRACT_SLIDE:   // NEW
      operation = ConvertRetractSlideBlock(*block);
      break;

    case BlockType::SET_LASER_CURRENT:    // NEW
      operation = ConvertSetLaserCurrentBlock(*block);
      break;

    case BlockType::LASER_ON:             // NEW
      operation = ConvertLaserOnBlock(*block);
      break;

    case BlockType::LASER_OFF:            // NEW
      operation = ConvertLaserOffBlock(*block);
      break;

    case BlockType::SET_TEC_TEMPERATURE:  // NEW
      operation = ConvertSetTECTemperatureBlock(*block);
      break;

    case BlockType::TEC_ON:               // NEW
      operation = ConvertTECOnBlock(*block);
      break;

    case BlockType::TEC_OFF:              // NEW
      operation = ConvertTECOffBlock(*block);
      break;

    default:
      m_machineOps.LogWarning("Unknown block type encountered: " +
        std::to_string(static_cast<int>(block->type)));

      // Notify progress callback of unknown block type
      if (m_progressCallback) {
        m_progressCallback(block->id, block->label, "Failed", "Unknown block type");
      }
      continue;
    }

    if (operation) {
      // Wrap operation with progress tracking for real-time feedback
      if (m_progressCallback) {
        auto wrappedOp = std::make_shared<ProgressTrackingOperation>(
          operation, block->id, block->label, m_progressCallback);
        sequence->AddOperation(wrappedOp);
      }
      else {
        // No progress callback, add operation directly
        sequence->AddOperation(operation);
      }

      m_machineOps.LogInfo("Added operation: " + operation->GetDescription());
    }
    else {
      // Failed to create operation
      if (m_progressCallback) {
        m_progressCallback(block->id, block->label, "Failed", "Failed to create hardware operation");
      }
    }
  }

  return sequence;
}

// ===================================================================
// Block conversion methods - convert each block type to sequence operation
// ===================================================================

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
  int delayMs = GetParameterValueAsInt(block, "delay_ms", 100);

  if (deviceName.empty()) {
    m_machineOps.LogError("CLEAR_OUTPUT block missing device_name parameter");
    return nullptr;
  }

  // Clear output = set output to false/LOW
  return std::make_shared<SetOutputOperation>(deviceName, pin, false, delayMs);
}

// ===================================================================
// Helper methods for parameter extraction
// ===================================================================

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
    m_machineOps.LogWarning("Invalid integer value for parameter " + paramName + ": " + value);
    return defaultValue;
  }
}

bool BlockSequenceConverter::GetParameterValueAsBool(const MachineBlock& block, const std::string& paramName, bool defaultValue) {
  std::string value = GetParameterValue(block, paramName);
  if (value.empty()) {
    return defaultValue;
  }

  // Convert to lowercase for comparison
  std::transform(value.begin(), value.end(), value.begin(), ::tolower);

  if (value == "true" || value == "1" || value == "yes" || value == "on") {
    return true;
  }
  else if (value == "false" || value == "0" || value == "no" || value == "off") {
    return false;
  }
  else {
    m_machineOps.LogWarning("Invalid boolean value for parameter " + paramName + ": " + value);
    return defaultValue;
  }
}

std::shared_ptr<SequenceOperation> BlockSequenceConverter::ConvertExtendSlideBlock(const MachineBlock& block) {
  std::string slideName = GetParameterValue(block, "slide_name");

  if (slideName.empty()) {
    m_machineOps.LogWarning("EXTEND_SLIDE block missing slide_name parameter");
    return nullptr;
  }

  m_machineOps.LogInfo("Converting EXTEND_SLIDE block for slide: " + slideName);
  return std::make_shared<ExtendSlideOperation>(slideName);
}

std::shared_ptr<SequenceOperation> BlockSequenceConverter::ConvertRetractSlideBlock(const MachineBlock& block) {
  std::string slideName = GetParameterValue(block, "slide_name");

  if (slideName.empty()) {
    m_machineOps.LogWarning("RETRACT_SLIDE block missing slide_name parameter");
    return nullptr;
  }

  m_machineOps.LogInfo("Converting RETRACT_SLIDE block for slide: " + slideName);
  return std::make_shared<RetractSlideOperation>(slideName);
}


// Step 11: Add converter methods in BlockSequenceConverter.cpp
std::shared_ptr<SequenceOperation> BlockSequenceConverter::ConvertSetLaserCurrentBlock(const MachineBlock& block) {
  std::string currentStr = GetParameterValue(block, "current_ma");
  std::string laserName = GetParameterValue(block, "laser_name");

  if (currentStr.empty()) {
    m_machineOps.LogWarning("SET_LASER_CURRENT block missing current_ma parameter");
    return nullptr;
  }

  float current = std::stof(currentStr);
  m_machineOps.LogInfo("Converting SET_LASER_CURRENT block: " + currentStr + " mA");
  return std::make_shared<SetLaserCurrentOperation>(current);
}

std::shared_ptr<SequenceOperation> BlockSequenceConverter::ConvertLaserOnBlock(const MachineBlock& block) {
  std::string laserName = GetParameterValue(block, "laser_name");

  m_machineOps.LogInfo("Converting LASER_ON block" + (laserName.empty() ? "" : " for: " + laserName));
  return std::make_shared<LaserOnOperation>(laserName);
}

std::shared_ptr<SequenceOperation> BlockSequenceConverter::ConvertLaserOffBlock(const MachineBlock& block) {
  std::string laserName = GetParameterValue(block, "laser_name");

  m_machineOps.LogInfo("Converting LASER_OFF block" + (laserName.empty() ? "" : " for: " + laserName));
  return std::make_shared<LaserOffOperation>(laserName);
}

std::shared_ptr<SequenceOperation> BlockSequenceConverter::ConvertSetTECTemperatureBlock(const MachineBlock& block) {
  std::string tempStr = GetParameterValue(block, "temperature_c");
  std::string laserName = GetParameterValue(block, "laser_name");

  if (tempStr.empty()) {
    m_machineOps.LogWarning("SET_TEC_TEMPERATURE block missing temperature_c parameter");
    return nullptr;
  }

  float temperature = std::stof(tempStr);
  m_machineOps.LogInfo("Converting SET_TEC_TEMPERATURE block: " + tempStr + "°C");
  return std::make_shared<SetTECTemperatureOperation>(temperature);
}

std::shared_ptr<SequenceOperation> BlockSequenceConverter::ConvertTECOnBlock(const MachineBlock& block) {
  std::string laserName = GetParameterValue(block, "laser_name");

  m_machineOps.LogInfo("Converting TEC_ON block" + (laserName.empty() ? "" : " for: " + laserName));
  return std::make_shared<TECOnOperation>(laserName);
}

std::shared_ptr<SequenceOperation> BlockSequenceConverter::ConvertTECOffBlock(const MachineBlock& block) {
  std::string laserName = GetParameterValue(block, "laser_name");

  m_machineOps.LogInfo("Converting TEC_OFF block" + (laserName.empty() ? "" : " for: " + laserName));
  return std::make_shared<TECOffOperation>(laserName);
}