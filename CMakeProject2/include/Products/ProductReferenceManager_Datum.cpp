// ProductReferenceManager_Datum.cpp - Datum creation and management
#include "ProductReferenceManager.h"
#include <iostream>

// =============================================================================
// DATUM SYSTEM CREATION
// =============================================================================

bool ProductReferenceManager::CreateDatumFrom2Points(const std::string& productName,
  const std::string& originPointName,
  const std::string& xAxisPointName) {
  auto* product = FindProductReference(productName);
  if (!product) {
    std::cout << "CreateDatumFrom2Points: Product not found: " << productName << std::endl;
    return false;
  }

  // Validate points exist
  const auto* originPoint = GetPoint(productName, originPointName);
  const auto* xAxisPoint = GetPoint(productName, xAxisPointName);

  if (!originPoint) {
    std::cout << "CreateDatumFrom2Points: Origin point not found: " << originPointName << std::endl;
    return false;
  }

  if (!xAxisPoint) {
    std::cout << "CreateDatumFrom2Points: X-axis point not found: " << xAxisPointName << std::endl;
    return false;
  }

  if (!originPoint->isValid) {
    std::cout << "CreateDatumFrom2Points: Origin point has invalid coordinates: " << originPointName << std::endl;
    return false;
  }

  if (!xAxisPoint->isValid) {
    std::cout << "CreateDatumFrom2Points: X-axis point has invalid coordinates: " << xAxisPointName << std::endl;
    return false;
  }

  // Check points are different
  if (originPointName == xAxisPointName) {
    std::cout << "CreateDatumFrom2Points: Origin and X-axis points are the same" << std::endl;
    return false;
  }

  // Create edge for X-axis
  std::string xAxisEdgeName = GenerateEdgeName(originPointName, xAxisPointName);
  std::cout << "CreateDatumFrom2Points: Creating edge: " << xAxisEdgeName << std::endl;

  // Check if edge already exists, if not create it
  if (!GetEdge(productName, xAxisEdgeName)) {
    Edge xAxisEdge(xAxisEdgeName, originPointName, xAxisPointName);
    xAxisEdge.description = "X-axis edge for datum";
    if (!AddEdge(productName, xAxisEdge)) {
      std::cout << "CreateDatumFrom2Points: Failed to create edge: " << xAxisEdgeName << std::endl;
      return false;
    }
    std::cout << "CreateDatumFrom2Points: Edge created successfully: " << xAxisEdgeName << std::endl;
  }
  else {
    std::cout << "CreateDatumFrom2Points: Edge already exists: " << xAxisEdgeName << std::endl;
  }

  // Set up datum
  product->datum.originPointName = originPointName;
  product->datum.xAxisEdgeName = xAxisEdgeName;
  product->datum.yAxisEdgeName = "";  // No Y-axis in 2-point system
  product->datum.constructionMethod = DatumReference::ConstructionMethod::TWO_POINT;
  product->datum.hasZAxis = false;

  product->lastModified = GetCurrentTimestamp();
  std::cout << "CreateDatumFrom2Points: Datum system created successfully" << std::endl;
  return true;
}

