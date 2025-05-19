// src/scanning/scanning_algorithm.cpp
#include "include/scanning/scanning_algorithm.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <chrono>

ScanningAlgorithm::ScanningAlgorithm(
  PIController& piController,
  GlobalDataStore& dataStore,
  const std::string& deviceName,
  const std::string& dataChannel,
  const ScanningParameters& parameters)
  : m_piController(piController),
  m_dataStore(dataStore),
  m_deviceName(deviceName),
  m_dataChannel(dataChannel),
  m_parameters(parameters),
  m_dataCollector(deviceName),
  m_isScanningActive(false),
  m_isHaltRequested(false)
{
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("ScanningAlgorithm initialized for device: " + deviceName);
}

ScanningAlgorithm::~ScanningAlgorithm() {
  HaltScan();

  // Wait for scan thread to finish
  if (m_scanThread.joinable()) {
    m_scanThread.join();
  }

  m_logger->LogInfo("ScanningAlgorithm destroyed for device: " + m_deviceName);
}

bool ScanningAlgorithm::StartScan() {
  // Check if already scanning
  if (m_isScanningActive) {
    m_logger->LogWarning("Scan already in progress for device: " + m_deviceName);
    return false;
  }

  try {
    // Validate parameters
    m_parameters.Validate();

    // Check if controller is connected
    if (!m_piController.IsConnected()) {
      throw std::runtime_error("PI Controller is not connected");
    }

    // Reset state
    m_isScanningActive = true;
    m_isHaltRequested = false;

    // Start scan thread
    m_scanThread = std::thread(&ScanningAlgorithm::ScanThreadFunction, this);

    m_logger->LogInfo("Scan started for device: " + m_deviceName);
    return true;
  }
  catch (const std::exception& e) {
    m_logger->LogError("Failed to start scan: " + std::string(e.what()));
    m_isScanningActive = false;

    if (m_errorCallback) {
      m_errorCallback(ScanErrorEventArgs(e.what()));
    }

    return false;
  }
}

void ScanningAlgorithm::HaltScan() {
  if (!m_isScanningActive) {
    return;
  }

  m_logger->LogInfo("Halting scan for device: " + m_deviceName);
  m_isHaltRequested = true;

  // Wait for scan thread to finish
  if (m_scanThread.joinable()) {
    m_scanThread.join();
  }

  m_isScanningActive = false;
  m_logger->LogInfo("Scan halted for device: " + m_deviceName);
}

void ScanningAlgorithm::ScanThreadFunction() {
  try {
    // Record baseline
    if (!RecordBaseline()) {
      throw std::runtime_error("Failed to record baseline");
    }

    // Execute scan sequence
    if (!ExecuteScanSequence()) {
      if (m_isHaltRequested) {
        m_logger->LogInfo("Scan was halted by user");
      }
      else {
        throw std::runtime_error("Failed to execute scan sequence");
      }
    }

    // Return to global peak position
    if (!m_isHaltRequested && m_globalPeak.value > 0) {
      if (!MoveToPosition(m_globalPeak.position)) {
        m_logger->LogWarning("Failed to return to global peak position");
      }
      else {
        m_logger->LogInfo("Returned to global peak position with value: " +
          std::to_string(m_globalPeak.value));
      }
    }

    // Scan completed successfully
    CleanupScan(true);
  }
  catch (const std::exception& e) {
    m_logger->LogError("Error during scan: " + std::string(e.what()));

    // Handle cancellation
    HandleScanCancellation();

    // Notify about error
    if (m_errorCallback) {
      m_errorCallback(ScanErrorEventArgs(e.what()));
    }

    CleanupScan(false);
  }
}

