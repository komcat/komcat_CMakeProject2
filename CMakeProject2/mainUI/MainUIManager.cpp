#include "MainUIManager.h"
#include "UIConfigEditor.h"
#include "UIConfigVisualizer.h"
#include "UIJogWindow.h"
#include "include/motions/MotionConfigManager.h"
#include "include/motions/pi_controller_manager.h"
#include "include/motions/acs_controller_manager.h"
#include "PIPanelUI.h"
#include "ACSPanelUI.h"
#include "IOPanelUI.h"
#include "UIPneumaticPanel.h"
#include "include/data/global_data_store.h"
#include "include/machine_operations.h"
#include "implot/implot.h"
#include <deque>
#include <map>
#include <chrono>
#include "imgui.h"
#include <iostream>
#include <string>
#include <ctime>

MainUIManager::MainUIManager(MotionConfigManager& configMgr)
    : motionConfigManager(configMgr),
    m_piControllerManager(nullptr),
    m_acsControllerManager(nullptr) {

    // Create UIConfigEditor with the config manager reference
    uiConfigEditor = std::make_unique<UIConfigEditor>(motionConfigManager);

    // Create UIConfigVisualizer with the config manager reference (camera manager will be set later)
    uiConfigVisualizer = std::make_unique<UIConfigVisualizer>(motionConfigManager, nullptr);

    // Create UIJogWindow using the single-parameter constructor for mock mode
    m_uiJogWindow = std::make_unique<UIJogWindow>(motionConfigManager);

    // NEW: Initialize DataInstrumentUI (replaces old individual UIs)
    m_dataInstrumentUI = std::make_unique<DataInstrumentUI>();
}

MainUIManager::~MainUIManager() = default;

void MainUIManager::RenderUI() {
    // Main window that fills the entire screen
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("MainApplication", nullptr, window_flags);

    // Only show top menu bar when on main page
    if (currentMainPage == MainPage::MAIN) {
        RenderTopMenuBar();
    }
    else {
        // Show back button in top-left when not on main page
        RenderBackButton();
    }

    RenderDateTime();
    RenderBreadcrumbs();
    ImGui::Separator();
    RenderMainContent();

    ImGui::End();

    // Always render jog window (it handles its own visibility internally)
    RenderGlobalJogWindow();
}

void MainUIManager::RenderBackButton() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(15, 8));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));

    if (ImGui::Button("<< BACK")) {
        if (currentManualSubPage != ManualSubPage::NONE) {
            currentManualSubPage = ManualSubPage::NONE;
        }
        else if (currentConfigSubPage != ConfigSubPage::NONE) {
            currentConfigSubPage = ConfigSubPage::NONE;
        }
        else {
            currentMainPage = MainPage::MAIN;
        }
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}

void MainUIManager::RenderTopMenuBar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 10));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.3f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.4f, 0.9f, 1.0f));

    if (ImGui::Button("Manual", ImVec2(120, 40))) {
        currentMainPage = MainPage::MANUAL;
        currentManualSubPage = ManualSubPage::NONE;
        currentConfigSubPage = ConfigSubPage::NONE;
    }

    ImGui::SameLine();
    if (ImGui::Button("Data & Instrument", ImVec2(150, 40))) {
        currentMainPage = MainPage::DATA_INSTRUMENT;
        currentManualSubPage = ManualSubPage::NONE;
        currentConfigSubPage = ConfigSubPage::NONE;
    }

    ImGui::SameLine();
    if (ImGui::Button("Run Program", ImVec2(120, 40))) {
        currentMainPage = MainPage::RUN_PROGRAM;
        currentManualSubPage = ManualSubPage::NONE;
        currentConfigSubPage = ConfigSubPage::NONE;
    }

    ImGui::SameLine();
    if (ImGui::Button("Config", ImVec2(120, 40))) {
        currentMainPage = MainPage::CONFIG;
        currentManualSubPage = ManualSubPage::NONE;
        currentConfigSubPage = ConfigSubPage::NONE;
    }

    ImGui::SameLine();
    if (ImGui::Button("Vision", ImVec2(120, 40))) {
        currentMainPage = MainPage::VISION;
        currentManualSubPage = ManualSubPage::NONE;
        currentConfigSubPage = ConfigSubPage::NONE;
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}