bool ProductReferenceManager::CreateDatumFrom3Points(const std::string& productName,
  const std::string& originPointName,
  const std::string& point2Name,
  const std::string& point3Name,
  bool useFirstEdgeAsX) {
  auto* product = FindProductReference(productName);
  if (!product) {
    std::cout << "CreateDatumFrom3Points: Product not found: " << productName << std::endl;
    return false;
  }

  // Validate points exist and are different
  const auto* originPoint = GetPoint(productName, originPointName);
  const auto* point2 = GetPoint(productName, point2Name);
  const auto* point3 = GetPoint(productName, point3Name);

  if (!originPoint) {
    std::cout << "CreateDatumFrom3Points: Origin point not found: " << originPointName << std::endl;
    return false;
  }

  if (!point2) {
    std::cout << "CreateDatumFrom3Points: Point2 not found: " << point2Name << std::endl;
    return false;
  }

  if (!point3) {
    std::cout << "CreateDatumFrom3Points: Point3 not found: " << point3Name << std::endl;
    return false;
  }

  if (!originPoint->isValid || !point2->isValid || !point3->isValid) {
    std::cout << "CreateDatumFrom3Points: One or more points have invalid coordinates" << std::endl;
    return false;
  }

  // Points must be different
  if (originPointName == point2Name || originPointName == point3Name || point2Name == point3Name) {
    std::cout << "CreateDatumFrom3Points: Points must be different from each other" << std::endl;
    return false;
  }

  // Create edges
  std::string edge1Name = GenerateEdgeName(originPointName, point2Name);
  std::string edge2Name = GenerateEdgeName(originPointName, point3Name);

  // Add edges if they don't exist
  if (!GetEdge(productName, edge1Name)) {
    Edge edge1(edge1Name, originPointName, point2Name);
    edge1.description = "First edge for datum creation";
    if (!AddEdge(productName, edge1)) {
      std::cout << "CreateDatumFrom3Points: Failed to create first edge: " << edge1Name << std::endl;
      return false;
    }
  }

  if (!GetEdge(productName, edge2Name)) {
    Edge edge2(edge2Name, originPointName, point3Name);
    edge2.description = "Second edge for datum creation";
    if (!AddEdge(productName, edge2)) {
      std::cout << "CreateDatumFrom3Points: Failed to create second edge: " << edge2Name << std::endl;
      return false;
    }
  }

  // Assign X and Y axes based on user choice
  std::string xAxisEdgeName = useFirstEdgeAsX ? edge1Name : edge2Name;
  std::string yAxisEdgeName = useFirstEdgeAsX ? edge2Name : edge1Name;

  // Set up datum
  product->datum.originPointName = originPointName;
  product->datum.xAxisEdgeName = xAxisEdgeName;
  product->datum.yAxisEdgeName = yAxisEdgeName;
  product->datum.constructionMethod = DatumReference::ConstructionMethod::THREE_POINT;
  product->datum.hasZAxis = true;  // Z-axis is calculated from X and Y

  product->lastModified = GetCurrentTimestamp();
  std::cout << "CreateDatumFrom3Points: Datum system created successfully (X-axis: " << xAxisEdgeName << ", Y-axis: " << yAxisEdgeName << ")" << std::endl;
  return true;
}

bool ProductReferenceManager::ClearDatum(const std::string& productName) {
  auto* product = FindProductReference(productName);
  if (!product) {
    std::cout << "ClearDatum: Product not found: " << productName << std::endl;
    return false;
  }

  // Reset datum to default state
  product->datum = DatumReference(product->name + "_Datum");
  product->lastModified = GetCurrentTimestamp();
  std::cout << "ClearDatum: Datum system cleared for product: " << productName << std::endl;
  return true;
}

bool ProductReferenceManager::ValidateDatumSystem(const std::string& productName, std::string& errorMessage) const {
  const auto* product = FindProductReference(productName);
  if (!product) {
    errorMessage = "Product not found";
    return false;
  }

  const auto& datum = product->datum;

  // Check if datum is set up
  if (datum.constructionMethod == DatumReference::ConstructionMethod::NONE) {
    errorMessage = "No datum system defined";
    return false;
  }

  // Validate origin point
  const auto* originPoint = GetPoint(productName, datum.originPointName);
  if (!originPoint || !originPoint->isValid) {
    errorMessage = "Invalid origin point";
    return false;
  }

  // Validate X-axis edge
  const auto* xAxisEdge = GetEdge(productName, datum.xAxisEdgeName);
  if (!xAxisEdge) {
    errorMessage = "Invalid X-axis edge";
    return false;
  }

  // For 3-point system, validate Y-axis edge
  if (datum.constructionMethod == DatumReference::ConstructionMethod::THREE_POINT) {
    const auto* yAxisEdge = GetEdge(productName, datum.yAxisEdgeName);
    if (!yAxisEdge) {
      errorMessage = "Invalid Y-axis edge";
      return false;
    }

    // Check that edges share the origin point
    if (xAxisEdge->point1Name != datum.originPointName && xAxisEdge->point2Name != datum.originPointName) {
      errorMessage = "X-axis edge does not connect to origin";
      return false;
    }

    if (yAxisEdge->point1Name != datum.originPointName && yAxisEdge->point2Name != datum.originPointName) {
      errorMessage = "Y-axis edge does not connect to origin";
      return false;
    }

    // Check angle between axes (should not be too small)
    auto angle = CalculateAngleBetweenEdges(productName, datum.xAxisEdgeName, datum.yAxisEdgeName);
    if (angle && (*angle < 5.0 || *angle > 175.0)) {
      errorMessage = "X and Y axes are too close to parallel (angle: " + std::to_string(*angle) + "°)";
      return false;
    }
  }

  errorMessage = "";
  return true;
}