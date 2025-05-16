// script_print_viewer.h
#pragma once

#include "include/ui/VerticalToolbarMenu.h"
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <ctime>  // For tm structure

class ScriptPrintViewer : public IHierarchicalTogglableUI {
public:
  ScriptPrintViewer();
  ~ScriptPrintViewer();

  // IHierarchicalTogglableUI implementation
  bool IsVisible() const override { return m_isVisible; }
  void ToggleWindow() override { m_isVisible = !m_isVisible; }
  const std::string& GetName() const override { return m_name; }
  bool HasChildren() const override { return false; }
  const std::vector<std::shared_ptr<IHierarchicalTogglableUI>>& GetChildren() const override { return m_children; }

  // Add a print message to the viewer
  void AddPrintMessage(const std::string& message);

  // Clear all messages
  void Clear();

  // Render the UI
  void RenderUI();

  // File logging configuration
  void EnableFileLogging(bool enable) { m_fileLoggingEnabled = enable; }
  bool IsFileLoggingEnabled() const { return m_fileLoggingEnabled; }
  void SetLogDirectory(const std::string& directory) { m_logDirectory = directory; }
  const std::string& GetLogDirectory() const { return m_logDirectory; }

private:
  struct PrintEntry {
    std::string message;
    std::chrono::system_clock::time_point timestamp;
  };


  bool m_isVisible;
  std::string m_name;
  std::vector<std::shared_ptr<IHierarchicalTogglableUI>> m_children;

  std::deque<PrintEntry> m_printHistory;
  std::mutex m_mutex;

  // UI settings
  bool m_autoScroll = true;
  bool m_showTimestamps = true;
  size_t m_maxEntries = 1000;

  // Filtering
  char m_filterBuffer[256] = "";

  // File logging members
  bool m_fileLoggingEnabled = true;
  std::string m_logDirectory = "logs/scripts";
  std::ofstream m_logFile;
  std::chrono::system_clock::time_point m_currentLogDate;

  // File logging methods
  void InitializeLogFile();
  void CheckAndRotateLogFile();
  void WriteToLogFile(const PrintEntry& entry);
  std::string GetLogFileName(const std::chrono::system_clock::time_point& date);
  void CloseLogFile();
};