// VisualizePage.h - Clean header with NO raylib includes
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
};