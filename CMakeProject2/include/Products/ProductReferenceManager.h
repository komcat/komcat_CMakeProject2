// ProductReferenceManager.h - Core logic for managing product references and datum systems
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <array>      // Added for CalculateVector return type
#include <fstream>    // Added for ID counter file operations
#include <iomanip>    // Added for ID formatting
#include <sstream>    // Added for ID formatting
#include <algorithm>  // Added for max() in ID generation
#include "nlohmann/json.hpp"

/**
 * @brief Core logic class for managing product references and datum coordinate systems
 *
 * This class handles all the mathematical calculations and data management for:
 * - Product references containing multiple datum systems
 * - Point and edge management
 * - Coordinate system calculations from 2-point or 3-point datum creation
 * - JSON persistence in /products folder
 * - Validation and error checking
 * - Automatic sequential ID generation
 */
class ProductReferenceManager {
public:
  // =============================================================================
  // DATA STRUCTURES
  // =============================================================================

  /**
   * @brief 3D Point with name identifier and action type
   */
  struct Point3D {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    std::string name;
    std::string description;
    bool isValid = false;  // Has user entered coordinates

    // Action type for the point
    enum class ActionType {
      FIDUCIAL,    // Reference points for datum construction
      INSPECTION,  // Points for vision inspection
      DISPENSE,    // Points for material dispensing
      PICK,        // Points for picking operations
      PLACE        // Points for placing operations
    } actionType = ActionType::FIDUCIAL;

    Point3D() = default;
    Point3D(const std::string& pointName, double px = 0.0, double py = 0.0, double pz = 0.0, ActionType action = ActionType::FIDUCIAL)
      : x(px), y(py), z(pz), name(pointName), isValid(true), actionType(action) {
    }

    // Helper method to get action type as string
    std::string GetActionTypeString() const {
      switch (actionType) {
      case ActionType::FIDUCIAL:   return "Fiducial";
      case ActionType::INSPECTION: return "Inspection";
      case ActionType::DISPENSE:   return "Dispense";
      case ActionType::PICK:       return "Pick";
      case ActionType::PLACE:      return "Place";
      default:                     return "Unknown";
      }
    }

    // Helper method to set action type from string
    bool SetActionTypeFromString(const std::string& actionStr) {
      if (actionStr == "Fiducial") { actionType = ActionType::FIDUCIAL; return true; }
      if (actionStr == "Inspection") { actionType = ActionType::INSPECTION; return true; }
      if (actionStr == "Dispense") { actionType = ActionType::DISPENSE; return true; }
      if (actionStr == "Pick") { actionType = ActionType::PICK; return true; }
      if (actionStr == "Place") { actionType = ActionType::PLACE; return true; }
      return false; // Invalid action type
    }
  };

  /**
   * @brief Edge connecting two points
   */
  struct Edge {
    std::string point1Name;  // Start point
    std::string point2Name;  // End point
    std::string name;        // Edge identifier
    std::string description;

    Edge() = default;
    Edge(const std::string& edgeName, const std::string& p1, const std::string& p2)
      : name(edgeName), point1Name(p1), point2Name(p2) {
    }
  };

  /**
   * @brief Datum reference coordinate system
   */
  struct DatumReference {
    std::string name;
    std::string description;

    // Origin point
    std::string originPointName;

    // Axis definitions
    std::string xAxisEdgeName;  // Edge defining X-axis
    std::string yAxisEdgeName;  // Edge defining Y-axis (optional for 2-point system)

    // System type
    enum class ConstructionMethod {
      NONE,
      TWO_POINT,   // Origin + 1 point → X-axis only
      THREE_POINT  // Origin + 2 points → X and Y axes
    } constructionMethod = ConstructionMethod::NONE;

    bool hasZAxis = false;  // Whether Z-axis is explicitly defined

    DatumReference() = default;
    DatumReference(const std::string& datumName) : name(datumName) {}
  };

  /**
   * @brief Complete product reference containing datum and geometry
   */
  struct ProductReference {
    std::string name;
    std::string description;
    std::string id;              // NEW: Auto-generated sequential ID (PROD_000001, etc.)

    // Geometry data
    std::vector<Point3D> points;
    std::vector<Edge> edges;

    // Datum system (default one per product)
    DatumReference datum;

    // Creation timestamp
    std::string createdDate;
    std::string lastModified;

    ProductReference() = default;
    ProductReference(const std::string& productName) : name(productName) {
      datum.name = productName + "_Datum";
    }
  };

