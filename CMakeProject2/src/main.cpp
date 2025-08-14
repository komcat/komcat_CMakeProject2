// main.cpp - Clean Entry Point
// =====================================================
// Application entry point - keeps main() minimal and focused
// =====================================================

#include "core/Application.h"
#include "utils/Logger.h"
#include "utils/Unicode.h"

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

int main(int argc, char* argv[]) {
  // Initialize platform-specific console support
  UnicodeUtils::InitializeConsole();

  //Logger::Info(L"🎯 === COMPREHENSIVE IMGUI EMOJI TEST ===");
  //Logger::Info(L"This test addresses all common emoji display issues:");
  //Logger::Info(L"✅ 1. Proper emoji font loading");
  //Logger::Info(L"✅ 2. Comprehensive Unicode ranges");
  //Logger::Info(L"✅ 3. Correct font atlas building");
  //Logger::Info(L"✅ 4. UTF-8 string encoding");
  //Logger::Info(L"✅ 5. FreeType color emoji support");
  //Logger::Info(L"");

  // Create and run application
  Application app;

  if (!app.Initialize()) {
    Logger::Error(L"❌ Failed to initialize application");
    return -1;
  }

  app.Run();
  app.Cleanup();

  Logger::Info(L"👋 Comprehensive emoji test completed successfully! 🎉");
  return 0;
}