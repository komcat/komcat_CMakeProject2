// SimpleSplashScreen.cpp
#include "SimpleSplashScreen.h"
#include <iostream>
#include <cmath>
#include <algorithm>

SimpleSplashScreen::SimpleSplashScreen() {
  startTime = std::chrono::steady_clock::now();
}

SimpleSplashScreen::~SimpleSplashScreen() {
  Shutdown();
}

bool SimpleSplashScreen::Initialize() {
  // Initialize SDL video subsystem
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cerr << "SDL init failed: " << SDL_GetError() << std::endl;
    return false;
  }

  // Create borderless window, centered on screen
  window = SDL_CreateWindow(
    "UAA3 System Loading",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    windowWidth,
    windowHeight,
    SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP
  );

  if (!window) {
    std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
    return false;
  }

  // Create renderer with VSync
  renderer = SDL_CreateRenderer(window, -1,
    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  if (!renderer) {
    std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
    return false;
  }

  // Enable alpha blending for smooth effects
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  // Initialize bitmap font
  if (!font.Initialize(renderer)) {
    std::cerr << "Failed to initialize bitmap font" << std::endl;
    return false;
  }

  // Initial render
  Render();

  return true;
}

void SimpleSplashScreen::UpdateProgress(const ApplicationInitializer::InitProgress& progress) {
  currentProgress = progress;
  animationTime += 0.016f; // Approximate 60 FPS

  Render();

  // Process events to keep window responsive
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    // Allow ESC to close splash (optional)
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
      // Could set a flag to skip splash
    }
  }
}

void SimpleSplashScreen::Render() {
  DrawBackground();
  DrawLogo();
  DrawTitle();
  DrawStatusText();
  DrawProgressBar();
  DrawStepIndicator();
  DrawElapsedTime();

  SDL_RenderPresent(renderer);
}

void SimpleSplashScreen::DrawBackground() {
  // Create gradient background (dark blue to black)
  for (int y = 0; y < windowHeight; y++) {
    int blue = 50 - (y * 30 / windowHeight);
    int green = 30 - (y * 20 / windowHeight);
    SDL_SetRenderDrawColor(renderer, 10, green, blue, 255);
    SDL_RenderDrawLine(renderer, 0, y, windowWidth, y);
  }

  // Add subtle grid pattern
  SDL_SetRenderDrawColor(renderer, 30, 40, 60, 30);
  for (int x = 0; x < windowWidth; x += 50) {
    SDL_RenderDrawLine(renderer, x, 0, x, windowHeight);
  }
  for (int y = 0; y < windowHeight; y += 50) {
    SDL_RenderDrawLine(renderer, 0, y, windowWidth, y);
  }

  // Draw border
  SDL_SetRenderDrawColor(renderer, 100, 150, 200, 255);
  SDL_Rect border = { 1, 1, windowWidth - 2, windowHeight - 2 };
  SDL_RenderDrawRect(renderer, &border);
}

void SimpleSplashScreen::DrawLogo() {
  int centerX = windowWidth / 2;
  int centerY = 120;

  // Draw animated hexagon logo (representing the hex controllers)
  SDL_SetRenderDrawColor(renderer, 100, 150, 255, 200);

  // Calculate hexagon points
  const int radius = 50;
  const int sides = 6;
  SDL_Point points[7]; // 6 points + 1 to close the shape

  for (int i = 0; i <= sides; i++) {
    float angle = (float)(i % sides) * (2.0f * M_PI / sides) - M_PI / 2;
    angle += animationTime * 0.5f; // Slow rotation

    points[i].x = centerX + (int)(radius * cos(angle));
    points[i].y = centerY + (int)(radius * sin(angle));
  }

  // Draw hexagon outline
  SDL_RenderDrawLines(renderer, points, sides + 1);

  // Draw inner hexagon
  SDL_SetRenderDrawColor(renderer, 50, 100, 200, 150);
  SDL_Point innerPoints[7];
  for (int i = 0; i <= sides; i++) {
    float angle = (float)(i % sides) * (2.0f * M_PI / sides) - M_PI / 2;
    angle -= animationTime * 0.7f; // Counter-rotation

    innerPoints[i].x = centerX + (int)(radius * 0.6f * cos(angle));
    innerPoints[i].y = centerY + (int)(radius * 0.6f * sin(angle));
  }
  SDL_RenderDrawLines(renderer, innerPoints, sides + 1);

  // Connect vertices between inner and outer hexagon
  SDL_SetRenderDrawColor(renderer, 70, 120, 220, 100);
  for (int i = 0; i < sides; i++) {
    SDL_RenderDrawLine(renderer, points[i].x, points[i].y,
      innerPoints[i].x, innerPoints[i].y);
  }
}

void SimpleSplashScreen::DrawTitle() {
  // Draw "UAA3" in large letters using bitmap font
  font.DrawTextCentered("UAA3 SYSTEM", 200, windowWidth, 3);

  // Draw version
  font.DrawTextCentered("VERSION 2.0.1", 235, windowWidth, 1);
}

