// ProductReferenceManager_Products.cpp - Product CRUD operations
#include "ProductReferenceManager.h"
#include <algorithm>
#include <iostream>

// =============================================================================
// CORE ACCESSOR METHODS
// =============================================================================

//const std::vector<ProductReferenceManager::ProductReference>& ProductReferenceManager::GetAllProductReferences() const {
//  return m_productReferences;
//}

ProductReferenceManager::ProductReference* ProductReferenceManager::GetProductReference(const std::string& name) {
  return FindProductReference(name);
}

const ProductReferenceManager::ProductReference* ProductReferenceManager::GetProductReference(const std::string& name) const {
  return FindProductReference(name);
}

bool ProductReferenceManager::HasProductReference(const std::string& name) const {
  return FindProductReference(name) != nullptr;
}

// =============================================================================
// PRODUCT REFERENCE MANAGEMENT
// =============================================================================

bool ProductReferenceManager::CreateProductReference(const std::string& name, const std::string& description) {
  if (name.empty()) {
    std::cout << "CreateProductReference: Product name cannot be empty" << std::endl;
    return false;
  }

  // Check if product already exists
  if (HasProductReference(name)) {
    std::cout << "CreateProductReference: Product already exists: " << name << std::endl;
    return false;
  }

  // Create new product reference
  ProductReference product(name);
  product.description = description;
  product.createdDate = GetCurrentTimestamp();
  product.lastModified = product.createdDate;

  m_productReferences.push_back(std::move(product));
  std::cout << "CreateProductReference: Product created successfully: " << name << std::endl;
  return true;
}

bool ProductReferenceManager::DeleteProductReference(const std::string& name) {
  auto it = std::find_if(m_productReferences.begin(), m_productReferences.end(),
    [&name](const ProductReference& product) {
    return product.name == name;
  });

  if (it != m_productReferences.end()) {
    std::cout << "DeleteProductReference: Product deleted: " << name << std::endl;
    m_productReferences.erase(it);
    return true;
  }

  std::cout << "DeleteProductReference: Product not found: " << name << std::endl;
  return false;
}