  // =============================================================================
  // CONSTRUCTION & LIFECYCLE
  // =============================================================================

  ProductReferenceManager();
  ~ProductReferenceManager();

  // =============================================================================
  // PRODUCT REFERENCE MANAGEMENT
  // =============================================================================

  /**
   * @brief Create a new product reference
   */
  bool CreateProductReference(const std::string& name, const std::string& description = "");

  /**
   * @brief Delete a product reference
   */
  bool DeleteProductReference(const std::string& name);

  /**
   * @brief Get all product references
   */
  const std::vector<ProductReference>& GetAllProductReferences() const { return m_productReferences; }

  /**
   * @brief Get specific product reference
   */
  ProductReference* GetProductReference(const std::string& name);
  const ProductReference* GetProductReference(const std::string& name) const;

  /**
   * @brief Check if product reference exists
   */
  bool HasProductReference(const std::string& name) const;

  // =============================================================================
  // POINT MANAGEMENT
  // =============================================================================

  /**
   * @brief Add point to product reference with action type
   */
  bool AddPoint(const std::string& productName, const Point3D& point);

  /**
   * @brief Add point to product reference with coordinates and action type
   */
  bool AddPoint(const std::string& productName, const std::string& pointName,
    double x = 0.0, double y = 0.0, double z = 0.0,
    Point3D::ActionType actionType = Point3D::ActionType::FIDUCIAL,
    const std::string& description = "");

  /**
   * @brief Update point coordinates (keeps existing action type)
   */
  bool UpdatePoint(const std::string& productName, const std::string& pointName,
    double newX, double newY, double newZ);

  /**
   * @brief Update point coordinates and action type
   */
  bool UpdatePointWithAction(const std::string& productName, const std::string& pointName,
    double newX, double newY, double newZ, Point3D::ActionType actionType);

  /**
   * @brief Update only point action type
   */
  bool UpdatePointActionType(const std::string& productName, const std::string& pointName,
    Point3D::ActionType actionType);

  /**
   * @brief Get points filtered by action type
   */
  std::vector<Point3D*> GetPointsByActionType(const std::string& productName, Point3D::ActionType actionType);
  std::vector<const Point3D*> GetPointsByActionType(const std::string& productName, Point3D::ActionType actionType) const;

  /**
   * @brief Remove point from product reference
   */
  bool RemovePoint(const std::string& productName, const std::string& pointName);

  /**
   * @brief Get point from product reference
   */
  Point3D* GetPoint(const std::string& productName, const std::string& pointName);
  const Point3D* GetPoint(const std::string& productName, const std::string& pointName) const;

  // =============================================================================
  // EDGE MANAGEMENT  
  // =============================================================================

  /**
   * @brief Add edge between two points
   */
  bool AddEdge(const std::string& productName, const Edge& edge);

  /**
   * @brief Remove edge from product reference
   */
  bool RemoveEdge(const std::string& productName, const std::string& edgeName);

  /**
   * @brief Get edge from product reference
   */
  Edge* GetEdge(const std::string& productName, const std::string& edgeName);
  const Edge* GetEdge(const std::string& productName, const std::string& edgeName) const;

  /**
   * @brief Auto-generate edge name from two points
   */
  std::string GenerateEdgeName(const std::string& point1, const std::string& point2) const;

  // =============================================================================
  // DATUM SYSTEM CREATION
  // =============================================================================

  /**
   * @brief Create datum using 2-point system (origin + 1 point = X-axis)
   * @param productName Target product reference
   * @param originPointName Point to use as origin
   * @param xAxisPointName Second point to define X-axis direction
   * @return true if datum was created successfully
   */
  bool CreateDatumFrom2Points(const std::string& productName,
    const std::string& originPointName,
    const std::string& xAxisPointName);

  /**
   * @brief Create datum using 3-point system (origin + 2 points = X and Y axes)
   * @param productName Target product reference
   * @param originPointName Point to use as origin
   * @param point2Name Second point for first edge
   * @param point3Name Third point for second edge
   * @param useFirstEdgeAsX If true, edge origin->point2 becomes X-axis, otherwise Y-axis
   * @return true if datum was created successfully
   */
  bool CreateDatumFrom3Points(const std::string& productName,
    const std::string& originPointName,
    const std::string& point2Name,
    const std::string& point3Name,
    bool useFirstEdgeAsX = true);

