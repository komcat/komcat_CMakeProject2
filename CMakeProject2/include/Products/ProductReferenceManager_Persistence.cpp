// ProductReferenceManager_Persistence.cpp - Complete JSON Save/Load Implementation
#include "ProductReferenceManager.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <chrono>

using json = nlohmann::json;

// =============================================================================
// HELPER METHODS FOR JSON CONVERSION
// =============================================================================

namespace {
  // Convert Point3D to JSON
  json Point3DToJson(const ProductReferenceManager::Point3D& point) {
    json j;
    j["name"] = point.name;
    j["description"] = point.description;
    j["x"] = point.x;
    j["y"] = point.y;
    j["z"] = point.z;
    j["isValid"] = point.isValid;
    j["actionType"] = point.GetActionTypeString();  // Save as string
    return j;
  }

  // Convert JSON to Point3D
  ProductReferenceManager::Point3D Point3DFromJson(const json& j) {
    ProductReferenceManager::Point3D point;
    point.name = j.value("name", "");
    point.description = j.value("description", "");
    point.x = j.value("x", 0.0);
    point.y = j.value("y", 0.0);
    point.z = j.value("z", 0.0);
    point.isValid = j.value("isValid", false);

    // Load action type (default to FIDUCIAL for backward compatibility)
    std::string actionStr = j.value("actionType", "Fiducial");
    if (!point.SetActionTypeFromString(actionStr)) {
      point.actionType = ProductReferenceManager::Point3D::ActionType::FIDUCIAL;
    }

    return point;
  }

  // Convert Edge to JSON
  json EdgeToJson(const ProductReferenceManager::Edge& edge) {
    json j;
    j["name"] = edge.name;
    j["description"] = edge.description;
    j["point1Name"] = edge.point1Name;
    j["point2Name"] = edge.point2Name;
    return j;
  }

  // Convert JSON to Edge
  ProductReferenceManager::Edge EdgeFromJson(const json& j) {
    ProductReferenceManager::Edge edge;
    edge.name = j.value("name", "");
    edge.description = j.value("description", "");
    edge.point1Name = j.value("point1Name", "");
    edge.point2Name = j.value("point2Name", "");
    return edge;
  }

  // Convert DatumReference to JSON
  json DatumReferenceToJson(const ProductReferenceManager::DatumReference& datum) {
    json j;
    j["name"] = datum.name;
    j["description"] = datum.description;
    j["originPointName"] = datum.originPointName;
    j["xAxisEdgeName"] = datum.xAxisEdgeName;
    j["yAxisEdgeName"] = datum.yAxisEdgeName;
    j["hasZAxis"] = datum.hasZAxis;

    // Convert construction method to string
    std::string methodStr;
    switch (datum.constructionMethod) {
    case ProductReferenceManager::DatumReference::ConstructionMethod::NONE:
      methodStr = "NONE";
      break;
    case ProductReferenceManager::DatumReference::ConstructionMethod::TWO_POINT:
      methodStr = "TWO_POINT";
      break;
    case ProductReferenceManager::DatumReference::ConstructionMethod::THREE_POINT:
      methodStr = "THREE_POINT";
      break;
    default:
      methodStr = "NONE";
      break;
    }
    j["constructionMethod"] = methodStr;

    return j;
  }

  // Convert JSON to DatumReference
  ProductReferenceManager::DatumReference DatumReferenceFromJson(const json& j) {
    ProductReferenceManager::DatumReference datum;
    datum.name = j.value("name", "");
    datum.description = j.value("description", "");
    datum.originPointName = j.value("originPointName", "");
    datum.xAxisEdgeName = j.value("xAxisEdgeName", "");
    datum.yAxisEdgeName = j.value("yAxisEdgeName", "");
    datum.hasZAxis = j.value("hasZAxis", false);

    // Convert construction method from string
    std::string methodStr = j.value("constructionMethod", "NONE");
    if (methodStr == "TWO_POINT") {
      datum.constructionMethod = ProductReferenceManager::DatumReference::ConstructionMethod::TWO_POINT;
    }
    else if (methodStr == "THREE_POINT") {
      datum.constructionMethod = ProductReferenceManager::DatumReference::ConstructionMethod::THREE_POINT;
    }
    else {
      datum.constructionMethod = ProductReferenceManager::DatumReference::ConstructionMethod::NONE;
    }

    return datum;
  }

