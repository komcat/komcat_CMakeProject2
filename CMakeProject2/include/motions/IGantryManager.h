// IGantryManager.h - Simplified version with only required methods
#pragma once

class IGantryManager {
public:
  virtual ~IGantryManager() = default;
  virtual bool MoveToXY(double x, double y) = 0;
  virtual bool MoveToXYZ(double x, double y, double z) = 0;
};