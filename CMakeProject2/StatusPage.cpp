// StatusPage.cpp - Implementation with custom font
#include "StatusPage.h"
#include "include/logger.h"

// Only include raylib in the .cpp file
#include <raylib.h>

StatusPage::StatusPage(Logger* logger) : m_logger(logger), m_customFont(nullptr), m_fontLoaded(false) {
  if (m_logger) {
    m_logger->LogInfo("StatusPage created");
  }

  // Load the custom font
  Font* customFont = new Font();
  *customFont = LoadFont("assets/fonts/CascadiaCode-Regular.ttf");

  if (customFont->texture.id != 0) {
    m_customFont = customFont;
    m_fontLoaded = true;
    if (m_logger) {
      m_logger->LogInfo("StatusPage: CascadiaCode-Regular font loaded successfully");
    }
  }
  else {
    delete customFont;
    m_customFont = nullptr;
    m_fontLoaded = false;
    if (m_logger) {
      m_logger->LogWarning("StatusPage: Failed to load CascadiaCode-Regular font, using default");
    }
  }
}

StatusPage::~StatusPage() {
  // Clean up font
  if (m_fontLoaded && m_customFont) {
    Font* customFont = static_cast<Font*>(m_customFont);
    UnloadFont(*customFont);
    delete customFont;
    m_customFont = nullptr;
  }

  if (m_logger) {
    m_logger->LogInfo("StatusPage destroyed");
  }
}

void StatusPage::Render() {
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

  // Status page content - using custom font
  DrawCustomText("Status Page", 10, 10, 20, DARKBLUE);
  DrawCustomText("S: Switch to Status | M: Menu | V: Live Video | ESC: Close", 10, 40, 14, GRAY);

  // Add some basic status info - using custom font
  DrawCustomText("System Status: Running", 10, 80, 16, GREEN);
  DrawCustomText("Raylib Window: Active", 10, 100, 16, GREEN);
  DrawCustomText("Thread Status: OK", 10, 120, 16, GREEN);

  // Add font status info
  if (m_fontLoaded) {
    DrawCustomText("Font: CascadiaCode-Regular (Loaded)", 10, GetScreenHeight() - 20, 12, DARKGREEN);
  }
  else {
    DrawCustomText("Font: Default (CascadiaCode failed to load)", 10, GetScreenHeight() - 20, 12, RED);
  }
}