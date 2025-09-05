// include/scanning/grid_volume_scanner.cpp
#include "grid_volume_scanner.h"
#include "grid_scanner.h"
#include "include/logger.h"
#include "PIScanMotionAdapter.h"
#include <algorithm>
#include <numeric>
#include <filesystem>

GridVolumeScanner::GridVolumeScanner(std::shared_ptr<IScanMotionController> motionController,
  DataClientManager& dataManager,
  const std::string& dataChannel)
  : GridScanner(motionController, dataManager, dataChannel) {

  // Create scan_json directory if it doesn't exist
  std::filesystem::create_directories("scan_json");

  LogScanInfo("GridVolumeScanner initialized - output directory: scan_json/");
}

// Add destructor to ensure thread cleanup
GridVolumeScanner::~GridVolumeScanner() {
  StopVolumeScan();
  if (m_volumeScanThread.joinable()) {
    m_volumeScanThread.join();
  }
}

void GridVolumeScanner::SetZScanParameters(double zStart, double zEnd, int zLayers) {
  m_zStart = zStart;
  m_zEnd = zEnd;
  m_zLayers = (std::max)(1, zLayers);

  LogScanInfo("Z scan parameters set: " + std::to_string(m_zLayers) +
    " layers from Z=" + std::to_string(m_zStart) +
    " to Z=" + std::to_string(m_zEnd));
}

