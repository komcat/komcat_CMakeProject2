// CoordinateSystem.h
#pragma once

#include "include/machine_operations.h"
#include "include/products/ModuleAlignment.h"
#include <nlohmann/json.hpp>
#include <array>
#include <string>
#include <memory>

/**
 * @brief Represents a coordinate system transformation
 *
 * Can be loaded from:
 * - JSON files exported from ModuleAlignment
 * - Real-time ModuleAlignment results
 * - Manual configuration
 */
class CoordinateSystem {
public:
  struct SystemData {
    std::string name;
    std::string timestamp;

    // Origin in machine coordinates
    PositionStruct origin;

    // Transformation matrices (4x4 homogeneous)
    std::array<std::array<double, 4>, 4> toMachine;
    std::array<std::array<double, 4>, 4> toLocal;

    // Axis information (for diagnostics/display)
    struct AxisInfo {
      double xLength = 0.0;
      double yLength = 0.0;
      double angleToGlobalX = 0.0;
      std::array<double, 3> xDirection = { 1, 0, 0 };
      std::array<double, 3> yDirection = { 0, 1, 0 };
      std::array<double, 3> zDirection = { 0, 0, 1 };
    } axes;

    bool isValid = false;
  };

  CoordinateSystem() = default;
  ~CoordinateSystem() = default;

  // Loading methods
  bool LoadFromJsonFile(const std::string& filepath);
  bool LoadFromJsonString(const std::string& jsonString);
  bool LoadFromAlignment(const ModuleAlignment::AlignmentResult& result);

  // Transformation methods
  PositionStruct TransformToMachine(const PositionStruct& localPos) const;
  PositionStruct TransformToLocal(const PositionStruct& machinePos) const;

  // Utility methods
  bool SaveToJsonFile(const std::string& filepath) const;
  std::string ToJsonString() const;

  // Getters
  const std::string& GetName() const { return m_data.name; }
  const std::string& GetTimestamp() const { return m_data.timestamp; }
  const PositionStruct& GetOrigin() const { return m_data.origin; }
  const SystemData& GetData() const { return m_data; }
  bool IsValid() const { return m_data.isValid; }

  // Get axis directions as unit vectors
  std::array<double, 3> GetXAxis() const { return m_data.axes.xDirection; }
  std::array<double, 3> GetYAxis() const { return m_data.axes.yDirection; }
  std::array<double, 3> GetZAxis() const { return m_data.axes.zDirection; }

  // Error handling
  const std::string& GetLastError() const { return m_lastError; }

private:
  SystemData m_data;
  std::string m_lastError;

  bool ParseJsonToSystem(const nlohmann::json& j);
  nlohmann::json SystemToJson() const;
  void SetError(const std::string& error);
  void ClearError();
};