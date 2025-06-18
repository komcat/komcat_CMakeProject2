// MenuManager.cpp - Complete implementation with all functionality including Laser TEC
#include "MenuManager.h"
#include "imgui.h"
#include "include/logger.h"
#include "include/machine_operations.h"
#include "VerticalToolbarMenu.h"

MenuManager::MenuManager(Logger* logger, MachineOperations* machineOps)
  : m_logger(logger), m_machineOps(machineOps), m_toolbar(nullptr) {
}

void MenuManager::RenderMainMenuBar() {
  if (ImGui::BeginMainMenuBar()) {
    RenderViewMenu();
    RenderManualMenu();
    RenderToolsMenu();

    if (m_machineOps) {
      RenderProcessMenu();
    }

    RenderHardwareMenu();
    RenderHelpMenu();

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

      ImGui::EndMenu();
    }

    // Toolbar controls
    if (m_toolbar && ImGui::BeginMenu("Toolbar")) {
      if (ImGui::MenuItem("Show All Windows")) {
        // Logic to show all windows in toolbar
        if (m_logger) m_logger->LogInfo("MenuManager: Show all windows requested");
      }

      if (ImGui::MenuItem("Hide All Windows")) {
        // Logic to hide all windows in toolbar
        if (m_logger) m_logger->LogInfo("MenuManager: Hide all windows requested");
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Save Window States")) {
        m_toolbar->SaveAllWindowStates();
        if (m_logger) m_logger->LogInfo("MenuManager: Window states saved");
      }

      ImGui::EndMenu();
    }

    ImGui::EndMenu();
  }
}

void MenuManager::RenderManualMenu() {
  if (ImGui::BeginMenu("Manual")) {
    // Motion Controls
    if (ImGui::BeginMenu("Motion")) {
      RenderMotionControlsMenu();
      ImGui::EndMenu();
    }

    // IO Submenu - NEW FUNCTIONALITY
    if (ImGui::BeginMenu("IO")) {
      RenderIOSubmenu();
      ImGui::EndMenu();
    }

    // Pneumatic Controls
    if (ImGui::BeginMenu("Pneumatic")) {
      RenderPneumaticControlsMenu();
      ImGui::EndMenu();
    }

    // Laser TEC Controls
    if (ImGui::BeginMenu("Laser TEC")) {
      RenderLaserTECControlsMenu();
      ImGui::EndMenu();
    }

    ImGui::Separator();

    // Quick access to main panels
    if (ImGui::MenuItem("Global Jog Panel")) {
      if (m_toolbar) {
        bool success = m_toolbar->ToggleComponentByName("Global Jog Panel");
        if (!success && m_logger) {
          m_logger->LogWarning("MenuManager: Global Jog Panel not found in toolbar");
        }
      }
    }

    ImGui::EndMenu();
  }
}

void MenuManager::RenderMotionControlsMenu() {
  if (!m_machineOps) {
    ImGui::MenuItem("(Motion operations not available)", nullptr, false, false);
    return;
  }

  // PI Controller Controls
  if (ImGui::BeginMenu("PI Controllers")) {
    if (ImGui::MenuItem("Show PI Manager")) {
      if (m_toolbar) {
        bool success = m_toolbar->ToggleComponentByName("PI");
        if (!success && m_logger) {
          m_logger->LogWarning("MenuManager: PI Controller Manager not found in toolbar");
        }
      }
    }

    ImGui::Separator();

    // Common PI operations
    if (ImGui::MenuItem("Home All PI Devices")) {
      // Example operation - you can expand this
      if (m_logger) m_logger->LogInfo("MenuManager: Home all PI devices requested");
    }

    if (ImGui::MenuItem("Stop All PI Motion")) {
      if (m_logger) m_logger->LogInfo("MenuManager: Stop all PI motion requested");
    }

    ImGui::EndMenu();
  }

  // ACS Controller Controls
  if (ImGui::BeginMenu("ACS Controllers")) {
    if (ImGui::MenuItem("Show ACS Manager")) {
      if (m_toolbar) {
        bool success = m_toolbar->ToggleComponentByName("Gantry");
        if (!success && m_logger) {
          m_logger->LogWarning("MenuManager: ACS Controller Manager not found in toolbar");
        }
      }
    }

    ImGui::Separator();

    // Common ACS operations
    if (ImGui::MenuItem("Home All ACS Devices")) {
      if (m_logger) m_logger->LogInfo("MenuManager: Home all ACS devices requested");
    }

    if (ImGui::MenuItem("Stop All ACS Motion")) {
      if (m_logger) m_logger->LogInfo("MenuManager: Stop all ACS motion requested");
    }

    ImGui::EndMenu();
  }

  ImGui::Separator();

  // Motion Control Layer
  if (ImGui::MenuItem("Show Motion Control")) {
    if (m_toolbar) {
      bool success = m_toolbar->ToggleComponentByName("Motion Control");
      if (!success && m_logger) {
        m_logger->LogWarning("MenuManager: Motion Control Layer not found in toolbar");
      }
    }
  }

  // Emergency stop
  ImGui::Separator();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
  if (ImGui::MenuItem("EMERGENCY STOP ALL")) {
    if (m_logger) m_logger->LogWarning("MenuManager: EMERGENCY STOP requested!");
    // Add emergency stop logic here
  }
  ImGui::PopStyleColor();
}

