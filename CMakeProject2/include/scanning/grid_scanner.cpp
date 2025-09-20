// include/scanning/grid_scanner.cpp
#include "include/scanning/grid_scanner.h"
#include "include/data/global_data_store.h"
#include "include/scanning/PIScanMotionAdapter.h"
#include <chrono>
#include <cmath>
#include <algorithm>
#include <sstream>

GridScanner::GridScanner(std::shared_ptr<IScanMotionController> motionController,
  DataClientManager& dataManager,
  const std::string& dataChannel)
  : m_motionController(motionController)
  , m_dataManager(dataManager)
  , m_dataChannel(dataChannel)
  , m_xStep(50.0)
  , m_yStep(10.0)
  , m_xPoints(2)
  , m_yPoints(5)
  , m_settlingTimeMs(100)
  , m_moveTimeoutSec(10)
  , m_originX(0.0)
  , m_originY(0.0)
  , m_scanning(false)
  , m_progress(0.0)
  , m_stopRequested(false)
  , m_lastValue(0.0) {

  // FIX: Check if motion controller is not null before using it
  std::string deviceInfo = motionController ? motionController->GetDeviceName() : "No Device";

  LogScanInfo("GridScanner created for device: " + deviceInfo + ", channel: " + dataChannel);

  // Subscribe to data channel
  m_dataManager.Subscribe(m_dataChannel, this);
}

GridScanner::~GridScanner() {
  StopScan();
  m_dataManager.Unsubscribe(m_dataChannel, this);
  LogScanInfo("GridScanner destroyed");
}

void GridScanner::SetGridParameters(double xStep, double yStep,
  int xPoints, int yPoints) {
  if (m_scanning) {
    Logger::GetInstance()->LogWarning("GridScanner: Cannot change parameters during scan");
    return;
  }

  // Validate step sizes (limit to reasonable range)
  if (xStep > 1000.0) { // Max 1mm (1000 µm)
    Logger::GetInstance()->LogWarning("GridScanner: X step too large, limiting to 1000 µm");
    xStep = 1000.0;
  }
  if (yStep > 1000.0) { // Max 1mm (1000 µm)
    Logger::GetInstance()->LogWarning("GridScanner: Y step too large, limiting to 1000 µm");
    yStep = 1000.0;
  }

  m_xStep = xStep;
  m_yStep = yStep;
  m_xPoints = (std::max)(1, xPoints);
  m_yPoints = (std::max)(1, yPoints);

  LogScanInfo("Grid parameters set: " +
    std::to_string(m_xPoints) + "x" + std::to_string(m_yPoints) +
    " points, step: " + std::to_string(m_xStep) + "x" + std::to_string(m_yStep) + " µm");
}

bool GridScanner::StartScan(std::function<void(const GridPoint&, double)> updateCallback) {
  if (m_scanning) {
    Logger::GetInstance()->LogWarning("GridScanner: Scan already in progress");
    return false;
  }

  // IMPORTANT: Make sure previous thread is cleaned up
  if (m_scanThread.joinable()) {
    Logger::GetInstance()->LogWarning("GridScanner: Previous scan thread still exists, cleaning up");
    m_scanThread.join();
  }

  // Check if motion controller exists and is connected
  if (!m_motionController) {
    Logger::GetInstance()->LogError("GridScanner: No motion controller set");
    return false;
  }

  if (!m_motionController->IsConnected()) {
    Logger::GetInstance()->LogError("GridScanner: Motion controller not connected");
    return false;
  }

  // Check that we can access GlobalDataStore
  GlobalDataStore* dataStore = GlobalDataStore::GetInstance();
  if (!dataStore) {
    Logger::GetInstance()->LogError("GridScanner: GlobalDataStore not available");
    return false;
  }

  m_updateCallback = updateCallback;
  m_scanning = true;
  m_progress = 0.0;
  m_stopRequested = false;

  // Clear previous scan data
  {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_scanData.clear();
  }

  LogScanInfo("Starting grid scan...");

  // Start scan thread
  m_scanThread = std::thread(&GridScanner::ScanThreadFunc, this);

  return true;
}

