// VisualizePage.h - Add font support
#pragma once

// Forward declaration only - no raylib includes in header
class Logger;

class VisualizePage {
public:
  VisualizePage(Logger* logger);
  ~VisualizePage();

  // Simple interface
  void Render();

private:
  Logger* m_logger;

  // Animation variables (no raylib types here)
  float m_animationTime;
  int m_rectangleCount;

  // Font support (keep as void* to avoid raylib includes in header)
  void* m_customFont;  // Will be cast to Font in .cpp
  bool m_fontLoaded;
};