bool ScanningAlgorithm::RecordBaseline() {
  m_logger->LogInfo("Recording baseline for device: " + m_deviceName);

  // Get current position
  PositionStruct currentPosition;
  bool gotPosition = GetCurrentPosition(currentPosition);
  if (!gotPosition) {
    m_logger->LogError("Failed to get current position for baseline");
    return false;
  }

  // Get current measurement
  //double currentValue = GetMeasurement();

  // Take multiple readings
  constexpr int numReadings = 5;
  double sum = 0.0;
  for (int i = 0; i < numReadings; i++) {
    sum += GetMeasurement();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  double currentValue = sum / numReadings;

  // Store baseline data
  m_baseline.value = currentValue;
  m_baseline.position = currentPosition;
  m_baseline.timestamp = std::chrono::system_clock::now();

  // Initialize global peak with baseline
  m_globalPeak.value = currentValue;
  m_globalPeak.position = currentPosition;
  m_globalPeak.timestamp = m_baseline.timestamp;
  m_globalPeak.context = "Initial Position";

  // Record in data collector
  m_dataCollector.RecordBaseline(currentValue, currentPosition);

  m_logger->LogInfo("Baseline recorded: Value=" + std::to_string(currentValue) +
    " at position X:" + std::to_string(currentPosition.x) +
    " Y:" + std::to_string(currentPosition.y) +
    " Z:" + std::to_string(currentPosition.z));

  // Notify progress
  OnProgressUpdated(0.0, "Baseline recorded");

  return true;
}

bool ScanningAlgorithm::ExecuteScanSequence() {
  // Iterate through step sizes (coarse to fine)
  for (size_t stepIndex = 0; stepIndex < m_parameters.stepSizes.size(); ++stepIndex) {
    double stepSize = m_parameters.stepSizes[stepIndex];

    if (!m_isScanningActive || m_isHaltRequested) {
      return false;
    }

    // Iterate through axes to scan
    for (size_t axisIndex = 0; axisIndex < m_parameters.axesToScan.size(); ++axisIndex) {
      const std::string& axis = m_parameters.axesToScan[axisIndex];

      if (!m_isScanningActive || m_isHaltRequested) {
        return false;
      }

      m_logger->LogInfo("Starting " + axis + " axis scan with step size " +
        std::to_string(stepSize * 1000) + " microns");

      // Calculate progress
      size_t currentStep = axisIndex + stepIndex * m_parameters.axesToScan.size();
      double totalSteps = m_parameters.axesToScan.size() * m_parameters.stepSizes.size();
      double progress = static_cast<double>(currentStep) / totalSteps;

      std::stringstream progressMsg;
      progressMsg << "Scanning " << axis << " axis with "
        << std::fixed << std::setprecision(3) << (stepSize * 1000)
        << " micron steps";

      OnProgressUpdated(progress, progressMsg.str());

      // Scan this axis
      if (!ScanAxis(axis, stepSize)) {
        if (m_isHaltRequested) {
          return false;
        }

        m_logger->LogWarning("Failed to scan " + axis + " axis, continuing with next axis");
        continue;
      }

      // Small delay between axes
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      // Return to global peak if significantly better
      if (!ReturnToGlobalPeakIfBetter()) {
        m_logger->LogWarning("Failed to return to global peak, continuing with next axis");
      }
    }
  }

  return true;
}

bool ScanningAlgorithm::ScanAxis(const std::string& axis, double stepSize) {
  m_logger->LogInfo("Starting " + axis + " axis scan with step size " +
    std::to_string(stepSize * 1000) + " microns");

  // Store initial position
  PositionStruct startPosition;
  if (!GetCurrentPosition(startPosition)) {
    m_logger->LogError("Failed to get current position for axis scan");
    return false;
  }

  double initialValue = GetMeasurement();

  // Scan in positive direction
  auto [positiveMaxValue, positiveMaxPosition] = ScanDirection(axis, stepSize, 1);

  if (m_isHaltRequested) {
    return false;
  }

  // Return to start position after positive scan
  m_logger->LogInfo("Returning to start position for negative " + axis + " axis scan");
  if (!MoveToPosition(startPosition)) {
    m_logger->LogError("Failed to return to start position");
    return false;
  }

  // Wait for motion to settle
  std::this_thread::sleep_for(std::chrono::milliseconds(m_parameters.motionSettleTimeMs));

  if (m_isHaltRequested) {
    return false;
  }

  // Scan in negative direction
  auto [negativeMaxValue, negativeMaxPosition] = ScanDirection(axis, stepSize, -1);

  // Determine the best position between positive and negative scans
  double bestValue = (std::max)(positiveMaxValue, negativeMaxValue);
  PositionStruct bestPosition = (positiveMaxValue > negativeMaxValue) ?
    positiveMaxPosition : negativeMaxPosition;

  // Update global peak
  std::stringstream context;
  context << axis << " axis scan with " << std::fixed << std::setprecision(3)
    << (stepSize * 1000) << " micron steps";

  UpdateGlobalPeak(bestValue, bestPosition, context.str());

  // Move to the best position found in this axis scan
  m_logger->LogInfo("Moving to best position found in " + axis + " axis scan");
  if (!MoveToPosition(bestPosition)) {
    m_logger->LogError("Failed to move to best position");
    return false;
  }

  // Wait for motion to settle
  std::this_thread::sleep_for(std::chrono::milliseconds(m_parameters.motionSettleTimeMs));

  return true;
}

std::pair<double, PositionStruct> ScanningAlgorithm::ScanDirection(
  const std::string& axis, double stepSize, int direction) {

  // Get current position
  PositionStruct currentPosition;
  if (!GetCurrentPosition(currentPosition)) {
    m_logger->LogError("Failed to get current position for direction scan");
    return { 0.0, currentPosition };
  }

  // Get current value
  double maxValue = GetMeasurement();
  PositionStruct maxPosition = currentPosition;

  double previousValue = maxValue;
  int consecutiveDecreases = 0;
  double totalDistance = 0.0;
  bool hasMovedFromMax = false;
  const double SIGNIFICANT_DECREASE_THRESHOLD = 0.05; // 5% threshold

  while (!m_isHaltRequested &&
    m_isScanningActive &&
    consecutiveDecreases < m_parameters.consecutiveDecreasesLimit &&
    totalDistance < m_parameters.maxTotalDistance) {

    // Move relative in the specified direction
    if (!MoveRelative(axis, direction * stepSize)) {
      m_logger->LogError("Failed to move relative in " + axis + " axis");
      break;
    }

    // Wait for motion to settle
    std::this_thread::sleep_for(std::chrono::milliseconds(m_parameters.motionSettleTimeMs));

    // Get the actual position after moving
    if (!GetCurrentPosition(currentPosition)) {
      m_logger->LogError("Failed to get current position after move");
      break;
    }

    // Get current value
    double currentValue = GetMeasurement();
    totalDistance += stepSize;

    // Calculate relative decrease from previous value
    double relativeDecrease = 0.0;
    if (previousValue > 0) {
      relativeDecrease = (previousValue - currentValue) / previousValue;
    }

    // Calculate gradient for monitoring
    double gradient = (currentValue - previousValue) / stepSize;

    // Record measurement
    m_dataCollector.RecordMeasurement(
      currentValue,
      currentPosition,
      axis,
      stepSize,
      direction
    );

    // Notify data point
    if (m_dataPointCallback) {
      m_dataPointCallback(currentValue, currentPosition);
    }

    // Log the data point
    std::stringstream logMsg;
    logMsg << axis << " " << (direction > 0 ? "+" : "-") << ": "
      << "Pos=" << std::fixed << std::setprecision(6) << GetAxisValue(currentPosition, axis)
      << "mm, Value=" << std::setprecision(6) << currentValue;

    m_logger->LogInfo(logMsg.str());

    // Check if this is a new maximum
    if (currentValue > maxValue) {
      maxValue = currentValue;
      maxPosition = currentPosition;
      consecutiveDecreases = 0;
      hasMovedFromMax = false;

      m_logger->LogInfo(logMsg.str() + " - New Local Maximum");
    }
    else {
      consecutiveDecreases++;
      hasMovedFromMax = true;

      // Check for significant decrease
      if (relativeDecrease > SIGNIFICANT_DECREASE_THRESHOLD) {
        m_logger->LogInfo("Significant decrease detected (" +
          std::to_string(relativeDecrease * 100) +
          "%). Stopping " + axis + " axis scan in this direction.");
        break;
      }

      // Check if we should return to local max
      if (consecutiveDecreases >= m_parameters.consecutiveDecreasesLimit) {
        m_logger->LogInfo("Consecutive decreases limit reached. Returning to local maximum.");
        break;
      }
    }

    previousValue = currentValue;
  }

  // Return to local maximum position if we've moved away from it
  if (hasMovedFromMax) {
    m_logger->LogInfo("Returning to local maximum position in " + axis + " axis");

    if (!MoveToPosition(maxPosition)) {
      m_logger->LogError("Failed to return to local maximum position");
      return { maxValue, maxPosition };
    }

    // Wait for motion to settle
    std::this_thread::sleep_for(std::chrono::milliseconds(m_parameters.motionSettleTimeMs));

    // Verify we're at the maximum
    double verificationValue = GetMeasurement();
    m_logger->LogInfo("Local maximum position verified: " + std::to_string(verificationValue));

    // Update maxValue in case there was any drift
    if (verificationValue > maxValue) {
      maxValue = verificationValue;
    }
  }

  return { maxValue, maxPosition };
}

bool ScanningAlgorithm::ReturnToGlobalPeakIfBetter() {
  if (m_globalPeak.value <= 0) {
    return true; // No peak yet
  }

  double currentValue = GetMeasurement();
  double improvement = m_globalPeak.value - currentValue;
  double relativeImprovement = 0.0;

  if (currentValue > 0) {
    relativeImprovement = improvement / currentValue;
  }

  if (relativeImprovement > m_parameters.improvementThreshold) {
    m_logger->LogInfo("Returning to better position (improvement: " +
      std::to_string(relativeImprovement * 100) + "%)");

    if (!MoveToPosition(m_globalPeak.position)) {
      m_logger->LogError("Failed to move to global peak position");
      return false;
    }

    // Wait for motion to settle
    std::this_thread::sleep_for(std::chrono::milliseconds(m_parameters.motionSettleTimeMs));

    // Verify the position
    double verificationValue = GetMeasurement();
    m_logger->LogInfo("Position verified with value: " + std::to_string(verificationValue));
  }

  return true;
}

void ScanningAlgorithm::UpdateGlobalPeak(double value, const PositionStruct& position, const std::string& context) {
  if (m_globalPeak.value <= 0 || value > m_globalPeak.value) {
    m_globalPeak.value = value;
    m_globalPeak.position = position;
    m_globalPeak.timestamp = std::chrono::system_clock::now();
    m_globalPeak.context = context;

    // Notify of peak update
    if (m_peakUpdateCallback) {
      m_peakUpdateCallback(value, position, context);
    }

    std::stringstream posStr;
    posStr << "X:" << std::fixed << std::setprecision(6) << position.x
      << " Y:" << std::setprecision(6) << position.y
      << " Z:" << std::setprecision(6) << position.z;

    m_logger->LogInfo("New global peak found: Value=" + std::to_string(value) +
      " at position " + posStr.str() + ", Context: " + context);
  }
}

bool ScanningAlgorithm::MoveToPosition(const PositionStruct& position) {
  try {
    // Use the PI controller to move to the absolute position using XYZ and UVW axes
    // Maintain the rotation axes values from the position structure
    bool success = m_piController.MoveToPositionAll(
      position.x, position.y, position.z,
      position.u, position.v, position.w,  // Use the actual UVW values from position
      false // non-blocking move
    );

    if (!success) {
      m_logger->LogError("Error moving device " + m_deviceName + " to position");
      return false;
    }

    // Wait for motion to complete using WaitForMotionCompletion for each axis
    // Need to wait for all axes including UVW if they changed
    bool allComplete = true;
    for (const auto& axis : { "X", "Y", "Z", "U", "V", "W" }) {
      if (!m_piController.WaitForMotionCompletion(axis)) {
        m_logger->LogError("Timeout waiting for " + std::string(axis) + " axis to complete motion");
        allComplete = false;
      }
    }

    return allComplete;
  }
  catch (const std::exception& e) {
    m_logger->LogError("Error moving device " + m_deviceName + " to position: " + e.what());
    return false;
  }
}

bool ScanningAlgorithm::MoveRelative(const std::string& axis, double distance) {
  try {
    // Use the PI controller to move relative directly
    bool success = m_piController.MoveRelative(axis, distance, false); // Non-blocking

    if (!success) {
      m_logger->LogError("Error moving " + axis + " axis relatively by " + std::to_string(distance));
      return false;
    }

    // Wait for motion to complete
    if (!m_piController.WaitForMotionCompletion(axis)) {
      m_logger->LogError("Timeout waiting for " + axis + " axis to complete relative motion");
      return false;
    }

    return true;
  }
  catch (const std::exception& e) {
    m_logger->LogError("Error moving " + axis + " axis relatively: " + e.what());
    return false;
  }
}

double ScanningAlgorithm::GetMeasurement() {
  // Define a timeout deadline
  auto deadline = std::chrono::system_clock::now() + m_parameters.measurementTimeout;

  while (std::chrono::system_clock::now() < deadline && !m_isHaltRequested) {
    // Try to get the measurement from the data store
    if (m_dataStore.HasValue(m_dataChannel)) {
      double value = m_dataStore.GetValue(m_dataChannel);

      // Check if value is within the allowed range
      if (value >= m_parameters.minValue && value <= m_parameters.maxValue) {
        return value;
      }
    }

    // Wait a short time before retrying
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  if (m_isHaltRequested) {
    throw std::runtime_error("Scan halted while waiting for measurement");
  }

  throw std::runtime_error("Failed to get measurement within timeout period");
}

bool ScanningAlgorithm::GetCurrentPosition(PositionStruct& position) {
  std::map<std::string, double> positions;
  if (m_piController.GetPositions(positions)) {
    position.x = positions["X"];
    position.y = positions["Y"];
    position.z = positions["Z"];
    position.u = positions["U"];  // Get actual UVW values
    position.v = positions["V"];
    position.w = positions["W"];
    return true;
  }
  return false;
}

double ScanningAlgorithm::GetAxisValue(const PositionStruct& position, const std::string& axis) {
  if (axis == "X") return position.x;
  if (axis == "Y") return position.y;
  if (axis == "Z") return position.z;

  // We're not using rotation axes in this implementation
  throw std::invalid_argument("Invalid axis: " + axis + " (only X, Y, Z are supported)");
}

void ScanningAlgorithm::HandleScanCancellation() {
  try {
    // Return to global peak after cancellation if we have one
    if (m_globalPeak.value > 0) {
      m_logger->LogInfo("Returning to global peak position after cancellation");

      if (!MoveToPosition(m_globalPeak.position)) {
        m_logger->LogError("Failed to return to global peak position after cancellation");
      }
      else {
        m_logger->LogInfo("Successfully returned to global peak position");
      }
    }
  }
  catch (const std::exception& e) {
    m_logger->LogError("Error during scan cancellation: " + std::string(e.what()));
  }
}

void ScanningAlgorithm::CleanupScan(bool success) {
  try {
    // Save all data
    if (!m_dataCollector.SaveResults()) {
      m_logger->LogWarning("Failed to save scan results");
    }
    else {
      m_logger->LogInfo("Scan results saved successfully");
    }

    // Update state
    m_isScanningActive = false;

    // Notify completion
    if (success && m_completionCallback) {
      m_completionCallback(ScanCompletedEventArgs(m_dataCollector.GetResults()));
    }
  }
  catch (const std::exception& e) {
    m_logger->LogError("Error during scan cleanup: " + std::string(e.what()));
  }
}

void ScanningAlgorithm::OnProgressUpdated(double progress, const std::string& status) {
  if (m_progressCallback) {
    m_progressCallback(ScanProgressEventArgs(progress, status));
  }
}

void ScanningAlgorithm::OnScanCompleted(const ScanResults& results) {
  if (m_completionCallback) {
    m_completionCallback(ScanCompletedEventArgs(results));
  }
}

void ScanningAlgorithm::OnErrorOccurred(const std::string& message) {
  if (m_errorCallback) {
    m_errorCallback(ScanErrorEventArgs(message));
  }
}