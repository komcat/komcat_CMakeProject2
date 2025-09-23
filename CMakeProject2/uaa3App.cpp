#include <iostream>
#include <SDL.h>
#include <SDL_opengl.h>

// Windows specific - include early
#ifdef _WIN32
#include "WinSockGuard.h"
#endif

// ImGui includes
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"
#include "implot/implot.h"

// FreeType support
#ifdef IMGUI_ENABLE_FREETYPE
#include "misc/freetype/imgui_freetype.h"
#endif

// Core includes
#include "MainUIManager.h"
#include "include/logger.h"
#include "include/data/global_data_store.h"
#include "Version.h"
#include "MenuManager_uaa3.h"
#include "IDSCameraUI.h"
#include "AppContext.h"
#include "RaylibCameraManager.h"
#include "ApplicationInitializer.h"
#include "ConsoleProgressDisplay.h"
#include "CameraConfigManager.h"
#include "include/cld101x/cld101x_manager.h"
#include "include/SMU/keithley2400_manager.h"
#include "include/ops/vision_ops.h"
#include "include/ops/io_ops.h"
#include "include/ops/motion_ops.h"
#include "include/cld101x/cld101x_operations.h"
#include "include/splashscreen/SimpleSplashScreen.h"
#include "include/PowerSupply/SPDPowerSupplyManager.h"
#include "include/PowerSupplyDevice/PowerSupplyManager.h"
#include "include/PowerSupplyDevice/MockPowerSupplyDevice.h"
#include "FileResultStorage.h"
#include "include/PowerSupplyDevice/PowerSupplyTestUI.h"


// Add these with your other includes
#include "include/scanning/grid_scanner.h"
#include "include/scanning/grid_scanner_ui.h"
#include "include/scanning/i_scan_motion_controller.h"
#include "include/scanning/PIScanMotionAdapter.h"
#include "include/scanning/grid_scanner_manager.h"
#include "include/scanning/grid_volume_scanner_manager.h"
#include "include/vision/VisionCameraExposureUI.h"
#include "include/data/DUTDatabaseViewerUI.h"
#include "SystemStatusUI.h"


// Keep your debug function as-is
bool g_deugMode = false; // Global debug mode flag








//void TestPowerSupplyManager() {
//	Logger* logger = Logger::GetInstance();
//	logger->LogInfo("=== POWER SUPPLY MANAGER TEST ===");
//
//	// Create manager and storage
//	auto psManager = std::make_shared<PowerSupplyManager>();
//	auto storage = std::make_shared<FileResultStorage>();
//
//	// Initialize storage
//	if (!storage->Initialize("./power_supply_test_data")) {
//		logger->LogError("Failed to initialize storage");
//		return;
//	}
//	psManager->SetResultStorage(storage);
//	logger->LogInfo("✓ Storage initialized");
//
//	// Add mock devices for testing
//	auto ps1 = std::make_shared<MockPowerSupplyDevice>("TestPS1", 1, "MOCK-1000");
//	auto ps2 = std::make_shared<MockPowerSupplyDevice>("TestPS2", 2, "MOCK-2000");
//
//	psManager->AddDevice(ps1, "PS1");
//	psManager->AddDevice(ps2, "PS2");
//	logger->LogInfo("✓ Added 2 mock devices");
//
//	// Connect devices
//	auto connectResult = psManager->ConnectAllDevices();
//	logger->LogInfo("✓ Connected " + std::to_string(connectResult.successCount) + " devices");
//
//	// Configure devices
//	psManager->SetModeConstantVoltage("PS1");
//	psManager->SetVoltage("PS1", 5.0f);
//	psManager->SetCurrent("PS1", 1.0f);
//	psManager->TurnOn("PS1");
//	logger->LogInfo("✓ PS1 configured: 5V, 1A limit, CV mode");
//
//	// Take measurement
//	auto measurement = psManager->ReadMeasurement("PS1");
//	logger->LogInfo("✓ Measurement: V=" + std::to_string(measurement.voltage) +
//		", I=" + std::to_string(measurement.current));
//
//	// Store measurement
//	psManager->StoreCurrentMeasurement("PS1", "test_measurement");
//	logger->LogInfo("✓ Measurement stored");
//
//	// Run a sweep
//	IPowerSupplyDevice::SweepConfig sweepConfig;
//	sweepConfig.mode = IPowerSupplyDevice::SweepConfig::Mode::CONSTANT_VOLTAGE;
//	sweepConfig.startValue = 0.0f;
//	sweepConfig.endValue = 3.0f;
//	sweepConfig.stepSize = 0.5f;
//	sweepConfig.delayMs = 100;
//
//	logger->LogInfo("Starting sweep...");
//	auto sweepResult = psManager->ExecuteSweepBlocking("PS1", sweepConfig);
//
//	if (sweepResult.completed) {
//		logger->LogInfo("✓ Sweep completed with " +
//			std::to_string(sweepResult.measurements.size()) + " points");
//		psManager->StoreSweepResults("PS1", "test_sweep");
//	}
//
//	// Query stored results
//	IResultStorage::QueryFilter filter;
//	filter.deviceType = "PowerSupply";
//	filter.maxResults = 10;
//
//	auto results = psManager->QueryStoredResults(filter);
//	logger->LogInfo("✓ Found " + std::to_string(results.size()) + " stored records");
//
//	// Cleanup
//	psManager->TurnOffAll();
//	psManager->DisconnectAllDevices();
//
//	logger->LogInfo("=== TEST COMPLETE ===");
//}


