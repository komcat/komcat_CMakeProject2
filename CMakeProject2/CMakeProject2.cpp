#include <algorithm> // For std::min and std::max 
#include "CMakeProject2.h"
#include "include/client_manager.h" // Replace tcp_client.h with our new manager
#include "include/logger.h" // Include our new logger header
#include "include/motions/MotionTypes.h"  // Add this line - include MotionTypes.h first
#include "include/motions/MotionConfigEditor.h" // Include our new editor header
#include "include/motions/MotionConfigManager.h"
#include "include/motions/pi_controller.h" // Include the PI controller header
#include "include/motions/pi_controller_manager.h" // Include the PI controller manager header
#include "include/ui/toolbar.h" // Include the toolbar header
#include "include/ui/ToolbarMenu.h"
#include "include/ui/GraphVisualizer.h"
#include "include/ui/ToggleableUIAdapter.h" // Basic adapter
#include "include/ui/ControllerAdapters.h"  // Custom adapters for controllers
#include "include/camera/pylon_camera_test.h" // Include the Pylon camera test header


// Add these includes at the top with the other include statements
#include "include/motions/acs_controller.h"
#include "include/motions/acs_controller_manager.h"
#include "include/motions/motion_control_layer.h" // Add this include for the motion control layer

// ImGui and SDL includes
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

// Include SDL2
#include <SDL.h>
#include <SDL_opengl.h>

#include "include/eziio/EziIO_Manager.h"
#include "include/eziio/EziIO_UI.h"
#include "include/eziio/PneumaticManager.h"
#include "include/eziio/PneumaticUI.h"
#include "IOConfigManager.h"
#include "include/data/data_client_manager.h"  // Add this at the top with other includes
#include "include/motions/pi_analog_reader.h"
#include "include/motions/pi_analog_manager.h"
#include "include/data/product_config_manager.h"