void MainUIManager::RenderDateTime() {
    // Get date/time strings
    time_t rawtime;
    struct tm timeinfo;
    char timeBuffer[80];
    char dateBuffer[80];

    time(&rawtime);
#ifdef _WIN32
    localtime_s(&timeinfo, &rawtime);
#else
    timeinfo = *localtime(&rawtime);
#endif

    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);
    strftime(dateBuffer, sizeof(dateBuffer), "%d %b %Y", &timeinfo);

    std::string datetime = std::string(dateBuffer) + "\n" + std::string(timeBuffer);

    // Calculate positions
    ImVec2 textSize = ImGui::CalcTextSize(datetime.c_str());
    float buttonWidth = 50.0f;
    float buttonHeight = 30.0f;
    float spacing = 10.0f;

    // Position for JOG button (left of date/time)
    ImGui::SameLine(ImGui::GetWindowWidth() - textSize.x - buttonWidth - spacing - 20);
    ImGui::SetCursorPosY(10);

    // Check if jog window is available
    bool jogAvailable = (m_uiJogWindow != nullptr);
    bool isRealMode = jogAvailable && m_piControllerManager && m_acsControllerManager;

    // Button styling - show different colors for real vs mock mode
    if (isRealMode && m_uiJogWindow->IsVisible()) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f)); // Active green (real mode)
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    }
    else if (jogAvailable && m_uiJogWindow->IsVisible()) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.5f, 0.2f, 1.0f)); // Active orange (mock mode)
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.6f, 0.3f, 1.0f));
    }
    else if (jogAvailable) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1.0f)); // Inactive gray
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f)); // Disabled dark gray
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    }

    if (ImGui::Button("JOG", ImVec2(buttonWidth, buttonHeight))) {
        if (m_uiJogWindow) {
            m_uiJogWindow->ToggleWindow();
        }
    }

    ImGui::PopStyleColor(2);

    // Update tooltip to show real vs mock mode
    if (ImGui::IsItemHovered()) {
        if (isRealMode) {
            ImGui::SetTooltip("Global Jog Control\n(Real mode - Controllers available)");
        }
        else if (jogAvailable) {
            ImGui::SetTooltip("Global Jog Control\n(Mock mode - Controllers not available)");
        }
        else {
            ImGui::SetTooltip("Global Jog Control\n(Not available)");
        }
    }

    // Date/time display (existing position)
    ImGui::SameLine(ImGui::GetWindowWidth() - textSize.x - 20);
    ImGui::SetCursorPosY(10);
    ImGui::Text("%s", datetime.c_str());
}

void MainUIManager::RenderBreadcrumbs() {
    std::string breadcrumb = "Home";

    switch (currentMainPage) {
    case MainPage::MAIN:
        break;
    case MainPage::MANUAL:
        breadcrumb += " > Manual";
        switch (currentManualSubPage) {
        case ManualSubPage::PI:
            breadcrumb += " > PI Controllers";
            break;
        case ManualSubPage::GANTRY:
            breadcrumb += " > Gantry (ACS)";
            break;
        case ManualSubPage::IO:
            breadcrumb += " > IO Control";
            break;
        case ManualSubPage::PNEUMATIC:
            breadcrumb += " > Pneumatic Slides";
            break;
        case ManualSubPage::CAMERA:
            breadcrumb += " > Camera";
            break;
        default:
            break;
        }
        break;
    case MainPage::DATA_INSTRUMENT:
        breadcrumb += " > Data & Instrument";
        // Note: DataInstrumentUI handles its own internal navigation
        break;
    case MainPage::RUN_PROGRAM:
        breadcrumb += " > Run Program";
        break;
    case MainPage::CONFIG:
        breadcrumb += " > Configuration";
        switch (currentConfigSubPage) {
        case ConfigSubPage::CONFIG_EDITOR:
            breadcrumb += " > Config Editor";
            break;
        case ConfigSubPage::NODE_VISUALIZER:
            breadcrumb += " > Node Visualizer";
            break;
        default:
            break;
        }
        break;
    case MainPage::VISION:
        breadcrumb += " > Vision";
        break;
    }

    ImGui::Text("%s", breadcrumb.c_str());
}

