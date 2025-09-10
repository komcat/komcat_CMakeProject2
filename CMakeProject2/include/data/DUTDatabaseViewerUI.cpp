#include "DUTDatabaseViewerUI.h"
#include "include/logger.h"
#include "imgui.h"
#include <sqlite3.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <filesystem>

DUTDatabaseViewerUI::DUTDatabaseViewerUI()
  : m_isVisible(false),
  m_windowName("DUT Database Viewer"),
  m_databasePath("db/dut_db.db"),
  m_db(nullptr),
  m_logger(nullptr),
  m_selectedSerialIndex(-1),
  m_autoRefresh(false),
  m_refreshInterval(5.0f),
  m_recordsPerPage(100),
  m_currentPage(0),
  m_totalPages(0) {

  std::memset(m_serialFilter, 0, sizeof(m_serialFilter));
  m_lastRefreshTime = std::chrono::steady_clock::now();

  // Initialize stats
  m_stats.totalRecords = 0;
  m_stats.minValue = 0.0;
  m_stats.maxValue = 0.0;
  m_stats.avgValue = 0.0;
}

DUTDatabaseViewerUI::~DUTDatabaseViewerUI() {
  Shutdown();
}

void DUTDatabaseViewerUI::Show() {
  m_isVisible = true;
  if (!m_db && !Initialize()) {
    LogError("Failed to initialize database connection");
  }
}

void DUTDatabaseViewerUI::Hide() {
  m_isVisible = false;
}

bool DUTDatabaseViewerUI::IsVisible() const {
  return m_isVisible;
}

const std::string& DUTDatabaseViewerUI::GetName() const {
  return m_windowName;
}

void DUTDatabaseViewerUI::SetDatabasePath(const std::string& path) {
  m_databasePath = path;
}

void DUTDatabaseViewerUI::SetLogger(Logger* logger) {
  m_logger = logger;
}

bool DUTDatabaseViewerUI::Initialize() {
  if (!ConnectToDatabase()) {
    return false;
  }

  LoadSerialNumbers();
  return true;
}

void DUTDatabaseViewerUI::Shutdown() {
  DisconnectDatabase();
  ClearData();
}

bool DUTDatabaseViewerUI::ConnectToDatabase() {
  std::lock_guard<std::mutex> lock(m_dataMutex);

  // Check if database file exists
  if (!std::filesystem::exists(m_databasePath)) {
    LogError("Database file does not exist: " + m_databasePath);
    return false;
  }

  int rc = sqlite3_open(m_databasePath.c_str(), &m_db);

  if (rc != SQLITE_OK) {
    LogError("Failed to open database: " + std::string(sqlite3_errmsg(m_db)));
    sqlite3_close(m_db);
    m_db = nullptr;
    return false;
  }

  // Enable WAL mode for better concurrent access
  char* errMsg = nullptr;
  rc = sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    LogError("Failed to set WAL mode: " + std::string(errMsg));
    sqlite3_free(errMsg);
  }

  LogInfo("Connected to database: " + m_databasePath);
  return true;
}

void DUTDatabaseViewerUI::DisconnectDatabase() {
  std::lock_guard<std::mutex> lock(m_dataMutex);

  if (m_db) {
    sqlite3_close(m_db);
    m_db = nullptr;
    LogInfo("Disconnected from database");
  }
}

void DUTDatabaseViewerUI::LoadSerialNumbers() {
  if (!m_db) {
    return;
  }

  std::lock_guard<std::mutex> lock(m_dataMutex);

  m_serialNumbers.clear();

  const char* sql = "SELECT DISTINCT serial_number FROM dut_data ORDER BY serial_number";
  sqlite3_stmt* stmt = nullptr;

  int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    LogError("Failed to prepare statement: " + std::string(sqlite3_errmsg(m_db)));
    return;
  }

  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    const char* serial = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    if (serial) {
      m_serialNumbers.push_back(serial);
    }
  }

  sqlite3_finalize(stmt);

  m_filteredSerialNumbers = m_serialNumbers;

  LogInfo("Loaded " + std::to_string(m_serialNumbers.size()) + " serial numbers");
}

