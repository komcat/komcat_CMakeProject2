#pragma once
// Add this include at the top:
#include "PIPanelUI.h"
#include "ACSPanelUI.h"
#include <memory>
#include "IOPanelUI.h"
#include "UIPneumaticPanel.h"
#include "UICameraPanel.h"
#include "include/data/data_client_manager.h"
#include "include/machine_operations.h"
#include "DataInstrumentUI.h"  // NEW: Replace old UI includes

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
class DataInstrumentModuleManager;  // NEW

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

    enum class ManualSubPage {
        NONE,
        PI,
        GANTRY,
        IO,
        PNEUMATIC,
        CAMERA
    };

    enum class ConfigSubPage {
        NONE,
        CONFIG_EDITOR,
        NODE_VISUALIZER
    };

    // REMOVED: DataInstrumentSubPage enum - handled by DataInstrumentUI now

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
    void SetMachineOperations(MachineOperations* machineOps);

    // NEW: Methods to access initialized data instrument modules
    DataInstrumentModuleManager* GetDataInstrumentModuleManager() const;
    DataClientManager* GetTcpDataManager() const;
    CLD101xManager* GetCLD101xManager() const;
    Keithley2400Manager* GetKeithleyManager() const;
    MachineOperations* GetMachineOperations();

private:
    MainPage currentMainPage = MainPage::MAIN;
    ManualSubPage currentManualSubPage = ManualSubPage::NONE;
    ConfigSubPage currentConfigSubPage = ConfigSubPage::NONE;
    // REMOVED: DataInstrumentSubPage currentDataInstrumentSubPage

    // Reference to the config manager (owned by main)
    MotionConfigManager& motionConfigManager;

    // UI components we own
    std::unique_ptr<UIConfigEditor> uiConfigEditor;
    std::unique_ptr<UIConfigVisualizer> uiConfigVisualizer;
    std::unique_ptr<DataInstrumentUI> m_dataInstrumentUI;  // NEW: Replaces old UIs

    std::unique_ptr<PIPanelUI> m_piPanelUI;
    std::unique_ptr<ACSPanelUI> m_acsPanelUI;
    std::unique_ptr<IOPanelUI> m_ioPanelUI;
    std::unique_ptr<UIJogWindow> m_uiJogWindow;
    std::unique_ptr<UIPneumaticPanel> m_pneumaticPanelUI;
    std::unique_ptr<UICameraPanel> m_cameraPanelUI;

    // Motion managers (optional, set later)
    PIControllerManager* m_piControllerManager = nullptr;
    ACSControllerManager* m_acsControllerManager = nullptr;
    EziIOManager* m_ioManager = nullptr;
    IOConfigManager* m_ioConfigManager = nullptr;
    PneumaticManager* m_pneumaticManager = nullptr;
    CameraManager* m_cameraManager = nullptr;
    MachineOperations* m_machineOperations = nullptr;

    // UI rendering methods
    void RenderTopMenuBar();
    void RenderBackButton();
    void RenderDateTime();
    void RenderBreadcrumbs();
    void RenderMainContent();
    void RenderGlobalJogWindow();

    // Main page rendering
    void RenderMainPage();
    void RenderManualPage();
    void RenderDataInstrumentPage();  // SIMPLIFIED - just calls m_dataInstrumentUI->Render()
    void RenderRunProgramPage();
    void RenderConfigPage();
    void RenderVisionPage();

    // Sub-page rendering
    void RenderManualSubPage();
    void RenderConfigSubPage();
    // REMOVED: RenderDataInstrumentSubPage() - not needed

    // Manual sub-pages
    void RenderPIPage();
    void RenderGantryPage();
    void RenderIOPage();
    void RenderPneumaticPage();
    void RenderCameraPage();

    // Config sub-pages
    void RenderConfigEditorPage();
    void RenderNodeVisualizerPage();
};