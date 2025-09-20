// include/motions/PIMotionSubscriber.h
#pragma once
#include "MotionDataSubscriber.h"
#include <chrono>
#include <iostream>
#include <array>      // For std::array
#include <cmath>      // For std::sqrt and math constants

// Define M_PI if not already defined (common on Windows)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class PIMotionSubscriber : public MotionDataSubscriber {
private:
  // PI-specific data
  std::map<std::string, double> m_lastPositions;
  std::map<std::string, std::chrono::steady_clock::time_point> m_lastUpdateTime;
  std::map<std::string, double> m_velocities;  // Calculated velocities

  // Configuration
  bool m_calculateVelocity = true;
  bool m_storeRawPositions = true;
  bool m_storeProcessedData = true;
  int m_decimationFactor = 1;  // Store every Nth update
  int m_updateCounter = 0;

public:
  PIMotionSubscriber() : MotionDataSubscriber("PI") {
    std::cout << "[PIMotionSubscriber] Created PI hexapod position subscriber" << std::endl;
  }

  // IPositionSubscriber implementation
  void OnPositionsUpdate(const std::string& deviceName,
    const std::map<std::string, double>& positions) override {
    if (!m_enabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Decimation - only process every Nth update
    m_updateCounter++;
    if (m_updateCounter % m_decimationFactor != 0) {
      return;
    }

    auto now = std::chrono::steady_clock::now();

    // Process each axis
    for (const auto& [axis, position] : positions) {
      // Store raw position
      if (m_storeRawPositions) {
        std::string posKey = deviceName + "-POS-" + axis;
        StoreValue(posKey, static_cast<float>(position));
      }

      // Calculate velocity if we have previous position
      if (m_calculateVelocity) {
        auto lastPosIt = m_lastPositions.find(deviceName + "-" + axis);
        auto lastTimeIt = m_lastUpdateTime.find(deviceName + "-" + axis);

        if (lastPosIt != m_lastPositions.end() &&
          lastTimeIt != m_lastUpdateTime.end()) {

          double deltaPos = position - lastPosIt->second;
          auto deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>
            (now - lastTimeIt->second).count();

          if (deltaTime > 0) {
            double velocity = (deltaPos / deltaTime) * 1000.0; // mm/s
            m_velocities[deviceName + "-" + axis] = velocity;

            std::string velKey = deviceName + "-VEL-" + axis;
            StoreValue(velKey, static_cast<float>(velocity));
          }
        }

        // Update last position and time
        m_lastPositions[deviceName + "-" + axis] = position;
        m_lastUpdateTime[deviceName + "-" + axis] = now;
      }

      // PI Hexapod specific: Calculate and store angular positions
      if (m_storeProcessedData) {
        if (axis == "U" || axis == "V" || axis == "W") {
          // Convert to degrees for angular axes
          std::string angleKey = deviceName + "-ANGLE-" + axis;
          float angleDeg = static_cast<float>(position * 180.0 / M_PI);
          StoreValue(angleKey, angleDeg);
        }

        // Store workspace radius (distance from origin)
        if (axis == "X" || axis == "Y" || axis == "Z") {
          static std::map<std::string, std::array<double, 3>> devicePositions;
          auto& pos = devicePositions[deviceName];

          if (axis == "X") pos[0] = position;
          else if (axis == "Y") pos[1] = position;
          else if (axis == "Z") pos[2] = position;

          double radius = std::sqrt(pos[0] * pos[0] + pos[1] * pos[1] + pos[2] * pos[2]);
          StoreValue(deviceName + "-RADIUS", static_cast<float>(radius));
        }
      }
    }
  }

  void OnMotionStatusChange(const std::string& deviceName,
    const std::string& axis,
    bool isMoving) override {
    if (!m_enabled) return;

    // Store motion status
    std::string key = deviceName + "-MOVING-" + axis;
    StoreValue(key, isMoving ? 1.0f : 0.0f);

    // PI specific: Store motion event timestamp
    if (isMoving) {
      std::string eventKey = deviceName + "-MOVE-START-" + axis;
      auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
      StoreValue(eventKey, static_cast<float>(timestamp));
    }
  }

  // Configuration methods
  void SetCalculateVelocity(bool enable) { m_calculateVelocity = enable; }
  void SetStoreRawPositions(bool enable) { m_storeRawPositions = enable; }
  void SetStoreProcessedData(bool enable) { m_storeProcessedData = enable; }
  void SetDecimationFactor(int factor) { m_decimationFactor = (std::max)(1, factor); }
};