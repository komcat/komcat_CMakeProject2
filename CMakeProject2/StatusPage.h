// StatusPage.h - New separate page class
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
};