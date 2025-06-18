// MenuManager.cpp
#include "MenuManager.h"
#include "imgui.h"
#include "include/logger.h"
// Include other necessary headers
class VerticalToolbarMenu; // Forward declaration
class MachineOperations; // Forward declaration

MenuManager::MenuManager(Logger* logger, MachineOperations* machineOps)
  : m_logger(logger), m_machineOps(machineOps), m_toolbar(nullptr) {
}

void MenuManager::RenderMainMenuBar() {
  if (ImGui::BeginMainMenuBar()) {
    RenderViewMenu();
    //RenderToolsMenu();

    //if (m_machineOps) {
    //  RenderProcessMenu();
    //}

    //RenderHardwareMenu();
    //RenderHelpMenu();

    ImGui::EndMainMenuBar();
  }
}

void MenuManager::RenderViewMenu() {
  if (ImGui::BeginMenu("View")) {
    // Logger controls
    if (m_logger && ImGui::BeginMenu("Logger")) {
      bool isMinimized = m_logger->IsMinimized();
      bool isMaximized = m_logger->IsMaximized();

      if (ImGui::MenuItem("Minimize", "Ctrl+M", isMinimized)) {
        if (!isMinimized) m_logger->ToggleMinimize();
      }

      if (ImGui::MenuItem("Maximize", "Ctrl+Shift+M", isMaximized)) {
        m_logger->ToggleMaximize();
      }

      // Add other logger controls...
      ImGui::EndMenu();
    }

    // Add other view controls...
    ImGui::EndMenu();
  }
}

// Implement other menu methods...

// In CMakeProject2.cpp - Usage:
// In main(), after creating components:
