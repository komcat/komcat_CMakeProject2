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
#include "include/motions/global_jog_panel.h"
#include "include/data/product_config_manager.h"
#include "include/eziio/IOControlPanel.h"
// Add these includes at the top with the other include statements
#include "include/cld101x_client.h"
#include "include/cld101x_manager.h"
#include "include/python_process_managaer.h"
// Add these includes at the top with the other include statements
#include "include/scanning/scanning_ui.h"
#include "include/scanning/scanning_algorithm.h"
#include "include/scanning/scanning_parameters.h"
#include "include/scanning/scan_data_collector.h"
#include "include/data/global_data_store.h" // Add this with your other includes
#include "implot/implot.h"
#include "include/data/DataChartManager.h"
#include "include/SequenceStep.h"
#include "include/machine_operations.h"
#include "InitializationWindow.h"
//#include "include/HexRightControllerWindow.h"
#include "include/HexControllerWindow.h"
#include <iostream>
#include <memory>

// Example of creating and using the initialization process
void RunInitializationProcess(MachineOperations& machineOps) {
	// Method 1: Using the dedicated InitializationStep class
	std::cout << "Running initialization using dedicated class..." << std::endl;

	auto initStep = std::make_unique<InitializationStep>(machineOps);

	// Set completion callback
	initStep->SetCompletionCallback([](bool success) {
		std::cout << "Initialization " << (success ? "succeeded" : "failed") << std::endl;
	});

	// Execute the step
	initStep->Execute();

	// Method 2: Using the more flexible SequenceStep class
	std::cout << "\nRunning initialization using sequence step..." << std::endl;

	auto sequenceStep = std::make_unique<SequenceStep>("Initialization", machineOps);

	// Add operations in sequence
	sequenceStep->AddOperation(std::make_shared<MoveToNodeOperation>(
		"gantry-main", "Process_Flow", "node_4027"));

	sequenceStep->AddOperation(std::make_shared<MoveToNodeOperation>(
		"hex-left", "Process_Flow", "node_5480"));

	sequenceStep->AddOperation(std::make_shared<MoveToNodeOperation>(
		"hex-right", "Process_Flow", "node_5136"));

	sequenceStep->AddOperation(std::make_shared<SetOutputOperation>(
		"IOBottom", 0, false)); // L_Gripper OFF

	sequenceStep->AddOperation(std::make_shared<SetOutputOperation>(
		"IOBottom", 2, false)); // R_Gripper OFF

	sequenceStep->AddOperation(std::make_shared<SetOutputOperation>(
		"IOBottom", 10, true)); // Vacuum_Base ON

	// Set completion callback
	sequenceStep->SetCompletionCallback([](bool success) {
		std::cout << "Sequence " << (success ? "succeeded" : "failed") << std::endl;
	});

	// Execute the step
	sequenceStep->Execute();
}

// Example of how to create a custom process step with specific behavior
class CustomProcessStep : public ProcessStep {
public:
	CustomProcessStep(MachineOperations& machineOps)
		: ProcessStep("CustomProcess", machineOps) {
	}

	bool Execute() override {
		LogInfo("Starting custom process");

		// Example of a more complex process with conditional logic

		// 1. Move gantry to safe position
		if (!m_machineOps.MoveDeviceToNode("gantry-main", "Process_Flow", "node_4027", true)) {
			LogError("Failed to move gantry to safe position");
			NotifyCompletion(false);
			return false;
		}

		// 2. Read a sensor to determine next step
		bool sensorState = false;
		if (!m_machineOps.ReadInput("IOBottom", 5, sensorState)) {
			LogError("Failed to read sensor");
			NotifyCompletion(false);
			return false;
		}

		// 3. Conditional branching based on sensor state
		if (sensorState) {
			LogInfo("Sensor active, moving to position A");
			if (!m_machineOps.MoveDeviceToNode("hex-left", "Process_Flow", "node_5557", true)) {
				LogError("Failed to move to position A");
				NotifyCompletion(false);
				return false;
			}
		}
		else {
			LogInfo("Sensor inactive, moving to position B");
			if (!m_machineOps.MoveDeviceToNode("hex-left", "Process_Flow", "node_5620", true)) {
				LogError("Failed to move to position B");
				NotifyCompletion(false);
				return false;
			}
		}

		LogInfo("Custom process completed successfully");
		NotifyCompletion(true);
		return true;
	}
};





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
	// ADD THIS LINE RIGHT AFTER:
	ImPlot::CreateContext();  // Initialize ImPlot

	// Get the logger instance
	Logger* logger = Logger::GetInstance();
	logger->Log("Application started");


	// Create Python Process Manager and start scripts
	PythonProcessManager pythonManager;

	// Start Python scripts
	if (!pythonManager.StartCLD101xServer()) {
		logger->LogWarning("Failed to start CLD101x server script, will continue without it");
	}
	else {
		logger->LogInfo("CLD101x server script started successfully");
	}

	//if (!pythonManager.StartKeithleyScript()) {
	//	logger->LogWarning("Failed to start Keithley script, will continue without it");
	//}
	//else {
	//	logger->LogInfo("Keithley script started successfully");
	//}






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

	// Create the Global Data Store if not already created
	GlobalDataStore& dataStore = *GlobalDataStore::GetInstance();
	logger->LogInfo("GlobalDataStore initialized");

	// Create the Scanning UI
