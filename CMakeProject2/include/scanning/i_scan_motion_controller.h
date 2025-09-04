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

  // Position control
  virtual bool MoveToXY(double x, double y, bool blocking = false) = 0;
  virtual bool GetCurrentXY(double& x, double& y) = 0;

  // Motion status
  virtual bool IsMoving() = 0;
  virtual bool StopMotion() = 0;

  // Optional: Get device name for logging
  virtual std::string GetDeviceName() const = 0;
};