void MainUIManager::RenderMainContent() {
    ImGui::SetCursorPosY(100);

    if (currentMainPage == MainPage::MAIN) {
        RenderMainPage();
    }
    else if (currentMainPage == MainPage::MANUAL) {
        if (currentManualSubPage == ManualSubPage::NONE) {
            RenderManualPage();
        }
        else {
            RenderManualSubPage();
        }
    }
    else if (currentMainPage == MainPage::DATA_INSTRUMENT) {
        RenderDataInstrumentPage();  // SIMPLIFIED - no sub-page handling
    }
    else if (currentMainPage == MainPage::RUN_PROGRAM) {
        RenderRunProgramPage();
    }
    else if (currentMainPage == MainPage::CONFIG) {
        if (currentConfigSubPage == ConfigSubPage::NONE) {
            RenderConfigPage();
        }
        else {
            RenderConfigSubPage();
        }
    }
    else if (currentMainPage == MainPage::VISION) {
        RenderVisionPage();
    }

    // Update any initialized TCP Data Manager
    if (currentMainPage == MainPage::DATA_INSTRUMENT) {
        DataClientManager* tcpManager = GetTcpDataManager();
        if (tcpManager) {
            tcpManager->UpdateClients();
        }
    }
}

void MainUIManager::RenderMainPage() {
    ImGui::SetWindowFontScale(2.0f);
    ImGui::Text("Welcome to uaa3App");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Spacing();
    ImGui::Text("Select a category from the top menu to begin:");
    ImGui::BulletText("Manual - Direct control of hardware components");
    ImGui::BulletText("Data & Instrument - Data monitoring and instruments");
    ImGui::BulletText("Run Program - Execute automated sequences");
    ImGui::BulletText("Config - System configuration and settings");
    ImGui::BulletText("Vision - Image processing and computer vision");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Simple status display
    ImGui::SetWindowFontScale(1.3f);
    ImGui::Text("System Status");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Motion Config Manager: Ready");
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ UI Config Editor: Ready");
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ UI Config Visualizer: Ready");

    // Show jog status
    if (m_uiJogWindow) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Global Jog Panel: Ready");
    }
    else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "○ Global Jog Panel: Waiting for motion controllers");
    }

    ImGui::Spacing();
    ImGui::Text("All systems operational. You can use the Config Editor to manage motion settings");
    ImGui::Text("and the Node Visualizer to view and edit motion graphs interactively.");
}

void MainUIManager::RenderManualPage() {
    ImGui::SetWindowFontScale(1.5f);
    ImGui::Text("Manual Control");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Spacing();
    ImGui::Text("Select a manual control option:");
    ImGui::Spacing();

    if (ImGui::Button("1. PI", ImVec2(200, 50))) {
        currentManualSubPage = ManualSubPage::PI;
    }

    if (ImGui::Button("2. Gantry", ImVec2(200, 50))) {
        currentManualSubPage = ManualSubPage::GANTRY;
    }

    if (ImGui::Button("3. IO", ImVec2(200, 50))) {
        currentManualSubPage = ManualSubPage::IO;
    }

    if (ImGui::Button("4. Pneumatic", ImVec2(200, 50))) {
        currentManualSubPage = ManualSubPage::PNEUMATIC;
    }

    if (ImGui::Button("5. Camera", ImVec2(200, 50))) {
        currentManualSubPage = ManualSubPage::CAMERA;
    }
}

void MainUIManager::RenderManualSubPage() {
    switch (currentManualSubPage) {
    case ManualSubPage::PI:
        RenderPIPage();
        break;
    case ManualSubPage::GANTRY:
        RenderGantryPage();
        break;
    case ManualSubPage::IO:
        RenderIOPage();
        break;
    case ManualSubPage::PNEUMATIC:
        RenderPneumaticPage();
        break;
    case ManualSubPage::CAMERA:
        RenderCameraPage();
        break;
    default:
        break;
    }
}

