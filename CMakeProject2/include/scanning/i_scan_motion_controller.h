// include/scanning/i_scan_motion_controller.h
#pragma once

#include <string>
#include <vector>
#include <map>

// Interface for motion control in scanning operations
class IScanMotionController {
public:
  virtual ~IScanMotionController() = default;

  // Connection status
  virtual bool IsConnected() const = 0;

  // 2D Position control (existing)
  virtual bool MoveToXY(double x, double y, bool blocking = false) = 0;
  virtual bool GetCurrentXY(double& x, double& y) = 0;

  // 3D Position control (NEW - for volume scanning)
  virtual bool MoveToXYZ(double x, double y, double z, bool blocking = false) = 0;
  virtual bool GetCurrentXYZ(double& x, double& y, double& z) = 0;
  virtual bool MoveToZ(double z, bool blocking = false) = 0;
  virtual bool GetCurrentZ(double& z) = 0;

  // Motion status
  virtual bool IsMoving() = 0;
  virtual bool StopMotion() = 0;

  // Optional: Get device name for logging
  virtual std::string GetDeviceName() const = 0;
};