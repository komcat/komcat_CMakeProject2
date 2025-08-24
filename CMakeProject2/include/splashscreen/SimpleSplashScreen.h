// SimpleSplashScreen.h
#pragma once

#include <SDL.h>
#include <string>
#include <vector>
#include <chrono>
#include "ApplicationInitializer.h"
#include "BitmapFont.h"

class SimpleSplashScreen {
private:
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;

  int windowWidth = 700;
  int windowHeight = 450;

  ApplicationInitializer::InitProgress currentProgress;
  std::chrono::steady_clock::time_point startTime;

  // Animation state
  float animationTime = 0.0f;
  std::vector<SDL_Point> logoPoints;

  // Bitmap font for text rendering
  BitmapFont font;

public:
  SimpleSplashScreen();
  ~SimpleSplashScreen();

  bool Initialize();
  void UpdateProgress(const ApplicationInitializer::InitProgress& progress);
  void Render();
  void Shutdown();

private:
  void DrawBackground();
  void DrawLogo();
  void DrawTitle();
  void DrawProgressBar();
  void DrawStatusText();
  void DrawStepIndicator();
  void DrawElapsedTime();
};