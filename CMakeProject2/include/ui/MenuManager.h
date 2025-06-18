// MenuManager.h - Create a dedicated menu management class
#pragma once
#include "include/logger.h"


// Include other necessary headers
class VerticalToolbarMenu; // Forward declaration
class MachineOperations; // Forward declaration

class MenuManager {
public:
  MenuManager(Logger* logger, MachineOperations* machineOps);

  void RenderMainMenuBar();

  // Individual menu renderers
  void RenderViewMenu();
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
  // Add other component references as needed
};

