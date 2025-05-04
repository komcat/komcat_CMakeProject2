// include/scanning/scan_data_collector.h
#pragma once

#include "include/motions/MotionTypes.h"
#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

// Forward declarations
struct ScanMeasurement;
struct ScanBaseline;
struct ScanPeak;
struct ScanStatistics;
struct ScanResults;

class ScanDataCollector {
public:
  ScanDataCollector(const std::string& deviceName);
  ~ScanDataCollector();

  // Record data points
  void RecordBaseline(double value, const PositionStruct& position);
  void RecordMeasurement(double value, const PositionStruct& position,
    const std::string& axis, double stepSize, int direction);

  // Get current data
  PositionStruct GetBaselinePosition() const;
  double GetBaselineValue() const;
  PositionStruct GetPeakPosition() const;
  double GetPeakValue() const;

  // Get final results
  ScanResults GetResults() const;

  // Save results to file
  bool SaveResults() const;

private:
  std::string m_deviceName;
  std::string m_scanId;
  std::vector<ScanMeasurement> m_measurements;
  std::unique_ptr<ScanBaseline> m_baseline;
  std::unique_ptr<ScanPeak> m_currentPeak;

  // Helper methods
  ScanStatistics CalculateStatistics() const;
  double CalculateStandardDeviation(const std::vector<double>& values) const;
};

// Data structures for scan measurements

struct ScanMeasurement {
  double value = 0.0;
  PositionStruct position;
  std::chrono::system_clock::time_point timestamp;
  std::string axis;
  double stepSize = 0.0;
  std::string direction;
  double gradient = 0.0;
  double relativeImprovement = 0.0;
  bool isPeak = false;
  bool isValid = true;

  // JSON serialization
  nlohmann::json ToJson() const;
};

struct ScanBaseline {
  double value = 0.0;
  PositionStruct position;
  std::chrono::system_clock::time_point timestamp;

  // JSON serialization
  nlohmann::json ToJson() const;
};

struct ScanPeak {
  double value = 0.0;
  PositionStruct position;
  std::chrono::system_clock::time_point timestamp;
  std::string context;

  // JSON serialization
  nlohmann::json ToJson() const;
};

struct ScanStatistics {
  double minValue = 0.0;
  double maxValue = 0.0;
  double averageValue = 0.0;
  double standardDeviation = 0.0;
  std::chrono::seconds totalDuration = std::chrono::seconds(0);
  int totalMeasurements = 0;
  std::map<std::string, int> measurementsPerAxis;

  // JSON serialization
  nlohmann::json ToJson() const;
};

struct ScanResults {
  std::string deviceId;
  std::string scanId;
  std::chrono::system_clock::time_point startTime;
  std::chrono::system_clock::time_point endTime;
  std::unique_ptr<ScanBaseline> baseline;
  std::unique_ptr<ScanPeak> peak;
  int totalMeasurements = 0;
  std::vector<ScanMeasurement> measurements;
  std::unique_ptr<ScanStatistics> statistics;

  // JSON serialization
  nlohmann::json ToJson() const;
};