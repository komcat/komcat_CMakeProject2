// DatumUI_LeftPanel.cpp - Product list and management controls with ID support
#include "DatumUI.h"

// =============================================================================
// LEFT PANEL COMPONENTS
// =============================================================================

void DatumUI::RenderLeftPanel() {
  ImGui::Text("Product References");
  ImGui::Separator();

  RenderProductCreationControls();
  ImGui::Spacing();

  // NEW: Add Save/Load controls
  RenderSaveLoadControls();
  ImGui::Spacing();

  RenderProductList();
  ImGui::Spacing();
  RenderSelectedProductInfo();
}

void DatumUI::RenderProductCreationControls() {
  if (ImGui::Button("Create New Product", ImVec2(-1, 30))) {
    OnCreateNewProduct();
  }

  ImGui::Spacing();
}

// UPDATED: Save/Load Controls with ID information
void DatumUI::RenderSaveLoadControls() {
  ImGui::Text("Save/Load Products");
  ImGui::Separator();

  // Save filename input
  static char saveFilename[256] = "my_products";
  ImGui::PushItemWidth(-1);
  ImGui::InputText("##SaveFilename", saveFilename, sizeof(saveFilename));
  ImGui::PopItemWidth();

  // Save/Load buttons
  if (ImGui::Button("Save All", ImVec2(-1, 0))) {
    if (m_referenceManager->SaveToFile(saveFilename)) {
      ShowSuccessMessage("Products saved successfully!");
    }
    else {
      ShowErrorMessage("Failed to save products");
    }
  }

  if (ImGui::Button("Auto Save", ImVec2(-1, 0))) {
    if (m_referenceManager->AutoSave()) {
      ShowSuccessMessage("Auto-save completed!");
    }
    else {
      ShowErrorMessage("Auto-save failed");
    }
  }

  // Load file dropdown
  auto availableFiles = m_referenceManager->GetAvailableProductFiles();
  static int selectedFileIndex = -1;
  static std::string selectedFilename = "";

  if (ImGui::BeginCombo("Load File", selectedFilename.empty() ? "Select file..." : selectedFilename.c_str())) {
    for (int i = 0; i < availableFiles.size(); i++) {
      bool isSelected = (selectedFileIndex == i);
      if (ImGui::Selectable(availableFiles[i].c_str(), isSelected)) {
        selectedFileIndex = i;
        selectedFilename = availableFiles[i];
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  // Load button
  ImGui::BeginDisabled(selectedFilename.empty());
  if (ImGui::Button("Load Products", ImVec2(-1, 0))) {
    if (m_referenceManager->LoadFromFile(selectedFilename)) {
      ShowSuccessMessage("Products loaded successfully!");
      // Clear selection since products list changed
      m_selectedProductName.clear();
      selectedFileIndex = -1;
      selectedFilename = "";
    }
    else {
      ShowErrorMessage("Failed to load products");
    }
  }
  ImGui::EndDisabled();

  // Show file count and helpful info about ID-based filenames
  if (!availableFiles.empty()) {
    ImGui::TextDisabled("(%zu saved files)", availableFiles.size());
  }

  ImGui::Spacing();
  ImGui::TextWrapped("💡 Tip: Individual products are saved with their ID in the filename (e.g., myproduct_PROD_000001.json)");
}

// UPDATED: Product List with ID Display
void DatumUI::RenderProductList() {
  ImGui::Text("Existing Products:");

  const auto& products = m_referenceManager->GetAllProductReferences();

  if (products.empty()) {
    ImGui::TextDisabled("No products created yet");
    return;
  }

  // Product selection list with ID display
  if (ImGui::BeginListBox("##ProductList", ImVec2(-1, 200))) {
    for (const auto& product : products) {
      bool isSelected = (m_selectedProductName == product.name);

      // Format: "ProductName (PROD_000001)" if ID exists, otherwise just name
      std::string displayText;
      if (!product.id.empty()) {
        displayText = product.name + " (" + product.id + ")";
      }
      else {
        displayText = product.name + " (No ID)";
      }

      if (ImGui::Selectable(displayText.c_str(), isSelected)) {
        OnProductSelectionChanged(product.name);
      }

      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndListBox();
  }

  // Delete product button
  if (!m_selectedProductName.empty()) {
    ImGui::Spacing();
    if (ImGui::Button("Delete Product", ImVec2(-1, 0))) {
      m_itemToDelete = m_selectedProductName;
      m_deleteType = DeleteType::PRODUCT;
      m_showDeleteConfirmDialog = true;
    }
  }
}

// UPDATED: Selected Product Info with Prominent ID Display
void DatumUI::RenderSelectedProductInfo() {
  if (m_selectedProductName.empty()) {
    ImGui::TextDisabled("No product selected");
    return;
  }

  const auto* product = m_referenceManager->GetProductReference(m_selectedProductName);
  if (!product) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Invalid product reference");
    return;
  }

  ImGui::Separator();
  ImGui::Text("Product Info:");

  // Display Product ID (read-only, highlighted in green)
  if (!product->id.empty()) {
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "ID: %s", product->id.c_str());
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "ID: Not assigned");
  }

  ImGui::Text("Name: %s", product->name.c_str());

  if (!product->description.empty()) {
    ImGui::TextWrapped("Description: %s", product->description.c_str());
  }

  ImGui::Text("Points: %zu", product->points.size());
  ImGui::Text("Edges: %zu", product->edges.size());

  // Count valid points
  int validPoints = 0;
  for (const auto& point : product->points) {
    if (point.isValid) validPoints++;
  }

  if (validPoints != product->points.size()) {
    ImGui::Text("Valid Points: %d/%zu", validPoints, product->points.size());
  }

  // Datum status
  if (product->datum.constructionMethod != ProductReferenceManager::DatumReference::ConstructionMethod::NONE) {
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Datum: Configured");

    // Quick datum info
    const char* methodStr = (product->datum.constructionMethod ==
      ProductReferenceManager::DatumReference::ConstructionMethod::TWO_POINT) ?
      "2-Point" : "3-Point";
    ImGui::Text("Method: %s", methodStr);
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "Datum: Not Set");
  }

  if (!product->createdDate.empty()) {
    ImGui::Spacing();
    ImGui::TextDisabled("Created: %s", product->createdDate.c_str());
  }

  // UPDATED: Individual product save option - now includes ID in filename automatically
  ImGui::Spacing();
  ImGui::Separator();
  if (ImGui::Button("Save This Product Only", ImVec2(-1, 0))) {
    // Filename will automatically include ID: productname_PROD_000001.json
    std::string filename = product->name + "_product";
    if (m_referenceManager->SaveProductToFile(product->name, filename)) {
      if (!product->id.empty()) {
        ShowSuccessMessage("Product '" + product->name + "' saved with ID " + product->id + "!");
      }
      else {
        ShowSuccessMessage("Product '" + product->name + "' saved!");
      }
    }
    else {
      ShowErrorMessage("Failed to save product");
    }
  }
}