// Call this function after your app starts up to test the flow
// You can add a button in your UI to trigger it, or call it from main() after initialization

int main(int argc, char* argv[])
{
	// Get the logger instance
	Logger* logger = Logger::GetInstance();
	logger->LogInfo("Hello World from uaa3App!");

	GlobalDataStore* dataStore = GlobalDataStore::GetInstance();

	// ===========================================
		// PHASE 1: INITIALIZATION WITH SPLASH SCREEN
		// ===========================================

		// Create splash screen
	SimpleSplashScreen splash;
	bool splashAvailable = splash.Initialize();

	// Create initializer
	ApplicationInitializer initializer(logger);

	// Set up progress callback
	if (splashAvailable) {
		initializer.SetProgressCallback([&splash](const ApplicationInitializer::InitProgress& progress) {
			splash.UpdateProgress(progress);
		});
	}
	else {
		// Fallback to console
		initializer.SetProgressCallback([](const ApplicationInitializer::InitProgress& progress) {
			ConsoleProgressDisplay::DisplayProgress(progress);
		});
	}

	// Create containers for hardware and operations
	ApplicationInitializer::HardwareManagers hardware;
	ApplicationInitializer::Operations operations;

	// Prepare initialization steps
	initializer.PrepareInitializationSteps(hardware, operations);

	// Execute initialization
	if (!initializer.ExecuteInitialization(hardware, operations)) {
		logger->LogError("Initialization failed - see log for details");

		if (splashAvailable) {
			// Show error on splash for 3 seconds
			SDL_Delay(3000);
		}
		splash.Shutdown();

		// Print detailed report
		std::cout << "\n" << initializer.GetInitializationReport() << std::endl;
		return -1;
	}

	// Close splash screen
	splash.Shutdown();

	logger->LogInfo("All systems initialized successfully!");

	// ===========================================
	// PHASE 2: SDL/IMGUI SETUP
	// ===========================================

	// Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
	{
		logger->LogError("SDL Initialization failed: " + std::string(SDL_GetError()));
		return -1;
	}
	// ADD THIS LINE HERE - after SDL_Init but before window creation
	SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "system");

	// Setup SDL window
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	SDL_Window* window = SDL_CreateWindow(Version::getWindowTitle().c_str(),
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		1200, 800, window_flags);

	if (window == nullptr)
	{
		logger->LogError("Error creating window: " + std::string(SDL_GetError()));
		SDL_Quit();
		return -1;
	}

	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, gl_context);
	SDL_GL_SetSwapInterval(1); // Enable vsync

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	// Initialize ImPlot context
	ImPlot::CreateContext();

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

