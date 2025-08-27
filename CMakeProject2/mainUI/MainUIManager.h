#pragma once
#include "AppContext.h" 
// Add this include at the top:
#include "PIPanelUI.h"
#include "ACSPanelUI.h"
#include <memory>
#include "IOPanelUI.h"
#include "UIPneumaticPanel.h"
#include "UICameraPanel.h"
#include "TCPDataManagerUI.h"
#include "include/data/data_client_manager.h"
#include "CLD101xEquipmentUI.h"
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
#include "UIVisionPanel.h"
#include "DatumUI.h"  // ADD THIS LINE - Include new DatumUI class
#include "ModuleAlignmentUI.h"
#include "mainUI/SPDPowerSupplyUI.h"


// Forward declarations
class MotionConfigManager;
class UIConfigEditor;
class UIConfigVisualizer;
class UIJogWindow;
class PIControllerManager;
class ACSControllerManager;
class VisionOps;
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
    SMU_MANAGER,
    SPD_POWER_SUPPLY  // ADD THIS
  };

  enum class VisionSubPage {
    NONE = 0,
    FIDUCIAL,
    DATUM_REFERENCE,
    MODULE_ALIGNMENT
  };

public:
  // DUAL CONSTRUCTORS - Both supported for smooth migration

  // Constructor 1: Original constructor (backward compatibility)
  MainUIManager(MotionConfigManager& configManager);

  // Constructor 2: New AppContext constructor
  explicit MainUIManager(AppContext& context);

  ~MainUIManager();

  void RenderUI();

  // KEEP ALL EXISTING SETTER METHODS (for backward compatibility with Constructor 1)
  void SetPIControllerManager(PIControllerManager* piManager);
  void SetACSControllerManager(ACSControllerManager* acsManager);
  void SetIOManager(EziIOManager* ioManager, IOConfigManager* ioConfigManager = nullptr);
  void SetPneumaticManager(PneumaticManager* pneumaticManager);
  void SetCameraManager(CameraManager* cameraManager);
  void SetDataClientManager(DataClientManager* dataClientManager);
  void SetCLD101xManager(CLD101xManager* cld101xManager);
  void SetKeithley2400Manager(Keithley2400Manager* keithleyManager);
  void SetMachineOperations(MachineOperations* machineOps);
  void SetConfigWatchdog(ConfigFileWatchdog* watchdog);

  // Keep utility methods
  void SetImguiFont(ImFont* font);
  ImFont* GetImguiFont() const { return m_imguiFont; }

  // Keep getters
  RunPageUI* GetRunPageUI() const { return m_runPageUI.get(); }
  UserPromptUI* GetUserPromptUI() const { return m_promptUI.get(); }
  UIVisionPanel* GetVisionPanel() const { return m_visionPanelUI.get(); }
	CLD101xEquipmentUI* GetCLD101xEquipmentUI() const { return m_cld101xEquipmentUI.get(); }
  DatumUI* GetDatumUI() const { return m_datumUI.get(); }
  MachineOperations* GetMachineOperations();

  void SetupVisionPanel();
  void RenderWatchdogStatus(ConfigFileWatchdog* watchdog);
  void ProcessKeyInput(SDL_Keycode key, bool pressed);