  // Convert ProductReference to JSON
  json ProductReferenceToJson(const ProductReferenceManager::ProductReference& product) {
    json j;
    j["name"] = product.name;
    j["description"] = product.description;
    j["createdDate"] = product.createdDate;
    j["lastModified"] = product.lastModified;

    // Convert points array
    j["points"] = json::array();
    for (const auto& point : product.points) {
      j["points"].push_back(Point3DToJson(point));
    }

    // Convert edges array
    j["edges"] = json::array();
    for (const auto& edge : product.edges) {
      j["edges"].push_back(EdgeToJson(edge));
    }

    // Convert datum
    j["datum"] = DatumReferenceToJson(product.datum);

    // Add metadata
    j["metadata"] = {
      {"version", "1.0"},
      {"format", "ProductReference"},
      {"pointCount", product.points.size()},
      {"edgeCount", product.edges.size()}
    };

    return j;
  }

  // Convert JSON to ProductReference
  ProductReferenceManager::ProductReference ProductReferenceFromJson(const json& j) {
    ProductReferenceManager::ProductReference product;
    product.name = j.value("name", "");
    product.description = j.value("description", "");
    product.createdDate = j.value("createdDate", "");
    product.lastModified = j.value("lastModified", "");

    // Convert points array
    if (j.contains("points") && j["points"].is_array()) {
      for (const auto& pointJson : j["points"]) {
        product.points.push_back(Point3DFromJson(pointJson));
      }
    }

    // Convert edges array
    if (j.contains("edges") && j["edges"].is_array()) {
      for (const auto& edgeJson : j["edges"]) {
        product.edges.push_back(EdgeFromJson(edgeJson));
      }
    }

    // Convert datum
    if (j.contains("datum")) {
      product.datum = DatumReferenceFromJson(j["datum"]);
    }

    return product;
  }

  // Get current timestamp as ISO string
  std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    return ss.str();
  }

  // Ensure products directory exists
  void EnsureProductsDirectoryExists() {
    const std::string productsDir = "products";
    try {
      if (!std::filesystem::exists(productsDir)) {
        std::filesystem::create_directories(productsDir);
        std::cout << "[ProductReferenceManager] Created products directory: " << productsDir << std::endl;
      }
    }
    catch (const std::exception& e) {
      std::cout << "[ProductReferenceManager] Error creating products directory: " << e.what() << std::endl;
    }
  }
}

// =============================================================================
// PUBLIC SAVE/LOAD METHODS
// =============================================================================

bool ProductReferenceManager::SaveToFile(const std::string& filename) const {
  try {
    EnsureProductsDirectoryExists();

    // Build full path
    std::string fullPath = "products/" + filename;
    if (fullPath.find(".json") == std::string::npos) {
      fullPath += ".json";
    }

    // Create master JSON with all products
    json masterJson;
    masterJson["metadata"] = {
      {"version", "1.0"},
      {"format", "ProductReferenceCollection"},
      {"productCount", m_productReferences.size()},
      {"savedAt", GetCurrentTimestamp()},
      {"application", "FWAAA Vision System"}
    };

    // Add all products
    masterJson["products"] = json::array();
    for (const auto& product : m_productReferences) {
      masterJson["products"].push_back(ProductReferenceToJson(product));
    }

    // Write to file with pretty formatting
    std::ofstream file(fullPath);
    if (!file.is_open()) {
      std::cout << "[ProductReferenceManager] Failed to open file for writing: " << fullPath << std::endl;
      return false;
    }

    file << masterJson.dump(4); // 4-space indentation for readability
    file.close();

    std::cout << "[ProductReferenceManager] Successfully saved " << m_productReferences.size()
      << " product reference(s) to: " << fullPath << std::endl;

    return true;
  }
  catch (const std::exception& e) {
    std::cout << "[ProductReferenceManager] Error saving to file: " << e.what() << std::endl;
    return false;
  }
}

