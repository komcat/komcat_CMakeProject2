
#pragma once
#include "AppSettings.h"
#include <string>

// Base class for all parameter classes that need database loading
template<typename T>
class ParameterLoader : public CategoryConfig {
public:
  ParameterLoader(const std::string& categoryName)
    : CategoryConfig(categoryName) {
    // Derived class will call initDefaults() and loadFromDatabase()
  }

  // Pure virtual methods that derived classes must implement
  virtual void initDefaults() = 0;
  virtual void loadFromDatabase() = 0;
  virtual void saveToDatabase() = 0;

  // Utility methods available to all parameter classes
  void refresh() {
    loadFromDatabase();
  }

  void reset() {
    initDefaults();
    saveToDatabase();
  }

  std::string getCategoryName() const {
    return getCategory();
  }
};
