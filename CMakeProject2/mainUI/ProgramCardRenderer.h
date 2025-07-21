// ProgramCardRenderer.h
#pragma once

#include "imgui.h"
#include <string>
#include <vector>
#include <functional>

// Forward declaration
struct MacroProgramItem;

class ProgramCardRenderer {
public:
  ProgramCardRenderer();
  ~ProgramCardRenderer() = default;

  // Main rendering method
  void RenderProgramCards(
    const std::vector<MacroProgramItem>& programItems,
    int currentExecutingIndex,
    bool isExecuting,
    std::function<void(const std::string&)> onRunProgram,
    std::function<void(int)> onRemoveProgram
  );

  // Enhanced execution controls
  void RenderEnhancedExecutionControls(
    bool isExecuting,
    bool isPaused,
    std::function<void()> onPlay,
    std::function<void()> onStop,
    std::function<void()> onPause
  );

  // Configuration
  void SetCardSpacing(float spacing) { m_cardSpacing = spacing; }
  void SetCardHeight(float height) { m_cardHeight = height; }
  void SetShowIcons(bool show) { m_showIcons = show; }
  void SetShowConnectors(bool show) { m_showConnectors = show; }

private:
  // Card rendering
  void RenderSingleCard(
    const MacroProgramItem& item,
    int index,
    bool isCurrentlyExecuting,
    std::function<void(const std::string&)> onRunProgram,
    std::function<void(int)> onRemoveProgram
  );

  // Visual elements
  void DrawCardBackground(ImVec2 cardPos, ImVec2 cardSize, ImU32 cardColor, ImU32 borderColor);
  void DrawStatusIndicator(ImVec2 cardPos, ImVec2 cardSize, ImU32 borderColor);
  void DrawConnector(ImVec2 startPos, ImVec2 endPos);

  // Utility methods
  const char* GetProgramIcon(const std::string& programName);
  ImU32 GetCardColor(bool isSelected, bool isExecuting);
  ImU32 GetBorderColor(bool isSelected, bool isExecuting);

  // Configuration
  float m_cardHeight = 80.0f;
  float m_cardSpacing = 15.0f;
  float m_borderWidth = 2.0f;
  float m_cornerRadius = 8.0f;
  bool m_showIcons = true;
  bool m_showConnectors = true;

  // Colors
  ImU32 m_defaultCardColor = IM_COL32(40, 40, 45, 255);
  ImU32 m_selectedCardColor = IM_COL32(0, 20, 60, 100);
  ImU32 m_executingCardColor = IM_COL32(0, 60, 0, 100);

  ImU32 m_defaultBorderColor = IM_COL32(100, 100, 100, 255);
  ImU32 m_selectedBorderColor = IM_COL32(0, 120, 255, 255);
  ImU32 m_executingBorderColor = IM_COL32(0, 255, 0, 255);
};