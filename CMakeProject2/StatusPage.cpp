// StatusPage.cpp - Implementation with raylib includes
#include "StatusPage.h"
#include "include/logger.h"

// Only include raylib in the .cpp file
#include <raylib.h>

StatusPage::StatusPage(Logger* logger) : m_logger(logger) {
  if (m_logger) {
    m_logger->LogInfo("StatusPage created");
  }
}

StatusPage::~StatusPage() {
  if (m_logger) {
    m_logger->LogInfo("StatusPage destroyed");
  }
}

void StatusPage::Render() {
  // Simple status page - just title
  DrawText("Status Page", 10, 10, 20, DARKBLUE);
  DrawText("S: Switch to Status | M: Menu | V: Live Video | ESC: Close", 10, 40, 14, GRAY);

  // Add some basic status info
  DrawText("System Status: Running", 10, 80, 16, GREEN);
  DrawText("Raylib Window: Active", 10, 100, 16, GREEN);
  DrawText("Thread Status: OK", 10, 120, 16, GREEN);
}