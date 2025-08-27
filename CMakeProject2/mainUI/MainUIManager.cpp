#include "MainUIManager.h"
#include "DatumUI.h"
#include "UIConfigEditor.h"
#include "UIConfigVisualizer.h"
#include "UIJogWindow.h"
#include "include/motions/MotionConfigManager.h"
#include "include/motions/pi_controller_manager.h"
#include "include/motions/acs_controller_manager.h"
#include "include/cld101x/cld101x_manager.h"
#include "CLD101xEquipmentUI.h"
// Add this include at the top:
#include "PIPanelUI.h"
#include "ACSPanelUI.h"
// Add this include at the top:
#include "IOPanelUI.h"
#include "UIPneumaticPanel.h"
// Add this include at the top with other includes:
#include "UISMUPanel.h"
#include "RunPageUI.h"            // NEW: Add this include
#include "Programming/UserPromptUI.h"  // NEW: Add this include

#include "include/data/global_data_store.h" // Add this with your other includes
#include "include/machine_operations.h"  // Add this include at the top
#include "AppContext.h"
#include <SDL.h>
#include "implot/implot.h"
#include <deque>
#include <map>
#include <chrono>

#include "imgui.h"
#include <iostream>
#include <string>
#include <ctime>





// UPDATE the constructor to initialize DatumUI:
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

	// STEP 1: Initialize UserPromptUI FIRST
	m_promptUI = std::make_unique<UserPromptUI>();

	// STEP 2: Initialize MachineBlockUI
	m_machineBlockUI = std::make_unique<MachineBlockUI>();

	// STEP 3: Initialize MacroManager
	m_macroManager = std::make_unique<MacroManager>();

	// STEP 4: Initialize MacroPanelUI and connect prompt UI
	m_macroPanelUI = std::make_unique<MacroPanelUI>();
	m_macroPanelUI->SetPromptUI(m_promptUI.get()); // NOW THIS WORKS!

	// Connect MacroManager to MachineBlockUI
	if (m_macroManager && m_machineBlockUI) {
		m_macroManager->SetMachineBlockUI(m_machineBlockUI.get());
		std::cout << "MainUIManager: MacroManager connected to MachineBlockUI" << std::endl;
	}

	// Connect MacroPanelUI to MacroManager and MachineBlockUI
	if (m_macroPanelUI && m_macroManager && m_machineBlockUI) {
		m_macroPanelUI->SetMacroManager(m_macroManager.get());
		m_macroPanelUI->SetMachineBlockUI(m_machineBlockUI.get());
		std::cout << "MainUIManager: MacroPanelUI connected to MacroManager and MachineBlockUI" << std::endl;
	}

	// Initialize TCP Data Manager UI
	m_tcpDataManagerUI = std::make_unique<TCPDataManagerUI>();
	if (!m_tcpDataManagerUI->Initialize(m_dataClientManager)) {
		// Log error but continue - the UI will show the error state
		std::cout << "Warning: TCP Data Manager failed to initialize" << std::endl;
	}

	// Initialize Global Data Store Viewer UI
	m_globalDataStoreViewerUI = std::make_unique<GlobalDataStoreViewerUI>();

	// Initialize RunPageUI as nullptr - will be created when MachineOperations is set
	m_runPageUI = nullptr;
	// NEW: Initialize IO Control Panel as nullptr - will be created when IO manager is set
	m_ioControlPanel = nullptr;  // <-- ADD THIS LINE


	// Initialize Vision Panel UI
	m_visionPanelUI = std::make_unique<UIVisionPanel>();
	std::cout << "MainUIManager: Vision Panel UI created successfully" << std::endl;

	// ADD THIS: Initialize DatumUI
	m_datumUI = std::make_unique<DatumUI>();
	std::cout << "MainUIManager: DatumUI created successfully" << std::endl;
	// ADD THIS: Initialize ModuleAlignmentUI
	m_moduleAlignmentUI = std::make_unique<ModuleAlignmentUI>();
	std::cout << "MainUIManager: ModuleAlignmentUI created successfully" << std::endl;

	// ADD these lines right after:
	auto productReferenceManager = std::make_shared<ProductReferenceManager>();
	m_moduleAlignmentUI->SetProductReferenceManager(productReferenceManager);

}


MainUIManager::~MainUIManager() = default;






// 1. UPDATE the existing AppContext constructor (around line 165)
// REPLACE the existing constructor with this:
MainUIManager::MainUIManager(AppContext& context)
	: m_context(&context),  // Store as pointer
	motionConfigManager(*context.GetMotionConfig()) {  // Initialize reference

	auto* logger = Logger::GetInstance();
	logger->LogInfo("MainUIManager: Initializing with pure AppContext approach");

	// Verify we have essential services
	if (!context.GetMotionConfig()) {
		logger->LogError("MainUIManager: MotionConfigManager not available in AppContext!");
		throw std::runtime_error("MotionConfigManager required but not available in AppContext");
	}

	logger->LogInfo("MainUIManager: AppContext validation passed");

	// Initialize all UI components
	InitializeUIComponents();

	// Connect UI components to services from AppContext
	ConnectUIToServices();

	logger->LogInfo("MainUIManager: Initialization complete with AppContext");
}



// TO THIS:
void MainUIManager::InitializeUIComponents() {
	auto* logger = Logger::GetInstance();
	logger->LogInfo("MainUIManager: Creating base UI components...");

	// Create base UI components that don't depend on services
	m_visionPanelUI = std::make_unique<UIVisionPanel>();
	m_datumUI = std::make_unique<DatumUI>();
	m_moduleAlignmentUI = std::make_unique<ModuleAlignmentUI>();
	m_tcpDataManagerUI = std::make_unique<TCPDataManagerUI>();
	m_globalDataStoreViewerUI = std::make_unique<GlobalDataStoreViewerUI>();

	// Programming UI components - CREATE ONLY ONE UserPromptUI
	m_promptUI = std::make_unique<UserPromptUI>();
	// m_userPromptUI will be created separately if needed for backward compatibility

	m_machineBlockUI = std::make_unique<MachineBlockUI>();
	m_macroManager = std::make_unique<MacroManager>();
	m_macroPanelUI = std::make_unique<MacroPanelUI>();




	// Connect programming components to each other
	if (m_macroManager && m_machineBlockUI) {
		m_macroManager->SetMachineBlockUI(m_machineBlockUI.get());
		logger->LogInfo("MainUIManager: MacroManager connected to MachineBlockUI");
	}

	if (m_macroPanelUI && m_macroManager && m_machineBlockUI) {
		m_macroPanelUI->SetMacroManager(m_macroManager.get());
		m_macroPanelUI->SetMachineBlockUI(m_machineBlockUI.get());
		m_macroPanelUI->SetPromptUI(m_promptUI.get());
		logger->LogInfo("MainUIManager: MacroPanelUI connected to MacroManager and MachineBlockUI");
	}

	// Module alignment setup
	if (m_moduleAlignmentUI) {
		auto productReferenceManager = std::make_shared<ProductReferenceManager>();
		m_moduleAlignmentUI->SetProductReferenceManager(productReferenceManager);
	}

	logger->LogInfo("MainUIManager: Base UI components created");
}
// MainUIManager.cpp - Complete methods to ADD or UPDATE



