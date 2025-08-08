// DatumUI.h - Pure UI class for Datum Reference interface
#pragma once

#include "ProductReferenceManager.h"
#include "imgui.h"
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Pure UI class for Datum Reference management interface
 *
 * This class provides the user interface for:
 * - Creating and managing product references
 * - Setting up datum coordinate systems using 2-point or 3-point methods
 * - Entering point coordinates (XYZ)
 * - Displaying lists of points and edges
 * - Visual graphical display of the coordinate system
 *
 * Follows the UI pattern established by UIVisionPanel/UIConfigVisualizer
 */
class DatumUI {
public:
  // =============================================================================
  // CONSTRUCTION & LIFECYCLE
  // =============================================================================

  DatumUI();
  ~DatumUI();

  // Disable copy/move to avoid issues with references
  DatumUI(const DatumUI&) = delete;
  DatumUI& operator=(const DatumUI&) = delete;
  DatumUI(DatumUI&&) = delete;
  DatumUI& operator=(DatumUI&&) = delete;

  // =============================================================================
  // MAIN UI INTERFACE
  // =============================================================================

  /**
   * @brief Main UI rendering method
   */
  void RenderUI();

  /**
   * @brief Toggle window visibility
   */
  void ToggleWindow() { m_showWindow = !m_showWindow; }

  /**
   * @brief Check if window is visible
   */
  bool IsVisible() const { return m_showWindow; }

  /**
   * @brief Set window visibility
   */
  void SetVisible(bool visible) { m_showWindow = visible; }

private:
  // =============================================================================
  // MEMBER VARIABLES
  // =============================================================================

  /// Core logic manager
  std::unique_ptr<ProductReferenceManager> m_referenceManager;

  /// Window visibility
  bool m_showWindow = true;

  /// Currently selected product reference
  std::string m_selectedProductName;

  /// Currently selected point for editing
  std::string m_selectedPointName;

  /// Currently selected edge for editing
  std::string m_selectedEdgeName;

  // =============================================================================
  // UI STATE - NEW PRODUCT CREATION
  // =============================================================================

  /// Show new product dialog
  bool m_showNewProductDialog = false;

  /// Input buffers for new product
  char m_newProductNameBuffer[256] = "";
  char m_newProductDescBuffer[512] = "";

  // =============================================================================
  // UI STATE - POINT MANAGEMENT
  // =============================================================================

  /// Show add point dialog
  bool m_showAddPointDialog = false;

  /// Show edit point dialog  
  bool m_showEditPointDialog = false;

  /// Show change action type dialog
  bool m_showChangeActionDialog = false;

  /// Input buffers for point data
  char m_pointNameBuffer[256] = "";
  char m_pointDescBuffer[512] = "";
  float m_pointCoords[3] = { 0.0f, 0.0f, 0.0f };  // X, Y, Z

  // =============================================================================
  // UI STATE - DATUM CREATION
  // =============================================================================

  /// Show datum creation dialog
  bool m_showDatumCreationDialog = false;

  /// Datum creation method selection
  enum class DatumCreationMethod {
    NONE,
    TWO_POINT,   // Origin + 1 point → X-axis
    THREE_POINT  // Origin + 2 points → X and Y axes
  } m_datumMethod = DatumCreationMethod::NONE;

  /// Selected points for datum creation
  std::string m_datumOriginPoint;
  std::string m_datumPoint2;
  std::string m_datumPoint3;

  /// For 3-point method: which edge should be X-axis
  bool m_useFirstEdgeAsX = true;

  // =============================================================================
  // UI STATE - VISUAL DISPLAY
  // =============================================================================

  /// Show visual display panel
  bool m_showVisualDisplay = true;

  /// Visual display settings
  float m_visualScale = 50.0f;
  float m_visualOffsetX = 0.0f;
  float m_visualOffsetY = 0.0f;
  bool m_showPointLabels = true;
  bool m_showEdgeLabels = true;
  bool m_showAxes = true;
  bool m_showGrid = true;

  // =============================================================================
  // UI STATE - CONFIRMATIONS & ERRORS
  // =============================================================================

  /// Show delete confirmation dialog
  bool m_showDeleteConfirmDialog = false;

  /// Item to delete (product/point/edge name)
  std::string m_itemToDelete;

  /// Delete type
  enum class DeleteType {
    NONE,
    PRODUCT,
    POINT,
    EDGE
  } m_deleteType = DeleteType::NONE;

  /// Error message display
  std::string m_errorMessage;
  float m_errorDisplayTime = 0.0f;

  /// Success message display
  std::string m_successMessage;
  float m_successDisplayTime = 0.0f;

  // =============================================================================
  // MAIN PANEL RENDERING METHODS
  // =============================================================================

  /**
   * @brief Render left panel with product list and controls
   */
  void RenderLeftPanel();

  /**
   * @brief Render middle panel with point/edge lists and editing
   */
  void RenderMiddlePanel();

