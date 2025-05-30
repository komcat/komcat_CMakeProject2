#include "SequentialOptimizedScanner.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <random>

// ScanConfig default constructor
SequentialOptimizedScanner::ScanConfig::ScanConfig() {
  // Z-axis gets priority with larger steps (based on your data showing Z dominance)
  zConfig.stepSizeCoarse = 0.005;      // 0.5μm - larger for fast peak finding
  zConfig.stepSizeFine = 0.001;        // 0.1μm - your current fine level
  zConfig.stepSizeUltraFine = 0.0002;  // 0.05μm - final convergence
  zConfig.maxStepsPerPhase = 8;
  zConfig.minImprovementThreshold = 0.005;  // 0.5% for Z-axis

  // XY gets smaller steps since they're for centering (based on your insight)
  xyConfig.stepSizeCoarse = 0.001;      // 0.1μm - smaller than current
  xyConfig.stepSizeFine = 0.0005;       // 0.05μm - fine positioning
  xyConfig.stepSizeUltraFine = 0.0002; // 0.025μm - ultra-fine
  xyConfig.maxStepsPerPhase = 4;         // Fewer steps for XY
  xyConfig.minImprovementThreshold = 0.001;  // 0.1% for XY

  useSmartDirectionSelection = true;
  useAdaptiveStepSize = true;
  maxConsecutiveDeclines = 3;
  convergenceThreshold = 0.0001; // 0.01%
}

// DirectionMemory methods
void SequentialOptimizedScanner::DirectionMemory::clear() {
  lastGoodDirection.clear();
  lastGoodImprovement.clear();
  consecutiveDeclines.clear();
}

// Constructor
SequentialOptimizedScanner::SequentialOptimizedScanner(const ScanConfig& cfg)
  : config(cfg), totalMeasurements(0) {

  // Default measurement function (for testing - replace with hardware interface)
  measurementFunc = [](double x, double y, double z) -> double {
    // Simple simulation - replace with actual hardware call
    double baseValue = 0.001;
    double xContrib = 0.0001 * exp(-pow((x + 5.833) * 1000, 2) / 1000);
    double yContrib = 0.0001 * exp(-pow((y + 4.563) * 1000, 2) / 1000);
    double zContrib = 0.0003 * exp(-pow((z + 0.8715) * 1000, 2) / 500);

    return baseValue + xContrib + yContrib + zContrib;
  };

  // Default position validation (accept all positions)
  positionValidationFunc = [](double x, double y, double z) -> bool {
    return true; // Replace with actual bounds checking
  };
}

// Hardware interface setup
void SequentialOptimizedScanner::setMeasurementFunction(MeasurementFunction func) {
  measurementFunc = func;
}

void SequentialOptimizedScanner::setPositionValidationFunction(PositionValidationFunction func) {
  positionValidationFunc = func;
}

// Main scanning function
ScanStep SequentialOptimizedScanner::optimizedSequentialScan(const ScanStep& startPosition) {
  // Initialize scan
  currentBest = startPosition;
  scanHistory.clear();
  totalMeasurements = 0;
  directionMemory.clear();

  std::cout << "=== Sequential XYZ Scan with Smart Direction Selection ===" << std::endl;
  std::cout << "Initial: (" << startPosition.x << ", " << startPosition.y << ", " << startPosition.z
    << ") = " << startPosition.value << std::endl;

  // Record starting position
  ScanStep initialStep = startPosition;
  initialStep.measurementIndex = 0;
  initialStep.timestamp = getCurrentTimestamp();
  scanHistory.push_back(initialStep);

  // Phase 1: Coarse optimization with smart direction selection
  ScanStep afterCoarse = coarseOptimizationPhase(currentBest);
  printPhaseResults("Coarse Optimization", startPosition, afterCoarse);

  // Phase 2: Fine optimization 
  ScanStep afterFine = fineOptimizationPhase(afterCoarse);
  printPhaseResults("Fine Optimization", afterCoarse, afterFine);

  // Phase 3: Ultra-fine convergence (optional)
  ScanStep final = afterFine;
  double improvement = (afterFine.value - afterCoarse.value) / afterCoarse.value;
  if (improvement > config.convergenceThreshold) {
    final = ultraFinePhase(afterFine);
    printPhaseResults("Ultra-Fine Convergence", afterFine, final);
  }
  else {
    std::cout << "Skipping ultra-fine phase - insufficient improvement" << std::endl;
  }

  printOptimizationSummary(startPosition, final);
  return final;
}