// 2. UPDATE the ConnectUIToServices() method
// REPLACE the existing method with this:
void MainUIManager::ConnectUIToServices() {
	auto* logger = Logger::GetInstance();
	logger->LogInfo("MainUIManager: Connecting UI to services...");

	// Get MotionConfigManager (required) - works for both constructors
	MotionConfigManager* motionConfig = &motionConfigManager;

	// Create UIConfigEditor and UIConfigVisualizer
	uiConfigEditor = std::make_unique<UIConfigEditor>(*motionConfig);

	// For UIConfigVisualizer, use smart getter for camera
	auto* camera = GetCameraManagerSmart();
	uiConfigVisualizer = std::make_unique<UIConfigVisualizer>(*motionConfig, camera);


	// Create UIJogWindow
	m_uiJogWindow = std::make_unique<UIJogWindow>(*motionConfig);

	// Connect PI Controller UI using smart getter
	if (auto* pi = GetPIController()) {
		if (!m_piPanelUI) {  // Only create if not already created
			m_piPanelUI = std::make_unique<PIPanelUI>(*pi);
		}

		// Connect to jog window
		if (m_uiJogWindow) {
			m_uiJogWindow->SetPIControllerManager(pi);
		}

		logger->LogInfo("MainUIManager: PI Controller UI connected");
	}

	// Connect ACS Controller UI using smart getter
	if (auto* acs = GetACSController()) {
		if (!m_acsPanelUI) {
			m_acsPanelUI = std::make_unique<ACSPanelUI>(*acs);
		}

		// Connect to jog window
		if (m_uiJogWindow) {
			m_uiJogWindow->SetACSControllerManager(acs);
		}

		logger->LogInfo("MainUIManager: ACS Controller UI connected");
	}

	// Connect IO UI using smart getter
	if (auto* io = GetIOManager()) {
		if (!m_ioPanelUI) {
			m_ioPanelUI = std::make_unique<IOPanelUI>(*io);

			// Set IO config if available
			if (auto* ioConfig = GetIOConfig()) {
				m_ioPanelUI->SetConfigManager(ioConfig);
			}
		}

		// Create IO Control Panel
		if (!m_ioControlPanel) {
			m_ioControlPanel = std::make_unique<IOControlPanel>(*io);
			m_ioControlPanel->LoadConfiguration("io_panel_config.json");
		}

		logger->LogInfo("MainUIManager: IO Panel UI connected");
	}

	// Connect Pneumatic UI using smart getter
	if (auto* pneumatic = GetPneumaticManager()) {
		if (!m_pneumaticPanelUI) {
			m_pneumaticPanelUI = std::make_unique<UIPneumaticPanel>(*pneumatic);
		}
		logger->LogInfo("MainUIManager: Pneumatic Panel UI connected");
	}

	// Connect Camera UI using smart getter
	if (auto* cameraManager = GetCameraManagerSmart()) {
		if (!m_cameraPanelUI) {
			m_cameraPanelUI = std::make_unique<UICameraPanel>(*cameraManager);
		}

		// Setup vision panel connections
		if (m_visionPanelUI) {
			m_visionPanelUI->SetCameraManager(cameraManager);
			m_visionPanelUI->SetMotionConfigManager(motionConfig);

			// Connect machine operations using smart getter
			if (m_context) {
				if (auto* machineOps = m_context->GetMachineOperations()) {
					m_visionPanelUI->SetMachineOperations(machineOps);
				}
			}
			else if (m_machineOperations) {
				m_visionPanelUI->SetMachineOperations(m_machineOperations);
			}
		}

		logger->LogInfo("MainUIManager: Camera Panel UI connected");
	}

	// Connect Data Services UI using smart getter
	if (auto* dataClient = GetDataClient()) {
		if (m_tcpDataManagerUI) {
			m_tcpDataManagerUI->Initialize(dataClient);
		}
		if (m_globalDataStoreViewerUI) {
			m_globalDataStoreViewerUI->SetDataClientManager(dataClient);
		}
		logger->LogInfo("MainUIManager: Data services UI connected");
	}

	// Connect Equipment UI using smart getter
	if (auto* keithley = GetKeithley()) {
		if (!m_smuPanelUI) {
			m_smuPanelUI = std::make_unique<UISMUPanel>(*keithley);
		}
		logger->LogInfo("MainUIManager: SMU Panel UI connected");
	}

	// ADD THIS - Create SPD UI when SPD manager is available
	if (auto* spdManager = GetSPDManager()) {
		if (!m_spdPowerSupplyUI) {
			m_spdPowerSupplyUI = std::make_unique<SPDPowerSupplyUI>(spdManager);
		}
		logger->LogInfo("MainUIManager: SPD Power Supply UI connected");
	}

	// Connect RunPageUI using smart getter for MachineOperations
	auto* machineOps = m_context ? m_context->GetMachineOperations() : m_machineOperations;
	if (machineOps && !m_runPageUI) {
		// RunPageUI constructor needs MachineOperations reference
		m_runPageUI = std::make_unique<RunPageUI>(*machineOps);  // Pass as reference
		m_runPageUI->SetUserPromptUI(m_promptUI.get());  // Connect UserPromptUI
		m_runPageUI->SetCameraManager(GetCameraManagerSmart());  // Connect camera manager

		// No SetPromptUI method - RunPageUI probably handles prompts differently
		// Check if RunPageUI has other methods for UserPromptUI or if it gets it from MachineOperations

		logger->LogInfo("MainUIManager: Run Page UI connected");
	}


	if (machineOps)	{
		uiConfigVisualizer->SetMachineOperations(machineOps);
	}

	// FIXED code:
	if (auto* cld101manager = GetCLD101x()) {
		if (!m_cld101xEquipmentUI) {
			m_cld101xEquipmentUI = std::make_unique<CLD101xEquipmentUI>();  // Default constructor
			m_cld101xEquipmentUI->SetCLD101xManager(cld101manager);  // Set manager via setter
		}
		logger->LogInfo("MainUIManager: CLD101x Equipment UI connected");
	}
	else {
		// Still create the UI even without manager - allows manual setup later
		if (!m_cld101xEquipmentUI) {
			m_cld101xEquipmentUI = std::make_unique<CLD101xEquipmentUI>();
			logger->LogWarning("MainUIManager: CLD101x Equipment UI created without manager");
		}
	}


	logger->LogInfo("MainUIManager: Service connections complete");
}



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

	// NEW: Always render IO control panel (it handles its own visibility internally)
	if (m_ioControlPanel && m_ioControlPanel->IsVisible()) {  // <-- ADDED THESE 3 LINES
		m_ioControlPanel->RenderUI();
	}

	// Render UserPromptUI (handles prompts from UAA3 sequences)
	if (m_promptUI) {
		m_promptUI->Render();
	}
}

