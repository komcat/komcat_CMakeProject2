// UIPneumaticPanel.h - Simple UI panel for pneumatic slide control
#pragma once

#include "imgui.h"
#include <memory>
#include <string>
#include <vector>
#include <map>

// Forward declarations
class PneumaticManager;
enum class SlideState;

class UIPneumaticPanel {
public:
  UIPneumaticPanel(PneumaticManager& pneumaticManager);
  ~UIPneumaticPanel();

  // Disable copy/move to avoid issues with references
  UIPneumaticPanel(const UIPneumaticPanel&) = delete;
  UIPneumaticPanel& operator=(const UIPneumaticPanel&) = delete;
  UIPneumaticPanel(UIPneumaticPanel&&) = delete;
  UIPneumaticPanel& operator=(UIPneumaticPanel&&) = delete;

  // UI rendering
  void RenderUI();
  void ToggleWindow();
  bool IsVisible() const { return m_showWindow; }
  void SetVisible(bool visible) { m_showWindow = visible; }

private:
  // Reference to pneumatic manager
  PneumaticManager& m_pneumaticManager;

  // UI state
  bool m_showWindow = true;

  // State change tracking for elapsed time
  std::map<std::string, float> m_stateChangeTimestamp;
  std::map<std::string, SlideState> m_lastKnownState;
  std::map<std::string, float> m_lastMovementDuration; // Store completed movement time

  // Helper methods
  void RenderSlideRow(const std::string& slideName);
  const char* GetStateString(SlideState state) const;
  std::string GetStateStringWithTime(const std::string& slideName, SlideState state) const;
  ImVec4 GetStateColor(SlideState state) const;
  void RenderNoSlidesMessage();
  void UpdateStateTimestamp(const std::string& slideName, SlideState currentState);
  float GetElapsedTime(const std::string& slideName) const;
};