// Phase 1: Coarse optimization
ScanStep SequentialOptimizedScanner::coarseOptimizationPhase(const ScanStep& start) {
  std::cout << "\n--- Phase 1: Coarse Optimization ---" << std::endl;

  ScanStep current = start;
  bool anyAxisImproved = true;
  int phaseIteration = 0;

  while (anyAxisImproved && phaseIteration < 3) {
    anyAxisImproved = false;
    phaseIteration++;

    std::cout << "Coarse iteration " << phaseIteration << std::endl;

    // Sequential XYZ scan with smart direction selection
    ScanStep afterZ = smartAxisOptimization(current, "Z", config.zConfig.stepSizeCoarse,
      config.zConfig.maxStepsPerPhase);
    if (afterZ.value > current.value) anyAxisImproved = true;

    ScanStep afterX = smartAxisOptimization(afterZ, "X", config.xyConfig.stepSizeCoarse,
      config.xyConfig.maxStepsPerPhase);
    if (afterX.value > afterZ.value) anyAxisImproved = true;

    ScanStep afterY = smartAxisOptimization(afterX, "Y", config.xyConfig.stepSizeCoarse,
      config.xyConfig.maxStepsPerPhase);
    if (afterY.value > afterX.value) anyAxisImproved = true;

    current = afterY;

    if (!anyAxisImproved) {
      std::cout << "No improvement in any axis, ending coarse phase" << std::endl;
    }
  }

  return current;
}

// Phase 2: Fine optimization
ScanStep SequentialOptimizedScanner::fineOptimizationPhase(const ScanStep& start) {
  std::cout << "\n--- Phase 2: Fine Optimization ---" << std::endl;

  ScanStep current = start;

  // Sequential XYZ with fine steps
  current = smartAxisOptimization(current, "Z", config.zConfig.stepSizeFine,
    config.zConfig.maxStepsPerPhase);
  current = smartAxisOptimization(current, "X", config.xyConfig.stepSizeFine,
    config.xyConfig.maxStepsPerPhase);
  current = smartAxisOptimization(current, "Y", config.xyConfig.stepSizeFine,
    config.xyConfig.maxStepsPerPhase);

  return current;
}

// Phase 3: Ultra-fine convergence
ScanStep SequentialOptimizedScanner::ultraFinePhase(const ScanStep& start) {
  std::cout << "\n--- Phase 3: Ultra-Fine Convergence ---" << std::endl;

  ScanStep current = start;

  // Very small steps for final convergence
  current = smartAxisOptimization(current, "Z", config.zConfig.stepSizeUltraFine, 3);
  current = smartAxisOptimization(current, "X", config.xyConfig.stepSizeUltraFine, 2);
  current = smartAxisOptimization(current, "Y", config.xyConfig.stepSizeUltraFine, 2);

  return current;
}

