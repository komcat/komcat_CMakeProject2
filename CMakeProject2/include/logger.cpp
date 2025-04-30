#include "include/logger.h"
#include "imgui.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

// Initialize static instance
std::unique_ptr<Logger> Logger::s_instance = nullptr;

Logger::Logger() {
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
    break;
  case LogLevel::Error:
    levelString = "ERROR";
    break;
  }

  std::string fileLogMessage = "[" + timestamp + "] [" + levelString + "] " + message;

  // Write to log file
  if (m_logFile.is_open()) {
    m_logFile << fileLogMessage << std::endl;
    m_logFile.flush(); // Ensure it's written immediately
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

void Logger::RenderUI() {
  // Get the display size to position the window at the bottom
  ImVec2 displaySize = ImGui::GetIO().DisplaySize;
  float logWindowHeight = 150.0f; // Fixed height for the log window

  // Set the window position and size (full width, fixed height at bottom)
  ImGui::SetNextWindowPos(ImVec2(0, displaySize.y - logWindowHeight), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(displaySize.x, logWindowHeight), ImGuiCond_Always);

  // Remove window decorations and make it non-movable and non-resizable
  ImGuiWindowFlags windowFlags =
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoCollapse;

  // Create ImGui window
  ImGui::Begin("Log Window", nullptr, windowFlags);

  // Add Clear button
  if (ImGui::Button("Clear")) {
    Clear();
  }

  ImGui::SameLine();

  // Add Save button
  if (ImGui::Button("Save")) {
    SaveLogsToFile("logs/saved_log.txt");
  }

  // Add filter buttons
  ImGui::SameLine();
  static bool showDebug = true;
  static bool showInfo = true;
  static bool showWarning = true;
  static bool showError = true;

  ImGui::SameLine(ImGui::GetWindowWidth() - 480);
  ImGui::Checkbox("Debug", &showDebug);

  ImGui::SameLine();
  ImGui::Checkbox("Info", &showInfo);

  ImGui::SameLine();
  ImGui::Checkbox("Warning", &showWarning);

  ImGui::SameLine();
  ImGui::Checkbox("Error", &showError);

  ImGui::Separator();

  // Create a child window for the scrollable log area
  ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

  // Lock mutex for thread safety when reading logs
  {
    std::lock_guard<std::mutex> lock(m_logMutex);

    // Display each log message with appropriate color based on level
    for (const auto& logMsg : m_logMessages) {
      // Skip messages based on filter settings
      if ((logMsg.level == LogLevel::Debug && !showDebug) ||
        (logMsg.level == LogLevel::Info && !showInfo) ||
        (logMsg.level == LogLevel::Warning && !showWarning) ||
        (logMsg.level == LogLevel::Error && !showError)) {
        continue;
      }

      // Set text color based on message level
      ImVec4 textColor;
      switch (logMsg.level) {
      case LogLevel::Debug:
        textColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Gray
        break;
      case LogLevel::Info:
        textColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White
        break;
      case LogLevel::Warning:
        textColor = ImVec4(1.0f, 0.7f, 0.0f, 1.0f); // Orange
        break;
      case LogLevel::Error:
        textColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); // Red
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

  ImGui::EndChild();
  ImGui::End();
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
    }

    file << "[" << logMsg.timestamp << "] [" << levelString << "] " << logMsg.text << std::endl;
  }

  file.close();
  return true;
}