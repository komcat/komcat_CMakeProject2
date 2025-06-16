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


class ExtendSlideRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;

private:
  std::string ExtractSlideName(MachineBlock* block);
  void RenderTestButton(const std::string& slideName, MachineOperations* machineOps);
};

class RetractSlideRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;

private:
  std::string ExtractSlideName(MachineBlock* block);
  void RenderTestButton(const std::string& slideName, MachineOperations* machineOps);
};


// Step 13: Add property renderers for laser/TEC blocks in BlockPropertyRenderers.h
class SetLaserCurrentRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;

private:
  std::tuple<std::string, std::string> ExtractLaserCurrentParameters(MachineBlock* block);
  void RenderTestButton(const std::string& current, const std::string& laserName, MachineOperations* machineOps);
};

class LaserOnRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;

private:
  std::string ExtractLaserName(MachineBlock* block);
  void RenderTestButton(const std::string& laserName, MachineOperations* machineOps);
};

class LaserOffRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;

private:
  std::string ExtractLaserName(MachineBlock* block);
  void RenderTestButton(const std::string& laserName, MachineOperations* machineOps);
};

class SetTECTemperatureRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;

private:
  std::tuple<std::string, std::string> ExtractTECTemperatureParameters(MachineBlock* block);
  void RenderTestButton(const std::string& temperature, const std::string& laserName, MachineOperations* machineOps);
};

class TECOnRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;

private:
  std::string ExtractLaserName(MachineBlock* block);
  void RenderTestButton(const std::string& laserName, MachineOperations* machineOps);
};

class TECOffRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;

private:
  std::string ExtractLaserName(MachineBlock* block);
  void RenderTestButton(const std::string& laserName, MachineOperations* machineOps);
};


// Add to BlockPropertyRenderers.h:
class PromptRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;

private:
  std::tuple<std::string, std::string> ExtractPromptParameters(MachineBlock* block);
  void RenderPreviewButton(const std::string& title, const std::string& message);
};


// Add these class declarations to BlockPropertyRenderers.h:

class MoveToPositionRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;

private:
  std::tuple<std::string, std::string, bool> ExtractMoveToPositionParameters(MachineBlock* block);
  void RenderTestButton(const std::string& controllerName, const std::string& positionName,
    bool blocking, MachineOperations* machineOps);
};

class MoveRelativeAxisRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;

private:
  std::tuple<std::string, std::string, std::string, bool> ExtractMoveRelativeAxisParameters(MachineBlock* block);
  void RenderTestButton(const std::string& controllerName, const std::string& axisName,
    const std::string& distance, bool blocking, MachineOperations* machineOps);
};



// Keithley Reset Renderer
class KeithleyResetRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;
};

// Keithley Set Output Renderer
class KeithleySetOutputRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;
};

// Keithley Voltage Source Renderer
class KeithleyVoltageSourceRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;
};

// Keithley Current Source Renderer
class KeithleyCurrentSourceRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;
};

// Keithley Read Voltage Renderer
class KeithleyReadVoltageRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;
};

// Keithley Read Current Renderer
class KeithleyReadCurrentRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;
};

// Keithley Read Resistance Renderer
class KeithleyReadResistanceRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;
};

// Keithley Send Command Renderer
class KeithleySendCommandRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;
};

class ScanOperationRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;

private:
  struct ScanParameters {
    std::string deviceName;
    std::string dataChannel;
    std::string stepSizesStr;
    int settlingTimeMs;
    std::string axesStr;
    int timeoutMinutes;
  };

  ScanParameters ExtractScanParameters(MachineBlock* block);
  void RenderTestButton(const ScanParameters& params, MachineOperations* machineOps);
  void RenderScanStatus(const ScanParameters& params, MachineOperations* machineOps);
  std::vector<double> ParseStepSizes(const std::string& stepSizesStr);
  std::vector<std::string> ParseAxes(const std::string& axesStr);
};