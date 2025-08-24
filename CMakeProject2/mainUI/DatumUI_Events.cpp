// DatumUI_Events.cpp - Event handlers for user interactions
#include "DatumUI.h"
#include <cstring>
#include <iostream>

// =============================================================================
// EVENT HANDLERS
// =============================================================================

void DatumUI::OnProductSelectionChanged(const std::string& productName) {
  m_selectedProductName = productName;

  // Clear point and edge selections when switching products
  m_selectedPointName.clear();
  m_selectedEdgeName.clear();

  // Reset visual display to center on new product
  m_visualScale = 1.0f;
  m_visualOffsetX = 0.0f;
  m_visualOffsetY = 0.0f;
}

void DatumUI::OnPointSelectionChanged(const std::string& pointName) {
  m_selectedPointName = pointName;
}

void DatumUI::OnEdgeSelectionChanged(const std::string& edgeName) {
  m_selectedEdgeName = edgeName;
}

void DatumUI::OnCreateNewProduct() {
  // Clear input buffers and show dialog
  ClearInputBuffers();
  m_showNewProductDialog = true;
}

void DatumUI::OnAddPoint() {
  if (m_selectedProductName.empty()) {
    ShowErrorMessage("No product selected");
    return;
  }

  // Clear input buffers and show dialog
  ClearInputBuffers();
  m_showAddPointDialog = true;
}

void DatumUI::OnEditPoint() {
  if (m_selectedProductName.empty()) {
    ShowErrorMessage("No product selected");
    return;
  }

  if (m_selectedPointName.empty()) {
    ShowErrorMessage("No point selected");
    return;
  }

  // Get current point data to populate dialog
  const auto* point = m_referenceManager->GetPoint(m_selectedProductName, m_selectedPointName);
  if (!point) {
    ShowErrorMessage("Selected point not found");
    return;
  }

  // For MSVC compiler
  strncpy_s(m_pointNameBuffer, sizeof(m_pointNameBuffer), point->name.c_str(), _TRUNCATE);
  strncpy_s(m_pointDescBuffer, sizeof(m_pointDescBuffer), point->description.c_str(), _TRUNCATE);

  if (point->isValid) {
    m_pointCoords[0] = static_cast<float>(point->x);
    m_pointCoords[1] = static_cast<float>(point->y);
    m_pointCoords[2] = static_cast<float>(point->z);
  }
  else {
    m_pointCoords[0] = 0.0f;
    m_pointCoords[1] = 0.0f;
    m_pointCoords[2] = 0.0f;
  }

  m_showEditPointDialog = true;
}

void DatumUI::OnDeletePoint() {
  if (m_selectedProductName.empty()) {
    ShowErrorMessage("No product selected");
    return;
  }

  if (m_selectedPointName.empty()) {
    ShowErrorMessage("No point selected");
    return;
  }

  // Set up delete confirmation dialog
  m_itemToDelete = m_selectedPointName;
  m_deleteType = DeleteType::POINT;
  m_showDeleteConfirmDialog = true;
}

// 1. UPDATE YOUR OnCreateDatum() METHOD:
void DatumUI::OnCreateDatum() {
  if (m_selectedProductName.empty() || m_datumOriginPoint.empty() || m_datumPoint2.empty()) {
    ShowErrorMessage("Missing required points for datum creation");
    return;
  }

  bool success = false;

  if (m_datumMethod == DatumCreationMethod::TWO_POINT) {
    // Create 2-point datum system
    success = m_referenceManager->CreateDatumFrom2Points(
      m_selectedProductName,
      m_datumOriginPoint,
      m_datumPoint2
    );

    if (success) {
      ShowSuccessMessage("2-Point datum system created successfully!");
    }
  }
  else if (m_datumMethod == DatumCreationMethod::THREE_POINT) {
    if (m_datumPoint3.empty()) {
      ShowErrorMessage("Y-axis point required for 3-point datum");
      return;
    }

    // Create 3-point datum system
    success = m_referenceManager->CreateDatumFrom3Points(
      m_selectedProductName,
      m_datumOriginPoint,
      m_datumPoint2,
      m_datumPoint3,
      m_useFirstEdgeAsX
    );

    if (success) {
      ShowSuccessMessage("3-Point datum system created successfully!");
    }
  }

  if (success) {
    // Reset the datum creation state
    ResetDatumCreationState();

    // Force UI refresh by clearing selections
    m_selectedEdgeName.clear();

    // Log the created edges for debugging
    const auto* product = m_referenceManager->GetProductReference(m_selectedProductName);
    if (product) {
      std::cout << "[DatumUI] Datum created. Product now has " << product->edges.size() << " edges:" << std::endl;
      for (const auto& edge : product->edges) {
        std::cout << "  - " << edge.name << " (" << edge.point1Name << " -> " << edge.point2Name << ")" << std::endl;
      }
    }
  }
  else {
    ShowErrorMessage("Failed to create datum system");
  }
}



// 2. UPDATE YOUR OnClearDatum() METHOD:
void DatumUI::OnClearDatum() {
  if (m_selectedProductName.empty()) {
    return;
  }

  if (m_referenceManager->ClearDatum(m_selectedProductName)) {
    ShowSuccessMessage("Datum system cleared");

    // Reset UI state
    ResetDatumCreationState();
    m_selectedEdgeName.clear();

    // Log for debugging
    const auto* product = m_referenceManager->GetProductReference(m_selectedProductName);
    if (product) {
      std::cout << "[DatumUI] Datum cleared. Product now has " << product->edges.size() << " edges" << std::endl;
    }
  }
  else {
    ShowErrorMessage("Failed to clear datum system");
  }
}