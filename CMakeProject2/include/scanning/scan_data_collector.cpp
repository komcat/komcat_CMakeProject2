// src/scanning/scan_data_collector.cpp
#include "include/scanning/scan_data_collector.h"
#include <cmath>
#include <numeric>
#include <iomanip>
#include <chrono>
#include <sstream>

ScanDataCollector::ScanDataCollector(const std::string& deviceName)
  : m_deviceName(deviceName)
{
  // Generate a unique scan ID based on timestamp
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << "scan_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
  m_scanId = ss.str();
}

ScanDataCollector::~ScanDataCollector() {
  // Save results on destruction if not already saved
  SaveResults();
}

void ScanDataCollector::RecordBaseline(double value, const PositionStruct& position) {
  // Create baseline data
  m_baseline = std::make_unique<ScanBaseline>();
  m_baseline->value = value;
  m_baseline->position = position;
  m_baseline->timestamp = std::chrono::system_clock::now();

  // Initialize peak with baseline
  m_currentPeak = std::make_unique<ScanPeak>();
  m_currentPeak->value = value;
  m_currentPeak->position = position;
  m_currentPeak->timestamp = m_baseline->timestamp;
  m_currentPeak->context = "Initial Position";
}

void ScanDataCollector::RecordMeasurement(double value, const PositionStruct& position,
  const std::string& axis, double stepSize, int direction) {
  ScanMeasurement measurement;
  measurement.value = value;
  measurement.position = position;
  measurement.timestamp = std::chrono::system_clock::now();
  measurement.axis = axis;
  measurement.stepSize = stepSize;
  measurement.direction = (direction > 0) ? "Positive" : "Negative";

  // Calculate gradient and improvement if we have previous measurements
  if (!m_measurements.empty()) {
    const auto& prev = m_measurements.back();

    // Calculate the distance moved
    double distance = 0.0;
    if (axis == "X") {
      distance = position.x - prev.position.x;
    }
    else if (axis == "Y") {
      distance = position.y - prev.position.y;
    }
    else if (axis == "Z") {
      distance = position.z - prev.position.z;
    }
    else if (axis == "U") {
      distance = position.u - prev.position.u;
    }
    else if (axis == "V") {
      distance = position.v - prev.position.v;
    }
    else if (axis == "W") {
      distance = position.w - prev.position.w;
    }

    if (std::abs(distance) > 1e-10) {
      measurement.gradient = (value - prev.value) / distance;
    }

    if (prev.value > 0) {
      measurement.relativeImprovement = (value - prev.value) / prev.value;
    }
  }

  // Check if this is a new peak
  if (!m_currentPeak || value > m_currentPeak->value) {
    m_currentPeak = std::make_unique<ScanPeak>();
    m_currentPeak->value = value;
    m_currentPeak->position = position;
    m_currentPeak->timestamp = measurement.timestamp;
    m_currentPeak->context = axis + " axis scan, " +
      (direction > 0 ? "positive" : "negative") +
      " direction, step size " + std::to_string(stepSize * 1000) + " microns";

    measurement.isPeak = true;
  }

  // Add to measurements collection
  m_measurements.push_back(measurement);
}

PositionStruct ScanDataCollector::GetBaselinePosition() const {
  return m_baseline ? m_baseline->position : PositionStruct{};
}

double ScanDataCollector::GetBaselineValue() const {
  return m_baseline ? m_baseline->value : 0.0;
}

PositionStruct ScanDataCollector::GetPeakPosition() const {
  return m_currentPeak ? m_currentPeak->position : PositionStruct{};
}

double ScanDataCollector::GetPeakValue() const {
  return m_currentPeak ? m_currentPeak->value : std::numeric_limits<double>::lowest();
}

ScanResults ScanDataCollector::GetResults() const {
  ScanResults results;
  results.deviceId = m_deviceName;
  results.scanId = m_scanId;

  if (!m_measurements.empty()) {
    results.startTime = m_measurements.front().timestamp;
    results.endTime = m_measurements.back().timestamp;
  }
  else {
    results.startTime = std::chrono::system_clock::now();
    results.endTime = results.startTime;
  }

  if (m_baseline) {
    results.baseline = std::make_unique<ScanBaseline>(*m_baseline);
  }

  if (m_currentPeak) {
    results.peak = std::make_unique<ScanPeak>(*m_currentPeak);
  }

  results.totalMeasurements = m_measurements.size();
  results.measurements = m_measurements;
  results.statistics = std::make_unique<ScanStatistics>(CalculateStatistics());

  return results;
}

bool ScanDataCollector::SaveResults() const {
  try {
    // Create results object
    auto results = GetResults();

    // Create logs directory if it doesn't exist
    std::filesystem::path logsPath = std::filesystem::current_path() / "logs" / "scanning";
    std::filesystem::create_directories(logsPath);

    // Create filename
    std::string filename = "ScanResults_" + m_deviceName + "_" + m_scanId + ".json";
    std::filesystem::path fullPath = logsPath / filename;

    // Convert results to JSON
    nlohmann::json resultsJson = results.ToJson();

    // Write to file
    std::ofstream file(fullPath);
    if (!file.is_open()) {
      return false;
    }

    file << std::setw(2) << resultsJson << std::endl;
    return true;
  }
  catch (const std::exception&) {
    return false;
  }
}

