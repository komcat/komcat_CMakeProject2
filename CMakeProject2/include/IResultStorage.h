// IResultStorage.h
#ifndef IRESULTSTORAGE_H
#define IRESULTSTORAGE_H

#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <map>

class IResultStorage {
public:
  struct Record {
    // Core fields - applicable to any device
    std::string deviceId;
    std::string deviceType;     // "PowerSupply", "Oscilloscope", "Multimeter", etc.
    std::chrono::system_clock::time_point timestamp;
    std::string resultType;      // "measurement", "sweep", "waveform", "spectrum", etc.
    std::string label;           // User-defined label

    // Flexible key-value store for any data
    std::map<std::string, double> numericValues;     // voltage: 3.3, current: 0.5
    std::map<std::string, std::string> stringValues; // status: "on", mode: "CV"
    std::map<std::string, std::vector<double>> arrayValues; // waveform: [1.0, 1.1, ...]

    // Optional metadata
    std::map<std::string, std::string> metadata;     // operator, test_condition, etc.
  };

  struct QueryFilter {
    std::optional<std::string> deviceId;
    std::optional<std::string> deviceType;
    std::optional<std::string> resultType;
    std::optional<std::string> label;
    std::optional<std::chrono::system_clock::time_point> startTime;
    std::optional<std::chrono::system_clock::time_point> endTime;
    std::map<std::string, std::string> metadataFilters;  // key-value pairs to match
    size_t maxResults = 100;
    std::string orderBy = "timestamp";  // "timestamp", "deviceId", etc.
    bool ascending = false;  // false = newest first
  };

  virtual ~IResultStorage() = default;

  // Core operations
  virtual bool Initialize(const std::string& connectionString) = 0;
  virtual bool Close() = 0;
  virtual bool IsInitialized() const = 0;

  // Store a generic record
  virtual bool Store(const Record& record) = 0;

  // Batch store for efficiency
  virtual bool StoreBatch(const std::vector<Record>& records) = 0;

  // Query operations
  virtual std::vector<Record> Query(const QueryFilter& filter) const = 0;
  virtual std::optional<Record> GetLatest(const std::string& deviceId = "") const = 0;
  virtual size_t GetRecordCount(const QueryFilter& filter = {}) const = 0;

  // Management
  virtual bool Delete(const QueryFilter& filter) = 0;
  virtual bool Clear() = 0;
  virtual size_t GetStorageSize() const = 0;

  // Export/Import
  virtual bool ExportToFile(const std::string& filename, const QueryFilter& filter = {}) const = 0;
  virtual bool ImportFromFile(const std::string& filename) = 0;
};

#endif // IRESULTSTORAGE_H