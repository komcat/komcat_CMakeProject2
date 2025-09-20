// DUTOperations.cpp

// CRITICAL: Include Windows/Winsock headers FIRST to prevent conflicts
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

// NOW include other headers
#include "AppContext.h"  // Safe to include after winsock headers
#include "include/data/DUTDataRecorder.h"
#include "UserInputOperations.h"
#include "SequenceStep.h"
#include "include/logger.h"

bool DUTStartRecordingOperation::Execute(MachineOperations& ops) {
  AppContext* context = &AppContext::GetInstance();
  DUTDataRecorder* recorder = context->GetDUTDataRecorder();

  if (!recorder) {
    ops.LogError("DUTDataRecorder not available in AppContext");
    return false;
  }

  // AUTO-INITIALIZE DATABASE CONNECTION if not already connected
  if (!recorder->IsDatabaseConnected()) {
    ops.LogInfo("Initializing database connection...");

    // Configure batch settings (you can adjust these defaults)
    recorder->SetBatchSize(100);  // Flush every 100 points
    recorder->SetAutoSaveInterval(std::chrono::seconds(30));  // Or every 30 seconds

    // Connect to default database (db/dut_db.db)
    // This will automatically:
    // 1. Create 'db' folder if it doesn't exist
    // 2. Create 'dut_db.db' file if it doesn't exist
    // 3. Create tables if they don't exist
    if (recorder->ConnectToDatabase()) {
      ops.LogInfo("Database connected successfully: " + recorder->GetDatabasePath());
      ops.LogInfo("Auto-save enabled (batch: " +
        std::to_string(recorder->GetBatchSize()) +
        " points, interval: " +
        std::to_string(recorder->GetAutoSaveInterval().count()) + "s)");
    }
    else {
      ops.LogWarning("Database connection failed - will save to files only");
      // Not a fatal error - continue with file-only mode
    }
  }

  // Start recording session
  recorder->Start(m_serialNumber);
  ops.LogInfo("Started DUT recording for: " + m_serialNumber);

  // Log current status
  if (recorder->IsDatabaseConnected()) {
    ops.LogInfo("Database auto-save is active");
  }
  else {
    ops.LogInfo("Recording to memory (file export only)");
  }

  return true;
}

bool DUTRecordDataOperation::Execute(MachineOperations& ops) {
  AppContext* context = &AppContext::GetInstance();
  DUTDataRecorder* recorder = context->GetDUTDataRecorder();

  if (!recorder || !recorder->IsRecording()) {
    ops.LogWarning("DUT not recording - skipping data point");
    return true; // Non-fatal
  }

  // Get value from GlobalDataStore
  double value = 0.0;
  if (ops.GetDataValue(m_dataKey, value)) {
    // This AUTOMATICALLY saves to database when batch threshold is reached!
    // Pass label to recorder (once DUTDataRecorder is updated)
    recorder->AddDataPoint(m_dataKey, value, m_label);

    // Log first few points and then periodically
    static size_t totalPoints = 0;
    totalPoints++;

    if (totalPoints <= 5 || totalPoints % 100 == 0) {  // First 5 points, then every 100
      std::string logMsg = "Recorded: " + m_dataKey + " = " + std::to_string(value);
      if (!m_label.empty()) {
        logMsg += " [" + m_label + "]";
      }
      ops.LogDebug(logMsg);

      if (totalPoints % 100 == 0) {  // Detailed status every 100 points
        ops.LogInfo("DUT Progress - Total: " +
          std::to_string(recorder->GetDataPointCount()) +
          ", Saved to DB: " +
          std::to_string(recorder->GetTotalSavedCount()) +
          ", Pending: " +
          std::to_string(recorder->GetPendingDataCount()));
      }
    }
  }
  else {
    ops.LogWarning("Data key not found: " + m_dataKey);
  }

  return true;
}