void MainUIManager::RenderPneumaticPage() {
    if (m_pneumaticPanelUI) {
        m_pneumaticPanelUI->RenderUI();
    }
    else {
        // Fallback message when pneumatic system is not available
        ImGui::SetWindowFontScale(1.5f);
        ImGui::Text("Pneumatic Control");
        ImGui::SetWindowFontScale(1.0f);

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Pneumatic System not available");
        ImGui::Text("Pneumatic Manager has not been initialized.");
        ImGui::Spacing();
        ImGui::Text("This typically means:");
        ImGui::BulletText("Pneumatic configuration is still loading");
        ImGui::BulletText("IO system is not connected");
        ImGui::BulletText("No pneumatic slides are configured");
        ImGui::BulletText("Check IOConfig.json for pneumatic slide definitions");
    }
}

void MainUIManager::RenderPIPage() {
    if (m_piPanelUI) {
        m_piPanelUI->RenderUI();
    }
    else {
        // Fallback message
        ImGui::Text("PI Controllers not available");
        ImGui::Text("Motion controller managers have not been initialized.");
    }
}

void MainUIManager::RenderGantryPage() {
    if (m_acsPanelUI) {
        // Render the new ACS panel UI
        m_acsPanelUI->RenderUI();
    }
    else {
        // Fallback when ACS controllers are not available
        ImGui::SetWindowFontScale(1.5f);
        ImGui::Text("ACS Gantry Controller");
        ImGui::SetWindowFontScale(1.0f);

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "ACS Controllers not available");
        ImGui::Text("Motion controller managers have not been initialized.");
        ImGui::Spacing();
        ImGui::Text("This typically means:");
        ImGui::BulletText("Motion configuration is still loading");
        ImGui::BulletText("ACS controllers are not enabled in configuration");
        ImGui::BulletText("Hardware connection issues");
        ImGui::BulletText("Check that gantry-main is configured with port 701");
    }
}

void MainUIManager::RenderIOPage() {
    if (m_ioPanelUI) {
        m_ioPanelUI->RenderUI();
    }
    else {
        // Fallback message when IO system is not available
        ImGui::SetWindowFontScale(1.5f);
        ImGui::Text("IO Control");
        ImGui::SetWindowFontScale(1.0f);

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "IO System not available");
        ImGui::Text("EziIO Manager has not been initialized.");
        ImGui::Spacing();
        ImGui::Text("This typically means:");
        ImGui::BulletText("IO configuration is still loading");
        ImGui::BulletText("EziIO devices are not enabled in configuration");
        ImGui::BulletText("Hardware connection issues");
        ImGui::BulletText("Check network connectivity to IO modules");
    }
}

void MainUIManager::RenderCameraPage() {
    if (m_cameraPanelUI) {
        // Render the new Camera panel UI
        m_cameraPanelUI->RenderUI();
    }
    else {
        // Fallback when camera manager is not available
        ImGui::SetWindowFontScale(1.5f);
        ImGui::Text("Camera Control");
        ImGui::SetWindowFontScale(1.0f);

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Camera Manager not available");
        ImGui::Text("Camera system has not been initialized.");
        ImGui::Spacing();
        ImGui::Text("This typically means:");
        ImGui::BulletText("Camera manager is still loading");
        ImGui::BulletText("No cameras are configured in the system");
        ImGui::BulletText("Camera hardware connection issues");
        ImGui::BulletText("Pylon SDK initialization failed");
    }
}

// NEW: Simplified Data & Instrument page
void MainUIManager::RenderDataInstrumentPage() {
    if (m_dataInstrumentUI) {
        m_dataInstrumentUI->Render();
    }
    else {
        ImGui::SetWindowFontScale(1.5f);
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Data & Instrument");
        ImGui::SetWindowFontScale(1.0f);

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Failed to create Data Instrument UI");
        ImGui::Text("Check console for error messages");
    }
}

void MainUIManager::RenderRunProgramPage() {
    ImGui::SetWindowFontScale(1.5f);
    ImGui::Text("Run Program");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Spacing();
    ImGui::Text("Program execution interface will be implemented here");
}

void MainUIManager::RenderConfigPage() {
    ImGui::SetWindowFontScale(1.5f);
    ImGui::Text("Configuration");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Spacing();
    ImGui::Text("Select a configuration tool:");
    ImGui::Spacing();

    if (ImGui::Button("1. Config Editor", ImVec2(200, 50))) {
        currentConfigSubPage = ConfigSubPage::CONFIG_EDITOR;
    }

    if (ImGui::Button("2. Node Visualizer", ImVec2(200, 50))) {
        currentConfigSubPage = ConfigSubPage::NODE_VISUALIZER;
    }
}

