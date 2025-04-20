#include <algorithm> // For std::min and std::max 
#include "CMakeProject2.h"
#include "randomwindow.h"
#include "client_manager.h" // Replace tcp_client.h with our new manager
#include "ACSC.h"

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

    // Create our ClientManager instead of a single TcpClient
    ClientManager clientManager;

    // For FPS calculation
    float frameTime = 0.0f;
    float fps = 0.0f;
    float fpsUpdateInterval = 0.5f; // Update FPS display every half second
    float fpsTimer = 0.0f;
    int frameCounter = 0;
    Uint64 lastFrameTime = SDL_GetPerformanceCounter();

    //ACS controller
    HANDLE hComm = ACSC_INVALID;
    bool isConnected = false;
    static char ipAddress[64] = "192.168.0.50";
    static bool connectionAttempted = false;
    static bool connectionSuccessful = false;
    static bool motorEnabledX = false;
    static bool motorEnabledY = false;
    static bool motorEnabledZ = false;
    static double xPos = 0.0, yPos = 0.0, zPos = 0.0;

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


        //ACS connection




        ImGui::Begin("ACS Controller");



        ImGui::InputText("IP Address", ipAddress, IM_ARRAYSIZE(ipAddress));

        if (!isConnected && ImGui::Button("Connect")) {
            hComm = acsc_OpenCommEthernet(ipAddress, ACSC_SOCKET_STREAM_PORT);
            connectionAttempted = true;
            if (hComm != ACSC_INVALID) {
                isConnected = true;
                connectionSuccessful = true;
            }
            else {
                connectionSuccessful = false;
            }
        }

        if (connectionAttempted) {
            if (connectionSuccessful) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "✅ Connected to %s", ipAddress);
            }
            else {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "❌ Failed to connect.");
            }
        }

        if (isConnected) {
            // Enable each motor if not already enabled
            if (!motorEnabledX) motorEnabledX = acsc_Enable(hComm, ACSC_AXIS_X, nullptr);
            if (!motorEnabledY) motorEnabledY = acsc_Enable(hComm, ACSC_AXIS_Y, nullptr);
            if (!motorEnabledZ) motorEnabledZ = acsc_Enable(hComm, ACSC_AXIS_Z, nullptr);

            // Display position for each axis
            if (acsc_GetFPosition(hComm, ACSC_AXIS_X, &xPos, nullptr)) {
                ImGui::Text("X Position: %.2f", xPos);
            }
            else {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to read X position");
            }

            if (acsc_GetFPosition(hComm, ACSC_AXIS_Y, &yPos, nullptr)) {
                ImGui::Text("Y Position: %.2f", yPos);
            }
            else {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to read Y position");
            }

            if (acsc_GetFPosition(hComm, ACSC_AXIS_Z, &zPos, nullptr)) {
                ImGui::Text("Z Position: %.2f", zPos);
            }
            else {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to read Z position");
            }

            if (ImGui::Button("Disconnect")) {
                acsc_CloseComm(hComm);
                hComm = ACSC_INVALID;
                isConnected = false;
                connectionAttempted = false;
                connectionSuccessful = false;
                motorEnabledX = motorEnabledY = motorEnabledZ = false;
            }
        }

        ImGui::End();







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