bool DUTEndRecordingOperation::Execute(MachineOperations& ops) {
  AppContext* context = &AppContext::GetInstance();
  DUTDataRecorder* recorder = context->GetDUTDataRecorder();

  if (!recorder) {
    ops.LogError("DUTDataRecorder not available");
    return false;
  }

  // Get final statistics before ending
  size_t totalPoints = recorder->GetDataPointCount();
  size_t savedToDb = recorder->GetTotalSavedCount();
  size_t pending = recorder->GetPendingDataCount();

  // End() automatically flushes remaining data to database
  recorder->End();

  // Force final flush to ensure all data is saved
  if (recorder->IsDatabaseConnected() && pending > 0) {
    recorder->FlushToDatabase();
    savedToDb = recorder->GetTotalSavedCount();  // Update count
    ops.LogInfo("Final database flush complete");
  }

  // File export as backup
  bool csvSuccess = false;
  bool jsonSuccess = false;

  if (m_exportCsv) {
    csvSuccess = recorder->ExportToCSV();
    if (csvSuccess) {
      ops.LogInfo("CSV backup exported to: dut_saved/");
    }
    else {
      ops.LogWarning("CSV export failed");
    }
  }

  if (m_exportJson) {
    jsonSuccess = recorder->ExportToJSON();
    if (jsonSuccess) {
      ops.LogInfo("JSON backup exported to: dut_saved/");
    }
    else {
      ops.LogWarning("JSON export failed");
    }
  }

  // Summary log
  ops.LogInfo("=== DUT Recording Summary ===");
  ops.LogInfo("Serial Number: " + recorder->GetCurrentSerialNumber());
  ops.LogInfo("Total Data Points: " + std::to_string(totalPoints));

  if (recorder->IsDatabaseConnected()) {
    ops.LogInfo("Saved to Database: " + std::to_string(savedToDb) + " points");
    ops.LogInfo("Database Location: " + recorder->GetDatabasePath());
  }

  if (csvSuccess || jsonSuccess) {
    ops.LogInfo("File Backups: " +
      std::string(csvSuccess ? "CSV " : "") +
      std::string(jsonSuccess ? "JSON" : ""));
  }
  ops.LogInfo("============================");

  return true;
}

// NEW: Configuration operation to customize database settings
class DUTConfigureDatabaseOperation : public SequenceOperation {
public:
  DUTConfigureDatabaseOperation(
    size_t batchSize = 100,
    std::chrono::seconds interval = std::chrono::seconds(30),
    const std::string& dbPath = "")
    : m_batchSize(batchSize),
    m_interval(interval),
    m_dbPath(dbPath) {
  }

  bool Execute(MachineOperations& ops) override {
    AppContext* context = &AppContext::GetInstance();
    DUTDataRecorder* recorder = context->GetDUTDataRecorder();

    if (!recorder) {
      ops.LogError("DUTDataRecorder not available");
      return false;
    }

    // Configure settings
    recorder->SetBatchSize(m_batchSize);
    recorder->SetAutoSaveInterval(m_interval);

    // Connect to database if not already connected
    if (!recorder->IsDatabaseConnected()) {
      std::string path = m_dbPath.empty() ? "db/dut_db.db" : m_dbPath;
      if (recorder->ConnectToDatabase(path)) {
        ops.LogInfo("Database configured: " + path);
      }
      else {
        ops.LogWarning("Database configuration failed");
      }
    }

    return true;
  }

  std::string GetDescription() const override {
    return "Configure DUT database settings";
  }

private:
  size_t m_batchSize;
  std::chrono::seconds m_interval;
  std::string m_dbPath;
};

// Manual flush operation (unchanged)
class DUTFlushToDatabaseOperation : public SequenceOperation {
public:
  bool Execute(MachineOperations& ops) override {
    AppContext* context = &AppContext::GetInstance();
    DUTDataRecorder* recorder = context->GetDUTDataRecorder();

    if (!recorder || !recorder->IsRecording()) {
      ops.LogWarning("Cannot flush - recorder not active");
      return true;
    }

    if (!recorder->IsDatabaseConnected()) {
      ops.LogWarning("Cannot flush - database not connected");
      return true;
    }

    size_t pendingBefore = recorder->GetPendingDataCount();
    if (pendingBefore == 0) {
      ops.LogDebug("No pending data to flush");
      return true;
    }

    recorder->FlushToDatabase();

    ops.LogInfo("Manual flush completed - " +
      std::to_string(pendingBefore) + " points saved to database");
    return true;
  }

  std::string GetDescription() const override {
    return "Force flush pending DUT data to database";
  }
};





