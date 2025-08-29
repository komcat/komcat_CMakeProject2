#include "DUTDataRecorder.h"
#include "include/data/global_data_store.h"
#include "SQLiteConnection.h"
#include "include/logger.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>

DUTDataRecorder::DUTDataRecorder()
  : m_isRecording(false),
  m_enableDatabase(false),
  m_batchSize(100),  // Default: flush every 100 points
  m_autoSaveInterval(30),  // Default: auto-flush every 30 seconds
  m_totalSavedToDb(0),
  m_databasePath("db/dut_db.db") {  // Fixed database location

  // Ensure directories exist
  EnsureDirectoryExists("dut_saved");  // For CSV/JSON exports
  EnsureDirectoryExists("db");  // For database file
}

DUTDataRecorder::~DUTDataRecorder() {
  if (m_isRecording) {
    End(); // Auto-save if still recording
  }
  if (m_dbConnection) {
    FlushToDatabase(); // Final flush
    DisconnectDatabase();
  }
}

bool DUTDataRecorder::ConnectToDatabase(const std::string& connectionString) {
  std::lock_guard<std::mutex> lock(m_dbMutex);

  // Use default path if no connection string provided
  std::string dbPath = connectionString.empty() ? m_databasePath : connectionString;

  // Update stored path
  m_databasePath = dbPath;

  // Ensure the database directory exists
  std::filesystem::path path(dbPath);
  if (path.has_parent_path()) {
    EnsureDirectoryExists(path.parent_path().string());
  }

  // Create database connection
  m_dbConnection = std::make_unique<SQLiteConnection>();

  if (!m_dbConnection) {
    auto* logger = Logger::GetInstance();
    logger->LogError("DUTDataRecorder: Database implementation not available");
    return false;
  }

  if (m_dbConnection->Connect(dbPath)) {
    auto* logger = Logger::GetInstance();
    logger->LogInfo("DUTDataRecorder: Connected to database at: " + dbPath);

    // SQLite automatically creates the database file if it doesn't exist
    // The CreateTableIfNotExists() in SQLiteConnection ensures tables are created

    m_enableDatabase = true;
    return true;
  }

  auto* logger = Logger::GetInstance();
  logger->LogError("DUTDataRecorder: Failed to connect to database at: " + dbPath);
  return false;
}

void DUTDataRecorder::DisconnectDatabase() {
  std::lock_guard<std::mutex> lock(m_dbMutex);

  if (m_dbConnection) {
    m_dbConnection->Disconnect();
    m_dbConnection.reset();
    m_enableDatabase = false;

    auto* logger = Logger::GetInstance();
    logger->LogInfo("DUTDataRecorder: Disconnected from database");
  }
}

void DUTDataRecorder::Start(const std::string& serialNumber) {
  if (m_isRecording) {
    End(); // End previous session
  }

  std::lock_guard<std::mutex> lock(m_dataMutex);

  m_currentSerialNumber = serialNumber;
  m_dataPoints.clear();
  m_pendingDataPoints.clear();
  m_isRecording = true;
  m_sessionStartTime = std::chrono::system_clock::now();
  m_lastFlushTime = m_sessionStartTime;
  m_totalSavedToDb = 0;

  auto* logger = Logger::GetInstance();
  logger->LogInfo("DUTDataRecorder: Started recording for DUT: " + serialNumber);
}

void DUTDataRecorder::AddDataPoint(const std::string& key, double value, const std::string& label) {
  if (!m_isRecording) {
    auto* logger = Logger::GetInstance();
    logger->LogWarning("DUTDataRecorder: Cannot add data point - not recording");
    return;
  }

  DataPoint point;
  point.key = key;
  point.value = value;
  point.label = label;  // Store label
  point.timestamp = std::chrono::system_clock::now();

  {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_dataPoints.push_back(point);

    // Add to pending buffer if database is enabled
    if (m_enableDatabase && m_dbConnection) {
      m_pendingDataPoints.push_back(point);
    }
  }

  // Check if we should flush to database
  CheckAutoFlush();
}


std::string DUTDataRecorder::GetLatestSerialNumber() {
  std::lock_guard<std::mutex> lock(m_dbMutex);

  if (!m_dbConnection || !m_dbConnection->IsConnected()) {
    return "";
  }

  // Cast to SQLiteConnection to access the GetLatestSerialNumber method
  SQLiteConnection* sqliteConn = static_cast<SQLiteConnection*>(m_dbConnection.get());
  if (!sqliteConn) {
    return "";
  }

  return sqliteConn->GetLatestSerialNumber();
}


