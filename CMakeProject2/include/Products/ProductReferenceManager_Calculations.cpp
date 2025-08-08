// ProductReferenceManager_Calculations.cpp - Mathematical calculations
#include "ProductReferenceManager.h"
#include <cmath>
#include <array>
#include <optional>

// Define M_PI if not already defined
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =============================================================================
// COORDINATE SYSTEM CALCULATIONS
// =============================================================================

std::optional<double> ProductReferenceManager::CalculateEdgeLength(const std::string& productName,
  const std::string& edgeName) const {
  const auto* edge = GetEdge(productName, edgeName);
  if (!edge) {
    return std::nullopt;
  }

  const auto* point1 = GetPoint(productName, edge->point1Name);
  const auto* point2 = GetPoint(productName, edge->point2Name);

  if (!point1 || !point2 || !point1->isValid || !point2->isValid) {
    return std::nullopt;
  }

  double dx = point2->x - point1->x;
  double dy = point2->y - point1->y;
  double dz = point2->z - point1->z;

  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

std::optional<double> ProductReferenceManager::CalculateAngleBetweenEdges(const std::string& productName,
  const std::string& edge1Name,
  const std::string& edge2Name) const {
  const auto* edge1 = GetEdge(productName, edge1Name);
  const auto* edge2 = GetEdge(productName, edge2Name);

  if (!edge1 || !edge2) {
    return std::nullopt;
  }

  // Get vectors for both edges
  auto vec1 = CalculateVector(*GetPoint(productName, edge1->point1Name),
    *GetPoint(productName, edge1->point2Name));
  auto vec2 = CalculateVector(*GetPoint(productName, edge2->point1Name),
    *GetPoint(productName, edge2->point2Name));

  if (!vec1 || !vec2) {
    return std::nullopt;
  }

  // Calculate dot product and magnitudes
  double dot = vec1.value()[0] * vec2.value()[0] + vec1.value()[1] * vec2.value()[1] + vec1.value()[2] * vec2.value()[2];
  double mag1 = std::sqrt(vec1.value()[0] * vec1.value()[0] + vec1.value()[1] * vec1.value()[1] + vec1.value()[2] * vec1.value()[2]);
  double mag2 = std::sqrt(vec2.value()[0] * vec2.value()[0] + vec2.value()[1] * vec2.value()[1] + vec2.value()[2] * vec2.value()[2]);

  if (mag1 == 0.0 || mag2 == 0.0) {
    return std::nullopt;
  }

  // Calculate angle in degrees
  double cosAngle = dot / (mag1 * mag2);
  cosAngle = std::max(-1.0, std::min(1.0, cosAngle));  // Clamp to valid range
  double angleRad = std::acos(cosAngle);
  return angleRad * 180.0 / M_PI;
}

std::optional<std::array<double, 3>> ProductReferenceManager::CalculateVector(const Point3D& from, const Point3D& to) const {
  if (!from.isValid || !to.isValid) {
    return std::nullopt;
  }

  return std::array<double, 3>{{to.x - from.x, to.y - from.y, to.z - from.z}};
}