// Core smart axis optimization
ScanStep SequentialOptimizedScanner::smartAxisOptimization(const ScanStep& start,
  const std::string& axis,
  double stepSize, int maxSteps) {
  std::cout << "\n  Optimizing " << axis << "-axis (step: " << stepSize
    << ", max steps: " << maxSteps << ")" << std::endl;

  ScanStep current = start;

  // Step 1: Smart direction selection
  std::string chosenDirection = selectBestDirection(current, axis, stepSize);
  if (chosenDirection.empty()) {
    std::cout << "    No improvement in either direction, skipping axis" << std::endl;
    return current;
  }

  std::cout << "    Chosen direction: " << chosenDirection << std::endl;

  // Step 2: Continue in chosen direction with adaptive step size
  double currentStepSize = stepSize;
  int consecutiveDeclines = 0;

  for (int step = 0; step < maxSteps; step++) {
    double deltaStep = (chosenDirection == "Positive") ? currentStepSize : -currentStepSize;
    ScanStep next = moveAxis(current, axis, deltaStep);

    double improvement = (next.value - current.value) / current.value;

    std::cout << "    Step " << step << ": " << axis << " = " << getAxisValue(next, axis)
      << ", value = " << std::fixed << std::setprecision(10) << next.value
      << " (improvement: " << std::setprecision(3) << (improvement * 100) << "%)" << std::endl;

    if (improvement > getAxisThreshold(axis)) {
      // Good improvement - continue
      current = next;
      consecutiveDeclines = 0;

      // Update direction memory
      updateDirectionMemory(axis, chosenDirection, improvement);

      // Adaptive step size increase for very good improvements
      if (config.useAdaptiveStepSize && improvement > 0.02) { // 2% improvement
        currentStepSize = std::min(currentStepSize * 1.2, stepSize * 1.5);
        std::cout << "    Increased step size to " << currentStepSize << std::endl;
      }

    }
    else {
      // Poor improvement
      consecutiveDeclines++;
      directionMemory.consecutiveDeclines[axis]++;

      if (consecutiveDeclines >= config.maxConsecutiveDeclines) {
        std::cout << "    Stopping due to consecutive declines" << std::endl;
        break;
      }

      // Adaptive step size reduction
      if (config.useAdaptiveStepSize) {
        currentStepSize *= 0.7;
        std::cout << "    Reduced step size to " << currentStepSize << std::endl;

        // Try one more step with smaller size
        if (currentStepSize > stepSize * 0.1) {
          continue;
        }
        else {
          std::cout << "    Step size too small, stopping" << std::endl;
          break;
        }
      }
    }
  }

  return current;
}

// Smart direction selection
std::string SequentialOptimizedScanner::selectBestDirection(const ScanStep& current,
  const std::string& axis,
  double stepSize) {
  // Check if we have memory of a good direction for this axis
  if (config.useSmartDirectionSelection &&
    directionMemory.lastGoodDirection.count(axis) > 0 &&
    directionMemory.consecutiveDeclines[axis] < 2) {

    std::string rememberedDirection = directionMemory.lastGoodDirection[axis];
    std::cout << "    Trying remembered good direction: " << rememberedDirection << std::endl;

    // Test the remembered direction first
    double deltaStep = (rememberedDirection == "Positive") ? stepSize : -stepSize;
    ScanStep testStep = moveAxis(current, axis, deltaStep);

    double improvement = (testStep.value - current.value) / current.value;
    if (improvement > getAxisThreshold(axis)) {
      std::cout << "    Remembered direction worked! Improvement: "
        << (improvement * 100) << "%" << std::endl;
      return rememberedDirection;
    }
    else {
      std::cout << "    Remembered direction failed, trying both directions" << std::endl;
    }
  }

  // Test both directions
  ScanStep positiveStep = moveAxis(current, axis, stepSize);
  ScanStep negativeStep = moveAxis(current, axis, -stepSize);

  double positiveImprovement = (positiveStep.value - current.value) / current.value;
  double negativeImprovement = (negativeStep.value - current.value) / current.value;

  std::cout << "    Positive direction: " << (positiveImprovement * 100) << "% improvement" << std::endl;
  std::cout << "    Negative direction: " << (negativeImprovement * 100) << "% improvement" << std::endl;

  double threshold = getAxisThreshold(axis);

  if (positiveImprovement > threshold && negativeImprovement > threshold) {
    // Both directions are good, pick the better one
    return (positiveImprovement > negativeImprovement) ? "Positive" : "Negative";
  }
  else if (positiveImprovement > threshold) {
    return "Positive";
  }
  else if (negativeImprovement > threshold) {
    return "Negative";
  }
  else {
    // Neither direction shows improvement
    return "";
  }
}

