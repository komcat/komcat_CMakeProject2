#pragma once
#include <vector>
#include <string>
#include <map>
#include <functional>
#include <cmath>

struct ScanStep {
  double x, y, z;
  double value;
  double relativeImprovement;
  std::string axis;
  std::string direction;
  double stepSize;
  bool isPeak;
  int measurementIndex;
  std::string timestamp;

  // Constructor
  ScanStep(double x = 0, double y = 0, double z = 0, double value = 0)
    : x(x), y(y), z(z), value(value), relativeImprovement(0),
    stepSize(0), isPeak(false), measurementIndex(0) {
  }
};

class AdaptivePowerScanner {
public:
  // Power-to-step-size mapping configuration
  struct PowerStepMapping {
    double maxPower;           // Maximum expected power (e.g., 400e-6)
    double minPower;           // Minimum threshold power (e.g., 2e-6)
    double minStepSize;        // Step size at max power (e.g., 0.0002 mm = 0.2 microns)
    double maxStepSize;        // Step size at min power (e.g., 0.010 mm = 10 microns)
    double gaussianSigma;      // Controls the curvature of power-distance relationship

    PowerStepMapping()
      : maxPower(400e-6), minPower(2e-6),
      minStepSize(0.0002), maxStepSize(0.010),
      gaussianSigma(0.001) {
    } // 1 micron sigma
  };

  // Physics-based direction constraints
  struct DirectionConstraints {
    std::map<std::string, std::string> forcedDirection; // axis -> "Positive"/"Negative"/"Both"
    std::map<std::string, double> maxTravel;           // axis -> max distance in mm

    DirectionConstraints() {
      // Default: Z- increases power (lens closer to source)
      forcedDirection["X"] = "Both";
      forcedDirection["Y"] = "Both";
      forcedDirection["Z"] = "Negative";  // Z- only increases power

      maxTravel["X"] = 0.005;  // 5mm max travel
      maxTravel["Y"] = 0.005;
      maxTravel["Z"] = 0.005;
    }
  };

  // Configuration structure
  struct ScanConfig {
    PowerStepMapping powerMapping;
    DirectionConstraints directionConstraints;

    // Algorithm parameters
    double convergenceThreshold;    // Stop when improvement < this
    int maxMeasurementsPerAxis;    // Safety limit
    double improvementThreshold;    // Minimum improvement to continue
    bool usePhysicsConstraints;    // Enable direction forcing
    bool usePowerAdaptiveSteps;    // Enable power-based step sizing

    ScanConfig()
      : convergenceThreshold(0.001),   // 0.1%
      maxMeasurementsPerAxis(20),
      improvementThreshold(0.005),   // 0.5%
      usePhysicsConstraints(true),
      usePowerAdaptiveSteps(true) {
    }
  };

private:
  // Internal state
  ScanConfig config;
  ScanStep currentBest;
  std::vector<ScanStep> scanHistory;
  int totalMeasurements;

  // Hardware interface functions
  std::function<double(double x, double y, double z)> measurementFunc;
  std::function<bool(double x, double y, double z)> positionValidationFunc;

public:
  // Constructor
  explicit AdaptivePowerScanner(const ScanConfig& cfg = ScanConfig());

  // Hardware interface setup
  void setMeasurementFunction(std::function<double(double x, double y, double z)> func);
  void setPositionValidationFunction(std::function<bool(double x, double y, double z)> func);

  // Configuration methods
  void setPowerRange(double minPower, double maxPower);
  void setStepSizeRange(double minStepMicrons, double maxStepMicrons);
  void setAxisDirection(const std::string& axis, const std::string& direction);
  void setGaussianSigma(double sigmaMicrons);

  // Main scanning function
  ScanStep adaptivePowerScan(const ScanStep& startPosition);

  // Results and statistics
  std::vector<ScanStep> getScanHistory() const;
  ScanStep getBestPosition() const;
  int getTotalMeasurements() const;
  double getTotalImprovement() const;

  // Analysis methods
  std::map<std::string, int> getMeasurementCountsByAxis() const;
  std::map<std::string, double> getAverageStepSizeByAxis() const;

  // Reset for new scan
  void reset();
  // Power-based step size calculation
  double calculateStepSize(double currentPower) const;
private:
  // Core adaptive optimization
  ScanStep optimizeAxis(const ScanStep& start, const std::string& axis);



  // Physics-based direction selection
  std::vector<std::string> getAllowedDirections(const std::string& axis) const;

  // Movement and measurement
  ScanStep moveAxis(const ScanStep& current, const std::string& axis, double delta);
  ScanStep testDirection(const ScanStep& current, const std::string& axis,
    const std::string& direction, double stepSize);
  double performMeasurement(const ScanStep& position);
  bool isPositionValid(const ScanStep& position) const;
  bool isWithinTravelLimits(const ScanStep& position) const;

  // Helper functions
  double getAxisValue(const ScanStep& step, const std::string& axis) const;
  void setAxisValue(ScanStep& step, const std::string& axis, double value);
  std::string getCurrentTimestamp() const;

  // Reporting
  void printScanSummary(const ScanStep& start, const ScanStep & final) const;
  void printAxisOptimization(const std::string& axis, const ScanStep& before,
    const ScanStep& after, int measurements) const;
};