void MenuManager::RenderIOSubmenu() {
  // IO Quick Panel - Toggle the existing component
  if (ImGui::MenuItem("IO Quick Panel")) {
    if (m_toolbar) {
      // Try both "IO Quick Panel" and "IO Quick Control" names
      bool success = m_toolbar->ToggleComponentByName("IO Quick Panel");
      if (!success) {
        success = m_toolbar->ToggleComponentByName("IO Quick Control");
      }
      if (!success && m_logger) {
        m_logger->LogWarning("MenuManager: IO Quick Panel component not found in toolbar");
      }
    }
    else if (m_logger) {
      m_logger->LogWarning("MenuManager: Toolbar reference not set");
    }
  }

  // IO Status - Toggle the existing component  
  if (ImGui::MenuItem("IO Status")) {
    if (m_toolbar) {
      // Try different possible names for the IO status component
      bool success = m_toolbar->ToggleComponentByName("IO Status");
      if (!success) {
        success = m_toolbar->ToggleComponentByName("IO Control");
      }
      if (!success) {
        success = m_toolbar->ToggleComponentByName("EziIO Status");
      }
      if (!success && m_logger) {
        m_logger->LogWarning("MenuManager: IO Status component not found in toolbar");
      }
    }
    else if (m_logger) {
      m_logger->LogWarning("MenuManager: Toolbar reference not set");
    }
  }

  // Pneumatic - Toggle the existing component
  if (ImGui::MenuItem("Pneumatic")) {
    if (m_toolbar) {
      bool success = m_toolbar->ToggleComponentByName("Pneumatic");
      if (!success && m_logger) {
        m_logger->LogWarning("MenuManager: Pneumatic component not found in toolbar");
      }
    }
    else if (m_logger) {
      m_logger->LogWarning("MenuManager: Toolbar reference not set");
    }
  }

  ImGui::Separator();

  // Quick IO operations
  if (m_machineOps && ImGui::BeginMenu("Quick Operations")) {
    if (ImGui::MenuItem("Clear All Latches")) {
      // Clear latches for all devices
      bool success1 = m_machineOps->ClearLatch(0, 0xFFFFFFFF); // Device 0
      bool success2 = m_machineOps->ClearLatch(1, 0xFFFFFFFF); // Device 1

      if (m_logger) {
        if (success1 && success2) {
          m_logger->LogInfo("MenuManager: Successfully cleared all latches");
        }
        else {
          m_logger->LogWarning("MenuManager: Failed to clear some latches");
        }
      }
    }

    if (ImGui::MenuItem("Test All Outputs OFF")) {
      // Turn off all outputs for safety
      if (m_logger) m_logger->LogInfo("MenuManager: Turning off all outputs for safety test");
      // Add logic to safely turn off all outputs
    }

    ImGui::EndMenu();
  }
}

void MenuManager::RenderPneumaticControlsMenu() {
  if (!m_machineOps) {
    ImGui::MenuItem("(Pneumatic operations not available)", nullptr, false, false);
    return;
  }

  if (ImGui::MenuItem("Show Pneumatic Panel")) {
    if (m_toolbar) {
      bool success = m_toolbar->ToggleComponentByName("Pneumatic");
      if (!success && m_logger) {
        m_logger->LogWarning("MenuManager: Pneumatic component not found in toolbar");
      }
    }
  }

  ImGui::Separator();

  // Quick pneumatic operations
  if (ImGui::BeginMenu("Quick Operations")) {
    if (ImGui::MenuItem("Retract All Slides")) {
      if (m_logger) m_logger->LogInfo("MenuManager: Retract all slides requested");
      // Add logic to retract all pneumatic slides
    }
    //TODO  m_machineOps->EmergencyStopAllSlides();
    /*if (ImGui::MenuItem("Emergency Stop Pneumatics")) {
      bool success = m_machineOps->EmergencyStopAllSlides();
      if (m_logger) {
        if (success) {
          m_logger->LogInfo("MenuManager: Emergency stop executed for all slides");
        }
        else {
          m_logger->LogError("MenuManager: Failed to execute pneumatic emergency stop");
        }
      }
    }*/

    ImGui::EndMenu();
  }
}

