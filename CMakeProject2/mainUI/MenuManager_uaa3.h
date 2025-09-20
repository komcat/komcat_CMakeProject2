#pragma once
#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "include/camera/IDSCameraUI.h"

// Generic interface for ImGui UI components
class IImguiUI {
public:
  virtual ~IImguiUI() = default;

  // Core UI functions that all ImGui windows should implement
  virtual void Render() = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual bool IsVisible() const = 0;

  // Optional: Get window name/title
  virtual const std::string& GetName() const {
    static std::string defaultName = "ImGui Window";
    return defaultName;
  }

  // Optional: Toggle visibility (can be overridden if needed)
  virtual void Toggle() {
    if (IsVisible()) {
      Hide();
    }
    else {
      Show();
    }
  }
};

class MenuManagerUaa3 {
public:
  MenuManagerUaa3();
  ~MenuManagerUaa3();

  void RenderMainMenuBar();

  // Window visibility controls
  bool IsRaylibDebugVisible() const { return m_showRaylibDebug; }
  void SetRaylibDebugVisible(bool visible) { m_showRaylibDebug = visible; }
  void ToggleRaylibDebug() { m_showRaylibDebug = !m_showRaylibDebug; }

  // Callbacks for menu actions
  void SetOnExitCallback(std::function<void()> callback) { m_onExit = callback; }
  void SetIDSCameraUI(IDSCameraUI* ui) { m_idsCameraUI = ui; }

  // Generic UI registration system
  void RegisterUI(const std::string& id, IImguiUI* ui, const std::string& menuCategory = "Windows");
  void RegisterUI(const std::string& id, std::shared_ptr<IImguiUI> ui, const std::string& menuCategory = "Windows");
  void UnregisterUI(const std::string& id);

  // UI control methods
  void ShowUI(const std::string& id);
  void HideUI(const std::string& id);
  void ToggleUI(const std::string& id);
  void ToggleUI(IImguiUI* ui);

  // Get registered UI
  IImguiUI* GetUI(const std::string& id);
  std::shared_ptr<IImguiUI> GetSharedUI(const std::string& id);

  // Render all registered UIs
  void RenderRegisteredUIs();

  // Set a UI instance directly (backwards compatibility)
  template<typename T>
  void SetUI(const std::string& id, T* ui, const std::string& menuCategory = "Windows") {
    static_assert(std::is_base_of<IImguiUI, T>::value, "UI must implement IImguiUI interface");
    RegisterUI(id, ui, menuCategory);
  }

  template<typename T>
  void SetUI(const std::string& id, std::shared_ptr<T> ui, const std::string& menuCategory = "Windows") {
    static_assert(std::is_base_of<IImguiUI, T>::value, "UI must implement IImguiUI interface");
    RegisterUI(id, std::static_pointer_cast<IImguiUI>(ui), menuCategory);
  }

private:
  bool m_showRaylibDebug = false;
  bool m_showAbout = false;
  bool m_showDemo = false;
  bool m_showGridVolumeScannerUI = false;
  bool m_showGridScannerUI = false;

  std::function<void()> m_onExit;

  // UI registry
  struct UIEntry {
    IImguiUI* rawPtr = nullptr;
    std::shared_ptr<IImguiUI> sharedPtr;
    std::string menuCategory;
    bool isShared = false;
  };

  std::unordered_map<std::string, UIEntry> m_registeredUIs;

  void RenderFileMenu();
  void RenderRaylibMenu();
  void RenderDebugMenu();
  void RenderHelpMenu();
  void RenderAboutDialog();
  void RenderWindowsMenu();

  IDSCameraUI* m_idsCameraUI = nullptr;
};