private:
  // UI State
  MainPage currentMainPage = MainPage::MAIN;
  ManualSubPage currentManualSubPage = ManualSubPage::NONE;
  ConfigSubPage currentConfigSubPage = ConfigSubPage::NONE;
  DataInstrumentSubPage currentDataInstrumentSubPage = DataInstrumentSubPage::NONE;
  ProgrammingSubPage currentProgrammingSubPage = ProgrammingSubPage::NONE;
  VisionSubPage currentVisionSubPage = VisionSubPage::NONE;

  // DUAL APPROACH MEMBERS
  // Reference to MotionConfigManager (always required)
  MotionConfigManager& motionConfigManager;

  // Optional pointer to AppContext (nullptr if using old constructor)
  AppContext* m_context = nullptr;

  // === SMART GETTER METHODS ===
  // These try AppContext first, then fall back to old member variables
  PIControllerManager* GetPIController() const;
  ACSControllerManager* GetACSController() const;
  CameraManager* GetCameraManagerSmart() const;
  EziIOManager* GetIOManager() const;
  IOConfigManager* GetIOConfig() const;
  PneumaticManager* GetPneumaticManager() const;
  DataClientManager* GetDataClient() const;
  Keithley2400Manager* GetKeithley() const;
  CLD101xManager* GetCLD101x() const;
  SPDPowerSupplyManager* GetSPDManager() const;

  // === KEEP OLD MEMBER VARIABLES (for backward compatibility) ===
  PIControllerManager* m_piControllerManager = nullptr;
  ACSControllerManager* m_acsControllerManager = nullptr;
  EziIOManager* m_ioManager = nullptr;
  IOConfigManager* m_ioConfigManager = nullptr;
  PneumaticManager* m_pneumaticManager = nullptr;
  CameraManager* m_cameraManager = nullptr;
  DataClientManager* m_dataClientManager = nullptr;
  CLD101xManager* m_cld101xManager = nullptr;
  Keithley2400Manager* m_keithleyManager = nullptr;
  MachineOperations* m_machineOperations = nullptr;
  ConfigFileWatchdog* m_configWatchdog = nullptr;

  // === UI COMPONENTS (unchanged) ===
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
  std::unique_ptr<ModuleAlignmentUI> m_moduleAlignmentUI;
  std::unique_ptr<UISMUPanel> m_smuPanelUI;
  std::unique_ptr<IOControlPanel> m_ioControlPanel;
  std::unique_ptr<MachineBlockUI> m_machineBlockUI;
  std::unique_ptr<MacroManager> m_macroManager;
  std::unique_ptr<RunPageUI> m_runPageUI;
  std::unique_ptr<UserPromptUI> m_userPromptUI;
  std::unique_ptr<UserPromptUI> m_promptUI;
  std::unique_ptr<UIVisionPanel> m_visionPanelUI;
  std::unique_ptr<DatumUI> m_datumUI;
	std::unique_ptr<CLD101xEquipmentUI> m_cld101xEquipmentUI;
  std::unique_ptr<SPDPowerSupplyUI> m_spdPowerSupplyUI;

  // Utility members
  ImFont* m_imguiFont = nullptr;
  bool m_showGlobalJogWindow = false;

  // === HELPER METHODS ===
  void InitializeUIComponents();
  void ConnectUIToServices();

  // All your existing render methods (unchanged)
  void RenderTopMenuBar();
  void RenderDateTime();
  void RenderBreadcrumbs();
  void RenderMainContent();
  void RenderBackButton();

  void RenderMainPage();
  void RenderManualPage();
  void RenderDataInstrumentPage();
  void RenderRunProgramPage();
  void RenderConfigPage();
  void RenderVisionPage();
  void RenderProgrammingPage();

  void RenderManualSubPage();
  void RenderPIPage();
  void RenderGantryPage();
  void RenderIOPage();
  void RenderCameraPage();
  void RenderPneumaticPage();

  void RenderConfigSubPage();
  void RenderConfigEditorPage();
  void RenderNodeVisualizerPage();

  void RenderDataInstrumentSubPage();
  void RenderGlobalDataStorePage();
  void RenderTcpDataManagerPage();
  void RenderCld101xEquipmentPage();
  void RenderSmuManagerPage();
  void RenderSPDPowerSupplyPage();

  void RenderProgrammingSubPage();
  void RenderMachineBlockPage();
  void RenderMacroManagerPage();

  void RenderVisionSubPage();
  void RenderFiducialPage();
  void RenderDatumReferencePage();
  void RenderModuleAlignmentPage();

  void RenderGlobalJogWindow();
  void CreateIOControlPanel();


};