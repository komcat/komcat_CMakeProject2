// UIConfigVisualizerPositionSubscriber.cpp
#include "UIConfigVisualizerPositionSubscriber.h"
#include "include/motions/MotionTypes.h"
#include "include/data/global_data_store.h"
#include "include/logger.h"

UIConfigVisualizerPositionSubscriber::UIConfigVisualizerPositionSubscriber()
  : MotionDataSubscriber("UIConfigVisualizer") {
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("UIConfigVisualizerPositionSubscriber: Created");
}

UIConfigVisualizerPositionSubscriber::~UIConfigVisualizerPositionSubscriber() {
  m_logger->LogInfo("UIConfigVisualizerPositionSubscriber: Destroyed, total updates: " +
    std::to_string(m_totalUpdates.load()));
}

void UIConfigVisualizerPositionSubscriber::OnPositionsUpdate(
  const std::string& deviceName,
  const std::map<std::string, double>& positions) {

  if (!IsEnabled()) return;

  std::lock_guard<std::mutex> lock(m_dataMutex);

  // Update device data
  auto& data = m_deviceData[deviceName];
  data.positions = positions;
  data.lastUpdate = std::chrono::steady_clock::now();

  // Increment update counter
  m_totalUpdates.fetch_add(1);

  // Store in GlobalDataStore for other components
  for (const auto& [axis, value] : positions) {
    std::string key = deviceName + "-POS-" + axis;
    StoreValue(key, static_cast<float>(value));
  }

  // Debug logging (optional)
  if (m_enableDebugLogging) {
    m_logger->LogDebug("UIConfigVisualizer: Position update for " + deviceName +
      " (axes: " + std::to_string(positions.size()) + ")");
  }
}

void UIConfigVisualizerPositionSubscriber::OnMotionStatusChange(
  const std::string& deviceName,
  const std::string& axis,
  bool isMoving) {

  if (!IsEnabled()) return;

  std::lock_guard<std::mutex> lock(m_dataMutex);

  auto& data = m_deviceData[deviceName];

  // Update axis motion status
  if (axis.empty()) {
    // Overall motion status
    data.isMoving = isMoving;
  }
  else {
    // Specific axis motion status
    data.axisMoving[axis] = isMoving;

    // Update overall motion status (any axis moving means device is moving)
    data.isMoving = false;
    for (const auto& [ax, moving] : data.axisMoving) {
      if (moving) {
        data.isMoving = true;
        break;
      }
    }
  }

  data.lastUpdate = std::chrono::steady_clock::now();

  // Store motion status in GlobalDataStore
  std::string key = deviceName + "-MOVING";
  StoreValue(key, data.isMoving ? 1.0f : 0.0f);

  if (!axis.empty()) {
    std::string axisKey = deviceName + "-" + axis + "-MOVING";
    StoreValue(axisKey, isMoving ? 1.0f : 0.0f);
  }
}

std::map<std::string, PositionStruct> UIConfigVisualizerPositionSubscriber::GetAllPositions() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);

  std::map<std::string, PositionStruct> result;

  for (const auto& [deviceName, data] : m_deviceData) {
    PositionStruct pos;

    // Extract position values
    auto xIt = data.positions.find("X");
    auto yIt = data.positions.find("Y");
    auto zIt = data.positions.find("Z");
    auto uIt = data.positions.find("U");
    auto vIt = data.positions.find("V");
    auto wIt = data.positions.find("W");

    if (xIt != data.positions.end()) pos.x = xIt->second;
    if (yIt != data.positions.end()) pos.y = yIt->second;
    if (zIt != data.positions.end()) pos.z = zIt->second;
    if (uIt != data.positions.end()) pos.u = uIt->second;
    if (vIt != data.positions.end()) pos.v = vIt->second;
    if (wIt != data.positions.end()) pos.w = wIt->second;

    result[deviceName] = pos;
  }

  return result;
}

bool UIConfigVisualizerPositionSubscriber::GetDevicePosition(
  const std::string& deviceName,
  PositionStruct& position) const {

  std::lock_guard<std::mutex> lock(m_dataMutex);

  auto it = m_deviceData.find(deviceName);
  if (it == m_deviceData.end()) {
    return false;
  }

  const auto& data = it->second;

  // Clear position first
  position = PositionStruct();

  // Extract position values
  auto xIt = data.positions.find("X");
  auto yIt = data.positions.find("Y");
  auto zIt = data.positions.find("Z");
  auto uIt = data.positions.find("U");
  auto vIt = data.positions.find("V");
  auto wIt = data.positions.find("W");

  if (xIt != data.positions.end()) position.x = xIt->second;
  if (yIt != data.positions.end()) position.y = yIt->second;
  if (zIt != data.positions.end()) position.z = zIt->second;
  if (uIt != data.positions.end()) position.u = uIt->second;
  if (vIt != data.positions.end()) position.v = vIt->second;
  if (wIt != data.positions.end()) position.w = wIt->second;

  return true;
}

bool UIConfigVisualizerPositionSubscriber::IsDeviceMoving(const std::string& deviceName) const {
  std::lock_guard<std::mutex> lock(m_dataMutex);

  auto it = m_deviceData.find(deviceName);
  if (it == m_deviceData.end()) {
    return false;
  }

  return it->second.isMoving;
}

bool UIConfigVisualizerPositionSubscriber::IsAxisMoving(
  const std::string& deviceName,
  const std::string& axis) const {

  std::lock_guard<std::mutex> lock(m_dataMutex);

  auto devIt = m_deviceData.find(deviceName);
  if (devIt == m_deviceData.end()) {
    return false;
  }

  auto axIt = devIt->second.axisMoving.find(axis);
  if (axIt == devIt->second.axisMoving.end()) {
    return false;
  }

  return axIt->second;
}

std::chrono::milliseconds UIConfigVisualizerPositionSubscriber::GetTimeSinceUpdate(
  const std::string& deviceName) const {

  std::lock_guard<std::mutex> lock(m_dataMutex);

  auto it = m_deviceData.find(deviceName);
  if (it == m_deviceData.end()) {
    return std::chrono::milliseconds::max();
  }

  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    now - it->second.lastUpdate);
}

std::vector<std::string> UIConfigVisualizerPositionSubscriber::GetTrackedDevices() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);

  std::vector<std::string> devices;
  devices.reserve(m_deviceData.size());

  for (const auto& [deviceName, _] : m_deviceData) {
    devices.push_back(deviceName);
  }

  return devices;
}

void UIConfigVisualizerPositionSubscriber::ClearDevice(const std::string& deviceName) {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  m_deviceData.erase(deviceName);
}

void UIConfigVisualizerPositionSubscriber::ClearAllDevices() {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  m_deviceData.clear();
  m_totalUpdates.store(0);
}

uint64_t UIConfigVisualizerPositionSubscriber::GetTotalUpdates() const {
  return m_totalUpdates.load();
}

void UIConfigVisualizerPositionSubscriber::SetDebugLogging(bool enable) {
  m_enableDebugLogging = enable;
}

bool UIConfigVisualizerPositionSubscriber::HasPositionData(const std::string& deviceName) const {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  return m_deviceData.find(deviceName) != m_deviceData.end();
}

size_t UIConfigVisualizerPositionSubscriber::GetDeviceCount() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  return m_deviceData.size();
}