#pragma region ImGui FreeType and Emoji Support
	// [KEEP YOUR ENTIRE FONT LOADING SECTION AS-IS]
#ifdef IMGUI_ENABLE_FREETYPE
	// Enable FreeType
	io.Fonts->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType();
	io.Fonts->FontBuilderFlags = ImGuiFreeTypeBuilderFlags_LightHinting;
	std::cout << "✅ FreeType enabled" << std::endl;
#else
	std::cout << "❌ FreeType disabled" << std::endl;
#endif

	// Simple font loading
	ImFont* mainFont = nullptr;
	bool emojiLoaded = false;

	// Load main font (try project font, fallback to default)
	if (std::filesystem::exists("assets/fonts/NotoSans-Regular.ttf")) {
		mainFont = io.Fonts->AddFontFromFileTTF("assets/fonts/NotoSans-Regular.ttf", 20.0f);
		std::cout << "✅ Loaded NotoSans-Regular" << std::endl;
	}
	else {
		mainFont = io.Fonts->AddFontDefault();
		std::cout << "📝 Using default font" << std::endl;
	}

	// Try to merge emoji font
	static ImFontConfig emojiConfig;
	emojiConfig.MergeMode = true;
#ifdef IMGUI_ENABLE_FREETYPE
	emojiConfig.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
