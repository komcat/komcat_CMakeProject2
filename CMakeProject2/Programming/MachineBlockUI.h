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



// Forward declarations
class MachineOperations;
class SequenceStep;

enum class BlockType {
  START,
  END,
  MOVE_NODE,
  WAIT,
  SET_OUTPUT,
  CLEAR_OUTPUT
};

struct BlockParameter {
  std::string name;
  std::string value;
  std::string type; // "string", "int", "float", "bool"
  std::string description;
};

struct BlockConnection {
  int fromBlockId;
  int toBlockId;
  ImVec2 fromPos;  // Connection point position
  ImVec2 toPos;    // Connection point position
};

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



private:
  // UI state
  bool m_showWindow = true;

  // Block management
  std::vector<std::unique_ptr<MachineBlock>> m_programBlocks;
  std::vector<MachineBlock> m_paletteBlocks;
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
  void SaveProgram();
  void LoadProgram();

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
};