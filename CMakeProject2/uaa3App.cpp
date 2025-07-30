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

// Include our UI manager and config manager
#include "MainUIManager.h"
#include "include/motions/MotionConfigManager.h"
#include "include/motions/pi_controller_manager.h"
#include "include/motions/pi_controller.h"
#include "include/motions/acs_controller_manager.h"
#include "include/motions/acs_controller.h"
#include "include/logger.h"
#include "include/eziio/EziIO_Manager.h"
#include "include/eziio/PneumaticManager.h"
#include "IOConfigManager.h"
#include "include/camera/CameraManager.h"
#include "include/data/global_data_store.h"
#include "include/data/data_client_manager.h"

// SMU includes
#include "include/SMU/keithley2400_client.h"
#include "include/SMU/keithley2400_manager.h"
#include "include/SMU/keithley2400_operations.h"

// Laser includes
#include "include/cld101x_manager.h"  
#include "include/cld101x_client.h"
#include "include/cld101x_operations.h"

// Motion and machine operations
#include "include/machine_operations.h"
#include "include/motions/motion_control_layer.h"

// NEW: Add the split operation classes
#include "include/ops/motion_ops.h"
#include "include/ops/io_ops.h"
#include "include/ops/vision_ops.h"
#include "Version.h"

#include "raylibclass.h"
#include "CameraFeedDisplay.h"
#include "MenuManager_uaa3.h"
#include "RaylibDebugWindow.h"
#include "LiveVideoSubscriber.h"
#include "CameraConfigManager.h"

#include "ConfigDatabaseUtils.h"
#include "ConfigFileWatchdog.h"



bool g_deugMode = false; // Global debug mode flag


// Example: Check camera status
void CheckCameraStatus(CameraManager& cameraManager) {
	auto statusList = cameraManager.GetAllCameraStatus();

	for (const auto& status : statusList) {
		std::cout << "Camera " << status.id << ":" << std::endl;
		std::cout << "  Connected: " << (status.connected ? "Yes" : "No") << std::endl;
		std::cout << "  Grabbing: " << (status.grabbing ? "Yes" : "No") << std::endl;
		std::cout << "  Exposure: " << status.currentExposure.exposure_time << "us" << std::endl;
		std::cout << "  Gain: " << status.currentExposure.gain << std::endl;
	}
}



// **NEW: Enhanced debug function for camera feed**
void DebugCameraFeedSetupEnhanced(CameraManager* cameraManager,
	CameraFeedDisplay* raylibCameraFeed,
	RaylibWindow* raylibWindow,
	Logger* logger) {

	logger->LogInfo("=== ENHANCED CAMERA FEED DEBUG ===");

	// Check camera manager
	if (!cameraManager) {
		logger->LogError("CameraManager is null");
		return;
	}

	logger->LogInfo("Camera count: " + std::to_string(cameraManager->GetCameraCount()));
	logger->LogInfo("Subscriber count: " + std::to_string(cameraManager->GetSubscriberCount()));
	logger->LogInfo("Broadcasting active: " + std::string(cameraManager->GetSubscriberCount() > 0 ? "YES" : "NO"));

	auto cameraIds = cameraManager->GetCameraIds();
	for (const auto& id : cameraIds) {
		auto status = cameraManager->GetCameraStatus(id);
		logger->LogInfo("Camera " + id + ":");
		logger->LogInfo("  Connected: " + std::string(status.connected ? "Yes" : "No"));
		logger->LogInfo("  Grabbing: " + std::string(status.grabbing ? "Yes" : "No"));
		logger->LogInfo("  Device: " + status.deviceInfo);
	}

	// Check camera feed display
	if (!raylibCameraFeed) {
		logger->LogError("RaylibCameraFeed is null");
		return;
	}

	logger->LogInfo("=== CAMERA FEED DISPLAY STATUS ===");
	logger->LogInfo("Feed has source: " + std::string(raylibCameraFeed->HasSource() ? "Yes" : "No"));
	logger->LogInfo("Feed has texture: " + std::string(raylibCameraFeed->HasValidTexture() ? "Yes" : "No"));
	logger->LogInfo("Feed status: " + raylibCameraFeed->GetStatusText());
	logger->LogInfo("Feed receiving frames: " + std::string(raylibCameraFeed->IsReceivingFrames() ? "Yes" : "No"));
	logger->LogInfo("Feed total frames: " + std::to_string(raylibCameraFeed->GetTotalFramesReceived()));
	logger->LogInfo("Feed frame rate: " + std::to_string(raylibCameraFeed->GetActualFrameRate()) + " fps");
	logger->LogInfo("Feed subscriber ID: " + raylibCameraFeed->GetSubscriberId());

	// Check raylib window
	if (!raylibWindow) {
		logger->LogError("RaylibWindow is null");
		return;
	}

	logger->LogInfo("=== RAYLIB WINDOW STATUS ===");
	logger->LogInfo("Raylib has camera feed: " + std::string(raylibWindow->HasCameraFeed() ? "Yes" : "No"));
	logger->LogInfo("Raylib feed visible: " + std::string(raylibWindow->IsCameraFeedVisible() ? "Yes" : "No"));

	logger->LogInfo("=== END ENHANCED DEBUG ===");
}


/**
 * @brief Initialize database with migration check
 */
bool InitializeDatabaseWithMigration(const std::string& databasePath, Logger* logger) {
	// Initialize database normally
	if (!ConfigDatabaseUtils::InitializeDatabase(databasePath, logger)) {
		return false;
	}

	UAA3::UAA3DatabaseManager& dbManager = UAA3::GetDatabaseManager();

	// Check if any tables need migration
	auto tables = dbManager.GetAllConfigTables();
	bool needsMigration = false;

	for (const auto& table : tables) {
		if (dbManager.IsOldFormatTable(table.tableName)) {
			needsMigration = true;
			break;
		}
	}

	if (needsMigration) {
		if (logger) {
			logger->LogInfo("🔄 Database migration needed - converting to simplified format");
		}

		// Create backup before migration
		UAA3::DatabaseResult backupResult = dbManager.CreateBackup();
		if (backupResult.success) {
			if (logger) {
				logger->LogInfo("✅ Backup created: " + backupResult.details);
			}
		}
		else {
			if (logger) {
				logger->LogWarning("⚠️ Failed to create backup: " + backupResult.errorMessage);
			}
		}

		// Migrate all tables
		auto migrationResults = dbManager.MigrateAllTablesToNewFormat();

		int successCount = 0;
		for (const auto& result : migrationResults) {
			if (result.success) {
				successCount++;
				if (logger) {
					logger->LogInfo("✅ " + result.details);
				}
			}
			else {
				if (logger) {
					logger->LogError("❌ " + result.errorMessage + " - " + result.details);
				}
			}
		}

		if (logger) {
			logger->LogInfo("🎉 Migration completed: " + std::to_string(successCount) +
				" out of " + std::to_string(migrationResults.size()) + " tables migrated");
		}
	}
	else {
		if (logger) {
			logger->LogInfo("✅ Database already using simplified JSON storage format");
		}
	}

	return true;
}

