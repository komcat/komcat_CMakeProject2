#include "AdaptivePowerScanner.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <chrono>

// Constructor
AdaptivePowerScanner::AdaptivePowerScanner(const ScanConfig& cfg)
  : config(cfg), totalMeasurements(0) {

  // Default measurement function (for testing)
  measurementFunc = [](double x, double y, double z) -> double {
    // Simple Gaussian simulation - replace with actual hardware call
    double baseValue = 0.001e-6;
    double peak = 400e-6;
    double xOpt = 0.0, yOpt = 0.0, zOpt = -0.001; // Optimal at Z = -1mm

    double distSq = pow(x - xOpt, 2) + pow(y - yOpt, 2) + pow(z - zOpt, 2);
    return baseValue + peak * exp(-distSq / (2 * pow(0.001, 2))); // 1mm sigma
  };

  // Default position validation
  positionValidationFunc = [this](double x, double y, double z) -> bool {
    return isWithinTravelLimits(ScanStep(x, y, z));
  };
}

// Hardware interface setup
void AdaptivePowerScanner::setMeasurementFunction(std::function<double(double x, double y, double z)> func) {
  measurementFunc = func;
}

void AdaptivePowerScanner::setPositionValidationFunction(std::function<bool(double x, double y, double z)> func) {
  positionValidationFunc = func;
}

// Configuration methods
void AdaptivePowerScanner::setPowerRange(double minPower, double maxPower) {
  config.powerMapping.minPower = minPower;
  config.powerMapping.maxPower = maxPower;
}

void AdaptivePowerScanner::setStepSizeRange(double minStepMicrons, double maxStepMicrons) {
  config.powerMapping.minStepSize = minStepMicrons / 1000.0; // Convert to mm
  config.powerMapping.maxStepSize = maxStepMicrons / 1000.0;
}

void AdaptivePowerScanner::setAxisDirection(const std::string& axis, const std::string& direction) {
  config.directionConstraints.forcedDirection[axis] = direction;
}

void AdaptivePowerScanner::setGaussianSigma(double sigmaMicrons) {
  config.powerMapping.gaussianSigma = sigmaMicrons / 1000.0; // Convert to mm
}

// Main scanning function
ScanStep AdaptivePowerScanner::adaptivePowerScan(const ScanStep& startPosition) {
  // Initialize scan
  currentBest = startPosition;
  scanHistory.clear();
  totalMeasurements = 0;

  std::cout << "=== Adaptive Power-Based Scanning ===" << std::endl;
  std::cout << "Initial: (" << std::fixed << std::setprecision(6)
    << startPosition.x << ", " << startPosition.y << ", " << startPosition.z
    << ") = " << std::scientific << std::setprecision(3)
    << startPosition.value << " A" << std::endl;

  // Record starting position
  ScanStep initialStep = startPosition;
  initialStep.measurementIndex = 0;
  initialStep.timestamp = getCurrentTimestamp();
  scanHistory.push_back(initialStep);

  // Sequential axis optimization with adaptive step sizes
  std::vector<std::string> axisOrder = { "Z", "X", "Y" }; // Z first for power optimization

  ScanStep current = currentBest;
  bool anyAxisImproved = true;
  int iteration = 0;

  while (anyAxisImproved && iteration < 3) {
    anyAxisImproved = false;
    iteration++;

    std::cout << "\n--- Iteration " << iteration << " ---" << std::endl;

    for (const std::string& axis : axisOrder) {
      ScanStep before = current;
      current = optimizeAxis(current, axis);

      if (current.value > before.value) {
        anyAxisImproved = true;
        double improvement = (current.value - before.value) / before.value;
        std::cout << "[Yes] " << axis << " improved by "
          << std::fixed << std::setprecision(2)
          << (improvement * 100) << "%" << std::endl;
      }
      else {
        std::cout << "- " << axis << " no improvement" << std::endl;
      }

      // Early termination if we've hit the maximum
      if (current.value >= config.powerMapping.maxPower * 0.95) {
        std::cout << "Near maximum power achieved, stopping scan" << std::endl;
        anyAxisImproved = false;
        break;
      }
    }
  }

  printScanSummary(startPosition, current);
  return current;
}