// Movement and measurement
ScanStep SequentialOptimizedScanner::moveAxis(const ScanStep& current, const std::string& axis, double delta) {
  ScanStep next = current;

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
    std::cout << "    Warning: Invalid position, returning current" << std::endl;
    return current;
  }

  next.axis = axis;
  next.direction = (delta > 0) ? "Positive" : "Negative";
  next.stepSize = std::abs(delta);
  next.measurementIndex = ++totalMeasurements;
  next.timestamp = getCurrentTimestamp();

  // Perform measurement
  next.value = performMeasurement(next);
  next.relativeImprovement = (next.value - current.value) / current.value;

  // Update best position
  if (next.value > currentBest.value) {
    currentBest = next;
    next.isPeak = true;
  }

  scanHistory.push_back(next);
  return next;
}

double SequentialOptimizedScanner::performMeasurement(const ScanStep& position) {
  return measurementFunc(position.x, position.y, position.z);
}

bool SequentialOptimizedScanner::isPositionValid(const ScanStep& position) const {
  return positionValidationFunc(position.x, position.y, position.z);
}

// Helper functions
double SequentialOptimizedScanner::getAxisValue(const ScanStep& step, const std::string& axis) const {
  if (axis == "X") return step.x;
  if (axis == "Y") return step.y;
  if (axis == "Z") return step.z;
  return 0.0;
}

void SequentialOptimizedScanner::setAxisValue(ScanStep& step, const std::string& axis, double value) {
  if (axis == "X") step.x = value;
  else if (axis == "Y") step.y = value;
  else if (axis == "Z") step.z = value;
}

double SequentialOptimizedScanner::getAxisThreshold(const std::string& axis) const {
  if (axis == "Z") return config.zConfig.minImprovementThreshold;
  return config.xyConfig.minImprovementThreshold;
}

SequentialOptimizedScanner::AxisConfig SequentialOptimizedScanner::getAxisConfig(const std::string& axis) const {
  if (axis == "Z") return config.zConfig;
  return config.xyConfig;
}

void SequentialOptimizedScanner::updateDirectionMemory(const std::string& axis,
  const std::string& direction,
  double improvement) {
  directionMemory.lastGoodDirection[axis] = direction;
  directionMemory.lastGoodImprovement[axis] = improvement;
  directionMemory.consecutiveDeclines[axis] = 0;
}

// Configuration methods
void SequentialOptimizedScanner::setZAxisSteps(double coarse, double fine, double ultraFine) {
  config.zConfig.stepSizeCoarse = coarse;
  config.zConfig.stepSizeFine = fine;
  config.zConfig.stepSizeUltraFine = ultraFine;
}

void SequentialOptimizedScanner::setXYAxisSteps(double coarse, double fine, double ultraFine) {
  config.xyConfig.stepSizeCoarse = coarse;
  config.xyConfig.stepSizeFine = fine;
  config.xyConfig.stepSizeUltraFine = ultraFine;
}

void SequentialOptimizedScanner::setAxisThresholds(double zThreshold, double xyThreshold) {
  config.zConfig.minImprovementThreshold = zThreshold;
  config.xyConfig.minImprovementThreshold = xyThreshold;
}

void SequentialOptimizedScanner::setMaxStepsPerPhase(int zMaxSteps, int xyMaxSteps) {
  config.zConfig.maxStepsPerPhase = zMaxSteps;
  config.xyConfig.maxStepsPerPhase = xyMaxSteps;
}

// Results and statistics
std::vector<ScanStep> SequentialOptimizedScanner::getScanHistory() const {
  return scanHistory;
}

ScanStep SequentialOptimizedScanner::getBestPosition() const {
  return currentBest;
}

int SequentialOptimizedScanner::getTotalMeasurements() const {
  return totalMeasurements;
}

std::map<std::string, int> SequentialOptimizedScanner::getMeasurementCountsByAxis() const {
  std::map<std::string, int> counts;
  for (const auto& step : scanHistory) {
    if (!step.axis.empty()) {
      counts[step.axis]++;
    }
  }
  return counts;
}

