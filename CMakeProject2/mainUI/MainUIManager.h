#pragma once
// Add this include at the top:
#include "PIPanelUI.h"
#include "ACSPanelUI.h"
#include <memory>
#include "IOPanelUI.h"
#include "UIPneumaticPanel.h"
#include "UICameraPanel.h"
#include "TCPDataManagerUI.h"
#include "include/data/data_client_manager.h"
#include "include/machine_operations.h"
#include "GlobalDataStoreViewerUI.h"
//#include "CLD101xEquipmentUI.h"
#include "UISMUPanel.h"
#include "Programming/MachineBlockUI.h"
#include "Programming/MacroManager.h"
#include "MacroPanelUI.h"
#include "RunPageUI.h"
#include "ConfigFileWatchdog.h"
#include "Processes/SAA3ProcessBuilders/NewProcesses_SAA3.h"
#include <SDL.h>
#include "include/eziio/IOControlPanel.h"

// Forward declarations
class MotionConfigManager;
class UIConfigEditor;
class UIConfigVisualizer;
class UIJogWindow;
class PIControllerManager;
class ACSControllerManager;

class PneumaticManager;
class EziIOManager;
class IOConfigManager;
class CameraManager;
class CLD101xManager;
class Keithley2400Manager;

class MainUIManager {
public:
  enum class MainPage {
    MAIN,
    MANUAL,
    DATA_INSTRUMENT,
    RUN_PROGRAM,
    CONFIG,
    VISION,
    PROGRAMMING    // Add this new page
  };

  // Add new enum for Programming sub-pages:
  enum class ProgrammingSubPage {
    NONE,
    MACHINE_BLOCK_UI,
    MACRO_MANAGER
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

  enum class DataInstrumentSubPage {
    NONE,
    GLOBAL_DATA_STORE,
    TCP_DATA_MANAGER,
    CLD101X_EQUIPMENT,    // RENAMED from CLD101X_TEC
    SMU_MANAGER
  };

public:
  // Constructor takes MotionConfigManager reference only
  MainUIManager(MotionConfigManager& configManager);
  ~MainUIManager();

  void RenderUI();

  // Method to set motion managers separately for cleaner initialization
  void SetPIControllerManager(PIControllerManager* piManager);
  void SetACSControllerManager(ACSControllerManager* acsManager);
  void SetIOManager(EziIOManager* ioManager, IOConfigManager* ioConfigManager = nullptr);
  void SetPneumaticManager(PneumaticManager* pneumaticManager);
  void SetCameraManager(CameraManager* cameraManager);
  void SetDataClientManager(DataClientManager* dataClientManager);
  void SetCLD101xManager(CLD101xManager* cld101xManager);
  void SetImguiFont(ImFont* font);
  ImFont* GetImguiFont() const { return m_imguiFont; }

  // Add this method in the public section with other setter methods:
  void SetKeithley2400Manager(Keithley2400Manager* keithleyManager);

  // Add this method declaration
  void SetMachineOperations(MachineOperations* machineOps);
  MachineOperations* GetMachineOperations();

  // NEW: Add getter for RunPageUI
  RunPageUI* GetRunPageUI() const { return m_runPageUI.get(); }

  // NEW: Add getter for UserPromptUI (optional, for debugging)
  UserPromptUI* GetUserPromptUI() const { return m_userPromptUI.get(); }
  void SetConfigWatchdog(ConfigFileWatchdog* watchdog);
  void RenderWatchdogStatus(ConfigFileWatchdog* watchdog);

  // Add this method for keyboard input processing
  void ProcessKeyInput(SDL_Keycode key, bool pressed);

private:
  MainPage currentMainPage = MainPage::MAIN;
  ManualSubPage currentManualSubPage = ManualSubPage::NONE;
  ConfigSubPage currentConfigSubPage = ConfigSubPage::NONE;
  DataInstrumentSubPage currentDataInstrumentSubPage = DataInstrumentSubPage::NONE;

  // ADD THIS LINE with other UI components:
  std::unique_ptr<UserPromptUI> m_promptUI;

  ImFont* m_imguiFont = nullptr; // Pointer to ImGui font 

  // Reference to the config manager (owned by main)
  MotionConfigManager& motionConfigManager;

  // UI components we own
  std::unique_ptr<UIConfigEditor> uiConfigEditor;
  std::unique_ptr<UIConfigVisualizer> uiConfigVisualizer;
  std::unique_ptr<TCPDataManagerUI> m_tcpDataManagerUI;
  std::unique_ptr<PIPanelUI> m_piPanelUI;
  std::unique_ptr<ACSPanelUI> m_acsPanelUI;
  std::unique_ptr<IOPanelUI> m_ioPanelUI;
  std::unique_ptr<UIJogWindow> m_uiJogWindow;
  std::unique_ptr<UIPneumaticPanel> m_pneumaticPanelUI;
  std::unique_ptr<UICameraPanel> m_cameraPanelUI;
  std::unique_ptr<GlobalDataStoreViewerUI> m_globalDataStoreViewerUI;
  std::unique_ptr<MacroPanelUI> m_macroPanelUI;

  CLD101xManager* m_cld101xManager = nullptr;

  // Motion managers (optional, set later)
  PIControllerManager* m_piControllerManager = nullptr;
  ACSControllerManager* m_acsControllerManager = nullptr;
  DataClientManager* m_dataClientManager = nullptr;
  Keithley2400Manager* m_keithleyManager = nullptr;
  ConfigFileWatchdog* m_configWatchdog = nullptr;

  // Jog window
  bool m_showGlobalJogWindow = false;

  // Add this member variable in the private section with other UI components:
  std::unique_ptr<UISMUPanel> m_smuPanelUI;

  EziIOManager* m_ioManager = nullptr;
  IOConfigManager* m_ioConfigManager = nullptr;

  // NEW: Add IO Control Panel for Q-IO button
  std::unique_ptr<IOControlPanel> m_ioControlPanel;

  // Add these member variables in the private section (with other panel UIs):
  PneumaticManager* m_pneumaticManager = nullptr;
  CameraManager* m_cameraManager = nullptr;
  MachineOperations* m_machineOperations = nullptr;

  // Add in private section with other member variables:
  ProgrammingSubPage currentProgrammingSubPage = ProgrammingSubPage::NONE;
  std::unique_ptr<MachineBlockUI> m_machineBlockUI;
  std::unique_ptr<MacroManager> m_macroManager;

  void RenderTopMenuBar();
  void RenderDateTime();  // This is where JOG and Q-IO buttons are rendered
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
  void RenderProgrammingPage();

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

  // Data Instrument sub-pages
  void RenderDataInstrumentSubPage();
  void RenderGlobalDataStorePage();
  void RenderTcpDataManagerPage();
  void RenderCld101xEquipmentPage();
  void RenderSmuManagerPage();

  void RenderProgrammingSubPage();
  void RenderMachineBlockPage();
  void RenderMacroManagerPage();

  // NEW: Add these members if not already present
  std::unique_ptr<RunPageUI> m_runPageUI;
  std::unique_ptr<UserPromptUI> m_userPromptUI;

  // NEW: Helper methods for IO Control Panel
  void CreateIOControlPanel();  // Helper to create IO panel when manager is available
};