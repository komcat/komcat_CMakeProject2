// script_print_viewer.cpp
#include "include/script/script_print_viewer.h"
#include "imgui.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

ScriptPrintViewer::ScriptPrintViewer()
  : m_isVisible(false), m_name("Script Print Output")
{
  // Initialize file logging if enabled
  if (m_fileLoggingEnabled) {
    InitializeLogFile();
  }
}

ScriptPrintViewer::~ScriptPrintViewer() {
  // Close log file if open
  CloseLogFile();
}

void ScriptPrintViewer::AddPrintMessage(const std::string& message) {
  std::lock_guard<std::mutex> lock(m_mutex);

  PrintEntry entry;
  entry.message = message;
  entry.timestamp = std::chrono::system_clock::now();

  m_printHistory.push_back(entry);

  // Write to log file if enabled
  if (m_fileLoggingEnabled) {
    CheckAndRotateLogFile();
    WriteToLogFile(entry);
  }

  // Limit history size
  while (m_printHistory.size() > m_maxEntries) {
    m_printHistory.pop_front();
  }
}

void ScriptPrintViewer::Clear() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_printHistory.clear();
}

void ScriptPrintViewer::RenderUI() {
  if (!m_isVisible) {
    return;
  }

  ImGui::Begin("Script Print Output", &m_isVisible);

  // Controls
  if (ImGui::Button("Clear")) {
    Clear();
  }

  ImGui::SameLine();
  ImGui::Checkbox("Auto-scroll", &m_autoScroll);

  ImGui::SameLine();
  ImGui::Checkbox("Show Timestamps", &m_showTimestamps);

  ImGui::SameLine();
  if (ImGui::Checkbox("Log to File", &m_fileLoggingEnabled)) {
    if (m_fileLoggingEnabled) {
      InitializeLogFile();
    }
    else {
      CloseLogFile();
    }
  }

  // Filter
  ImGui::InputText("Filter", m_filterBuffer, sizeof(m_filterBuffer));

  // Show current log file info if logging is enabled
  if (m_fileLoggingEnabled && m_logFile.is_open()) {
    ImGui::Text("Logging to: %s", GetLogFileName(m_currentLogDate).c_str());
  }

  ImGui::Separator();

  // Print output area
  ImGui::BeginChild("PrintOutput", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

  {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& entry : m_printHistory) {
      // Filter check
      if (strlen(m_filterBuffer) > 0) {
        if (entry.message.find(m_filterBuffer) == std::string::npos) {
          continue;
        }
      }

      // Format output
      if (m_showTimestamps) {
        // Convert timestamp to string
        auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);

        std::stringstream timestampStr;
        timestampStr << std::put_time(std::localtime(&time_t), "%H:%M:%S");

        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%s]", timestampStr.str().c_str());
        ImGui::SameLine();
      }

      ImGui::TextWrapped("%s", entry.message.c_str());
    }
  }

  // Auto-scroll to bottom
  if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
    ImGui::SetScrollHereY(1.0f);
  }

  ImGui::EndChild();

  ImGui::End();
}

void ScriptPrintViewer::InitializeLogFile() {
  try {
    // Create log directory if it doesn't exist
    std::filesystem::create_directories(m_logDirectory);

    // Get current date
    m_currentLogDate = std::chrono::system_clock::now();

    // Open log file
    std::string filename = GetLogFileName(m_currentLogDate);
    m_logFile.open(filename, std::ios::app);

    if (m_logFile.is_open()) {
      // Write header if file is new/empty
      std::filesystem::path filePath(filename);
      if (std::filesystem::file_size(filePath) == 0) {
        m_logFile << "=== Script Print Log Started at ";

        auto time_t = std::chrono::system_clock::to_time_t(m_currentLogDate);
        m_logFile << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        m_logFile << " ===" << std::endl;
        m_logFile << std::endl;
      }
    }
  }
  catch (const std::exception& e) {
    // If we can't create the log file, disable file logging
    m_fileLoggingEnabled = false;
  }
}

void ScriptPrintViewer::CheckAndRotateLogFile() {
  auto now = std::chrono::system_clock::now();

  // Convert to time_t and then to tm structure to compare dates
  auto currentTime_t = std::chrono::system_clock::to_time_t(now);
  auto logTime_t = std::chrono::system_clock::to_time_t(m_currentLogDate);

  struct tm currentTm, logTm;

  // Use localtime_s on Windows or localtime_r on Unix for thread safety
#ifdef _WIN32
  localtime_s(&currentTm, &currentTime_t);
  localtime_s(&logTm, &logTime_t);
#else
  localtime_r(&currentTime_t, &currentTm);
  localtime_r(&logTime_t, &logTm);
#endif

  // Compare year, month, and day
  if (currentTm.tm_year != logTm.tm_year ||
    currentTm.tm_mon != logTm.tm_mon ||
    currentTm.tm_mday != logTm.tm_mday) {
    CloseLogFile();
    InitializeLogFile();
  }
}

void ScriptPrintViewer::WriteToLogFile(const PrintEntry& entry) {
  if (!m_logFile.is_open()) {
    return;
  }

  // Format timestamp
  auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);

  m_logFile << "[";
  m_logFile << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  m_logFile << "] " << entry.message << std::endl;

  // Flush to ensure data is written
  m_logFile.flush();
}

std::string ScriptPrintViewer::GetLogFileName(const std::chrono::system_clock::time_point& date) {
  auto time_t = std::chrono::system_clock::to_time_t(date);

  std::stringstream filename;
  filename << m_logDirectory << "/script_print_";
  filename << std::put_time(std::localtime(&time_t), "%Y%m%d");
  filename << ".log";

  return filename.str();
}

void ScriptPrintViewer::CloseLogFile() {
  if (m_logFile.is_open()) {
    // Write closing message
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    m_logFile << std::endl;
    m_logFile << "=== Script Print Log Closed at ";
    m_logFile << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    m_logFile << " ===" << std::endl;

    m_logFile.close();
  }
}