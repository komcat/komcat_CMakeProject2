#include "MainUIManager.h"
#include "UIConfigEditor.h"
#include "UIConfigVisualizer.h"
#include "UIJogWindow.h"
#include "include/motions/MotionConfigManager.h"
#include "include/motions/pi_controller_manager.h"
#include "include/motions/acs_controller_manager.h"
#include "include/cld101x_manager.h"
#include "CLD101xEquipmentUI.h"
// Add this include at the top:
#include "PIPanelUI.h"
#include "ACSPanelUI.h"
// Add this include at the top:
#include "IOPanelUI.h"
#include "UIPneumaticPanel.h"
// Add this include at the top with other includes:
#include "UISMUPanel.h"
#include "UserPromptUI.h"  // ADD THIS LINE

#include "include/data/global_data_store.h" // Add this with your other includes
#include "include/machine_operations.h"  // Add this include at the top


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

	// STEP 5: Connect prompt UI to MachineBlockUI as well (if it has this method)
	// if (m_machineBlockUI) {
	//     m_machineBlockUI->SetPromptUI(m_promptUI.get());
	// }

	// Initialize TCP Data Manager UI
	m_tcpDataManagerUI = std::make_unique<TCPDataManagerUI>();
	if (!m_tcpDataManagerUI->Initialize()) {
		// Log error but continue - the UI will show the error state
		std::cout << "Warning: TCP Data Manager failed to initialize" << std::endl;
	}

	// Initialize Global Data Store Viewer UI
	m_globalDataStoreViewerUI = std::make_unique<GlobalDataStoreViewerUI>();

	//try {
	//	m_cld101xEquipmentUI = std::make_unique<CLD101xEquipmentUI>();
	//}
	//catch (const std::exception& e) {
	//	// Handle initialization error if needed
	//	m_cld101xEquipmentUI = nullptr;
	//}
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
		currentProgrammingSubPage = ProgrammingSubPage::NONE;  // Reset programming sub-page
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

// In RenderDateTime method - UPDATE the JOG button logic:
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
		RenderVisionPage();
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

	ImGui::Spacing();
	ImGui::Text("All systems operational. You can use the Config Editor to manage motion settings");
	ImGui::Text("and the Node Visualizer to view and edit motion graphs interactively.");
}


// Update RenderManualPage() to include the pneumatic button:
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

	if (ImGui::Button("4. Pneumatic", ImVec2(200, 50))) {    // Add this button
		currentManualSubPage = ManualSubPage::PNEUMATIC;
	}

	if (ImGui::Button("5. Camera", ImVec2(200, 50))) {       // Update number
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




// Update RenderDataInstrumentPage() to show the 4 buttons

// 1. UPDATE RenderDataInstrumentPage() - Change button text and enum:
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

	// UPDATED BUTTON TEXT AND ENUM:
	if (ImGui::Button("3. CLD101x Equipment", ImVec2(250, 50))) {
		currentDataInstrumentSubPage = DataInstrumentSubPage::CLD101X_EQUIPMENT;
	}

	if (ImGui::Button("4. SMU Manager", ImVec2(250, 50))) {
		currentDataInstrumentSubPage = DataInstrumentSubPage::SMU_MANAGER;
	}
}


// Add new method to handle Data Instrument sub-pages

// ==============================================================================
// HEADER FILE CHANGES (MainUIManager.h)
// ==============================================================================

// 1. UPDATE the DataInstrumentSubPage enum - RENAME CLD101X_TEC to CLD101X_EQUIPMENT:
enum class DataInstrumentSubPage {
	NONE,
	GLOBAL_DATA_STORE,
	TCP_DATA_MANAGER,
	CLD101X_EQUIPMENT,    // RENAMED from CLD101X_TEC
	SMU_MANAGER
};

// 2. UPDATE method declaration in private section:
// CHANGE: void RenderCld101xTecPage();
// TO:
void RenderCld101xEquipmentPage();


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




// ==============================================================================
// HEADER FILE CHANGES (MainUIManager.h)
// ==============================================================================

// 1. ADD FORWARD DECLARATION at the top with other forward declarations:
class CLD101xManager;

// 2. ADD METHOD DECLARATION in public section with other SetXXXManager methods:
void SetCLD101xManager(CLD101xManager* cld101xManager);

// 3. ADD MEMBER VARIABLE in private section with other manager pointers:
CLD101xManager* m_cld101xManager = nullptr;


// ==============================================================================
// IMPLEMENTATION FILE CHANGES (MainUIManager.cpp)
// ==============================================================================


// In MainUIManager::SetCLD101xManager method, add:
void MainUIManager::SetCLD101xManager(CLD101xManager* cld101xManager) {
	m_cld101xManager = cld101xManager;

	// REMOVE the CLD101xEquipmentUI connection:
	// if (m_cld101xEquipmentUI) {
	//     m_cld101xEquipmentUI->SetCLD101xManager(cld101xManager);
	// }

	// KEEP the Global Data Store integration:
	if (cld101xManager) {
		cld101xManager->EnableGlobalDataStoreForAll(true);
		Logger::GetInstance()->LogInfo("MainUIManager: Enabled Global Data Store for CLD101x equipment");
	}
}


// 2. UPDATE RenderCld101xEquipmentPage() to use the manager:
void MainUIManager::RenderCld101xEquipmentPage() {
	if (m_cld101xManager) {
		// Render manager UI directly
		m_cld101xManager->RenderUI();

		// Also render individual client UIs if needed
		auto clientNames = m_cld101xManager->GetClientNames();
		for (const auto& clientName : clientNames) {
			auto client = m_cld101xManager->GetClient(clientName);
			if (client && client->IsVisible()) {
				client->RenderUI();
			}
		}
	}
	else {
		// Fallback when manager not available
		ImGui::SetWindowFontScale(1.5f);
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "CLD101x Equipment Control");
		ImGui::SetWindowFontScale(1.0f);

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "CLD101x Manager not available");
		ImGui::Text("Equipment control is not initialized.");
		ImGui::Spacing();
		ImGui::Text("This typically means:");
		ImGui::BulletText("CLD101x manager is still loading");
		ImGui::BulletText("No CLD101x devices are configured");
		ImGui::BulletText("Hardware connection issues");
		ImGui::BulletText("Check network connectivity to laser controllers");
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


// Add this method implementation:
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
}


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
		// This ensures MacroManager has access to the fully initialized MachineBlockUI
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

		// Optionally, you can create any UI components that depend on MachineOperations here
		// For example, if you had a MachineOperationsUI panel:
		// m_machineOpsPanelUI = std::make_unique<MachineOperationsPanelUI>(*m_machineOperations);

		// You could also pass the MachineOperations to existing UI components that need it
		// For example, if UIJogWindow needs MachineOperations:
		if (m_uiJogWindow) {
			// m_uiJogWindow->SetMachineOperations(m_machineOperations);
		}

		// Or pass it to other panel UIs that might need access to high-level operations
		if (m_piPanelUI) {
			// m_piPanelUI->SetMachineOperations(m_machineOperations);
		}

		if (m_acsPanelUI) {
			// m_acsPanelUI->SetMachineOperations(m_machineOperations);
		}

		if (m_pneumaticPanelUI) {
			// m_pneumaticPanelUI->SetMachineOperations(m_machineOperations);
		}

		if (m_cameraPanelUI) {
			// m_cameraPanelUI->SetMachineOperations(m_machineOperations);
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