// Core adaptive axis optimization
ScanStep AdaptivePowerScanner::optimizeAxis(const ScanStep& start, const std::string& axis) {
  std::cout << "\nOptimizing " << axis << "-axis:" << std::endl;

  ScanStep current = start;
  int axisStepCount = 0;

  // Get allowed directions for this axis
  std::vector<std::string> allowedDirections = getAllowedDirections(axis);

  if (allowedDirections.empty()) {
    std::cout << "  No allowed directions for " << axis << "-axis" << std::endl;
    return current;
  }

  bool keepOptimizing = true;

  while (keepOptimizing && axisStepCount < config.maxMeasurementsPerAxis) {
    // Calculate adaptive step size based on current power
    double stepSize = calculateStepSize(current.value);

    std::cout << "  Current power: " << std::scientific << std::setprecision(3)
      << current.value << " A → Step size: " << std::fixed
      << std::setprecision(1) << (stepSize * 1000000) << " μm" << std::endl;

    // Test allowed directions
    ScanStep bestCandidate = current;
    std::string bestDirection = "";

    for (const std::string& direction : allowedDirections) {
      ScanStep candidate = testDirection(current, axis, direction, stepSize);
      axisStepCount++;

      if (candidate.value > bestCandidate.value) {
        bestCandidate = candidate;
        bestDirection = direction;
      }
    }

    // Check if we found improvement
    double improvement = (bestCandidate.value - current.value) / current.value;

    if (improvement > config.improvementThreshold) {
      current = bestCandidate;

      std::cout << "  → " << bestDirection << " step: "
        << std::fixed << std::setprecision(6) << getAxisValue(current, axis)
        << " mm, Power: " << std::scientific << std::setprecision(3)
        << current.value << " A (+";
      std::cout << std::fixed << std::setprecision(2) << (improvement * 100) << "%)" << std::endl;

      // Update global best
      if (current.value > currentBest.value) {
        currentBest = current;
        current.isPeak = true;
      }
    }
    else {
      std::cout << "  No significant improvement, stopping " << axis << "-axis optimization" << std::endl;
      keepOptimizing = false;
    }

    // Check convergence
    if (improvement < config.convergenceThreshold) {
      std::cout << "  Converged on " << axis << "-axis" << std::endl;
      keepOptimizing = false;
    }
  }

  std::cout << "  " << axis << "-axis measurements: " << axisStepCount << std::endl;
  return current;
}

// Power-based step size calculation
double AdaptivePowerScanner::calculateStepSize(double currentPower) const {
  if (!config.usePowerAdaptiveSteps) {
    return config.powerMapping.minStepSize; // Use minimum step size if adaptive disabled
  }

  // Clamp power to valid range
  double clampedPower = std::max(config.powerMapping.minPower,
    std::min(currentPower, config.powerMapping.maxPower));

  // Calculate normalized power (0 = min power, 1 = max power)
  double powerRange = config.powerMapping.maxPower - config.powerMapping.minPower;
  double normalizedPower = (clampedPower - config.powerMapping.minPower) / powerRange;

  // Use parabolic/Gaussian-like mapping: higher power = smaller steps
  // For Gaussian relationship: step size inversely related to power level
  double stepSizeRange = config.powerMapping.maxStepSize - config.powerMapping.minStepSize;

  // Inverse relationship: low power (far from optimum) = large steps
  // High power (near optimum) = small steps
  double stepSizeFactor = 1.0 - normalizedPower; // Invert: 1 at min power, 0 at max power

  // Apply Gaussian-like curve for smoother transition
  stepSizeFactor = exp(-pow(normalizedPower, 2) / (2 * pow(0.5, 2))); // Gaussian centered at 0

  double stepSize = config.powerMapping.minStepSize + stepSizeFactor * stepSizeRange;

  return stepSize;
}

// Physics-based direction selection
std::vector<std::string> AdaptivePowerScanner::getAllowedDirections(const std::string& axis) const {
  std::vector<std::string> directions;

  if (!config.usePhysicsConstraints) {
    directions = { "Positive", "Negative" };
    return directions;
  }

  auto it = config.directionConstraints.forcedDirection.find(axis);
  if (it != config.directionConstraints.forcedDirection.end()) {
    std::string constraint = it->second;

    if (constraint == "Positive") {
      directions.push_back("Positive");
    }
    else if (constraint == "Negative") {
      directions.push_back("Negative");
    }
    else if (constraint == "Both") {
      directions.push_back("Positive");
      directions.push_back("Negative");
    }
  }
  else {
    // Default to both if not specified
    directions = { "Positive", "Negative" };
  }

  return directions;
}

// Test a specific direction
ScanStep AdaptivePowerScanner::testDirection(const ScanStep& current, const std::string& axis,
  const std::string& direction, double stepSize) {
  double delta = (direction == "Positive") ? stepSize : -stepSize;
  return moveAxis(current, axis, delta);
}

// Movement and measurement
ScanStep AdaptivePowerScanner::moveAxis(const ScanStep& current, const std::string& axis, double delta) {
  ScanStep next = current;

  // Apply movement
  if (axis == "X") {
    next.x += delta;
  }
  else if (axis == "Y") {
    next.y += delta;
  }
  else if (axis == "Z") {
    next.z += delta;
  }

  // Validate position
  if (!isPositionValid(next)) {
    // Return current position if invalid
    return current;
  }

  // Fill in step information
  next.axis = axis;
  next.direction = (delta > 0) ? "Positive" : "Negative";
  next.stepSize = std::abs(delta);
  next.measurementIndex = ++totalMeasurements;
  next.timestamp = getCurrentTimestamp();

  // Perform measurement
  next.value = performMeasurement(next);
  next.relativeImprovement = (next.value - current.value) / current.value;

  // Record in history
  scanHistory.push_back(next);

  return next;
}

