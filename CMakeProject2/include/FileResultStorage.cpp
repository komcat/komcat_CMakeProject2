// FileResultStorage.cpp
#include "FileResultStorage.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <nlohmann/json.hpp>  // Using nlohmann/json for simplicity

using json = nlohmann::json;

bool FileResultStorage::Initialize(const std::string& connectionString) {
  std::lock_guard<std::mutex> lock(storageMutex);

  baseDirectory = connectionString;

  // Create base directory if it doesn't exist
  try {
    if (!std::filesystem::exists(baseDirectory)) {
      std::filesystem::create_directories(baseDirectory);
    }
    initialized = true;
    return true;
  }
  catch (const std::filesystem::filesystem_error& ) {
    initialized = false;
    return false;
  }
}

bool FileResultStorage::Close() {
  std::lock_guard<std::mutex> lock(storageMutex);
  initialized = false;
  return true;
}

std::filesystem::path FileResultStorage::ParseDatePath(
  const std::chrono::system_clock::time_point& timestamp) const {

  std::time_t time_t = std::chrono::system_clock::to_time_t(timestamp);
  std::tm tm{};
  localtime_s(&tm, &time_t);

  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d");
  return oss.str();
}

std::filesystem::path FileResultStorage::GenerateFilePath(const Record& record) const {
  auto time_t = std::chrono::system_clock::to_time_t(record.timestamp);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    record.timestamp.time_since_epoch()) % 1000;

  std::tm tm{}; // Initialize the tm struct

  // Use the safer, thread-safe function
  localtime_s(&tm, &time_t);

  // Create directory structure: baseDir/YYYY-MM-DD/deviceType/deviceId/
  auto datePath = ParseDatePath(record.timestamp);
  auto dirPath = std::filesystem::path(baseDirectory) / datePath / record.deviceType / record.deviceId;

  // Create directories if they don't exist
  std::filesystem::create_directories(dirPath);

  // Create filename: HH-MM-SS-mmm_resultType_label.json
  std::ostringstream filename;
  filename << std::put_time(&tm, "%H-%M-%S");
  filename << "-" << std::setfill('0') << std::setw(3) << ms.count();
  filename << "_" << record.resultType;
  if (!record.label.empty()) {
    // Sanitize label for filename
    std::string sanitizedLabel = record.label;
    std::replace(sanitizedLabel.begin(), sanitizedLabel.end(), ' ', '_');
    std::replace(sanitizedLabel.begin(), sanitizedLabel.end(), '/', '-');
    std::replace(sanitizedLabel.begin(), sanitizedLabel.end(), '\\', '-');
    filename << "_" << sanitizedLabel;
  }
  filename << ".json";

  return dirPath / filename.str();
}

std::string FileResultStorage::SerializeRecord(const Record& record) const {
  json j;

  // Core fields
  j["deviceId"] = record.deviceId;
  j["deviceType"] = record.deviceType;
  j["timestamp"] = std::chrono::system_clock::to_time_t(record.timestamp);
  j["timestamp_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(
    record.timestamp.time_since_epoch()).count();
  j["resultType"] = record.resultType;
  j["label"] = record.label;

  // Numeric values
  if (!record.numericValues.empty()) {
    j["numericValues"] = record.numericValues;
  }

  // String values
  if (!record.stringValues.empty()) {
    j["stringValues"] = record.stringValues;
  }

  // Array values
  if (!record.arrayValues.empty()) {
    json arrays;
    for (const auto& [key, vec] : record.arrayValues) {
      arrays[key] = vec;
    }
    j["arrayValues"] = arrays;
  }

  // Metadata
  if (!record.metadata.empty()) {
    j["metadata"] = record.metadata;
  }

  return j.dump(2);  // Pretty print with 2 spaces
}

