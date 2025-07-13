// ACSPanelUI.h - UI panel for managing multiple ACS controllers
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>

// Forward declarations
class ACSControllerManager;
class ACSController;

class ACSPanelUI {
public:
  ACSPanelUI(ACSControllerManager& acsManager);
  ~ACSPanelUI();

  // Disable copy/move to avoid issues with references
  ACSPanelUI(const ACSPanelUI&) = delete;
  ACSPanelUI& operator=(const ACSPanelUI&) = delete;
  ACSPanelUI(ACSPanelUI&&) = delete;
  ACSPanelUI& operator=(ACSPanelUI&&) = delete;

  // UI rendering
  void RenderUI();
  void ToggleWindow();
  bool IsVisible() const { return m_showWindow; }
  void SetVisible(bool visible) { m_showWindow = visible; }

private:
  // Reference to ACS controller manager
  ACSControllerManager& m_acsManager;

  // UI state
  bool m_showWindow = true;
  std::string m_selectedControllerName;

  // Panel rendering methods
  void RenderLeftPanel();   // List of ACS controllers
  void RenderRightPanel();  // Selected controller interface

  // Helper methods
  void RenderControllerList();
  void RenderSelectedControllerUI();
  void RenderNoSelectionMessage();

  // Embedded UI rendering methods
  void RenderControllerHeader(ACSController* controller);
  void RenderConnectionControls(ACSController* controller);
  void RenderMotionStatus(ACSController* controller);
  void RenderJogControls(ACSController* controller);
  void RenderJogDistanceControl();
  void RenderPositionDisplay(ACSController* controller);
  void RenderNamedPositions(ACSController* controller);
  void RenderUtilityControls(ACSController* controller);

  // Helper method to get available axes for a controller
  std::vector<std::string> GetControllerAxes(const std::string& controllerName);

  // Velocity control rendering
  void RenderVelocityControls(ACSController* controller);

  // UI state for jog controls
  double m_jogDistance;

  // UI state for velocity controls
  std::map<std::string, double> m_axisVelocities;  // Store velocity for each axis
};