bool GridVolumeScanner::StartVolumeScan(std::function<void(int layer, int total, double z)> progressCallback) {
  if (m_volumeScanActive) {
    LogScanInfo("Volume scan already in progress");
    return false;
  }

  if (!m_motionController || !m_motionController->IsConnected()) {
    Logger::GetInstance()->LogError("GridVolumeScanner: Motion controller not ready");
    return false;
  }

  // Clean up previous thread if exists
  if (m_volumeScanThread.joinable()) {
    m_volumeScanThread.join();
  }

  m_volumeScanActive = true;
  m_stopVolumeRequested = false;

  // Start scan in separate thread
  m_volumeScanThread = std::thread([this, progressCallback]() {
    // Initialize volume data
    m_volumeData.data.clear();
    m_volumeData.zPositions.clear();
    m_volumeData.scanId = GenerateScanId();
    m_volumeData.timestamp = GetCurrentTimestamp();
    m_volumeData.peakValue = -std::numeric_limits<double>::infinity();

    auto startTime = std::chrono::steady_clock::now();

    LogScanInfo("Starting adaptive 3D volume scan: " + std::to_string(m_zLayers) +
      " layers, " + std::to_string(m_xPoints) + "x" +
      std::to_string(m_yPoints) + " grid per layer (peak-following mode)");

    // Store original position for final return
    double originalX = 0.0;
    double originalY = 0.0;
    double originalZ = 0.0;

    // Get current XY position (required)
    if (!m_motionController->GetCurrentXY(originalX, originalY)) {
      Logger::GetInstance()->LogError("Failed to get original XY position");
      m_volumeScanActive = false;
      return;
    }

    LogScanInfo("Original position: X=" + std::to_string(originalX) +
      ", Y=" + std::to_string(originalY));

    // Track current position as we follow peaks
    double currentX = originalX;
    double currentY = originalY;
    double currentZ = m_zStart;  // Start at first Z position

    // Scan each Z layer with adaptive peak following
    for (int layer = 0; layer < m_zLayers && !m_stopVolumeRequested; ++layer) {
      LogScanInfo("=== Layer " + std::to_string(layer + 1) + "/" +
        std::to_string(m_zLayers) + " ===");

      // Verify current Z position before scanning
      double actualZ = currentZ;
      try {
        if (auto piAdapter = dynamic_cast<PIScanMotionAdapter*>(m_motionController.get())) {
          auto* controller = piAdapter->GetController();
          if (controller) {
            std::map<std::string, double> positions;
            if (controller->GetPositions(positions)) {
              auto zIt = positions.find("Z");
              if (zIt != positions.end()) {
                actualZ = zIt->second;
                LogScanInfo("Current Z position confirmed: " + std::to_string(actualZ));
              }
            }
          }
        }
      }
      catch (...) {
        LogScanInfo("Z position verification failed, using assumed Z=" + std::to_string(currentZ));
      }

      currentZ = actualZ; // Use verified position
      LogScanInfo("Scanning at confirmed position: X=" + std::to_string(currentX) +
        ", Y=" + std::to_string(currentY) + ", Z=" + std::to_string(currentZ));

      // Ensure we're at the current position before scanning
      if (!m_motionController->MoveToXY(currentX, currentY, true)) {
        Logger::GetInstance()->LogError("Failed to move to scan position");
        continue;
      }

      // Add settling time for movement
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      // Notify progress callback
      if (progressCallback) {
        progressCallback(layer, m_zLayers, currentZ);
      }

      // Scan this layer at current position
      auto layerData = ScanLayer(currentZ);

      // Store layer data
      m_volumeData.data.push_back(layerData);
      m_volumeData.zPositions.push_back(currentZ);

      // Save individual layer
      SaveLayerData(layer, currentZ, layerData);

      // Find peak in this layer
      double layerPeakValue = -std::numeric_limits<double>::infinity();
      int peakRow = -1, peakCol = -1;

      for (int row = 0; row < m_yPoints; ++row) {
        for (int col = 0; col < m_xPoints; ++col) {
          if (layerData[row][col] > layerPeakValue) {
            layerPeakValue = layerData[row][col];
            peakRow = row;
            peakCol = col;
          }
        }
      }

      // Calculate physical peak position for this layer
      if (peakRow >= 0 && peakCol >= 0) {
        // Calculate peak position relative to current scan center
        double peakXOffset = ((peakCol - (m_xPoints - 1) / 2.0) * m_xStep) / 1000.0; // Convert µm to mm
        double peakYOffset = ((peakRow - (m_yPoints - 1) / 2.0) * m_yStep) / 1000.0; // Convert µm to mm

        double layerPeakX = currentX + peakXOffset;
        double layerPeakY = currentY - peakYOffset; // Y is inverted in grid coordinates

        LogScanInfo("Layer peak found: value=" + std::to_string(layerPeakValue) +
          " at grid[" + std::to_string(peakCol) + "," + std::to_string(peakRow) + "]" +
          " physical(" + std::to_string(layerPeakX) + "," + std::to_string(layerPeakY) + ")");

        // Move to this layer's peak position
        if (!m_motionController->MoveToXY(layerPeakX, layerPeakY, true)) {
          Logger::GetInstance()->LogError("Failed to move to layer peak position");
        }
        else {
          LogScanInfo("Moved to layer peak position");
          // Update current position
          currentX = layerPeakX;
          currentY = layerPeakY;
        }

        // Update global peak if this is the best so far
        if (layerPeakValue > m_volumeData.peakValue) {
          m_volumeData.peakValue = layerPeakValue;
          m_volumeData.peakPosition = VolumeGridPoint(layerPeakX, layerPeakY, currentZ, peakRow, peakCol, layer);
          LogScanInfo("New global peak: " + std::to_string(layerPeakValue));
        }
      }
      else {
        Logger::GetInstance()->LogWarning("No valid peak found in layer " + std::to_string(layer));
      }

      // Move to next Z position (except for last layer)
      if (layer < m_zLayers - 1) {
        // Calculate the Z step size from UI parameters
        double zStepSize = (m_zEnd - m_zStart) / (std::max)(1, m_zLayers - 1);

        LogScanInfo("=== VOLUME SCAN Z DEBUG ===");
        LogScanInfo("m_zStart=" + std::to_string(m_zStart) + " mm, m_zEnd=" + std::to_string(m_zEnd) + " mm, m_zLayers=" + std::to_string(m_zLayers));
        LogScanInfo("Calculated zStepSize=" + std::to_string(zStepSize) + " mm (" + std::to_string(zStepSize * 1000.0) + " µm)");
        LogScanInfo("Current controller Z position: " + std::to_string(currentZ) + " mm");

        // Move relative by the step size (not absolute position)
        double zRelativeMove = zStepSize;  // Just the step size, not absolute calculation

        LogScanInfo("DEBUG: Commanding RELATIVE Z move: " + std::to_string(zRelativeMove) + " mm (" +
          std::to_string(zRelativeMove * 1000.0) + " µm)");

        // Try to move Z axis - PI controller uses mm units
        bool zMoveSuccess = false;
        try {
          // Check if we have a PI controller that supports Z movement
          if (auto piAdapter = dynamic_cast<PIScanMotionAdapter*>(m_motionController.get())) {
            auto* controller = piAdapter->GetController();
            if (controller) {

              // Use relative move with step size
              LogScanInfo("Executing relative Z move by step size...");
              zMoveSuccess = controller->MoveRelative("Z", zRelativeMove, true);

              LogScanInfo("DEBUG: Z movement command result: " + std::string(zMoveSuccess ? "SUCCESS" : "FAILED"));

              if (zMoveSuccess) {
                // Update current Z by adding the step
                currentZ += zRelativeMove;
                LogScanInfo("✓ Z movement completed, new expected position: " + std::to_string(currentZ) + " mm");
              }
              else {
                Logger::GetInstance()->LogError("✗ Z movement failed - check Z axis configuration");
              }

              // Always verify actual position after movement
              std::map<std::string, double> positions;
              if (controller->GetPositions(positions)) {
                auto zIt = positions.find("Z");
                if (zIt != positions.end()) {
                  double actualZ = zIt->second;
                  LogScanInfo("DEBUG: Controller reports actual Z position: " + std::to_string(actualZ) + " mm");
                  LogScanInfo("DEBUG: Movement difference: expected=" + std::to_string(currentZ) +
                    ", actual=" + std::to_string(actualZ) +
                    ", error=" + std::to_string(actualZ - currentZ) + " mm");
                  currentZ = actualZ;  // Use actual position
                }
                else {
                  LogScanInfo("DEBUG: Z axis not found in position report");
                }
              }
            }
          }
        }
        catch (const std::exception& e) {
          Logger::GetInstance()->LogError("Z movement exception: " + std::string(e.what()));
          zMoveSuccess = false;
        }
        catch (...) {
          Logger::GetInstance()->LogError("Unknown Z movement exception");
          zMoveSuccess = false;
        }

        if (!zMoveSuccess) {
          Logger::GetInstance()->LogWarning("Z movement not available - continuing with Z=" + std::to_string(currentZ));
        }
      }

      LogScanInfo("Layer " + std::to_string(layer + 1) + " complete. Current position: (" +
        std::to_string(currentX) + ", " + std::to_string(currentY) + ", " + std::to_string(currentZ) + ")");
    }

    // Calculate total scan duration
    auto endTime = std::chrono::steady_clock::now();
    m_volumeData.scanDuration = std::chrono::duration<double>(endTime - startTime).count();

    // Save complete volume data
    SaveVolumeData();

    // Return to original position
    if (!m_stopVolumeRequested) {
      LogScanInfo("Returning to original position: X=" + std::to_string(originalX) +
        ", Y=" + std::to_string(originalY));

      // Move back to original XY position
      m_motionController->MoveToXY(originalX, originalY, true);
    }

    m_volumeScanActive = false;

    LogScanInfo("Adaptive volume scan completed!");
    LogScanInfo("Global peak: " + std::to_string(m_volumeData.peakValue) +
      " at final position (" + std::to_string(m_volumeData.peakPosition.x) + ", " +
      std::to_string(m_volumeData.peakPosition.y) + ", " +
      std::to_string(m_volumeData.peakPosition.z) + ")");
    LogScanInfo("Peak trace saved in volume data for alignment analysis");
  });

  return true;  // Return immediately, scan runs in background
}

