#include <iostream>
#include <SDL.h>
#include <SDL_opengl.h>

// ImGui includes
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

// Include our UI manager and config manager
#include "MainUIManager.h"
#include "include/motions/MotionConfigManager.h"
#include "include/motions/pi_controller_manager.h"
#include "include/motions/pi_controller.h"
#include "include/motions/acs_controller_manager.h"
#include "include/motions/acs_controller.h"
#include "include/logger.h" // Include our new logger header
#include "include/eziio//EziIO_Manager.h"
#include "include/eziio/PneumaticManager.h"
#include "IOConfigManager.h"
#include "include/camera/CameraManager.h"


// Create a global camera manager instance
CameraManager g_cameraManager;

// Example: Check camera status
void CheckCameraStatus() {
	auto statusList = g_cameraManager.GetAllCameraStatus();

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
	SDL_Window* window = SDL_CreateWindow("uaa3App - Motion Configuration",
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
		// Create with a default path or handle gracefully
		motionConfigManager = std::make_unique<MotionConfigManager>("default_config.json");
	}

	// PI Controller Manager (conditional)
	std::unique_ptr<PIControllerManager> piControllerManager;
	//if (moduleConfig.isEnabled("PI_CONTROLLERS")) {

	piControllerManager = std::make_unique<PIControllerManager>(*motionConfigManager);
	if (piControllerManager->ConnectAll()) {
		logger->LogInfo("Successfully connected to all enabled PI controllers");
	}
	else {
		logger->LogWarning("Failed to connect to some PI controllers");
	}

	std::unique_ptr<ACSControllerManager> acsControllerManager;
	acsControllerManager = std::make_unique<ACSControllerManager>(*motionConfigManager);
	if (acsControllerManager->ConnectAll()) {
		logger->LogInfo("Successfully connected to all enabled ACS controllers");
	}
	else {
		logger->LogWarning("Failed to connect to some ACS controllers");
	}


	// Read all velocity settings for PI controllers
	if (piControllerManager) {
		logger->LogInfo("Reading current velocity settings for PI controllers...");

		// Get all PI controller names from config
		std::vector<std::string> piControllerNames = { "hex-left", "hex-right", "hex-bottom" };

		for (const std::string& controllerName : piControllerNames) {
			PIController* controller = piControllerManager->GetController(controllerName);

			if (controller && controller->IsConnected()) {
				logger->LogInfo("Reading velocities for controller: " + controllerName);

				// Get available axes for this controller
				const auto& axes = controller->GetAvailableAxes();

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

	// Read all velocity settings for ACS controllers
	if (acsControllerManager) {
		logger->LogInfo("Reading current velocity settings for ACS controllers...");

		// Get all ACS controller names from config
		std::vector<std::string> acsControllerNames = { "gantry-main" };

		for (const std::string& controllerName : acsControllerNames) {
			ACSController* controller = acsControllerManager->GetController(controllerName);

			if (controller && controller->IsConnected()) {
				logger->LogInfo("Reading velocities for controller: " + controllerName);

				// Typical ACS gantry axes
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


	//initialize EziIO system
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


	//Initialize Pneumatic System if enabled
	// Pneumatic System (conditional)
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

	//Camera initialization
	// Camera 1 - Auto-connect to first available
	CameraInfo camera1("main_camera", "Top view camera");
	g_cameraManager.AddCamera(camera1);

	// Initialize all cameras with auto-connect enabled
	g_cameraManager.InitializeAllCameras();

	CheckCameraStatus();
	// Start grabbing on all connected cameras
	//g_cameraManager.StartGrabbingAll();

	


	// ✅ Create the main UI manager with just the config manager
	MainUIManager uiManager(*motionConfigManager);
	logger->LogInfo("MainUIManager created with MotionConfigManager");

	// TODO: Later, when you have motion managers, you can add them like this:
	// std::unique_ptr<PIControllerManager> piManager = std::make_unique<PIControllerManager>(*motionConfigManager);
	// std::unique_ptr<ACSControllerManager> acsManager = std::make_unique<ACSControllerManager>(*motionConfigManager);
	// uiManager.SetMotionManagers(piManager.get(), acsManager.get());

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

	if (g_cameraManager.GetCameraCount() > 0) {
		uiManager.SetCameraManager(&g_cameraManager);
	}


	bool done = false;
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

		// ✅ Render the main UI
		uiManager.RenderUI();

		// Rendering
		ImGui::Render();
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		SDL_GL_SwapWindow(window);
	}

	// Cleanup
	logger->LogInfo("Shutting down uaa3App...");

	g_cameraManager.StopGrabbingAll();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	g_cameraManager.DisconnectCamera(camera1.id);

	piControllerManager.get()->DisconnectAll();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	acsControllerManager.get()->DisconnectAll();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	ioManager->stopPolling();
	pneumaticManager->stopPolling();
	ioManager->disconnectAll();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));




	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	logger->LogInfo("uaa3App finished!");
	return 0;
}

