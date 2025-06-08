// BlockPropertyRenderers.h
#pragma once

#include "imgui.h"
#include "include/machine_operations.h"
#include <string>
#include <memory>
#include <tuple>

// Forward declarations
struct MachineBlock;
enum class BlockType;

// ═══════════════════════════════════════════════════════════════════════════════════════
// BASE CLASS FOR BLOCK PROPERTY RENDERERS
// ═══════════════════════════════════════════════════════════════════════════════════════
class BlockPropertyRenderer {
public:
  virtual ~BlockPropertyRenderer() = default;

  // Main rendering methods - each block type can override these
  virtual void RenderProperties(MachineBlock* block, MachineOperations* machineOps) = 0;
  virtual void RenderActions(MachineBlock* block, MachineOperations* machineOps) = 0;
  virtual void RenderValidation(MachineBlock* block) = 0;

protected:
  // Helper methods available to all renderers
  void RenderStandardParameters(MachineBlock* block);
  void RenderParameter(struct BlockParameter& param);
  void RenderSuccessPopup(const std::string& message);
  void RenderErrorPopup(const std::string& message);
};

// ═══════════════════════════════════════════════════════════════════════════════════════
// SPECIALIZED RENDERERS FOR EACH BLOCK TYPE
// ═══════════════════════════════════════════════════════════════════════════════════════

class StartBlockRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;
};

class EndBlockRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;
};

class MoveNodeRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;

private:
  // Helper methods specific to MOVE_NODE blocks
  std::tuple<std::string, std::string, std::string> ExtractMoveNodeParameters(MachineBlock* block);
  void RenderPositionInfo(const std::string& deviceName, const std::string& graphName, const std::string& nodeId);
  void RenderSavePositionButton(const std::string& deviceName, const std::string& graphName,
    const std::string& nodeId, MachineOperations* machineOps);
  void HandleSavePosition(const std::string& deviceName, const std::string& graphName,
    const std::string& nodeId, MachineOperations* machineOps);
  void RenderValidationWarnings(const std::string& deviceName, const std::string& graphName, const std::string& nodeId);
  void RenderHelperText();
};

class WaitRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;

private:
  void ValidateWaitTime(const std::string& waitTimeStr);
};

class SetOutputRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;

private:
  std::tuple<std::string, std::string, std::string, std::string> ExtractOutputParameters(MachineBlock* block);
  void RenderTestButton(const std::string& deviceName, const std::string& pin,
    const std::string& state, const std::string& delay, MachineOperations* machineOps);
};

class ClearOutputRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;

private:
  std::tuple<std::string, std::string, std::string> ExtractClearOutputParameters(MachineBlock* block);
  void RenderTestButton(const std::string& deviceName, const std::string& pin,
    const std::string& delay, MachineOperations* machineOps);
};

// Default renderer for unknown block types
class DefaultRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;
};

// ═══════════════════════════════════════════════════════════════════════════════════════
// RENDERER FACTORY
// ═══════════════════════════════════════════════════════════════════════════════════════
class BlockRendererFactory {
public:
  // FIXED: Add static keyword
  static std::unique_ptr<BlockPropertyRenderer> CreateRenderer(BlockType type);
};