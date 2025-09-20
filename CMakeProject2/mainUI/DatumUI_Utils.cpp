// DatumUI_Utils.cpp - Helper functions and utilities
#include "DatumUI.h"
#include <cstring>
#include <algorithm>
#include <cctype>

// =============================================================================
// UI HELPER METHODS
// =============================================================================

void DatumUI::ShowErrorMessage(const std::string& message, float duration) {
  m_errorMessage = message;
  m_errorDisplayTime = duration;
}

void DatumUI::ShowSuccessMessage(const std::string& message, float duration) {
  m_successMessage = message;
  m_successDisplayTime = duration;
}

void DatumUI::UpdateMessageDisplays() {
  // Update error message timer
  if (m_errorDisplayTime > 0.0f) {
    m_errorDisplayTime -= ImGui::GetIO().DeltaTime;
    if (m_errorDisplayTime <= 0.0f) {
      m_errorMessage.clear();
    }
  }

  // Update success message timer
  if (m_successDisplayTime > 0.0f) {
    m_successDisplayTime -= ImGui::GetIO().DeltaTime;
    if (m_successDisplayTime <= 0.0f) {
      m_successMessage.clear();
    }
  }
}

void DatumUI::ClearInputBuffers() {
  // Clear string buffers
  memset(m_newProductNameBuffer, 0, sizeof(m_newProductNameBuffer));
  memset(m_newProductDescBuffer, 0, sizeof(m_newProductDescBuffer));
  memset(m_pointNameBuffer, 0, sizeof(m_pointNameBuffer));
  memset(m_pointDescBuffer, 0, sizeof(m_pointDescBuffer));

  // Reset coordinate inputs
  m_pointCoords[0] = 0.0f;
  m_pointCoords[1] = 0.0f;
  m_pointCoords[2] = 0.0f;
}

// 3. ADD THIS ENHANCED ResetDatumCreationState() METHOD:
void DatumUI::ResetDatumCreationState() {
  m_datumMethod = DatumCreationMethod::NONE;
  m_datumOriginPoint.clear();
  m_datumPoint2.clear();
  m_datumPoint3.clear();
  m_useFirstEdgeAsX = true;
}

bool DatumUI::ValidatePointName(const std::string& name, std::string& errorMsg) const {
  // Check if name is empty
  if (name.empty()) {
    errorMsg = "Point name cannot be empty";
    return false;
  }

  // Check name length
  if (name.length() > 50) {
    errorMsg = "Point name too long (max 50 characters)";
    return false;
  }

  // Check for invalid characters (basic validation)
  for (char c : name) {
    if (!std::isalnum(c) && c != '_' && c != '-' && c != ' ') {
      errorMsg = "Point name contains invalid characters (use letters, numbers, _, -, space)";
      return false;
    }
  }

  // Check if name starts or ends with whitespace
  if (name.front() == ' ' || name.back() == ' ') {
    errorMsg = "Point name cannot start or end with spaces";
    return false;
  }

  // Check for uniqueness within the selected product
  if (!m_selectedProductName.empty()) {
    if (!m_referenceManager->IsPointNameUnique(m_selectedProductName, name)) {
      errorMsg = "Point name already exists in this product";
      return false;
    }
  }

  errorMsg.clear();
  return true;
}

bool DatumUI::ValidateProductName(const std::string& name, std::string& errorMsg) const {
  // Check if name is empty
  if (name.empty()) {
    errorMsg = "Product name cannot be empty";
    return false;
  }

  // Check name length
  if (name.length() > 100) {
    errorMsg = "Product name too long (max 100 characters)";
    return false;
  }

  // Check for invalid characters (basic validation)
  for (char c : name) {
    if (!std::isalnum(c) && c != '_' && c != '-' && c != ' ') {
      errorMsg = "Product name contains invalid characters (use letters, numbers, _, -, space)";
      return false;
    }
  }

  // Check if name starts or ends with whitespace
  if (name.front() == ' ' || name.back() == ' ') {
    errorMsg = "Product name cannot start or end with spaces";
    return false;
  }

  // Check for uniqueness
  if (m_referenceManager->HasProductReference(name)) {
    errorMsg = "Product name already exists";
    return false;
  }

  errorMsg.clear();
  return true;
}