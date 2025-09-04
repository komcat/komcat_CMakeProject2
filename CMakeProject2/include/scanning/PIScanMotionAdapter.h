//include/scanning/PIScanMotionAdapter.h
#pragma once
#include "include/motions/pi_controller.h"
#include "i_scan_motion_controller.h"
#include <string>
#include <vector>
#include <map>


// Adapter for PI Controller
class PIScanMotionAdapter : public IScanMotionController {
public:
  PIScanMotionAdapter(PIController* controller, const std::string& deviceName)
    : m_controller(controller), m_deviceName(deviceName) {
  }

  bool IsConnected() const override {
    return m_controller && m_controller->IsConnected();
  }

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

  bool IsMoving() override {
    if (!m_controller) return false;
    return m_controller->IsMoving("X") || m_controller->IsMoving("Y");
  }

  bool StopMotion() override {
    if (!m_controller) return false;
    return m_controller->StopAllAxes();
  }

  std::string GetDeviceName() const override {
    return m_deviceName;
  }

private:
  PIController* m_controller;
  std::string m_deviceName;
};



/*
// Create the motion adapter
auto piController = piControllerManager.GetController("YourDevice");
auto motionAdapter = std::make_shared<PIScanMotionAdapter>(piController, "YourDevice");

// Create the scanner with the adapter
auto gridScanner = std::make_shared<GridScanner>(
    motionAdapter,
    dataClientManager,
    "DataChannel1"
);

// If you later want to use different hardware, just create a different adapter:
// auto acsAdapter = std::make_shared<ACSScanMotionAdapter>(acsController, "ACSDevice");
// auto gridScanner = std::make_shared<GridScanner>(acsAdapter, dataClientManager, "DataChannel1");



*/