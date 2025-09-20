// ProductReferenceManager_Validation.cpp - Validation functions
#include "ProductReferenceManager.h"
#include <algorithm>
#include <vector>
#include <iostream>

// =============================================================================
// VALIDATION & ERROR CHECKING
// =============================================================================

bool ProductReferenceManager::ValidateProductReference(const std::string& productName, std::string& errorMessage) const {
  const auto* product = FindProductReference(productName);
  if (!product) {
    errorMessage = "Product not found";
    return false;
  }

  // Check for duplicate point names
  std::vector<std::string> pointNames;
  for (const auto& point : product->points) {
    if (std::find(pointNames.begin(), pointNames.end(), point.name) != pointNames.end()) {
      errorMessage = "Duplicate point name: " + point.name;
      return false;
    }
    pointNames.push_back(point.name);
  }

  // Check for duplicate edge names
  std::vector<std::string> edgeNames;
  for (const auto& edge : product->edges) {
    if (std::find(edgeNames.begin(), edgeNames.end(), edge.name) != edgeNames.end()) {
      errorMessage = "Duplicate edge name: " + edge.name;
      return false;
    }
    edgeNames.push_back(edge.name);
  }

  // Validate edges reference existing points
  for (const auto& edge : product->edges) {
    if (!GetPoint(productName, edge.point1Name) || !GetPoint(productName, edge.point2Name)) {
      errorMessage = "Edge '" + edge.name + "' references non-existent point";
      return false;
    }
  }

  errorMessage = "";
  return true;
}

bool ProductReferenceManager::IsPointNameUnique(const std::string& productName, const std::string& pointName) const {
  return GetPoint(productName, pointName) == nullptr;
}

bool ProductReferenceManager::IsEdgeNameUnique(const std::string& productName, const std::string& edgeName) const {
  return GetEdge(productName, edgeName) == nullptr;
}

bool ProductReferenceManager::ValidateEdgePoints(const std::string& productName,
  const std::string& point1, const std::string& point2,
  std::string& errorMessage) const {
  if (point1 == point2) {
    errorMessage = "Edge cannot connect a point to itself";
    return false;
  }

  const auto* p1 = GetPoint(productName, point1);
  const auto* p2 = GetPoint(productName, point2);

  if (!p1) {
    errorMessage = "Point '" + point1 + "' does not exist";
    return false;
  }

  if (!p2) {
    errorMessage = "Point '" + point2 + "' does not exist";
    return false;
  }

  if (!p1->isValid || !p2->isValid) {
    errorMessage = "One or both points have invalid coordinates";
    return false;
  }

  errorMessage = "";
  return true;
}