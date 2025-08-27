#include <iostream>
#include <SDL.h>
#include <SDL_opengl.h>

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
// Keep your debug function as-is
bool g_deugMode = false; // Global debug mode flag





// Add this function to test SPD -> GlobalDataStore flow manually
void TestSPDToGlobalDataStore() {
	Logger* logger = Logger::GetInstance();
	logger->LogInfo("=== MANUAL SPD TEST START ===");

	// Get managers
	AppContext& context = AppContext::GetInstance();
	auto* spdManager = context.GetSPDPowerSupply();
	GlobalDataStore* dataStore = GlobalDataStore::GetInstance();

	if (!spdManager) {
		logger->LogError("SPD Manager not available");
		return;
	}

	if (!dataStore) {
		logger->LogError("GlobalDataStore not available");
		return;
	}

	// Check subscription
	bool subscribed = dataStore->IsSPDSubscribed();
	logger->LogInfo("GlobalDataStore SPD subscription: " + std::string(subscribed ? "ACTIVE" : "INACTIVE"));

	// 1. Connect devices
	logger->LogInfo("1. Connecting SPD devices...");
	int connected = spdManager->ConnectAll();
	logger->LogInfo("   Connected: " + std::to_string(connected) + " devices");

	if (connected == 0) {
		logger->LogWarning("No devices connected - test aborted");
		return;
	}

	// 2. Set CV mode: 3.3V, 0.5A limit
	logger->LogInfo("2. Setting CV mode: 3.3V, 0.5A limit...");
	bool cvSet = spdManager->SetConstantVoltageMode(3.3, 0.5);
	logger->LogInfo("   CV mode result: " + std::string(cvSet ? "SUCCESS" : "FAILED"));

	// 3. Turn on outputs
	logger->LogInfo("3. Turning on outputs...");
	bool outputsOn = spdManager->SetAllOutputs(true);
	logger->LogInfo("   Outputs enabled: " + std::string(outputsOn ? "SUCCESS" : "FAILED"));

	// 4. Set up callback that updates GlobalDataStore (this will override any existing callback)
	logger->LogInfo("4. Setting up direct callback...");
	spdManager->SetStatusUpdateCallback(
		[dataStore, logger](const std::string& deviceName, const std::string& status) {
			static int callbackCount = 0;
			callbackCount++;

			if (callbackCount <= 5) {  // Log first 5 callbacks
				std::cout << "[SPD CALLBACK #" << callbackCount << "] Device: " << deviceName
					<< " Status: " << status << std::endl;
			}

			// Parse and store in GlobalDataStore
			if (status.find("Status read failed") == std::string::npos &&
				status.find("Disconnected") == std::string::npos) {

				float voltage = 0.0f, current = 0.0f;
				bool outputState = false;

				// Parse voltage: "V: X.XXXV"
				size_t vPos = status.find("V: ");
				if (vPos != std::string::npos) {
					size_t vEnd = status.find("V", vPos + 3);
					if (vEnd != std::string::npos) {
						try {
							voltage = std::stof(status.substr(vPos + 3, vEnd - vPos - 3));
						}
						catch (...) {
							std::cout << "[PARSE ERROR] Failed to parse voltage from: " << status << std::endl;
						}
					}
				}

				// Parse current: "I: X.XXXA"  
				size_t iPos = status.find("I: ");
				if (iPos != std::string::npos) {
					size_t iEnd = status.find("A", iPos + 3);
					if (iEnd != std::string::npos) {
						try {
							current = std::stof(status.substr(iPos + 3, iEnd - iPos - 3));
						}
						catch (...) {
							std::cout << "[PARSE ERROR] Failed to parse current from: " << status << std::endl;
						}
					}
				}

				// Parse output state
				outputState = status.find("Output: ON") != std::string::npos;

				// Update GlobalDataStore
				std::string voltageChannel = "SPD-" + deviceName + "-Voltage";
				std::string currentChannel = "SPD-" + deviceName + "-Current";
				std::string outputChannel = "SPD-" + deviceName + "-Output";
				std::string powerChannel = "SPD-" + deviceName + "-Power";

				dataStore->SetValue(voltageChannel, voltage);
				dataStore->SetValue(currentChannel, current);
				dataStore->SetValue(outputChannel, outputState ? 1.0f : 0.0f);
				dataStore->SetValue(powerChannel, voltage * current);

				if (callbackCount <= 3) {
					std::cout << "[GLOBALDATA UPDATE] Created channels: V=" << voltage
						<< "V, I=" << current << "A, Power=" << (voltage * current)
						<< "W, Output=" << (outputState ? "ON" : "OFF") << std::endl;
				}
			}
		}
	);

	// 5. Start polling
	logger->LogInfo("5. Starting polling (1000ms interval)...");
	spdManager->StartAllPolling(1000);

	// 6. Wait and check data
	logger->LogInfo("6. Waiting 3 seconds for data...");
	std::this_thread::sleep_for(std::chrono::seconds(3));

	// Check GlobalDataStore channels
	auto channels = dataStore->GetAvailableChannels();
	int spdChannelCount = 0;

	logger->LogInfo("7. Checking GlobalDataStore channels:");
	logger->LogInfo("   Total channels: " + std::to_string(channels.size()));

	for (const auto& ch : channels) {
		if (ch.find("SPD-") == 0) {
			spdChannelCount++;
			float value = dataStore->GetValue(ch);
			logger->LogInfo("   FOUND: " + ch + " = " + std::to_string(value));
		}
	}

	if (spdChannelCount > 0) {
		logger->LogInfo("   SUCCESS: " + std::to_string(spdChannelCount) + " SPD channels active!");
	}
	else {
		logger->LogError("   FAILED: No SPD channels found!");
		logger->LogInfo("   All available channels:");
		for (const auto& ch : channels) {
			logger->LogInfo("     - " + ch);
		}
	}

	// 8. Stop polling  
	logger->LogInfo("8. Stopping polling...");
	spdManager->StopAllPolling();

	// 9. Turn off outputs
	logger->LogInfo("9. Turning off outputs...");
	spdManager->SetAllOutputs(false);

	// 10. Final report
	auto finalChannels = dataStore->GetAvailableChannels();
	int finalSpdCount = 0;

	logger->LogInfo("10. Final channel values (outputs OFF, devices connected):");
	for (const auto& ch : finalChannels) {
		if (ch.find("SPD-") == 0) {
			finalSpdCount++;
			float value = dataStore->GetValue(ch);
			logger->LogInfo("    " + ch + ": " + std::to_string(value));
		}
	}

	logger->LogInfo("=== TEST COMPLETE ===");
	logger->LogInfo("Final SPD channels in GlobalDataStore: " + std::to_string(finalSpdCount));
}

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
        } else {
            std::cout << "✅ Set main font as default application font (no emojis)" << std::endl;
        }
    } else {
        std::cout << "⚠️ No main font loaded, using ImGui default" << std::endl;
    }
} else {
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
	raylibOptions.autoConnectCamera = true;        // Auto-connect first available camera
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

	logger->LogInfo("=== STARTING RENDER LOOP ===");






	//test PSD
	
	//TestSPDToGlobalDataStore();


	// ===========================================
	// PHASE 4: MAIN RENDER LOOP
	// ===========================================

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

#pragma region DebugMode
		////debug mode smaller ops
		//if (g_deugMode) {
		//	// NEW: Add test window for MotionOps
		//	static bool showMotionTest = true;
		//	if (showMotionTest && operations.motion) {
		//		ImGui::Begin("MotionOps Test", &showMotionTest);

		//		ImGui::Text("Gantry Movement Test");
		//		ImGui::Separator();

		//		// Test buttons for gantry movement
		//		if (ImGui::Button("Move X +1mm")) {
		//			bool success = operations.motion->MoveRelative("gantry-main", "X", 1.0, true, "MainLoop_Test");
		//			if (success) {
		//				logger->LogInfo("Successfully moved gantry X +1mm");
		//			}
		//			else {
		//				logger->LogError("Failed to move gantry X +1mm");
		//			}
		//		}

		//		ImGui::SameLine();
		//		if (ImGui::Button("Move X -1mm")) {
		//			bool success = operations.motion->MoveRelative("gantry-main", "X", -1.0, true, "MainLoop_Test");
		//			if (success) {
		//				logger->LogInfo("Successfully moved gantry X -1mm");
		//			}
		//			else {
		//				logger->LogError("Failed to move gantry X -1mm");
		//			}
		//		}

		//		if (ImGui::Button("Move Y +1mm")) {
		//			bool success = operations.motion->MoveRelative("gantry-main", "Y", 1.0, true, "MainLoop_Test");
		//			if (success) {
		//				logger->LogInfo("Successfully moved gantry Y +1mm");
		//			}
		//			else {
		//				logger->LogError("Failed to move gantry Y +1mm");
		//			}
		//		}

		//		ImGui::SameLine();
		//		if (ImGui::Button("Move Y -1mm")) {
		//			bool success = operations.motion->MoveRelative("gantry-main", "Y", -1.0, true, "MainLoop_Test");
		//			if (success) {
		//				logger->LogInfo("Successfully moved gantry Y -1mm");
		//			}
		//			else {
		//				logger->LogError("Failed to move gantry Y -1mm");
		//			}
		//		}

		//		if (ImGui::Button("Move Z +1mm")) {
		//			bool success = operations.motion->MoveRelative("gantry-main", "Z", 1.0, true, "MainLoop_Test");
		//			if (success) {
		//				logger->LogInfo("Successfully moved gantry Z +1mm");
		//			}
		//			else {
		//				logger->LogError("Failed to move gantry Z +1mm");
		//			}
		//		}

		//		ImGui::SameLine();
		//		if (ImGui::Button("Move Z -1mm")) {
		//			bool success = operations.motion->MoveRelative("gantry-main", "Z", -1.0, true, "MainLoop_Test");
		//			if (success) {
		//				logger->LogInfo("Successfully moved gantry Z -1mm");
		//			}
		//			else {
		//				logger->LogError("Failed to move gantry Z -1mm");
		//			}
		//		}

		//		ImGui::Separator();
		//		ImGui::Text("Device Status:");

		//		if (operations.motion->IsDeviceConnected("gantry-main")) {
		//			ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ Gantry Connected");
		//		}
		//		else {
		//			ImGui::TextColored(ImVec4(1, 0, 0, 1), "✗ Gantry Disconnected");
		//		}

		//		if (operations.motion->IsDeviceMoving("gantry-main")) {
		//			ImGui::TextColored(ImVec4(1, 1, 0, 1), "⚠ Gantry Moving");
		//		}
		//		else {
		//			ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ Gantry Idle");
		//		}

		//		// Show current position if available
		//		PositionStruct currentPos;
		//		if (operations.motion->GetDeviceCurrentPosition("gantry-main", currentPos)) {
		//			ImGui::Text("Current Position:");
		//			ImGui::Text("  X: %.3f mm", currentPos.x);
		//			ImGui::Text("  Y: %.3f mm", currentPos.y);
		//			ImGui::Text("  Z: %.3f mm", currentPos.z);
		//		}

		//		ImGui::End();
		//	}
		//}
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

	if (hardware.keithley) {
		hardware.keithley->DisconnectAll();
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