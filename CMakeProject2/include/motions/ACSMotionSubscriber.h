// include/motions/ACSMotionSubscriber.h
#pragma once
#include "MotionDataSubscriber.h"
#include <iostream>

class ACSMotionSubscriber : public MotionDataSubscriber {
private:
  // ACS-specific tracking
  std::map<std::string, double> m_homePositions;
  std::map<std::string, double> m_softLimits;
  bool m_trackRelativePosition = false;

public:
  ACSMotionSubscriber() : MotionDataSubscriber("ACS") {
    std::cout << "[ACSMotionSubscriber] Created ACS gantry position subscriber" << std::endl;
  }

  void OnPositionsUpdate(const std::string& deviceName,
    const std::map<std::string, double>& positions) override {
    if (!m_enabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& [axis, position] : positions) {
      // Store absolute position
      std::string posKey = deviceName + "-POS-" + axis;
      StoreValue(posKey, static_cast<float>(position));

      // ACS specific: Store relative position from home
      if (m_trackRelativePosition) {
        auto homeIt = m_homePositions.find(deviceName + "-" + axis);
        if (homeIt != m_homePositions.end()) {
          float relativePos = static_cast<float>(position - homeIt->second);
          std::string relKey = deviceName + "-REL-POS-" + axis;
          StoreValue(relKey, relativePos);
        }
      }

      // ACS specific: Check soft limits
      auto limitKey = deviceName + "-" + axis;
      auto limitIt = m_softLimits.find(limitKey);
      if (limitIt != m_softLimits.end()) {
        bool nearLimit = std::abs(position) > (limitIt->second * 0.9); // 90% of limit
        std::string limitWarningKey = deviceName + "-NEAR-LIMIT-" + axis;
        StoreValue(limitWarningKey, nearLimit ? 1.0f : 0.0f);
      }
    }
  }

  void OnMotionStatusChange(const std::string& deviceName,
    const std::string& axis,
    bool isMoving) override {
    if (!m_enabled) return;

    std::string key = deviceName + "-MOVING-" + axis;
    StoreValue(key, isMoving ? 1.0f : 0.0f);
  }

  // ACS-specific configuration
  void SetHomePosition(const std::string& deviceName, const std::string& axis, double position) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_homePositions[deviceName + "-" + axis] = position;
    m_trackRelativePosition = true;
  }

  void SetSoftLimit(const std::string& deviceName, const std::string& axis, double limit) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_softLimits[deviceName + "-" + axis] = limit;
  }
};