int main(int argc, char* argv[])
{
	// Get the logger instance
	Logger* logger = Logger::GetInstance();
	logger->LogInfo("Hello World from uaa3App!");

	GlobalDataStore* dataStore = GlobalDataStore::GetInstance();

#pragma region INITIALIZING DATABASE CONFIGURATION SYSTEM
	// ================================================================
	// ADD THIS ENTIRE SECTION - DATABASE CONFIGURATION SYSTEM
	// ================================================================

	// REPLACE the database initialization with this:
	logger->LogInfo("=== INITIALIZING DATABASE CONFIGURATION SYSTEM ===");

	// Initialize database with automatic migration
	bool databaseAvailable = InitializeDatabaseWithMigration("", logger);
	if (databaseAvailable) {
		logger->LogInfo("✅ Database configuration system ready (with migration check)");
	}
	else {
		logger->LogWarning("⚠️  Database unavailable - using traditional file-based configs");
	}

	// List of all configuration files to manage with database
	std::vector<std::string> configFiles = {
		"camera_calibration.json",
		"camera_exposure_config.json",
		"camera_to_object_offset.json",
		"data_display_config.json",
		"DataServerConfig.json",
		"io_panel_config.json",
		"IOConfig.json",
		"motion_config.json",
		"program.json",
		"script_runner_config.json",
		"smu_config.json",
		"toolbar_state.json",
		"transformation_matrix.json",
		"camera_config.json",
	};

	// SCAN PRESETS FOLDER DYNAMICALLY
	logger->LogInfo("Scanning presets folder for JSON files...");
	std::string presetsFolder = "presets";
	int presetFilesFound = 0;

	try {
		if (std::filesystem::exists(presetsFolder) && std::filesystem::is_directory(presetsFolder)) {
			for (const auto& entry : std::filesystem::directory_iterator(presetsFolder)) {
				if (entry.is_regular_file()) {
					std::string filename = entry.path().filename().string();
					std::string extension = entry.path().extension().string();

					// Convert extension to lowercase for comparison
					std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

					if (extension == ".json") {
						// Add relative path from presets folder
						std::string relativePath = entry.path().string();
						configFiles.push_back(relativePath);
						presetFilesFound++;
						logger->LogInfo("  📄 Found preset: " + filename);
					}
				}
			}
		}
		else {
			logger->LogWarning("⚠️ Presets folder not found: " + presetsFolder);
		}
	}
	catch (const std::filesystem::filesystem_error& e) {
		logger->LogError("❌ Error scanning presets folder: " + std::string(e.what()));
	}

	logger->LogInfo("Found " + std::to_string(presetFilesFound) + " preset files in /" + presetsFolder);

	// Load all configurations with database integration
	logger->LogInfo("Loading configurations with database integration...");
	std::vector<ConfigDatabaseUtils::ConfigLoadResult> configResults =
		ConfigDatabaseUtils::LoadMultipleConfigs(configFiles, logger);

	// Log summary of configuration loading
	int fromDatabase = 0, fromFile = 0, savedToDb = 0, failed = 0;
	for (const auto& result : configResults) {
		if (result.success) {
			if (result.loadedFromDatabase) fromDatabase++;
			if (result.source == "file") fromFile++;
			if (result.savedToDatabase) savedToDb++;
		}
		else {
			failed++;
		}
	}

	logger->LogInfo("Configuration loading summary:");
	logger->LogInfo("  📁 From database: " + std::to_string(fromDatabase));
	logger->LogInfo("  💾 From files: " + std::to_string(fromFile));
	logger->LogInfo("  💿 Saved to database: " + std::to_string(savedToDb));
	logger->LogInfo("  ❌ Failed: " + std::to_string(failed));

	// Log database statistics
	if (databaseAvailable) {
		ConfigDatabaseUtils::LogDatabaseStats(logger);
	}

	logger->LogInfo("=== DATABASE CONFIGURATION SYSTEM READY ===");
	logger->LogInfo("");


	// ================================================================
// CONFIGURATION FILE WATCHDOG SYSTEM  
// ================================================================

	logger->LogInfo("=== STARTING CONFIGURATION FILE WATCHDOG ===");

	// Create and configure the watchdog
	std::unique_ptr<ConfigFileWatchdog> configWatchdog = nullptr;

	if (databaseAvailable) {
		// Create watchdog with manual configuration using our complete config files list
		configWatchdog = std::make_unique<ConfigFileWatchdog>(1000, true, logger);

		// Add all configuration files from our complete list (includes presets)
		int addedCount = configWatchdog->AddFiles(configFiles);
		logger->LogInfo("Added " + std::to_string(addedCount) + " out of " +
			std::to_string(configFiles.size()) + " config files to watchdog");

		if (presetFilesFound > 0) {
			logger->LogInfo("  📂 Including " + std::to_string(presetFilesFound) + " preset files in watchdog");
		}

		// Add custom callback for file changes (optional)
		configWatchdog->AddChangeCallback([logger](const ConfigFileWatchdog::FileChangeEvent& event) {
			if (event.updateSuccess) {
				logger->LogInfo("🔄 Config file auto-synced to database: " + event.filename);
			}
			else if (!event.errorMessage.empty()) {
				logger->LogWarning("⚠️ Auto-sync failed for " + event.filename + ": " + event.errorMessage);
			}
			});

		// Start the watchdog
		if (configWatchdog->Start()) {
			logger->LogInfo("✅ Configuration file watchdog started");
			logger->LogInfo("📂 Monitoring " + std::to_string(configWatchdog->GetWatchedFiles().size()) + " config files");
		}
		else {
			logger->LogError("❌ Failed to start configuration file watchdog");
			configWatchdog.reset();
		}
	}
	else {
		logger->LogInfo("⏭️ Skipping watchdog - database not available");
	}

	logger->LogInfo("=== CONFIGURATION SYSTEM READY ===");
	logger->LogInfo("");

#pragma endregion




	// Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
	{
		logger->LogError("SDL Initialization failed: " + std::string(SDL_GetError()));
		return -1;
	}

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


//#pragma region Enhanced Font Loading with FreeType and Emoji Support
//
//	// Font loading code with comprehensive Unicode and emoji support
//	std::cout << "=== INITIALIZING FREETYPE FONT SYSTEM ===" << std::endl;
//
//#ifdef IMGUI_ENABLE_FREETYPE
//	// Enable FreeType font builder
//	io.Fonts->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType();
//	io.Fonts->FontBuilderFlags = ImGuiFreeTypeBuilderFlags_LightHinting;
//	std::cout << "✅ FreeType 2.13.3 builder enabled" << std::endl;
//#else
//	std::cout << "❌ FreeType not enabled - add #define IMGUI_ENABLE_FREETYPE to imconfig.h" << std::endl;
//#endif
//
//#ifdef IMGUI_USE_WCHAR32
//	std::cout << "✅ IMGUI_USE_WCHAR32 enabled - 32-bit Unicode support active" << std::endl;
//#else
//	std::cout << "❌ IMGUI_USE_WCHAR32 disabled - add #define IMGUI_USE_WCHAR32 to imconfig.h for emoji" << std::endl;
//#endif
//
//	// Comprehensive Unicode ranges for technical applications
//	static const ImWchar comprehensive_ranges[] = {
//		0x0020, 0x00FF, // Basic Latin + Latin Supplement  
//		0x0100, 0x017F, // Latin Extended-A
//		0x0180, 0x024F, // Latin Extended-B
//		0x0370, 0x03FF, // Greek and Coptic (includes μ at 0x03BC)
//		0x0400, 0x04FF, // Cyrillic
//		0x2000, 0x206F, // General Punctuation
//		0x2070, 0x209F, // Superscripts and Subscripts
//		0x20A0, 0x20CF, // Currency Symbols
//		0x2100, 0x214F, // Letterlike Symbols
//		0x2150, 0x218F, // Number Forms
//		0x2190, 0x21FF, // Arrows
//		0x2200, 0x22FF, // Mathematical Operators (comprehensive math symbols)
//		0x2300, 0x23FF, // Miscellaneous Technical
//		0x2460, 0x24FF, // Enclosed Alphanumerics
//		0x25A0, 0x25FF, // Geometric Shapes
//		0x2600, 0x26FF, // Miscellaneous Symbols
//		0x2700, 0x27BF, // Dingbats
//		0x231A, 0x231B, // Watch symbols
//		0x2764, 0x2764, // Heavy black heart
//		0,
//	};
//
//	// Emoji ranges for comprehensive emoji support
//	static const ImWchar emoji_ranges[] = {
//		0x1F600, 0x1F64F, // Emoticons
//		0x1F300, 0x1F5FF, // Misc Symbols and Pictographs
//		0x1F680, 0x1F6FF, // Transport and Map
//		0x1F700, 0x1F77F, // Alchemical Symbols
//		0x1F780, 0x1F7FF, // Geometric Shapes Extended
//		0x1F800, 0x1F8FF, // Supplemental Arrows-C
//		0x1F900, 0x1F9FF, // Supplemental Symbols and Pictographs
//		0x1FA00, 0x1FA6F, // Chess Symbols
//		0x1FA70, 0x1FAFF, // Symbols and Pictographs Extended-A
//		0x2600, 0x26FF,   // Miscellaneous Symbols (weather, zodiac)
//		0x2700, 0x27BF,   // Dingbats
//		0x231A, 0x231B,   // Watch symbols
//		0x2764, 0x2764,   // Heavy black heart
//		0,
//	};
//
//	ImFont* mainFont = nullptr;
//	bool fontLoaded = false;
//	bool emojiLoaded = false;
//
//	// Strategy 1: Try to load project fonts first
//	std::cout << "1. Trying project fonts..." << std::endl;
//
//	const char* projectFonts[] = {
//		"assets/fonts/NotoSans-Regular.ttf",
//		"assets/fonts/JuliaMono-Regular.ttf",
//		"assets/fonts/CascadiaCode-Regular.ttf"
//	};
//
//	for (const char* fontPath : projectFonts) {
//		if (std::filesystem::exists(fontPath)) {
//			mainFont = io.Fonts->AddFontFromFileTTF(fontPath, 16.0f, nullptr, comprehensive_ranges);
//			if (mainFont) {
//				std::cout << "✅ Loaded project font: " << fontPath << std::endl;
//				fontLoaded = true;
//				break;
//			}
//		}
//	}
//
//	// Strategy 2: Try system fonts if project fonts not found
//	if (!fontLoaded) {
//		std::cout << "2. Trying system fonts..." << std::endl;
//
//		const char* systemFonts[] = {
//			"C:/Windows/Fonts/segoeui.ttf",     // Segoe UI (excellent Unicode support)
//			"C:/Windows/Fonts/calibri.ttf",    // Calibri
//			"C:/Windows/Fonts/arial.ttf",      // Arial
//			"C:/Windows/Fonts/times.ttf",      // Times New Roman
//		};
//
//		for (const char* fontPath : systemFonts) {
//			if (std::filesystem::exists(fontPath)) {
//				mainFont = io.Fonts->AddFontFromFileTTF(fontPath, 16.0f, nullptr, comprehensive_ranges);
//				if (mainFont) {
//					std::cout << "✅ Loaded system font: " << fontPath << std::endl;
//					fontLoaded = true;
//					break;
//				}
//			}
//		}
//	}
//
//	// Strategy 3: Fallback to default font
//	if (!fontLoaded) {
//		std::cout << "3. Using ImGui default font..." << std::endl;
//		mainFont = io.Fonts->AddFontDefault();
//		fontLoaded = true;
//	}
//
//	// Strategy 4: Merge emoji font for emoji support
//	std::cout << "4. Loading emoji fonts..." << std::endl;
//
//	// Configure emoji font merge
//	static ImFontConfig emojiConfig;
//	emojiConfig.MergeMode = true;          // CRITICAL: Merge into existing font
//	emojiConfig.PixelSnapH = true;
//	// emojiConfig.GlyphExtraSpacing.x = 1.0f; // Not available in this ImGui version
//
//#ifdef IMGUI_ENABLE_FREETYPE
//	// Enable color emoji loading for FreeType
//	emojiConfig.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
//	std::cout << "✅ Color emoji loading enabled" << std::endl;
//#endif
//
//	// Try multiple emoji font sources
//	const char* emojiFonts[] = {
//		"C:/Windows/Fonts/seguiemj.ttf",           // Windows Segoe UI Emoji (bitmap)
//		"assets/fonts/NotoColorEmoji.ttf",         // Google Noto Color Emoji (SVG)
//		"assets/fonts/Apple Color Emoji.ttc",     // Apple Color Emoji (bitmap)
//		"C:/Windows/Fonts/tahoma.ttf"             // Fallback with some emoji
//	};
//
//	for (const char* emojiPath : emojiFonts) {
//		if (std::filesystem::exists(emojiPath)) {
//			std::cout << "Attempting to merge emoji font: " << emojiPath << std::endl;
//
//			try {
//				ImFont* merged = io.Fonts->AddFontFromFileTTF(emojiPath, 16.0f, &emojiConfig, emoji_ranges);
//				if (merged) {
//					std::cout << "✅ Successfully merged emoji font: " << emojiPath << std::endl;
//					emojiLoaded = true;
//					break;
//				}
//			}
//			catch (const std::exception& e) {
//				std::cout << "⚠️ Failed to load emoji font " << emojiPath << ": " << e.what() << std::endl;
//			}
//		}
//	}
//
//	if (!emojiLoaded) {
//		std::cout << "⚠️ No emoji fonts loaded - emoji may display as squares" << std::endl;
//	}
//
//	// Build font atlas
//	std::cout << "5. Building font atlas..." << std::endl;
//	bool buildSuccess = io.Fonts->Build();
//	if (buildSuccess) {
//		std::cout << "✅ Font atlas built successfully" << std::endl;
//		std::cout << "   Atlas size: " << io.Fonts->TexWidth << "x" << io.Fonts->TexHeight << std::endl;
//		std::cout << "   Total fonts: " << io.Fonts->Fonts.Size << std::endl;
//
//		// Show glyph counts for each font
//		for (int i = 0; i < io.Fonts->Fonts.Size; i++) {
//			ImFont* font = io.Fonts->Fonts[i];
//			std::cout << "   Font " << i << ": " << font->Glyphs.Size << " glyphs, "
//				<< font->FontSize << "px" << std::endl;
//		}
//	}
//	else {
//		std::cout << "❌ Failed to build font atlas" << std::endl;
//	}
//
//	// Test critical characters
//	std::cout << "6. Testing critical character availability..." << std::endl;
//
//	struct TestChar {
//		ImWchar codepoint;
//		const char* name;
//	};
//
//	TestChar testChars[] = {
//		{0x03BC, "μ (micro symbol)"},
//		{0x00B0, "° (degree)"},
//		{0x00B1, "± (plus-minus)"},
//		{0x2192, "→ (right arrow)"},
//		{0x231A, "⌚ (watch)"},
//		{0x1F60A, "😊 (smile)"},
//	};
//
//	int foundCount = 0;
//	for (const auto& testChar : testChars) {
//		bool found = false;
//		for (int i = 0; i < io.Fonts->Fonts.Size && !found; i++) {
//			const ImFontGlyph* glyph = io.Fonts->Fonts[i]->FindGlyph(testChar.codepoint);
//			if (glyph) found = true;
//		}
//		if (found) {
//			std::cout << "   ✅ " << testChar.name << std::endl;
//			foundCount++;
//		}
//		else {
//			std::cout << "   ❌ " << testChar.name << std::endl;
//		}
//	}
//
//	std::cout << "=== FONT SYSTEM INITIALIZATION COMPLETE ===" << std::endl;
//	std::cout << "Summary:" << std::endl;
//	std::cout << "  Main font: " << (fontLoaded ? "✅ Loaded" : "❌ Failed") << std::endl;
//	std::cout << "  Emoji support: " << (emojiLoaded ? "✅ Loaded" : "⚠️ Limited") << std::endl;
//	std::cout << "  Test characters: " << foundCount << "/" << (sizeof(testChars) / sizeof(testChars[0])) << " available" << std::endl;
//	std::cout << "" << std::endl;
//
//#pragma endregion

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();





#pragma region ImGui FreeType and Emoji Support


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
	if (std::filesystem::exists("assets/fonts/Roboto-Regular.ttf")) {
		mainFont = io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Regular.ttf", 16.0f);
		std::cout << "✅ Loaded Roboto-Regular" << std::endl;
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
			ImFont* emoji = io.Fonts->AddFontFromFileTTF(emojiPath, 16.0f, &emojiConfig, extended_emoji_ranges);
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



	// ✅ Create MotionConfigManager
	std::unique_ptr<MotionConfigManager> motionConfigManager;

	try {
		motionConfigManager = std::make_unique<MotionConfigManager>("motion_config.json");
		logger->LogInfo("MotionConfigManager created successfully");
	}
	catch (const std::exception& e) {
		logger->LogError("Failed to create MotionConfigManager: " + std::string(e.what()));
		logger->LogWarning("Will use default configuration");
		motionConfigManager = std::make_unique<MotionConfigManager>("default_config.json");
	}

	// PI Controller Manager
	std::unique_ptr<PIControllerManager> piControllerManager;
	piControllerManager = std::make_unique<PIControllerManager>(*motionConfigManager);
	if (piControllerManager->ConnectAll()) {
		logger->LogInfo("Successfully connected to all enabled PI controllers");
	}
	else {
		logger->LogWarning("Failed to connect to some PI controllers");
	}

	// ACS Controller Manager
	std::unique_ptr<ACSControllerManager> acsControllerManager;
	acsControllerManager = std::make_unique<ACSControllerManager>(*motionConfigManager);
	if (acsControllerManager->ConnectAll()) {
		logger->LogInfo("Successfully connected to all enabled ACS controllers");
	}
	else {
		logger->LogWarning("Failed to connect to some ACS controllers");
	}

	// Read velocity settings for PI controllers
	if (piControllerManager) {
		logger->LogInfo("Reading current velocity settings for PI controllers...");
		std::vector<std::string> piControllerNames = { "hex-left", "hex-right", "hex-bottom" };

		for (const std::string& controllerName : piControllerNames) {
			PIController* controller = piControllerManager->GetController(controllerName);

			if (controller && controller->IsConnected()) {
				logger->LogInfo("Reading velocities for controller: " + controllerName);
				double velocity = 0.0;
				if (controller->GetSystemVelocity(velocity)) {
					logger->LogInfo("  " + controllerName + "  velocity: " +
						std::to_string(velocity) + " mm/s");
				}
				else {
					logger->LogWarning("  " + controllerName + " velocity: Failed to read");
				}
			}
			else {
				logger->LogWarning("Controller " + controllerName + ": Not connected or not found");
			}
		}
	}

	// Read velocity settings for ACS controllers
	if (acsControllerManager) {
		logger->LogInfo("Reading current velocity settings for ACS controllers...");
		std::vector<std::string> acsControllerNames = { "gantry-main" };

		for (const std::string& controllerName : acsControllerNames) {
			ACSController* controller = acsControllerManager->GetController(controllerName);

			if (controller && controller->IsConnected()) {
				logger->LogInfo("Reading velocities for controller: " + controllerName);
				std::vector<std::string> axes = { "X", "Y", "Z" };

				for (const std::string& axis : axes) {
					double velocity = 0.0;
					if (controller->GetVelocity(axis, velocity)) {
						logger->LogInfo("  " + controllerName + " " + axis + " axis velocity: " +
							std::to_string(velocity) + " mm/s");
					}
					else {
						logger->LogWarning("  " + controllerName + " " + axis + " axis velocity: Failed to read");
					}
				}
			}
			else {
				logger->LogWarning("Controller " + controllerName + ": Not connected or not found");
			}
		}
	}

	// Initialize EziIO system
	std::unique_ptr<EziIOManager> ioManager;
	std::unique_ptr<IOConfigManager> ioconfigManager;
	ioManager = std::make_unique<EziIOManager>();
	ioconfigManager = std::make_unique<IOConfigManager>();

	if (!ioManager->initialize()) {
		logger->LogError("Failed to initialize EziIO manager");
	}
	else {
		ioconfigManager = std::make_unique<IOConfigManager>();
		if (!ioconfigManager->loadConfig("IOConfig.json")) {
			logger->LogWarning("Failed to load IO configuration, using default settings");
		}
		ioconfigManager->initializeIOManager(*ioManager);

		if (!ioManager->connectAll()) {
			logger->LogWarning("Failed to connect to all IO devices");
		}
		ioManager->startPolling(100);
		logger->LogInfo("EziIO system initialized");
	}

	// Initialize Pneumatic System
	std::unique_ptr<PneumaticManager> pneumaticManager;
	pneumaticManager = std::make_unique<PneumaticManager>(*ioManager);
	if (!pneumaticManager->initialize()) {
		logger->LogWarning("Failed to initialize Pneumatic Manager");
	}
	else {
		if (!ioconfigManager->initializePneumaticManager(*pneumaticManager)) {
			logger->LogWarning("Failed to initialize pneumatic manager");
		}

		pneumaticManager->startPolling(100);

		pneumaticManager->setStateChangeCallback([&logger](const std::string& slideName, SlideState state) {
			std::string stateStr;
			switch (state) {
			case SlideState::EXTENDED: stateStr = "Extended (Down)"; break;
			case SlideState::RETRACTED: stateStr = "Retracted (Up)"; break;
			case SlideState::MOVING: stateStr = "Moving"; break;
			case SlideState::P_ERROR: stateStr = "ERROR"; break;
			default: stateStr = "Unknown";
			}
			logger->LogInfo("Pneumatic slide '" + slideName + "' changed state to: " + stateStr);
			});

		logger->LogInfo("Pneumatic system initialized");
	}

	// Camera initialization with configuration manager
	std::unique_ptr<CameraManager> cameraManager = std::make_unique<CameraManager>();
	std::unique_ptr<CameraConfigManager> cameraConfigManager;

	// Create and initialize camera configuration manager
	try {
		cameraConfigManager = std::make_unique<CameraConfigManager>("camera_config.json");
		cameraConfigManager->SetLogger(logger);

		if (cameraConfigManager->LoadConfig()) {
			logger->LogInfo("Camera configuration loaded successfully");

			// Initialize CameraManager with the loaded configuration
			if (cameraConfigManager->InitializeCameraManager(*cameraManager)) {
				logger->LogInfo("CameraManager initialized from configuration");

				// Log summary of loaded cameras
				auto enabledCameras = cameraConfigManager->GetEnabledCameraIds();
				logger->LogInfo("Enabled cameras from config: " + std::to_string(enabledCameras.size()));
				for (const auto& cameraId : enabledCameras) {
					logger->LogInfo("  - " + cameraId);
				}
			}
			else {
				logger->LogWarning("Failed to initialize CameraManager from configuration");
			}
		}
		else {
			logger->LogWarning("Failed to load camera configuration, using fallback setup");

			// Fallback to original hardcoded setup
			auto camera1 = CameraInfo::CreateByIP("main_camera", "192.168.0.68", "Top view camera");
			cameraManager->AddCamera(camera1);
			auto camera2 = CameraInfo::CreateByIP("aux_camera", "192.168.0.69", "Auxilary Camera");
			cameraManager->AddCamera(camera2);

			cameraManager->InitializeAllCameras();
			logger->LogInfo("Cameras initialized with fallback configuration");
		}

		// Check camera status regardless of configuration source
		CheckCameraStatus(*cameraManager);

	}
	catch (const std::exception& e) {
		logger->LogError("Exception during camera initialization: " + std::string(e.what()));
		logger->LogInfo("Attempting fallback camera setup");

		// Emergency fallback
		try {
			auto camera1 = CameraInfo::CreateByIP("main_camera", "192.168.0.68", "Top view camera");
			cameraManager->AddCamera(camera1);
			cameraManager->InitializeAllCameras();
			logger->LogInfo("Emergency fallback camera setup completed");
		}
		catch (const std::exception& fallbackEx) {
			logger->LogError("Emergency fallback also failed: " + std::string(fallbackEx.what()));
		}
	}



	CheckCameraStatus(*cameraManager);








	// Initialize TCP Data Client Manager
	std::unique_ptr<DataClientManager> dataClientManager;
	try {
		dataClientManager = std::make_unique<DataClientManager>("DataServerConfig.json");
		//dataClientManager->ConnectAutoClients();
		logger->LogInfo("DataClientManager initialized successfully");
	}
	catch (const std::exception& e) {
		logger->LogError("Failed to initialize DataClientManager: " + std::string(e.what()));
		logger->LogWarning("TCP Data Manager will not be available");
	}

	// FIXED: Initialize CLD101x Manager with proper error handling
	std::unique_ptr<CLD101xManager> cld101xManager;
	std::unique_ptr<CLD101xOperations> laserOps;

	cld101xManager = std::make_unique<CLD101xManager>();

	//// Try to initialize with error checking
	//if (cld101xManager->Initialize()) {
	//	logger->LogInfo("CLD101x Manager initialized successfully");

	//	// Try to connect to laser hardware
	//	if (cld101xManager->ConnectAll()) {
	//		logger->LogInfo("Successfully connected to CLD101x laser systems");
	//		// Only create laserOps after successful connection
	//		laserOps = std::make_unique<CLD101xOperations>(*cld101xManager);
	//		logger->LogInfo("CLD101x operations module created");
	//	}
	//	else {
	//		logger->LogWarning("Failed to connect to CLD101x laser hardware");
	//		logger->LogInfo("Laser operations will be disabled - system will run without laser control");
	//		// laserOps remains nullptr - this is safe and intentional
	//	}
	//}
	//else {
	//	logger->LogError("Failed to initialize CLD101x Manager");
	//	logger->LogInfo("Laser operations will be disabled - system will run without laser control");
	//	// laserOps remains nullptr - this is safe and intentional
	//}

	// After creating cld101xManager, before creating MachineOperations:
// After creating cld101xManager, before creating MachineOperations:
	if (cld101xManager) {
		logger->LogInfo("=== MANUAL CLD101x CONNECTION ATTEMPT ===");

		// Manually add the client that the UI successfully connects to
		logger->LogInfo("Adding CLD101x client: 127.0.0.88:65432");
		cld101xManager->AddClient("CLD101x", "127.0.0.88", 65432);

		logger->LogInfo("Attempting ConnectAll()...");
		if (cld101xManager->ConnectAll()) {
			logger->LogInfo("ConnectAll() returned SUCCESS");
			logger->LogInfo("Creating CLD101xOperations...");
			laserOps = std::make_unique<CLD101xOperations>(*cld101xManager);

			if (laserOps) {
				logger->LogInfo("LaserOps successfully created!");
			}
			else {
				logger->LogError("LaserOps creation FAILED!");
			}
		}
		else {
			logger->LogError("ConnectAll() returned FAILURE");
		}

		logger->LogInfo("Final laserOps status: " + std::string(laserOps ? "AVAILABLE" : "NULL"));
		logger->LogInfo("=== END MANUAL CONNECTION ===");
	}


	// FIXED: Initialize Keithley 2400 Manager  
	std::unique_ptr<Keithley2400Manager> keithleyManager;
	std::unique_ptr<Keithley2400Operations> smuOps;

	keithleyManager = std::make_unique<Keithley2400Manager>();

	if (keithleyManager->Initialize("smu_config.json")) {
		logger->LogInfo("Keithley2400Manager initialized from config file");

		if (keithleyManager->ConnectAll()) {
			logger->LogInfo("Successfully connected to Keithley 2400 servers");
			// Only create smuOps after successful connection
			smuOps = std::make_unique<Keithley2400Operations>(*keithleyManager);
		}
		else {
			logger->LogWarning("Failed to connect to Keithley 2400 servers from config");
			// Don't create smuOps if connection failed
		}
	}
	else {
		logger->LogWarning("Failed to load Keithley config, trying fallback connection");
		keithleyManager->AddClient("Keithley-Main", "127.0.0.101", 8888);

		if (keithleyManager->ConnectAll()) {
			logger->LogInfo("Successfully connected to Keithley 2400 servers (fallback)");
			// Only create smuOps after successful fallback connection
			smuOps = std::make_unique<Keithley2400Operations>(*keithleyManager);
		}
		else {
			logger->LogInfo("No Keithley hardware available - SMU operations will be disabled");
			// smuOps remains nullptr - this is intentional and safe
		}
	}

	// Create Motion Control Layer
	std::unique_ptr<MotionControlLayer> motionControlLayer;
	if (piControllerManager && acsControllerManager) {
		motionControlLayer = std::make_unique<MotionControlLayer>(
			*motionConfigManager, *piControllerManager, *acsControllerManager);

		motionControlLayer->SetPathCompletionCallback([&logger](bool success) {
			if (success) {
				logger->LogInfo("Path execution completed successfully");
			}
			else {
				logger->LogWarning("Path execution failed or was cancelled");
			}
			});
		logger->LogInfo("MotionControlLayer initialized");
	}

	// Machine Operations - SAFE: Works with or without optional hardware
	std::unique_ptr<MachineOperations> machineOps;
	if (motionControlLayer && piControllerManager && ioManager && pneumaticManager) {

		machineOps = std::make_unique<MachineOperations>(
			*motionControlLayer,
			*piControllerManager,
			*ioManager,
			*pneumaticManager,
			laserOps.get(),      // Safe - can be nullptr
			cameraManager.get(),
			smuOps.get()         // Safe - can be nullptr
		);

		// Build comprehensive hardware status message
		std::string hardwareStatus = "Real MachineOperations initialized";

		// Check laser hardware status
		if (laserOps) {
			hardwareStatus += " WITH laser support";
			machineOps->SetLaserOperations(laserOps.get());
		}
		else {
			hardwareStatus += " WITHOUT laser support";
		}

		// Check SMU hardware status
		if (smuOps) {
			hardwareStatus += " and WITH SMU support";
			machineOps->SetSMUOperations(smuOps.get());
		}
		else {
			hardwareStatus += " and WITHOUT SMU support";
		}

		// Check camera hardware status (optional additional info)
		if (cameraManager) {
			hardwareStatus += " and WITH camera support";
		}
		else {
			hardwareStatus += " and WITHOUT camera support";
		}

		logger->LogInfo(hardwareStatus);

		// Log individual hardware status details for diagnostics
		if (!laserOps) {
			logger->LogInfo("Note: Laser operations disabled - LaserOn(), LaserOff(), SetLaserCurrent(), etc. will return false");
		}

		if (!smuOps) {
			logger->LogInfo("Note: SMU operations disabled - SMU_SetOutput(), SMU_ReadVoltage(), etc. will return false");
		}

		// Log what core systems are working
		logger->LogInfo("Core systems available: Motion Control, IO Management, Pneumatics");
	}
	else {
		logger->LogWarning("MachineOperations not initialized - missing required core components");
		logger->LogInfo("Required core components status:");
		logger->LogInfo("  - motionControlLayer: " + std::string(motionControlLayer ? "AVAILABLE" : "MISSING"));
		logger->LogInfo("  - piControllerManager: " + std::string(piControllerManager ? "AVAILABLE" : "MISSING"));
		logger->LogInfo("  - ioManager: " + std::string(ioManager ? "AVAILABLE" : "MISSING"));
		logger->LogInfo("  - pneumaticManager: " + std::string(pneumaticManager ? "AVAILABLE" : "MISSING"));
	}


#pragma region InitializeSplitOperations

	// NEW: Initialize the split operation classes
	std::unique_ptr<MotionOps> motionOps;
	std::unique_ptr<IOOps> ioOps;
	std::unique_ptr<VisionOps> visionOps;

	// Get shared database managers from MachineOperations if available
	std::shared_ptr<DatabaseManager> dbManager;
	std::shared_ptr<OperationResultsManager> resultsManager;

	if (machineOps) {
		dbManager = machineOps->GetDatabaseManager();
		resultsManager = machineOps->GetResultsManager();
	}

	// Initialize MotionOps
	if (motionControlLayer && piControllerManager) {
		motionOps = std::make_unique<MotionOps>(
			*motionControlLayer,
			*piControllerManager,
			dbManager,
			resultsManager
		);
		logger->LogInfo("MotionOps initialized");
	}

	// Initialize IOOps
	if (ioManager && pneumaticManager) {
		ioOps = std::make_unique<IOOps>(
			*ioManager,
			*pneumaticManager,
			dbManager,
			resultsManager
		);
		logger->LogInfo("IOOps initialized");
	}

	// Initialize VisionOps
	if (cameraManager) {
		visionOps = std::make_unique<VisionOps>(
			cameraManager.get(),
			dbManager,
			resultsManager
		);
		logger->LogInfo("VisionOps initialized");
	}



#pragma endregion




	// Create the main UI manager
	MainUIManager uiManager(*motionConfigManager);
	logger->LogInfo("MainUIManager created with MotionConfigManager");
	// NEW: Setup UserPromptUI and connect to RunPageUI
	std::unique_ptr<UserPromptUI> userPromptUI = std::make_unique<UserPromptUI>();

	// Set all managers in UI
	if (piControllerManager) {
		uiManager.SetPIControllerManager(piControllerManager.get());
	}
	if (acsControllerManager) {
		uiManager.SetACSControllerManager(acsControllerManager.get());
	}
	if (ioManager) {
		uiManager.SetIOManager(ioManager.get(), ioconfigManager.get());
	}
	if (pneumaticManager) {
		uiManager.SetPneumaticManager(pneumaticManager.get());
	}
	if (cameraManager && cameraManager->GetCameraCount() > 0) {
		uiManager.SetCameraManager(cameraManager.get());
	}
	if (dataClientManager) {
		uiManager.SetDataClientManager(dataClientManager.get());
	}
	if (cld101xManager) {
		uiManager.SetCLD101xManager(cld101xManager.get());
	}
	if (keithleyManager) {
		uiManager.SetKeithley2400Manager(keithleyManager.get());
	}
	if (machineOps) {
		uiManager.SetMachineOperations(machineOps.get());
		logger->LogInfo("MachineOperations set in MainUIManager");
	}
	// Connect UserPromptUI to RunPageUI
	// Verify it worked
	if (auto* runPageUI = uiManager.GetRunPageUI()) {
		std::cout << "✓ RunPageUI accessible - UAA3 sequences enabled!" << std::endl;
	}
	else {
		std::cout << "✗ RunPageUI not accessible - check setup" << std::endl;
	}

	// ADD THIS - Connect watchdog to UI manager
	if (configWatchdog) {
		uiManager.SetConfigWatchdog(configWatchdog.get());
		logger->LogInfo("Config watchdog connected to MainUIManager");
	}


	// In your uaa3App.cpp, replace the camera feed setup section with this:

	// CORRECTED: Create CameraFeedDisplay FIRST, then RaylibWindow
	std::unique_ptr<CameraFeedDisplay> raylibCameraFeed;
	std::unique_ptr<RaylibWindow> raylibWindow;

	// Step 1: Create camera feed display first
	if (cameraManager && cameraManager->GetCameraCount() > 0) {
		logger->LogInfo("Setting up camera feed for raylib 3D window...");

		// Create camera feed display
		raylibCameraFeed = std::make_unique<CameraFeedDisplay>();

		// **NEW: Connect to broadcasting system instead of direct camera**
		auto cameraIds = cameraManager->GetCameraIds();
		if (!cameraIds.empty()) {
			// Try to find a connected camera
			std::string selectedCameraId;
			for (const auto& cameraId : cameraIds) {
				auto status = cameraManager->GetCameraStatus(cameraId);
				if (status.connected) {
					selectedCameraId = cameraId;
					break;
				}
			}

			// If no connected camera found, use the first one anyway
			if (selectedCameraId.empty()) {
				selectedCameraId = cameraIds[0];
				logger->LogInfo("No connected cameras found, will try to connect to: " + selectedCameraId);
			}

			// **CRITICAL CHANGE: Use broadcasting system instead of direct camera connection**
			logger->LogInfo("=== SETTING UP CAMERA FEED WITH BROADCASTING ===");

			// Set the target camera for the feed display
			raylibCameraFeed->SetTargetCamera(selectedCameraId);
			logger->LogInfo("CameraFeedDisplay set to target camera: " + selectedCameraId);

			// **CRITICAL: Subscribe the CameraFeedDisplay to the broadcasting system**
			std::shared_ptr<CameraFrameSubscriber> feedSubscriber =
				std::static_pointer_cast<CameraFrameSubscriber>(
					std::shared_ptr<CameraFeedDisplay>(raylibCameraFeed.get(), [](CameraFeedDisplay*) {}));

			cameraManager->SubscribeToFrames(feedSubscriber);
			logger->LogInfo("CameraFeedDisplay subscribed to broadcasting system");

			// **CRITICAL: Start the broadcasting system if not already started**
			cameraManager->StartBroadcastSystem();
			logger->LogInfo("Camera broadcasting system started");

			// Try to auto-start the camera if it's connected
			PylonCameraTest* selectedCamera = cameraManager->GetCamera(selectedCameraId);
			if (selectedCamera) {
				auto& pylonCamera = selectedCamera->GetCamera();
				if (pylonCamera.IsConnected()) {
					if (!pylonCamera.IsGrabbing()) {
						// **IMPORTANT: Use StartGrabbing which automatically sets up broadcasting**
						if (cameraManager->StartGrabbing(selectedCameraId)) {
							logger->LogInfo("Started camera grabbing with broadcasting for raylib feed");
						}
						else {
							logger->LogWarning("Failed to start camera grabbing for raylib feed");
						}
					}
					else {
						logger->LogInfo("Camera already grabbing - should be broadcasting");
					}
				}
				else {
					logger->LogInfo("Camera not connected yet - feed will activate when camera connects");
				}
			}

			logger->LogInfo("=== CAMERA FEED SETUP COMPLETE ===");
		}
	}
	else {
		if (!cameraManager) {
			logger->LogInfo("No camera manager available for raylib integration");
		}
		else if (cameraManager->GetCameraCount() == 0) {
			logger->LogInfo("No cameras available for raylib integration");
		}
	}

	// Step 2: Initialize RaylibWindow AFTER camera feed is ready
	bool enableRaylib3D = true; // Set to false to disable

	if (enableRaylib3D) {
		raylibWindow = std::make_unique<RaylibWindow>();

		// Set the logger first
		raylibWindow->SetLogger(logger);

		// Set connections BEFORE initializing
		if (piControllerManager) {
			raylibWindow->SetPIControllerManager(piControllerManager.get());
		}
		if (dataStore) {
			raylibWindow->SetDataStore(dataStore);
		}
		if (machineOps) {
			raylibWindow->SetMachineOperations(machineOps.get());
		}

		// Set camera feed BEFORE starting thread (now raylibCameraFeed exists)
		if (raylibCameraFeed) {
			raylibWindow->SetCameraFeedDisplay(raylibCameraFeed.get());
			logger->LogInfo("Connected camera feed to raylib window");
		}
		else {
			logger->LogInfo("No camera feed available for raylib window");
		}

		// THEN initialize the thread (this starts the render loop)
		if (raylibWindow->Initialize()) {
			logger->LogInfo("Raylib 3D Window thread started successfully");
		}
		else {
			logger->LogError("Failed to start Raylib 3D Window");
			raylibWindow.reset(); // Clean up on failure
		}
	}


	//debug setup 
// You can call this function in your main loop or when troubleshooting:
	DebugCameraFeedSetupEnhanced(cameraManager.get(), raylibCameraFeed.get(), raylibWindow.get(), logger);




	bool done = false;
	bool glyphChecked = false;



	// ADD MenuManager creation HERE instead:
// Create menu manager and debug window
	std::unique_ptr<MenuManagerUaa3> menuManager = std::make_unique<MenuManagerUaa3>();
	std::unique_ptr<RaylibDebugWindow> raylibDebugWindow = std::make_unique<RaylibDebugWindow>();

	// Set up the raylib debug window
	if (cameraManager) {
		raylibDebugWindow->SetCameraManager(cameraManager.get());
	}
	if (raylibWindow) {
		raylibDebugWindow->SetRaylibWindow(raylibWindow.get());
	}
	if (raylibCameraFeed) {
		raylibDebugWindow->SetCameraFeedDisplay(raylibCameraFeed.get());
	}
	raylibDebugWindow->SetLogger(logger);

	// Set up menu callbacks
	menuManager->SetOnExitCallback([&done]() {
		done = true;
		});


	//uiManager.SetImguiFont(mainFont);

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


			// *** ADD THIS NEW SECTION FOR KEYBOARD INPUT ***
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
		if (dataClientManager) {
			dataClientManager->UpdateClients();
		}




		// Render the main UI
		uiManager.RenderUI();

		// IMPORTANT: Render UserPromptUI to handle UAA3 prompts
		if (userPromptUI) {
			userPromptUI->Render();
		}

#pragma region rayLibwindow
		// RENDER RAYLIB DEBUG WINDOW (controlled by menu)
		if (menuManager->IsRaylibDebugVisible()) {
			ImGui::Begin("Raylib Live Feed Debug", nullptr);

			// Put your existing debug code here temporarily
			ImGui::Text("3D Window Camera Feed Control");
			ImGui::Separator();

			// Camera selection dropdown
			static std::string selectedCameraId;
			auto cameraIds = cameraManager->GetCameraIds();

			if (!cameraIds.empty()) {
				// Initialize selection if empty
				if (selectedCameraId.empty()) {
					selectedCameraId = cameraIds[0];
				}

				ImGui::Text("Select Camera:");
				if (ImGui::BeginCombo("##CameraSelection", selectedCameraId.c_str())) {
					for (const auto& cameraId : cameraIds) {
						bool isSelected = (selectedCameraId == cameraId);
						auto status = cameraManager->GetCameraStatus(cameraId);

						std::string displayName = cameraId;
						if (status.connected) {
							displayName += " (Connected)";
						}
						else {
							displayName += " (Disconnected)";
						}

						if (ImGui::Selectable(displayName.c_str(), isSelected)) {
							selectedCameraId = cameraId;

							// ✅ Update the raylib camera feed to use subscriber mode for new camera
							if (raylibCameraFeed) {
								logger->LogInfo("Switching raylib camera feed from subscriber mode...");

								// **CRITICAL: Switch raylibCameraFeed to new camera using subscriber mode**
								raylibCameraFeed->SetTargetCamera(selectedCameraId);
								logger->LogInfo("Raylib camera feed switched to: " + selectedCameraId);

								// Make sure the camera is grabbing for the feed
								PylonCameraTest* newCamera = cameraManager->GetCamera(selectedCameraId);
								if (newCamera) {
									auto& pylonCamera = newCamera->GetCamera();
									if (pylonCamera.IsConnected() && !pylonCamera.IsGrabbing()) {
										if (cameraManager->StartGrabbing(selectedCameraId)) {
											logger->LogInfo("Started grabbing for raylib feed: " + selectedCameraId);
										}
									}
								}
							}
						}

						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}

				// Add basic camera controls
				auto status = cameraManager->GetCameraStatus(selectedCameraId);
				ImGui::Separator();
				ImGui::Text("Camera Status:");
				ImGui::SameLine();
				if (status.connected) {
					ImGui::TextColored(ImVec4(0, 1, 0, 1), "Connected");
				}
				else {
					ImGui::TextColored(ImVec4(1, 0, 0, 1), "Disconnected");
				}

				// Quick start button
				if (ImGui::Button("Quick Start: Connect & Start Feed")) {
					if (cameraManager->ConnectCamera(selectedCameraId)) {
						logger->LogInfo("Connected camera: " + selectedCameraId);
						if (cameraManager->StartGrabbing(selectedCameraId)) {
							logger->LogInfo("Started video feed for: " + selectedCameraId);
							PylonCameraTest* camera = cameraManager->GetCamera(selectedCameraId);
							if (camera && raylibCameraFeed) {
								raylibCameraFeed->SetPylonCameraSource(camera);
								raylibWindow->SetCameraFeedVisible(true);
								logger->LogInfo("Camera feed connected to 3D window");
							}
						}
					}
				}


				// Fixed version with proper variable scope

				ImGui::Separator();
				ImGui::Text("Live Preview:");

				if (!selectedCameraId.empty()) {
					PylonCameraTest* camera = cameraManager->GetCamera(selectedCameraId);
					if (camera) {
						auto& pylonCamera = camera->GetCamera();

						// **MOVE STATIC VARIABLES TO TOP LEVEL - OUTSIDE ALL BLOCKS**
						static std::shared_ptr<LiveVideoSubscriber> debugSubscriber = nullptr;
						static unsigned int debugTextureID = 0;
						static bool subscriberCreated = false;
						static std::string lastSelectedCamera = "";
						static int frameCheckCounter = 0;

						frameCheckCounter++;

						// **ENHANCED DEBUG: Show detailed camera status**
						ImGui::Text("=== CAMERA DEBUG INFO ===");
						ImGui::Text("Selected Camera: %s", selectedCameraId.c_str());
						ImGui::Text("Camera Connected: %s", pylonCamera.IsConnected() ? "YES" : "NO");
						ImGui::Text("Camera Grabbing: %s", pylonCamera.IsGrabbing() ? "YES" : "NO");
						ImGui::Text("Camera Has Valid Texture: %s", camera->HasValidTexture() ? "YES" : "NO");
						ImGui::Text("Camera Texture ID: %u", camera->GetTextureID());

						// **ENHANCED DEBUG: Show broadcast system status**
						ImGui::Text("=== BROADCAST SYSTEM DEBUG ===");
						ImGui::Text("Total Subscribers: %zu", cameraManager->GetSubscriberCount());
						auto subscriberIds = cameraManager->GetSubscriberIds();
						for (size_t i = 0; i < subscriberIds.size(); ++i) {
							ImGui::Text("  Subscriber %zu: %s", i + 1, subscriberIds[i].c_str());
						}

						ImGui::Separator();

						// Reset if camera changed
						if (lastSelectedCamera != selectedCameraId) {
							logger->LogInfo("=== CAMERA CHANGED DEBUG ===");
							logger->LogInfo("Old camera: " + lastSelectedCamera);
							logger->LogInfo("New camera: " + selectedCameraId);

							debugSubscriber = nullptr;
							subscriberCreated = false;
							lastSelectedCamera = selectedCameraId;

							// Clean up old texture
							if (debugTextureID != 0) {
								glDeleteTextures(1, &debugTextureID);
								debugTextureID = 0;
								logger->LogInfo("Cleaned up old texture");
							}
							logger->LogInfo("=== END CAMERA CHANGED DEBUG ===");
						}

						// **FIX 1: Check and start grabbing**
						if (pylonCamera.IsConnected() && !pylonCamera.IsGrabbing()) {
							ImGui::TextColored(ImVec4(1, 1, 0, 1), "Camera connected but not grabbing");
							if (ImGui::Button("Start Grabbing for Preview")) {
								try {
									logger->LogInfo("=== STARTING GRABBING DEBUG ===");
									logger->LogInfo("Camera ID: " + selectedCameraId);
									logger->LogInfo("Camera connected: " + std::string(pylonCamera.IsConnected() ? "YES" : "NO"));

									if (cameraManager->StartGrabbing(selectedCameraId)) {
										logger->LogInfo("StartGrabbing returned SUCCESS");
										logger->LogInfo("Camera now grabbing: " + std::string(pylonCamera.IsGrabbing() ? "YES" : "NO"));

										// **DEBUG: Force start broadcast system**
										cameraManager->StartBroadcastSystem();
										logger->LogInfo("Broadcast system started");
										logger->LogInfo("Subscriber count: " + std::to_string(cameraManager->GetSubscriberCount()));

									}
									else {
										logger->LogError("StartGrabbing returned FAILURE");
									}
									logger->LogInfo("=== END STARTING GRABBING DEBUG ===");
								}
								catch (const std::exception& e) {
									logger->LogError("Exception starting grabbing: " + std::string(e.what()));
								}
								catch (...) {
									logger->LogError("Unknown exception starting grabbing");
								}
							}
						}

						// **CREATE SUBSCRIBER (regardless of grabbing status)**
						if (!subscriberCreated) {
							try {
								logger->LogInfo("=== CREATING SUBSCRIBER DEBUG ===");
								logger->LogInfo("Creating subscriber for: " + selectedCameraId);

								debugSubscriber = std::make_shared<LiveVideoSubscriber>(selectedCameraId);
								logger->LogInfo("Subscriber created with ID: " + debugSubscriber->GetSubscriberId());

								logger->LogInfo("Subscribing to CameraManager...");
								cameraManager->SubscribeToFrames(debugSubscriber);
								logger->LogInfo("Subscription successful");
								logger->LogInfo("Total subscribers now: " + std::to_string(cameraManager->GetSubscriberCount()));

								subscriberCreated = true;
								logger->LogInfo("=== END CREATING SUBSCRIBER DEBUG ===");
							}
							catch (const std::exception& e) {
								logger->LogError("Exception creating subscriber: " + std::string(e.what()));
								debugSubscriber = nullptr;
							}
							catch (...) {
								logger->LogError("Unknown exception creating subscriber");
								debugSubscriber = nullptr;
							}
						}

						// **ENHANCED DEBUG: Show detailed subscriber status**

						if (debugSubscriber) {
							ImGui::Text("=== SUBSCRIBER STATS ===");
							ImGui::Text("Connected: %s", debugSubscriber->IsCameraConnected() ? "Yes" : "No");
							ImGui::Text("Grabbing: %s", debugSubscriber->IsCameraGrabbing() ? "Yes" : "No");
							ImGui::Text("Frames RX: %llu", debugSubscriber->GetTotalFramesReceived());
							ImGui::Text("Last Frame Timestamp: %llu", debugSubscriber->GetLastFrameTimestamp());

							// **NEW: Frame buffering system**
							static CameraFrameData lastValidFrame;
							static bool hasBufferedFrame = false;
							static auto lastFrameUpdate = std::chrono::steady_clock::now();

							// **Check for new frames and buffer them**
							if (debugSubscriber->HasNewFrame()) {
								auto newFrame = debugSubscriber->GetLatestFrame();
								debugSubscriber->MarkFrameConsumed();

								if (newFrame.IsValid() && newFrame.channels == 3) {
									lastValidFrame = std::move(newFrame);  // Store the frame
									hasBufferedFrame = true;
									lastFrameUpdate = std::chrono::steady_clock::now();
								}
							}

							// **ALWAYS display the buffered frame if we have one**
							if (hasBufferedFrame) {
								// Update texture with buffered frame
								if (debugTextureID == 0) {
									glGenTextures(1, &debugTextureID);
								}

								glBindTexture(GL_TEXTURE_2D, debugTextureID);
								glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
								glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

								glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
									lastValidFrame.width, lastValidFrame.height, 0,
									GL_RGB, GL_UNSIGNED_BYTE, lastValidFrame.imageData.data());

								glBindTexture(GL_TEXTURE_2D, 0);

								// Display the texture
								ImTextureID texID = (ImTextureID)(intptr_t)debugTextureID;
								ImGui::Image(texID, ImVec2(300, 200));

								// Show frame info and freshness
								auto timeSinceUpdate = std::chrono::steady_clock::now() - lastFrameUpdate;
								auto millisSinceUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(timeSinceUpdate).count();

								if (ImGui::IsItemHovered()) {
									ImGui::SetTooltip("Live Camera Feed\nFrame: %dx%d\nTotal Frames: %llu\nLast Update: %lld ms ago",
										lastValidFrame.width, lastValidFrame.height,
										debugSubscriber->GetTotalFramesReceived(),
										millisSinceUpdate);
								}

								// **Show freshness indicator**
								if (millisSinceUpdate < 100) {
									ImGui::TextColored(ImVec4(0, 1, 0, 1), "LIVE (updated %lld ms ago)", millisSinceUpdate);
								}
								else if (millisSinceUpdate < 1000) {
									ImGui::TextColored(ImVec4(1, 1, 0, 1), "Recent (updated %lld ms ago)", millisSinceUpdate);
								}
								else {
									ImGui::TextColored(ImVec4(1, 0, 0, 1), "Stale (updated %lld ms ago)", millisSinceUpdate);
								}

							}
							else {
								// Only show "waiting" if we truly have no frames yet
								ImGui::Button("Waiting for first frame...", ImVec2(300, 200));
								ImGui::Text("No frames received yet");
							}
						}

						// **FIX: Replace your debug window subscriber management with this leak-free version**

// Replace the subscriber section in your debug window with this:

						if (debugSubscriber) {
							// **CLEANUP CHECK: Remove subscriber if camera changed**
							if (debugSubscriber->GetTargetCamera() != selectedCameraId) {
								logger->LogInfo("=== CLEANING UP OLD SUBSCRIBER ===");
								logger->LogInfo("Old camera: " + debugSubscriber->GetTargetCamera());
								logger->LogInfo("New camera: " + selectedCameraId);

								// Unsubscribe the old subscriber
								cameraManager->UnsubscribeFromFrames(debugSubscriber->GetSubscriberId());
								logger->LogInfo("Unsubscribed old subscriber: " + debugSubscriber->GetSubscriberId());

								// Clear old subscriber
								debugSubscriber = nullptr;
								subscriberCreated = false;

								// Clean up old texture
								if (debugTextureID != 0) {
									glDeleteTextures(1, &debugTextureID);
									debugTextureID = 0;
									logger->LogInfo("Cleaned up old texture");
								}
								logger->LogInfo("=== END CLEANUP OLD SUBSCRIBER ===");
							}
						}

						// **CREATE NEW SUBSCRIBER only if needed**
						if (!subscriberCreated || !debugSubscriber) {
							try {
								logger->LogInfo("=== CREATING NEW SUBSCRIBER DEBUG ===");
								logger->LogInfo("Creating subscriber for: " + selectedCameraId);

								debugSubscriber = std::make_shared<LiveVideoSubscriber>(selectedCameraId);
								logger->LogInfo("Subscriber created with ID: " + debugSubscriber->GetSubscriberId());

								logger->LogInfo("Subscribing to CameraManager...");
								cameraManager->SubscribeToFrames(debugSubscriber);
								logger->LogInfo("Subscription successful");
								logger->LogInfo("Total subscribers now: " + std::to_string(cameraManager->GetSubscriberCount()));

								subscriberCreated = true;
								logger->LogInfo("=== END CREATING NEW SUBSCRIBER DEBUG ===");
							}
							catch (const std::exception& e) {
								logger->LogError("Exception creating subscriber: " + std::string(e.what()));
								debugSubscriber = nullptr;
								subscriberCreated = false;
							}
							catch (...) {
								logger->LogError("Unknown exception creating subscriber");
								debugSubscriber = nullptr;
								subscriberCreated = false;
							}
						}

						// **ALSO: Add a manual cleanup button**
						// Add this button to your manual debug controls:

						if (ImGui::Button("Cleanup All Subscribers")) {
							logger->LogInfo("=== MANUAL SUBSCRIBER CLEANUP ===");
							logger->LogInfo("Current subscriber count: " + std::to_string(cameraManager->GetSubscriberCount()));

							// Get all subscriber IDs
							auto subscriberIds = cameraManager->GetSubscriberIds();
							for (const auto& id : subscriberIds) {
								logger->LogInfo("Found subscriber: " + id);
							}

							// Clear our debug subscriber
							if (debugSubscriber) {
								cameraManager->UnsubscribeFromFrames(debugSubscriber->GetSubscriberId());
								logger->LogInfo("Unsubscribed debug subscriber: " + debugSubscriber->GetSubscriberId());
								debugSubscriber = nullptr;
							}

							subscriberCreated = false;

							// Clean up texture
							if (debugTextureID != 0) {
								glDeleteTextures(1, &debugTextureID);
								debugTextureID = 0;
								logger->LogInfo("Cleaned up debug texture");
							}

							logger->LogInfo("Final subscriber count: " + std::to_string(cameraManager->GetSubscriberCount()));
							logger->LogInfo("=== END MANUAL CLEANUP ===");
						}

						// **IMPROVED: Better camera change detection**
						// Replace your camera change detection with this more robust version:

						// Reset if camera changed
						if (lastSelectedCamera != selectedCameraId) {
							logger->LogInfo("=== CAMERA CHANGED DEBUG ===");
							logger->LogInfo("Old camera: " + lastSelectedCamera);
							logger->LogInfo("New camera: " + selectedCameraId);

							// **IMPORTANT: Clean up subscriber for old camera**
							if (debugSubscriber && debugSubscriber->GetTargetCamera() != selectedCameraId) {
								logger->LogInfo("Unsubscribing from old camera: " + debugSubscriber->GetTargetCamera());
								cameraManager->UnsubscribeFromFrames(debugSubscriber->GetSubscriberId());
								debugSubscriber = nullptr;
							}

							subscriberCreated = false;
							lastSelectedCamera = selectedCameraId;

							// Clean up old texture
							if (debugTextureID != 0) {
								glDeleteTextures(1, &debugTextureID);
								debugTextureID = 0;
								logger->LogInfo("Cleaned up old texture");
							}
							logger->LogInfo("=== END CAMERA CHANGED DEBUG ===");
						}

						if (ImGui::Button("Copy to Main Camera UI")) {
							logger->LogInfo("Camera feed copied to main UI");
						}

						// **MANUAL DEBUG BUTTONS - NOW debugSubscriber IS IN SCOPE**
						ImGui::Separator();
						ImGui::Text("Manual Debug Controls:");

						if (ImGui::Button("Force Camera Status Update")) {
							if (debugSubscriber) {
								debugSubscriber->OnCameraStatusChanged(selectedCameraId, pylonCamera.IsConnected(), pylonCamera.IsGrabbing());
								logger->LogInfo("Manually triggered camera status update");
							}
							else {
								logger->LogWarning("Cannot update status - debugSubscriber is null");
							}
						}

						ImGui::SameLine();
						if (ImGui::Button("Log All Debug Info")) {
							logger->LogInfo("=== MANUAL DEBUG DUMP ===");
							logger->LogInfo("Selected camera: " + selectedCameraId);
							logger->LogInfo("Camera connected: " + std::string(pylonCamera.IsConnected() ? "YES" : "NO"));
							logger->LogInfo("Camera grabbing: " + std::string(pylonCamera.IsGrabbing() ? "YES" : "NO"));
							logger->LogInfo("Subscriber count: " + std::to_string(cameraManager->GetSubscriberCount()));
							if (debugSubscriber) {
								logger->LogInfo("Subscriber ID: " + debugSubscriber->GetSubscriberId());
								logger->LogInfo("Subscriber total frames: " + std::to_string(debugSubscriber->GetTotalFramesReceived()));
								logger->LogInfo("Subscriber connected: " + std::string(debugSubscriber->IsCameraConnected() ? "YES" : "NO"));
								logger->LogInfo("Subscriber grabbing: " + std::string(debugSubscriber->IsCameraGrabbing() ? "YES" : "NO"));
							}
							else {
								logger->LogWarning("debugSubscriber is null");
							}
							logger->LogInfo("=== END MANUAL DEBUG DUMP ===");
						}



						ImGui::SameLine();
						if (ImGui::Button("Restart Camera With Broadcasting")) {
							if (pylonCamera.IsConnected()) {
								logger->LogInfo("=== RESTARTING CAMERA WITH BROADCASTING ===");
								logger->LogInfo("Camera: " + selectedCameraId);
								logger->LogInfo("Currently grabbing: " + std::string(pylonCamera.IsGrabbing() ? "YES" : "NO"));

								// Stop current grabbing
								if (pylonCamera.IsGrabbing()) {
									cameraManager->StopGrabbing(selectedCameraId);
									logger->LogInfo("Stopped current grabbing");
									std::this_thread::sleep_for(std::chrono::milliseconds(200));
								}

								// Start grabbing with broadcasting (this should set the callback)
								if (cameraManager->StartGrabbing(selectedCameraId)) {
									logger->LogInfo("Restarted grabbing with broadcasting");
									logger->LogInfo("Now grabbing: " + std::string(pylonCamera.IsGrabbing() ? "YES" : "NO"));
								}
								else {
									logger->LogError("Failed to restart grabbing");
								}

								logger->LogInfo("=== END RESTART CAMERA ===");
							}
							else {
								logger->LogWarning("Camera not connected - cannot restart");
							}
						}

						if (ImGui::Button("Test Frame Callback")) {
							logger->LogInfo("=== TESTING FRAME CALLBACK ===");
							if (pylonCamera.IsConnected() && pylonCamera.IsGrabbing()) {
								// This should trigger the callback if it's set up correctly
								logger->LogInfo("Camera is grabbing - callbacks should be firing");
								logger->LogInfo("Check console for '[DEBUG] OnCameraFrameReceived called' messages");
							}
							else {
								logger->LogWarning("Camera not grabbing - no callbacks expected");
							}
							logger->LogInfo("=== END TEST FRAME CALLBACK ===");
						}


						// **ALSO: Add this debug button to manually trigger a test frame**
						// Add this to your debug window:

						if (ImGui::Button("Send Test Frame")) {
							logger->LogInfo("=== SENDING TEST FRAME ===");

							if (debugSubscriber) {
								// Create a test frame manually
								CameraFrameData testFrame;
								testFrame.cameraId = selectedCameraId;
								testFrame.width = 320;
								testFrame.height = 240;
								testFrame.channels = 3;
								testFrame.timestamp = 12345;
								testFrame.frameNumber = 999;

								// Create blue test image
								size_t dataSize = testFrame.width * testFrame.height * 3;
								testFrame.imageData.resize(dataSize);

								for (size_t i = 0; i < dataSize; i += 3) {
									testFrame.imageData[i] = 0;       // R
									testFrame.imageData[i + 1] = 0;     // G  
									testFrame.imageData[i + 2] = 255;   // B (blue)
								}

								logger->LogInfo("Created test frame: " + std::to_string(testFrame.width) + "x" + std::to_string(testFrame.height));
								logger->LogInfo("Sending to subscriber: " + debugSubscriber->GetSubscriberId());

								try {
									debugSubscriber->OnNewFrame(testFrame);
									logger->LogInfo("Test frame sent successfully!");
								}
								catch (const std::exception& e) {
									logger->LogError("Exception sending test frame: " + std::string(e.what()));
								}
							}
							else {
								logger->LogWarning("No subscriber available for test");
							}

							logger->LogInfo("=== END SEND TEST FRAME ===");
						}

					}
					else {
						ImGui::Button("Camera not found", ImVec2(300, 200));
						ImGui::Text("ERROR: Camera pointer is null");
					}
				}
				else {
					ImGui::Button("No camera selected", ImVec2(300, 200));
					ImGui::Text("ERROR: selectedCameraId is empty");
				}



			}
			else {
				ImGui::Text("No cameras available");
			}

			ImGui::End();
		}

		// Update raylib window with current machine data (add this in the main loop)
		if (raylibWindow && raylibWindow->IsRunning()) {
			// Collect current machine data from your systems
			MachineData data = { 0, 0, 0, 0, 0, 0, 0, 0, 0, false, false, false };


			// Update 3D visualization (thread-safe)
			raylibWindow->UpdateMachineData(data);

			// Check if raylib window was closed
			if (raylibWindow->ShouldClose()) {
				logger->LogInfo("Raylib window closed by user");
				raylibWindow->Shutdown();
				raylibWindow.reset();
			}
		}
#pragma endregion

#pragma region DebugMode
		//debug mode smaller ops
		if (g_deugMode) {
			// NEW: Add test window for MotionOps
			static bool showMotionTest = true;
			if (showMotionTest && motionOps) {
				ImGui::Begin("MotionOps Test", &showMotionTest);

				ImGui::Text("Gantry Movement Test");
				ImGui::Separator();

				// Test buttons for gantry movement
				if (ImGui::Button("Move X +1mm")) {
					bool success = motionOps->MoveRelative("gantry-main", "X", 1.0, true, "MainLoop_Test");
					//bool success = true;
					if (success) {
						logger->LogInfo("Successfully moved gantry X +1mm");
					}
					else {
						logger->LogError("Failed to move gantry X +1mm");
					}
				}

				ImGui::SameLine();
				if (ImGui::Button("Move X -1mm")) {
					bool success = motionOps->MoveRelative("gantry-main", "X", -1.0, true, "MainLoop_Test");
					if (success) {
						logger->LogInfo("Successfully moved gantry X -1mm");
					}
					else {
						logger->LogError("Failed to move gantry X -1mm");
					}
				}

				if (ImGui::Button("Move Y +1mm")) {
					bool success = motionOps->MoveRelative("gantry-main", "Y", 1.0, true, "MainLoop_Test");
					if (success) {
						logger->LogInfo("Successfully moved gantry Y +1mm");
					}
					else {
						logger->LogError("Failed to move gantry Y +1mm");
					}
				}

				ImGui::SameLine();
				if (ImGui::Button("Move Y -1mm")) {
					bool success = motionOps->MoveRelative("gantry-main", "Y", -1.0, true, "MainLoop_Test");
					if (success) {
						logger->LogInfo("Successfully moved gantry Y -1mm");
					}
					else {
						logger->LogError("Failed to move gantry Y -1mm");
					}
				}

				if (ImGui::Button("Move Z +1mm")) {
					bool success = motionOps->MoveRelative("gantry-main", "Z", 1.0, true, "MainLoop_Test");
					if (success) {
						logger->LogInfo("Successfully moved gantry Z +1mm");
					}
					else {
						logger->LogError("Failed to move gantry Z +1mm");
					}
				}

				ImGui::SameLine();
				if (ImGui::Button("Move Z -1mm")) {
					bool success = motionOps->MoveRelative("gantry-main", "Z", -1.0, true, "MainLoop_Test");
					if (success) {
						logger->LogInfo("Successfully moved gantry Z -1mm");
					}
					else {
						logger->LogError("Failed to move gantry Z -1mm");
					}
				}

				ImGui::Separator();
				ImGui::Text("Device Status:");

				if (motionOps->IsDeviceConnected("gantry-main")) {
					ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ Gantry Connected");
				}
				else {
					ImGui::TextColored(ImVec4(1, 0, 0, 1), "✗ Gantry Disconnected");
				}

				if (motionOps->IsDeviceMoving("gantry-main")) {
					ImGui::TextColored(ImVec4(1, 1, 0, 1), "⚠ Gantry Moving");
				}
				else {
					ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ Gantry Idle");
				}

				// Show current position if available
				PositionStruct currentPos;
				if (motionOps->GetDeviceCurrentPosition("gantry-main", currentPos)) {
					ImGui::Text("Current Position:");
					ImGui::Text("  X: %.3f mm", currentPos.x);
					ImGui::Text("  Y: %.3f mm", currentPos.y);
					ImGui::Text("  Z: %.3f mm", currentPos.z);
				}

				ImGui::End();
			}
		}
#pragma endregion


		// ADD THIS - Optional: Log watchdog stats periodically
		static auto lastWatchdogStats = std::chrono::steady_clock::now();
		auto now = std::chrono::steady_clock::now();
		auto timeSinceStats = std::chrono::duration_cast<std::chrono::minutes>(now - lastWatchdogStats);

		if (configWatchdog && timeSinceStats.count() >= 10) { // Every 10 minutes
			auto stats = configWatchdog->GetStatistics();
			if (stats["changes_detected"].get<int>() > 0) {
				logger->LogInfo("📊 Watchdog stats: " +
					std::to_string(stats["changes_detected"].get<int>()) + " changes, " +
					std::to_string(stats["database_updates"].get<int>()) + " DB updates");
			}
			lastWatchdogStats = now;
		}










		// Main test window
		ImGui::Begin("Emoji Test");

		ImGui::Text("Configuration Status:");
#ifdef IMGUI_USE_WCHAR32
		ImGui::TextColored(ImVec4(0, 1, 0, 1), "✅ IMGUI_USE_WCHAR32");
#else
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "❌ IMGUI_USE_WCHAR32 - add to imconfig.h");
#endif

#ifdef IMGUI_ENABLE_FREETYPE
		ImGui::TextColored(ImVec4(0, 1, 0, 1), "✅ IMGUI_ENABLE_FREETYPE");
#else
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "❌ IMGUI_ENABLE_FREETYPE - add to imconfig.h");
#endif

		if (emojiLoaded) {
			ImGui::TextColored(ImVec4(0, 1, 0, 1), "✅ Emoji font loaded");
		}
		else {
			ImGui::TextColored(ImVec4(1, 0, 0, 1), "❌ No emoji font");
		}

		ImGui::Separator();

		ImGui::Text("Font Debug Information:");
		ImGui::Text("Total fonts loaded: %d", io.Fonts->Fonts.Size);

		for (int i = 0; i < io.Fonts->Fonts.Size; i++) {
			ImFont* font = io.Fonts->Fonts[i];
			ImGui::Text("Font %d: %.0f px, %d glyphs", i, font->FontSize, font->Glyphs.Size);

			// Test Greek letters with each font
			if (ImGui::TreeNode(("Test with Font " + std::to_string(i)).c_str())) {
				ImGui::PushFont(font);
				ImGui::Text("Greek test: α β γ μ");
				ImGui::Text("Math test: ± ÷ × ≠ ∞");
				ImGui::Text("Emoji test: 👍 ❤ 🔥");
				ImGui::PopFont();
				ImGui::TreePop();
			}
		}

		ImGui::Separator();

		// Check specific glyph availability
		ImGui::Text("Glyph Availability Check:");

		struct TestGlyph {
			ImWchar codepoint;
			const char* name;
			const char* utf8;
		};

		TestGlyph testGlyphs[] = {
				{0x03B1, "α (alpha)", reinterpret_cast<const char*>(u8"α")},
				{0x03B2, "β (beta)", reinterpret_cast<const char*>(u8"β")},
				{0x03B3, "γ (gamma)", reinterpret_cast<const char*>(u8"γ")},
				{0x03BC, "μ (mu)", reinterpret_cast<const char*>(u8"μ")},
				{0x1F44D, "👍 (thumbs up)", reinterpret_cast<const char*>(u8"👍")},
				{0x2764, "❤ (heart)", reinterpret_cast<const char*>(u8"❤")},
		};

		for (const auto& glyph : testGlyphs) {
			const ImFontGlyph* g = nullptr;
			int fontIndex = -1;

			for (int i = 0; i < io.Fonts->Fonts.Size; i++) {
				g = io.Fonts->Fonts[i]->FindGlyph(glyph.codepoint);
				if (g) {
					fontIndex = i;
					break;
				}
			}

			ImGui::Text("U+%04X %s: %s",
				glyph.codepoint,
				glyph.name,
				g ? "✅ Found" : "❌ Missing");

			if (g) {
				ImGui::SameLine();
				ImGui::Text("(Font %d) ", fontIndex);
				ImGui::SameLine();

				// Try to render the actual character
				if (fontIndex >= 0 && fontIndex < io.Fonts->Fonts.Size) {
					ImGui::PushFont(io.Fonts->Fonts[fontIndex]);
					ImGui::Text("%s", reinterpret_cast<const char*>(glyph.utf8));
					ImGui::PopFont();
				}
			}
		}

		ImGui::Separator();

		// Additional debug: check fallback character
		ImFont* currentFont = io.Fonts->Fonts[0];
		ImGui::Text("Font Debug:");
		ImGui::Text("Fallback char: U+%04X (%c)", currentFont->FallbackChar, (char)currentFont->FallbackChar);
		ImGui::Text("Ellipsis char: U+%04X", currentFont->EllipsisChar);

		// Test direct character codes
		ImGui::Text("Direct character test:");
		ImGui::Text("Alpha test: %lc", (wint_t)0x03B1);  // Try wide character

		ImGui::Separator();

		ImGui::Text("Basic Tests:");
		ImGui::Text("ASCII: Hello World 123");

		// Test with different approaches
		ImGui::Text("Unicode Test Methods:");
		ImGui::Text("Method 1 (reinterpret_cast): ");
		ImGui::SameLine();
		ImGui::TextUnformatted(reinterpret_cast<const char*>(u8"α β γ μ"));

		ImGui::Text("Method 2 (direct UTF-8): ");
		ImGui::SameLine();
		ImGui::Text("α β γ μ");  // Direct UTF-8 without u8 prefix

		ImGui::Text("Method 3 (escape codes): ");
		ImGui::SameLine();
		ImGui::Text("\u03B1 \u03B2 \u03B3 \u03BC");  // α β γ μ as Unicode escapes

		ImGui::Text("Method 4 (hex codes): ");
		ImGui::SameLine();
		ImGui::Text("U+03B1=%c U+03B2=%c U+03B3=%c U+03BC=%c",
			(char)0x03B1, (char)0x03B2, (char)0x03B3, (char)0x03BC);

		ImGui::TextUnformatted(reinterpret_cast<const char*>(u8"Math: ± ÷ × ≠ ∞"));
		ImGui::TextUnformatted(reinterpret_cast<const char*>(u8"Arrows: ← → ↑ ↓"));

		ImGui::Separator();

		ImGui::Text("Your Test Cases:");
		ImGui::TextUnformatted(reinterpret_cast<const char*>(u8"⌚ <- watch"));
		ImGui::TextUnformatted(reinterpret_cast<const char*>(u8"😊 <- smile"));

		ImGui::Separator();

		ImGui::Text("More Emoji Tests:");
		ImGui::TextUnformatted(reinterpret_cast<const char*>(u8"👍 ❤ 🔥 🎉 🚀"));
		ImGui::TextUnformatted(reinterpret_cast<const char*>(u8"😀 😃 😄 😁 😆"));

		ImGui::Separator();

		ImGui::Text("Instructions:");
		ImGui::BulletText("Add '#define IMGUI_USE_WCHAR32' to imconfig.h");
		ImGui::BulletText("Add '#define IMGUI_ENABLE_FREETYPE' to imconfig.h");
		ImGui::BulletText("Put emoji font in assets/fonts/");

		ImGui::End();







		// Rendering
		ImGui::Render();
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		SDL_GL_SwapWindow(window);
	}


	// Cleanup RaylibWindow (add this in the cleanup section)
	if (raylibWindow) {
		logger->LogInfo("Shutting down Raylib thread...");
		raylibWindow->Shutdown();
		raylibWindow.reset();
	}

	// Cleanup
	logger->LogInfo("Shutting down uaa3App...");

	cameraManager->StopGrabbingAll();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	// Get the camera IDs from the config manager
	auto enabledCameraIds = cameraConfigManager->GetEnabledCameraIds();

	// Disconnect all enabled cameras
	for (const auto& cameraId : enabledCameraIds) {
		if (cameraManager->DisconnectCamera(cameraId)) {
			logger->LogInfo("Disconnected camera: " + cameraId);
		}
		else {
			logger->LogWarning("Failed to disconnect camera: " + cameraId);
		}
	}

	piControllerManager.get()->DisconnectAll();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	acsControllerManager.get()->DisconnectAll();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	ioManager->stopPolling();
	pneumaticManager->stopPolling();
	ioManager->disconnectAll();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	// Cleanup CLD101x
	if (cld101xManager) {
		cld101xManager->DisconnectAll();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	// Cleanup Keithley2400
	if (keithleyManager) {
		keithleyManager->DisconnectAll();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	// Cleanup ImPlot
	ImPlot::DestroyContext();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	logger->LogInfo("uaa3App finished!");
	return 0;
}