// Create the Hexapod Optimization UI
	ScanningUI hexapodScanningUI(piControllerManager, dataStore);
	logger->LogInfo("Hexapod Scanning UI initialized");

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
	// Create our IO Control Panel for quick output control
	IOControlPanel ioControlPanel(ioManager);
	logger->LogInfo("IOControlPanel initialized for quick output control");




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

	// Create the CLD101x manager
	CLD101xManager cld101xManager;
	logger->LogInfo("CLD101xManager initialized");

	// Initialize the manager (it will create a default client)
	cld101xManager.Initialize();

	// Later in the initialization section:
	GlobalJogPanel globalJogPanel(configManager, piControllerManager, acsControllerManager);
	logger->LogInfo("GlobalJogPanel initialized");



	// Then in the initialization section, add:

	DataChartManager dataChartManager;
	dataChartManager.Initialize();

	// Create the window with a reference to the controller manager
	//HexRightControllerWindow hexRightWindow(piControllerManager);
	//logger->LogInfo("HexRightControllerWindow created");
	// Create the window with a reference to the controller manager
	HexControllerWindow hexControllerWindow(piControllerManager);
	logger->LogInfo("HexControllerWindow created");

	// Add the channels you want to monitor
	dataChartManager.AddChannel("GPIB-Current", "Current Reading", "A", false);
	//dataChartManager.AddChannel("Virtual_1", "Virtual Channel 1", "unit", true);
	//dataChartManager.AddChannel("Virtual_2", "Virtual Channel 2", "unit", true);
	//dataChartManager.AddChannel("hex-left-A-5", "Voltage L5", "unit", true); 
	//dataChartManager.AddChannel("hex-left-A-6", "Voltage L6", "unit", true);
	dataChartManager.AddChannel("hex-right-A-5", "Voltage R5", "unit", true);
	//dataChartManager.AddChannel("hex-right-A-6", "Voltage R6", "unit", true);

	// Add components with standard methods

	toolbarMenu.AddReference(CreateTogglableUI(hexapodScanningUI, "Scanning Optimizer"));
	toolbarMenu.AddReference(CreateTogglableUI(globalJogPanel, "Global Jog Panel"));
	toolbarMenu.AddReference(CreateTogglableUI(dataChartManager, "Data Chart"));
	toolbarMenu.AddReference(CreateTogglableUI(dataClientManager, "Data TCP/IP"));
	toolbarMenu.AddReference(CreateTogglableUI(ioControlPanel, "IO Quick Control"));
	// Add to toolbar menu
	toolbarMenu.AddReference(CreateTogglableUI(cld101xManager, "Laser TEC Cntrl"));
	toolbarMenu.AddReference(CreateTogglableUI(pneumaticUI, "Pneumatic"));


	// Add controller managers using our custom adapters
	toolbarMenu.AddReference(CreateACSControllerAdapter(acsControllerManager, "Gantry"));
	toolbarMenu.AddReference(CreatePIControllerAdapter(piControllerManager, "PI"));
	// Add PIAnalogManager as a toggleable UI component
	toolbarMenu.AddReference(std::shared_ptr<ITogglableUI>(&piAnalogManager));

	// Add the IOControlPanel using the same CreateTogglableUI adapter

	// Add to toolbar menu

	toolbarMenu.AddReference(CreateTogglableUI(ioUI, "IO Control"));
	toolbarMenu.AddReference(CreateTogglableUI(configEditor, "Config Editor"));
	toolbarMenu.AddReference(CreateTogglableUI(graphVisualizer, "Graph Visualizer"));
	toolbarMenu.AddReference(CreateTogglableUI(productConfigManager, "Products Config"));
	// Add it to the toolbar menu
	//toolbarMenu.AddReference(CreateTogglableUI(hexRightWindow, "Hex-Right"));


	// Add it to the toolbar menu
	toolbarMenu.AddReference(CreateTogglableUI(hexControllerWindow, "Hex Controllers"));
	// Log successful initialization
	logger->LogInfo("ToolbarMenu initialized with " +
		std::to_string(toolbarMenu.GetComponentCount()) +
		" components");


	// Create the machine operations object
	MachineOperations machineOps(
		motionControlLayer,
		piControllerManager,
		ioManager,
		pneumaticManager
	);


	// Run the initialization process
	// Create the initialization window
	InitializationWindow initWindow(machineOps);

	// Create and run a custom process step
	//auto customStep = std::make_unique<CustomProcessStep>(machineOps);
	//customStep->Execute();



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

			//process key events
			if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
				globalJogPanel.ProcessKeyInput(
					event.key.keysym.sym,
					event.type == SDL_KEYDOWN
				);
			}
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
		// Render IO Control Panel UI
		ioControlPanel.RenderUI();
		// Render CLD101xManager UI
		cld101xManager.RenderUI();



		// In your main loop where you render UIs:
		globalJogPanel.RenderUI();


		RenderSimpleChart();
		
		dataChartManager.Update();
		dataChartManager.RenderUI();
		RenderValueDisplay();
		hexapodScanningUI.RenderUI();

		// Render our UI windows
		initWindow.RenderUI();
		//hexRightWindow.RenderUI();
		hexControllerWindow.RenderUI();

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



	// Stop Python scripts before other cleanup
	logger->LogInfo("Stopping Python processes...");
	pythonManager.StopAllProcesses();





	pylonCameraTest.GetCamera().StopGrabbing();
	pylonCameraTest.GetCamera().Disconnect();
	if (pylonCameraTest.GetCamera().IsCameraDeviceRemoved())
	{
		std::cout << "Camera device removed" << std::endl;
	}
	else {
		std::cout << "Camera device not removed" << std::endl;
	}

	// Disconnect all CLD101x clients
	cld101xManager.DisconnectAll();


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
	// ADD THIS LINE RIGHT BEFORE:
	ImPlot::DestroyContext();  // Clean up ImPlot
	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();



	return 0;
}