std::map<std::string, double> SequentialOptimizedScanner::getAverageImprovementByAxis() const {
  std::map<std::string, std::vector<double>> improvements;
  std::map<std::string, double> averages;

  for (const auto& step : scanHistory) {
    if (!step.axis.empty()) {
      improvements[step.axis].push_back(step.relativeImprovement);
    }
  }

  for (const auto& [axis, values] : improvements) {
    if (!values.empty()) {
      double sum = 0;
      for (double val : values) sum += val;
      averages[axis] = sum / values.size();
    }
  }

  return averages;
}

double SequentialOptimizedScanner::getTotalImprovement() const {
  if (scanHistory.empty()) return 0.0;

  double initial = scanHistory[0].value;
  double final = currentBest.value;
  return (final - initial) / initial;
}

// Reporting methods
void SequentialOptimizedScanner::printOptimizationSummary(const ScanStep& start, const ScanStep & final) const {
  std::cout << "\n=== Optimization Summary ===" << std::endl;
  std::cout << "Total measurements: " << totalMeasurements << std::endl;

  // Count measurements per axis
  auto axisCounts = getMeasurementCountsByAxis();
  std::cout << "Measurements per axis:" << std::endl;
  for (const auto& [axis, count] : axisCounts) {
    double percentage = (100.0 * count) / totalMeasurements;
    std::cout << "  " << axis << ": " << count << " (" << std::fixed
      << std::setprecision(1) << percentage << "%)" << std::endl;
  }

  double totalImprovement = (final.value - start.value) / start.value;
  std::cout << "Total improvement: " << std::fixed << std::setprecision(2)
    << (totalImprovement * 100) << "%" << std::endl;
  std::cout << "Final position: (" << std::setprecision(8) << final.x << ", "
    << final.y << ", " << final.z << ")" << std::endl;
  std::cout << "Final value: " << std::setprecision(10) << final.value << std::endl;
  std::cout << "Peak value: " << currentBest.value << std::endl;

  // Direction memory summary
  std::cout << "\nDirection memory learned:" << std::endl;
  for (const auto& [axis, direction] : directionMemory.lastGoodDirection) {
    auto it = directionMemory.lastGoodImprovement.find(axis);
    if (it != directionMemory.lastGoodImprovement.end()) {
      double improvement = it->second;
      std::cout << "  " << axis << ": " << direction << " (best improvement: "
        << std::setprecision(2) << (improvement * 100) << "%)" << std::endl;
    }
  }
  std::cout << "=============================" << std::endl;
}

void SequentialOptimizedScanner::printPhaseResults(const std::string& phaseName,
  const ScanStep& before,
  const ScanStep& after) const {
  double improvement = (after.value - before.value) / before.value;
  std::cout << "After " << phaseName << ": value = " << std::fixed << std::setprecision(10)
    << after.value << " (improvement: " << std::setprecision(3)
    << (improvement * 100) << "%)" << std::endl;
}

// Utility functions
std::string SequentialOptimizedScanner::getCurrentTimestamp() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

std::string SequentialOptimizedScanner::generateScanId() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << "optimized_scan_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
  return ss.str();
}

void SequentialOptimizedScanner::reset() {
  scanHistory.clear();
  totalMeasurements = 0;
  directionMemory.clear();
  currentBest = ScanStep();
}

