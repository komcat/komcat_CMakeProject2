// ProductReferenceManager_Products.cpp - Product CRUD operations
#include "ProductReferenceManager.h"
#include <algorithm>
#include <iostream>
#include <chrono>

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


// Modify the CreateProductReference method to auto-generate ID
bool ProductReferenceManager::CreateProductReference(const std::string& name,
  const std::string& description) {
  // Check if product already exists
  if (GetProductReference(name) != nullptr) {
    return false; // Product with this name already exists
  }

  // Generate unique ID
  int nextId = LoadNextProductId();
  std::string productId = GenerateProductId(nextId);

  // Create new product reference
  ProductReference newProduct;
  newProduct.name = name;
  newProduct.description = description;
  newProduct.id = productId;                    // NEW: Set auto-generated ID

  // Set creation date
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

	std::tm timeinfo;
	localtime_s(&timeinfo, &time_t);

  std::ostringstream dateStream;
  dateStream << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
  newProduct.createdDate = dateStream.str();

  // Initialize empty datum system
  newProduct.datum.constructionMethod = DatumReference::ConstructionMethod::NONE;

  // Add to collection
  m_productReferences.push_back(newProduct);

  // Update counter for next product
  SaveNextProductId(nextId + 1);

  std::cout << "[ProductReferenceManager] Created product '" << name
    << "' with ID: " << productId << std::endl;

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