// Update RenderBackButton() to handle Data Instrument sub-pages


// Update RenderBackButton() to handle Programming sub-pages:
void MainUIManager::RenderBackButton() {
	// Add 10 pixels of vertical spacing at the top
	ImGui::Dummy(ImVec2(0.0f, 15.0f));

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(15, 8));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));

	if (ImGui::Button("<< BACK")) {
		if (currentManualSubPage != ManualSubPage::NONE) {
			currentManualSubPage = ManualSubPage::NONE;
		}
		else if (currentDataInstrumentSubPage != DataInstrumentSubPage::NONE) {
			currentDataInstrumentSubPage = DataInstrumentSubPage::NONE;
		}
		else if (currentConfigSubPage != ConfigSubPage::NONE) {
			currentConfigSubPage = ConfigSubPage::NONE;
		}
		else if (currentProgrammingSubPage != ProgrammingSubPage::NONE) {
			currentProgrammingSubPage = ProgrammingSubPage::NONE;  // Handle programming sub-pages
		}
		else if (currentVisionSubPage != VisionSubPage::NONE) {  // ADD THIS LINE
			currentVisionSubPage = VisionSubPage::NONE;           // ADD THIS LINE
		}                                                         // ADD THIS LINE
		else {
			currentMainPage = MainPage::MAIN;
		}
	}

	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar();
}

// Update RenderTopMenuBar() to add 10px spacing at the top:
void MainUIManager::RenderTopMenuBar() {
	// Add 10 pixels of vertical spacing at the top
	ImGui::Dummy(ImVec2(0.0f, 12.0f));

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 10));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.3f, 0.8f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.4f, 0.9f, 1.0f));
	if (ImGui::Button("Manual", ImVec2(120, 40))) {
		currentMainPage = MainPage::MANUAL;
		currentManualSubPage = ManualSubPage::NONE;
		currentDataInstrumentSubPage = DataInstrumentSubPage::NONE;
		currentConfigSubPage = ConfigSubPage::NONE;
		currentProgrammingSubPage = ProgrammingSubPage::NONE;  // Reset programming sub-page
	}
	ImGui::SameLine();
	if (ImGui::Button("Data & Instrument", ImVec2(150, 40))) {
		currentMainPage = MainPage::DATA_INSTRUMENT;
		currentManualSubPage = ManualSubPage::NONE;
		currentDataInstrumentSubPage = DataInstrumentSubPage::NONE;
		currentConfigSubPage = ConfigSubPage::NONE;
		currentProgrammingSubPage = ProgrammingSubPage::NONE;  // Reset programming sub-page
	}
	ImGui::SameLine();
	if (ImGui::Button("Run Program", ImVec2(120, 40))) {
		currentMainPage = MainPage::RUN_PROGRAM;
		currentManualSubPage = ManualSubPage::NONE;
		currentDataInstrumentSubPage = DataInstrumentSubPage::NONE;
		currentConfigSubPage = ConfigSubPage::NONE;
		currentProgrammingSubPage = ProgrammingSubPage::NONE;  // Reset programming sub-page
	}
	ImGui::SameLine();
	if (ImGui::Button("Config", ImVec2(120, 40))) {
		currentMainPage = MainPage::CONFIG;
		currentManualSubPage = ManualSubPage::NONE;
		currentDataInstrumentSubPage = DataInstrumentSubPage::NONE;
		currentConfigSubPage = ConfigSubPage::NONE;
		currentProgrammingSubPage = ProgrammingSubPage::NONE;  // Reset programming sub-page
	}
	ImGui::SameLine();
	if (ImGui::Button("Vision", ImVec2(120, 40))) {
		currentMainPage = MainPage::VISION;
		currentManualSubPage = ManualSubPage::NONE;
		currentDataInstrumentSubPage = DataInstrumentSubPage::NONE;
		currentConfigSubPage = ConfigSubPage::NONE;
		currentProgrammingSubPage = ProgrammingSubPage::NONE;
		currentVisionSubPage = VisionSubPage::NONE;  // ADD THIS LINE
	}
	ImGui::SameLine();
	if (ImGui::Button("Programming", ImVec2(130, 40))) {
		currentMainPage = MainPage::PROGRAMMING;
		currentManualSubPage = ManualSubPage::NONE;
		currentDataInstrumentSubPage = DataInstrumentSubPage::NONE;
		currentConfigSubPage = ConfigSubPage::NONE;
		currentProgrammingSubPage = ProgrammingSubPage::NONE;  // Reset programming sub-page
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

	// NEW: Position for Q-IO button (left of JOG button)
	ImGui::SameLine(ImGui::GetWindowWidth() - textSize.x - (buttonWidth * 2) - (spacing * 2) - 20);
	ImGui::SetCursorPosY(30);

	// Check if IO control is available
	bool ioAvailable = (m_ioControlPanel != nullptr);
	bool ioRealMode = ioAvailable && m_ioManager;

	// Q-IO Button styling - similar pattern to JOG button
	if (ioRealMode && m_ioControlPanel->IsVisible()) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.7f, 1.0f)); // Active blue (real mode)
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.8f, 1.0f));
	}
	else if (ioAvailable && m_ioControlPanel->IsVisible()) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.5f, 0.2f, 1.0f)); // Active orange (limited mode)
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.6f, 0.3f, 1.0f));
	}
	else if (ioAvailable) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1.0f)); // Inactive gray
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
	}
	else {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f)); // Disabled dark gray
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
	}

	if (ImGui::Button("Q-IO", ImVec2(buttonWidth, buttonHeight))) {
		if (m_ioControlPanel) {
			m_ioControlPanel->ToggleWindow();
		}
	}
	ImGui::PopStyleColor(2);

	// Q-IO tooltip
	if (ImGui::IsItemHovered()) {
		if (ioRealMode) {
			ImGui::SetTooltip("Quick IO Control\n(Real mode - IO Manager available)");
		}
		else if (ioAvailable) {
			ImGui::SetTooltip("Quick IO Control\n(Limited mode - Check IO Manager)");
		}
		else {
			ImGui::SetTooltip("Quick IO Control\n(Not available - IO Manager missing)");
		}
	}

	// Position for JOG button (left of date/time, right of Q-IO button)
	ImGui::SameLine(ImGui::GetWindowWidth() - textSize.x - buttonWidth - spacing - 20);
	ImGui::SetCursorPosY(30);

	// Check if jog window is available
	bool jogAvailable = (m_uiJogWindow != nullptr);

	// **UPDATED: Check if UIJogWindow is showing real GlobalJogPanel or mock mode**
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

	// **UPDATED: Update tooltip to show real vs mock mode**
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

