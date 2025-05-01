// product_config_manager.cpp
#include "include/data/product_config_manager.h"
#include "imgui.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

// In the constructor of ProductConfigManager, initialize the window title
ProductConfigManager::ProductConfigManager(MotionConfigManager& configManager)
  : m_configManager(configManager),
  m_windowTitle("Product Config")  // Set the window title for toolbar menu
{
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("ProductConfigManager: Initialized");

  // Create the config directory if it doesn't exist
  if (!std::filesystem::exists(m_configDir)) {
    std::filesystem::create_directory(m_configDir);
    m_logger->LogInfo("ProductConfigManager: Created config directory: " + m_configDir);
  }
}


std::vector<std::string> ProductConfigManager::GetProductList()
{
  std::vector<std::string> products;

  try {
    // Check if the directory exists
    if (!std::filesystem::exists(m_configDir)) {
      return products;
    }

    // Iterate through directories in the config directory
    for (const auto& entry : std::filesystem::directory_iterator(m_configDir)) {
      if (entry.is_directory()) {
        std::string productName = entry.path().filename().string();

        // Only add if it has a config.json file
        if (std::filesystem::exists(entry.path() / "config.json")) {
          products.push_back(productName);
        }
      }
    }

    // Sort alphabetically
    std::sort(products.begin(), products.end());
  }
  catch (const std::exception& e) {
    m_logger->LogError("ProductConfigManager: Error getting product list: " + std::string(e.what()));
  }

  return products;
}

std::string ProductConfigManager::GetProductPath(const std::string& productName)
{
  return m_configDir + "/" + productName;
}

bool ProductConfigManager::SaveMetadata(const std::string& productName, const ProductMetadata& metadata)
{
  std::string metadataPath = GetProductPath(productName) + "/metadata.json";

  try {
    // Create JSON object
    nlohmann::json metadataJson;
    metadataJson["version"] = metadata.version;
    metadataJson["createdDate"] = metadata.createdDate;
    metadataJson["lastUpdated"] = metadata.lastUpdated;
    metadataJson["description"] = metadata.description;

    // Write to file
    std::ofstream file(metadataPath);
    if (!file.is_open()) {
      m_logger->LogError("ProductConfigManager: Failed to open metadata file for writing: " + metadataPath);
      return false;
    }

    file << metadataJson.dump(2); // Pretty print with 2-space indentation
    return true;
  }
  catch (const std::exception& e) {
    m_logger->LogError("ProductConfigManager: Error saving metadata: " + std::string(e.what()));
    return false;
  }
}

bool ProductConfigManager::LoadMetadata(const std::string& productName, ProductMetadata& metadata)
{
  std::string metadataPath = GetProductPath(productName) + "/metadata.json";

  try {
    // Check if file exists
    if (!std::filesystem::exists(metadataPath)) {
      m_logger->LogWarning("ProductConfigManager: Metadata file not found: " + metadataPath);
      return false;
    }

    // Open and read the file
    std::ifstream file(metadataPath);
    if (!file.is_open()) {
      m_logger->LogError("ProductConfigManager: Failed to open metadata file: " + metadataPath);
      return false;
    }

    // Parse JSON
    nlohmann::json metadataJson;
    file >> metadataJson;

    // Extract metadata
    metadata.version = metadataJson.value("version", "1.0");
    metadata.createdDate = metadataJson.value("createdDate", "");
    metadata.lastUpdated = metadataJson.value("lastUpdated", "");
    metadata.description = metadataJson.value("description", "");

    return true;
  }
  catch (const std::exception& e) {
    m_logger->LogError("ProductConfigManager: Error loading metadata: " + std::string(e.what()));
    return false;
  }
}