void MainUIManager::RenderVisionPage() {
    ImGui::SetWindowFontScale(1.5f);
    ImGui::Text("Vision & Image Processing");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Spacing();
    ImGui::Text("Computer vision and image processing tools will be implemented here");

    // Placeholder Vision controls
    ImGui::Separator();
    ImGui::Text("Image Processing Controls:");

    static float threshold = 128.0f;
    static float blur = 1.0f;
    static bool enableEdgeDetection = false;

    ImGui::SliderFloat("Threshold", &threshold, 0.0f, 255.0f);
    ImGui::SliderFloat("Blur Radius", &blur, 0.0f, 10.0f);
    ImGui::Checkbox("Enable Edge Detection", &enableEdgeDetection);

    ImGui::Spacing();
    if (ImGui::Button("Process Image")) {
        std::cout << "Processing image with threshold: " << threshold
            << ", blur: " << blur
            << ", edge detection: " << (enableEdgeDetection ? "ON" : "OFF") << std::endl;
    }

    ImGui::SameLine();
    if (ImGui::Button("Capture & Analyze")) {
        std::cout << "Capturing and analyzing image..." << std::endl;
    }
}

void MainUIManager::RenderConfigSubPage() {
    switch (currentConfigSubPage) {
    case ConfigSubPage::CONFIG_EDITOR:
        RenderConfigEditorPage();
        break;
    case ConfigSubPage::NODE_VISUALIZER:
        RenderNodeVisualizerPage();
        break;
    default:
        break;
    }
}

void MainUIManager::RenderConfigEditorPage() {
    // Super simple - just render the UIConfigEditor!
    if (uiConfigEditor) {
        uiConfigEditor->RenderUI();
    }
    else {
        ImGui::Text("Config Editor not available");
    }
}

void MainUIManager::RenderNodeVisualizerPage() {
    // Replace the placeholder with actual UIConfigVisualizer!
    if (uiConfigVisualizer) {
        uiConfigVisualizer->RenderUI();
    }
    else {
        ImGui::Text("Node Visualizer not available");
    }
}

void MainUIManager::RenderGlobalJogWindow() {
    // Let UIJogWindow handle its own rendering
    if (m_uiJogWindow) {
        m_uiJogWindow->RenderUI();
    }
}

// ============================================================================
// SETTER METHODS
// ============================================================================

void MainUIManager::SetPIControllerManager(PIControllerManager* piManager) {
    m_piControllerManager = piManager;

    // Create PI Panel UI when PI manager is available
    if (m_piControllerManager) {
        m_piPanelUI = std::make_unique<PIPanelUI>(*m_piControllerManager);
        std::cout << "MainUIManager: PI Panel UI created successfully" << std::endl;
    }

    // Update UIJogWindow with PI controller manager
    if (m_uiJogWindow) {
        m_uiJogWindow->SetPIControllerManager(piManager);
        std::cout << "MainUIManager: UIJogWindow updated with PI Controller Manager" << std::endl;
    }
}

void MainUIManager::SetACSControllerManager(ACSControllerManager* acsManager) {
    m_acsControllerManager = acsManager;

    // Create ACS-related UI components here when needed
    if (m_acsControllerManager) {
        std::cout << "MainUIManager: ACS Controller Manager set successfully" << std::endl;
        m_acsPanelUI = std::make_unique<ACSPanelUI>(*m_acsControllerManager);
        std::cout << "MainUIManager: ACS Panel UI created successfully" << std::endl;
    }

    // Update UIJogWindow with ACS controller manager
    if (m_uiJogWindow) {
        m_uiJogWindow->SetACSControllerManager(acsManager);
        std::cout << "MainUIManager: UIJogWindow updated with ACS Controller Manager" << std::endl;
    }
}

