// ProductReferenceManager_Edges.cpp - Edge management operations
#include "ProductReferenceManager.h"
#include <algorithm>
#include <iostream>

// =============================================================================
// EDGE MANAGEMENT
// =============================================================================

bool ProductReferenceManager::AddEdge(const std::string& productName, const Edge& edge) {
  auto* product = FindProductReference(productName);
  if (!product) {
    std::cout << "AddEdge: Product not found: " << productName << std::endl;
    return false;
  }

  // Validate edge points exist
  std::string errorMsg;
  if (!ValidateEdgePoints(productName, edge.point1Name, edge.point2Name, errorMsg)) {
    std::cout << "AddEdge: Edge validation failed: " << errorMsg << std::endl;
    return false;
  }

  // Check if edge name is unique
  if (!IsEdgeNameUnique(productName, edge.name)) {
    std::cout << "AddEdge: Edge name already exists: " << edge.name << std::endl;
    return false;
  }

  product->edges.push_back(edge);
  product->lastModified = GetCurrentTimestamp();
  std::cout << "AddEdge: Edge added successfully: " << edge.name << " (" << edge.point1Name << " → " << edge.point2Name << ")" << std::endl;
  return true;
}

bool ProductReferenceManager::RemoveEdge(const std::string& productName, const std::string& edgeName) {
  auto* product = FindProductReference(productName);
  if (!product) {
    std::cout << "RemoveEdge: Product not found: " << productName << std::endl;
    return false;
  }

  auto it = std::find_if(product->edges.begin(), product->edges.end(),
    [&edgeName](const Edge& edge) {
    return edge.name == edgeName;
  });

  if (it != product->edges.end()) {
    product->edges.erase(it);
    product->lastModified = GetCurrentTimestamp();

    // Clear datum if it references this edge
    if (product->datum.xAxisEdgeName == edgeName || product->datum.yAxisEdgeName == edgeName) {
      ClearDatum(productName);
      std::cout << "RemoveEdge: Datum system cleared due to axis edge removal" << std::endl;
    }

    std::cout << "RemoveEdge: Edge removed successfully: " << edgeName << std::endl;
    return true;
  }

  std::cout << "RemoveEdge: Edge not found: " << edgeName << std::endl;
  return false;
}

ProductReferenceManager::Edge* ProductReferenceManager::GetEdge(const std::string& productName, const std::string& edgeName) {
  auto* product = FindProductReference(productName);
  if (!product) {
    return nullptr;
  }

  auto it = std::find_if(product->edges.begin(), product->edges.end(),
    [&edgeName](const Edge& edge) {
    return edge.name == edgeName;
  });

  return (it != product->edges.end()) ? &(*it) : nullptr;
}

const ProductReferenceManager::Edge* ProductReferenceManager::GetEdge(const std::string& productName, const std::string& edgeName) const {
  const auto* product = FindProductReference(productName);
  if (!product) {
    return nullptr;
  }

  auto it = std::find_if(product->edges.begin(), product->edges.end(),
    [&edgeName](const Edge& edge) {
    return edge.name == edgeName;
  });

  return (it != product->edges.end()) ? &(*it) : nullptr;
}

std::string ProductReferenceManager::GenerateEdgeName(const std::string& point1, const std::string& point2) const {
  return point1 + "_to_" + point2;
}