void DUTDatabaseViewerUI::LoadDataForSerial(const std::string& serialNumber) {
  if (!m_db || serialNumber.empty()) {
    return;
  }

  std::lock_guard<std::mutex> lock(m_dataMutex);

  m_displayData.clear();
  m_selectedSerial = serialNumber;

  // Query data for selected serial number
  const char* sql = "SELECT key, value, label, timestamp FROM dut_data "
    "WHERE serial_number = ? ORDER BY timestamp DESC";

  sqlite3_stmt* stmt = nullptr;

  int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    LogError("Failed to prepare statement: " + std::string(sqlite3_errmsg(m_db)));
    return;
  }

  sqlite3_bind_text(stmt, 1, serialNumber.c_str(), -1, SQLITE_TRANSIENT);

  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    DataRecord record;
    record.serialNumber = serialNumber;

    const char* key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    if (key) record.key = key;

    record.value = sqlite3_column_double(stmt, 1);

    const char* label = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    if (label) record.label = label;

    const char* timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    if (timestamp) {
      record.timestampStr = timestamp;
      // Parse timestamp for internal use if needed
      record.timestamp = std::chrono::system_clock::now(); // Simplified for now
    }

    m_displayData.push_back(record);
  }

  sqlite3_finalize(stmt);

  // Update pagination
  m_totalPages = (m_displayData.size() + m_recordsPerPage - 1) / m_recordsPerPage;
  m_currentPage = 0;

  UpdateStatistics();

  LogInfo("Loaded " + std::to_string(m_displayData.size()) +
    " records for serial: " + serialNumber);
}

void DUTDatabaseViewerUI::RefreshData() {
  if (!m_selectedSerial.empty()) {
    LoadDataForSerial(m_selectedSerial);
  }
  else {
    LoadSerialNumbers();
  }
}

void DUTDatabaseViewerUI::ClearData() {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  m_displayData.clear();
  m_serialNumbers.clear();
  m_filteredSerialNumbers.clear();
  m_selectedSerial.clear();
  m_selectedSerialIndex = -1;
}

void DUTDatabaseViewerUI::UpdateStatistics() {
  m_stats.totalRecords = static_cast<int>(m_displayData.size());

  if (m_displayData.empty()) {
    m_stats.minValue = 0.0;
    m_stats.maxValue = 0.0;
    m_stats.avgValue = 0.0;
    m_stats.mostRecentTimestamp = "N/A";
    return;
  }

  double sum = 0.0;
  m_stats.minValue = m_displayData[0].value;
  m_stats.maxValue = m_displayData[0].value;
  auto mostRecentTime = m_displayData[0].timestamp;

  for (const auto& record : m_displayData) {
    sum += record.value;
    m_stats.minValue = (std::min)(m_stats.minValue, record.value);
    m_stats.maxValue = (std::max)(m_stats.maxValue, record.value);

    if (record.timestamp > mostRecentTime) {
      mostRecentTime = record.timestamp;
    }
  }

  m_stats.avgValue = sum / m_displayData.size();
  m_stats.mostRecentTimestamp = FormatTimestamp(mostRecentTime);
}

void DUTDatabaseViewerUI::ApplySerialFilter() {
  m_filteredSerialNumbers.clear();

  std::string filter(m_serialFilter);

  if (filter.empty()) {
    m_filteredSerialNumbers = m_serialNumbers;
    return;
  }

  // Convert filter to lowercase for case-insensitive search
  std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

  for (const auto& serial : m_serialNumbers) {
    std::string serialLower = serial;
    std::transform(serialLower.begin(), serialLower.end(), serialLower.begin(), ::tolower);

    if (serialLower.find(filter) != std::string::npos) {
      m_filteredSerialNumbers.push_back(serial);
    }
  }
}

