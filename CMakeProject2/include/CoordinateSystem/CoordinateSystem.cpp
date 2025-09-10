// CoordinateSystem.cpp
#include "CoordinateSystem.h"
#include <fstream>
#include <iostream>
#include <sstream>

bool CoordinateSystem::LoadFromJsonFile(const std::string& filepath) {
  ClearError();

  try {
    std::ifstream file(filepath);
    if (!file.is_open()) {
      SetError("Failed to open file: " + filepath);
      return false;
    }

    nlohmann::json j;
    file >> j;
    file.close();

    return ParseJsonToSystem(j);

  }
  catch (const std::exception& e) {
    SetError("Exception loading JSON file: " + std::string(e.what()));
    return false;
  }
}

bool CoordinateSystem::LoadFromJsonString(const std::string& jsonString) {
  ClearError();

  try {
    nlohmann::json j = nlohmann::json::parse(jsonString);
    return ParseJsonToSystem(j);

  }
  catch (const std::exception& e) {
    SetError("Exception parsing JSON string: " + std::string(e.what()));
    return false;
  }
}

bool CoordinateSystem::LoadFromAlignment(const ModuleAlignment::AlignmentResult& result) {
  ClearError();

  if (!result.success) {
    SetError("Alignment result is not successful");
    return false;
  }

  m_data = SystemData();
  m_data.name = result.alignmentName.empty() ? "Unnamed_System" : result.alignmentName;
  m_data.timestamp = result.timestamp;

  // Copy origin
  m_data.origin = result.centerPosition;

  // Build transformation matrices
  const auto& xAxis = result.xAxisDirection;
  const auto& yAxis = result.yAxisDirection;
  const auto& zAxis = result.zAxisDirection;
  const auto& center = result.centerPosition;

  // Matrix to machine (T = [R | t])
  m_data.toMachine[0][0] = xAxis.x;  m_data.toMachine[0][1] = yAxis.x;  m_data.toMachine[0][2] = zAxis.x;  m_data.toMachine[0][3] = center.x;
  m_data.toMachine[1][0] = xAxis.y;  m_data.toMachine[1][1] = yAxis.y;  m_data.toMachine[1][2] = zAxis.y;  m_data.toMachine[1][3] = center.y;
  m_data.toMachine[2][0] = xAxis.z;  m_data.toMachine[2][1] = yAxis.z;  m_data.toMachine[2][2] = zAxis.z;  m_data.toMachine[2][3] = center.z;
  m_data.toMachine[3][0] = 0.0;      m_data.toMachine[3][1] = 0.0;      m_data.toMachine[3][2] = 0.0;      m_data.toMachine[3][3] = 1.0;

  // Matrix to local (inverse: T^-1 = [R^T | -R^T*t])
  // Transpose rotation part
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      m_data.toLocal[i][j] = m_data.toMachine[j][i];
    }
  }

  // Calculate -R^T * t
  m_data.toLocal[0][3] = -(m_data.toLocal[0][0] * center.x + m_data.toLocal[0][1] * center.y + m_data.toLocal[0][2] * center.z);
  m_data.toLocal[1][3] = -(m_data.toLocal[1][0] * center.x + m_data.toLocal[1][1] * center.y + m_data.toLocal[1][2] * center.z);
  m_data.toLocal[2][3] = -(m_data.toLocal[2][0] * center.x + m_data.toLocal[2][1] * center.y + m_data.toLocal[2][2] * center.z);

  m_data.toLocal[3][0] = 0.0;
  m_data.toLocal[3][1] = 0.0;
  m_data.toLocal[3][2] = 0.0;
  m_data.toLocal[3][3] = 1.0;

  // Store axis information
  m_data.axes.xLength = result.xAxisLength;
  m_data.axes.yLength = result.yAxisLength;
  m_data.axes.angleToGlobalX = result.axisAngle;
  m_data.axes.xDirection = { xAxis.x, xAxis.y, xAxis.z };
  m_data.axes.yDirection = { yAxis.x, yAxis.y, yAxis.z };
  m_data.axes.zDirection = { zAxis.x, zAxis.y, zAxis.z };

  m_data.isValid = true;
  return true;
}