double AdaptivePowerScanner::performMeasurement(const ScanStep& position) {
  return measurementFunc(position.x, position.y, position.z);
}

bool AdaptivePowerScanner::isPositionValid(const ScanStep& position) const {
  return positionValidationFunc(position.x, position.y, position.z);
}

bool AdaptivePowerScanner::isWithinTravelLimits(const ScanStep& position) const {
  for (const auto& [axis, maxTravel] : config.directionConstraints.maxTravel) {
    double axisValue = getAxisValue(position, axis);
    if (std::abs(axisValue) > maxTravel) {
      return false;
    }
  }
  return true;
}

// Helper functions
double AdaptivePowerScanner::getAxisValue(const ScanStep& step, const std::string& axis) const {
  if (axis == "X") return step.x;
  if (axis == "Y") return step.y;
  if (axis == "Z") return step.z;
  return 0.0;
}

void AdaptivePowerScanner::setAxisValue(ScanStep& step, const std::string& axis, double value) {
  if (axis == "X") step.x = value;
  else if (axis == "Y") step.y = value;
  else if (axis == "Z") step.z = value;
}

// Results and statistics
std::vector<ScanStep> AdaptivePowerScanner::getScanHistory() const {
  return scanHistory;
}

ScanStep AdaptivePowerScanner::getBestPosition() const {
  return currentBest;
}

int AdaptivePowerScanner::getTotalMeasurements() const {
  return totalMeasurements;
}

double AdaptivePowerScanner::getTotalImprovement() const {
  if (scanHistory.empty()) return 0.0;

  double initial = scanHistory[0].value;
  double final = currentBest.value;
  return (final - initial) / initial;
}

std::map<std::string, int> AdaptivePowerScanner::getMeasurementCountsByAxis() const {
  std::map<std::string, int> counts;
  for (const auto& step : scanHistory) {
    if (!step.axis.empty()) {
      counts[step.axis]++;
    }
  }
  return counts;
}

std::map<std::string, double> AdaptivePowerScanner::getAverageStepSizeByAxis() const {
  std::map<std::string, std::vector<double>> stepSizes;
  std::map<std::string, double> averages;

  for (const auto& step : scanHistory) {
    if (!step.axis.empty() && step.stepSize > 0) {
      stepSizes[step.axis].push_back(step.stepSize);
    }
  }

  for (const auto& [axis, sizes] : stepSizes) {
    if (!sizes.empty()) {
      double sum = 0;
      for (double size : sizes) sum += size;
      averages[axis] = sum / sizes.size();
    }
  }

  return averages;
}

void AdaptivePowerScanner::reset() {
  scanHistory.clear();
  totalMeasurements = 0;
  currentBest = ScanStep();
}

// Utility functions
std::string AdaptivePowerScanner::getCurrentTimestamp() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

// Reporting
void AdaptivePowerScanner::printScanSummary(const ScanStep& start, const ScanStep & final) const {
  std::cout << "\n=== Adaptive Scan Summary ===" << std::endl;
  std::cout << "Total measurements: " << totalMeasurements << std::endl;

  // Power improvement
  double powerImprovement = (final.value - start.value) / start.value * 100.0;
  std::cout << "Power improvement: " << std::fixed << std::setprecision(2)
    << powerImprovement << "%" << std::endl;

  std::cout << "Final power: " << std::scientific << std::setprecision(3)
    << final.value << " A" << std::endl;

  std::cout << "Final position: (" << std::fixed << std::setprecision(6)
    << final.x << ", " << final.y << ", " << final.z << ") mm" << std::endl;

  // Measurements per axis
  auto axisCounts = getMeasurementCountsByAxis();
  std::cout << "Measurements per axis:" << std::endl;
  for (const auto& [axis, count] : axisCounts) {
    double percentage = (100.0 * count) / totalMeasurements;
    std::cout << "  " << axis << ": " << count << " ("
      << std::setprecision(1) << percentage << "%)" << std::endl;
  }

  // Average step sizes
  auto avgStepSizes = getAverageStepSizeByAxis();
  std::cout << "Average step sizes:" << std::endl;
  for (const auto& [axis, avgSize] : avgStepSizes) {
    std::cout << "  " << axis << ": " << std::setprecision(1)
      << (avgSize * 1000000) << " μm" << std::endl;
  }

  std::cout << "==============================" << std::endl;
}