bool ProductConfigManager::SaveAsNewProduct(const std::string& productName, const std::string& description)
{
  if (productName.empty()) {
    m_logger->LogError("ProductConfigManager: Cannot save product with empty name");
    return false;
  }

  // Check if product already exists
  std::string productPath = GetProductPath(productName);
  if (std::filesystem::exists(productPath)) {
    m_logger->LogError("ProductConfigManager: Product already exists: " + productName);
    return false;
  }

  // Create the product directory
  try {
    std::filesystem::create_directories(productPath);
  }
  catch (const std::exception& e) {
    m_logger->LogError("ProductConfigManager: Failed to create product directory: " + std::string(e.what()));
    return false;
  }

  // Save the current configuration to the product directory
  std::string configPath = productPath + "/config.json";
  bool success = m_configManager.SaveConfig(configPath);

  if (success) {
    // Create and save metadata
    ProductMetadata metadata;

    // Get current time
    auto now = std::chrono::system_clock::now();
    auto nowTimeT = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&nowTimeT), "%Y-%m-%dT%H:%M:%SZ");

    metadata.version = "1.0";
    metadata.createdDate = ss.str();
    metadata.lastUpdated = ss.str();
    metadata.description = description;

    SaveMetadata(productName, metadata);

    m_currentProduct = productName;
    m_logger->LogInfo("ProductConfigManager: Saved new product configuration: " + productName);
  }
  else {
    m_logger->LogError("ProductConfigManager: Failed to save configuration for product: " + productName);

    // Clean up directory if config save failed
    try {
      std::filesystem::remove_all(productPath);
    }
    catch (...) {
      // Ignore cleanup errors
    }
  }

  return success;
}

bool ProductConfigManager::LoadProductConfig(const std::string& productName)
{
  std::string configPath = GetProductPath(productName) + "/config.json";

  // Check if the file exists
  if (!std::filesystem::exists(configPath)) {
    m_logger->LogError("ProductConfigManager: Product configuration not found: " + configPath);
    return false;
  }

  // Load the configuration file
  bool success = false;
  try {
    // Create a new instance of MotionConfigManager using the product's config file
    MotionConfigManager tempConfig(configPath);

    // Now we need to copy all the data from tempConfig to m_configManager
    // Since there's no CopyFrom method in MotionConfigManager, we'll save to a temp file and reload
    std::string tempFile = "temp_config.json";
    if (tempConfig.SaveConfig(tempFile)) {
      // Now reload the main config manager from this file
      m_configManager = MotionConfigManager(tempFile);
      success = true;

      // Clean up temp file
      std::filesystem::remove(tempFile);
    }

    if (success) {
      m_currentProduct = productName;
      m_logger->LogInfo("ProductConfigManager: Loaded product configuration: " + productName);
    }
  }
  catch (const std::exception& e) {
    m_logger->LogError("ProductConfigManager: Failed to load product configuration: " + std::string(e.what()));
    success = false;
  }

  return success;
}

bool ProductConfigManager::UpdateProductConfig(const std::string& productName)
{
  if (productName.empty()) {
    m_logger->LogError("ProductConfigManager: Cannot update product with empty name");
    return false;
  }

  // Check if product exists
  std::string productPath = GetProductPath(productName);
  if (!std::filesystem::exists(productPath)) {
    m_logger->LogError("ProductConfigManager: Product not found: " + productName);
    return false;
  }

  // Save the current configuration to the product directory
  std::string configPath = productPath + "/config.json";
  bool success = m_configManager.SaveConfig(configPath);

  if (success) {
    // Load existing metadata
    ProductMetadata metadata;
    if (!LoadMetadata(productName, metadata)) {
      // If metadata doesn't exist, create new metadata
      metadata.version = "1.0";
      metadata.createdDate = "";
    }

    // Update the last modified timestamp
    auto now = std::chrono::system_clock::now();
    auto nowTimeT = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&nowTimeT), "%Y-%m-%dT%H:%M:%SZ");
    metadata.lastUpdated = ss.str();

    // Save updated metadata
    SaveMetadata(productName, metadata);

    m_currentProduct = productName;
    m_logger->LogInfo("ProductConfigManager: Updated product configuration: " + productName);
  }
  else {
    m_logger->LogError("ProductConfigManager: Failed to update configuration for product: " + productName);
  }

  return success;
}

bool ProductConfigManager::DeleteProductConfig(const std::string& productName)
{
  if (productName.empty()) {
    m_logger->LogError("ProductConfigManager: Cannot delete product with empty name");
    return false;
  }

  std::string productPath = GetProductPath(productName);
  if (!std::filesystem::exists(productPath)) {
    m_logger->LogError("ProductConfigManager: Product not found: " + productName);
    return false;
  }

  try {
    std::filesystem::remove_all(productPath);
    m_logger->LogInfo("ProductConfigManager: Deleted product configuration: " + productName);

    // If the current product was deleted, clear it
    if (m_currentProduct == productName) {
      m_currentProduct.clear();
    }

    return true;
  }
  catch (const std::exception& e) {
    m_logger->LogError("ProductConfigManager: Failed to delete product: " + std::string(e.what()));
    return false;
  }
}

