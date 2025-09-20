// UIConfigVisualizerPositionSubscriber.h
#pragma once
#include "include/motions/IPositionSubscriber.h"
#include "include/motions/MotionDataSubscriber.h"
#include <map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <vector>

class Logger;
struct PositionStruct;

class UIConfigVisualizerPositionSubscriber : public MotionDataSubscriber {
public:
  UIConfigVisualizerPositionSubscriber();
  ~UIConfigVisualizerPositionSubscriber();

  // IPositionSubscriber implementation
  void OnPositionsUpdate(const std::string& deviceName,
    const std::map<std::string, double>& positions) override;
  void OnMotionStatusChange(const std::string& deviceName,
    const std::string& axis,
    bool isMoving) override;

  // Position access methods
  std::map<std::string, PositionStruct> GetAllPositions() const;
  bool GetDevicePosition(const std::string& deviceName, PositionStruct& position) const;

  // Motion status methods
  bool IsDeviceMoving(const std::string& deviceName) const;
  bool IsAxisMoving(const std::string& deviceName, const std::string& axis) const;

  // Timing methods
  std::chrono::milliseconds GetTimeSinceUpdate(const std::string& deviceName) const;

  // Device management
  std::vector<std::string> GetTrackedDevices() const;
  void ClearDevice(const std::string& deviceName);
  void ClearAllDevices();

  // Statistics
  uint64_t GetTotalUpdates() const;
  size_t GetDeviceCount() const;
  bool HasPositionData(const std::string& deviceName) const;

  // Debug control
  void SetDebugLogging(bool enable);

private:
  // Device data structure
  struct DeviceData {
    std::map<std::string, double> positions;
    std::map<std::string, bool> axisMoving;
    bool isMoving = false;
    std::chrono::steady_clock::time_point lastUpdate;
  };

  // Data storage
  std::map<std::string, DeviceData> m_deviceData;
  mutable std::mutex m_dataMutex;

  // Statistics
  std::atomic<uint64_t> m_totalUpdates{ 0 };

  // Debug control
  bool m_enableDebugLogging = false;

  // Logger
  Logger* m_logger = nullptr;
};