#endif

	// UPDATED: Extended emoji ranges for better coverage
	static const ImWchar extended_emoji_ranges[] = {
		// Basic emoji blocks
		0x1F600, 0x1F64F, // Emoticons
		0x1F300, 0x1F5FF, // Misc Symbols and Pictographs  
		0x1F680, 0x1F6FF, // Transport and Map
		0x1F700, 0x1F77F, // Alchemical Symbols
		0x1F780, 0x1F7FF, // Geometric Shapes Extended
		0x1F800, 0x1F8FF, // Supplemental Arrows-C
		0x1F900, 0x1F9FF, // Supplemental Symbols and Pictographs
		0x1FA00, 0x1FA6F, // Chess Symbols  
		0x1FA70, 0x1FAFF, // Symbols and Pictographs Extended-A

		// Additional useful ranges
		0x2600, 0x26FF,   // Miscellaneous Symbols
		0x2700, 0x27BF,   // Dingbats
		0x231A, 0x231B,   // Watch symbols
		0x2764, 0x2764,   // Heavy black heart
		0x2049, 0x2049,   // Exclamation question mark
		0x203C, 0x203C,   // Double exclamation mark

		// Enclosed alphanumerics (for circled numbers like ①②③④⑤)
		0x2460, 0x24FF,   // Enclosed Alphanumerics
		0x1F100, 0x1F1FF, // Enclosed Alphanumeric Supplement

		// Technical symbols
		0x2300, 0x23FF,   // Miscellaneous Technical
		0x25A0, 0x25FF,   // Geometric Shapes

		// Arrows and symbols
		0x2190, 0x21FF,   // Arrows
		0x2200, 0x22FF,   // Mathematical Operators

		// Variation selectors (important for emoji presentation)
		0xFE00, 0xFE0F,   // Variation Selectors
		0xE0100, 0xE01EF, // Variation Selectors Supplement

		// Zero-width joiner and other combining characters
		0x200D, 0x200D,   // Zero Width Joiner (for emoji sequences)
		0x20E3, 0x20E3,   // Combining Enclosing Keycap (for number emojis like 1️⃣)

		0, // Terminator
	};

	// Try multiple emoji sources (Windows font first - no SVG issues)
	const char* emojiSources[] = {
		"C:/Windows/Fonts/seguiemj.ttf"        // Windows emoji (bitmap - no SVG)
		//,"assets/fonts/NotoColorEmoji.ttf"       // Google emoji (SVG - might fail)
	};

	for (const char* emojiPath : emojiSources) {
		if (std::filesystem::exists(emojiPath)) {
			// Use the extended emoji ranges instead of the wide range
			ImFont* emoji = io.Fonts->AddFontFromFileTTF(emojiPath, 20.0f, &emojiConfig, extended_emoji_ranges);
			if (emoji) {
				std::cout << "✅ Loaded emoji with extended ranges: " << emojiPath << std::endl;
				emojiLoaded = true;
				break;
			}
		}
	}

	if (!emojiLoaded) {
		std::cout << "⚠️ No emoji font loaded" << std::endl;

		// Fallback: Try loading with basic ranges only
		std::cout << "🔄 Attempting fallback emoji loading..." << std::endl;

		static const ImWchar basic_emoji_ranges[] = {
			0x1F600, 0x1F64F, // Emoticons only
			0x2600, 0x26FF,   // Miscellaneous Symbols
			0x2460, 0x24FF,   // Enclosed Alphanumerics (①②③④⑤)
			0,
		};

		for (const char* emojiPath : emojiSources) {
			if (std::filesystem::exists(emojiPath)) {
				ImFont* emoji = io.Fonts->AddFontFromFileTTF(emojiPath, 16.0f, &emojiConfig, basic_emoji_ranges);
				if (emoji) {
					std::cout << "✅ Loaded emoji with basic ranges: " << emojiPath << std::endl;
					emojiLoaded = true;
					break;
				}
			}
		}
	}

	// Build fonts
	if (io.Fonts->Build()) {
		std::cout << "✅ Font atlas built successfully" << std::endl;
		std::cout << "   Total fonts: " << io.Fonts->Fonts.Size << std::endl;

		// Debug: Show glyph counts
		for (int i = 0; i < io.Fonts->Fonts.Size; i++) {
			ImFont* font = io.Fonts->Fonts[i];
			std::cout << "   Font " << i << ": " << font->Glyphs.Size << " glyphs, "
				<< font->FontSize << "px" << std::endl;
		}

		if (emojiLoaded) {
			std::cout << "🎉 Emoji support enabled with extended character ranges" << std::endl;
		}

		// FIXED: Always set mainFont as default, regardless of emoji status
		if (mainFont) {
			io.FontDefault = mainFont;  // This makes it the default for everything
			if (emojiLoaded) {
				std::cout << "✅ Set emoji-enabled font as default application font" << std::endl;
			}
			else {
				std::cout << "✅ Set main font as default application font (no emojis)" << std::endl;
			}
		}
		else {
			std::cout << "⚠️ No main font loaded, using ImGui default" << std::endl;
		}
	}
	else {
		std::cout << "❌ Failed to build font atlas" << std::endl;
	}

	// Set the emoji-enabled font as the default font for the entire application
	if (mainFont && emojiLoaded) {
		// Make the merged font (main + emoji) the default
		io.FontDefault = mainFont;  // This makes it the default for everything
		std::cout << "✅ Set emoji-enabled font as default application font" << std::endl;
	}
