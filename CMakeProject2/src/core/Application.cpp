
// core/Application.cpp
#include "Application.h"
#include "../utils/Logger.h"
#include "../utils/Unicode.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"
#include <GL/gl.h>

Application::Application()
  : running(true)
  , window1(nullptr)
  , window2(nullptr)
  , imgui_context1(nullptr)
  , imgui_context2(nullptr)
  , uiRenderer1(nullptr)
  , uiRenderer2(nullptr) {
}

Application::~Application() {
  Cleanup();
}

bool Application::Initialize() {
  if (!InitializeSDL()) return false;

  // Create windows
  window1 = std::make_unique<Window>(
    "Window 1 - Comprehensive Test", 800, 600,
    ImVec4(0.2f, 0.3f, 0.4f, 1.0f)
  );

  window2 = std::make_unique<Window>(
    "Window 2 - Secondary Tools", 600, 400,
    ImVec4(0.4f, 0.2f, 0.4f, 1.0f)
  );

  if (!window1->Initialize() || !window2->Initialize()) {
    return false;
  }

  if (!InitializeImGui()) return false;

  Logger::Success(L"Application initialized successfully with dual windows");
  return true;
}

bool Application::InitializeSDL() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    Logger::Error(L"SDL Initialization failed");
    return false;
  }

  SetupOpenGLAttributes();
  return true;
}

void Application::SetupOpenGLAttributes() {
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
}


bool Application::InitializeImGui() {
  // Setup ImGui for Window 1
  window1->MakeContextCurrent();

  IMGUI_CHECKVERSION();
  imgui_context1 = ImGui::CreateContext();
  ImGui::SetCurrentContext(imgui_context1);

  ImGuiIO& io1 = ImGui::GetIO();
  io1.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  ImGui::StyleColorsClassic();

  // CRITICAL: Actually call the font manager setup!
  auto fontResult1 = fontManager.SetupComprehensiveFonts(io1);
  if (!fontResult1.success) {
    Logger::Warning(L"Window 1 font setup had issues: " +
      UnicodeUtils::StringToWString(fontResult1.errorMessage));
  }
  else {
    Logger::Success(L"Window 1 fonts loaded successfully with emoji support!");
  }

  if (!ImGui_ImplSDL2_InitForOpenGL(window1->GetSDLWindow(), window1->GetGLContext())) {
    Logger::Error(L"Failed to initialize ImGui SDL2 backend for Window 1");
    return false;
  }

  if (!ImGui_ImplOpenGL3_Init("#version 130")) {
    Logger::Error(L"Failed to initialize ImGui OpenGL3 backend for Window 1");
    return false;
  }

  // Setup ImGui for Window 2
  window2->MakeContextCurrent();

  imgui_context2 = ImGui::CreateContext();
  ImGui::SetCurrentContext(imgui_context2);

  ImGuiIO& io2 = ImGui::GetIO();
  io2.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  ImGui::StyleColorsDark();

  // CRITICAL: Setup fonts for window 2 as well!
  auto fontResult2 = fontManager.SetupComprehensiveFonts(io2);
  if (!fontResult2.success) {
    Logger::Warning(L"Window 2 font setup had issues: " +
      UnicodeUtils::StringToWString(fontResult2.errorMessage));
  }
  else {
    Logger::Success(L"Window 2 fonts loaded successfully with emoji support!");
  }

  if (!ImGui_ImplSDL2_InitForOpenGL(window2->GetSDLWindow(), window2->GetGLContext())) {
    Logger::Error(L"Failed to initialize ImGui SDL2 backend for Window 2");
    return false;
  }

  if (!ImGui_ImplOpenGL3_Init("#version 130")) {
    Logger::Error(L"Failed to initialize ImGui OpenGL3 backend for Window 2");
    return false;
  }

  // Create UI renderers
  uiRenderer1 = std::make_unique<UIRenderer>(fontManager, "Window 1");
  uiRenderer2 = std::make_unique<UIRenderer>(fontManager, "Window 2");

  Logger::Success(L"ImGui initialized with comprehensive font support for both windows");
  return true;
}


void Application::Run() {
  Logger::Info(L"🚀 Starting dual window comprehensive emoji test...");

  while (running && !window1->ShouldClose() && !window2->ShouldClose()) {
    ProcessEvents();
    Render();
    SDL_Delay(16); // ~60 FPS
  }

  Logger::Info(L"🛑 Dual window application ended");
}

void Application::ProcessEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_WINDOWEVENT) {
      HandleWindowEvent(event.window);
    }
    else {
      HandleGlobalEvent(event);
    }
  }
}

void Application::HandleWindowEvent(const SDL_WindowEvent& windowEvent) {
  Uint32 windowID = windowEvent.windowID;

  if (windowID == window1->GetWindowID()) {
    // Process event for Window 1
    window1->MakeContextCurrent();
    ImGui::SetCurrentContext(imgui_context1);

    SDL_Event event;
    event.type = SDL_WINDOWEVENT;
    event.window = windowEvent;
    ImGui_ImplSDL2_ProcessEvent(&event);

    if (windowEvent.event == SDL_WINDOWEVENT_CLOSE) {
      window1->SetShouldClose(true);
    }
  }
  else if (windowID == window2->GetWindowID()) {
    // Process event for Window 2
    window2->MakeContextCurrent();
    ImGui::SetCurrentContext(imgui_context2);

    SDL_Event event;
    event.type = SDL_WINDOWEVENT;
    event.window = windowEvent;
    ImGui_ImplSDL2_ProcessEvent(&event);

    if (windowEvent.event == SDL_WINDOWEVENT_CLOSE) {
      window2->SetShouldClose(true);
    }
  }
}

void Application::HandleGlobalEvent(const SDL_Event& event) {
  // Send global events to both windows

  // Window 1
  window1->MakeContextCurrent();
  ImGui::SetCurrentContext(imgui_context1);
  ImGui_ImplSDL2_ProcessEvent(&event);

  // Window 2
  window2->MakeContextCurrent();
  ImGui::SetCurrentContext(imgui_context2);
  ImGui_ImplSDL2_ProcessEvent(&event);

  if (event.type == SDL_QUIT) {
    running = false;
  }
}

void Application::Render() {
  // Render Window 1
  RenderWindow(*window1, imgui_context1, *uiRenderer1);

  // Render Window 2
  RenderWindow(*window2, imgui_context2, *uiRenderer2);
}

void Application::RenderWindow(Window& window, ImGuiContext* context, UIRenderer& renderer) {
  window.MakeContextCurrent();
  ImGui::SetCurrentContext(context);

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  int width, height;
  window.GetSize(width, height);
  glViewport(0, 0, width, height);

  ImVec4 clearColor = window.GetClearColor();
  glClearColor(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
  glClear(GL_COLOR_BUFFER_BIT);

  // Render UI
  renderer.Render(running);

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  window.SwapBuffers();
}

void Application::Cleanup() {
  // Cleanup ImGui contexts
  if (imgui_context1) {
    window1->MakeContextCurrent();
    ImGui::SetCurrentContext(imgui_context1);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext(imgui_context1);
    imgui_context1 = nullptr;
  }

  if (imgui_context2) {
    window2->MakeContextCurrent();
    ImGui::SetCurrentContext(imgui_context2);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext(imgui_context2);
    imgui_context2 = nullptr;
  }

  // Cleanup windows
  window1.reset();
  window2.reset();

  SDL_Quit();
  Logger::Success(L"Dual window comprehensive cleanup complete");
}