void SimpleSplashScreen::DrawProgressBar() {
  int barX = 50;
  int barY = 320;
  int barWidth = windowWidth - 100;
  int barHeight = 30;

  // Background
  SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
  SDL_Rect bgBar = { barX, barY, barWidth, barHeight };
  SDL_RenderFillRect(renderer, &bgBar);

  // Progress fill
  int fillWidth = (int)(barWidth * currentProgress.percentage / 100.0f);

  // Choose color based on state
  if (currentProgress.hasError) {
    SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);  // Red
  }
  else if (currentProgress.percentage < 30) {
    SDL_SetRenderDrawColor(renderer, 255, 150, 50, 255);  // Orange
  }
  else if (currentProgress.percentage < 70) {
    SDL_SetRenderDrawColor(renderer, 255, 220, 50, 255);  // Yellow
  }
  else {
    SDL_SetRenderDrawColor(renderer, 50, 255, 100, 255);  // Green
  }

  SDL_Rect fillBar = { barX, barY, fillWidth, barHeight };
  SDL_RenderFillRect(renderer, &fillBar);

  // Add gradient effect on progress bar
  for (int i = 0; i < 10; i++) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 20 - i * 2);
    SDL_RenderDrawLine(renderer, barX, barY + i,
      barX + fillWidth, barY + i);
  }

  // Border
  SDL_SetRenderDrawColor(renderer, 150, 150, 200, 255);
  SDL_RenderDrawRect(renderer, &bgBar);

  // Percentage text in center of bar
  std::string percentText = std::to_string((int)currentProgress.percentage) + "%";
  font.DrawTextCentered(percentText, barY + 8, windowWidth, 2);
}

void SimpleSplashScreen::DrawStatusText() {
  // Current operation text
  if (currentProgress.hasError) {
    // For error messages, we'll just display "ERROR" since the bitmap font is limited
    font.DrawTextCentered("ERROR", 280, windowWidth, 1);
  }
  else {
    // Simplify status text to basic ASCII characters only
    std::string simpleStatus = currentProgress.currentOperation;

    // Replace any problematic characters with basic ones
    for (char& c : simpleStatus) {
      if (c < 32 || c > 126) {
        c = '?';
      }
    }

    font.DrawTextCentered(simpleStatus, 280, windowWidth, 1);
  }
}

void SimpleSplashScreen::DrawStepIndicator() {
  int dotY = 370;
  int dotSize = 10;
  int spacing = 35;
  int totalWidth = currentProgress.totalSteps * spacing;
  int startX = (windowWidth - totalWidth) / 2;

  for (int i = 0; i < currentProgress.totalSteps; i++) {
    int x = startX + (i * spacing) + spacing / 2;

    // Draw connecting line
    if (i < currentProgress.totalSteps - 1) {
      if (i < currentProgress.currentStep - 1) {
        SDL_SetRenderDrawColor(renderer, 50, 200, 100, 255);  // Green
      }
      else {
        SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);  // Gray
      }
      SDL_RenderDrawLine(renderer, x + dotSize / 2, dotY + dotSize / 2,
        x + spacing - dotSize / 2, dotY + dotSize / 2);
    }

    // Draw dot
    if (i < currentProgress.currentStep - 1) {
      // Completed
      SDL_SetRenderDrawColor(renderer, 50, 255, 100, 255);
      SDL_Rect dot = { x, dotY, dotSize, dotSize };
      SDL_RenderFillRect(renderer, &dot);
    }
    else if (i == currentProgress.currentStep - 1) {
      // Current - animated
      int pulse = (int)(5 * sin(animationTime * 5));
      SDL_SetRenderDrawColor(renderer, 255, 220, 100, 255);
      SDL_Rect dot = { x - pulse / 2, dotY - pulse / 2,
                    dotSize + pulse, dotSize + pulse };
      SDL_RenderFillRect(renderer, &dot);
    }
    else {
      // Pending
      SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
      SDL_Rect dot = { x, dotY, dotSize, dotSize };
      SDL_RenderDrawRect(renderer, &dot);
    }
  }

  // Step text using bitmap font
  std::string stepText = "STEP " + std::to_string(currentProgress.currentStep) +
    " OF " + std::to_string(currentProgress.totalSteps);
  font.DrawTextCentered(stepText, 395, windowWidth, 1);
}

void SimpleSplashScreen::DrawElapsedTime() {
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);

  std::string timeText = "ELAPSED: " + std::to_string(elapsed.count()) + " MS";
  font.DrawTextCentered(timeText, 415, windowWidth, 1);
}

void SimpleSplashScreen::Shutdown() {
  font.Shutdown();

  if (renderer) {
    SDL_DestroyRenderer(renderer);
    renderer = nullptr;
  }

  if (window) {
    SDL_DestroyWindow(window);
    window = nullptr;
  }

  // Only quit SDL if we initialized it
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}