// PIPanelUI.h - UI panel for managing multiple PI controllers
#pragma once

#include <memory>
#include <string>

// Forward declarations
class PIControllerManager;
class PIController;

class PIPanelUI {
public:
  PIPanelUI(PIControllerManager& piManager);
  ~PIPanelUI();

  // Disable copy/move to avoid issues with references
  PIPanelUI(const PIPanelUI&) = delete;
  PIPanelUI& operator=(const PIPanelUI&) = delete;
  PIPanelUI(PIPanelUI&&) = delete;
  PIPanelUI& operator=(PIPanelUI&&) = delete;

  // UI rendering
  void RenderUI();
  void ToggleWindow();
  bool IsVisible() const { return m_showWindow; }
  void SetVisible(bool visible) { m_showWindow = visible; }

private:
  // Reference to PI controller manager
  PIControllerManager& m_piManager;

  // UI state
  bool m_showWindow = true;
  std::string m_selectedControllerName;

  // Panel rendering methods
  void RenderLeftPanel();   // List of PI controllers
  void RenderRightPanel();  // Selected controller interface

  // Helper methods
  void RenderControllerList();
  void RenderSelectedControllerUI();
  void RenderNoSelectionMessage();

  // Embedded UI rendering methods
  void RenderControllerHeader(PIController* controller);
  void RenderConnectionControls(PIController* controller);
  void RenderMotionStatus(PIController* controller);
  void RenderJogControls(PIController* controller);
  void RenderJogDistanceControl();
  void RenderSystemVelocityControl(PIController* controller);
  void RenderPositionDisplay(PIController* controller);
  void RenderNamedPositions(PIController* controller);
  void RenderUtilityControls(PIController* controller);

  // UI state for jog controls
  double m_jogDistance;
  double m_systemVelocity;
};