bool CoordinateSystem::ParseJsonToSystem(const nlohmann::json& j) {
  try {
    m_data = SystemData();

    // Parse metadata
    if (j.contains("metadata")) {
      m_data.name = j["metadata"].value("alignment_name", "Unknown");
      m_data.timestamp = j["metadata"].value("timestamp", "");
    }

    // Parse center/origin
    if (j.contains("results") && j["results"].contains("center_position")) {
      const auto& center = j["results"]["center_position"];
      m_data.origin.x = center.value("x", 0.0);
      m_data.origin.y = center.value("y", 0.0);
      m_data.origin.z = center.value("z", 0.0);
    }

    // Parse transformation matrices
    if (j.contains("transformation")) {
      // Matrix to machine
      if (j["transformation"].contains("matrix_to_machine")) {
        const auto& matrix = j["transformation"]["matrix_to_machine"];
        for (int i = 0; i < 4 && i < matrix.size(); i++) {
          for (int j = 0; j < 4 && j < matrix[i].size(); j++) {
            m_data.toMachine[i][j] = matrix[i][j];
          }
        }
      }

      // Matrix to local/alignment
      if (j["transformation"].contains("matrix_to_alignment")) {
        const auto& matrix = j["transformation"]["matrix_to_alignment"];
        for (int i = 0; i < 4 && i < matrix.size(); i++) {
          for (int j = 0; j < 4 && j < matrix[i].size(); j++) {
            m_data.toLocal[i][j] = matrix[i][j];
          }
        }
      }
    }

    // Parse axes information
    if (j.contains("results") && j["results"].contains("axes")) {
      const auto& axes = j["results"]["axes"];

      m_data.axes.angleToGlobalX = axes.value("angle_to_global_x", 0.0);

      if (axes.contains("x_axis")) {
        m_data.axes.xLength = axes["x_axis"].value("length", 0.0);
        m_data.axes.xDirection[0] = axes["x_axis"].value("x", 1.0);
        m_data.axes.xDirection[1] = axes["x_axis"].value("y", 0.0);
        m_data.axes.xDirection[2] = axes["x_axis"].value("z", 0.0);
      }

      if (axes.contains("y_axis")) {
        m_data.axes.yLength = axes["y_axis"].value("length", 0.0);
        m_data.axes.yDirection[0] = axes["y_axis"].value("x", 0.0);
        m_data.axes.yDirection[1] = axes["y_axis"].value("y", 1.0);
        m_data.axes.yDirection[2] = axes["y_axis"].value("z", 0.0);
      }

      if (axes.contains("z_axis")) {
        m_data.axes.zDirection[0] = axes["z_axis"].value("x", 0.0);
        m_data.axes.zDirection[1] = axes["z_axis"].value("y", 0.0);
        m_data.axes.zDirection[2] = axes["z_axis"].value("z", 1.0);
      }
    }

    m_data.isValid = true;
    return true;

  }
  catch (const std::exception& e) {
    SetError("Exception parsing JSON: " + std::string(e.what()));
    return false;
  }
}

PositionStruct CoordinateSystem::TransformToMachine(const PositionStruct& localPos) const {
  if (!m_data.isValid) {
    return localPos; // Return unchanged if invalid
  }

  // Apply transformation: machine = T * local
  double local[4] = { localPos.x, localPos.y, localPos.z, 1.0 };
  double machine[4] = { 0.0, 0.0, 0.0, 0.0 };

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      machine[i] += m_data.toMachine[i][j] * local[j];
    }
  }

  PositionStruct result;
  result.x = machine[0];
  result.y = machine[1];
  result.z = machine[2];
  result.u = localPos.u; // Pass through rotational axes
  result.v = localPos.v;
  result.w = localPos.w;

  return result;
}

PositionStruct CoordinateSystem::TransformToLocal(const PositionStruct& machinePos) const {
  if (!m_data.isValid) {
    return machinePos; // Return unchanged if invalid
  }

  // Apply inverse transformation: local = T^-1 * machine
  double machine[4] = { machinePos.x, machinePos.y, machinePos.z, 1.0 };
  double local[4] = { 0.0, 0.0, 0.0, 0.0 };

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      local[i] += m_data.toLocal[i][j] * machine[j];
    }
  }

  PositionStruct result;
  result.x = local[0];
  result.y = local[1];
  result.z = local[2];
  result.u = machinePos.u; // Pass through rotational axes
  result.v = machinePos.v;
  result.w = machinePos.w;

  return result;
}

bool CoordinateSystem::SaveToJsonFile(const std::string& filepath) const {
  try {
    std::ofstream file(filepath);
    if (!file.is_open()) {
      return false;
    }

    file << std::setw(4) << SystemToJson() << std::endl;
    file.close();
    return true;

  }
  catch (const std::exception&) {
    return false;
  }
}

std::string CoordinateSystem::ToJsonString() const {
  try {
    return SystemToJson().dump(4);
  }
  catch (const std::exception&) {
    return "{}";
  }
}

nlohmann::json CoordinateSystem::SystemToJson() const {
  nlohmann::json j;

  j["metadata"]["name"] = m_data.name;
  j["metadata"]["timestamp"] = m_data.timestamp;
  j["metadata"]["is_valid"] = m_data.isValid;

  j["origin"]["x"] = m_data.origin.x;
  j["origin"]["y"] = m_data.origin.y;
  j["origin"]["z"] = m_data.origin.z;

  // Transformation matrices
  j["transformation"]["matrix_to_machine"] = nlohmann::json::array();
  j["transformation"]["matrix_to_local"] = nlohmann::json::array();

  for (int i = 0; i < 4; i++) {
    nlohmann::json rowMachine = nlohmann::json::array();
    nlohmann::json rowLocal = nlohmann::json::array();

    for (int j = 0; j < 4; j++) {
      rowMachine.push_back(m_data.toMachine[i][j]);
      rowLocal.push_back(m_data.toLocal[i][j]);
    }

    j["transformation"]["matrix_to_machine"].push_back(rowMachine);
    j["transformation"]["matrix_to_local"].push_back(rowLocal);
  }

  // Axes
  j["axes"]["x_axis"]["direction"] = m_data.axes.xDirection;
  j["axes"]["x_axis"]["length"] = m_data.axes.xLength;

  j["axes"]["y_axis"]["direction"] = m_data.axes.yDirection;
  j["axes"]["y_axis"]["length"] = m_data.axes.yLength;

  j["axes"]["z_axis"]["direction"] = m_data.axes.zDirection;
  j["axes"]["angle_to_global_x"] = m_data.axes.angleToGlobalX;

  return j;
}

void CoordinateSystem::SetError(const std::string& error) {
  m_lastError = error;
  std::cerr << "CoordinateSystem Error: " << error << std::endl;
}

void CoordinateSystem::ClearError() {
  m_lastError.clear();
}