ScanStatistics ScanDataCollector::CalculateStatistics() const {
  if (m_measurements.empty()) {
    return ScanStatistics{};
  }

  ScanStatistics stats;

  // Extract values
  std::vector<double> values;
  values.reserve(m_measurements.size());

  for (const auto& measurement : m_measurements) {
    values.push_back(measurement.value);
  }

  // Calculate statistics
  auto [minIt, maxIt] = std::minmax_element(values.begin(), values.end());
  stats.minValue = *minIt;
  stats.maxValue = *maxIt;

  double sum = std::accumulate(values.begin(), values.end(), 0.0);
  stats.averageValue = sum / values.size();

  double squareSum = std::accumulate(values.begin(), values.end(), 0.0,
    [&stats](double acc, double val) {
    return acc + (val - stats.averageValue) * (val - stats.averageValue);
  });

  if (values.size() > 1) {
    stats.standardDeviation = std::sqrt(squareSum / (values.size() - 1));
  }

  // Duration
  if (m_measurements.size() >= 2) {
    auto duration = m_measurements.back().timestamp - m_measurements.front().timestamp;
    stats.totalDuration = std::chrono::duration_cast<std::chrono::seconds>(duration);
  }

  stats.totalMeasurements = m_measurements.size();

  // Count per axis
  stats.measurementsPerAxis.clear();
  for (const auto& measurement : m_measurements) {
    stats.measurementsPerAxis[measurement.axis]++;
  }

  return stats;
}

double ScanDataCollector::CalculateStandardDeviation(const std::vector<double>& values) const {
  if (values.size() <= 1) {
    return 0.0;
  }

  double sum = std::accumulate(values.begin(), values.end(), 0.0);
  double mean = sum / values.size();

  double squareSum = 0.0;
  for (double value : values) {
    squareSum += (value - mean) * (value - mean);
  }

  return std::sqrt(squareSum / (values.size() - 1));
}

// JSON serialization methods for data structures
nlohmann::json ScanMeasurement::ToJson() const {
  nlohmann::json j;
  j["value"] = value;

  j["position"] = {
      {"x", position.x},
      {"y", position.y},
      {"z", position.z},
      {"u", position.u},
      {"v", position.v},
      {"w", position.w}
  };

  // Convert timestamp to string
  auto time_t = std::chrono::system_clock::to_time_t(timestamp);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  j["timestamp"] = ss.str();

  j["axis"] = axis;
  j["stepSize"] = stepSize;
  j["direction"] = direction;
  j["gradient"] = gradient;
  j["relativeImprovement"] = relativeImprovement;
  j["isPeak"] = isPeak;
  j["isValid"] = isValid;

  return j;
}

nlohmann::json ScanBaseline::ToJson() const {
  nlohmann::json j;
  j["value"] = value;

  j["position"] = {
      {"x", position.x},
      {"y", position.y},
      {"z", position.z},
      {"u", position.u},
      {"v", position.v},
      {"w", position.w}
  };

  // Convert timestamp to string
  auto time_t = std::chrono::system_clock::to_time_t(timestamp);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  j["timestamp"] = ss.str();

  return j;
}

nlohmann::json ScanPeak::ToJson() const {
  nlohmann::json j;
  j["value"] = value;

  j["position"] = {
      {"x", position.x},
      {"y", position.y},
      {"z", position.z},
      {"u", position.u},
      {"v", position.v},
      {"w", position.w}
  };

  // Convert timestamp to string
  auto time_t = std::chrono::system_clock::to_time_t(timestamp);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  j["timestamp"] = ss.str();

  j["context"] = context;

  return j;
}

nlohmann::json ScanStatistics::ToJson() const {
  nlohmann::json j;
  j["minValue"] = minValue;
  j["maxValue"] = maxValue;
  j["averageValue"] = averageValue;
  j["standardDeviation"] = standardDeviation;
  j["totalDurationSeconds"] = totalDuration.count();
  j["totalMeasurements"] = totalMeasurements;

  j["measurementsPerAxis"] = nlohmann::json::object();
  for (const auto& [axis, count] : measurementsPerAxis) {
    j["measurementsPerAxis"][axis] = count;
  }

  return j;
}

nlohmann::json ScanResults::ToJson() const {
  nlohmann::json j;
  j["deviceId"] = deviceId;
  j["scanId"] = scanId;

  // Convert timestamps to string
  auto startTime_t = std::chrono::system_clock::to_time_t(startTime);
  auto endTime_t = std::chrono::system_clock::to_time_t(endTime);

  std::stringstream startSs, endSs;
  startSs << std::put_time(std::localtime(&startTime_t), "%Y-%m-%d %H:%M:%S");
  endSs << std::put_time(std::localtime(&endTime_t), "%Y-%m-%d %H:%M:%S");

  j["startTime"] = startSs.str();
  j["endTime"] = endSs.str();

  if (baseline) {
    j["baseline"] = baseline->ToJson();
  }

  if (peak) {
    j["peak"] = peak->ToJson();
  }

  j["totalMeasurements"] = totalMeasurements;

  j["measurements"] = nlohmann::json::array();
  for (const auto& measurement : measurements) {
    j["measurements"].push_back(measurement.ToJson());
  }

  if (statistics) {
    j["statistics"] = statistics->ToJson();
  }

  return j;
}