bool ProductReferenceManager::LoadFromFile(const std::string& filename) {
  try {
    // Build full path
    std::string fullPath = "products/" + filename;
    if (fullPath.find(".json") == std::string::npos) {
      fullPath += ".json";
    }

    // Check if file exists
    if (!std::filesystem::exists(fullPath)) {
      std::cout << "[ProductReferenceManager] File does not exist: " << fullPath << std::endl;
      return false;
    }

    // Read file
    std::ifstream file(fullPath);
    if (!file.is_open()) {
      std::cout << "[ProductReferenceManager] Failed to open file for reading: " << fullPath << std::endl;
      return false;
    }

    json masterJson;
    file >> masterJson;
    file.close();

    // Clear existing data
    m_productReferences.clear();

    // Handle different file formats
    if (masterJson.contains("products") && masterJson["products"].is_array()) {
      // ProductReferenceCollection format
      std::cout << "[ProductReferenceManager] Loading ProductReferenceCollection format" << std::endl;

      for (const auto& productJson : masterJson["products"]) {
        try {
          ProductReference product = ProductReferenceFromJson(productJson);
          product.lastModified = GetCurrentTimestamp();
          m_productReferences.push_back(product);

          std::cout << "[ProductReferenceManager] Loaded product: " << product.name
            << " (Points: " << product.points.size()
            << ", Edges: " << product.edges.size() << ")" << std::endl;
        }
        catch (const std::exception& e) {
          std::cout << "[ProductReferenceManager] Error loading individual product: " << e.what() << std::endl;
        }
      }
    }
    else if (masterJson.contains("product")) {
      // SingleProductReference format
      std::cout << "[ProductReferenceManager] Loading SingleProductReference format" << std::endl;

      try {
        ProductReference product = ProductReferenceFromJson(masterJson["product"]);
        product.lastModified = GetCurrentTimestamp();
        m_productReferences.push_back(product);

        std::cout << "[ProductReferenceManager] Loaded product: " << product.name
          << " (Points: " << product.points.size()
          << ", Edges: " << product.edges.size() << ")" << std::endl;
      }
      catch (const std::exception& e) {
        std::cout << "[ProductReferenceManager] Error loading product: " << e.what() << std::endl;
        return false;
      }
    }
    else {
      // Legacy format - assume entire JSON is the product
      std::cout << "[ProductReferenceManager] Loading legacy format" << std::endl;

      try {
        ProductReference product = ProductReferenceFromJson(masterJson);
        product.lastModified = GetCurrentTimestamp();
        if (product.createdDate.empty()) {
          product.createdDate = product.lastModified;
        }
        m_productReferences.push_back(product);

        std::cout << "[ProductReferenceManager] Loaded product: " << product.name
          << " (Points: " << product.points.size()
          << ", Edges: " << product.edges.size() << ")" << std::endl;
      }
      catch (const std::exception& e) {
        std::cout << "[ProductReferenceManager] Error loading legacy format: " << e.what() << std::endl;
        return false;
      }
    }

    std::cout << "[ProductReferenceManager] Successfully loaded " << m_productReferences.size()
      << " product reference(s) from: " << fullPath << std::endl;

    return true;
  }
  catch (const std::exception& e) {
    std::cout << "[ProductReferenceManager] Error loading from file: " << e.what() << std::endl;
    return false;
  }
}

// =============================================================================
// ADDITIONAL CONVENIENCE METHODS
// =============================================================================