// Update RenderBreadcrumbs() to include pneumatic breadcrumb:

// Update RenderBreadcrumbs() to include Data Instrument breadcrumbs
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

		// 3. UPDATE RenderBreadcrumbs() - Change enum and breadcrumb text:
	case MainPage::DATA_INSTRUMENT:
		breadcrumb += " > Data & Instrument";
		switch (currentDataInstrumentSubPage) {
		case DataInstrumentSubPage::GLOBAL_DATA_STORE:
			breadcrumb += " > Global Data Store Viewer";
			break;
		case DataInstrumentSubPage::TCP_DATA_MANAGER:
			breadcrumb += " > TCP Data Manager";
			break;
		case DataInstrumentSubPage::CLD101X_EQUIPMENT:  // RENAMED enum
			breadcrumb += " > CLD101x Equipment";  // UPDATED breadcrumb text
			break;
		case DataInstrumentSubPage::SMU_MANAGER:
			breadcrumb += " > SMU Manager";
			break;
			// Update RenderBreadcrumbs() - add SPD breadcrumb:
		case DataInstrumentSubPage::SPD_POWER_SUPPLY:
			breadcrumb += " > SPD Power Supply";
			break;
		default:
			break;
		}
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
		switch (currentVisionSubPage) {
		case VisionSubPage::FIDUCIAL:
			breadcrumb += " > Fiducial";
			break;
		case VisionSubPage::DATUM_REFERENCE:
			breadcrumb += " > Datum Reference";
			break;
		case VisionSubPage::MODULE_ALIGNMENT:     // ADD THIS CASE
			breadcrumb += " > Module Alignment";  // ADD THIS CASE
			break;                                // ADD THIS CASE
		}
		break;
	}
	ImGui::Text("%s", breadcrumb.c_str());
}



// Update RenderMainContent() to handle Programming page:
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
		if (currentDataInstrumentSubPage == DataInstrumentSubPage::NONE) {
			RenderDataInstrumentPage();
		}
		else {
			RenderDataInstrumentSubPage();
		}
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
		if (currentVisionSubPage == VisionSubPage::NONE) {
			RenderVisionPage();
		}
		else {
			RenderVisionSubPage();
		}
	}
	else if (currentMainPage == MainPage::PROGRAMMING) {
		if (currentProgrammingSubPage == ProgrammingSubPage::NONE) {
			RenderProgrammingPage();
		}
		else {
			RenderProgrammingSubPage();
		}
	}

	// Update TCP Data Manager if on that page
	if (currentMainPage == MainPage::DATA_INSTRUMENT &&
		currentDataInstrumentSubPage == DataInstrumentSubPage::TCP_DATA_MANAGER) {
		if (m_tcpDataManagerUI) {
			m_tcpDataManagerUI->Update();
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


	// NEW: Show IO control status
	if (m_ioControlPanel) {
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Quick IO Control: Ready");
	}
	else {
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "○ Quick IO Control: Waiting for IO manager");
	}

	ImGui::Spacing();
	ImGui::Text("All systems operational. You can use the Config Editor to manage motion settings");
	ImGui::Text("and the Node Visualizer to view and edit motion graphs interactively.");
}


// In MainUIManager.cpp - Replace the RenderManualPage() method with this fixed version:

// Fix 1: Use simpler emoji alternatives in RenderManualPage()

void MainUIManager::RenderManualPage() {
	ImGui::SetWindowFontScale(1.5f);
	ImGui::Text("Manual Control");
	ImGui::SetWindowFontScale(1.0f);

	ImGui::Spacing();
	ImGui::Text("Select a manual control option:");
	ImGui::Spacing();



	// OPTION 1: Use simple single-character emojis (avoid compound emojis)
	if (ImGui::Button(reinterpret_cast<const char*>(u8"1. PI 🤖"), ImVec2(200, 50))) {
		currentManualSubPage = ManualSubPage::PI;
	}

	if (ImGui::Button(reinterpret_cast<const char*>(u8"2. Gantry 🦾"), ImVec2(200, 50))) { // Changed from 🦿 to 🦾
		currentManualSubPage = ManualSubPage::GANTRY;
	}

	if (ImGui::Button(reinterpret_cast<const char*>(u8"3. IO ⚡"), ImVec2(200, 50))) { // Changed from 🔌 to ⚡
		currentManualSubPage = ManualSubPage::IO;
	}

	if (ImGui::Button(reinterpret_cast<const char*>(u8"4. Pneumatic 💨"), ImVec2(200, 50))) {
		currentManualSubPage = ManualSubPage::PNEUMATIC;
	}

	if (ImGui::Button(reinterpret_cast<const char*>(u8"5. Camera 📷"), ImVec2(200, 50))) {
		currentManualSubPage = ManualSubPage::CAMERA;
	}


}


// Update RenderManualSubPage() to include pneumatic case:
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
	case ManualSubPage::PNEUMATIC:        // Add this case
		RenderPneumaticPage();
		break;
	case ManualSubPage::CAMERA:
		RenderCameraPage();
		break;
	default:
		break;
	}
}

// Add this new method implementation:
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



// Replace RenderPIPage() method with:
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

// Replace the existing MainUIManager::RenderGantryPage() method with:
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



// Replace the existing RenderIOPage() method with:
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



// Update the existing RenderCameraPage() method:
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





// Update RenderDataInstrumentPage() - add the button:
void MainUIManager::RenderDataInstrumentPage() {
	ImGui::SetWindowFontScale(1.5f);
	ImGui::Text("Data & Instrument");
	ImGui::SetWindowFontScale(1.0f);

	ImGui::Spacing();
	ImGui::Text("Select a data monitoring and instrument control option:");
	ImGui::Spacing();

	if (ImGui::Button("1. Global Data Store Viewer", ImVec2(250, 50))) {
		currentDataInstrumentSubPage = DataInstrumentSubPage::GLOBAL_DATA_STORE;
	}

	if (ImGui::Button("2. TCP Data Manager", ImVec2(250, 50))) {
		currentDataInstrumentSubPage = DataInstrumentSubPage::TCP_DATA_MANAGER;
	}

	if (ImGui::Button("3. CLD101x Equipment", ImVec2(250, 50))) {
		currentDataInstrumentSubPage = DataInstrumentSubPage::CLD101X_EQUIPMENT;
	}

	if (ImGui::Button("4. SMU Manager", ImVec2(250, 50))) {
		currentDataInstrumentSubPage = DataInstrumentSubPage::SMU_MANAGER;
	}

	// ADD THIS - New SPD button
	if (ImGui::Button("5. SPD Power Supply", ImVec2(250, 50))) {
		currentDataInstrumentSubPage = DataInstrumentSubPage::SPD_POWER_SUPPLY;
	}
}