  /**
   * @brief Clear datum from product reference
   */
  bool ClearDatum(const std::string& productName);

  // =============================================================================
  // COORDINATE SYSTEM CALCULATIONS
  // =============================================================================

  /**
   * @brief Calculate edge length between two points
   */
  std::optional<double> CalculateEdgeLength(const std::string& productName,
    const std::string& edgeName) const;

  /**
   * @brief Calculate angle between two edges (in degrees)
   */
  std::optional<double> CalculateAngleBetweenEdges(const std::string& productName,
    const std::string& edge1Name,
    const std::string& edge2Name) const;

  /**
   * @brief Validate that datum system is mathematically valid
   */
  bool ValidateDatumSystem(const std::string& productName, std::string& errorMessage) const;

  // =============================================================================
  // DATA PERSISTENCE - JSON Support
  // =============================================================================

  /**
   * @brief Save all product references to JSON file in /products folder
   * @param filename Name of file (without path, .json extension optional)
   * @return true if saved successfully
   */
  bool SaveToFile(const std::string& filename) const;

  /**
   * @brief Load product references from JSON file in /products folder
   * @param filename Name of file (without path, .json extension optional)
   * @return true if loaded successfully
   */
  bool LoadFromFile(const std::string& filename);

  /**
   * @brief Save single product reference to separate JSON file
   * @param productName Name of product to save
   * @param filename Name of file (without path, .json extension optional)
   * @return true if saved successfully
   */
  bool SaveProductToFile(const std::string& productName, const std::string& filename) const;

  /**
   * @brief Load single product reference from JSON file and add to collection
   * @param filename Name of file (without path, .json extension optional)
   * @return true if loaded successfully
   */
  bool LoadProductFromFile(const std::string& filename);

  /**
   * @brief Auto-save all products with timestamp
   * @return true if saved successfully
   */
  bool AutoSave() const;

  /**
   * @brief Get list of available JSON files in /products folder
   * @return Vector of filenames
   */
  std::vector<std::string> GetAvailableProductFiles() const;

  // =============================================================================
  // VALIDATION & ERROR CHECKING
  // =============================================================================

  /**
   * @brief Validate product reference data integrity
   */
  bool ValidateProductReference(const std::string& productName, std::string& errorMessage) const;

  /**
   * @brief Check if point name is unique within product
   */
  bool IsPointNameUnique(const std::string& productName, const std::string& pointName) const;

  /**
   * @brief Check if edge name is unique within product
   */
  bool IsEdgeNameUnique(const std::string& productName, const std::string& edgeName) const;

private:
  // =============================================================================
  // PRIVATE MEMBERS
  // =============================================================================

  /// Storage for all product references
  std::vector<ProductReference> m_productReferences;

  /// Static constant for ID counter file
  static const std::string ID_COUNTER_FILE;

  // =============================================================================
  // PRIVATE HELPER METHODS
  // =============================================================================

  /**
   * @brief Find product reference by name (non-const)
   */
  ProductReference* FindProductReference(const std::string& name);

  /**
   * @brief Find product reference by name (const)
   */
  const ProductReference* FindProductReference(const std::string& name) const;

  /**
   * @brief Auto-generate unique point name
   */
  std::string GenerateUniquePointName(const std::string& productName, const std::string& baseName = "Point") const;

  /**
   * @brief Auto-generate unique edge name
   */
  std::string GenerateUniqueEdgeName(const std::string& productName, const std::string& baseName = "Edge") const;

  /**
   * @brief Validate that two points are not the same (for edge creation)
   */
  bool ValidateEdgePoints(const std::string& productName,
    const std::string& point1, const std::string& point2,
    std::string& errorMessage) const;

  /**
   * @brief Calculate vector between two points
   */
  std::optional<std::array<double, 3>> CalculateVector(const Point3D& from, const Point3D& to) const;

  /**
   * @brief Get current timestamp string
   */
  std::string GetCurrentTimestamp() const;

  // =============================================================================
  // NEW: ID GENERATION METHODS
  // =============================================================================

  /**
   * @brief Load next product ID from counter file
   */
  static int LoadNextProductId();

  /**
   * @brief Save next product ID to counter file
   */
  static void SaveNextProductId(int nextId);

  /**
   * @brief Generate formatted product ID (PROD_000001 format)
   */
  static std::string GenerateProductId(int idNumber);

  /**
   * @brief Helper method to load product from JSON with ID handling
   */
  void LoadProductFromJson(const nlohmann::json& productJson);
};