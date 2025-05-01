// product_config_manager.h
#pragma once

#include "include/motions/MotionConfigManager.h"
#include "include/logger.h"
#include "include/ui/ToolbarMenu.h"  // Include for ITogglableUI interface
#include <vector>

// Make ProductConfigManager implement ITogglableUI interface
class ProductConfigManager : public ITogglableUI {
public:
  ProductConfigManager(MotionConfigManager& configManager);

  // List all available product configurations
  std::vector<std::string> GetProductList();

  // Load a specific product configuration
  bool LoadProductConfig(const std::string& productName);

  // Save the current configuration as a new product
  bool SaveAsNewProduct(const std::string& productName, const std::string& description);

  // Update an existing product with current configuration
  bool UpdateProductConfig(const std::string& productName);

  // Delete a product configuration
  bool DeleteProductConfig(const std::string& productName);

  // Render UI for product management
  void RenderUI();

  // ITogglableUI interface implementation
  bool IsVisible() const override { return m_showWindow; }
  void ToggleWindow() override { m_showWindow = !m_showWindow; }
  const std::string& GetName() const override { return m_windowTitle; }

private:
  MotionConfigManager& m_configManager;
  Logger* m_logger;

  std::string m_currentProduct;
  std::string m_configDir = "save";

  // UI state
  bool m_showWindow = true;
  std::string m_windowTitle = "Product Config";

  struct ProductMetadata {
    std::string version;
    std::string createdDate;
    std::string lastUpdated;
    std::string description;
  };

  // Helper methods
  std::string GetProductPath(const std::string& productName);
  bool SaveMetadata(const std::string& productName, const ProductMetadata& metadata);
  bool LoadMetadata(const std::string& productName, ProductMetadata& metadata);
};