// Add new method to handle Data Instrument sub-pages




// ==============================================================================
// IMPLEMENTATION FILE CHANGES (MainUIManager.cpp)
// ==============================================================================


// 2. UPDATE RenderDataInstrumentSubPage() - Change enum and method call:
void MainUIManager::RenderDataInstrumentSubPage() {
	switch (currentDataInstrumentSubPage) {
	case DataInstrumentSubPage::GLOBAL_DATA_STORE:
		RenderGlobalDataStorePage();
		break;
	case DataInstrumentSubPage::TCP_DATA_MANAGER:
		RenderTcpDataManagerPage();
		break;
	case DataInstrumentSubPage::CLD101X_EQUIPMENT:  // RENAMED enum
		RenderCld101xEquipmentPage();  // RENAMED method call
		break;
	case DataInstrumentSubPage::SMU_MANAGER:
		RenderSmuManagerPage();
		break;
	case DataInstrumentSubPage::SPD_POWER_SUPPLY:  // ADD THIS
		RenderSPDPowerSupplyPage();
		break;
	default:
		break;
	}
}





// Replace the old RenderGlobalDataStorePage() method with:
void MainUIManager::RenderGlobalDataStorePage() {
	if (m_globalDataStoreViewerUI) {
		m_globalDataStoreViewerUI->Render();
	}
	else {
		ImGui::SetWindowFontScale(1.5f);
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Global Data Store Viewer");
		ImGui::SetWindowFontScale(1.0f);

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Failed to create Global Data Store Viewer UI");
		ImGui::Text("Check console for error messages");
	}
}


// Replace the placeholder RenderTcpDataManagerPage() method:
void MainUIManager::RenderTcpDataManagerPage() {
	if (m_tcpDataManagerUI) {
		m_tcpDataManagerUI->Render();
	}
	else {
		ImGui::SetWindowFontScale(1.5f);
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "TCP Data Manager");
		ImGui::SetWindowFontScale(1.0f);

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Failed to create TCP Data Manager UI");
		ImGui::Text("Check console for error messages");
	}
}





// In MainUIManager::SetCLD101xManager method, add:
void MainUIManager::SetCLD101xManager(CLD101xManager* cld101xManager) {
	m_cld101xManager = cld101xManager;


	if (cld101xManager) {
		cld101xManager->EnableGlobalDataStoreForAll(true);
		Logger::GetInstance()->LogInfo("MainUIManager: Enabled Global Data Store for CLD101x equipment");
	}
}



void MainUIManager::RenderCld101xEquipmentPage() {
	if (m_cld101xEquipmentUI) {
		// Use the equipment UI directly
		m_cld101xEquipmentUI->Render();
	}
	else {
		// Fallback when UI not available
		ImGui::SetWindowFontScale(1.5f);
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "CLD101x Equipment Control");
		ImGui::SetWindowFontScale(1.0f);

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "CLD101x UI not initialized");
		ImGui::Text("This is an internal error - please check the console for details.");
	}
}


// Update RenderSmuManagerPage() method to use the dedicated UI:
void MainUIManager::RenderSmuManagerPage() {
	if (m_smuPanelUI) {
		m_smuPanelUI->RenderUI();
	}
	else {
		ImGui::SetWindowFontScale(1.5f);
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "SMU Manager");
		ImGui::SetWindowFontScale(1.0f);

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Failed to create SMU Panel UI");
		ImGui::Text("Check console for error messages");
		ImGui::Text("Ensure Keithley2400Manager is properly initialized");
	}
}




void MainUIManager::RenderRunProgramPage() {
	if (m_runPageUI) {
		m_runPageUI->RenderUI();
	}
	else {
		// Fallback if RunPageUI is not initialized
		ImGui::Text("Run Program");
		ImGui::Separator();
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
			"Machine Operations not initialized. RunPageUI unavailable.");
		ImGui::Text("Please ensure the machine system is properly configured.");

		// NEW: Add button to try setup if MachineOperations is available
		if (m_machineOperations && ImGui::Button("Initialize RunPageUI")) {
			SetMachineOperations(m_machineOperations);
		}
	}
}


// MODIFY the existing RenderConfigPage() method to include watchdog status

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

	// ADD THIS - Watchdog status in config page
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (m_configWatchdog) {
		RenderWatchdogStatus(m_configWatchdog);
	}
	else {
		ImGui::Text("📂 Configuration File Watchdog: Not Available");
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Watchdog not initialized or database unavailable");
	}
}



// Update RenderVisionPage() to include the third button:
void MainUIManager::RenderVisionPage() {
	ImGui::SetWindowFontScale(1.5f);
	ImGui::Text("Vision System");
	ImGui::SetWindowFontScale(1.0f);

	ImGui::Spacing();
	ImGui::Text("Select a vision processing option:");
	ImGui::Spacing();

	if (ImGui::Button("1. Fiducial Detection", ImVec2(250, 50))) {
		currentVisionSubPage = VisionSubPage::FIDUCIAL;
	}

	if (ImGui::Button("2. Datum Reference", ImVec2(250, 50))) {
		currentVisionSubPage = VisionSubPage::DATUM_REFERENCE;
	}

	// ADD THIS: Third button for Module Alignment
	if (ImGui::Button("3. Module Alignment", ImVec2(250, 50))) {
		currentVisionSubPage = VisionSubPage::MODULE_ALIGNMENT;
	}
}


// Update RenderVisionSubPage() to handle Module Alignment:
void MainUIManager::RenderVisionSubPage() {
	switch (currentVisionSubPage) {
	case VisionSubPage::FIDUCIAL:
		RenderFiducialPage();
		break;
	case VisionSubPage::DATUM_REFERENCE:
		RenderDatumReferencePage();
		break;
	case VisionSubPage::MODULE_ALIGNMENT:       // ADD THIS CASE
		RenderModuleAlignmentPage();             // ADD THIS CASE
		break;                                   // ADD THIS CASE
	default:
		break;
	}
}

// 6. ADD Fiducial page method (move current UIVisionPanel here):
void MainUIManager::RenderFiducialPage() {
	// Move the current UIVisionPanel rendering here
	if (m_visionPanelUI) {
		m_visionPanelUI->RenderUI();
	}
}

// 7. ADD Datum Reference page method (placeholder for now):