FileResultStorage::Record FileResultStorage::DeserializeRecord(const std::string& jsonStr) const {
  Record record;

  try {
    json j = json::parse(jsonStr);

    // Core fields
    record.deviceId = j["deviceId"];
    record.deviceType = j["deviceType"];
    record.resultType = j["resultType"];
    record.label = j.value("label", "");

    // Reconstruct timestamp
    if (j.contains("timestamp_ms")) {
      auto ms = std::chrono::milliseconds(j["timestamp_ms"].get<int64_t>());
      record.timestamp = std::chrono::system_clock::time_point(ms);
    }
    else {
      auto time_t = j["timestamp"].get<std::time_t>();
      record.timestamp = std::chrono::system_clock::from_time_t(time_t);
    }

    // Numeric values
    if (j.contains("numericValues")) {
      record.numericValues = j["numericValues"].get<std::map<std::string, double>>();
    }

    // String values
    if (j.contains("stringValues")) {
      record.stringValues = j["stringValues"].get<std::map<std::string, std::string>>();
    }

    // Array values
    if (j.contains("arrayValues")) {
      for (auto& [key, value] : j["arrayValues"].items()) {
        record.arrayValues[key] = value.get<std::vector<double>>();
      }
    }

    // Metadata
    if (j.contains("metadata")) {
      record.metadata = j["metadata"].get<std::map<std::string, std::string>>();
    }

  }
  catch (const json::exception& ) {
    // Return empty record on parse error
  }

  return record;
}

bool FileResultStorage::Store(const Record& record) {
  if (!initialized) return false;

  std::lock_guard<std::mutex> lock(storageMutex);

  try {
    auto filePath = GenerateFilePath(record);
    std::string jsonStr = SerializeRecord(record);

    std::ofstream file(filePath);
    if (!file.is_open()) return false;

    file << jsonStr;
    file.close();
    return true;

  }
  catch (const std::exception& ) {
    return false;
  }
}

bool FileResultStorage::StoreBatch(const std::vector<Record>& records) {
  if (!initialized) return false;

  bool allSuccess = true;
  for (const auto& record : records) {
    if (!Store(record)) {
      allSuccess = false;
    }
  }
  return allSuccess;
}

bool FileResultStorage::MatchesFilter(const Record& record, const QueryFilter& filter) const {
  // Check device ID
  if (filter.deviceId.has_value() && record.deviceId != filter.deviceId.value()) {
    return false;
  }

  // Check device type
  if (filter.deviceType.has_value() && record.deviceType != filter.deviceType.value()) {
    return false;
  }

  // Check result type
  if (filter.resultType.has_value() && record.resultType != filter.resultType.value()) {
    return false;
  }

  // Check label
  if (filter.label.has_value() && record.label != filter.label.value()) {
    return false;
  }

  // Check time range
  if (filter.startTime.has_value() && record.timestamp < filter.startTime.value()) {
    return false;
  }
  if (filter.endTime.has_value() && record.timestamp > filter.endTime.value()) {
    return false;
  }

  // Check metadata filters
  for (const auto& [key, value] : filter.metadataFilters) {
    auto it = record.metadata.find(key);
    if (it == record.metadata.end() || it->second != value) {
      return false;
    }
  }

  return true;
}

std::vector<IResultStorage::Record> FileResultStorage::Query(const QueryFilter& filter) const {
  if (!initialized) return {};

  std::lock_guard<std::mutex> lock(storageMutex);
  std::vector<Record> results;

  try {
    // Iterate through directory structure
    for (const auto& dateEntry : std::filesystem::recursive_directory_iterator(baseDirectory)) {
      if (!dateEntry.is_regular_file()) continue;
      if (dateEntry.path().extension() != ".json") continue;

      auto recordOpt = ReadRecordFromFile(dateEntry.path());
      if (!recordOpt.has_value()) continue;

      const auto& record = recordOpt.value();
      if (MatchesFilter(record, filter)) {
        results.push_back(record);

        // Check max results
        if (results.size() >= filter.maxResults) {
          break;
        }
      }
    }

    // Sort results
    if (filter.orderBy == "timestamp") {
      std::sort(results.begin(), results.end(),
        [ascending = filter.ascending](const Record& a, const Record& b) {
        return ascending ? (a.timestamp < b.timestamp) : (a.timestamp > b.timestamp);
      });
    }
    else if (filter.orderBy == "deviceId") {
      std::sort(results.begin(), results.end(),
        [ascending = filter.ascending](const Record& a, const Record& b) {
        return ascending ? (a.deviceId < b.deviceId) : (a.deviceId > b.deviceId);
      });
    }

  }
  catch (const std::exception& ) {
    // Return what we have so far
  }

  return results;
}

