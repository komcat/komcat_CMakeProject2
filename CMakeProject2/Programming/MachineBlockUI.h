// MachineBlockUI.h - Fixed version
#pragma once

#include "imgui.h"
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include <functional>  // For std::function
#include <thread>      // For std::thread
#include <algorithm>   // For std::transform
#include <nlohmann/json.hpp>
#include "virtual_machine_operations_adapter.h"
#include "ProgramManager.h"
#include "FeedbackUI.h"  // Include the feedback UI
#include "UserPromptUI.h"  // Include the user prompt UI

// Forward declarations
class MachineOperations;
class SequenceStep;

enum class BlockType {
  START,
  END,
  MOVE_NODE,
  WAIT,
  SET_OUTPUT,
  CLEAR_OUTPUT,
  EXTEND_SLIDE,    // NEW: Extend pneumatic slide
  RETRACT_SLIDE,
  SET_LASER_CURRENT,    // NEW: Set laser current (mA)
  LASER_ON,             // NEW: Turn laser on
  LASER_OFF,            // NEW: Turn laser off
  SET_TEC_TEMPERATURE,  // NEW: Set TEC temperature (°C)
  TEC_ON,               // NEW: Turn TEC on
  TEC_OFF,               // NEW: Turn TEC off
  PROMPT,             // NEW: User confirmation prompt
  MOVE_TO_POSITION,     // Move to saved position name
  MOVE_RELATIVE_AXIS    // Move relative on specified axis
};

// In MachineBlockUI.h, make sure BlockParameter struct looks like this:
struct BlockParameter {
  std::string name;
  std::string value;
  std::string type; // "string", "int", "float", "bool"
  std::string description;

  // Explicit constructor to ensure proper copying
  BlockParameter(const std::string& n, const std::string& v, const std::string& t, const std::string& d)
    : name(n), value(v), type(t), description(d) {
  }

  // Default copy constructor and assignment operator should work correctly
  BlockParameter(const BlockParameter&) = default;
  BlockParameter& operator=(const BlockParameter&) = default;
};

struct BlockConnection {
  int fromBlockId;
  int toBlockId;
  ImVec2 fromPos;  // Connection point position
  ImVec2 toPos;    // Connection point position
};

// MOVED: Define MachineBlock before BlockCategory
struct MachineBlock {
  int id;
  BlockType type;
  std::string label;
  ImVec2 position;
  std::vector<BlockParameter> parameters;
  ImU32 color;
  bool selected = false;

  // Connection points
  std::vector<int> inputConnections;   // Block IDs that connect TO this block
  std::vector<int> outputConnections;  // Block IDs that this block connects TO

  MachineBlock(int id, BlockType type, const std::string& label, ImU32 color)
    : id(id), type(type), label(label), color(color), position(0, 0) {
  }
};

// NOW BlockCategory can use MachineBlock since it's already defined
struct BlockCategory {
  std::string name;
  std::string description;
  ImVec4 headerColor;
  std::vector<MachineBlock> blocks;
  bool expanded = true; // Default expanded state
};

class MachineBlockUI {
public:
  MachineBlockUI();
  ~MachineBlockUI() = default;

  void RenderUI();
  void ToggleWindow() { m_showWindow = !m_showWindow; }
  bool IsVisible() const { return m_showWindow; }

  // Program execution
  void ExecuteProgram();
  bool ValidateProgram() const;

  // NEW: Integration with sequence execution
  void SetMachineOperations(MachineOperations* machineOps) { m_machineOps = machineOps; }

  // Execute the program using the sequence framework
  void ExecuteProgramAsSequence();

  // Execute with completion callback
  void ExecuteProgramAsSequence(std::function<void(bool)> onComplete);

  // Debug execution (simulation only)
  void ExecuteProgramDebugOnly();

  // NEW: Single block execution capability
  void ExecuteSingleBlock(MachineBlock* block);
  void ExecuteSingleBlockAsSequence(MachineBlock* block, std::function<void(bool)> onComplete = nullptr);

  // Add this public method
  void SetVirtualMachineOperations(VirtualMachineOperationsAdapter* virtualOps) {
    m_virtualOps = virtualOps;
  }

  bool ExecuteBlockWithVirtualOps(MachineBlock* block);
  void ExecuteProgramWithVirtualOps();

