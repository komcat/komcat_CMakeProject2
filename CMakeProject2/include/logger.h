#pragma once

#include <string>
#include <deque>
#include <mutex>
#include <fstream>
#include <memory>

// Enum for log message levels
enum class LogLevel {
  Debug,  // Gray - debug information
  Info,   // White - normal information
  Warning, // Orange - warnings
  Error,    // Red - errors
  Process  // Cyan - process information
};

// Structure to store log messages with their level
struct LogMessage {
  std::string text;
  LogLevel level;
  std::string timestamp;

  LogMessage(const std::string& msg, LogLevel lvl, const std::string& time)
    : text(msg), level(lvl), timestamp(time) {
  }
};

// Logger class for handling text logging
class Logger {
private:
  // Singleton instance
  static std::unique_ptr<Logger> s_instance;

  // Container to store log messages (max 100)
  std::deque<LogMessage> m_logMessages;

  // Mutex for thread safety
  std::mutex m_logMutex;

  // Current log file
  std::ofstream m_logFile;

  // Current date string
  std::string m_currentDate;

  // Is logger window minimized?
  bool m_isMinimized = false;

  // Is logger window maximized (full screen)?
  bool m_isMaximized = false;

  // Font size scale factor
  float m_fontSize = 1.6f;

  // Number of unread messages (when minimized)
  int m_unreadMessages = 0;

  // Number of unread notifications by type
  int m_unreadWarnings = 0;
  int m_unreadErrors = 0;

  // Should also log to stdout?
  bool m_logToStdout = true;

  // Constructor is private (singleton pattern)
  Logger();

  // Open a new log file for the current date
  void OpenLogFile();

  // Check if date has changed and update log file if needed
  void CheckAndUpdateLogFile();

  // Get current timestamp as string
  std::string GetTimestamp();

public:
  // Destructor
  ~Logger();

  // Get singleton instance
  static Logger* GetInstance();

  // Log a message with specified level
  void Log(const std::string& message, LogLevel level = LogLevel::Info);

  // Convenience methods for different log levels
  void LogDebug(const std::string& message);
  void LogInfo(const std::string& message);
  void LogWarning(const std::string& message);
  void LogError(const std::string& message);
  void LogProcess(const std::string& message); // New method for process logs

  // Clear all logs
  void Clear();

  // Toggle minimized state
  void ToggleMinimize();

  // Toggle maximized state (full screen)
  void ToggleMaximize();

  // Font size controls
  void IncreaseFontSize();
  void DecreaseFontSize();

  // Reset unread message counters
  void ResetUnreadCounters();

  // Render ImGui window for logs
  void RenderUI();

  // Save logs to file (custom filename)
  bool SaveLogsToFile(const std::string& filename);

  // Enable/disable stdout logging
  void SetLogToStdout(bool enable);
  bool IsLoggingToStdout() const;

  // Add these new methods:
  bool IsMinimized() const { return m_isMinimized; }
  bool IsMaximized() const { return m_isMaximized; }
};