void GridScanner::StopScan() {
  if (!m_scanning && !m_scanThread.joinable()) {
    return;  // Nothing to stop
  }

  LogScanInfo("Stopping scan...");
  m_stopRequested = true;

  // Give thread time to notice stop request
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  if (m_scanThread.joinable()) {
    m_scanThread.join();
  }

  m_scanning = false;
  m_progress = 0.0;  // Reset progress
  LogScanInfo("Scan stopped");
}

std::vector<std::vector<double>> GridScanner::GetDataGrid() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);

  // Initialize grid with NaN for unmeasured points
  std::vector<std::vector<double>> grid(m_yPoints,
    std::vector<double>(m_xPoints, std::numeric_limits<double>::quiet_NaN()));

  // Fill grid with collected data
  for (const auto& data : m_scanData) {
    if (data.position.row >= 0 && data.position.row < m_yPoints &&
      data.position.col >= 0 && data.position.col < m_xPoints) {
      grid[data.position.row][data.position.col] = data.value;
    }
  }

  return grid;
}

GridPoint GridScanner::GetCurrentPosition() const {
  return m_currentTarget;
}

double GridScanner::GetLastValue() const {
  return m_lastValue.load();
}

std::vector<GridScanData> GridScanner::GetCollectedData() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  return m_scanData;
}

void GridScanner::OnDataReceived(const std::string& channelId,
  float value,
  const DataPoint& dataPoint) {
  // Just track that channel is active, don't store values
  if (channelId == m_dataChannel || channelId == "(" + m_dataChannel + ")") {
    m_lastDataTime = std::chrono::steady_clock::now();
    m_dataChannelActive = true;
    m_lastValue = static_cast<double>(value);
  }
}

void GridScanner::OnConnectionChanged(const std::string& channelId, bool connected) {
  LogScanInfo("Data channel " + channelId + " connection changed: " +
    (connected ? "connected" : "disconnected"));

  if (!connected && m_scanning && channelId == m_dataChannel) {
    Logger::GetInstance()->LogWarning("GridScanner: Data channel disconnected during scan!");
  }
}

void GridScanner::OnDataError(const std::string& channelId,
  const std::string& errorMessage) {
  if (channelId != m_dataChannel) {
    return;
  }

  Logger::GetInstance()->LogError("GridScanner: Data error on channel " +
    channelId + ": " + errorMessage);

  if (m_scanning) {
    Logger::GetInstance()->LogWarning("GridScanner: Data error during active scan");
  }
}

void GridScanner::LogScanInfo(const std::string& message) {
  std::string deviceName = m_motionController ? m_motionController->GetDeviceName() : "NoDevice";
  Logger::GetInstance()->LogInfo("GridScanner[" + deviceName + "]: " + message);
}

std::vector<GridPoint> GridScanner::GenerateSnakePattern() {
  std::vector<GridPoint> points;

  // Get current position as center
  if (!m_motionController->GetCurrentXY(m_originX, m_originY)) {
    Logger::GetInstance()->LogError("GridScanner: Failed to get current position");
    return points;
  }

  LogScanInfo("Center position: X=" + std::to_string(m_originX) +
    "mm, Y=" + std::to_string(m_originY) + "mm");

  // Calculate the starting position (top-left corner of the grid)
  double xOffset = ((m_xPoints - 1) * m_xStep / 2.0) / 1000.0; // Convert µm to mm
  double yOffset = ((m_yPoints - 1) * m_yStep / 2.0) / 1000.0; // Convert µm to mm

  double startX = m_originX - xOffset;  // Left side of grid
  double startY = m_originY + yOffset;  // Top side of grid

  LogScanInfo("Grid corners: TopLeft(" + std::to_string(startX) + ", " +
    std::to_string(startY) + ") to BottomRight(" +
    std::to_string(startX + (m_xPoints - 1) * m_xStep / 1000.0) + ", " +
    std::to_string(startY - (m_yPoints - 1) * m_yStep / 1000.0) + ")");

  // Generate snake pattern starting from top-left
  for (int row = 0; row < m_yPoints; ++row) {
    double y = startY - (row * m_yStep / 1000.0); // Y decreases as we go down

    if (row % 2 == 0) {
      // Even rows: left to right (positive X direction)
      for (int col = 0; col < m_xPoints; ++col) {
        double x = startX + (col * m_xStep / 1000.0);
        points.push_back(GridPoint(x, y, row, col));
      }
    }
    else {
      // Odd rows: right to left (snake pattern)
      for (int col = m_xPoints - 1; col >= 0; --col) {
        double x = startX + (col * m_xStep / 1000.0);
        points.push_back(GridPoint(x, y, row, col));
      }
    }
  }

  LogScanInfo("Generated " + std::to_string(points.size()) + " grid points in centered snake pattern");

  // Log the corner points for verification
  if (!points.empty()) {
    LogScanInfo("First point (top-left): X=" + std::to_string(points[0].x) +
      "mm, Y=" + std::to_string(points[0].y) + "mm");
    LogScanInfo("Last point: X=" + std::to_string(points.back().x) +
      "mm, Y=" + std::to_string(points.back().y) + "mm");

    // Show relative positions from center
    LogScanInfo("First point relative to center: dX=" +
      std::to_string((points[0].x - m_originX) * 1000.0) +
      "µm, dY=" + std::to_string((points[0].y - m_originY) * 1000.0) + "µm");
  }

  return points;
}

