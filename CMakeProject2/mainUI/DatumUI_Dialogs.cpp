// DatumUI_Dialogs.cpp - Complete popup dialog implementations
#include "DatumUI.h"

// =============================================================================
// DIALOG RENDERING METHODS
// =============================================================================

void DatumUI::RenderNewProductDialog() {
  if (!m_showNewProductDialog) return;

  ImGui::OpenPopup("Create New Product");

  if (ImGui::BeginPopupModal("Create New Product", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Enter product details:");
    ImGui::Spacing();

    ImGui::Text("Name:");
    ImGui::InputText("##ProductName", m_newProductNameBuffer, sizeof(m_newProductNameBuffer));

    ImGui::Text("Description (optional):");
    ImGui::InputTextMultiline("##ProductDesc", m_newProductDescBuffer, sizeof(m_newProductDescBuffer), ImVec2(300, 60));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Create", ImVec2(100, 0))) {
      std::string name = m_newProductNameBuffer;
      std::string desc = m_newProductDescBuffer;

      std::string errorMsg;
      if (ValidateProductName(name, errorMsg)) {
        if (m_referenceManager->CreateProductReference(name, desc)) {
          ShowSuccessMessage("Product '" + name + "' created successfully");
          OnProductSelectionChanged(name);
          m_showNewProductDialog = false;
          ClearInputBuffers();
        }
        else {
          ShowErrorMessage("Failed to create product");
        }
      }
      else {
        ShowErrorMessage(errorMsg);
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0))) {
      m_showNewProductDialog = false;
      ClearInputBuffers();
    }

    ImGui::EndPopup();
  }
}


void DatumUI::RenderEditPointDialog() {
  if (!m_showEditPointDialog) return;

  ImGui::OpenPopup("Edit Point");

  if (ImGui::BeginPopupModal("Edit Point", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Edit point coordinates:");
    ImGui::Spacing();

    ImGui::Text("Point: %s", m_pointNameBuffer);
    ImGui::Spacing();

    ImGui::Text("Description:");
    ImGui::InputText("##PointDesc", m_pointDescBuffer, sizeof(m_pointDescBuffer));

    ImGui::Spacing();
    ImGui::Text("Coordinates:");
    ImGui::InputFloat3("XYZ", m_pointCoords);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Update", ImVec2(100, 0))) {
      if (m_referenceManager->UpdatePoint(m_selectedProductName, m_pointNameBuffer,
        m_pointCoords[0], m_pointCoords[1], m_pointCoords[2])) {
        ShowSuccessMessage("Point updated successfully");
        m_showEditPointDialog = false;
        ClearInputBuffers();
      }
      else {
        ShowErrorMessage("Failed to update point");
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0))) {
      m_showEditPointDialog = false;
      ClearInputBuffers();
    }

    ImGui::EndPopup();
  }
}