  // Add these methods to your existing MachineBlockUI class
  void ShowFeedbackWindow() {
    if (m_feedbackUI) {
      m_feedbackUI->Show();
      m_showFeedbackWindow = true;
    }
  }

  void HideFeedbackWindow() {
    if (m_feedbackUI) {
      m_feedbackUI->Hide();
      m_showFeedbackWindow = false;
    }
  }

  // Call this in your render loop
  void RenderFeedback() {
    if (m_feedbackUI) {
      m_feedbackUI->Render();
    }
  }
  // Add this public method
  void RenderPromptUI() {
    if (m_promptUI) {
      m_promptUI->Render();
    }
  }
  void UpdateBlockLabelIfNeeded(MachineBlock* block);

private:
  // UI state
  bool m_showWindow = true;

  // Block management
  std::vector<std::unique_ptr<MachineBlock>> m_programBlocks;
  std::vector<BlockCategory> m_blockCategories;
  std::vector<BlockConnection> m_connections;
  MachineBlock* m_selectedBlock = nullptr;
  int m_nextBlockId = 1;

  // Connection state
  bool m_isConnecting = false;
  MachineBlock* m_connectionStart = nullptr;
  ImVec2 m_connectionStartPos;
  ImVec2 m_mousePos;

  // Drag and drop state
  bool m_isDragging = false;
  MachineBlock* m_draggedBlock = nullptr;
  ImVec2 m_dragOffset;

  // Canvas state for middle panel
  ImVec2 m_canvasOffset = ImVec2(0, 0);
  float m_canvasZoom = 1.0f;

  // Panel dimensions
  float m_leftPanelWidth = 200.0f;
  float m_rightPanelWidth = 250.0f;

  // Block rendering constants
  const float BLOCK_WIDTH = 120.0f;
  const float BLOCK_HEIGHT = 60.0f;
  const float BLOCK_ROUNDING = 8.0f;
  const float TEXT_PADDING = 8.0f;
  const float CONNECTOR_RADIUS = 6.0f;

  // Colors - Enhanced with START/END
  // Colors - Muted/Professional versions
  const ImU32 START_COLOR = IM_COL32(40, 150, 40, 255);      // Muted Green
  const ImU32 END_COLOR = IM_COL32(180, 40, 60, 255);        // Muted Red
  const ImU32 MOVE_NODE_COLOR = IM_COL32(70, 120, 180, 255); // Muted Blue
  const ImU32 WAIT_COLOR = IM_COL32(200, 130, 40, 255);      // Muted Orange
  const ImU32 SET_OUTPUT_COLOR = IM_COL32(40, 160, 90, 255); // Muted Lime Green
  const ImU32 CLEAR_OUTPUT_COLOR = IM_COL32(200, 80, 40, 255); // Muted Red Orange
  const ImU32 EXTEND_SLIDE_COLOR = IM_COL32(100, 200, 100, 255);   // Light green
  const ImU32 RETRACT_SLIDE_COLOR = IM_COL32(200, 100, 100, 255);  // Light red
  const ImU32 SET_LASER_CURRENT_COLOR = IM_COL32(255, 150, 50, 255);   // Orange
  const ImU32 LASER_ON_COLOR = IM_COL32(255, 100, 100, 255);           // Red
  const ImU32 LASER_OFF_COLOR = IM_COL32(150, 150, 150, 255);          // Gray
  const ImU32 SET_TEC_TEMPERATURE_COLOR = IM_COL32(100, 150, 255, 255); // Blue
  const ImU32 TEC_ON_COLOR = IM_COL32(100, 200, 255, 255);             // Light blue
  const ImU32 TEC_OFF_COLOR = IM_COL32(120, 120, 180, 255);            // Dark blue
  const ImU32 PROMPT_COLOR = IM_COL32(255, 200, 50, 255);  // Gold/Yellow
  // Add new block colors in MachineBlockUI.h:
  const ImU32 MOVE_TO_POSITION_COLOR = IM_COL32(50, 150, 200, 255);     // Light blue
  const ImU32 MOVE_RELATIVE_AXIS_COLOR = IM_COL32(150, 100, 200, 255);  // Light purple

  // Canvas background colors
  const ImU32 CANVAS_BG_COLOR = IM_COL32(45, 45, 45, 255);
  const ImU32 GRID_COLOR = IM_COL32(80, 80, 80, 100);

