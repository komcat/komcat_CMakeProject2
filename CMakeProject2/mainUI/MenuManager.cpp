#include "MenuManager.h"
#include "imgui.h"
#include "Version.h"

MenuManager::MenuManager() {
}

MenuManager::~MenuManager() {
}

void MenuManager::RenderMainMenuBar() {
  if (ImGui::BeginMainMenuBar()) {
    RenderFileMenu();
    RenderRaylibMenu();
    RenderDebugMenu();
    RenderHelpMenu();

    ImGui::EndMainMenuBar();
  }

  // Render dialogs that might be opened from menus
  RenderAboutDialog();
}

void MenuManager::RenderFileMenu() {
  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("New Project", "Ctrl+N")) {
      // TODO: Implement new project
    }

    if (ImGui::MenuItem("Open Project", "Ctrl+O")) {
      // TODO: Implement open project
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Exit", "Alt+F4")) {
      if (m_onExit) {
        m_onExit();
      }
    }

    ImGui::EndMenu();
  }
}

void MenuManager::RenderRaylibMenu() {
  if (ImGui::BeginMenu("Raylib")) {
    ImGui::MenuItem("Live Feed Debug", nullptr, &m_showRaylibDebug);

    ImGui::Separator();

    if (ImGui::MenuItem("Show 3D Window")) {
      // TODO: Show/focus raylib window
    }

    if (ImGui::MenuItem("Hide 3D Window")) {
      // TODO: Hide raylib window
    }

    ImGui::EndMenu();
  }
}

void MenuManager::RenderDebugMenu() {
  if (ImGui::BeginMenu("Debug")) {
    ImGui::MenuItem("Show Demo Window", nullptr, &m_showDemo);

    if (ImGui::MenuItem("Clear Logs")) {
      // TODO: Clear logger
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Motion Test Window")) {
      // TODO: Show motion test window
    }

    ImGui::EndMenu();
  }

  // Render debug windows if enabled
  if (m_showDemo) {
    ImGui::ShowDemoWindow(&m_showDemo);
  }
}

void MenuManager::RenderHelpMenu() {
  if (ImGui::BeginMenu("Help")) {
    if (ImGui::MenuItem("About")) {
      m_showAbout = true;
    }

    if (ImGui::MenuItem("User Manual")) {
      // TODO: Open documentation
    }

    ImGui::EndMenu();
  }
}

void MenuManager::RenderAboutDialog() {
  if (m_showAbout) {
    ImGui::OpenPopup("About");
  }

  if (ImGui::BeginPopupModal("About", &m_showAbout, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("%s", Version::getWindowTitle().c_str());
    ImGui::Text("Version: %s", Version::getFullVersionString().c_str());
    ImGui::Text("Build: %s", Version::getVersionWithBuildInfo().c_str());

    ImGui::Separator();
    ImGui::Text("3D Machine Control System");
    ImGui::Text("Built with Raylib + ImGui");

    ImGui::Separator();
    if (ImGui::Button("Close")) {
      m_showAbout = false;
    }

    ImGui::EndPopup();
  }
}