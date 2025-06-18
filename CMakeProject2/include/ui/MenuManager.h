// MenuManager.h - Unified approach using MachineOperations
#pragma once
#include "include/logger.h"

// Forward declarations
class VerticalToolbarMenu;
class MachineOperations;

class MenuManager {
public:
  MenuManager(Logger* logger, MachineOperations* machineOps);

  void RenderMainMenuBar();

  // Individual menu renderers
  void RenderViewMenu();
  void RenderManualMenu();
  //void RenderToolsMenu();
  //void RenderProcessMenu();
  //void RenderHardwareMenu();
  //void RenderHelpMenu();

  // Set component references
  void SetLogger(Logger* logger) { m_logger = logger; }
  void SetMachineOperations(MachineOperations* ops) { m_machineOps = ops; }
  void SetVerticalToolbar(VerticalToolbarMenu* toolbar) { m_toolbar = toolbar; }

private:
  Logger* m_logger;
  MachineOperations* m_machineOps;
  VerticalToolbarMenu* m_toolbar;

  // Helper methods for Manual menu
  void RenderControllersSubmenu();
  void RenderMotionSubmenu();
};