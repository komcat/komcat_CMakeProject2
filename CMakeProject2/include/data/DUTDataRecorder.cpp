#include "DUTDataRecorder.h"
#include "include/data/global_data_store.h"
#include "include/logger.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>

DUTDataRecorder::DUTDataRecorder()
  : m_isRecording(false) {
  EnsureDirectoryExists("dut_saved");
}

DUTDataRecorder::~DUTDataRecorder() {
  if (m_isRecording) {
    End(); // Auto-save if still recording
  }
}

void DUTDataRecorder::Start(const std::string& serialNumber) {
  if (m_isRecording) {
    End(); // End previous session
  }

  m_currentSerialNumber = serialNumber;
  m_dataPoints.clear();
  m_isRecording = true;
  m_sessionStartTime = std::chrono::system_clock::now();

  auto* logger = Logger::GetInstance();
  logger->LogInfo("DUTDataRecorder: Started recording for DUT: " + serialNumber);
}

void DUTDataRecorder::AddDataPoint(const std::string& key, double value) {
  if (!m_isRecording) {
    auto* logger = Logger::GetInstance();
    logger->LogWarning("DUTDataRecorder: Cannot add data point - not recording");
    return;
  }

  DataPoint point;
  point.key = key;
  point.value = value;
  point.timestamp = std::chrono::system_clock::now();

  m_dataPoints.push_back(point);
}

void DUTDataRecorder::End() {
  if (!m_isRecording) {
    return;
  }

  m_isRecording = false;

  auto* logger = Logger::GetInstance();
  logger->LogInfo("DUTDataRecorder: Ended recording for DUT: " + m_currentSerialNumber +
    " (" + std::to_string(m_dataPoints.size()) + " data points)");
}

bool DUTDataRecorder::ExportToCSV(const std::string& filename) {
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

  // Write header
  file << "Serial_Number,Key,Value,Timestamp\n";

  // Write data points
  for (const auto& point : m_dataPoints) {
    auto time_t = std::chrono::system_clock::to_time_t(point.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      point.timestamp.time_since_epoch()) % 1000;

    file << m_currentSerialNumber << ","
      << point.key << ","
      << std::fixed << std::setprecision(6) << point.value << ","
      << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
      << "." << std::setfill('0') << std::setw(3) << ms.count()
      << "\n";
  }

  file.close();

  auto* logger = Logger::GetInstance();
  logger->LogInfo("DUTDataRecorder: Exported CSV to: " + filepath);
  return true;
}

bool DUTDataRecorder::ExportToJSON(const std::string& filename) {
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
  file << "  \"session_start\": \"" << std::put_time(std::localtime(&start_time_t), "%Y-%m-%d %H:%M:%S") << "\",\n";
  file << "  \"data_point_count\": " << m_dataPoints.size() << ",\n";
  file << "  \"data_points\": [\n";

  for (size_t i = 0; i < m_dataPoints.size(); ++i) {
    const auto& point = m_dataPoints[i];
    auto time_t = std::chrono::system_clock::to_time_t(point.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      point.timestamp.time_since_epoch()) % 1000;

    file << "    {\n";
    file << "      \"key\": \"" << point.key << "\",\n";
    file << "      \"value\": " << std::fixed << std::setprecision(6) << point.value << ",\n";
    file << "      \"timestamp\": \"" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
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
  ss << m_currentSerialNumber << "_"
    << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
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