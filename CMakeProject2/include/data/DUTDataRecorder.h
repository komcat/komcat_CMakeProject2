#pragma once

#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <filesystem>

class GlobalDataStore; // Forward declaration

class DUTDataRecorder {
public:
  DUTDataRecorder();
  ~DUTDataRecorder();

  // Start recording for a new DUT
  void Start(const std::string& serialNumber);

  // Add data point from global data store
  void AddDataPoint(const std::string& key, double value);

  // End current recording session
  void End();

  // Export methods
  bool ExportToCSV(const std::string& filename = "");
  bool ExportToJSON(const std::string& filename = "");

  // Getters
  const std::string& GetCurrentSerialNumber() const { return m_currentSerialNumber; }
  bool IsRecording() const { return m_isRecording; }
  size_t GetDataPointCount() const { return m_dataPoints.size(); }

private:
  struct DataPoint {
    std::string key;
    double value;
    std::chrono::system_clock::time_point timestamp;
  };

  std::string m_currentSerialNumber;
  std::vector<DataPoint> m_dataPoints;
  bool m_isRecording;
  std::chrono::system_clock::time_point m_sessionStartTime;

  // Helper methods
  std::string GetSaveDirectory();
  std::string GenerateFilename(const std::string& extension);
  void EnsureDirectoryExists(const std::string& path);
};