void DUTDataRecorder::CheckAutoFlush() {
  if (!m_enableDatabase || !m_dbConnection) {
    return;
  }

  bool shouldFlush = false;

  {
    std::lock_guard<std::mutex> lock(m_dataMutex);

    // Check batch size trigger
    if (m_pendingDataPoints.size() >= m_batchSize) {
      shouldFlush = true;
    }

    // Check time interval trigger
    auto now = std::chrono::system_clock::now();
    auto timeSinceLastFlush = std::chrono::duration_cast<std::chrono::seconds>(
      now - m_lastFlushTime);

    if (timeSinceLastFlush >= m_autoSaveInterval && !m_pendingDataPoints.empty()) {
      shouldFlush = true;
    }
  }

  if (shouldFlush) {
    FlushToDatabase();
  }
}

void DUTDataRecorder::FlushToDatabase() {
  if (!m_enableDatabase || !m_dbConnection) {
    return;
  }

  std::vector<DataPoint> batchToSave;

  {
    std::lock_guard<std::mutex> lock(m_dataMutex);

    if (m_pendingDataPoints.empty()) {
      return;
    }

    // Move pending points to batch
    batchToSave = std::move(m_pendingDataPoints);
    m_pendingDataPoints.clear();
    m_lastFlushTime = std::chrono::system_clock::now();
  }

  // Save batch to database (outside of data mutex to avoid blocking AddDataPoint)
  if (SaveBatchToDatabase(batchToSave)) {
    m_totalSavedToDb += batchToSave.size();

    auto* logger = Logger::GetInstance();
    logger->LogInfo("DUTDataRecorder: Flushed " + std::to_string(batchToSave.size()) +
      " data points to database (Total: " + std::to_string(m_totalSavedToDb) + ")");
  }
  else {
    // On failure, add back to pending
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_pendingDataPoints.insert(m_pendingDataPoints.begin(),
      batchToSave.begin(), batchToSave.end());

    auto* logger = Logger::GetInstance();
    logger->LogError("DUTDataRecorder: Failed to save batch to database, data retained in buffer");
  }
}

bool DUTDataRecorder::SaveBatchToDatabase(const std::vector<DataPoint>& batch) {
  std::lock_guard<std::mutex> lock(m_dbMutex);

  if (!m_dbConnection || !m_dbConnection->IsConnected()) {
    return false;
  }

  // Start transaction for batch insert
  if (!m_dbConnection->BeginTransaction()) {
    auto* logger = Logger::GetInstance();
    logger->LogError("DUTDataRecorder: Failed to begin database transaction");
    return false;
  }

  // Insert all data points WITH LABELS
  for (const auto& point : batch) {
    // FIXED: Now passing the label parameter
    if (!m_dbConnection->InsertDataPoint(m_currentSerialNumber, point.key,
      point.value, point.timestamp, point.label)) {
      auto* logger = Logger::GetInstance();
      logger->LogError("DUTDataRecorder: Failed to insert data point: " +
        m_dbConnection->GetLastError());
      m_dbConnection->RollbackTransaction();
      return false;
    }
  }

  // Commit transaction
  if (!m_dbConnection->CommitTransaction()) {
    auto* logger = Logger::GetInstance();
    logger->LogError("DUTDataRecorder: Failed to commit transaction");
    m_dbConnection->RollbackTransaction();
    return false;
  }

  return true;
}

void DUTDataRecorder::End() {
  if (!m_isRecording) {
    return;
  }

  // Final flush to database
  if (m_enableDatabase && m_dbConnection) {
    FlushToDatabase();
  }

  m_isRecording = false;

  auto* logger = Logger::GetInstance();
  logger->LogInfo("DUTDataRecorder: Ended recording for DUT: " + m_currentSerialNumber +
    " (" + std::to_string(m_dataPoints.size()) + " total points, " +
    std::to_string(m_totalSavedToDb) + " saved to DB)");
}