  // NEW: Machine operations for actual execution
  MachineOperations* m_machineOps = nullptr;

  // NEW: Current executing sequence
  std::unique_ptr<SequenceStep> m_currentSequence = nullptr;

  // NEW: Execution state
  bool m_isExecuting = false;
  std::string m_executionStatus = "Ready";

  // UI rendering methods
  void RenderLeftPanel();
  void RenderMiddlePanel();
  void RenderRightPanel();
  void RenderExecutionStatus(); // NEW: Render execution status
  void RenderPaletteBlock(const MachineBlock& block, int index);
  void RenderProgramBlock(const MachineBlock& block, const ImVec2& canvasPos);
  void RenderConnections(const ImVec2& canvasPos);
  void RenderGrid(const ImVec2& canvasPos, const ImVec2& canvasSize);

  // NEW: Add this method declaration
  void RenderBlockHeader(MachineBlock* block);
  void UpdateBlockLabel(MachineBlock& block);

  // Canvas utilities
  ImVec2 WorldToCanvas(const ImVec2& canvasPos, const ImVec2& worldPos) const;
  ImVec2 CanvasToWorld(const ImVec2& canvasPos, const ImVec2& canvasPos_) const;

  // Block management methods
  void InitializePalette();
  void AddBlockToProgram(BlockType type, const ImVec2& position);
  void InitializeBlockParameters(MachineBlock& block);
  void DeleteSelectedBlock();
  MachineBlock* GetBlockAtPosition(const ImVec2& pos, const ImVec2& canvasPos);

  // Connection methods
  void StartConnection(MachineBlock* fromBlock, const ImVec2& startPos);
  void CompleteConnection(MachineBlock* toBlock);
  void CancelConnection();
  void DeleteConnection(int fromBlockId, int toBlockId);

  // Block connection points
  ImVec2 GetBlockInputPoint(const MachineBlock& block, const ImVec2& canvasPos) const;
  ImVec2 GetBlockOutputPoint(const MachineBlock& block, const ImVec2& canvasPos) const;

  // Utility methods
  ImU32 GetBlockColor(BlockType type) const;
  std::string BlockTypeToString(BlockType type) const;

  // Program execution helpers
  MachineBlock* FindStartBlock();
  std::vector<MachineBlock*> GetExecutionOrder();
  bool HasValidFlow() const;
  bool HasValidExecutionPath() const;  // ADD THIS LINE
  int CountBlocksOfType(BlockType type) const;
  bool CanBlockAcceptInput(const MachineBlock& block) const;
  bool CanBlockProvideOutput(const MachineBlock& block) const;

  // File operations
  // OR add overloaded versions to maintain compatibility:
  void SaveProgram();  // Calls SaveProgram("default")
  void SaveProgram(const std::string& programName);
  void LoadProgram();  // Calls LoadProgram("default") 
  void LoadProgram(const std::string& programName);

  // Quick actions
  void QuickStart();
  void ClearAll();

  // Helper functions for block display
  std::string GetBlockDescription(const MachineBlock& block) const;
  std::string GetParameterValue(const MachineBlock& block, const std::string& paramName) const;

  // JSON helper methods
  std::string BlockTypeToJsonString(BlockType type) const;
  BlockType JsonStringToBlockType(const std::string& typeStr) const;

  // Helper method to create a single block sequence
  std::vector<MachineBlock*> CreateSingleBlockExecutionOrder(MachineBlock* block);

  // Add this private member
  VirtualMachineOperationsAdapter* m_virtualOps = nullptr;

  std::unique_ptr<ProgramManager> m_programManager;

  // Add these new members
  std::unique_ptr<FeedbackUI> m_feedbackUI;
  bool m_showFeedbackWindow = false;

  void UpdateBlockResult(int blockId, const std::string& status,
    const std::string& result, const std::string& details);

  std::vector<MachineBlock*> m_currentExecutionOrder;
  size_t m_currentBlockIndex = 0;

  void ExecuteSequenceWithMonitoring();
  void ExecuteSequenceWithSimpleMonitoring();
  void MonitorSequenceProgress(std::atomic<bool>& executionComplete);
  int GetEstimatedBlockExecutionTime(MachineBlock* block);

  void RenderBlockCategory(BlockCategory& category);
  size_t GetTotalBlockCount() const;

  std::unique_ptr<UserPromptUI> m_promptUI;
};