void GridVolumeScanner::StopVolumeScan() {
  if (m_volumeScanActive) {
    LogScanInfo("Stopping volume scan");
    m_stopVolumeRequested = true;
    StopScan(); // Stop current layer scan if active

    // Wait for thread to finish
    if (m_volumeScanThread.joinable()) {
      m_volumeScanThread.join();
    }

    m_volumeScanActive = false;
  }
}

std::vector<std::vector<double>> GridVolumeScanner::ScanLayer(double z) {
  // Get current grid parameters from parent class
  int currentXPoints = GetXPoints();
  int currentYPoints = GetYPoints();

  // Create layer data with correct dimensions
  std::vector<std::vector<double>> layerData(currentYPoints,
    std::vector<double>(currentXPoints, 0.0));

  std::atomic<bool> scanComplete{ false };
  std::mutex layerDataMutex;
  std::atomic<int> pointsCollected{ 0 };
  int totalExpectedPoints = currentXPoints * currentYPoints;

  // Start grid scan with callback
  StartScan([&](const GridPoint& point, double value) {
    std::lock_guard<std::mutex> lock(layerDataMutex);

    // Bounds checking to prevent vector subscript out of range
    if (point.row >= 0 && point.row < layerData.size() &&
      point.col >= 0 && point.col < layerData[point.row].size()) {
      layerData[point.row][point.col] = value;
      pointsCollected++;

      // Check if we've collected all points
      if (pointsCollected >= totalExpectedPoints) {
        scanComplete = true;
      }
    }
    else {
      Logger::GetInstance()->LogError("GridVolumeScanner: Point out of bounds - row:" +
        std::to_string(point.row) + " col:" +
        std::to_string(point.col) +
        " (grid size: " + std::to_string(layerData.size()) +
        "x" + std::to_string(currentXPoints) + ")");
    }
  });

  // Wait for scan to complete with timeout
  auto startWait = std::chrono::steady_clock::now();
  while (!scanComplete && !m_stopVolumeRequested) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Add timeout check
    auto elapsed = std::chrono::steady_clock::now() - startWait;
    if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() > 300) { // 5 min timeout
      Logger::GetInstance()->LogError("Layer scan timeout - collected " +
        std::to_string(pointsCollected) + "/" +
        std::to_string(totalExpectedPoints) + " points");
      break;
    }
  }

  // Wait for any remaining points to be processed
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  LogScanInfo("Layer scan complete - collected " + std::to_string(pointsCollected) +
    "/" + std::to_string(totalExpectedPoints) + " points");

  return layerData;
}