std::optional<IResultStorage::Record> FileResultStorage::ReadRecordFromFile(
  const std::filesystem::path& path) const {

  try {
    std::ifstream file(path);
    if (!file.is_open()) return std::nullopt;

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    auto record = DeserializeRecord(buffer.str());
    return record;

  }
  catch (const std::exception& ) {
    return std::nullopt;
  }
}

std::optional<IResultStorage::Record> FileResultStorage::GetLatest(const std::string& deviceId) const {
  QueryFilter filter;
  filter.deviceId = deviceId.empty() ? std::nullopt : std::optional(deviceId);
  filter.maxResults = 1;
  filter.orderBy = "timestamp";
  filter.ascending = false;  // Newest first

  auto results = Query(filter);
  if (!results.empty()) {
    return results.front();
  }
  return std::nullopt;
}

size_t FileResultStorage::GetRecordCount(const QueryFilter& filter) const {
  auto temp = filter;
  temp.maxResults = std::numeric_limits<size_t>::max();
  return Query(temp).size();
}

bool FileResultStorage::Delete(const QueryFilter& filter) {
  if (!initialized) return false;

  std::lock_guard<std::mutex> lock(storageMutex);

  auto records = Query(filter);
  bool allSuccess = true;

  for (const auto& record : records) {
    auto path = GenerateFilePath(record);
    try {
      if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
      }
    }
    catch (const std::exception& ) {
      allSuccess = false;
    }
  }

  return allSuccess;
}

bool FileResultStorage::Clear() {
  if (!initialized) return false;

  std::lock_guard<std::mutex> lock(storageMutex);

  try {
    for (const auto& entry : std::filesystem::directory_iterator(baseDirectory)) {
      std::filesystem::remove_all(entry.path());
    }
    return true;
  }
  catch (const std::exception& ) {
    return false;
  }
}

size_t FileResultStorage::GetStorageSize() const {
  if (!initialized) return 0;

  std::lock_guard<std::mutex> lock(storageMutex);
  size_t totalSize = 0;

  try {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(baseDirectory)) {
      if (entry.is_regular_file()) {
        totalSize += entry.file_size();
      }
    }
  }
  catch (const std::exception& ) {
    // Return what we counted so far
  }

  return totalSize;
}

bool FileResultStorage::ExportToFile(const std::string& filename, const QueryFilter& filter) const {
  if (!initialized) return false;

  auto records = Query(filter);

  try {
    json j;
    j["exportTime"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    j["recordCount"] = records.size();

    json recordsArray = json::array();
    for (const auto& record : records) {
      recordsArray.push_back(json::parse(SerializeRecord(record)));
    }
    j["records"] = recordsArray;

    std::ofstream file(filename);
    if (!file.is_open()) return false;

    file << j.dump(2);
    file.close();
    return true;

  }
  catch (const std::exception& ) {
    return false;
  }
}

bool FileResultStorage::ImportFromFile(const std::string& filename) {
  if (!initialized) return false;

  try {
    std::ifstream file(filename);
    if (!file.is_open()) return false;

    json j = json::parse(file);
    file.close();

    if (!j.contains("records")) return false;

    std::vector<Record> records;
    for (const auto& recordJson : j["records"]) {
      records.push_back(DeserializeRecord(recordJson.dump()));
    }

    return StoreBatch(records);

  }
  catch (const std::exception& ) {
    return false;
  }
}