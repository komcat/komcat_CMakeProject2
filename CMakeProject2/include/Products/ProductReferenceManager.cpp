// ProductReferenceManager.cpp - Main coordination and core functions
#include "ProductReferenceManager.h"
#include <algorithm>
#include <iostream>

// Define M_PI if not already defined
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =============================================================================
// CONSTRUCTION & LIFECYCLE
// =============================================================================

ProductReferenceManager::ProductReferenceManager() {
  // Initialize with empty state
}

ProductReferenceManager::~ProductReferenceManager() {
  // Cleanup handled by RAII
}

// =============================================================================
// PRIVATE HELPER METHODS (Keep only the core helpers here)
// =============================================================================

ProductReferenceManager::ProductReference* ProductReferenceManager::FindProductReference(const std::string& name) {
  auto it = std::find_if(m_productReferences.begin(), m_productReferences.end(),
    [&name](const ProductReference& product) {
    return product.name == name;
  });
  return (it != m_productReferences.end()) ? &(*it) : nullptr;
}

const ProductReferenceManager::ProductReference* ProductReferenceManager::FindProductReference(const std::string& name) const {
  auto it = std::find_if(m_productReferences.begin(), m_productReferences.end(),
    [&name](const ProductReference& product) {
    return product.name == name;
  });
  return (it != m_productReferences.end()) ? &(*it) : nullptr;
}