// REPLACE the existing RenderDatumReferencePage() method with:
void MainUIManager::RenderDatumReferencePage() {
	if (m_datumUI) {
		m_datumUI->RenderUI();
	}
	else {
		// Fallback if DatumUI is not initialized
		ImGui::SetWindowFontScale(1.5f);
		ImGui::Text("Datum Reference");
		ImGui::SetWindowFontScale(1.0f);

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Datum Reference System not available");
		ImGui::Text("DatumUI has not been initialized.");
		ImGui::Spacing();
		ImGui::Text("This is an internal error - please check the console for details.");
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


void MainUIManager::SetPIControllerManager(PIControllerManager* piManager) {
	m_piControllerManager = piManager;

	// Create PI Panel UI when PI manager is available
	if (m_piControllerManager) {
		m_piPanelUI = std::make_unique<PIPanelUI>(*m_piControllerManager);
		std::cout << "MainUIManager: PI Panel UI created successfully" << std::endl;
	}

	// **UPDATED: Update UIJogWindow with PI controller manager**
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
		// Future: Create ACS Panel UI similar to PI Panel UI
		m_acsPanelUI = std::make_unique<ACSPanelUI>(*m_acsControllerManager);
		std::cout << "MainUIManager: ACS Panel UI created successfully" << std::endl;
	}
	// **UPDATED: Update UIJogWindow with ACS controller manager**
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

		// NEW: Create IO Control Panel for Q-IO button
		CreateIOControlPanel();  // <-- ADD THIS LINE
	}
}

// NEW: Helper method to create IO Control Panel
void MainUIManager::CreateIOControlPanel() {
	if (m_ioManager && !m_ioControlPanel) {
		m_ioControlPanel = std::make_unique<IOControlPanel>(*m_ioManager);

		// Load default configuration
		m_ioControlPanel->LoadConfiguration("io_panel_config.json");

		std::cout << "MainUIManager: IO Control Panel created successfully for Q-IO button" << std::endl;
	}
}


// Add this method implementation:
void MainUIManager::SetPneumaticManager(PneumaticManager* pneumaticManager) {
	m_pneumaticManager = pneumaticManager;

	// Create Pneumatic Panel UI when pneumatic manager is available
	if (m_pneumaticManager) {
		m_pneumaticPanelUI = std::make_unique<UIPneumaticPanel>(*m_pneumaticManager);
		std::cout << "MainUIManager: Pneumatic Panel UI created successfully" << std::endl;
	}
}



void MainUIManager::SetDataClientManager(DataClientManager* dataClientManager) {
	m_dataClientManager = dataClientManager;

	if (m_dataClientManager) {
		std::cout << "MainUIManager: DataClientManager set successfully" << std::endl;

		// Initialize TCPDataManagerUI with the external DataClientManager
		if (m_tcpDataManagerUI) {
			if (m_tcpDataManagerUI->Initialize(m_dataClientManager)) {
				std::cout << "MainUIManager: TCPDataManagerUI initialized with external DataClientManager" << std::endl;
			}
			else {
				std::cout << "MainUIManager: Failed to initialize TCPDataManagerUI" << std::endl;
			}
		}

		// Pass the data client manager to the Global Data Store Viewer
		if (m_globalDataStoreViewerUI) {
			m_globalDataStoreViewerUI->SetDataClientManager(m_dataClientManager);
		}
	}
}

// Add this method implementation with other setter methods:
void MainUIManager::SetKeithley2400Manager(Keithley2400Manager* keithleyManager) {
	m_keithleyManager = keithleyManager;

	// Create SMU Panel UI when SMU manager is available
	if (m_keithleyManager) {
		m_smuPanelUI = std::make_unique<UISMUPanel>(*m_keithleyManager);
		std::cout << "MainUIManager: SMU Panel UI created successfully" << std::endl;
	}
}

// Add this method implementation:
// In MainUIManager.cpp - Update the SetCameraManager method
// Add this code to pass camera manager to RunPageUI

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

	// NEW: Pass camera manager to MacroPanelUI for camera handler
	if (m_macroPanelUI && m_cameraManager) {
		m_macroPanelUI->SetCameraManager(m_cameraManager);
		std::cout << "MainUIManager: MacroPanelUI updated with Camera Manager" << std::endl;
	}

	// IMPORTANT: Pass camera manager to RunPageUI if it exists
	if (m_runPageUI && m_cameraManager) {
		m_runPageUI->SetCameraManager(m_cameraManager);
		std::cout << "MainUIManager: RunPageUI updated with Camera Manager" << std::endl;
	}


	// ADD THIS: Connect camera manager to vision panel
	if (m_visionPanelUI && m_cameraManager) {
		m_visionPanelUI->SetCameraManager(m_cameraManager);
		std::cout << "MainUIManager: Vision Panel connected to Camera Manager" << std::endl;
	}

	// ADD THIS: Connect camera manager to ModuleAlignmentUI
	if (m_moduleAlignmentUI && m_cameraManager) {
		m_moduleAlignmentUI->SetCameraManager(m_cameraManager);
		std::cout << "MainUIManager: ModuleAlignmentUI updated with Camera Manager" << std::endl;
	}

}




// REPLACE the SetMachineOperations method in MainUIManager.cpp with this fixed version