void DUTDatabaseViewerUI::Render() {
  if (!m_isVisible) return;

  // Auto-refresh logic
  if (m_autoRefresh) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastRefreshTime);

    if (elapsed.count() >= m_refreshInterval) {
      RefreshData();
      m_lastRefreshTime = now;
    }
  }

  // Main window
  ImGui::SetNextWindowSize(ImVec2(1000, 600), ImGuiCond_FirstUseEver);

  if (ImGui::Begin(m_windowName.c_str(), &m_isVisible)) {
    // Check database connection
    if (!m_db) {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Database not connected!");
      if (ImGui::Button("Connect")) {
        Initialize();
      }
    }
    else {
      RenderControlPanel();
      ImGui::Separator();

      // Split view: serial selector on left, data on right
      float leftPanelWidth = 250.0f;

      // Left panel - Serial selector
      ImGui::BeginChild("SerialPanel", ImVec2(leftPanelWidth, 0), true);
      RenderSerialSelector();
      ImGui::EndChild();

      ImGui::SameLine();

      // Right panel - Data view
      ImGui::BeginChild("DataPanel", ImVec2(0, 0), true);

      if (!m_selectedSerial.empty()) {
        RenderStatisticsPanel();
        ImGui::Separator();
        RenderDataTable();
        RenderPagination();
      }
      else {
        ImGui::Text("Select a serial number to view data");
      }

      ImGui::EndChild();
    }
  }
  ImGui::End();
}

void DUTDatabaseViewerUI::RenderControlPanel() {
  // Database info
  ImGui::Text("Database: %s", m_databasePath.c_str());
  ImGui::SameLine();

  if (ImGui::Button("Refresh")) {
    RefreshData();
  }

  ImGui::SameLine();
  ImGui::Checkbox("Auto Refresh", &m_autoRefresh);

  if (m_autoRefresh) {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::DragFloat("Interval (s)", &m_refreshInterval, 0.5f, 1.0f, 60.0f);
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(100);
  if (ImGui::InputInt("Per Page", &m_recordsPerPage, 10, 100)) {
    if (m_recordsPerPage < 10) m_recordsPerPage = 10;
    if (m_recordsPerPage > 1000) m_recordsPerPage = 1000;

    // Recalculate pagination
    if (!m_displayData.empty()) {
      m_totalPages = (static_cast<int>(m_displayData.size()) + m_recordsPerPage - 1) / m_recordsPerPage;
      if (m_currentPage >= m_totalPages) {
        m_currentPage = m_totalPages - 1;
      }
    }
  }
}

void DUTDatabaseViewerUI::RenderSerialSelector() {
  ImGui::Text("Serial Numbers (%zu)", m_serialNumbers.size());
  ImGui::Separator();

  // Filter input
  if (ImGui::InputText("Filter", m_serialFilter, sizeof(m_serialFilter))) {
    ApplySerialFilter();
  }

  ImGui::Separator();

  // Serial number list
  ImGui::BeginChild("SerialList", ImVec2(0, 0), false);

  for (const auto& serial : m_filteredSerialNumbers) {
    bool isSelected = (serial == m_selectedSerial);

    if (ImGui::Selectable(serial.c_str(), isSelected)) {
      LoadDataForSerial(serial);
    }

    // Highlight selected
    if (isSelected) {
      ImGui::SetItemDefaultFocus();
    }
  }

  ImGui::EndChild();
}

void DUTDatabaseViewerUI::RenderDataTable() {
  // Table
  if (ImGui::BeginTable("DataTable", 5,
    ImGuiTableFlags_Borders |
    ImGuiTableFlags_RowBg |
    ImGuiTableFlags_Resizable |
    ImGuiTableFlags_ScrollY |
    ImGuiTableFlags_SizingStretchProp)) {

    // Headers
    ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch, 0.3f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.2f);
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 0.2f);
    ImGui::TableSetupColumn("Timestamp", ImGuiTableColumnFlags_WidthStretch, 0.3f);
    ImGui::TableSetupScrollFreeze(0, 1); // Freeze header row
    ImGui::TableHeadersRow();

    // Calculate display range based on pagination
    int startIdx = m_currentPage * m_recordsPerPage;
    int endIdx = (std::min)(startIdx + m_recordsPerPage, static_cast<int>(m_displayData.size()));

    // Data rows
    for (int i = startIdx; i < endIdx; ++i) {
      const auto& record = m_displayData[i];

      ImGui::TableNextRow();

      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%d", i + 1);

      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%s", record.key.c_str());

      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%.6f", record.value);

      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%s", record.label.c_str());

      ImGui::TableSetColumnIndex(4);
      ImGui::Text("%s", record.timestampStr.c_str());
    }

    ImGui::EndTable();
  }
}