#pragma endregion

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init("#version 130");

	// ===========================================
	// PHASE 3: CREATE UI COMPONENTS
	// ===========================================

	// Get AppContext (already populated by ApplicationInitializer)
	auto& context = AppContext::GetInstance();

	// CREATE MainUIManager with AppContext
	MainUIManager uiManager(context);
	// Store the font reference in MainUIManager
	if (mainFont) {
		uiManager.SetImguiFont(mainFont);  // This stores it in m_imguiFont
	}
	logger->LogInfo("MainUIManager created with AppContext approach");

	// Connect UserPromptUI to RunPageUI
	// Verify it worked
	if (auto* runPageUI = uiManager.GetRunPageUI()) {
		std::cout << "✓ RunPageUI accessible - UAA3 sequences enabled!" << std::endl;
	}
	else {
		std::cout << "✗ RunPageUI not accessible - check setup" << std::endl;
	}






	// Connect watchdog to UI manager (if available)
	if (hardware.configWatchdog) {
		uiManager.SetConfigWatchdog(hardware.configWatchdog.get());
		logger->LogInfo("Config watchdog connected to MainUIManager");
	}

	// Create IDS Camera UI
	std::unique_ptr<IDSCameraUI> idsCameraUI;
	idsCameraUI = std::make_unique<IDSCameraUI>();
	logger->LogInfo("IDS Camera UI initialized");

	// Create the raylib camera manager using AppContext
	std::unique_ptr<RaylibCameraManager> raylibManager;
	raylibManager = std::make_unique<RaylibCameraManager>(context, logger);

	// Set data store if available
	if (dataStore) {
		raylibManager->SetDataStore(dataStore);
	}

	// Configure initialization options
	RaylibCameraManager::InitializationOptions raylibOptions;
	raylibOptions.enableRaylib3D = true;           // Set to false to disable raylib
	raylibOptions.autoConnectCamera = false;        // Auto-connect first available camera
	raylibOptions.enableDebugWindow = true;        // Enable debug UI
	raylibOptions.cameraConnectionTimeout = 500;   // Connection timeout in ms

	// Initialize the manager
	if (raylibManager->Initialize(raylibOptions)) {
		logger->LogInfo("✅ Raylib and camera integration initialized successfully");
	}
	else {
		logger->LogError("❌ Failed to initialize raylib integration: " + raylibManager->GetLastError());
		// Continue without raylib (graceful degradation)
		raylibManager.reset();
	}

	// Create menu manager 
	std::unique_ptr<MenuManagerUaa3> menuManager = std::make_unique<MenuManagerUaa3>();

	// Set up menu callbacks
	bool done = false;
	menuManager->SetOnExitCallback([&done]() {
		done = true;
	});

	if (menuManager && idsCameraUI) {
		menuManager->SetIDSCameraUI(idsCameraUI.get());
		logger->LogInfo("IDS Camera UI connected to menu system");
	}

	// ===========================================
// PHASE 3.5: CREATE GRID SCANNER FOR TESTING
// ===========================================

