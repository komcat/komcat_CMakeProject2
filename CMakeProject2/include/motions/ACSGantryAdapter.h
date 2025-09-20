// ACSGantryAdapter.h
#pragma once
#include "IGantryManager.h"
#include "include/motions/acs_controller_manager.h"

class ACSGantryAdapter : public IGantryManager {
public:
  ACSGantryAdapter(ACSControllerManager* manager, const std::string& deviceName)
    : m_manager(manager), m_deviceName(deviceName) {
  }

  bool MoveToXY(double x, double y) override {
    if (!m_manager) return false;

    auto controller = m_manager->GetController(m_deviceName);
    if (!controller || !controller->IsConnected()) return false;

    std::vector<std::string> axes = { "X", "Y" };
    std::vector<double> positions = { x, y };
    return controller->MoveToPositionMultiAxis(axes, positions, true);
  }

  bool MoveToXYZ(double x, double y, double z) override {
    if (!m_manager) return false;

    auto controller = m_manager->GetController(m_deviceName);
    if (!controller || !controller->IsConnected()) return false;

    std::vector<std::string> axes = { "X", "Y", "Z" };
    std::vector<double> positions = { x, y, z };
    return controller->MoveToPositionMultiAxis(axes, positions, true);
  }

private:
  ACSControllerManager* m_manager;
  std::string m_deviceName;
};