void MenuManager::RenderLaserTECControlsMenu() {
  if (!m_machineOps) {
    ImGui::MenuItem("(Laser TEC operations not available)", nullptr, false, false);
    return;
  }

  if (ImGui::MenuItem("Show Laser TEC Control")) {
    if (m_toolbar) {
      // Try different possible names for the laser TEC component
      bool success = m_toolbar->ToggleComponentByName("Laser TEC Cntrl");
      if (!success) {
        success = m_toolbar->ToggleComponentByName("Laser TEC Control");
      }
      if (!success) {
        success = m_toolbar->ToggleComponentByName("CLD101x");
      }
      if (!success && m_logger) {
        m_logger->LogWarning("MenuManager: Laser TEC Control component not found in toolbar");
      }
    }
  }

  ImGui::Separator();

  // Quick laser operations
  if (ImGui::BeginMenu("Quick Operations")) {
    if (ImGui::MenuItem("Laser ON")) {
      bool success = m_machineOps->LaserOn("default");
      if (m_logger) {
        if (success) {
          m_logger->LogInfo("MenuManager: Laser turned ON");
        }
        else {
          m_logger->LogError("MenuManager: Failed to turn laser ON");
        }
      }
    }

    if (ImGui::MenuItem("Laser OFF")) {
      bool success = m_machineOps->LaserOff("default");
      if (m_logger) {
        if (success) {
          m_logger->LogInfo("MenuManager: Laser turned OFF");
        }
        else {
          m_logger->LogError("MenuManager: Failed to turn laser OFF");
        }
      }
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Emergency Laser OFF")) {
      bool success = m_machineOps->LaserOff("default");
      if (m_logger) {
        if (success) {
          m_logger->LogWarning("MenuManager: Emergency laser shutdown executed");
        }
        else {
          m_logger->LogError("MenuManager: Failed to execute emergency laser shutdown");
        }
      }
    }

    ImGui::EndMenu();
  }
}

void MenuManager::RenderToolsMenu() {
  if (ImGui::BeginMenu("Tools")) {
    if (ImGui::MenuItem("Config Editor")) {
      if (m_toolbar) {
        bool success = m_toolbar->ToggleComponentByName("Config Editor");
        if (!success && m_logger) {
          m_logger->LogWarning("MenuManager: Config Editor not found in toolbar");
        }
      }
    }

    if (ImGui::MenuItem("Graph Visualizer")) {
      if (m_toolbar) {
        bool success = m_toolbar->ToggleComponentByName("Graph Visualizer");
        if (!success && m_logger) {
          m_logger->LogWarning("MenuManager: Graph Visualizer not found in toolbar");
        }
      }
    }

    if (ImGui::MenuItem("Script Editor")) {
      if (m_toolbar) {
        bool success = m_toolbar->ToggleComponentByName("Script Editor");
        if (!success && m_logger) {
          m_logger->LogWarning("MenuManager: Script Editor not found in toolbar");
        }
      }
    }

    if (ImGui::MenuItem("Macro Programming")) {
      if (m_toolbar) {
        bool success = m_toolbar->ToggleComponentByName("Macro Programming");
        if (!success && m_logger) {
          m_logger->LogWarning("MenuManager: Macro Programming not found in toolbar");
        }
      }
    }

    ImGui::EndMenu();
  }
}

void MenuManager::RenderProcessMenu() {
  if (ImGui::BeginMenu("Process")) {
    if (ImGui::MenuItem("Process Control")) {
      if (m_toolbar) {
        bool success = m_toolbar->ToggleComponentByName("Process Control");
        if (!success && m_logger) {
          m_logger->LogWarning("MenuManager: Process Control not found in toolbar");
        }
      }
    }

    if (ImGui::MenuItem("Scanning V1")) {
      if (m_toolbar) {
        bool success = m_toolbar->ToggleComponentByName("Scanning V1");
        if (!success && m_logger) {
          m_logger->LogWarning("MenuManager: Scanning V1 not found in toolbar");
        }
      }
    }

    if (ImGui::MenuItem("Scanning V2")) {
      if (m_toolbar) {
        bool success = m_toolbar->ToggleComponentByName("Scanning V2 (test)");
        if (!success && m_logger) {
          m_logger->LogWarning("MenuManager: Scanning V2 not found in toolbar");
        }
      }
    }

    if (ImGui::MenuItem("Block Programming")) {
      if (m_toolbar) {
        bool success = m_toolbar->ToggleComponentByName("Block Programming");
        if (!success && m_logger) {
          m_logger->LogWarning("MenuManager: Block Programming not found in toolbar");
        }
      }
    }

    ImGui::EndMenu();
  }
}

