// StatusPage.h - Add font support
#pragma once

// Forward declaration only - no raylib includes in header
class Logger;

class StatusPage {
public:
  StatusPage(Logger* logger);
  ~StatusPage();

  // Simple interface
  void Render();

private:
  Logger* m_logger;

  // Font support (keep as void* to avoid raylib includes in header)
  void* m_customFont;  // Will be cast to Font in .cpp
  bool m_fontLoaded;
};