// ProductReferenceManager_Points.cpp - Point management operations with action types
#include "ProductReferenceManager.h"
#include <algorithm>
#include <iostream>

// =============================================================================
// POINT MANAGEMENT
// =============================================================================

bool ProductReferenceManager::AddPoint(const std::string& productName, const Point3D& point) {
  auto* product = FindProductReference(productName);
  if (!product) {
    std::cout << "AddPoint: Product not found: " << productName << std::endl;
    return false;
  }

  // Check if point name is unique
  if (!IsPointNameUnique(productName, point.name)) {
    std::cout << "AddPoint: Point name already exists: " << point.name << std::endl;
    return false;
  }

  product->points.push_back(point);
  product->lastModified = GetCurrentTimestamp();
  std::cout << "AddPoint: Point added successfully: " << point.name
    << " (" << point.x << ", " << point.y << ", " << point.z << ") "
    << "Action: " << point.GetActionTypeString() << std::endl;
  return true;
}

// NEW: Overloaded AddPoint with action type parameter
bool ProductReferenceManager::AddPoint(const std::string& productName, const std::string& pointName,
  double x, double y, double z, Point3D::ActionType actionType, const std::string& description) {

  Point3D point(pointName, x, y, z, actionType);
  point.description = description;
  return AddPoint(productName, point);
}

bool ProductReferenceManager::UpdatePoint(const std::string& productName, const std::string& pointName,
  double x, double y, double z) {
  auto* point = GetPoint(productName, pointName);
  if (!point) {
    std::cout << "UpdatePoint: Point not found: " << pointName << " in product: " << productName << std::endl;
    return false;
  }

  point->x = x;
  point->y = y;
  point->z = z;
  point->isValid = true;

  // Update product timestamp
  auto* product = FindProductReference(productName);
  if (product) {
    product->lastModified = GetCurrentTimestamp();
  }

  std::cout << "UpdatePoint: Point updated successfully: " << pointName
    << " (" << x << ", " << y << ", " << z << ")" << std::endl;
  return true;
}

// NEW: UpdatePoint with action type - renamed to avoid conflict
bool ProductReferenceManager::UpdatePointWithAction(const std::string& productName, const std::string& pointName,
  double x, double y, double z, Point3D::ActionType actionType) {

  auto* point = GetPoint(productName, pointName);
  if (!point) {
    std::cout << "UpdatePointWithAction: Point not found: " << pointName << " in product: " << productName << std::endl;
    return false;
  }

  point->x = x;
  point->y = y;
  point->z = z;
  point->actionType = actionType;
  point->isValid = true;

  // Update product timestamp
  auto* product = FindProductReference(productName);
  if (product) {
    product->lastModified = GetCurrentTimestamp();
  }

  std::cout << "UpdatePointWithAction: Point updated successfully: " << pointName
    << " (" << x << ", " << y << ", " << z << ") "
    << "Action: " << point->GetActionTypeString() << std::endl;
  return true;
}

// NEW: Update only point action type
bool ProductReferenceManager::UpdatePointActionType(const std::string& productName, const std::string& pointName,
  Point3D::ActionType actionType) {

  auto* point = GetPoint(productName, pointName);
  if (!point) {
    std::cout << "UpdatePointActionType: Point not found: " << pointName << " in product: " << productName << std::endl;
    return false;
  }

  point->actionType = actionType;

  // Update product timestamp
  auto* product = FindProductReference(productName);
  if (product) {
    product->lastModified = GetCurrentTimestamp();
  }

  std::cout << "UpdatePointActionType: Point action type updated: " << pointName
    << " -> " << point->GetActionTypeString() << std::endl;
  return true;
}

bool ProductReferenceManager::RemovePoint(const std::string& productName, const std::string& pointName) {
  auto* product = FindProductReference(productName);
  if (!product) {
    std::cout << "RemovePoint: Product not found: " << productName << std::endl;
    return false;
  }

  // Find and remove point
  auto it = std::find_if(product->points.begin(), product->points.end(),
    [&pointName](const Point3D& point) {
    return point.name == pointName;
  });

  if (it != product->points.end()) {
    std::cout << "RemovePoint: Removing " << it->GetActionTypeString() << " point: " << pointName << std::endl;

    product->points.erase(it);
    product->lastModified = GetCurrentTimestamp();

    // Also remove any edges that reference this point
    auto removedEdges = std::remove_if(product->edges.begin(), product->edges.end(),
      [&pointName](const Edge& edge) {
      return edge.point1Name == pointName || edge.point2Name == pointName;
    });

    int edgesRemoved = std::distance(removedEdges, product->edges.end());
    product->edges.erase(removedEdges, product->edges.end());

    // Clear datum if it references this point
    if (product->datum.originPointName == pointName) {
      ClearDatum(productName);
      std::cout << "RemovePoint: Datum system cleared due to origin point removal" << std::endl;
    }

    std::cout << "RemovePoint: Point removed successfully: " << pointName << " (also removed " << edgesRemoved << " related edges)" << std::endl;
    return true;
  }

  std::cout << "RemovePoint: Point not found: " << pointName << std::endl;
  return false;
}

ProductReferenceManager::Point3D* ProductReferenceManager::GetPoint(const std::string& productName, const std::string& pointName) {
  auto* product = FindProductReference(productName);
  if (!product) {
    return nullptr;
  }

  auto it = std::find_if(product->points.begin(), product->points.end(),
    [&pointName](const Point3D& point) {
    return point.name == pointName;
  });

  return (it != product->points.end()) ? &(*it) : nullptr;
}

const ProductReferenceManager::Point3D* ProductReferenceManager::GetPoint(const std::string& productName, const std::string& pointName) const {
  const auto* product = FindProductReference(productName);
  if (!product) {
    return nullptr;
  }

  auto it = std::find_if(product->points.begin(), product->points.end(),
    [&pointName](const Point3D& point) {
    return point.name == pointName;
  });

  return (it != product->points.end()) ? &(*it) : nullptr;
}

// NEW: Get points filtered by action type
std::vector<ProductReferenceManager::Point3D*> ProductReferenceManager::GetPointsByActionType(
  const std::string& productName, Point3D::ActionType actionType) {

  std::vector<Point3D*> filteredPoints;

  auto* product = FindProductReference(productName);
  if (!product) {
    return filteredPoints;
  }

  for (auto& point : product->points) {
    if (point.actionType == actionType) {
      filteredPoints.push_back(&point);
    }
  }

  /*std::cout << "GetPointsByActionType: Found " << filteredPoints.size()
    << " points of type " << (filteredPoints.empty() ? "Unknown" : filteredPoints[0]->GetActionTypeString())
    << " in product " << productName << std::endl;*/

  return filteredPoints;
}

std::vector<const ProductReferenceManager::Point3D*> ProductReferenceManager::GetPointsByActionType(
  const std::string& productName, Point3D::ActionType actionType) const {

  std::vector<const Point3D*> filteredPoints;

  const auto* product = FindProductReference(productName);
  if (!product) {
    return filteredPoints;
  }

  for (const auto& point : product->points) {
    if (point.actionType == actionType) {
      filteredPoints.push_back(&point);
    }
  }

  return filteredPoints;
}