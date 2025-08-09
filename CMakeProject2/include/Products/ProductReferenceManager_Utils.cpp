// ProductReferenceManager_Utils.cpp - Helper functions and utilities
#include "ProductReferenceManager.h"
#include <sstream>
#include <iomanip>
#include <chrono>

// =============================================================================
// UTILITY HELPER METHODS
// =============================================================================

std::string ProductReferenceManager::GenerateUniquePointName(const std::string& productName, const std::string& baseName) const {
  int counter = 1;
  std::string candidateName = baseName + std::to_string(counter);

  while (!IsPointNameUnique(productName, candidateName)) {
    counter++;
    candidateName = baseName + std::to_string(counter);
  }

  return candidateName;
}

std::string ProductReferenceManager::GenerateUniqueEdgeName(const std::string& productName, const std::string& baseName) const {
  int counter = 1;
  std::string candidateName = baseName + std::to_string(counter);

  while (!IsEdgeNameUnique(productName, candidateName)) {
    counter++;
    candidateName = baseName + std::to_string(counter);
  }

  return candidateName;
}

std::string ProductReferenceManager::GetCurrentTimestamp() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  return ss.str();
}