int main(int argc, char* argv[])
{
	// Setup SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
	{
		printf("Error: %s\n", SDL_GetError());
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

	// OR for borderless fullscreen at desktop resolution (often preferred):
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
		SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_FULLSCREEN_DESKTOP);

	SDL_Window* window = SDL_CreateWindow("Random Number Generator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, window_flags);
	if (window == nullptr)
	{
		printf("Error creating window: %s\n", SDL_GetError());
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
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

	// Setup Dear ImGui style
	//ImGui::StyleColorsDark();
	ImGui::StyleColorsLight();

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init("#version 130");


	// Get the logger instance
	Logger* logger = Logger::GetInstance();
	logger->Log("Application started");

	//set up toolbarmenu
	ToolbarMenu toolbarMenu;



	// Create our ClientManager instead of a single TcpClient
	ClientManager clientManager;
	logger->Log("ClientManager initialized");

	// For FPS calculation
	float frameTime = 0.0f;
	float fps = 0.0f;
	float fpsUpdateInterval = 0.5f; // Update FPS display every half second
	float fpsTimer = 0.0f;
	int frameCounter = 0;
	Uint64 lastFrameTime = SDL_GetPerformanceCounter();



	// Add camera window instance


	//load motion config json
	 // Create path to the JSON configuration file
	std::string configPath = "motion_config.json";

	// Create the configuration manager and load the file
	MotionConfigManager configManager(configPath);
	// Create the configuration editor
	MotionConfigEditor configEditor(configManager);
	logger->LogInfo("MotionConfigEditor initialized");

	// Create the graph visualizer
	GraphVisualizer graphVisualizer(configManager);
	logger->LogInfo("GraphVisualizer initialized");




	//create PIController
	// Create a controller instance
	//PIController controller;

	// Create the PI Controller Manager
	PIControllerManager piControllerManager(configManager);
	// Connect to all enabled controllers
	if (piControllerManager.ConnectAll()) {
		logger->LogInfo("Successfully connected to all enabled PI controllers");
	}
	else {
		logger->LogWarning("Failed to connect to some PI controllers");
	}
	PIAnalogManager piAnalogManager(piControllerManager, configManager);
	logger->LogInfo("PIAnalogManager initialized");




	// Create the ACS Controller Manager
	ACSControllerManager acsControllerManager(configManager);
	// Connect to all enabled controllers
	if (acsControllerManager.ConnectAll()) {
		logger->LogInfo("Successfully connected to all enabled ACS controllers");
	}
	else {
		logger->LogWarning("Failed to connect to some ACS controllers");
	}

	// Create the Motion Control Layer
	MotionControlLayer motionControlLayer(configManager, piControllerManager, acsControllerManager);
	logger->LogInfo("MotionControlLayer initialized");

	// Set up callbacks for the Motion Control Layer
	motionControlLayer.SetPathCompletionCallback([&logger](bool success) {
		if (success) {
			logger->LogInfo("Path execution completed successfully");
		}
		else {
			logger->LogWarning("Path execution failed or was cancelled");
		}
	});


	// Example of planning and executing a path - uncomment to test
	// This plans a path from the gantry home to the pic position
	/*
	if (motionControlLayer.PlanPath("Process_Flow", "node_9069", "node_9098")) {
		logger->LogInfo("Path planned successfully, starting execution");
		motionControlLayer.ExecutePath(false); // Non-blocking execution
	}
	else {
		logger->LogError("Failed to plan path");
	}
	*/

	// Log the loaded devices
	const auto& devices = configManager.GetAllDevices();
	logger->LogInfo("Loaded " + std::to_string(devices.size()) + " devices");

	for (const auto& [name, device] : devices) {
		std::string deviceInfo = "Device: " + name +
			" (ID: " + std::to_string(device.Id) +
			", IP: " + device.IpAddress +
			", Enabled: " + (device.IsEnabled ? "Yes" : "No") + ")";
		logger->LogInfo(deviceInfo);

		// Log positions for each device
		auto positionsOpt = configManager.GetDevicePositions(name);
		if (positionsOpt.has_value()) {
			const auto& positions = positionsOpt.value().get();
			logger->LogInfo("  Positions: " + std::to_string(positions.size()));

			for (const auto& [posName, pos] : positions) {
				std::string posInfo = "    " + posName + ": (" +
					std::to_string(pos.x) + ", " +
					std::to_string(pos.y) + ", " +
					std::to_string(pos.z);

				// Add rotation values if any are non-zero
				if (pos.u != 0.0 || pos.v != 0.0 || pos.w != 0.0) {
					posInfo += ", " + std::to_string(pos.u) + ", " +
						std::to_string(pos.v) + ", " +
						std::to_string(pos.w);
				}

				posInfo += ")";
				logger->LogInfo(posInfo);
			}
		}
	}


	// Log the loaded graphs
	const auto& graphs = configManager.GetAllGraphs();
	logger->LogInfo("Loaded " + std::to_string(graphs.size()) + " graphs");

	for (const auto& [name, graph] : graphs) {
		logger->LogInfo("Graph: " + name);
		logger->LogInfo("  Nodes: " + std::to_string(graph.Nodes.size()));
		logger->LogInfo("  Edges: " + std::to_string(graph.Edges.size()));
	}

	// Log the settings
	const auto& settings = configManager.GetSettings();
	logger->LogInfo("Settings:");
	logger->LogInfo("  Default Speed: " + std::to_string(settings.DefaultSpeed));
	logger->LogInfo("  Default Acceleration: " + std::to_string(settings.DefaultAcceleration));
	logger->LogInfo("  Log Level: " + settings.LogLevel);

	logger->LogInfo("Configuration loaded successfully");

	// Create our PylonCameraTest instance
	PylonCameraTest pylonCameraTest;
	// In CMakeProject2.cpp, add the following in the main loop


	//initialize EZIIO
	//TODOload jsonconfig.
	//TODO ioconfigjson...
// Create the EziIO manager
	EziIOManager ioManager;

	// Initialize the manager
	if (!ioManager.initialize()) {
		std::cerr << "Failed to initialize EziIO manager" << std::endl;
		return 1;
	}
	// Initialize and load the IO configuration
	IOConfigManager ioconfigManager;
	if (!ioconfigManager.loadConfig("IOConfig.json")) {
		std::cerr << "Failed to load IO configuration, using default settings" << std::endl;
	}

	// Setup devices from config
	ioconfigManager.initializeIOManager(ioManager);





	// 3. Create and configure the Pneumatic Manager
	PneumaticManager pneumaticManager(ioManager);
	if (!ioconfigManager.initializePneumaticManager(pneumaticManager)) {
		logger->LogWarning("Failed to initialize pneumatic manager");
	}
	// 4. Initialize the pneumatic manager and start status polling
	pneumaticManager.initialize();
	pneumaticManager.startPolling(50); // 50ms polling interval
	// 5. Create the pneumatic UI
	PneumaticUI pneumaticUI(pneumaticManager);
	logger->LogInfo("Pneumatic control system initialized");

	// Optional: Register for state change notifications
	pneumaticManager.setStateChangeCallback([&logger](const std::string& slideName, SlideState state) {
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


	// Connect to all devices
	if (!ioManager.connectAll()) {
		std::cerr << "Failed to connect to all devices" << std::endl;
		// Continue anyway
	}
	// Start the polling thread - 100ms refresh rate
	ioManager.startPolling(100);
	std::cout << "Status polling started in background thread" << std::endl;

	// Create the EziIO UI
	EziIO_UI ioUI(ioManager);
	//TODO UI take ioconfigjson information about pin name and show on the status table.

	// Set the config manager to enable named pins in the UI
	ioUI.setConfigManager(&ioconfigManager);
	if (ioUI.IsVisible())
	{
		ioUI.ToggleWindow();  // or similar method to hide the window
		logger->LogInfo("EziIO UI initialized");
	}



	// In the main function, after initializing the ClientManager:
// Create the DataClientManager with the config file path
	DataClientManager dataClientManager("DataServerConfig.json");
	logger->LogInfo("DataClientManager initialized");




	// Create the ProductConfigManager instance
	ProductConfigManager productConfigManager(configManager);



	// Add components with standard methods
	toolbarMenu.AddReference(CreateTogglableUI(configEditor, "Config Editor"));
	toolbarMenu.AddReference(CreateTogglableUI(graphVisualizer, "Graph Visualizer"));
	toolbarMenu.AddReference(CreateTogglableUI(ioUI, "IO Control"));
	toolbarMenu.AddReference(CreateTogglableUI(pneumaticUI, "Pneumatic"));
	toolbarMenu.AddReference(CreateTogglableUI(dataClientManager, "Data TCP/IP"));

	// Add controller managers using our custom adapters
	toolbarMenu.AddReference(CreateACSControllerAdapter(acsControllerManager, "Gantry"));
	toolbarMenu.AddReference(CreatePIControllerAdapter(piControllerManager, "PI"));
	// Add PIAnalogManager as a toggleable UI component
	toolbarMenu.AddReference(std::shared_ptr<ITogglableUI>(&piAnalogManager));
	toolbarMenu.AddReference(CreateTogglableUI(productConfigManager, "Products Config"));
	// Log successful initialization
	logger->LogInfo("ToolbarMenu initialized with " +
		std::to_string(toolbarMenu.GetComponentCount()) +
		" components");

// Main loop
	bool done = false;
	bool cameraDisconnectedWarningShown = false;  // To track if we've shown a notification
	while (!done)
	{
		// Poll and handle events
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT)
				done = true;
			if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
				done = true;
		}

		// Calculate delta time and FPS
		Uint64 currentFrameTime = SDL_GetPerformanceCounter();
		float deltaTime = (currentFrameTime - lastFrameTime) / (float)SDL_GetPerformanceFrequency();
		lastFrameTime = currentFrameTime;

		// Update frame counter
		frameCounter++;
		fpsTimer += deltaTime;

		// Update FPS calculation every update interval
		if (fpsTimer >= fpsUpdateInterval) {
			fps = frameCounter / fpsTimer;
			frameCounter = 0;
			fpsTimer = 0;
		}

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		// Add an exit button in a fixed position
		ImGui::SetNextWindowPos(ImVec2(105, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(200, 60), ImGuiCond_Always); // Increased width to accommodate both buttons
		ImGui::SetNextWindowBgAlpha(0.7f); // Semi-transparent background
		ImGui::Begin("Exit", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

		// Add minimize button
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f)); // Blue button
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.3f, 0.7f, 1.0f));
		if (ImGui::Button("Minimize", ImVec2(80, 40))) {
			// Call SDL function to minimize the window
			SDL_MinimizeWindow(window);
		}
		ImGui::PopStyleColor(3);

		ImGui::SameLine();

		// Style the exit button to be more noticeable
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); // Red button
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
		// Make the button fill most of the window
		if (ImGui::Button("Exit", ImVec2(80, 40))) {
			// Your existing exit code...
			done = true;
		}
		ImGui::PopStyleColor(3); // Remove the 3 style colors we pushed
		ImGui::End();


		// Create a performance overlay
		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
		ImGui::Begin("Performance", nullptr,
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoMove);
		ImGui::Text("FPS: %.1f", fps);

		if (enableDebug)	logger->LogInfo("FPS: " + std::to_string(fps));

		ImGui::End();

		// Render logger UI
		logger->RenderUI();







		// Update all TCP clients
		clientManager.UpdateClients();

		// Render TCP client manager UI
		clientManager.RenderUI();

		// Add this line before the FPS display window rendering
		//toolbar.RenderUI();
		// Render in main loop
		toolbarMenu.RenderUI();

		// Render motion configuration editor UI
		configEditor.RenderUI();
		graphVisualizer.RenderUI();



		//PI hexapod
		//controller.RenderUI();
		piControllerManager.RenderUI();
		piAnalogManager.RenderUI();

		// Add this to render each controller's individual UI window
		// This is necessary because they won't render automatically
		for (const auto& [name, device] : configManager.GetAllDevices()) {
			// Check if it's a PI device
			if (device.Port == 50000 && device.IsEnabled) {
				PIController* controller = piControllerManager.GetController(name);
				if (controller && controller->IsConnected()) {
					controller->RenderUI();
				}
			}
		}

		// Render ACS controller manager UI
		acsControllerManager.RenderUI();

		// Add this to render each controller's individual UI window
		// This is necessary because they won't render automatically
		for (const auto& [name, device] : configManager.GetAllDevices()) {
			// Check if it's an ACS device (non-PI port)
			if (device.Port != 50000 && device.IsEnabled) {
				ACSController* controller = acsControllerManager.GetController(name);
				if (controller && controller->IsConnected()) {
					controller->RenderUI();
				}
			}
		}

		// Render the Motion Control Layer UI
		motionControlLayer.RenderUI();


		// Render Pylon Camera Test UI
		pylonCameraTest.RenderUI();



		// Render the EziIO UI
		ioUI.RenderUI();
		// In the main rendering loop
		pneumaticUI.RenderUI();

		// In the main loop, add this before your SDL_GL_SwapWindow(window) call:
// Update the DataClientManager
		dataClientManager.UpdateClients();
		// Render DataClientManager UI
		dataClientManager.RenderUI();

		//product config manager
		productConfigManager.RenderUI();


		// Rendering
		ImGui::Render();
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window);
	}
	// When exit is triggered:
	logger->Log("Application shutting down");
	pylonCameraTest.GetCamera().StopGrabbing();
	pylonCameraTest.GetCamera().Disconnect();
	if (pylonCameraTest.GetCamera().IsCameraDeviceRemoved())
	{
		std::cout << "Camera device removed" << std::endl;
	}
	else {
		std::cout << "Camera device not removed" << std::endl;
	}




	try
	{
		piAnalogManager.ToggleWindow();
		piAnalogManager.stopPolling();

		// Clean up reader resources
		piAnalogManager.cleanupReaders();

		// Clear the analog manager first (it depends on controller managers)
				// Give a short delay to ensure all background operations complete
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		// Or delete piAnalogManager; // If using raw pointer
		//disconnect controller
		piControllerManager.DisconnectAll();
		acsControllerManager.DisconnectAll();


	
	}
	catch (const std::exception& e) {
		std::cout << "Exception during shutdown: " << e.what() << std::endl;
	}
	catch (...) {
		std::cout << "Unknown exception during shutdown" << std::endl;
	}



	// Before application exit
	pneumaticManager.stopPolling();
	// Stop the polling thread
	ioManager.stopPolling();

	// Disconnect from all devices
	ioManager.disconnectAll();



	
	

	// Cleanup - keep outside try-catch to ensure it always happens
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();



	return 0;
}


