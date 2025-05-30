#pragma once
#include <vector>
#include <string>
#include <map>
#include <functional>

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

class SequentialOptimizedScanner {
public:
  // Configuration structures
  struct AxisConfig {
    double stepSizeCoarse;
    double stepSizeFine;
    double stepSizeUltraFine;
    int maxStepsPerPhase;
    double minImprovementThreshold;
  };

  struct ScanConfig {
    // Z-axis configuration (primary optimization axis)
    AxisConfig zConfig;

    // XY-axis configuration (fine positioning)
    AxisConfig xyConfig;

    // Algorithm parameters
    bool useSmartDirectionSelection;
    bool useAdaptiveStepSize;
    int maxConsecutiveDeclines;
    double convergenceThreshold;

    // Default constructor with optimized values
    ScanConfig();
  };

  // Hardware interface function type
  using MeasurementFunction = std::function<double(double x, double y, double z)>;
  using PositionValidationFunction = std::function<bool(double x, double y, double z)>;

private:
  // Direction selection tracking
  struct DirectionMemory {
    std::map<std::string, std::string> lastGoodDirection; // axis -> "Positive"/"Negative"
    std::map<std::string, double> lastGoodImprovement;   // axis -> improvement value
    std::map<std::string, int> consecutiveDeclines;      // axis -> decline count

    void clear();
  };

  // Internal state
  ScanConfig config;
  ScanStep currentBest;
  std::vector<ScanStep> scanHistory;
  int totalMeasurements;
  DirectionMemory directionMemory;

  // Hardware interface functions
  MeasurementFunction measurementFunc;
  PositionValidationFunction positionValidationFunc;

public:
  // Constructor
  explicit SequentialOptimizedScanner(const ScanConfig& cfg = ScanConfig());

  // Hardware interface setup
  void setMeasurementFunction(MeasurementFunction func);
  void setPositionValidationFunction(PositionValidationFunction func);

  // Main scanning function
  ScanStep optimizedSequentialScan(const ScanStep& startPosition);

  // Configuration methods
  void setZAxisSteps(double coarse, double fine, double ultraFine);
  void setXYAxisSteps(double coarse, double fine, double ultraFine);
  void setAxisThresholds(double zThreshold, double xyThreshold);
  void setMaxStepsPerPhase(int zMaxSteps, int xyMaxSteps);

  // Results and statistics
  std::vector<ScanStep> getScanHistory() const;
  ScanStep getBestPosition() const;
  int getTotalMeasurements() const;

  // Analysis methods
  std::map<std::string, int> getMeasurementCountsByAxis() const;
  std::map<std::string, double> getAverageImprovementByAxis() const;
  double getTotalImprovement() const;

  // JSON export (compatible with your existing format)
  std::string exportToJSON(const std::string& scanId, const std::string& deviceId) const;

  // Reset for new scan
  void reset();

private:
  // Phase implementations
  ScanStep coarseOptimizationPhase(const ScanStep& start);
  ScanStep fineOptimizationPhase(const ScanStep& start);
  ScanStep ultraFinePhase(const ScanStep& start);

  // Core optimization logic
  ScanStep smartAxisOptimization(const ScanStep& start, const std::string& axis,
    double stepSize, int maxSteps);
  std::string selectBestDirection(const ScanStep& current, const std::string& axis,
    double stepSize);

  // Movement and measurement
  ScanStep moveAxis(const ScanStep& current, const std::string& axis, double delta);
  double performMeasurement(const ScanStep& position);
  bool isPositionValid(const ScanStep& position) const;

  // Helper functions
  double getAxisValue(const ScanStep& step, const std::string& axis) const;
  void setAxisValue(ScanStep& step, const std::string& axis, double value);
  double getAxisThreshold(const std::string& axis) const;
  AxisConfig getAxisConfig(const std::string& axis) const;

  // Statistics and reporting
  void updateDirectionMemory(const std::string& axis, const std::string& direction,
    double improvement);
  void printOptimizationSummary(const ScanStep& start, const ScanStep & final) const;
  void printPhaseResults(const std::string& phaseName, const ScanStep& before,
    const ScanStep& after) const;

  // Utility functions
  std::string getCurrentTimestamp() const;
  std::string generateScanId() const;
};