void MainUIManager::SetIOManager(EziIOManager* ioManager, IOConfigManager* ioConfigManager) {
    m_ioManager = ioManager;
    m_ioConfigManager = ioConfigManager;

    // Create IO Panel UI when IO manager is available
    if (m_ioManager) {
        m_ioPanelUI = std::make_unique<IOPanelUI>(*m_ioManager);

        // Set config manager if available (for pin naming)
        if (m_ioConfigManager) {
            m_ioPanelUI->SetConfigManager(m_ioConfigManager);
        }

        std::cout << "MainUIManager: IO Panel UI created successfully" << std::endl;
    }
}

void MainUIManager::SetPneumaticManager(PneumaticManager* pneumaticManager) {
    m_pneumaticManager = pneumaticManager;

    // Create Pneumatic Panel UI when pneumatic manager is available
    if (m_pneumaticManager) {
        m_pneumaticPanelUI = std::make_unique<UIPneumaticPanel>(*m_pneumaticManager);
        std::cout << "MainUIManager: Pneumatic Panel UI created successfully" << std::endl;
    }
}

void MainUIManager::SetCameraManager(CameraManager* cameraManager) {
    m_cameraManager = cameraManager;

    // Create Camera Panel UI when camera manager is available
    if (m_cameraManager) {
        m_cameraPanelUI = std::make_unique<UICameraPanel>(*m_cameraManager);
        std::cout << "MainUIManager: Camera Panel UI created successfully" << std::endl;
    }

    // Update UIConfigVisualizer with camera manager
    if (uiConfigVisualizer && m_cameraManager) {
        // Need to recreate UIConfigVisualizer with camera manager
        uiConfigVisualizer = std::make_unique<UIConfigVisualizer>(motionConfigManager, m_cameraManager);
        std::cout << "MainUIManager: UIConfigVisualizer updated with Camera Manager" << std::endl;
    }
}


void MainUIManager::SetMachineOperations(MachineOperations* machineOps) {
    m_machineOperations = machineOps;

    if (m_machineOperations) {
        std::cout << "MainUIManager: MachineOperations set successfully" << std::endl;

        // EXISTING: Update UIConfigVisualizer
        if (uiConfigVisualizer) {
            uiConfigVisualizer->SetMachineOperations(m_machineOperations);
        }

        // NEW: Register auto-update callback with DataInstrumentModuleManager
        DataInstrumentModuleManager* moduleManager = GetDataInstrumentModuleManager();
        if (moduleManager) {
            moduleManager->SetMachineOperationsCallback(m_machineOperations);
            std::cout << "MainUIManager: Registered MachineOperations for auto-updates with DataInstrumentModuleManager" << std::endl;
        }

        // ... rest of existing code for other UI components ...
    }
    else {
        std::cout << "MainUIManager: MachineOperations cleared" << std::endl;

        // NEW: Clear callback when MachineOperations is cleared
        DataInstrumentModuleManager* moduleManager = GetDataInstrumentModuleManager();
        if (moduleManager) {
            moduleManager->SetMachineOperationsCallback(nullptr);
            std::cout << "MainUIManager: Cleared MachineOperations callback from DataInstrumentModuleManager" << std::endl;
        }
    }
}
// ============================================================================
// GETTER METHODS FOR DATA INSTRUMENT MODULES
// ============================================================================

DataInstrumentModuleManager* MainUIManager::GetDataInstrumentModuleManager() const {
    if (m_dataInstrumentUI) {
        return m_dataInstrumentUI->GetModuleManager();
    }
    return nullptr;
}

DataClientManager* MainUIManager::GetTcpDataManager() const {
    if (m_dataInstrumentUI && m_dataInstrumentUI->GetModuleManager()) {
        return m_dataInstrumentUI->GetModuleManager()->GetTcpDataManager();
    }
    return nullptr;
}

CLD101xManager* MainUIManager::GetCLD101xManager() const {
    if (m_dataInstrumentUI && m_dataInstrumentUI->GetModuleManager()) {
        return m_dataInstrumentUI->GetModuleManager()->GetCLD101xManager();
    }
    return nullptr;
}

Keithley2400Manager* MainUIManager::GetKeithleyManager() const {
    if (m_dataInstrumentUI && m_dataInstrumentUI->GetModuleManager()) {
        return m_dataInstrumentUI->GetModuleManager()->GetKeithleyManager();
    }
    return nullptr;
}

MachineOperations* MainUIManager::GetMachineOperations() {
    return m_machineOperations;
}