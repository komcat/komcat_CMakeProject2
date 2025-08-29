#pragma once

#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <atomic>

class GlobalDataStore; // Forward declaration
class DatabaseConnection; // Forward declaration

class DUTDataRecorder {
public:
  DUTDataRecorder();
  ~DUTDataRecorder();

  // Configuration methods
  void SetBatchSize(size_t size) { m_batchSize = size; }
  void SetAutoSaveInterval(std::chrono::seconds interval) { m_autoSaveInterval = interval; }
  void EnableDatabaseSave(bool enable) { m_enableDatabase = enable; }

  // Configuration getters
  size_t GetBatchSize() const { return m_batchSize; }
  std::chrono::seconds GetAutoSaveInterval() const { return m_autoSaveInterval; }
  bool IsDatabaseEnabled() const { return m_enableDatabase; }

  // Database connection
  bool ConnectToDatabase(const std::string& connectionString = "");
  void DisconnectDatabase();
  bool IsDatabaseConnected() const { return m_dbConnection != nullptr; }
  std::string GetDatabasePath() const { return m_databasePath; }

  // Start recording for a new DUT
  void Start(const std::string& serialNumber);

  // Add data point from global data store
  // Updated method with label parameter
  void AddDataPoint(const std::string& key, double value, const std::string& label = "");

  // NEW: Get the most recent serial number from database
  std::string GetLatestSerialNumber();
  // End current recording session
  void End();

  // Manual flush to database
  void FlushToDatabase();

  // Export methods (still available as backup)
  bool ExportToCSV(const std::string& filename = "");
  bool ExportToJSON(const std::string& filename = "");

  // Getters
  const std::string& GetCurrentSerialNumber() const { return m_currentSerialNumber; }
  bool IsRecording() const { return m_isRecording; }
  size_t GetDataPointCount() const { return m_dataPoints.size(); }
  size_t GetPendingDataCount() const { return m_pendingDataPoints.size(); }
  size_t GetTotalSavedCount() const { return m_totalSavedToDb; }

private:
  struct DataPoint {
    std::string key;
    double value;
    std::string label;  // NEW: label field
    std::chrono::system_clock::time_point timestamp;
  };

  // Recording state
  std::string m_currentSerialNumber;
  std::vector<DataPoint> m_dataPoints;  // All data points (for file export)
  std::vector<DataPoint> m_pendingDataPoints;  // Buffer for database batch
  bool m_isRecording;
  std::chrono::system_clock::time_point m_sessionStartTime;
  std::chrono::system_clock::time_point m_lastFlushTime;

  // Database configuration
  std::unique_ptr<DatabaseConnection> m_dbConnection;
  bool m_enableDatabase;
  size_t m_batchSize;
  std::chrono::seconds m_autoSaveInterval;
  std::atomic<size_t> m_totalSavedToDb;
  std::string m_databasePath;

  // Thread safety
  mutable std::mutex m_dataMutex;
  mutable std::mutex m_dbMutex;

  // Helper methods
  std::string GetSaveDirectory();
  std::string GenerateFilename(const std::string& extension);
  void EnsureDirectoryExists(const std::string& path);

  // Database helpers
  bool ShouldFlush() const;
  bool SaveBatchToDatabase(const std::vector<DataPoint>& batch);
  void CheckAutoFlush();
};

// Simple Database Connection Interface
class DatabaseConnection {
public:
  virtual ~DatabaseConnection() = default;

  virtual bool Connect(const std::string& connectionString) = 0;
  virtual void Disconnect() = 0;
  virtual bool IsConnected() const = 0;

  virtual bool BeginTransaction() = 0;
  virtual bool CommitTransaction() = 0;
  virtual bool RollbackTransaction() = 0;

  // Updated method signature to include label parameter
  virtual bool InsertDataPoint(
    const std::string& serialNumber,
    const std::string& key,
    double value,
    const std::chrono::system_clock::time_point& timestamp,
    const std::string& label = ""  // NEW: label parameter with default
  ) = 0;

  virtual std::string GetLastError() const = 0;
};