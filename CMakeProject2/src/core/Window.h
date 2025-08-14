// core/Window.h
#pragma once

#include <SDL.h>
#include <SDL_opengl.h>
#include <string>
#include "imgui.h"

class Window {
public:
  Window(const std::string& title, int width, int height, ImVec4 clearColor);
  ~Window();

  bool Initialize();
  void MakeContextCurrent();
  void SwapBuffers();
  void Cleanup();

  // Getters
  SDL_Window* GetSDLWindow() const { return window; }
  SDL_GLContext GetGLContext() const { return gl_context; }
  Uint32 GetWindowID() const;
  const std::string& GetTitle() const { return title; }
  ImVec4 GetClearColor() const { return clear_color; }

  // State
  bool ShouldClose() const { return should_close; }
  void SetShouldClose(bool close) { should_close = close; }

  // Size management
  void GetSize(int& width, int& height) const;
  void SetSize(int width, int height);

private:
  SDL_Window* window;
  SDL_GLContext gl_context;
  std::string title;
  int width, height;
  ImVec4 clear_color;
  bool should_close;

  int GetPositionX() const;
};
