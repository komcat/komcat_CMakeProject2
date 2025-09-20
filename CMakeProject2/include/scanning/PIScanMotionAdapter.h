//include/scanning/PIScanMotionAdapter.h
#pragma once
#include "include/motions/pi_controller.h"
#include "i_scan_motion_controller.h"
#include <string>
#include <vector>
#include <map>

// Adapter for PI Controller with full 3D support
class PIScanMotionAdapter : public IScanMotionController {
public:
  PIScanMotionAdapter(PIController* controller, const std::string& deviceName)
    : m_controller(controller), m_deviceName(deviceName) {
  }

  // Subscribe/unsubscribe methods
  void SubscribeScanner(IPositionSubscriber* scanner) {
    if (m_controller && scanner) {
      m_controller->SubscribeToPositions(scanner, "GridScanner_" + m_deviceName);
    }
  }

  void UnsubscribeScanner() {
    if (m_controller) {
      m_controller->UnsubscribeFromPositions("GridScanner_" + m_deviceName);
    }
  }

  // Connection status
  bool IsConnected() const override {
    return m_controller && m_controller->IsConnected();
  }

  // 2D Movement (existing)
  bool MoveToXY(double x, double y, bool blocking) override {
    if (!m_controller) return false;
    std::vector<std::string> axes = { "X", "Y" };
    std::vector<double> positions = { x, y };
    return m_controller->MoveToPositionMultiAxis(axes, positions, blocking);
  }

  bool GetCurrentXY(double& x, double& y) override {
    if (!m_controller) return false;
    std::map<std::string, double> positions;
    if (m_controller->GetPositions(positions)) {
      auto xIt = positions.find("X");
      auto yIt = positions.find("Y");
      if (xIt != positions.end() && yIt != positions.end()) {
        x = xIt->second;
        y = yIt->second;
        return true;
      }
    }
    return false;
  }

  // 3D Movement (NEW)
  bool MoveToXYZ(double x, double y, double z, bool blocking) override {
    if (!m_controller) return false;
    std::vector<std::string> axes = { "X", "Y", "Z" };
    std::vector<double> positions = { x, y, z };
    return m_controller->MoveToPositionMultiAxis(axes, positions, blocking);
  }

  bool GetCurrentXYZ(double& x, double& y, double& z) override {
    if (!m_controller) return false;
    std::map<std::string, double> positions;
    if (m_controller->GetPositions(positions)) {
      auto xIt = positions.find("X");
      auto yIt = positions.find("Y");
      auto zIt = positions.find("Z");
      if (xIt != positions.end() && yIt != positions.end() && zIt != positions.end()) {
        x = xIt->second;
        y = yIt->second;
        z = zIt->second;
        return true;
      }
    }
    return false;
  }

  bool MoveToZ(double z, bool blocking) override {
    if (!m_controller) return false;
    return m_controller->MoveToPosition("Z", z, blocking);
  }

  bool GetCurrentZ(double& z) override {
    if (!m_controller) return false;
    std::map<std::string, double> positions;
    if (m_controller->GetPositions(positions)) {
      auto zIt = positions.find("Z");
      if (zIt != positions.end()) {
        z = zIt->second;
        return true;
      }
    }
    return false;
  }

  // Motion status
  bool IsMoving() override {
    if (!m_controller) return false;
    return m_controller->IsMoving("X") || m_controller->IsMoving("Y") || m_controller->IsMoving("Z");
  }

  bool StopMotion() override {
    if (!m_controller) return false;
    return m_controller->StopAllAxes();
  }

  std::string GetDeviceName() const override {
    return m_deviceName;
  }

  // Add method to access underlying controller for advanced operations
  PIController* GetController() const {
    return m_controller;
  }

private:
  PIController* m_controller;
  std::string m_deviceName;
};