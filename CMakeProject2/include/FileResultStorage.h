// FileResultStorage.h
#ifndef FILERESULTSTORAGE_H
#define FILERESULTSTORAGE_H

#include "IResultStorage.h"
#include <mutex>
#include <filesystem>

class FileResultStorage : public IResultStorage {
private:
  std::string baseDirectory;
  mutable std::mutex storageMutex;
  bool initialized = false;

  // File organization: baseDir/YYYY-MM-DD/deviceType/deviceId/HH-MM-SS-mmm_resultType_label.json
  std::filesystem::path GenerateFilePath(const Record& record) const;
  std::filesystem::path ParseDatePath(const std::chrono::system_clock::time_point& timestamp) const;

  // JSON serialization
  std::string SerializeRecord(const Record& record) const;
  Record DeserializeRecord(const std::string& jsonStr) const;

  // File operations
  bool WriteRecordToFile(const Record& record, const std::filesystem::path& path) const;
  std::optional<Record> ReadRecordFromFile(const std::filesystem::path& path) const;

  // Helper for filtering
  bool MatchesFilter(const Record& record, const QueryFilter& filter) const;

public:
  FileResultStorage() = default;
  virtual ~FileResultStorage() { Close(); }

  // Core operations
  bool Initialize(const std::string& connectionString) override;
  bool Close() override;
  bool IsInitialized() const override { return initialized; }

  // Store operations
  bool Store(const Record& record) override;
  bool StoreBatch(const std::vector<Record>& records) override;

  // Query operations
  std::vector<Record> Query(const QueryFilter& filter) const override;
  std::optional<Record> GetLatest(const std::string& deviceId = "") const override;
  size_t GetRecordCount(const QueryFilter& filter = {}) const override;

  // Management
  bool Delete(const QueryFilter& filter) override;
  bool Clear() override;
  size_t GetStorageSize() const override;

  // Export/Import
  bool ExportToFile(const std::string& filename, const QueryFilter& filter = {}) const override;
  bool ImportFromFile(const std::string& filename) override;
};

#endif // FILERESULTSTORAGE_H