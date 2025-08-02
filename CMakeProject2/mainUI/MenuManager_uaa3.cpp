#include "MenuManager_uaa3.h"
#include "imgui.h"
#include "Version.h"

MenuManagerUaa3::MenuManagerUaa3() {
}

MenuManagerUaa3::~MenuManagerUaa3() {
}



void MenuManagerUaa3::RenderMainMenuBar() {
  if (ImGui::BeginMainMenuBar()) {
    //RenderFileMenu();
    RenderRaylibMenu();
    //RenderDebugMenu();
    //RenderHelpMenu();

    ImGui::EndMainMenuBar();
  }

  // Render dialogs that might be opened from menus
  RenderAboutDialog();
}

void MenuManagerUaa3::RenderFileMenu() {
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

void MenuManagerUaa3::RenderRaylibMenu() {
  if (ImGui::BeginMenu("Raylib")) {
    ImGui::MenuItem("Live Feed Debug", nullptr, &m_showRaylibDebug);

    ImGui::Separator();

    // Add IDS Camera Test option
    if (ImGui::MenuItem("IDS Camera Test", nullptr,
      m_idsCameraUI ? m_idsCameraUI->IsVisible() : false)) {
      if (m_idsCameraUI) {
        m_idsCameraUI->ToggleVisibility();
      }
    }

    ImGui::EndMenu();
  }
}

void MenuManagerUaa3::RenderDebugMenu() {
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

void MenuManagerUaa3::RenderHelpMenu() {
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

void MenuManagerUaa3::RenderAboutDialog() {
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