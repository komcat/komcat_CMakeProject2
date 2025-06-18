#include "include/logger.h"
#include "imgui.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <iostream> // Added for stdout logging

// Initialize static instance
std::unique_ptr<Logger> Logger::s_instance = nullptr;

Logger::Logger() : m_isMinimized(false), m_isMaximized(false), m_fontSize(1.5f),
m_unreadMessages(0), m_unreadWarnings(0), m_unreadErrors(0), m_logToStdout(true) {
  // Get current date and open initial log file
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::tm tm = {};

#ifdef _WIN32
  localtime_s(&tm, &time);
#else
  tm = *std::localtime(&time);
#endif

  std::stringstream ss;
  ss << std::put_time(&tm, "%Y-%m-%d");
  m_currentDate = ss.str();

  // Open log file
  OpenLogFile();
}

Logger::~Logger() {
  // Close log file if open
  if (m_logFile.is_open()) {
    m_logFile.close();
  }
}

Logger* Logger::GetInstance() {
  if (!s_instance) {
    s_instance = std::unique_ptr<Logger>(new Logger());
  }
  return s_instance.get();
}

void Logger::OpenLogFile() {
  // Create logs directory if it doesn't exist
  std::filesystem::path logDir = "logs";
  if (!std::filesystem::exists(logDir)) {
    std::filesystem::create_directory(logDir);
  }

  // Create filename with current date
  std::string filename = "logs/log_" + m_currentDate + ".txt";

  // Close existing file if open
  if (m_logFile.is_open()) {
    m_logFile.close();
  }

  // Open new log file (append mode)
  m_logFile.open(filename, std::ios::app);

  if (!m_logFile) {
    // If failed to open, try to create logs directory again and retry
    std::filesystem::create_directory(logDir);
    m_logFile.open(filename, std::ios::app);
  }
}

void Logger::CheckAndUpdateLogFile() {
  // Get current date
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::tm tm = {};

#ifdef _WIN32
  localtime_s(&tm, &time);
#else
  tm = *std::localtime(&time);
#endif

  std::stringstream ss;
  ss << std::put_time(&tm, "%Y-%m-%d");
  std::string currentDate = ss.str();

  // If date has changed, update log file
  if (currentDate != m_currentDate) {
    m_currentDate = currentDate;
    OpenLogFile();
  }
}

std::string Logger::GetTimestamp() {
  // Get current timestamp
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::tm tm = {};

#ifdef _WIN32
  localtime_s(&tm, &time);
#else
  tm = *std::localtime(&time);
#endif

  std::stringstream ss;
  ss << std::put_time(&tm, "%H:%M:%S");
  return ss.str();
}

void Logger::Log(const std::string& message, LogLevel level) {
  // Lock mutex for thread safety
  std::lock_guard<std::mutex> lock(m_logMutex);

  // Check if date has changed and update log file if needed
  CheckAndUpdateLogFile();

  // Get current timestamp
  std::string timestamp = GetTimestamp();

  // Create log message
  LogMessage logMsg(message, level, timestamp);

  // Add to deque (limit to 100 entries)
  m_logMessages.push_back(logMsg);
  if (m_logMessages.size() > 100) {
    m_logMessages.pop_front();
  }

  // Format log message with timestamp and level for file
  std::string levelString;
  switch (level) {
  case LogLevel::Debug:
    levelString = "DEBUG";
    break;
  case LogLevel::Info:
    levelString = "INFO";
    break;
  case LogLevel::Warning:
    levelString = "WARNING";
    m_unreadWarnings++;
    break;
  case LogLevel::Error:
    levelString = "ERROR";
    m_unreadErrors++;
    break;
  case LogLevel::Process:
    levelString = "PROC";
    break;
  }

  // Increment unread counter if minimized
  if (m_isMinimized) {
    m_unreadMessages++;
  }

  std::string fileLogMessage = "[" + timestamp + "] [" + levelString + "] " + message;

  // Write to log file
  if (m_logFile.is_open()) {
    m_logFile << fileLogMessage << std::endl;
    m_logFile.flush(); // Ensure it's written immediately
  }

  // Log to stdout if enabled
  if (m_logToStdout) {
    // Set console color based on log level (ANSI escape codes for color)
    std::string colorCode;
    switch (level) {
    case LogLevel::Debug:
      colorCode = "\033[90m"; // Gray
      break;
    case LogLevel::Info:
      colorCode = "\033[37m"; // White
      break;
    case LogLevel::Warning:
      colorCode = "\033[33m"; // Yellow
      break;
    case LogLevel::Error:
      colorCode = "\033[31m"; // Red
      break;
    case LogLevel::Process:
      colorCode = "\033[36m"; // Cyan
      break;
    }

    std::string resetColor = "\033[0m"; // Reset color to default

    // Output to stdout with appropriate coloring
    std::cout << colorCode << fileLogMessage << resetColor << std::endl;
  }
}

// Convenience methods for different log levels
void Logger::LogDebug(const std::string& message) {
  Log(message, LogLevel::Debug);
}