bool ProductReferenceManager::SaveProductToFile(const std::string& productName, const std::string& filename) const {
  try {
    const ProductReference* product = GetProductReference(productName);
    if (!product) {
      std::cout << "[ProductReferenceManager] Product not found: " << productName << std::endl;
      return false;
    }

    EnsureProductsDirectoryExists();

    // Build full path
    std::string fullPath = "products/" + filename;
    if (fullPath.find(".json") == std::string::npos) {
      fullPath += ".json";
    }

    // Create JSON for single product
    json productJson = ProductReferenceToJson(*product);

    // Add metadata wrapper
    json fileJson;
    fileJson["metadata"] = {
      {"version", "1.0"},
      {"format", "SingleProductReference"},
      {"savedAt", GetCurrentTimestamp()},
      {"application", "FWAAA Vision System"}
    };
    fileJson["product"] = productJson;

    // Write to file
    std::ofstream file(fullPath);
    if (!file.is_open()) {
      std::cout << "[ProductReferenceManager] Failed to open file for writing: " << fullPath << std::endl;
      return false;
    }

    file << fileJson.dump(4);
    file.close();

    std::cout << "[ProductReferenceManager] Successfully saved product '" << productName
      << "' to: " << fullPath << std::endl;

    return true;
  }
  catch (const std::exception& e) {
    std::cout << "[ProductReferenceManager] Error saving product to file: " << e.what() << std::endl;
    return false;
  }
}

bool ProductReferenceManager::LoadProductFromFile(const std::string& filename) {
  try {
    // Build full path
    std::string fullPath = "products/" + filename;
    if (fullPath.find(".json") == std::string::npos) {
      fullPath += ".json";
    }

    // Check if file exists
    if (!std::filesystem::exists(fullPath)) {
      std::cout << "[ProductReferenceManager] File does not exist: " << fullPath << std::endl;
      return false;
    }

    // Read file
    std::ifstream file(fullPath);
    if (!file.is_open()) {
      std::cout << "[ProductReferenceManager] Failed to open file for reading: " << fullPath << std::endl;
      return false;
    }

    json fileJson;
    file >> fileJson;
    file.close();

    ProductReference product;

    // Handle both single product and collection formats
    if (fileJson.contains("product")) {
      // Single product format
      product = ProductReferenceFromJson(fileJson["product"]);
    }
    else if (fileJson.contains("products") && fileJson["products"].is_array() && !fileJson["products"].empty()) {
      // Collection format - take first product
      product = ProductReferenceFromJson(fileJson["products"][0]);
    }
    else {
      // Legacy format - assume entire JSON is the product
      product = ProductReferenceFromJson(fileJson);
    }

    // Check if product already exists
    if (HasProductReference(product.name)) {
      std::cout << "[ProductReferenceManager] Product '" << product.name
        << "' already exists. Use a different name or delete existing product first." << std::endl;
      return false;
    }

    // Update timestamps
    product.lastModified = GetCurrentTimestamp();
    if (product.createdDate.empty()) {
      product.createdDate = product.lastModified;
    }

    // Add to collection
    m_productReferences.push_back(product);

    std::cout << "[ProductReferenceManager] Successfully loaded product: " << product.name
      << " (Points: " << product.points.size()
      << ", Edges: " << product.edges.size() << ")" << std::endl;

    return true;
  }
  catch (const std::exception& e) {
    std::cout << "[ProductReferenceManager] Error loading product from file: " << e.what() << std::endl;
    return false;
  }
}

bool ProductReferenceManager::AutoSave() const {
  // Auto-save all products with timestamp
  std::string filename = "products_autosave_" + GetCurrentTimestamp();
  // Replace colons with underscores for Windows compatibility
  std::replace(filename.begin(), filename.end(), ':', '_');
  return SaveToFile(filename);
}

std::vector<std::string> ProductReferenceManager::GetAvailableProductFiles() const {
  std::vector<std::string> files;

  const std::string productsDir = "products";
  if (!std::filesystem::exists(productsDir)) {
    return files;
  }

  try {
    for (const auto& entry : std::filesystem::directory_iterator(productsDir)) {
      if (entry.is_regular_file() && entry.path().extension() == ".json") {
        files.push_back(entry.path().filename().string());
      }
    }

    // Sort alphabetically
    std::sort(files.begin(), files.end());
  }
  catch (const std::exception& e) {
    std::cout << "[ProductReferenceManager] Error scanning products directory: " << e.what() << std::endl;
  }

  return files;
}