#include <algorithm> // For std::min and std::max 
#include "CMakeProject2.h"
#include "randomwindow.h"
#include "client_manager.h" // Replace tcp_client.h with our new manager
#include "camera_window.h" // Add this line to include the camera window header
#include "logger.h" // Include our new logger header
//#include "MotionTypes.h"  // Add this line - include MotionTypes.h first
#include "MotionConfigEditor.h" // Include our new editor header
#include "MotionConfigManager.h"
#include "toolbar.h" // Include the toolbar header
#include "acs_monitor.h"
#include "GraphVisualizer.h"
#include "DraggableNode.h"




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

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init("#version 130");


	// Get the logger instance
	Logger* logger = Logger::GetInstance();
	logger->Log("Application started");





	// Create our RandomWindow instance
	RandomWindow randomWindow;
	logger->Log("RandomWindow initialized");

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
	CameraWindow cameraWindow;
	logger->LogInfo("CameraWindow initialized");

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

	DraggableNode draggableNode;
	logger->LogInfo("DraggableNode initialized");
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


	// Main loop
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

		// Create a small window for FPS display
		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(200, 50), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
		ImGui::Begin("Performance", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
		ImGui::Text("FPS: %.1f", fps);
		ImGui::End();

		// Render logger UI
		logger->RenderUI();



		// Render our random window
		randomWindow.Render();

		// Check if our window wants to close
		if (randomWindow.IsDone()) {
			done = true;
		}

		// Update all TCP clients
		clientManager.UpdateClients();

		// Render TCP client manager UI
		clientManager.RenderUI();

		// Add this line before the FPS display window rendering
		toolbar.RenderUI();
		// Render motion configuration editor UI
		configEditor.RenderUI();
		graphVisualizer.RenderUI();
		draggableNode.RenderUI();


		//ACS connection
		acsMonitor.RenderUI();




		// Render camera window UI
		cameraWindow.RenderUI();

		// Check if camera window wants to close (if needed)
		if (cameraWindow.IsDone()) {
			done = true;
		}



		// Rendering
		ImGui::Render();
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window);
	}

	logger->Log("Application shutting down");


	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}