void GridScanner::ScanThreadFunc() {
  // Check connection
  if (!m_motionController || !m_motionController->IsConnected()) {
    Logger::GetInstance()->LogError("GridScanner: Motion controller not connected");
    m_scanning = false;
    return;
  }

  // Get GlobalDataStore instance
  GlobalDataStore* dataStore = GlobalDataStore::GetInstance();
  if (!dataStore) {
    Logger::GetInstance()->LogError("GridScanner: GlobalDataStore not available");
    m_scanning = false;
    return;
  }

  // Store original position before starting scan
  if (!m_motionController->GetCurrentXY(m_originalX, m_originalY)) {
    Logger::GetInstance()->LogError("GridScanner: Failed to get original position");
    m_scanning = false;
    return;
  }
  LogScanInfo("Original position stored: X=" + std::to_string(m_originalX) +
    "mm, Y=" + std::to_string(m_originalY) + "mm");

  // Reset peak tracking
  m_peakValue = -std::numeric_limits<double>::infinity();
  m_peakPosition = GridPoint();

  // Generate grid points from current position (which becomes origin for grid)
  std::vector<GridPoint> gridPoints = GenerateSnakePattern();
  if (gridPoints.empty()) {
    Logger::GetInstance()->LogError("GridScanner: Failed to generate grid points");
    m_scanning = false;
    return;
  }

  int totalPoints = static_cast<int>(gridPoints.size());
  LogScanInfo("Scanning " + std::to_string(totalPoints) + " points");

  // Determine the correct data key format
  std::string dataKey = m_dataChannel;  // Start with plain channel name
  float testValue = 0.0f;
  if (!dataStore->TryGetValue(dataKey, testValue)) {
    // Try with parentheses
    dataKey = "(" + m_dataChannel + ")";
    if (!dataStore->TryGetValue(dataKey, testValue)) {
      Logger::GetInstance()->LogWarning("GridScanner: Channel " + m_dataChannel +
        " not found in GlobalDataStore");
    }
  }
  LogScanInfo("Using data key: " + dataKey);

  // Scan each point
  for (int i = 0; i < totalPoints && !m_stopRequested; ++i) {
    const GridPoint& target = gridPoints[i];
    m_currentTarget = target;

    std::stringstream logMsg;
    logMsg << "Moving to point " << (i + 1) << "/" << totalPoints
      << " (row:" << target.row << ", col:" << target.col << ")"
      << " X:" << target.x << " Y:" << target.y;
    LogScanInfo(logMsg.str());

    // Move to target position using interface
    bool moveSuccess = m_motionController->MoveToXY(target.x, target.y, false); // non-blocking

    if (!moveSuccess) {
      Logger::GetInstance()->LogError("GridScanner: Failed to initiate move to point");
      continue;
    }

    // Wait for motion to complete
    if (!WaitForMove(m_moveTimeoutSec * 1000)) {
      Logger::GetInstance()->LogError("GridScanner: Move timeout exceeded");
      continue;
    }

    // Wait for settling
    std::this_thread::sleep_for(std::chrono::milliseconds(m_settlingTimeMs));

    // Collect data at this point
    GridScanData scanData;
    scanData.position = target;
    scanData.timestamp = std::chrono::duration<double>(
      std::chrono::system_clock::now().time_since_epoch()).count();

    // Get single current value directly from GlobalDataStore - NO AVERAGING
    float currentValue = 0.0f;
    if (dataStore->TryGetValue(dataKey, currentValue)) {
      scanData.value = static_cast<double>(currentValue);
      m_lastValue = scanData.value;
    }
    else {
      // If can't get value from store, use last known value
      scanData.value = m_lastValue.load();
      Logger::GetInstance()->LogWarning("Could not get value from GlobalDataStore at point " +
        std::to_string(i + 1));
    }

    // Check for peak value
    if (scanData.value > m_peakValue) {
      m_peakValue = scanData.value;
      m_peakPosition = target;
      LogScanInfo("New peak found: " + std::to_string(m_peakValue) +
        " at grid[" + std::to_string(target.col) + "," +
        std::to_string(target.row) + "] physical(" +
        std::to_string(target.x) + "mm, " +
        std::to_string(target.y) + "mm)");
    }

    // Store the scan data
    {
      std::lock_guard<std::mutex> lock(m_dataMutex);
      m_scanData.push_back(scanData);
    }

    // Update progress
    m_progress = static_cast<double>(i + 1) / totalPoints;

    // Notify UI callback if provided
    if (m_updateCallback) {
      m_updateCallback(target, scanData.value);
    }

    LogScanInfo("Point " + std::to_string(i + 1) + " complete. Value: " +
      std::to_string(scanData.value));
  }

  // Return to ORIGINAL position (position before scan started)
  if (!m_stopRequested) {
    LogScanInfo("Returning to original position: X=" + std::to_string(m_originalX) +
      "mm, Y=" + std::to_string(m_originalY) + "mm");

    bool returnSuccess = m_motionController->MoveToXY(m_originalX, m_originalY, false);
    if (returnSuccess) {
      WaitForMove(m_moveTimeoutSec * 1000);
      LogScanInfo("Successfully returned to original position");
    }
    else {
      Logger::GetInstance()->LogError("GridScanner: Failed to return to original position");
    }
  }

  // Set final status
  m_scanning = false;
  m_progress = 1.0;
  m_stopRequested = false;

  // Log final results
  if (m_peakValue > -std::numeric_limits<double>::infinity()) {
    LogScanInfo("Scan completed. Collected " + std::to_string(m_scanData.size()) +
      " points. Peak value: " + std::to_string(m_peakValue) +
      " at position X:" + std::to_string(m_peakPosition.x) +
      "mm, Y:" + std::to_string(m_peakPosition.y) + "mm");
  }
  else {
    LogScanInfo("Scan completed. Collected " + std::to_string(m_scanData.size()) +
      " points. No valid peak found.");
  }
}

