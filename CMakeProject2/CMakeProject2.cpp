#include <algorithm> // For std::min and std::max 
#include "CMakeProject2.h"
#include "include/client_manager.h" // Replace tcp_client.h with our new manager
#include "include/logger.h" // Include our new logger header
//#include "MotionTypes.h"  // Add this line - include MotionTypes.h first
#include "include/motions/MotionConfigEditor.h" // Include our new editor header
#include "include/motions/MotionConfigManager.h"
#include "include/ui/toolbar.h" // Include the toolbar header
#include "include/motions/acs_monitor.h"
#include "include/ui/GraphVisualizer.h"
#include "include/camera/pylon_camera_test.h" // Include the Pylon camera test header
#include "include/motions/pi_controller.h" // Include the PI controller header
#include "include/motions/pi_controller_manager.h" // Include the PI controller manager header

// ImGui and SDL includes
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

// Include SDL2
#include <SDL.h>
#include <SDL_opengl.h>



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

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
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
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init("#version 130");


	// Get the logger instance
	Logger* logger = Logger::GetInstance();
	logger->Log("Application started");






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

	//ACS controller
	ACSMonitor acsMonitor;
	logger->LogInfo("ACSMonitor initialized");

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

	// Update toolbar initialization to include the graph visualizer
	// Replace your existing toolbar initialization with:
	Toolbar toolbar(configEditor, graphVisualizer);
	logger->LogInfo("Toolbar initialized with GraphVisualizer support");


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

	// Connect to a specific device
	//if (controller.Connect("192.168.0.30", 50000)) {
	//	std::cout << "Successfully connected to PI controller" << std::endl;
	//}
	//else {
	//	std::cout << "Failed to connect to PI controller" << std::endl;
	//}

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
		ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 120, 100), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(110, 60), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.7f); // Semi-transparent background
		ImGui::Begin("Exit", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
		// Style the exit button to be more noticeable
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); // Red button
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
		// Make the button fill most of the window
		if (ImGui::Button("Exit Safely", ImVec2(100, 40))) {
			// Perform safe shutdown of camera resources
			std::cout << "Safe exit initiated..." << std::endl;



			// Access your PylonCameraTest instance
			// Assuming you have a PylonCameraTest instance in your main loop named pylonCameraTest

			// Safely stop camera operations in PylonCameraTest
			// This will access the underlying PylonCamera instance and clean up resources
			if (pylonCameraTest.GetCamera().IsGrabbing()) {
				std::cout << "Stopping camera grabbing..." << std::endl;
				pylonCameraTest.GetCamera().StopGrabbing();
			}

			if (pylonCameraTest.GetCamera().IsConnected()) {
				std::cout << "Disconnecting camera..." << std::endl;
				pylonCameraTest.GetCamera().Disconnect();
			}

			// Add a small delay to ensure camera operations have completed
			SDL_Delay(200);

			// Finally terminate Pylon library safely
			// You might need to add a static method in your PylonCamera class for this


			// Signal the main loop to exit
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
		
		if(enableDebug)	logger->LogInfo("FPS: " + std::to_string(fps));

		ImGui::End();

		// Render logger UI
		logger->RenderUI();



		//// Camera disconnection popup
		//if (ImGui::BeginPopupModal("Camera Disconnected", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
		//	ImGui::Text("The camera has been physically disconnected!");
		//	ImGui::Text("Reconnect the device and use the 'Try Reconnect' button in the camera window.");

		//	if (ImGui::Button("OK", ImVec2(120, 0))) {
		//		ImGui::CloseCurrentPopup();
		//	}
		//	ImGui::EndPopup();
		//}





		// Update all TCP clients
		clientManager.UpdateClients();

		// Render TCP client manager UI
		clientManager.RenderUI();

		// Add this line before the FPS display window rendering
		toolbar.RenderUI();
		// Render motion configuration editor UI
		configEditor.RenderUI();
		graphVisualizer.RenderUI();
		

		//ACS connection
		acsMonitor.RenderUI();

		//PI hexapod
		//controller.RenderUI();
		piControllerManager.RenderUI();


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

		// Render Pylon Camera Test UI
		pylonCameraTest.RenderUI();



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



	// Cleanup - keep outside try-catch to ensure it always happens
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();


	
	return 0;
}