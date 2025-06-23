#pragma once
#include <string>
#include <sstream>

//version
/*
version 2.1.0.46
- add macro manager executing block programs
- user write multiple block program and connected into larger macro program.

*/

// Version information - manually set for now
#define VERSION_MAJOR 2
#define VERSION_MINOR 1
#define VERSION_PATCH 1
#define VERSION_BUILD 17

// Build information
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

// Git information
#define GIT_COMMIT_HASH "unknown"
#define GIT_BRANCH "unknown"

// Debug/Release configuration
#ifdef _DEBUG
#define BUILD_CONFIG "Debug"
#else
#define BUILD_CONFIG "Release"
#endif

class Version {
private:
  // Helper function to convert int to string
  static std::string intToString(int value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
  }

public:
  // Get version as string in format "1.0.0"
  static std::string getVersionString() {
    return intToString(VERSION_MAJOR) + "." +
      intToString(VERSION_MINOR) + "." +
      intToString(VERSION_PATCH);
  }

  // Get full version with build number "1.0.0.123"
  static std::string getFullVersionString() {
    return getVersionString() + "." + intToString(VERSION_BUILD);
  }

  // Get version with build info "1.0.0 (Debug, Dec 15 2024)"
  static std::string getVersionWithBuildInfo() {
    return getVersionString() + " (" + BUILD_CONFIG + ", " + BUILD_DATE + ")";
  }

  // Get complete title for window - NOW INCLUDES BUILD NUMBER
  static std::string getWindowTitle() {
    return "FWAAA v" + getFullVersionString() + " - " + BUILD_CONFIG;
  }

  // Alternative window title formats (choose one):

  // Option 1: Compact format with build
  static std::string getWindowTitleCompact() {
    return "FWAAA v" + getVersionString() + " Build " + intToString(VERSION_BUILD) + " - " + BUILD_CONFIG;
  }

  // Option 2: Detailed format with build
  static std::string getWindowTitleDetailed() {
    return "FWAAA v" + getVersionString() + " (Build " + intToString(VERSION_BUILD) + ") - " + BUILD_CONFIG;
  }

  // Option 3: Short format
  static std::string getWindowTitleShort() {
    return "SDV v" + getFullVersionString() + " - " + BUILD_CONFIG;
  }

  // Get detailed version info for about dialog or console
  static std::string getDetailedVersionInfo() {
    return "FWAAA\n"
      "Version: " + getFullVersionString() + "\n"
      "Build: " + BUILD_CONFIG + " (" + BUILD_DATE + " " + BUILD_TIME + ")\n"
      "Branch: " + std::string(GIT_BRANCH) + "\n"
      "Commit: " + std::string(GIT_COMMIT_HASH);
  }

  // Get version components as integers
  static int getMajor() { return VERSION_MAJOR; }
  static int getMinor() { return VERSION_MINOR; }
  static int getPatch() { return VERSION_PATCH; }
  static int getBuild() { return VERSION_BUILD; }
};