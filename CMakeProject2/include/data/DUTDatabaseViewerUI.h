#pragma once

#include "MenuManager_uaa3.h"
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <mutex>

// Forward declarations
struct sqlite3;
struct sqlite3_stmt;
class Logger;

class DUTDatabaseViewerUI : public IImguiUI {
public:
  DUTDatabaseViewerUI();
  ~DUTDatabaseViewerUI();

  // IImguiUI interface implementation
  void Render() override;
  void Show() override;
  void Hide() override;
  bool IsVisible() const override;
  const std::string& GetName() const override;

  // Configuration
  void SetDatabasePath(const std::string& path);
  void SetLogger(Logger* logger);

  // Initialize and connect to database
  bool Initialize();
  void Shutdown();

private:
  struct DataRecord {
    std::string serialNumber;
    std::string key;
    double value;
    std::string label;
    std::chrono::system_clock::time_point timestamp;
    std::string timestampStr;  // Pre-formatted for display
  };

  // UI State
  bool m_isVisible;
  std::string m_windowName;

  // Database
  std::string m_databasePath;
  sqlite3* m_db;  // Direct SQLite handle
  Logger* m_logger;

  // Data
  std::vector<std::string> m_serialNumbers;
  std::vector<DataRecord> m_displayData;
  std::string m_selectedSerial;
  int m_selectedSerialIndex;

  // Filters
  char m_serialFilter[256];
  std::vector<std::string> m_filteredSerialNumbers;

  // Table settings
  bool m_autoRefresh;
  float m_refreshInterval;
  std::chrono::steady_clock::time_point m_lastRefreshTime;

  // Pagination
  int m_recordsPerPage;
  int m_currentPage;
  int m_totalPages;

  // Statistics
  struct Stats {
    int totalRecords;
    double minValue;
    double maxValue;
    double avgValue;
    std::string mostRecentTimestamp;
  } m_stats;

  // Thread safety
  mutable std::mutex m_dataMutex;

  // Helper methods
  bool ConnectToDatabase();
  void DisconnectDatabase();
  void LoadSerialNumbers();
  void LoadDataForSerial(const std::string& serialNumber);
  void RefreshData();
  void ClearData();
  void UpdateStatistics();
  void ApplySerialFilter();

  // UI Rendering helpers
  void RenderControlPanel();
  void RenderSerialSelector();
  void RenderDataTable();
  void RenderStatisticsPanel();
  void RenderPagination();

  // Utility
  std::string FormatTimestamp(const std::chrono::system_clock::time_point& tp);
  void LogInfo(const std::string& message);
  void LogError(const std::string& message);
};