void DUTDatabaseViewerUI::RenderStatisticsPanel() {
  ImGui::Text("Statistics for: %s", m_selectedSerial.c_str());
  ImGui::Separator();

  // Use columns for neat alignment
  ImGui::Columns(2, nullptr, false);

  ImGui::Text("Total Records:");
  ImGui::NextColumn();
  ImGui::Text("%d", m_stats.totalRecords);
  ImGui::NextColumn();

  ImGui::Text("Min Value:");
  ImGui::NextColumn();
  ImGui::Text("%.6f", m_stats.minValue);
  ImGui::NextColumn();

  ImGui::Text("Max Value:");
  ImGui::NextColumn();
  ImGui::Text("%.6f", m_stats.maxValue);
  ImGui::NextColumn();

  ImGui::Text("Avg Value:");
  ImGui::NextColumn();
  ImGui::Text("%.6f", m_stats.avgValue);
  ImGui::NextColumn();

  ImGui::Text("Most Recent:");
  ImGui::NextColumn();
  ImGui::Text("%s", m_stats.mostRecentTimestamp.c_str());

  ImGui::Columns(1);
}

void DUTDatabaseViewerUI::RenderPagination() {
  if (m_totalPages <= 1) return;

  ImGui::Separator();

  // Previous button
  if (ImGui::Button("<<")) {
    m_currentPage = 0;
  }
  ImGui::SameLine();

  if (ImGui::Button("<")) {
    if (m_currentPage > 0) m_currentPage--;
  }
  ImGui::SameLine();

  // Page info
  ImGui::Text("Page %d / %d", m_currentPage + 1, m_totalPages);
  ImGui::SameLine();

  // Next button
  if (ImGui::Button(">")) {
    if (m_currentPage < m_totalPages - 1) m_currentPage++;
  }
  ImGui::SameLine();

  if (ImGui::Button(">>")) {
    m_currentPage = m_totalPages - 1;
  }

  // Records info
  ImGui::SameLine();
  int startRecord = m_currentPage * m_recordsPerPage + 1;
  int endRecord = (std::min)((m_currentPage + 1) * m_recordsPerPage, static_cast<int>(m_displayData.size()));
  ImGui::Text("  (Showing %d-%d of %d)", startRecord, endRecord, static_cast<int>(m_displayData.size()));
}

std::string DUTDatabaseViewerUI::FormatTimestamp(const std::chrono::system_clock::time_point& tp) {
  auto time_t = std::chrono::system_clock::to_time_t(tp);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    tp.time_since_epoch()) % 1000;

  std::tm timeinfo;
  localtime_s(&timeinfo, &time_t);

  std::ostringstream oss;
  oss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
  oss << "." << std::setfill('0') << std::setw(3) << ms.count();

  return oss.str();
}

void DUTDatabaseViewerUI::LogInfo(const std::string& message) {
  if (m_logger) {
    m_logger->LogInfo("DUTDatabaseViewerUI: " + message);
  }
}

void DUTDatabaseViewerUI::LogError(const std::string& message) {
  if (m_logger) {
    m_logger->LogError("DUTDatabaseViewerUI: " + message);
  }
}