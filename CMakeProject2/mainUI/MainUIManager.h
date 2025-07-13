#pragma once
// Add this include at the top:
#include "PIPanelUI.h"
#include "ACSPanelUI.h"
#include <memory>
#include "IOPanelUI.h"
// Add this include at the top (with other includes):
#include "UIPneumaticPanel.h"


// Forward declarations
class MotionConfigManager;
class UIConfigEditor;
class UIConfigVisualizer;
class UIJogWindow;
class PIControllerManager;
class ACSControllerManager;

// Add forward declaration (with other forward declarations):
class PneumaticManager;
// Add forward declaration (with other forward declarations):
class EziIOManager;
class IOConfigManager;

class MainUIManager {
public:
  enum class MainPage {
    MAIN,
    MANUAL,
    DATA_INSTRUMENT,
    RUN_PROGRAM,
    CONFIG,
    VISION
  };

  // Update the ManualSubPage enum to include PNEUMATIC:
  enum class ManualSubPage {
    NONE,
    PI,
    GANTRY,
    IO,
    PNEUMATIC,    // Add this new option
    CAMERA
  };


  enum class ConfigSubPage {
    NONE,
    CONFIG_EDITOR,
    NODE_VISUALIZER
  };

public:
  // Constructor takes MotionConfigManager reference only
  MainUIManager(MotionConfigManager& configManager);
  ~MainUIManager();


  void RenderUI();
  // Method to set motion managers separately for cleaner initialization
  void SetPIControllerManager(PIControllerManager* piManager);
  void SetACSControllerManager(ACSControllerManager* acsManager);
  // Add this method declaration (with other SetManager methods):
  void SetIOManager(EziIOManager* ioManager, IOConfigManager* ioConfigManager = nullptr);

  // Add this method declaration (with other SetManager methods):
  void SetPneumaticManager(PneumaticManager* pneumaticManager);

  
private:
  MainPage currentMainPage = MainPage::MAIN;
  ManualSubPage currentManualSubPage = ManualSubPage::NONE;
  ConfigSubPage currentConfigSubPage = ConfigSubPage::NONE;

  // Reference to the config manager (owned by main)
  MotionConfigManager& motionConfigManager;

  // UI components we own
  std::unique_ptr<UIConfigEditor> uiConfigEditor;
  std::unique_ptr<UIConfigVisualizer> uiConfigVisualizer;

  // Motion managers (optional, set later)
  PIControllerManager* m_piControllerManager = nullptr;
  ACSControllerManager* m_acsControllerManager = nullptr;

  // Jog window
  bool m_showGlobalJogWindow = false;
  std::unique_ptr<UIJogWindow> m_uiJogWindow;



  // Add these member variables in the private section:
  std::unique_ptr<PIPanelUI> m_piPanelUI;
  std::unique_ptr<ACSPanelUI> m_acsPanelUI;

  std::unique_ptr<IOPanelUI> m_ioPanelUI;
  EziIOManager* m_ioManager = nullptr;
  IOConfigManager* m_ioConfigManager = nullptr;


  // Add these member variables in the private section (with other panel UIs):
  std::unique_ptr<UIPneumaticPanel> m_pneumaticPanelUI;
  PneumaticManager* m_pneumaticManager = nullptr;


  void RenderTopMenuBar();
  void RenderDateTime();
  void RenderBreadcrumbs();
  void RenderMainContent();
  void RenderBackButton();
  // Main pages
  void RenderMainPage();
  void RenderManualPage();
  void RenderDataInstrumentPage();
  void RenderRunProgramPage();
  void RenderConfigPage();
  void RenderVisionPage();

  // Manual sub-pages
  void RenderManualSubPage();
  void RenderPIPage();
  void RenderGantryPage();
  void RenderIOPage();
  void RenderCameraPage();

  // Config sub-pages
  void RenderConfigSubPage();
  void RenderConfigEditorPage();
  void RenderNodeVisualizerPage();

  // Jog window
  void RenderGlobalJogWindow();

  // Add this method declaration (with other render methods):
  void RenderPneumaticPage();

};