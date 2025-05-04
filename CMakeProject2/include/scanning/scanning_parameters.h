// include/scanning/scanning_parameters.h
#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <stdexcept>
#include <algorithm>

class ScanningParameters {
public:
  // Motion control parameters
  int motionSettleTimeMs = 400;
  int consecutiveDecreasesLimit = 3;
  double improvementThreshold = 0.01; // 1%

  // Scan range parameters
  std::vector<std::string> axesToScan = { "Z", "X", "Y" };
  std::vector<double> stepSizes = { 0.001, 0.005, 0.010 }; // 1, 5, 10 microns

  // Safety parameters
  double maxStepSize = 0.5; // mm
  double maxTotalDistance = 5.0; // mm
  double minValue = std::numeric_limits<double>::lowest();
  double maxValue = (std::numeric_limits<double>::max)();

  // Timing parameters
  std::chrono::seconds scanTimeout = std::chrono::minutes(30);
  std::chrono::seconds measurementTimeout = std::chrono::seconds(5);

  // Static method to create default parameters
  static ScanningParameters CreateDefault();

  // Validate parameters
  void Validate() const;
};

// Implementation
inline ScanningParameters ScanningParameters::CreateDefault() {
  return ScanningParameters();
}

inline void ScanningParameters::Validate() const {
  if (stepSizes.empty()) {
    throw std::invalid_argument("At least one step size must be specified");
  }

  if (axesToScan.empty()) {
    throw std::invalid_argument("At least one axis must be specified");
  }

  for (double stepSize : stepSizes) {
    if (stepSize <= 0 || stepSize > maxStepSize) {
      throw std::invalid_argument("Step size " + std::to_string(stepSize) +
        " is invalid. Must be between 0 and " + std::to_string(maxStepSize));
    }
  }

  if (motionSettleTimeMs < 0) {
    throw std::invalid_argument("Motion settle time cannot be negative");
  }

  if (consecutiveDecreasesLimit < 1) {
    throw std::invalid_argument("Consecutive decreases limit must be at least 1");
  }

  if (improvementThreshold < 0 || improvementThreshold > 1) {
    throw std::invalid_argument("Improvement threshold must be between 0 and 1");
  }
}