void MainUIManager::SetMachineOperations(MachineOperations* machineOps) {
	m_machineOperations = machineOps;

	if (m_machineOperations) {
		std::cout << "MainUIManager: MachineOperations set successfully" << std::endl;

		uiConfigVisualizer->SetMachineOperations(m_machineOperations);

		// Pass MachineOperations to MachineBlockUI for real execution
		if (m_machineBlockUI && machineOps) {
			m_machineBlockUI->SetMachineOperations(machineOps);
			std::cout << "MainUIManager: MachineBlockUI updated with MachineOperations" << std::endl;
		}

		// IMPORTANT: Reconnect MacroManager to MachineBlockUI after MachineOperations is set
		if (m_macroManager && m_machineBlockUI) {
			m_macroManager->SetMachineBlockUI(m_machineBlockUI.get());
			std::cout << "MainUIManager: MacroManager reconnected with initialized MachineBlockUI" << std::endl;
		}

		// NEW: Reconnect MacroPanelUI after MachineOperations is set
		if (m_macroPanelUI && m_macroManager && m_machineBlockUI) {
			m_macroPanelUI->SetMacroManager(m_macroManager.get());
			m_macroPanelUI->SetMachineBlockUI(m_machineBlockUI.get());
			std::cout << "MainUIManager: MacroPanelUI reconnected with initialized components" << std::endl;
		}

		// Create UserPromptUI and RunPageUI when MachineOperations is available
		if (!m_userPromptUI) {
			// 1. Create UserPromptUI
			m_userPromptUI = std::make_unique<UserPromptUI>();
			std::cout << "MainUIManager: UserPromptUI created" << std::endl;
		}

		// ADD THIS: Connect to ModuleAlignmentUI
		if (m_moduleAlignmentUI) {
			m_moduleAlignmentUI->SetMachineOperations(m_machineOperations);
			std::cout << "MainUIManager: ModuleAlignmentUI updated with MachineOperations" << std::endl;
		}

		if (!m_runPageUI) {
			// 2. Create RunPageUI
			m_runPageUI = std::make_unique<RunPageUI>(*m_machineOperations);
			std::cout << "MainUIManager: RunPageUI created" << std::endl;

			// 3. Connect UserPromptUI to RunPageUI
			if (m_userPromptUI) {
				m_runPageUI->SetUserPromptUI(m_userPromptUI.get());
				std::cout << "MainUIManager: UserPromptUI connected to RunPageUI" << std::endl;
			}

			// Connect UserPromptUI to RunPageUI
			if (m_userPromptUI) {
				m_runPageUI->SetUserPromptUI(m_userPromptUI.get());
				std::cout << "MainUIManager: UserPromptUI connected to RunPageUI" << std::endl;

				// VERIFY the connection worked
				if (m_runPageUI->HasUserPromptUI()) {
					std::cout << "✓ UserPromptUI connection verified" << std::endl;
				}
				else {
					std::cout << "✗ UserPromptUI connection FAILED" << std::endl;
				}
			}
			else {
				std::cout << "✗ UserPromptUI is NULL!" << std::endl;
			}

			// 4. Set font if available
			if (m_imguiFont) {
				m_runPageUI->SetImguiFont(m_imguiFont);
				std::cout << "MainUIManager: Font set on RunPageUI" << std::endl;
			}

			// 5. CRITICAL: Set camera manager if available
			if (m_cameraManager) {
				m_runPageUI->SetCameraManager(m_cameraManager);
				std::cout << "MainUIManager: Camera Manager set on RunPageUI" << std::endl;
			}

			// NEW: Connect to vision panel
			if (m_visionPanelUI) {
				m_visionPanelUI->SetMachineOperations(m_machineOperations);
				std::cout << "MainUIManager: Vision Panel updated with MachineOperations" << std::endl;
			}

			// Also connect MotionConfigManager if available
			if (m_visionPanelUI) {
				m_visionPanelUI->SetMotionConfigManager(&motionConfigManager);
				std::cout << "MainUIManager: Vision Panel updated with MotionConfigManager" << std::endl;
			}

		}


	}
	else {
		std::cout << "MainUIManager: MachineOperations cleared" << std::endl;
	}
}



// Optionally, add a getter method as well
MachineOperations* MainUIManager::GetMachineOperations() {
	return m_machineOperations;
}


// Implement RenderProgrammingPage():
void MainUIManager::RenderProgrammingPage() {
	ImGui::SetWindowFontScale(1.5f);
	ImGui::Text("Programming");
	ImGui::SetWindowFontScale(1.0f);

	ImGui::Spacing();
	ImGui::Text("Select programming tool:");
	ImGui::Spacing();

	// Two main buttons for Step 2
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 15));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));

	if (ImGui::Button("Block Script", ImVec2(200, 80))) {
		currentProgrammingSubPage = ProgrammingSubPage::MACHINE_BLOCK_UI;
	}

	ImGui::SameLine();
	ImGui::Spacing();
	ImGui::SameLine();

	if (ImGui::Button("Macro Program", ImVec2(200, 80))) {
		currentProgrammingSubPage = ProgrammingSubPage::MACRO_MANAGER;
	}

	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar();

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// Add descriptions
	ImGui::Text("Block Script: Visual block-based programming interface");
	ImGui::Text("Macro Program: Sequence multiple programs into macros");
}



// 7. Update RenderProgrammingSubPage() to handle both sub-pages:
void MainUIManager::RenderProgrammingSubPage() {
	if (currentProgrammingSubPage == ProgrammingSubPage::MACHINE_BLOCK_UI) {
		RenderMachineBlockPage();
	}
	else if (currentProgrammingSubPage == ProgrammingSubPage::MACRO_MANAGER) {
		RenderMacroManagerPage();  // Add this case
	}
}

// Implement RenderMachineBlockPage():
void MainUIManager::RenderMachineBlockPage() {
	if (m_machineBlockUI) {
		m_machineBlockUI->RenderUI();
	}
	else {
		ImGui::SetWindowFontScale(1.5f);
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Machine Block UI");
		ImGui::SetWindowFontScale(1.0f);
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Failed to create Machine Block UI");
		ImGui::Text("Check console for error messages");
	}
}


// 5. UPDATE RenderMacroManagerPage() to use the new MacroPanelUI:
void MainUIManager::RenderMacroManagerPage() {
	if (m_macroPanelUI) {
		// Use the new clean MacroPanelUI instead of MacroManager's RenderUI()
		m_macroPanelUI->RenderMacroPanel();
	}
	else {
		ImGui::SetWindowFontScale(1.5f);
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Macro Panel UI");
		ImGui::SetWindowFontScale(1.0f);

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Failed to create Macro Panel UI");
		ImGui::Text("Check console for error messages");
	}
}

void MainUIManager::SetConfigWatchdog(ConfigFileWatchdog* watchdog) {
	m_configWatchdog = watchdog;
	if (watchdog) {
		std::cout << "MainUIManager: Config watchdog connected" << std::endl;
	}
}



// ADD this helper function (can be private method or standalone function)

void MainUIManager::RenderWatchdogStatus(ConfigFileWatchdog* watchdog) {
	if (!watchdog) {
		return;
	}

	// Create a collapsible section in your UI
	if (ImGui::CollapsingHeader("📂 Configuration File Watchdog")) {
		auto stats = watchdog->GetStatistics();

		// Status indicators
		bool running = stats["running"].get<bool>();
		if (running) {
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "● RUNNING");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "● STOPPED");
		}

		ImGui::SameLine();
		ImGui::Text("Uptime: %d minutes", stats["uptime_minutes"].get<int>());

		// Statistics
		ImGui::Separator();
		ImGui::Text("Statistics:");
		ImGui::BulletText("Changes detected: %d", stats["changes_detected"].get<int>());
		ImGui::BulletText("Database updates: %d", stats["database_updates"].get<int>());
		ImGui::BulletText("Update failures: %d", stats["update_failures"].get<int>());
		ImGui::BulletText("Poll interval: %d ms", stats["poll_interval_ms"].get<int>());

		// Watched files
		ImGui::Separator();
		ImGui::Text("Watched Files (%d):", stats["watched_files_count"].get<int>());

		if (stats.contains("files") && stats["files"].is_array()) {
			for (const auto& file : stats["files"]) {
				std::string filename = file["path"].get<std::string>();
				bool exists = file["exists"].get<bool>();

				if (exists) {
					ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓");
				}
				else {
					ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "✗");
				}
				ImGui::SameLine();
				ImGui::Text("%s", filename.c_str());
			}
		}

		// Recent events
		ImGui::Separator();
		if (ImGui::TreeNode("Recent Events")) {
			auto recentEvents = watchdog->GetRecentEvents(5);

			if (recentEvents.empty()) {
				ImGui::Text("No recent events");
			}
			else {
				for (const auto& event : recentEvents) {
					if (event.updateSuccess) {
						ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓");
					}
					else {
						ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "✗");
					}
					ImGui::SameLine();
					ImGui::Text("%s", event.filename.c_str());

					if (!event.errorMessage.empty()) {
						ImGui::SameLine();
						ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
							"(%s)", event.errorMessage.c_str());
					}
				}
			}
			ImGui::TreePop();
		}

		// Control buttons
		ImGui::Separator();
		if (running) {
			if (ImGui::Button("Stop Watchdog")) {
				watchdog->Stop();
			}
		}
		else {
			if (ImGui::Button("Start Watchdog")) {
				watchdog->Start();
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Force Check Now")) {
			int changes = watchdog->ForceCheck();
			// Show changes detected in console/log
		}
	}
}

