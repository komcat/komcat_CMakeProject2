// include/scanning/grid_volume_scanner.h
#pragma once

#include "grid_scanner.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

struct VolumeGridPoint {
  double x, y, z;
  int row, col, layer;

  VolumeGridPoint() : x(0), y(0), z(0), row(0), col(0), layer(0) {}
  VolumeGridPoint(double _x, double _y, double _z, int _row, int _col, int _layer)
    : x(_x), y(_y), z(_z), row(_row), col(_col), layer(_layer) {
  }
};

struct VolumeScanData {
  std::vector<std::vector<std::vector<double>>> data;  // [layer][row][col]
  std::vector<double> zPositions;                      // Z position for each layer
  VolumeGridPoint peakPosition;
  double peakValue;
  std::string scanId;
  double timestamp;
  double scanDuration;
};

class GridVolumeScanner : public GridScanner {
public:
  GridVolumeScanner(std::shared_ptr<IScanMotionController> motionController,
    DataClientManager& dataManager,
    const std::string& dataChannel);

  ~GridVolumeScanner();

  // Set Z scan parameters
  void SetZScanParameters(double zStart, double zEnd, int zLayers);

  // Main volume scan function with progress callback
  bool StartVolumeScan(std::function<void(int layer, int total, double z)> progressCallback = nullptr);

  // Stop volume scan
  void StopVolumeScan();

  // Get volume data
  const VolumeScanData& GetVolumeData() const { return m_volumeData; }

  // Check if volume scan is active
  bool IsVolumeScanActive() const { return m_volumeScanActive; }

private:
  // Z scanning parameters
  double m_zStart = -1.0;
  double m_zEnd = 1.0;
  int m_zLayers = 5;

  // Volume data
  VolumeScanData m_volumeData;
  std::thread m_volumeScanThread;
  std::atomic<bool> m_volumeScanActive{ false };
  std::atomic<bool> m_stopVolumeRequested{ false };


  // Save functions
  void SaveLayerData(int layerIndex, double z, const std::vector<std::vector<double>>& data);
  void SaveVolumeData();

  // Helper functions
  std::string GenerateScanId() const;
  double GetCurrentTimestamp() const;
  std::vector<std::vector<double>> ScanLayer(double z);
  VolumeGridPoint Find3DPeak() const;
  std::tuple<double, double, double> CalculateStats(const std::vector<std::vector<double>>& data) const;
  nlohmann::json FindLayerPeak(const std::vector<std::vector<double>>& data, double z) const;


};