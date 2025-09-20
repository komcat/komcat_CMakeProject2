// include/scanning/grid_scanner.h
#pragma once

#include "include/scanning/i_scan_motion_controller.h"
#include "include/data/data_client_manager.h"
#include "include/data/global_data_store.h"
#include "include/logger.h"
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>
#include <chrono>

// Forward declaration
struct DataPoint;

struct GridPoint {
  double x;
  double y;
  int row;
  int col;

  GridPoint() : x(0), y(0), row(0), col(0) {}
  GridPoint(double _x, double _y, int _row, int _col)
    : x(_x), y(_y), row(_row), col(_col) {
  }
};

struct GridScanData {
  GridPoint position;
  double value;
  double timestamp;
  std::vector<double> trajectory;
};

// Add to grid_scanner.h
struct GridPoint3D {
  double x, y, z;
  int row, col, layer;

  GridPoint3D() : x(0), y(0), z(0), row(0), col(0), layer(0) {}
  GridPoint3D(double _x, double _y, double _z, int _row, int _col, int _layer)
    : x(_x), y(_y), z(_z), row(_row), col(_col), layer(_layer) {
  }
};

struct Volume3DData {
  std::vector<std::vector<std::vector<double>>> data;  // [layer][row][col]
  std::vector<double> zPositions;                      // Z position for each layer
  GridPoint3D peakPosition;
  double peakValue;
  std::string scanId;
  double timestamp;
};

class GridScanner : public IDataSubscriber, public IPositionSubscriber {
public:
  // Constructor now takes the motion interface
  // Modified constructor - allow null motion controller
  GridScanner(std::shared_ptr<IScanMotionController> motionController,  // can be null
    DataClientManager& dataManager,
    const std::string& dataChannel);

  virtual ~GridScanner();

  // Configuration
  void SetGridParameters(double xStep = 50.0,
    double yStep = 10.0,
    int xPoints = 2,
    int yPoints = 5);

  void SetSettlingTime(int milliseconds) { m_settlingTimeMs = milliseconds; }
  void SetMoveTimeout(int seconds) { m_moveTimeoutSec = seconds; }

  // Main scanning interface
  bool StartScan(std::function<void(const GridPoint&, double)> updateCallback = nullptr);
  void StopScan();
  bool IsScanActive() const { return m_scanning.load(); }
  double GetProgress() const { return m_progress.load(); }

  // Data access
  std::vector<GridScanData> GetCollectedData() const;
  std::vector<std::vector<double>> GetDataGrid() const;
  GridPoint GetCurrentPosition() const;
  double GetLastValue() const;

  // IDataSubscriber implementation
  void OnDataReceived(const std::string& channelId,
    float value,
    const DataPoint& dataPoint) override;
  void OnConnectionChanged(const std::string& channelId, bool connected) override;
  void OnDataError(const std::string& channelId,
    const std::string& errorMessage) override;
  std::string GetSubscriberName() const override {
    // FIX: Check for null motion controller
    if (m_motionController) {
      return "GridScanner_" + m_motionController->GetDeviceName();
    }
    return "GridScanner_NoDevice";
  }

  // Add method to set/change motion controller
  void SetMotionController(std::shared_ptr<IScanMotionController> controller);
  bool HasMotionController() const { return m_motionController != nullptr; }
  bool IsMotionControllerConnected() const {
    return m_motionController && m_motionController->IsConnected();
  }

  // IPositionSubscriber implementation
  void OnPositionsUpdate(const std::string& deviceName,
    const std::map<std::string, double>& positions) override;

  void OnMotionStatusChange(const std::string& deviceName,
    const std::string& axis,
    bool isMoving) override;

  bool IsDataChannelConnected() const;

  // Add method to change data channel
  void SetDataChannel(const std::string& channel);

  double GetPeakValue() const { return m_peakValue; }
  GridPoint GetPeakPosition() const { return m_peakPosition; }
  bool HasValidPeak() const { return m_peakValue > -std::numeric_limits<double>::infinity(); }

  // Add these accessor methods to expose protected members
  std::shared_ptr<IScanMotionController> GetMotionController() const {
    return m_motionController;
  }

  DataClientManager& GetDataManager() const {
    return m_dataManager;
  }

  // Grid parameter accessors
  double GetXStep() const { return m_xStep; }
  double GetYStep() const { return m_yStep; }
  int GetXPoints() const { return m_xPoints; }
  int GetYPoints() const { return m_yPoints; }
  double GetOriginX() const { return m_originX; }
  double GetOriginY() const { return m_originY; }

  // Timing accessors
  int GetSettlingTimeMs() const { return m_settlingTimeMs; }
  int GetMoveTimeoutSec() const { return m_moveTimeoutSec; }

protected:
  // Motion controller interface
  std::shared_ptr<IScanMotionController> m_motionController;
  DataClientManager& m_dataManager;

  // Data channel
  std::string m_dataChannel;

  // Grid parameters
  double m_xStep;
  double m_yStep;
  int m_xPoints;
  int m_yPoints;
  int m_settlingTimeMs;
  int m_moveTimeoutSec;

  // Origin position
  double m_originX;
  double m_originY;

  // Scan state
  std::atomic<bool> m_scanning;
  std::atomic<double> m_progress;
  std::thread m_scanThread;
  std::atomic<bool> m_stopRequested;

  // Data collection
  mutable std::mutex m_dataMutex;
  std::vector<GridScanData> m_scanData;

  // Current state tracking
  GridPoint m_currentTarget;
  std::atomic<double> m_lastValue{ 0.0 };
  std::chrono::steady_clock::time_point m_lastDataTime;
  std::atomic<bool> m_dataChannelActive{ false };

  // Callback for UI updates
  std::function<void(const GridPoint&, double)> m_updateCallback;

  // Internal methods
  std::vector<GridPoint> GenerateSnakePattern();
  void ScanThreadFunc();


  // Peak tracking
  double m_peakValue = -std::numeric_limits<double>::infinity();
  GridPoint m_peakPosition;

  // Original position (before scan started)
  double m_originalX = 0.0;
  double m_originalY = 0.0;

  // Add member to track current positions
  std::map<std::string, double> m_currentPositions;
  std::atomic<bool> m_motionComplete{ false };

  bool WaitForMove(int timeoutMs);
  void LogScanInfo(const std::string& message);
};