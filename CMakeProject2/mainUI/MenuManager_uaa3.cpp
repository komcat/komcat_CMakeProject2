#include "MenuManager_uaa3.h"
#include "imgui.h"
#include "Version.h"
#include "include/logger.h"

MenuManagerUaa3::MenuManagerUaa3() {
}

MenuManagerUaa3::~MenuManagerUaa3() {
}

void MenuManagerUaa3::RegisterUI(const std::string& id, IImguiUI* ui, const std::string& menuCategory) {
  if (!ui) {
    Logger::GetInstance()->LogWarning("Attempted to register null UI with ID: " + id);
    return;
  }

  UIEntry entry;
  entry.rawPtr = ui;
  entry.menuCategory = menuCategory;
  entry.isShared = false;

  m_registeredUIs[id] = entry;
  Logger::GetInstance()->LogInfo("Registered UI: " + id + " (" + ui->GetName() + ") in category: " + menuCategory);
}

void MenuManagerUaa3::RegisterUI(const std::string& id, std::shared_ptr<IImguiUI> ui, const std::string& menuCategory) {
  if (!ui) {
    Logger::GetInstance()->LogWarning("Attempted to register null shared UI with ID: " + id);
    return;
  }

  UIEntry entry;
  entry.sharedPtr = ui;
  entry.rawPtr = ui.get();
  entry.menuCategory = menuCategory;
  entry.isShared = true;

  m_registeredUIs[id] = entry;
  Logger::GetInstance()->LogInfo("Registered shared UI: " + id + " (" + ui->GetName() + ") in category: " + menuCategory);
}

void MenuManagerUaa3::UnregisterUI(const std::string& id) {
  auto it = m_registeredUIs.find(id);
  if (it != m_registeredUIs.end()) {
    m_registeredUIs.erase(it);
    Logger::GetInstance()->LogInfo("Unregistered UI: " + id);
  }
}

void MenuManagerUaa3::ShowUI(const std::string& id) {
  auto it = m_registeredUIs.find(id);
  if (it != m_registeredUIs.end() && it->second.rawPtr) {
    it->second.rawPtr->Show();
  }
}

void MenuManagerUaa3::HideUI(const std::string& id) {
  auto it = m_registeredUIs.find(id);
  if (it != m_registeredUIs.end() && it->second.rawPtr) {
    it->second.rawPtr->Hide();
  }
}

void MenuManagerUaa3::ToggleUI(const std::string& id) {
  auto it = m_registeredUIs.find(id);
  if (it != m_registeredUIs.end() && it->second.rawPtr) {
    it->second.rawPtr->Toggle();
  }
}

void MenuManagerUaa3::ToggleUI(IImguiUI* ui) {
  if (ui) {
    ui->Toggle();
  }
}

IImguiUI* MenuManagerUaa3::GetUI(const std::string& id) {
  auto it = m_registeredUIs.find(id);
  if (it != m_registeredUIs.end()) {
    return it->second.rawPtr;
  }
  return nullptr;
}

std::shared_ptr<IImguiUI> MenuManagerUaa3::GetSharedUI(const std::string& id) {
  auto it = m_registeredUIs.find(id);
  if (it != m_registeredUIs.end() && it->second.isShared) {
    return it->second.sharedPtr;
  }
  return nullptr;
}

void MenuManagerUaa3::RenderRegisteredUIs() {
  for (auto& [id, entry] : m_registeredUIs) {
    if (entry.rawPtr && entry.rawPtr->IsVisible()) {
      entry.rawPtr->Render();
    }
  }
}

void MenuManagerUaa3::RenderMainMenuBar() {
  if (ImGui::BeginMainMenuBar()) {
    //RenderFileMenu();
    RenderRaylibMenu();
    RenderWindowsMenu();  // Add this to show registered windows
    //RenderDebugMenu();
    //RenderHelpMenu();

    ImGui::EndMainMenuBar();
  }

  // Render dialogs that might be opened from menus
  RenderAboutDialog();
}

void MenuManagerUaa3::RenderWindowsMenu() {
  // Group UIs by category
  std::unordered_map<std::string, std::vector<std::string>> categorizedUIs;

  for (const auto& [id, entry] : m_registeredUIs) {
    categorizedUIs[entry.menuCategory].push_back(id);
  }

  // Render each category as a menu
  for (const auto& [category, uiIds] : categorizedUIs) {
    if (ImGui::BeginMenu(category.c_str())) {
      for (const auto& id : uiIds) {
        auto it = m_registeredUIs.find(id);
        if (it != m_registeredUIs.end() && it->second.rawPtr) {
          bool visible = it->second.rawPtr->IsVisible();
          if (ImGui::MenuItem(it->second.rawPtr->GetName().c_str(), nullptr, &visible)) {
            ToggleUI(id);
          }
        }
      }
      ImGui::EndMenu();
    }
  }
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

    // Legacy menu items - these could be replaced with registered UIs
    ImGui::MenuItem("Debug Grid Scanner", nullptr, &m_showGridScannerUI);
    ImGui::Separator();
    ImGui::MenuItem("Debug Grid Volume Scanner", nullptr, &m_showGridVolumeScannerUI);

    // Also show any registered UIs in the "Raylib" category
    ImGui::Separator();
    for (const auto& [id, entry] : m_registeredUIs) {
      if (entry.menuCategory == "Raylib" && entry.rawPtr) {
        bool visible = entry.rawPtr->IsVisible();
        if (ImGui::MenuItem(entry.rawPtr->GetName().c_str(), nullptr, &visible)) {
          ToggleUI(id);
        }
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

    // Show any registered UIs in the "Debug" category
    ImGui::Separator();
    for (const auto& [id, entry] : m_registeredUIs) {
      if (entry.menuCategory == "Debug" && entry.rawPtr) {
        bool visible = entry.rawPtr->IsVisible();
        if (ImGui::MenuItem(entry.rawPtr->GetName().c_str(), nullptr, &visible)) {
          ToggleUI(id);
        }
      }
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