void ProductConfigManager::RenderUI()
{

  if (!m_showWindow) return;  // Don't render if window is hidden

  if (!ImGui::Begin("Product Configuration Manager")) {
    ImGui::End();
    return;
  }

  // Display currently loaded product
  ImGui::Text("Current Product: %s",
    m_currentProduct.empty() ? "None" : m_currentProduct.c_str());

  ImGui::Separator();

  // Product List with load button
  ImGui::Text("Available Products:");
  static int selectedProductIndex = -1;
  std::vector<std::string> products = GetProductList();

  ImGui::BeginChild("ProductList", ImVec2(0, 200), true);
  for (int i = 0; i < products.size(); i++) {
    if (ImGui::Selectable(products[i].c_str(), selectedProductIndex == i)) {
      selectedProductIndex = i;
    }
  }
  ImGui::EndChild();

  // Actions for selected product
  if (selectedProductIndex >= 0 && selectedProductIndex < products.size()) {
    const std::string& productName = products[selectedProductIndex];

    // Display metadata if available
    ProductMetadata metadata;
    if (LoadMetadata(productName, metadata)) {
      ImGui::Text("Description: %s", metadata.description.c_str());
      ImGui::Text("Created: %s", metadata.createdDate.c_str());
      ImGui::Text("Last Updated: %s", metadata.lastUpdated.c_str());
    }

    ImGui::Separator();

    // Load button
    if (ImGui::Button("Load Selected Product")) {
      if (LoadProductConfig(productName)) {
        // Success popup
        ImGui::OpenPopup("Load Successful");
      }
      else {
        // Error popup
        ImGui::OpenPopup("Load Error");
      }
    }

    ImGui::SameLine();

    // Update button
    if (ImGui::Button("Update Selected")) {
      if (UpdateProductConfig(productName)) {
        // Success popup
        ImGui::OpenPopup("Update Successful");
      }
      else {
        // Error popup
        ImGui::OpenPopup("Update Error");
      }
    }

    ImGui::SameLine();

    // Delete button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("Delete")) {
      ImGui::OpenPopup("Confirm Delete");
    }
    ImGui::PopStyleColor();
  }

  ImGui::Separator();

  // Create new product section
  static char newProductName[128] = "";
  static char newProductDesc[256] = "";

  ImGui::Text("Create New Product Configuration:");
  ImGui::InputText("Product Name", newProductName, sizeof(newProductName));
  ImGui::InputTextMultiline("Description", newProductDesc, sizeof(newProductDesc), ImVec2(-1, 60));

  if (ImGui::Button("Save Current Configuration As New Product")) {
    if (strlen(newProductName) > 0) {
      if (SaveAsNewProduct(newProductName, newProductDesc)) {
        // Clear input fields on success
        memset(newProductName, 0, sizeof(newProductName));
        memset(newProductDesc, 0, sizeof(newProductDesc));

        // Success popup
        ImGui::OpenPopup("Save Successful");
      }
      else {
        // Error popup
        ImGui::OpenPopup("Save Error");
      }
    }
    else {
      ImGui::OpenPopup("Error");
    }
  }

  // Handle popups
  if (ImGui::BeginPopupModal("Confirm Delete", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Are you sure you want to delete product '%s'?",
      products[selectedProductIndex].c_str());
    ImGui::Text("This operation cannot be undone!");
    ImGui::Separator();

    if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
      DeleteProductConfig(products[selectedProductIndex]);
      selectedProductIndex = -1;
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  // Simple notification popups
  if (ImGui::BeginPopupModal("Load Successful", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Product configuration loaded successfully!");
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("Load Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Failed to load product configuration!");
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("Update Successful", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Product configuration updated successfully!");
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("Update Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Failed to update product configuration!");
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("Save Successful", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("New product configuration saved successfully!");
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("Save Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Failed to save new product configuration!");
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Please enter a product name!");
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  ImGui::End();
}