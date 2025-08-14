// core/Application.h
#pragma once

#include <memory>
#include <atomic>
#include <SDL.h>
#include "Window.h"
#include "../ui/FontManager.h"
#include "../ui/UIRenderer.h"

class Application {
public:
  Application();
  ~Application();

  bool Initialize();
  void Run();
  void Cleanup();

private:
  // Core systems
  bool InitializeSDL();
  bool InitializeImGui();
  void SetupOpenGLAttributes();

  // Main loop
  void ProcessEvents();
  void Render();
  void RenderWindow(Window& window, ImGuiContext* context, UIRenderer& renderer);

  // Event handling
  void HandleWindowEvent(const SDL_WindowEvent& windowEvent);
  void HandleGlobalEvent(const SDL_Event& event);

  // State
  std::atomic<bool> running;

  // Windows and contexts
  std::unique_ptr<Window> window1;
  std::unique_ptr<Window> window2;
  ImGuiContext* imgui_context1;
  ImGuiContext* imgui_context2;

  // Systems
  FontManager fontManager;
  std::unique_ptr<UIRenderer> uiRenderer1;
  std::unique_ptr<UIRenderer> uiRenderer2;
};