// Create GridScanner manager - always shows UI
	std::unique_ptr<GridScannerManager> gridScannerManager;
	gridScannerManager = std::make_unique<GridScannerManager>();

	// Get managers from AppContext
	auto* piManager = AppContext::GetInstance().GetPIController();
	auto* dataClient = AppContext::GetInstance().GetDataClient();

	// Set the managers (can be null, UI will still show)
	if (piManager) {
		gridScannerManager->SetPIManager(piManager);
		logger->LogInfo("GridScanner: PI manager set");
	}

	if (dataClient) {
		gridScannerManager->SetDataClient(dataClient);
		logger->LogInfo("GridScanner: Data client set");
	}

	// Show the UI
	//gridScannerManager->Show();
	logger->LogInfo("GridScanner UI created (hardware will connect when available)");

	std::unique_ptr<GridVolumeScannerManager> volumeScannerManager;
	volumeScannerManager = std::make_unique<GridVolumeScannerManager>();

	// Set managers
	if (piManager) {
		volumeScannerManager->SetPIManager(piManager);
		logger->LogInfo("Volume Scanner: PI manager set");
	}

	if (dataClient) {
		volumeScannerManager->SetDataClient(dataClient);
		logger->LogInfo("Volume Scanner: Data client set");
	}

	// Show UI
	//volumeScannerManager->Show();
	logger->LogInfo("GridVolumeScanner UI created (hardware will connect when available)");

	// In your initialization code:
	if (gridScannerManager && gridScannerManager->GetUI()) {
		menuManager->RegisterUI("grid_scanner", gridScannerManager->GetUI(), "Scanners");
	}

	if (volumeScannerManager && volumeScannerManager->GetUI()) {
		menuManager->RegisterUI("volume_scanner", volumeScannerManager->GetUI(), "Scanners");
	}



	// After creating the VisionCameraExposureManager and UI
	auto exposureManager = std::make_unique<VisionCameraExposureManager>();
	exposureManager->SetLogger(logger);
	exposureManager->Initialize("camera_exposure_config.json");

	// Create the UI
	auto visionExposureUI = std::make_unique<VisionCameraExposureUI>();
	visionExposureUI->SetExposureManager(exposureManager.get());

	auto* cameraManager = context.GetCameraManager();
	visionExposureUI->SetCameraManager(cameraManager);
	auto* machineOperations = context.GetMachineOperations();
	visionExposureUI->SetMachineOperations(machineOperations);

	// Try this registration format
	if (menuManager && visionExposureUI) {
		menuManager->RegisterUI("vision_exposure", visionExposureUI.get(), "Vision");
		logger->LogInfo("Registered Vision Camera Exposure UI with menu system");
	}

	// Create the viewer UI
	auto dutViewerUI = std::make_unique<DUTDatabaseViewerUI>();

	// Configure it
	dutViewerUI->SetLogger(logger);
	dutViewerUI->SetDatabasePath("db/dut_db.db");  // Uses your default path

	// Register with menu manager
	if (menuManager && dutViewerUI) {
		menuManager->RegisterUI("dut_viewer", dutViewerUI.get(), "Data");
		logger->LogInfo("Registered DUT Database Viewer UI with menu system");
	}


	auto systemStatusUI = std::make_unique<SystemStatusUI>();
	menuManager->RegisterUI("system_status", systemStatusUI.get(), "System");
	logger->LogInfo("Registered System Status UI with menu system");

	// After creating other UI components:
	auto psTestUI = std::make_unique<PowerSupplyTestUI>();

	// Register with menu (if your menu supports it):
	if (menuManager) {
		menuManager->RegisterUI("ps_test", psTestUI.get(), "Testing");
	}



	// ===========================================
	// PHASE 4: MAIN RENDER LOOP
	// ===========================================
	logger->LogInfo("=== STARTING RENDER LOOP ===");
	while (!done)
	{
		// Poll and handle events
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT)
				done = true;
			if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
				event.window.windowID == SDL_GetWindowID(window))
				done = true;

			// Process keyboard events for UIJogWindow
			if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
				// Add debug output to see if keys are being detected
				const char* keyName = SDL_GetKeyName(event.key.keysym.sym);
				const char* eventType = (event.type == SDL_KEYDOWN) ? "DOWN" : "UP";

				// Only log common movement keys to avoid spam
				if (event.key.keysym.sym == SDLK_w || event.key.keysym.sym == SDLK_a ||
					event.key.keysym.sym == SDLK_s || event.key.keysym.sym == SDLK_d ||
					event.key.keysym.sym == SDLK_q || event.key.keysym.sym == SDLK_e ||
					event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_DOWN ||
					event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_RIGHT) {

					logger->LogInfo("Keyboard: " + std::string(keyName) + " " + std::string(eventType));
				}

				// Forward keyboard events to UIJogWindow through MainUIManager
				uiManager.ProcessKeyInput(event.key.keysym.sym, event.type == SDL_KEYDOWN);
			}
		}

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		// RENDER MENU BAR FIRST - BEFORE ANYTHING ELSE
		menuManager->RenderMainMenuBar();

		// Update DataClientManager continuously in background
		if (hardware.dataClient) {
			hardware.dataClient->UpdateClients();
		}

		// Render the main UI
		uiManager.RenderUI();


		// In render loop
		if (gridScannerManager) {
			// Check for hardware connections periodically
			static auto lastCheck = std::chrono::steady_clock::now();
			auto now = std::chrono::steady_clock::now();
			if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCheck).count() >= 2) {
				gridScannerManager->UpdateHardwareConnections();
				lastCheck = now;
			}

			// Always render the UI
			gridScannerManager->Render();
		}

		if (volumeScannerManager) {
			// Periodic hardware check
			static auto lastCheckVolume = std::chrono::steady_clock::now();
			auto now = std::chrono::steady_clock::now();
			if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCheckVolume).count() >= 2) {
				volumeScannerManager->UpdateHardwareConnections();
				lastCheckVolume = now;
			}

			// Always render UI
			volumeScannerManager->Render();
		}


		if(visionExposureUI){
			visionExposureUI->Render();
		}

		if(dutViewerUI){
			dutViewerUI->Render();
		}

		// In render loop, add:
		if (systemStatusUI) {
			systemStatusUI->Render();
		}

		if (psTestUI) {
			psTestUI->Render();
		}




