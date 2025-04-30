#pragma once

#include "PneumaticManager.h"
#include "imgui.h"
#include <map>

class PneumaticUI {
public:
  PneumaticUI(PneumaticManager& manager);
  ~PneumaticUI();

  // Render the ImGui UI window
  void RenderUI();

  // Check if the window is currently visible
  bool IsVisible() const { return m_showWindow; }

  // Toggle window visibility
  void ToggleWindow() { m_showWindow = !m_showWindow; }

private:
  // Reference to the pneumatic manager
  PneumaticManager& m_pneumaticManager;

  // UI state
  bool m_showWindow;
  bool m_showDebugInfo;

  // Color mapping for slide states
  std::map<SlideState, ImVec4> m_stateColors;

  // Helper methods
  void RenderSlidePanel(const std::string& slideName);
  ImVec4 GetStateColor(SlideState state) const;
  const char* GetStateString(SlideState state) const;

  // Store timestamps of state changes for animation effects
  std::map<std::string, float> m_stateChangeTimestamp;

  // Animation helpers
  bool IsAnimating(const std::string& slideName) const;
  float GetAnimationProgress(const std::string& slideName) const;
};