void Logger::LogInfo(const std::string& message) {
  Log(message, LogLevel::Info);
}

void Logger::LogWarning(const std::string& message) {
  Log(message, LogLevel::Warning);
}

void Logger::LogError(const std::string& message) {
  Log(message, LogLevel::Error);
}

void Logger::Clear() {
  std::lock_guard<std::mutex> lock(m_logMutex);
  m_logMessages.clear();
}

void Logger::ToggleMinimize() {
  // Can't minimize if maximized - must return to normal state first
  if (m_isMaximized) {
    m_isMaximized = false;
  }

  m_isMinimized = !m_isMinimized;

  // Reset unread counters when expanding
  if (!m_isMinimized) {
    ResetUnreadCounters();
  }
}

void Logger::ToggleMaximize() {
  // Can't maximize if minimized - must return to normal state first
  if (m_isMinimized) {
    m_isMinimized = false;
    ResetUnreadCounters();
  }

  m_isMaximized = !m_isMaximized;
}

void Logger::IncreaseFontSize() {
  // Increase font size with upper limit (3.0x)
  m_fontSize = std::min(m_fontSize + 0.1f, 3.0f);
}

void Logger::DecreaseFontSize() {
  // Decrease font size with lower limit (0.5x)
  m_fontSize = std::max(m_fontSize - 0.1f, 0.5f);
}

void Logger::ResetUnreadCounters() {
  m_unreadMessages = 0;
  m_unreadWarnings = 0;
  m_unreadErrors = 0;
}

// Enable/disable stdout logging
void Logger::SetLogToStdout(bool enable) {
  m_logToStdout = enable;
}

bool Logger::IsLoggingToStdout() const {
  return m_logToStdout;
}