  /**
   * @brief Render right panel with visual display
   */
  void RenderRightPanel();

  // =============================================================================
  // LEFT PANEL COMPONENTS
  // =============================================================================

  /**
   * @brief Render product reference list
   */
  void RenderProductList();

  /**
   * @brief Render product creation controls
   */
  void RenderProductCreationControls();

  /**
   * @brief Render selected product info
   */
  void RenderSelectedProductInfo();

  /**
   * @brief Render save/load controls
   */
  void RenderSaveLoadControls();

  // =============================================================================
  // MIDDLE PANEL COMPONENTS
  // =============================================================================

  /**
   * @brief Render point list and management
   */
  void RenderPointList();

  /**
   * @brief Render edge list and management
   */
  void RenderEdgeList();

  /**
   * @brief Render datum creation controls
   */
  void RenderDatumCreationControls();

  /**
   * @brief Render current datum system info
   */
  void RenderCurrentDatumInfo();

  // =============================================================================
  // RIGHT PANEL COMPONENTS
  // =============================================================================

  /**
   * @brief Render visual 2D display of coordinate system
   */
  void RenderVisualDisplay();

  /**
   * @brief Render visual display controls (zoom, pan, options)
   */
  void RenderVisualDisplayControls();

  // =============================================================================
  // DIALOG RENDERING METHODS
  // =============================================================================

  /**
   * @brief Render new product creation dialog
   */
  void RenderNewProductDialog();

  /**
   * @brief Render add point dialog
   */
  void RenderAddPointDialog();

  /**
   * @brief Render edit point dialog
   */
  void RenderEditPointDialog();

  /**
   * @brief Render change action type dialog
   */
  void RenderChangeActionTypeDialog();

  /**
   * @brief Render datum creation dialog
   */
  void RenderDatumCreationDialog();

  /**
   * @brief Render delete confirmation dialog
   */
  void RenderDeleteConfirmDialog();

  // =============================================================================
  // VISUAL DISPLAY HELPER METHODS
  // =============================================================================

  /**
   * @brief Draw point in visual display
   */
  void DrawPoint(ImDrawList* drawList, const ProductReferenceManager::Point3D& point,
    ImVec2 canvasPos, ImVec2 canvasSize, ImU32 color);

  /**
   * @brief Draw edge in visual display
   */
  void DrawEdge(ImDrawList* drawList,
    const ProductReferenceManager::Point3D& point1,
    const ProductReferenceManager::Point3D& point2,
    ImVec2 canvasPos, ImVec2 canvasSize, ImU32 color, float thickness = 2.0f);

  /**
   * @brief Draw coordinate axes
   */
  void DrawCoordinateAxes(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize);

  /**
   * @brief Draw grid background
   */
  void DrawGrid(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize);

  /**
   * @brief Convert 3D point to 2D display coordinates
   */
  ImVec2 WorldToScreen(const ProductReferenceManager::Point3D& point,
    ImVec2 canvasPos, ImVec2 canvasSize);

  /**
   * @brief Auto-zoom to fit all points in view
   */
  void AutoZoomToFitPoints();

  /**
   * @brief Get color for action type display
   */
  ImVec4 GetActionTypeColor(ProductReferenceManager::Point3D::ActionType actionType) const;

  // =============================================================================
  // UI HELPER METHODS
  // =============================================================================

  /**
   * @brief Show error message for specified duration
   */
  void ShowErrorMessage(const std::string& message, float duration = 3.0f);

  /**
   * @brief Show success message for specified duration
   */
  void ShowSuccessMessage(const std::string& message, float duration = 2.0f);

  /**
   * @brief Update and render message displays
   */
  void UpdateMessageDisplays();

  /**
   * @brief Clear all input buffers
   */
  void ClearInputBuffers();

  /**
   * @brief Reset datum creation state
   */
  void ResetDatumCreationState();

  /**
   * @brief Validate point name input
   */
  bool ValidatePointName(const std::string& name, std::string& errorMsg) const;

  /**
   * @brief Validate product name input
   */
  bool ValidateProductName(const std::string& name, std::string& errorMsg) const;

  // =============================================================================
  // EVENT HANDLERS
  // =============================================================================

  /**
   * @brief Handle product selection change
   */
  void OnProductSelectionChanged(const std::string& productName);

  /**
   * @brief Handle point selection change
   */
  void OnPointSelectionChanged(const std::string& pointName);

  /**
   * @brief Handle edge selection change
   */
  void OnEdgeSelectionChanged(const std::string& edgeName);

  /**
   * @brief Handle create new product button
   */
  void OnCreateNewProduct();

  /**
   * @brief Handle add point button
   */
  void OnAddPoint();

  /**
   * @brief Handle edit point button
   */
  void OnEditPoint();

  /**
   * @brief Handle delete point button
   */
  void OnDeletePoint();

  /**
   * @brief Handle create datum button
   */
  void OnCreateDatum();

  /**
   * @brief Handle clear datum button
   */
  void OnClearDatum();
};