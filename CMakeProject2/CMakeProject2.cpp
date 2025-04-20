
#include <algorithm> // Add this include for std::min and std::max 
#include "CMakeProject2.h"
#include "randomwindow.h"
#include "tcp_client.h"

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

    // Create our RandomWindow instance
    RandomWindow randomWindow;



    // Variables for TCP client
    TcpClient tcpClient;
    std::string serverIp = "127.0.0.1";
    int serverPort = 8888;
    bool connected = false;
    float receivedValue = 0.0f;
    char serverIpBuffer[64] = "127.0.0.1";  // Buffer for ImGui input
    int connectionStatus = 0;  // 0: Disconnected, 1: Connected, 2: Failed
    char statusMessage[128] = "Not connected";
    float receivedValues[100] = {}; // Array to store last 100 values for visualization
    int valuesCount = 0;
    int valuesCursor = 0;


    // For FPS calculation
    float frameTime = 0.0f;
    float fps = 0.0f;
    float fpsUpdateInterval = 0.5f; // Update FPS display every half second
    float fpsTimer = 0.0f;
    int frameCounter = 0;
    Uint64 lastFrameTime = SDL_GetPerformanceCounter();

    


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


        // Then in your main loop, add this code before ImGui::NewFrame():
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

        // Render our random window
        randomWindow.Render();

        // Check if our window wants to close
        if (randomWindow.IsDone()) {
            done = true;
        }



        // Create ImGui window for TCP client
        ImGui::Begin("TCP Client");

        // Server connection settings
        ImGui::InputText("Server IP", serverIpBuffer, sizeof(serverIpBuffer));
        ImGui::InputInt("Server Port", &serverPort);

        // Connect / Disconnect button
        if (!connected)
        {
            if (ImGui::Button("Connect"))
            {
                serverIp = serverIpBuffer;
                connected = tcpClient.Connect(serverIp, serverPort);
                connectionStatus = connected ? 1 : 2;
                snprintf(statusMessage, sizeof(statusMessage), connected ?
                    "Connected to %s:%d" : "Failed to connect to %s:%d",
                    serverIp.c_str(), serverPort);
            }
        }
        else
        {
            if (ImGui::Button("Disconnect"))
            {
                tcpClient.Disconnect();
                connected = false;
                connectionStatus = 0;
                snprintf(statusMessage, sizeof(statusMessage), "Disconnected from %s:%d",
                    serverIp.c_str(), serverPort);
            }
        }

        // Update connection status if it changed
        if (connected && !tcpClient.IsConnected())
        {
            connected = false;
            connectionStatus = 0;
            snprintf(statusMessage, sizeof(statusMessage), "Connection lost to %s:%d",
                serverIp.c_str(), serverPort);
        }

        // Display connection status
        ImGui::Text("Status: %s", statusMessage);

        // Inside the main loop where you have "if (connected)"
        if (connected)
        {
            // Get all new values since last frame
            std::deque<float> newValues = tcpClient.GetReceivedValues();

            // Update the circular buffer with new values
            for (float val : newValues) {
                receivedValues[valuesCursor] = val;
                valuesCursor = (valuesCursor + 1) % 100;
                if (valuesCount < 100)
                    valuesCount++;

                // Also update the latest value
                receivedValue = val;
            }

            // Display the latest received value (always use the latest actual value)
            ImGui::Separator();
            ImGui::Text("Latest received value: %.6f", tcpClient.GetLatestValue());

            // Debug info
            ImGui::Text("Values in buffer: %d", valuesCount);
            ImGui::Text("New values this frame: %d", (int)newValues.size());

            // Plot the received values
            ImGui::Separator();
            ImGui::Text("Received Values History:");

            // Calculate min/max for better scaling (with safety checks)
            float minValue = 0.0f;
            float maxValue = 1.0f;

            if (valuesCount > 0) {
                // Initialize with first value
                minValue = receivedValues[0];
                maxValue = receivedValues[0];

                // Find actual min/max
                for (int i = 0; i < valuesCount; i++) {
                    minValue = (std::min)(minValue, receivedValues[i]);
                    maxValue = (std::max)(maxValue, receivedValues[i]);
                }

                // Add margins (10% padding)
                float range = maxValue - minValue;
                if (range < 0.001f) range = 0.1f;  // Prevent too small ranges

                float margin = range * 0.1f;
                minValue = (std::max)(0.0f, minValue - margin);
                maxValue = (std::min)(1.0f, maxValue + margin);
            }

            // Create the plot with proper offset to show most recent values first
            ImGui::PlotLines("##values",
                receivedValues,           // Array
                valuesCount,              // Count
                valuesCursor,             // Offset (to show most recent values first)
                nullptr,                  // Overlay text
                minValue,                 // Y-min
                maxValue,                 // Y-max
                ImVec2(0, 80),            // Graph size
                sizeof(float));           // Stride

            ImGui::Text("Min displayed: %.2f, Max displayed: %.2f", minValue, maxValue);
        }
        ImGui::End();

        // Rendering
        ImGui::Render();



        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}