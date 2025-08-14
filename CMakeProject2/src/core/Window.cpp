
// core/Window.cpp
#include "Window.h"
#include "../utils/Logger.h"
#include "../utils/Unicode.h"

Window::Window(const std::string& title, int width, int height, ImVec4 clearColor)
  : window(nullptr)
  , gl_context(nullptr)
  , title(title)
  , width(width)
  , height(height)
  , clear_color(clearColor)
  , should_close(false) {
}

Window::~Window() {
  Cleanup();
}

bool Window::Initialize() {
  SDL_WindowFlags window_flags = (SDL_WindowFlags)(
    SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

  int x_pos = GetPositionX();
  int y_pos = 100;

  window = SDL_CreateWindow(
    title.c_str(),
    x_pos, y_pos,
    width, height,
    window_flags
  );

  if (!window) {
    Logger::Error(L"Failed to create window: " + UnicodeUtils::StringToWString(title));
    return false;
  }

  gl_context = SDL_GL_CreateContext(window);
  if (!gl_context) {
    Logger::Error(L"Failed to create GL context for: " + UnicodeUtils::StringToWString(title));
    return false;
  }

  Logger::Success(L"Created window: " + UnicodeUtils::StringToWString(title));
  return true;
}

void Window::MakeContextCurrent() {
  SDL_GL_MakeCurrent(window, gl_context);
}

void Window::SwapBuffers() {
  SDL_GL_SwapWindow(window);
}

Uint32 Window::GetWindowID() const {
  return window ? SDL_GetWindowID(window) : 0;
}

void Window::GetSize(int& w, int& h) const {
  if (window) {
    SDL_GetWindowSize(window, &w, &h);
  }
  else {
    w = width;
    h = height;
  }
}

void Window::SetSize(int w, int h) {
  width = w;
  height = h;
  if (window) {
    SDL_SetWindowSize(window, w, h);
  }
}

int Window::GetPositionX() const {
  // Position windows side by side based on title
  if (title.find("Window 1") != std::string::npos) {
    return 100;
  }
  else if (title.find("Window 2") != std::string::npos) {
    return 950;
  }
  return SDL_WINDOWPOS_UNDEFINED;
}

void Window::Cleanup() {
  if (gl_context) {
    SDL_GL_DeleteContext(gl_context);
    gl_context = nullptr;
  }

  if (window) {
    SDL_DestroyWindow(window);
    window = nullptr;
  }

  if (!title.empty()) {
    Logger::Success(L"Cleaned up: " + UnicodeUtils::StringToWString(title));
  }
}