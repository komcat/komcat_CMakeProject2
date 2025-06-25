// VisualizePage.cpp - Implementation with custom font
#include "VisualizePage.h"
#include "include/logger.h"

// ONLY include raylib in the .cpp file
#include <raylib.h>
#include <math.h>

VisualizePage::VisualizePage(Logger* logger)
  : m_logger(logger), m_animationTime(0.0f), m_rectangleCount(8), m_customFont(nullptr), m_fontLoaded(false) {

  if (m_logger) {
    m_logger->LogInfo("VisualizePage created");
  }

  // Load the custom font
  Font* customFont = new Font();
  *customFont = LoadFont("assets/fonts/CascadiaCode-Regular.ttf");

  if (customFont->texture.id != 0) {
    m_customFont = customFont;
    m_fontLoaded = true;
    if (m_logger) {
      m_logger->LogInfo("CascadiaCode-Regular font loaded successfully");
    }
  }
  else {
    delete customFont;
    m_customFont = nullptr;
    m_fontLoaded = false;
    if (m_logger) {
      m_logger->LogWarning("Failed to load CascadiaCode-Regular font, using default");
    }
  }
}

VisualizePage::~VisualizePage() {
  // Clean up font
  if (m_fontLoaded && m_customFont) {
    Font* customFont = static_cast<Font*>(m_customFont);
    UnloadFont(*customFont);
    delete customFont;
    m_customFont = nullptr;
  }

  if (m_logger) {
    m_logger->LogInfo("VisualizePage destroyed");
  }
}

void VisualizePage::Render() {
  // Update animation time
  m_animationTime += GetFrameTime();

  // Helper function to draw text with custom font or fallback
  auto DrawCustomText = [this](const char* text, int posX, int posY, int fontSize, Color color) {
    if (m_fontLoaded && m_customFont) {
      Font* customFont = static_cast<Font*>(m_customFont);
      DrawTextEx(*customFont, text, { (float)posX, (float)posY }, (float)fontSize, 1.0f, color);
    }
    else {
      DrawText(text, posX, posY, fontSize, color);
    }
  };

  // Page title - using custom font
  DrawCustomText("Visualize Page", 10, 10, 20, DARKBLUE);
  DrawCustomText("R: Rectangles | V: Live | M: Menu | S: Status | ESC: Close", 10, 40, 14, GRAY);

  // Interactive controls - using custom font
  DrawCustomText("Press +/- to change rectangle count", 10, 70, 14, DARKGREEN);
  DrawCustomText(TextFormat("Rectangle Count: %d", m_rectangleCount), 10, 90, 14, BLACK);

  // Handle input for rectangle count
  if (IsKeyPressed(KEY_KP_ADD) || IsKeyPressed(KEY_EQUAL)) {
    if (m_rectangleCount < 20) m_rectangleCount++;
  }
  if (IsKeyPressed(KEY_KP_SUBTRACT) || IsKeyPressed(KEY_MINUS)) {
    if (m_rectangleCount > 1) m_rectangleCount--;
  }

  // Draw animated rectangles in a grid pattern
  int startX = 100;
  int startY = 130;
  int rectSize = 60;
  int spacing = 80;
  int columns = 4;

  for (int i = 0; i < m_rectangleCount; i++) {
    // Calculate position in grid
    int col = i % columns;
    int row = i / columns;

    int x = startX + col * spacing;
    int y = startY + row * spacing;

    // Create animated color based on time and rectangle index
    float colorPhase = m_animationTime * 2.0f + i * 0.5f;
    int red = (int)((sin(colorPhase) + 1.0f) * 127.5f);
    int green = (int)((sin(colorPhase + 2.0f) + 1.0f) * 127.5f);
    int blue = (int)((sin(colorPhase + 4.0f) + 1.0f) * 127.5f);

    Color rectColor = { (unsigned char)red, (unsigned char)green, (unsigned char)blue, 255 };

    // Animated size - slightly pulsing
    float sizePulse = sin(m_animationTime * 3.0f + i * 0.3f) * 5.0f;
    int animatedSize = rectSize + (int)sizePulse;

    // Draw rectangle with border
    DrawRectangle(x, y, animatedSize, animatedSize, rectColor);
    DrawRectangleLines(x, y, animatedSize, animatedSize, BLACK);

    // Draw rectangle number - using custom font
    DrawCustomText(TextFormat("%d", i + 1), x + animatedSize / 2 - 8, y + animatedSize / 2 - 8, 16, WHITE);
  }

  // Draw some static rectangles for comparison - using custom font
  DrawCustomText("Static Rectangles:", 600, 130, 16, DARKBLUE);

  // Red rectangle
  DrawRectangle(600, 160, 100, 60, RED);
  DrawRectangleLines(600, 160, 100, 60, DARKGRAY);
  DrawCustomText("Red", 630, 180, 16, WHITE);

  // Green rectangle
  DrawRectangle(720, 160, 100, 60, GREEN);
  DrawRectangleLines(720, 160, 100, 60, DARKGRAY);
  DrawCustomText("Green", 745, 180, 16, WHITE);

  // Blue rectangle
  DrawRectangle(600, 240, 100, 60, BLUE);
  DrawRectangleLines(600, 240, 100, 60, DARKGRAY);
  DrawCustomText("Blue", 630, 260, 16, WHITE);

  // Yellow rectangle
  DrawRectangle(720, 240, 100, 60, YELLOW);
  DrawRectangleLines(720, 240, 100, 60, DARKGRAY);
  DrawCustomText("Yellow", 740, 260, 16, BLACK);

  // Show animation info - using custom font
  DrawCustomText(TextFormat("Animation Time: %.2f seconds", m_animationTime), 10, GetScreenHeight() - 60, 14, PURPLE);
  DrawCustomText("Rectangles are animated with color and size changes!", 10, GetScreenHeight() - 40, 14, PURPLE);

  // Add font status info
  if (m_fontLoaded) {
    DrawCustomText("Font: CascadiaCode-Regular (Loaded)", 10, GetScreenHeight() - 20, 12, DARKGREEN);
  }
  else {
    DrawCustomText("Font: Default (CascadiaCode failed to load)", 10, GetScreenHeight() - 20, 12, RED);
  }
}