bool DUTDataRecorder::ExportToCSV(const std::string& filename) {
  std::lock_guard<std::mutex> lock(m_dataMutex);

  if (m_dataPoints.empty()) {
    auto* logger = Logger::GetInstance();
    logger->LogWarning("DUTDataRecorder: No data to export");
    return false;
  }

  std::string filepath = filename.empty() ?
    GetSaveDirectory() + "/" + GenerateFilename("csv") :
    GetSaveDirectory() + "/" + filename;

  std::ofstream file(filepath);
  if (!file.is_open()) {
    auto* logger = Logger::GetInstance();
    logger->LogError("DUTDataRecorder: Failed to create CSV file: " + filepath);
    return false;
  }

  // Write header with label column
  file << "Serial_Number,Key,Value,Timestamp,Label\n";

  // Write data points
  for (const auto& point : m_dataPoints) {
    auto time_t = std::chrono::system_clock::to_time_t(point.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      point.timestamp.time_since_epoch()) % 1000;

    std::tm timeinfo;
    localtime_s(&timeinfo, &time_t);
    file << m_currentSerialNumber << ","
      << point.key << ","
      << std::fixed << std::setprecision(6) << point.value << ","
      << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S")
      << "." << std::setfill('0') << std::setw(3) << ms.count() << ","
      << point.label << "\n";  // Include label
  }

  file.close();
  auto* logger = Logger::GetInstance();
  logger->LogInfo("DUTDataRecorder: Data exported to CSV: " + filepath);
  return true;
}

bool DUTDataRecorder::ExportToJSON(const std::string& filename) {
  std::lock_guard<std::mutex> lock(m_dataMutex);

  if (m_dataPoints.empty()) {
    auto* logger = Logger::GetInstance();
    logger->LogWarning("DUTDataRecorder: No data to export");
    return false;
  }

  std::string filepath = filename.empty() ?
    GetSaveDirectory() + "/" + GenerateFilename("json") :
    GetSaveDirectory() + "/" + filename;

  std::ofstream file(filepath);
  if (!file.is_open()) {
    auto* logger = Logger::GetInstance();
    logger->LogError("DUTDataRecorder: Failed to create JSON file: " + filepath);
    return false;
  }

  // Write JSON structure
  file << "{\n";
  file << "  \"serial_number\": \"" << m_currentSerialNumber << "\",\n";

  auto start_time_t = std::chrono::system_clock::to_time_t(m_sessionStartTime);

  std::tm timeinfo;
  localtime_s(&timeinfo, &start_time_t);

  file << "  \"session_start\": \"" << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S") << "\",\n";
  file << "  \"data_point_count\": " << m_dataPoints.size() << ",\n";
  file << "  \"data_saved_to_db\": " << m_totalSavedToDb << ",\n";
  file << "  \"data_points\": [\n";

  for (size_t i = 0; i < m_dataPoints.size(); ++i) {
    const auto& point = m_dataPoints[i];
    auto time_t = std::chrono::system_clock::to_time_t(point.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      point.timestamp.time_since_epoch()) % 1000;

    std::tm timeinfo;
    localtime_s(&timeinfo, &time_t);

    file << "    {\n";
    file << "      \"key\": \"" << point.key << "\",\n";
    file << "      \"value\": " << std::fixed << std::setprecision(6) << point.value << ",\n";
    file << "      \"label\": \"" << point.label << "\",\n";  // Added label to JSON
    file << "      \"timestamp\": \"" << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S")
      << "." << std::setfill('0') << std::setw(3) << ms.count() << "\"\n";
    file << "    }";

    if (i < m_dataPoints.size() - 1) {
      file << ",";
    }
    file << "\n";
  }

  file << "  ]\n";
  file << "}\n";

  file.close();

  auto* logger = Logger::GetInstance();
  logger->LogInfo("DUTDataRecorder: Exported JSON to: " + filepath);
  return true;
}

std::string DUTDataRecorder::GetSaveDirectory() {
  return "dut_saved";
}

std::string DUTDataRecorder::GenerateFilename(const std::string& extension) {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;

  std::tm timeinfo;
  localtime_s(&timeinfo, &time_t);

  ss << m_currentSerialNumber << "_"
    << std::put_time(&timeinfo, "%Y%m%d_%H%M%S")
    << "." << extension;
  return ss.str();
}

void DUTDataRecorder::EnsureDirectoryExists(const std::string& path) {
  try {
    if (!std::filesystem::exists(path)) {
      std::filesystem::create_directories(path);
      auto* logger = Logger::GetInstance();
      logger->LogInfo("DUTDataRecorder: Created directory: " + path);
    }
  }
  catch (const std::filesystem::filesystem_error& e) {
    auto* logger = Logger::GetInstance();
    logger->LogError("DUTDataRecorder: Failed to create directory " + path + ": " + e.what());
  }
}