void DatumUI::RenderDatumCreationDialog() {
  if (!m_showDatumCreationDialog) return;

  const char* title = (m_datumMethod == DatumCreationMethod::TWO_POINT) ?
    "Create 2-Point Datum System" : "Create 3-Point Datum System";

  ImGui::OpenPopup(title);

  if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    const auto* product = m_referenceManager->GetProductReference(m_selectedProductName);
    if (!product) {
      ImGui::CloseCurrentPopup();
      return;
    }

    // Get list of valid points
    std::vector<std::string> validPointNames;
    for (const auto& point : product->points) {
      if (point.isValid) {
        validPointNames.push_back(point.name);
      }
    }

    ImGui::Text("Select points for datum creation:");
    ImGui::Spacing();

    // Show which method is selected
    if (m_datumMethod == DatumCreationMethod::TWO_POINT) {
      ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Method: 2-Point System (Origin + 1 point → X-axis)");
    }
    else {
      ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Method: 3-Point System (Origin + 2 points → X & Y axes)");
    }
    ImGui::Spacing();

    // Origin point selection
    ImGui::Text("Origin Point:");
    if (ImGui::BeginCombo("##OriginPoint", m_datumOriginPoint.empty() ? "Select..." : m_datumOriginPoint.c_str())) {
      for (const auto& pointName : validPointNames) {
        bool isSelected = (m_datumOriginPoint == pointName);
        if (ImGui::Selectable(pointName.c_str(), isSelected)) {
          m_datumOriginPoint = pointName;
        }
        if (isSelected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }

    // Second point selection
    ImGui::Text("Second Point:");
    if (ImGui::BeginCombo("##Point2", m_datumPoint2.empty() ? "Select..." : m_datumPoint2.c_str())) {
      for (const auto& pointName : validPointNames) {
        if (pointName != m_datumOriginPoint) { // Don't allow same as origin
          bool isSelected = (m_datumPoint2 == pointName);
          if (ImGui::Selectable(pointName.c_str(), isSelected)) {
            m_datumPoint2 = pointName;
          }
          if (isSelected) {
            ImGui::SetItemDefaultFocus();
          }
        }
      }
      ImGui::EndCombo();
    }

    // Third point selection (ONLY for 3-point method)
    if (m_datumMethod == DatumCreationMethod::THREE_POINT) {
      ImGui::Text("Third Point:");
      if (ImGui::BeginCombo("##Point3", m_datumPoint3.empty() ? "Select..." : m_datumPoint3.c_str())) {
        for (const auto& pointName : validPointNames) {
          if (pointName != m_datumOriginPoint && pointName != m_datumPoint2) {
            bool isSelected = (m_datumPoint3 == pointName);
            if (ImGui::Selectable(pointName.c_str(), isSelected)) {
              m_datumPoint3 = pointName;
            }
            if (isSelected) {
              ImGui::SetItemDefaultFocus();
            }
          }
        }
        ImGui::EndCombo();
      }

      ImGui::Spacing();
      ImGui::Text("Axis Assignment:");
      if (ImGui::RadioButton("First edge (Origin → Point2) as X-axis", m_useFirstEdgeAsX)) {
        m_useFirstEdgeAsX = true;
      }
      if (ImGui::RadioButton("Second edge (Origin → Point3) as X-axis", !m_useFirstEdgeAsX)) {
        m_useFirstEdgeAsX = false;
      }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Validation and create button
    bool canCreate = !m_datumOriginPoint.empty() && !m_datumPoint2.empty();
    if (m_datumMethod == DatumCreationMethod::THREE_POINT) {
      canCreate = canCreate && !m_datumPoint3.empty();
    }

    if (!canCreate) {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    }

    if (ImGui::Button("Create Datum", ImVec2(120, 0))) {
      if (canCreate) {
        bool success = false;

        // FIX: Call the correct method based on the datum method
        if (m_datumMethod == DatumCreationMethod::TWO_POINT) {
          success = m_referenceManager->CreateDatumFrom2Points(
            m_selectedProductName, m_datumOriginPoint, m_datumPoint2);
        }
        else if (m_datumMethod == DatumCreationMethod::THREE_POINT) {
          success = m_referenceManager->CreateDatumFrom3Points(
            m_selectedProductName, m_datumOriginPoint, m_datumPoint2, m_datumPoint3, m_useFirstEdgeAsX);
        }

        if (success) {
          ShowSuccessMessage("Datum system created successfully");
          m_showDatumCreationDialog = false;
          ResetDatumCreationState();
        }
        else {
          ShowErrorMessage("Failed to create datum system");
        }
      }
    }

    if (!canCreate) {
      ImGui::PopStyleVar();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0))) {
      m_showDatumCreationDialog = false;
      ResetDatumCreationState();
    }

    ImGui::EndPopup();
  }
}

void DatumUI::RenderDeleteConfirmDialog() {
  if (!m_showDeleteConfirmDialog) return;

  ImGui::OpenPopup("Confirm Delete");

  if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    std::string itemType;
    switch (m_deleteType) {
    case DeleteType::PRODUCT: itemType = "product"; break;
    case DeleteType::POINT: itemType = "point"; break;
    case DeleteType::EDGE: itemType = "edge"; break;
    default: itemType = "item"; break;
    }

    ImGui::Text("Are you sure you want to delete %s '%s'?", itemType.c_str(), m_itemToDelete.c_str());

    if (m_deleteType == DeleteType::POINT) {
      ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "Warning: This will also remove related edges and may clear the datum system.");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Delete", ImVec2(100, 0))) {
      bool success = false;
      std::string successMsg;

      switch (m_deleteType) {
      case DeleteType::PRODUCT:
        success = m_referenceManager->DeleteProductReference(m_itemToDelete);
        successMsg = "Product deleted";
        if (success && m_selectedProductName == m_itemToDelete) {
          m_selectedProductName.clear();
          m_selectedPointName.clear();
          m_selectedEdgeName.clear();
        }
        break;

      case DeleteType::POINT:
        success = m_referenceManager->RemovePoint(m_selectedProductName, m_itemToDelete);
        successMsg = "Point deleted";
        if (success && m_selectedPointName == m_itemToDelete) {
          m_selectedPointName.clear();
        }
        break;

      case DeleteType::EDGE:
        success = m_referenceManager->RemoveEdge(m_selectedProductName, m_itemToDelete);
        successMsg = "Edge deleted";
        if (success && m_selectedEdgeName == m_itemToDelete) {
          m_selectedEdgeName.clear();
        }
        break;
      }

      if (success) {
        ShowSuccessMessage(successMsg);
      }
      else {
        ShowErrorMessage("Failed to delete " + itemType);
      }

      m_showDeleteConfirmDialog = false;
      m_itemToDelete.clear();
      m_deleteType = DeleteType::NONE;
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0))) {
      m_showDeleteConfirmDialog = false;
      m_itemToDelete.clear();
      m_deleteType = DeleteType::NONE;
    }

    ImGui::EndPopup();
  }
}