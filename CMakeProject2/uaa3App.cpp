#include <iostream>
#include <SDL.h>
#include <SDL_opengl.h>

// ImGui includes
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"
#include "implot/implot.h"

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
#include "motion_ops.h"
#include "io_ops.h"
#include "vision_ops.h"
#include "Version.h"

#include "raylibclass.h"

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

int main(int argc, char* argv[])
{
	// Get the logger instance
	Logger* logger = Logger::GetInstance();
	logger->LogInfo("Hello World from uaa3App!");

	GlobalDataStore* dataStore = GlobalDataStore::GetInstance();

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


#pragma region Font Loading



	// Font loading code with Greek support for μ symbol
	static const ImWchar ranges[] = {
		0x0020, 0x00FF, // Basic Latin + Latin Supplement  
		0x0370, 0x03FF, // Greek and Coptic (includes μ at 0x03BC)
		0x2200, 0x22FF, // Mathematical Operators (JuliaMono has lots of math symbols)
		0,
	};

	bool fontLoaded = false;

	// Try bundled font first (put in your project directory)
	ImFont* font = io.Fonts->AddFontFromFileTTF("assets/fonts/JuliaMono-Regular.ttf", 16.0f, NULL, ranges);
	if (font != NULL) {
		logger->LogInfo("Loaded bundled Noto Sans font with Greek support");
		fontLoaded = true;
	}
	else {
		// Fallback to system fonts
		const char* systemFonts[] = {
			"C:/Windows/Fonts/times.ttf",
			"C:/Windows/Fonts/calibri.ttf",
			"C:/Windows/Fonts/segoeui.ttf",
		};

		for (const char* fontPath : systemFonts) {
			font = io.Fonts->AddFontFromFileTTF(fontPath, 16.0f, NULL, ranges);
			if (font != NULL) {
				logger->LogInfo("Loaded system font: " + std::string(fontPath));
				fontLoaded = true;
				break;
			}
		}
	}

	if (!fontLoaded) {
		// Final fallback
		io.Fonts->AddFontDefault();
		logger->LogWarning("No Greek font support available, using 'um' instead of 'μm'");
	}

	// Build font atlas BEFORE checking glyphs
	bool buildSuccess = io.Fonts->Build();
	if (!buildSuccess) {
		logger->LogError("Failed to build font atlas");
	}

#pragma endregion

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

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

	// Camera initialization
	std::unique_ptr<CameraManager> cameraManager = std::make_unique<CameraManager>();

	//// Camera 1 - Auto-connect to first available
	//CameraInfo camera1("main_camera", "Top view camera");
	//cameraManager->AddCamera(camera1);

	//// Initialize all cameras with auto-connect enabled
	//cameraManager->InitializeAllCameras();

	// In your main application, try this:
	auto camera1 = CameraInfo::CreateByIP("main_camera", "192.168.0.68", "Top view camera");
	cameraManager->AddCamera(camera1);
	auto camera1 = CameraInfo::CreateByIP("aux_camera", "192.168.0.69", "Auxilary Camera");
	cameraManager->AddCamera(camera1);

	cameraManager->InitializeAllCameras();



	CheckCameraStatus(*cameraManager);








	// Initialize TCP Data Client Manager
	std::unique_ptr<DataClientManager> dataClientManager;
	try {
		dataClientManager = std::make_unique<DataClientManager>("DataServerConfig.json");
		dataClientManager->ConnectAutoClients();
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



	// Initialize RaylibWindow (add this section after machineOps creation)
	std::unique_ptr<RaylibWindow> raylibWindow;

	// Check if we should enable the 3D window (you can add this to a config file later)
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

		// Connect MachineOperations if available
		if (machineOps) {
			raylibWindow->SetMachineOperations(machineOps.get());
			logger->LogInfo("Connected MachineOperations to raylib window");
		}

		// Connect camera if available
		if (cameraManager && cameraManager->GetCameraCount() > 0) {
			// Assuming you want to connect the first camera for video feed
			auto cameraIds = cameraManager->GetCameraIds();
			if (!cameraIds.empty()) {
				PylonCameraTest* firstCamera = cameraManager->GetCamera(cameraIds[0]);
				if (firstCamera) {
					// You'll need to add this method to your PylonCameraTest or use the existing pattern
					// from CMakeProject2 where it calls SetRaylibWindow
					logger->LogInfo("Connected camera to raylib window for video feed");
				}
			}
		}

		// THEN initialize the thread
		if (raylibWindow->Initialize()) {
			logger->LogInfo("Raylib 3D Window thread started successfully");
		}
		else {
			logger->LogError("Failed to start Raylib 3D Window");
			raylibWindow.reset(); // Clean up on failure
		}
	}


	bool done = false;
	bool glyphChecked = false;

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
		}

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();



		// Update DataClientManager continuously in background
		if (dataClientManager) {
			dataClientManager->UpdateClients();
		}


		

		// Render the main UI
		uiManager.RenderUI();


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

	cameraManager->DisconnectCamera(camera1.id);

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