// JSON export (compatible with your existing format)
std::string SequentialOptimizedScanner::exportToJSON(const std::string& scanId,
  const std::string& deviceId) const {
  std::stringstream json;
  json << std::fixed << std::setprecision(10);

  json << "{\n";
  json << "  \"scanId\": \"" << scanId << "\",\n";
  json << "  \"deviceId\": \"" << deviceId << "\",\n";
  json << "  \"algorithmType\": \"SequentialOptimizedScanner\",\n";

  if (!scanHistory.empty()) {
    const ScanStep& baseline = scanHistory[0];
    json << "  \"baseline\": {\n";
    json << "    \"position\": {\n";
    json << "      \"x\": " << baseline.x << ",\n";
    json << "      \"y\": " << baseline.y << ",\n";
    json << "      \"z\": " << baseline.z << "\n";
    json << "    },\n";
    json << "    \"timestamp\": \"" << baseline.timestamp << "\",\n";
    json << "    \"value\": " << baseline.value << "\n";
    json << "  },\n";

    json << "  \"peak\": {\n";
    json << "    \"position\": {\n";
    json << "      \"x\": " << currentBest.x << ",\n";
    json << "      \"y\": " << currentBest.y << ",\n";
    json << "      \"z\": " << currentBest.z << "\n";
    json << "    },\n";
    json << "    \"timestamp\": \"" << currentBest.timestamp << "\",\n";
    json << "    \"value\": " << currentBest.value << ",\n";
    json << "    \"context\": \"" << currentBest.axis << " axis scan, "
      << currentBest.direction << " direction, step size "
      << currentBest.stepSize << " microns\"\n";
    json << "  },\n";

    json << "  \"measurements\": [\n";
    for (size_t i = 1; i < scanHistory.size(); ++i) { // Skip baseline
      const ScanStep& step = scanHistory[i];
      json << "    {\n";
      json << "      \"axis\": \"" << step.axis << "\",\n";
      json << "      \"direction\": \"" << step.direction << "\",\n";
      json << "      \"stepSize\": " << step.stepSize << ",\n";
      json << "      \"position\": {\n";
      json << "        \"x\": " << step.x << ",\n";
      json << "        \"y\": " << step.y << ",\n";
      json << "        \"z\": " << step.z << "\n";
      json << "      },\n";
      json << "      \"value\": " << step.value << ",\n";
      json << "      \"relativeImprovement\": " << step.relativeImprovement << ",\n";
      json << "      \"isPeak\": " << (step.isPeak ? "true" : "false") << ",\n";
      json << "      \"isValid\": true,\n";
      json << "      \"timestamp\": \"" << step.timestamp << "\"\n";
      json << "    }";
      if (i < scanHistory.size() - 1) json << ",";
      json << "\n";
    }
    json << "  ],\n";

    // Statistics
    auto axisCounts = getMeasurementCountsByAxis();
    json << "  \"statistics\": {\n";
    json << "    \"totalMeasurements\": " << totalMeasurements << ",\n";
    json << "    \"measurementsPerAxis\": {\n";
    bool first = true;
    for (const auto& [axis, count] : axisCounts) {
      if (!first) json << ",\n";
      json << "      \"" << axis << "\": " << count;
      first = false;
    }
    json << "\n    },\n";

    // Calculate min/max values
    double minValue = scanHistory[0].value;
    double maxValue = scanHistory[0].value;
    double sumValues = 0;
    for (const auto& step : scanHistory) {
      minValue = std::min(minValue, step.value);
      maxValue = std::max(maxValue, step.value);
      sumValues += step.value;
    }
    double avgValue = sumValues / scanHistory.size();

    json << "    \"minValue\": " << minValue << ",\n";
    json << "    \"maxValue\": " << maxValue << ",\n";
    json << "    \"averageValue\": " << avgValue << ",\n";
    json << "    \"totalImprovement\": " << getTotalImprovement() << "\n";
    json << "  },\n";

    // Algorithm-specific statistics
    json << "  \"algorithmStats\": {\n";
    json << "    \"smartDirectionSelection\": " << (config.useSmartDirectionSelection ? "true" : "false") << ",\n";
    json << "    \"adaptiveStepSize\": " << (config.useAdaptiveStepSize ? "true" : "false") << ",\n";
    json << "    \"directionMemory\": {\n";
    first = true;
    for (const auto& [axis, direction] : directionMemory.lastGoodDirection) {
      if (!first) json << ",\n";
      json << "      \"" << axis << "\": {\n";
      json << "        \"lastGoodDirection\": \"" << direction << "\",\n";
      auto it = directionMemory.lastGoodImprovement.find(axis);
      if (it != directionMemory.lastGoodImprovement.end()) {
        json << "        \"lastImprovement\": " << it->second << "\n";
      }
      else {
        json << "        \"lastImprovement\": 0.0\n";
      }
      json << "      }";
      first = false;
    }
    json << "\n    }\n";
    json << "  }\n";
  }

  json << "}";
  return json.str();
}