bool GridScanner::WaitForMove(int timeoutMs) {
  auto startTime = std::chrono::steady_clock::now();

  // For very small movements, use position-based detection
  double startX, startY;
  m_motionController->GetCurrentXY(startX, startY);

  // Calculate expected distance
  double expectedDistance = std::sqrt(
    std::pow(m_currentTarget.x - startX, 2) +
    std::pow(m_currentTarget.y - startY, 2)
  );

  // If movement is very small, use position tolerance instead of IsMoving
  const double POSITION_TOLERANCE = 0.0001; // 0.1 µm tolerance
  bool usePositionCheck = expectedDistance < 0.001; // Less than 1 µm

  int checkCount = 0;
  while (true) {
    checkCount++;

    if (usePositionCheck) {
      // For small moves, check if we reached target position
      double currentX, currentY;
      if (m_motionController->GetCurrentXY(currentX, currentY)) {
        double distanceToTarget = std::sqrt(
          std::pow(m_currentTarget.x - currentX, 2) +
          std::pow(m_currentTarget.y - currentY, 2)
        );

        if (distanceToTarget < POSITION_TOLERANCE) {
          // We're close enough to target
          return true;
        }
      }

      // Also check IsMoving as backup
      if (!m_motionController->IsMoving() && checkCount > 5) {
        // Motion controller says we're not moving after a few checks
        return true;
      }
    }
    else {
      // For larger moves, use normal IsMoving check
      if (!m_motionController->IsMoving()) {
        return true;
      }
    }

    // Check for stop request
    if (m_stopRequested) {
      m_motionController->StopMotion();
      return false;
    }

    // Check for timeout
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - startTime).count();
    if (elapsed > timeoutMs) {
      // For very small moves, might be already there
      if (usePositionCheck) {
        Logger::GetInstance()->LogWarning("GridScanner: Timeout on small move, assuming complete");
        return true; // Assume we made it for tiny moves
      }

      Logger::GetInstance()->LogError("GridScanner: Move timeout after " +
        std::to_string(elapsed) + " ms");
      m_motionController->StopMotion();
      return false;
    }

    // Small delay to prevent busy waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

bool GridScanner::IsDataChannelConnected() const {
  // First check if channel exists in GlobalDataStore
  GlobalDataStore* dataStore = GlobalDataStore::GetInstance();
  if (dataStore) {
    auto channels = dataStore->GetAvailableChannels();
    bool channelExists = false;

    for (const auto& channel : channels) {
      // Check both exact match and parentheses format
      if (channel == m_dataChannel || channel == "(" + m_dataChannel + ")") {
        channelExists = true;
        break;
      }
    }

    if (!channelExists) {
      return false;
    }
  }

  // Then check if we're receiving data recently
  std::lock_guard<std::mutex> lock(m_dataMutex);
  auto now = std::chrono::steady_clock::now();
  auto timeSinceData = std::chrono::duration_cast<std::chrono::seconds>(
    now - m_lastDataTime).count();
  return m_dataChannelActive && timeSinceData < 5;
}

void GridScanner::SetMotionController(std::shared_ptr<IScanMotionController> controller) {
  // Unsubscribe from old controller if exists
  if (m_motionController) {
    if (auto* adapter = dynamic_cast<PIScanMotionAdapter*>(m_motionController.get())) {
      adapter->UnsubscribeScanner();
    }
  }

  m_motionController = controller;

  // Subscribe to new controller
  if (controller) {
    if (auto* adapter = dynamic_cast<PIScanMotionAdapter*>(controller.get())) {
      adapter->SubscribeScanner(this);
    }
    LogScanInfo("Motion controller set: " + controller->GetDeviceName());
  }
}

void GridScanner::SetDataChannel(const std::string& channel) {
  if (channel != m_dataChannel) {
    // Unsubscribe from old channel
    m_dataManager.Unsubscribe(m_dataChannel, this);

    // Subscribe to new channel
    m_dataChannel = channel;
    m_dataManager.Subscribe(m_dataChannel, this);
    LogScanInfo("Data channel changed to: " + channel);
  }
}

void GridScanner::OnPositionsUpdate(const std::string& deviceName,
  const std::map<std::string, double>& positions) {
  // Only process if it's our device
  if (m_motionController &&
    deviceName == m_motionController->GetDeviceName()) {

    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_currentPositions = positions;

    // Update origin if needed
    auto xIt = positions.find("X");
    auto yIt = positions.find("Y");
    if (xIt != positions.end() && yIt != positions.end()) {
      // These are always up-to-date
      // Could use for real-time tracking during scan
    }
  }
}

void GridScanner::OnMotionStatusChange(const std::string& deviceName,
  const std::string& axis,
  bool isMoving) {
  if (m_motionController &&
    deviceName == m_motionController->GetDeviceName()) {

    // Track when motion completes
    if (!isMoving && (axis == "X" || axis == "Y")) {
      // Check if both X and Y are idle
      // This helps WaitForMove complete faster
      m_motionComplete = true;
    }
  }
}