void MainUIManager::SetImguiFont(ImFont* font) {
	if (font) {
		m_imguiFont = font;
		std::cout << "MainUIManager: Custom font set successfully" << std::endl;

		// Set font for RunPageUI only if it exists
		if (m_runPageUI) {
			m_runPageUI->SetImguiFont(m_imguiFont);
		}
	}
	else {
		std::cerr << "MainUIManager: Failed to set custom font - font is null" << std::endl;
	}
}

void MainUIManager::ProcessKeyInput(SDL_Keycode key, bool pressed) {
	// Forward keyboard input to UIJogWindow if it exists
	if (m_uiJogWindow) {
		m_uiJogWindow->ProcessKeyInput(key, pressed);
	}
}

// Alternative: Add a dedicated method for setting up vision panel
void MainUIManager::SetupVisionPanel() {
	if (!m_visionPanelUI) {
		std::cout << "MainUIManager: Vision Panel not created" << std::endl;
		return;
	}

	// Connect MotionConfigManager
	m_visionPanelUI->SetMotionConfigManager(&motionConfigManager);
	std::cout << "MainUIManager: Connected MotionConfigManager to Vision Panel" << std::endl;

	// Connect MachineOperations if available
	if (m_machineOperations) {
		m_visionPanelUI->SetMachineOperations(m_machineOperations);
		std::cout << "MainUIManager: Connected MachineOperations to Vision Panel" << std::endl;
	}

	// Connect CameraManager if available
	if (m_cameraManager) {
		m_visionPanelUI->SetCameraManager(m_cameraManager);
		std::cout << "MainUIManager: Connected CameraManager to Vision Panel" << std::endl;
	}


}

// Add the new RenderModuleAlignmentPage() method:
void MainUIManager::RenderModuleAlignmentPage() {
	if (m_moduleAlignmentUI) {
		m_moduleAlignmentUI->RenderUI();
	}
	else {
		// Fallback if ModuleAlignmentUI is not initialized
		ImGui::SetWindowFontScale(1.5f);
		ImGui::Text("Module Alignment");
		ImGui::SetWindowFontScale(1.0f);

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Module Alignment System not available");
		ImGui::Text("ModuleAlignmentUI has not been initialized.");
		ImGui::Spacing();
		ImGui::Text("This is an internal error - please check the console for details.");
	}
}

// 1. ADD the smart getter methods implementations
PIControllerManager* MainUIManager::GetPIController() const {
	if (m_context) {
		if (auto* pi = m_context->GetPIController()) return pi;
	}
	return m_piControllerManager;  // Fallback to old way
}

ACSControllerManager* MainUIManager::GetACSController() const {
	if (m_context) {
		if (auto* acs = m_context->GetACSController()) return acs;
	}
	return m_acsControllerManager;  // Fallback to old way
}

CameraManager* MainUIManager::GetCameraManagerSmart() const {
	if (m_context) {
		if (auto* camera = m_context->GetCameraManager()) return camera;
	}
	return m_cameraManager;  // Fallback to old way
}

EziIOManager* MainUIManager::GetIOManager() const {
	if (m_context) {
		if (auto* io = m_context->GetIOManager()) return io;
	}
	return m_ioManager;  // Fallback to old way
}

IOConfigManager* MainUIManager::GetIOConfig() const {
	if (m_context) {
		if (auto* ioConfig = m_context->GetIOConfig()) return ioConfig;
	}
	return m_ioConfigManager;  // Fallback to old way
}

PneumaticManager* MainUIManager::GetPneumaticManager() const {
	if (m_context) {
		if (auto* pneumatic = m_context->GetPneumaticManager()) return pneumatic;
	}
	return m_pneumaticManager;  // Fallback to old way
}

DataClientManager* MainUIManager::GetDataClient() const {
	if (m_context) {
		if (auto* dataClient = m_context->GetDataClient()) return dataClient;
	}
	return m_dataClientManager;  // Fallback to old way
}

Keithley2400Manager* MainUIManager::GetKeithley() const {
	if (m_context) {
		if (auto* keithley = m_context->GetKeithley()) return keithley;
	}
	return m_keithleyManager;  // Fallback to old way
}

// Add SPD manager getter method to MainUIManager (add to both .h and .cpp):
SPDPowerSupplyManager* MainUIManager::GetSPDManager() const {
	if (m_context) {
		if (auto* spd = m_context->GetSPDPowerSupply()) return spd;
	}
	return nullptr;  // Add fallback member if you add it later
}

CLD101xManager* MainUIManager::GetCLD101x() const {
	if (m_context) {
		if (auto* cld101x = m_context->GetCLD101x()) {
			Logger::GetInstance()->LogInfo("GetCLD101x: Found in AppContext");
			return cld101x;
		}
	}
	if (m_cld101xManager) {
		Logger::GetInstance()->LogInfo("GetCLD101x: Using fallback member variable");
		return m_cld101xManager;
	}
	Logger::GetInstance()->LogWarning("GetCLD101x: No CLD101x manager available!");
	return nullptr;
}


// Add the render method implementation:
void MainUIManager::RenderSPDPowerSupplyPage() {
	if (m_spdPowerSupplyUI) {
		m_spdPowerSupplyUI->RenderUI();
	}
	else {
		ImGui::SetWindowFontScale(1.5f);
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "SPD Power Supply Control");
		ImGui::SetWindowFontScale(1.0f);

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "SPD UI not initialized");
		ImGui::Text("SPD Power Supply Manager may not be available.");
	}
}