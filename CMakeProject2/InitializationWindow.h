#pragma once

#include "include/machine_operations.h"
#include "include/SequenceStep.h"
#include <memory>
#include <string>

class InitializationWindow {
public:
  InitializationWindow(MachineOperations& machineOps);
  ~InitializationWindow() = default;

  // Render the ImGui window
  void RenderUI();

  // Toggle window visibility
  void ToggleWindow() { m_showWindow = !m_showWindow; }
  bool IsVisible() const { return m_showWindow; }

private:
  MachineOperations& m_machineOps;
  bool m_showWindow = true;
  bool m_isInitializing = false;
  std::string m_statusMessage = "Ready";

  // Process step
  std::unique_ptr<SequenceStep> m_initStep;

  // Run the initialization process
  void RunInitializationProcess();
};