#pragma region rayLibwindow
		// Update raylib with current machine data
		if (raylibManager && raylibManager->IsRunning()) {
			// Check if raylib window was closed
			if (raylibManager->ShouldClose()) {
				logger->LogInfo("Raylib window closed by user");
				raylibManager->Shutdown();
				raylibManager.reset();
			}
		}

		// Render raylib debug UI if enabled
		if (raylibManager && menuManager) {
			raylibManager->RenderDebugUI(menuManager.get());
		}
#pragma endregion


		// Optional: Log watchdog stats periodically
		static auto lastWatchdogStats = std::chrono::steady_clock::now();
		auto now = std::chrono::steady_clock::now();
		auto timeSinceStats = std::chrono::duration_cast<std::chrono::minutes>(now - lastWatchdogStats);

		if (hardware.configWatchdog && timeSinceStats.count() >= 10) { // Every 10 minutes
			auto stats = hardware.configWatchdog->GetStatistics();
			if (stats["changes_detected"].get<int>() > 0) {
				logger->LogInfo("📊 Watchdog stats: " +
					std::to_string(stats["changes_detected"].get<int>()) + " changes, " +
					std::to_string(stats["database_updates"].get<int>()) + " DB updates");
			}
			lastWatchdogStats = now;
		}

		if (idsCameraUI) {
			idsCameraUI->Render();
		}

		// Rendering
		ImGui::Render();
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		SDL_GL_SwapWindow(window);
	}

	// ===========================================
	// PHASE 5: CLEANUP
	// ===========================================

	logger->LogInfo("=== SHUTTING DOWN ===");

	// Cleanup RaylibWindow
	if (raylibManager) {
		logger->LogInfo("Shutting down raylib integration...");
		raylibManager->Shutdown();
		raylibManager.reset();
	}




	// ===========================================
	// PHASE 5: CLEANUP
	// ===========================================

	logger->LogInfo("=== SHUTTING DOWN ===");

	// Cleanup RaylibWindow
	if (raylibManager) {
		logger->LogInfo("Shutting down raylib integration...");
		raylibManager->Shutdown();
		raylibManager.reset();
	}

	// Stop cameras
	if (hardware.camera) {
		hardware.camera->StopGrabbingAll();
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		// Get the camera IDs from the config manager
		if (hardware.cameraConfig) {
			auto enabledCameraIds = hardware.cameraConfig->GetEnabledCameraIds();

			// Disconnect all enabled cameras
			for (const auto& cameraId : enabledCameraIds) {
				if (hardware.camera->DisconnectCamera(cameraId)) {
					logger->LogInfo("Disconnected camera: " + cameraId);
				}
				else {
					logger->LogWarning("Failed to disconnect camera: " + cameraId);
				}
			}
		}
		else {
			// If no config manager, try to disconnect all cameras that might be connected
			auto cameraIds = hardware.camera->GetCameraIds();
			for (const auto& cameraId : cameraIds) {
				if (hardware.camera->DisconnectCamera(cameraId)) {
					logger->LogInfo("Disconnected camera: " + cameraId);
				}
				else {
					logger->LogWarning("Failed to disconnect camera: " + cameraId);
				}
			}
		}
	}

	// Disconnect motion controllers
	if (hardware.piController) {
		hardware.piController->DisconnectAll();
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	if (hardware.acsController) {
		hardware.acsController->DisconnectAll();
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	// Stop IO systems
	if (hardware.ioManager) {
		hardware.ioManager->stopPolling();
		hardware.ioManager->disconnectAll();
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	if (hardware.pneumatic) {
		hardware.pneumatic->stopPolling();
	}

	// Cleanup instruments
	if (hardware.laser) {
		hardware.laser->DisconnectAll();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}



	// Cleanup ImGui
	ImPlot::DestroyContext();
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	// Cleanup SDL
	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	logger->LogInfo("uaa3App finished!");
	return 0;
}