void Logger::RenderUI() {
  // Get the display size for window positioning
  ImVec2 displaySize = ImGui::GetIO().DisplaySize;

  if (m_isMinimized) {
    // Height for the minimized status bar
    float statusBarHeight = 30.0f;

    // Set the window position and size (full width, minimal height at bottom)
    ImGui::SetNextWindowPos(ImVec2(0, displaySize.y - statusBarHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(displaySize.x, statusBarHeight), ImGuiCond_Always);

    // Set a dark background for the status bar
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 0.9f));

    // Remove window decorations and make it non-movable and non-resizable
    ImGuiWindowFlags windowFlags =
      ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoTitleBar;

    // Create ImGui window
    ImGui::Begin("Log Status", nullptr, windowFlags);

    // Add expand button
    if (ImGui::Button("Expand Log")) {
      ToggleMinimize();
    }

    ImGui::SameLine();

    // Show message counters in different colors based on severity
    ImGui::Text("Messages: ");

    // Show unread message count if there are any
    if (m_unreadMessages > 0) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%d", m_unreadMessages);
    }

    // Show unread warnings count if there are any
    if (m_unreadWarnings > 0) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Warnings: %d", m_unreadWarnings);
    }

    // Show unread errors count if there are any
    if (m_unreadErrors > 0) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Errors: %d", m_unreadErrors);
    }

    // Show the latest log message
    if (!m_logMessages.empty()) {
      ImGui::SameLine(ImGui::GetWindowWidth() - 400);

      // Get the latest message
      const auto& latestMsg = m_logMessages.back();

      // Set text color based on message level
      ImVec4 textColor;
      switch (latestMsg.level) {
      case LogLevel::Debug:
        textColor = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        break;
      case LogLevel::Info:
        textColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        break;
      case LogLevel::Warning:
        textColor = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
        break;
      case LogLevel::Error:
        textColor = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
        break;
      }

      // Display the latest message with ellipsis if too long
      std::string latestText = latestMsg.text;
      if (latestText.length() > 50) {
        latestText = latestText.substr(0, 47) + "...";
      }

      ImGui::TextColored(textColor, "%s", latestText.c_str());
    }

    ImGui::End();
    ImGui::PopStyleColor();
  }
  else {
    // Calculate window height based on the current state (normal vs maximized)
    float logWindowHeight = m_isMaximized ? displaySize.y : 150.0f;
    float logWindowYPos = m_isMaximized ? 0 : (displaySize.y - logWindowHeight);

    // Set the window position and size
    ImGui::SetNextWindowPos(ImVec2(0, logWindowYPos), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(displaySize.x, logWindowHeight), ImGuiCond_Always);

    // Set a dark background for the log window
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 0.9f));

    // Remove window decorations and make it non-movable and non-resizable
    ImGuiWindowFlags windowFlags =
      ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoCollapse;

    // Create ImGui window
    ImGui::Begin("Log Window", nullptr, windowFlags);

    // Add buttons for window control (Minimize, Maximize, Clear, Save)
    if (ImGui::Button("Minimize")) {
      ToggleMinimize();
    }

    ImGui::SameLine();

    // Toggle between Maximize and Restore based on current state
    if (m_isMaximized) {
      if (ImGui::Button("Restore")) {
        ToggleMaximize();
      }
    }
    else {
      if (ImGui::Button("Maximize")) {
        ToggleMaximize();
      }
    }

    ImGui::SameLine();

    // Add Clear button
    if (ImGui::Button("Clear")) {
      Clear();
    }

    ImGui::SameLine();

    // Add Save button
    if (ImGui::Button("Save")) {
      SaveLogsToFile("logs/saved_log.txt");
    }

    // Font size controls
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20); // Add some spacing

    if (ImGui::Button("F-")) {
      DecreaseFontSize();
    }

    ImGui::SameLine();
    if (ImGui::Button("F+")) {
      IncreaseFontSize();
    }

    ImGui::SameLine();
    ImGui::Text("Font: %.1fx", m_fontSize);

    // Add stdout toggle
    ImGui::SameLine();
    bool logToStdout = m_logToStdout;
    if (ImGui::Checkbox("Log to Console", &logToStdout)) {
      SetLogToStdout(logToStdout);
    }

    // Add filter buttons
    ImGui::SameLine();
    static bool showDebug = true;
    static bool showInfo = true;
    static bool showWarning = true;
    static bool showError = true;
    static bool showProcess = true;  // Add new filter

    ImGui::SameLine(ImGui::GetWindowWidth() - 580);  // Adjust position to accommodate the new checkbox
    ImGui::Checkbox("Debug", &showDebug);

    ImGui::SameLine();
    ImGui::Checkbox("Info", &showInfo);

    ImGui::SameLine();
    ImGui::Checkbox("Warning", &showWarning);

    ImGui::SameLine();
    ImGui::Checkbox("Error", &showError);

    ImGui::SameLine();
    ImGui::Checkbox("Process", &showProcess);  // Add new checkbox

    ImGui::Separator();

    // Create a child window for the scrollable log area
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    // Set the font scale factor for all text in the log area
    ImGui::SetWindowFontScale(m_fontSize);

    // Lock mutex for thread safety when reading logs
    {
      std::lock_guard<std::mutex> lock(m_logMutex);

      // Display each log message with appropriate color based on level
      for (const auto& logMsg : m_logMessages) {
        // Skip messages based on filter settings
        if ((logMsg.level == LogLevel::Debug && !showDebug) ||
          (logMsg.level == LogLevel::Info && !showInfo) ||
          (logMsg.level == LogLevel::Warning && !showWarning) ||
          (logMsg.level == LogLevel::Error && !showError) ||
          (logMsg.level == LogLevel::Process && !showProcess)) {  // Add new filter check
          continue;
        }

        // Set text color based on message level
        ImVec4 textColor;
        switch (logMsg.level) {
        case LogLevel::Debug:
          textColor = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); // Lighter Gray for better contrast
          break;
        case LogLevel::Info:
          textColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White
          break;
        case LogLevel::Warning:
          textColor = ImVec4(1.0f, 0.8f, 0.0f, 1.0f); // Brighter Orange
          break;
        case LogLevel::Error:
          textColor = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); // Brighter Red
          break;
        case LogLevel::Process:
          textColor = ImVec4(0.2f, 0.8f, 0.8f, 1.0f); // Cyan
          break;
        }

        // Format message with timestamp
        std::string displayText = "[" + logMsg.timestamp + "] " + logMsg.text;

        // Set text color and display message
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::TextWrapped("%s", displayText.c_str());
        ImGui::PopStyleColor();
      }
    }

    // Auto-scroll to the bottom
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
      ImGui::SetScrollHereY(1.0f);
    }

    // Reset the font scale at the end of the child window
    ImGui::SetWindowFontScale(1.0f);

    ImGui::EndChild();
    ImGui::End();

    // Pop the window background color
    ImGui::PopStyleColor();
  }
}

bool Logger::SaveLogsToFile(const std::string& filename) {
  std::lock_guard<std::mutex> lock(m_logMutex);

  // Create logs directory if it doesn't exist
  std::filesystem::path logDir = "logs";
  if (!std::filesystem::exists(logDir)) {
    std::filesystem::create_directory(logDir);
  }

  // Open file for writing
  std::ofstream file(filename);
  if (!file.is_open()) {
    return false;
  }

  // Write all logs to file
  for (const auto& logMsg : m_logMessages) {
    // Format message with timestamp and level
    std::string levelString;
    switch (logMsg.level) {
    case LogLevel::Debug:
      levelString = "DEBUG";
      break;
    case LogLevel::Info:
      levelString = "INFO";
      break;
    case LogLevel::Warning:
      levelString = "WARNING";
      break;
    case LogLevel::Error:
      levelString = "ERROR";
      break;
    case LogLevel::Process:
      levelString = "PROC";
      break;
    
    }

    file << "[" << logMsg.timestamp << "] [" << levelString << "] " << logMsg.text << std::endl;
  }

  file.close();
  return true;
}

// Convenience method for process log level
void Logger::LogProcess(const std::string& message) {
  Log(message, LogLevel::Process);
}