void GridVolumeScanner::SaveLayerData(int layerIndex, double z,
  const std::vector<std::vector<double>>& data) {
  nlohmann::json layerJson;

  // Metadata
  layerJson["metadata"] = {
      {"scan_id", m_volumeData.scanId},
      {"layer_index", layerIndex},
      {"z_position", z},
      {"timestamp", m_volumeData.timestamp},
      {"device", m_motionController ? m_motionController->GetDeviceName() : "Unknown"},
      {"data_channel", m_dataChannel},
      {"grid_size", {{"x", m_xPoints}, {"y", m_yPoints}}},
      {"step_size", {{"x", m_xStep}, {"y", m_yStep}}},
      {"origin", {{"x", m_originX}, {"y", m_originY}, {"z", z}}}
  };

  // Data points with full XYZ coordinates
  nlohmann::json points = nlohmann::json::array();
  for (int row = 0; row < m_yPoints; ++row) {
    for (int col = 0; col < m_xPoints; ++col) {
      double x = m_originX - ((m_xPoints - 1) * m_xStep / 2.0) / 1000.0 + (col * m_xStep / 1000.0);
      double y = m_originY + ((m_yPoints - 1) * m_yStep / 2.0) / 1000.0 - (row * m_yStep / 1000.0);

      points.push_back({
          {"grid_position", {{"row", row}, {"col", col}}},
          {"physical_position", {{"x", x}, {"y", y}, {"z", z}}},
          {"value", data[row][col]}
        });
    }
  }
  layerJson["points"] = points;

  // Statistics for this layer
  auto [minVal, maxVal, avgVal] = CalculateStats(data);
  layerJson["statistics"] = {
      {"min", minVal},
      {"max", maxVal},
      {"average", avgVal},
      {"peak", FindLayerPeak(data, z)}
  };

  // Save to scan_json folder
  std::string filename = "scan_json/scan_" + m_volumeData.scanId + "_layer_" +
    std::to_string(layerIndex) + ".json";
  std::ofstream file(filename);
  file << layerJson.dump(2);
  file.close();

  LogScanInfo("Saved layer " + std::to_string(layerIndex) + " to " + filename);
}