void RenderValueDisplay() {
	// Access values from the global store
	float currentValue = GlobalDataStore::GetInstance()->GetValue("GPIB-Current");

	// Create a window to display the value
	ImGui::Begin("Current Reading");

	// Format and display the value with appropriate unit
	ImGui::Text("Current: %.4f A", currentValue);

	ImGui::End();
}

// Simple ImPlot test with 10 data points
// Add this to your main loop
// Add this function where ImGui is initialized (before the main loop)
void InitializeImPlot() {
	// Create the ImPlot context
	ImPlot::CreateContext();
	Logger::GetInstance()->LogInfo("ImPlot context created successfully");
}

// Add this function where ImGui is shut down
void ShutdownImPlot() {
	// Destroy the ImPlot context
	ImPlot::DestroyContext();
	Logger::GetInstance()->LogInfo("ImPlot context destroyed");
}
void RenderSimpleChart() {
	try {
		// Create static data for the chart (only initialize once)
		static float x_data[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
		static float y_data[10] = { 0, 1, 4, 9, 16, 25, 36, 49, 64, 81 }; // y = x^2
		static bool initialized = false;

		// You could also create animated data that changes each frame
		static float animated_data[10];
		static float time = 0.0f;

		// Update the animated data each frame
		time += ImGui::GetIO().DeltaTime;
		for (int i = 0; i < 10; i++) {
			animated_data[i] = sinf(x_data[i] * 0.5f + time) * 5.0f;
		}

		// Create a window
		ImGui::Begin("Simple ImPlot Test");

		ImGui::Text("Testing ImPlot with simple arrays");

		// Create a plot inside the window
		if (ImPlot::BeginPlot("Simple Plot")) {
			// Plot the static data (x² function)
			ImPlot::PlotLine("x²", x_data, y_data, 10);

			// Plot the animated data (sine wave)
			ImPlot::PlotLine("sin(x)", x_data, animated_data, 10);

			ImPlot::EndPlot();
		}

		ImGui::End();
	}
	catch (const std::exception& e) {
		ImGui::Begin("Error");
		ImGui::Text("Exception in RenderSimpleChart: %s", e.what());
		ImGui::End();
	}
	catch (...) {
		ImGui::Begin("Error");
		ImGui::Text("Unknown exception in RenderSimpleChart");
		ImGui::End();
	}
}

// Then in your main loop, add:
// RenderSimpleChart();