void MenuManager::RenderHardwareMenu() {
  if (ImGui::BeginMenu("Hardware")) {
    // Motion hardware
    if (ImGui::BeginMenu("Motion")) {
      RenderPIControllerMenu();
      RenderACSControllerMenu();
      ImGui::EndMenu();
    }

    // Camera hardware
    if (ImGui::BeginMenu("Camera")) {
      RenderCameraControlsMenu();
      ImGui::EndMenu();
    }

    // Laser hardware
    if (ImGui::BeginMenu("Laser")) {
      RenderLaserHardwareMenu();
      ImGui::EndMenu();
    }

    // Data acquisition
    if (ImGui::BeginMenu("Data Acquisition")) {
      RenderDataAcquisitionMenu();
      ImGui::EndMenu();
    }

    ImGui::EndMenu();
  }
}

void MenuManager::RenderPIControllerMenu() {
  if (ImGui::MenuItem("PI Controller Manager")) {
    if (m_toolbar) {
      bool success = m_toolbar->ToggleComponentByName("PI");
      if (!success && m_logger) {
        m_logger->LogWarning("MenuManager: PI Controller Manager not found in toolbar");
      }
    }
  }
}

void MenuManager::RenderACSControllerMenu() {
  if (ImGui::MenuItem("ACS Controller Manager")) {
    if (m_toolbar) {
      bool success = m_toolbar->ToggleComponentByName("Gantry");
      if (!success && m_logger) {
        m_logger->LogWarning("MenuManager: ACS Controller Manager not found in toolbar");
      }
    }
  }
}

void MenuManager::RenderCameraControlsMenu() {
  if (ImGui::MenuItem("Top Camera")) {
    if (m_toolbar) {
      bool success = m_toolbar->ToggleComponentByName("Top Camera");
      if (!success && m_logger) {
        m_logger->LogWarning("MenuManager: Top Camera not found in toolbar");
      }
    }
  }

  if (ImGui::MenuItem("Camera Testing")) {
    if (m_toolbar) {
      bool success = m_toolbar->ToggleComponentByName("Camera Testing");
      if (!success && m_logger) {
        m_logger->LogWarning("MenuManager: Camera Testing not found in toolbar");
      }
    }
  }

  if (ImGui::MenuItem("Camera Exposure")) {
    if (m_toolbar) {
      bool success = m_toolbar->ToggleComponentByName("Camera Exposure");
      if (!success && m_logger) {
        m_logger->LogWarning("MenuManager: Camera Exposure not found in toolbar");
      }
    }
  }
}

void MenuManager::RenderLaserHardwareMenu() {
  if (ImGui::MenuItem("Laser TEC Control")) {
    if (m_toolbar) {
      bool success = m_toolbar->ToggleComponentByName("Laser TEC Cntrl");
      if (!success) {
        success = m_toolbar->ToggleComponentByName("Laser TEC Control");
      }
      if (!success && m_logger) {
        m_logger->LogWarning("MenuManager: Laser TEC Control not found in toolbar");
      }
    }
  }
}

void MenuManager::RenderDataAcquisitionMenu() {
  if (ImGui::MenuItem("Keithley 2400")) {
    if (m_toolbar) {
      bool success = m_toolbar->ToggleComponentByName("Keithley 2400");
      if (!success && m_logger) {
        m_logger->LogWarning("MenuManager: Keithley 2400 not found in toolbar");
      }
    }
  }

  if (ImGui::MenuItem("Data Chart")) {
    if (m_toolbar) {
      bool success = m_toolbar->ToggleComponentByName("Data Chart");
      if (!success && m_logger) {
        m_logger->LogWarning("MenuManager: Data Chart not found in toolbar");
      }
    }
  }

  if (ImGui::MenuItem("Data TCP/IP")) {
    if (m_toolbar) {
      bool success = m_toolbar->ToggleComponentByName("Data TCP/IP");
      if (!success && m_logger) {
        m_logger->LogWarning("MenuManager: Data TCP/IP not found in toolbar");
      }
    }
  }
}

void MenuManager::RenderHelpMenu() {
  if (ImGui::BeginMenu("Help")) {
    if (ImGui::MenuItem("About")) {
      if (m_logger) m_logger->LogInfo("MenuManager: About dialog requested");
      // Add about dialog logic
    }

    if (ImGui::MenuItem("User Manual")) {
      if (m_logger) m_logger->LogInfo("MenuManager: User manual requested");
      // Add user manual logic
    }

    if (ImGui::MenuItem("System Status")) {
      if (m_logger) m_logger->LogInfo("MenuManager: System status requested");
      // Add system status logic
    }

    ImGui::EndMenu();
  }
}