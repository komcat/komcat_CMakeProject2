// MenuManager.cpp - Unified approach using MachineOperations
#include "MenuManager.h"
#include "imgui.h"
#include "include/logger.h"
#include "include/machine_operations.h"

MenuManager::MenuManager(Logger* logger, MachineOperations* machineOps)
  : m_logger(logger), m_machineOps(machineOps), m_toolbar(nullptr) {
}

void MenuManager::RenderMainMenuBar() {
  if (ImGui::BeginMainMenuBar()) {
    RenderViewMenu();
    RenderManualMenu();
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

void MenuManager::RenderManualMenu() {
  if (ImGui::BeginMenu("Manual")) {
    RenderControllersSubmenu();
    RenderMotionSubmenu();
    ImGui::EndMenu();
  }
}

void MenuManager::RenderControllersSubmenu() {
  if (ImGui::BeginMenu("Controllers")) {
    // PI Controller - Access through MachineOperations
    if (m_machineOps && m_machineOps->GetPIControllerManager()) {
      auto* piManager = m_machineOps->GetPIControllerManager();
      bool isVisible = piManager->IsVisible();
      if (ImGui::MenuItem("PI", nullptr, isVisible)) {
        piManager->ToggleWindow();
        if (m_logger) {
          m_logger->LogInfo("PI Controller " + std::string(piManager->IsVisible() ? "opened" : "closed"));
        }
      }
    }
    else {
      // Show disabled item if manager not available
      ImGui::MenuItem("PI", nullptr, false, false);
    }

    // ACS Controller - Access through MachineOperations
    if (m_machineOps && m_machineOps->GetACSControllerManager()) {
      auto* acsManager = m_machineOps->GetACSControllerManager();
      bool isVisible = acsManager->IsVisible();
      if (ImGui::MenuItem("ACS", nullptr, isVisible)) {
        acsManager->ToggleWindow();
        if (m_logger) {
          m_logger->LogInfo("ACS Controller " + std::string(acsManager->IsVisible() ? "opened" : "closed"));
        }
      }
    }
    else {
      // Show disabled item if manager not available
      ImGui::MenuItem("ACS", nullptr, false, false);
    }

    ImGui::EndMenu();
  }
}

void MenuManager::RenderMotionSubmenu() {
  if (ImGui::BeginMenu("Motion")) {
    // Motion Layer (Path) - Access through MachineOperations
    if (m_machineOps && m_machineOps->GetMotionControlLayer()) {
      auto* motionLayer = m_machineOps->GetMotionControlLayer();
      bool isVisible = motionLayer->IsVisible();
      if (ImGui::MenuItem("Motion Layer (Path)", nullptr, isVisible)) {
        motionLayer->ToggleWindow();
        if (m_logger) {
          m_logger->LogInfo("Motion Layer " + std::string(motionLayer->IsVisible() ? "opened" : "closed"));
        }
      }
    }
    else {
      ImGui::MenuItem("Motion Layer (Path)", nullptr, false, false);
    }

    // Position Editor (config editor) - Access through MachineOperations
    if (m_machineOps && m_machineOps->GetMotionConfigEditor()) {
      auto* configEditor = m_machineOps->GetMotionConfigEditor();
      bool isVisible = configEditor->IsVisible();
      if (ImGui::MenuItem("Position Editor (config editor)", nullptr, isVisible)) {
        configEditor->ToggleWindow();
        if (m_logger) {
          m_logger->LogInfo("Position Editor " + std::string(configEditor->IsVisible() ? "opened" : "closed"));
        }
      }
    }
    else {
      ImGui::MenuItem("Position Editor (config editor)", nullptr, false, false);
    }

    // Graph Visualizer - Access through MachineOperations
    if (m_machineOps && m_machineOps->GetGraphVisualizer()) {
      auto* graphVisualizer = m_machineOps->GetGraphVisualizer();
      bool isVisible = graphVisualizer->IsVisible();
      if (ImGui::MenuItem("Graph Visualizer", nullptr, isVisible)) {
        graphVisualizer->ToggleWindow();
        if (m_logger) {
          m_logger->LogInfo("Graph Visualizer " + std::string(graphVisualizer->IsVisible() ? "opened" : "closed"));
        }
      }
    }
    else {
      ImGui::MenuItem("Graph Visualizer", nullptr, false, false);
    }

    ImGui::EndMenu();
  }
}

// Implement other menu methods...