void GridVolumeScanner::SaveVolumeData() {
  nlohmann::json volumeJson;

  // Metadata
  volumeJson["metadata"] = {
      {"scan_id", m_volumeData.scanId},
      {"timestamp", m_volumeData.timestamp},
      {"scan_type", "3D_volume"},
      {"device", m_motionController ? m_motionController->GetDeviceName() : "Unknown"},
      {"data_channel", m_dataChannel},
      {"scan_duration_seconds", m_volumeData.scanDuration}
  };

  // Volume parameters
  double xRange = (m_xPoints - 1) * m_xStep / 1000.0;
  double yRange = (m_yPoints - 1) * m_yStep / 1000.0;

  volumeJson["volume_params"] = {
      {"grid_size", {
          {"x", m_xPoints},
          {"y", m_yPoints},
          {"z", m_zLayers}
      }},
      {"step_size", {
          {"x", m_xStep},
          {"y", m_yStep},
          {"z", m_zLayers > 1 ? (m_zEnd - m_zStart) / (m_zLayers - 1) : 0}
      }},
      {"step_units", {
          {"xy", "micrometers"},
          {"z", "millimeters"}
      }},
      {"bounds", {
          {"x_range", {m_originX - xRange / 2, m_originX + xRange / 2}},
          {"y_range", {m_originY - yRange / 2, m_originY + yRange / 2}},
          {"z_range", {m_zStart, m_zEnd}},
          {"units", "millimeters"}
      }}
  };

  // Compact volume data format
  nlohmann::json dataArray = nlohmann::json::array();
  for (size_t layer = 0; layer < m_volumeData.data.size(); ++layer) {
    nlohmann::json layerData;
    layerData["z"] = m_volumeData.zPositions[layer];
    layerData["values"] = m_volumeData.data[layer];
    dataArray.push_back(layerData);
  }
  volumeJson["volume_data"] = dataArray;

  // Global statistics
  volumeJson["statistics"] = {
      {"total_points", m_xPoints * m_yPoints * m_zLayers},
      {"global_peak", {
          {"value", m_volumeData.peakValue},
          {"position", {
              {"x", m_volumeData.peakPosition.x},
              {"y", m_volumeData.peakPosition.y},
              {"z", m_volumeData.peakPosition.z}
          }},
          {"grid_index", {
              {"col", m_volumeData.peakPosition.col},
              {"row", m_volumeData.peakPosition.row},
              {"layer", m_volumeData.peakPosition.layer}
          }}
      }}
  };

  // Layer file references (update paths to include scan_json/)
  nlohmann::json layers = nlohmann::json::array();
  for (int i = 0; i < m_zLayers; ++i) {
    layers.push_back({
        {"index", i},
        {"z_position", m_volumeData.zPositions[i]},
        {"filename", "scan_json/scan_" + m_volumeData.scanId + "_layer_" + std::to_string(i) + ".json"}
      });
  }
  volumeJson["layer_files"] = layers;

  // Save to scan_json folder
  std::string filename = "scan_json/scan_" + m_volumeData.scanId + "_volume.json";
  std::ofstream file(filename);
  file << volumeJson.dump(2);
  file.close();

  LogScanInfo("Saved volume data to " + filename);
}

std::string GridVolumeScanner::GenerateScanId() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  std::tm timeinfo;
  localtime_s(&timeinfo, &time_t);
  ss << std::put_time(&timeinfo, "%Y%m%d_%H%M%S");

  // Add milliseconds for uniqueness
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    now.time_since_epoch()) % 1000;
  ss << "_" << std::setfill('0') << std::setw(3) << ms.count();

  return ss.str();
}

double GridVolumeScanner::GetCurrentTimestamp() const {
  return std::chrono::duration<double>(
    std::chrono::system_clock::now().time_since_epoch()).count();
}

std::tuple<double, double, double> GridVolumeScanner::CalculateStats(
  const std::vector<std::vector<double>>& data) const {

  double minVal = (std::numeric_limits<double>::max)();
  double maxVal = std::numeric_limits<double>::lowest();
  double sum = 0.0;
  int count = 0;

  for (const auto& row : data) {
    for (double val : row) {
      if (!std::isnan(val) && !std::isinf(val)) {
        minVal = (std::min)(minVal, val);
        maxVal = (std::max)(maxVal, val);
        sum += val;
        count++;
      }
    }
  }

  double avgVal = count > 0 ? sum / count : 0.0;
  return std::make_tuple(minVal, maxVal, avgVal);
}

nlohmann::json GridVolumeScanner::FindLayerPeak(
  const std::vector<std::vector<double>>& data, double z) const {

  double peakValue = -std::numeric_limits<double>::infinity();
  int peakRow = -1, peakCol = -1;

  for (int row = 0; row < m_yPoints; ++row) {
    for (int col = 0; col < m_xPoints; ++col) {
      if (data[row][col] > peakValue) {
        peakValue = data[row][col];
        peakRow = row;
        peakCol = col;
      }
    }
  }

  if (peakRow >= 0 && peakCol >= 0) {
    double x = m_originX - ((m_xPoints - 1) * m_xStep / 2.0) / 1000.0 + (peakCol * m_xStep / 1000.0);
    double y = m_originY + ((m_yPoints - 1) * m_yStep / 2.0) / 1000.0 - (peakRow * m_yStep / 1000.0);

    return {
        {"value", peakValue},
        {"position", {{"x", x}, {"y", y}, {"z", z}}},
        {"grid_index", {{"row", peakRow}, {"col", peakCol}}}
    };
  }

  return nlohmann::json::object();
}

VolumeGridPoint GridVolumeScanner::Find3DPeak() const {
  // Already found during scanning
  return m_volumeData.peakPosition;
}