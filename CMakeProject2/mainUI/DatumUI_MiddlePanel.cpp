// DatumUI_MiddlePanel.cpp - Enhanced with Action Types
#include "DatumUI.h"

// =============================================================================
// MIDDLE PANEL COMPONENTS - ENHANCED WITH ACTION TYPES
// =============================================================================

void DatumUI::RenderMiddlePanel() {
  ImGui::Text("Points & Datum Management");
  ImGui::Separator();

  // Tab system for organization
  if (ImGui::BeginTabBar("MiddlePanelTabs")) {
    if (ImGui::BeginTabItem("Points")) {
      RenderPointList();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Edges")) {
      RenderEdgeList();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Datum System")) {
      RenderDatumCreationControls();
      ImGui::Spacing();
      RenderCurrentDatumInfo();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}

// 1. ADD THIS TO YOUR EXISTING MIDDLE PANEL FILE (replace RenderPointList method):
void DatumUI::RenderPointList() {
  if (m_selectedProductName.empty()) {
    ImGui::TextDisabled("No product selected");
    return;
  }

  const auto* product = m_referenceManager->GetProductReference(m_selectedProductName);
  if (!product) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Invalid product reference");
    return;
  }

  // Add Point button
  if (ImGui::Button("Add Point", ImVec2(-1, 0))) {
    OnAddPoint();
  }

  ImGui::Spacing();

  // Action type filter
  static int selectedActionFilter = 0; // 0 = All, 1-5 = specific types
  const char* actionFilterItems[] = { "All Points", "Fiducials", "Inspection", "Dispense", "Pick", "Place" };

  ImGui::Text("Filter by Action Type:");
  ImGui::PushItemWidth(-1);
  if (ImGui::Combo("##ActionFilter", &selectedActionFilter, actionFilterItems, IM_ARRAYSIZE(actionFilterItems))) {
    // Filter changed - display will update automatically
  }
  ImGui::PopItemWidth();

  ImGui::Spacing();

  // Points table with action types
  if (ImGui::BeginTable("PointsTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
    // Headers
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableSetupColumn("Z", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableHeadersRow();

    // Filter points based on selection
    std::vector<const ProductReferenceManager::Point3D*> filteredPoints;

    if (selectedActionFilter == 0) {
      // Show all points
      for (const auto& point : product->points) {
        filteredPoints.push_back(&point);
      }
    }
    else {
      // Show specific action type
      ProductReferenceManager::Point3D::ActionType filterType =
        static_cast<ProductReferenceManager::Point3D::ActionType>(selectedActionFilter - 1);
      auto actionPoints = m_referenceManager->GetPointsByActionType(m_selectedProductName, filterType);
      for (const auto* point : actionPoints) {
        filteredPoints.push_back(point);
      }
    }

    // Display filtered points
    for (const auto* point : filteredPoints) {
      ImGui::TableNextRow();

      // Point name - clickable for selection
      ImGui::TableSetColumnIndex(0);
      bool isSelected = (m_selectedPointName == point->name);
      if (ImGui::Selectable(point->name.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
        OnPointSelectionChanged(point->name);
      }

      // Action type with color coding
      ImGui::TableSetColumnIndex(1);
      ImVec4 actionColor = GetActionTypeColor(point->actionType);
      ImGui::TextColored(actionColor, "%s", point->GetActionTypeString().c_str());

      // Coordinates
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%.3f", point->x);

      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%.3f", point->y);

      ImGui::TableSetColumnIndex(4);
      ImGui::Text("%.3f", point->z);

      // Status
      ImGui::TableSetColumnIndex(5);
      if (point->isValid) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "OK");
      }
      else {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Invalid");
      }
    }

    ImGui::EndTable();
  }

  // Point management buttons
  if (!m_selectedPointName.empty()) {
    ImGui::Spacing();

    if (ImGui::Button("Edit Point", ImVec2(-1, 0))) {
      OnEditPoint();
    }

    if (ImGui::Button("Change Action Type", ImVec2(-1, 0))) {
      m_showChangeActionDialog = true;
    }

    if (ImGui::Button("Delete Point", ImVec2(-1, 0))) {
      OnDeletePoint();
    }
  }

  // Show point count by action type
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Point Summary:");

  auto fiducials = m_referenceManager->GetPointsByActionType(m_selectedProductName, ProductReferenceManager::Point3D::ActionType::FIDUCIAL);
  auto inspections = m_referenceManager->GetPointsByActionType(m_selectedProductName, ProductReferenceManager::Point3D::ActionType::INSPECTION);
  auto dispenses = m_referenceManager->GetPointsByActionType(m_selectedProductName, ProductReferenceManager::Point3D::ActionType::DISPENSE);
  auto picks = m_referenceManager->GetPointsByActionType(m_selectedProductName, ProductReferenceManager::Point3D::ActionType::PICK);
  auto places = m_referenceManager->GetPointsByActionType(m_selectedProductName, ProductReferenceManager::Point3D::ActionType::PLACE);

  ImGui::Text("Fiducials: %zu | Inspection: %zu | Dispense: %zu", fiducials.size(), inspections.size(), dispenses.size());
  ImGui::Text("Pick: %zu | Place: %zu | Total: %zu", picks.size(), places.size(), product->points.size());
}

// =============================================================================
// NEW DIALOG: CHANGE ACTION TYPE
// =============================================================================

// 2. ADD THIS NEW DIALOG METHOD (to your dialogs file or any DatumUI .cpp file):
void DatumUI::RenderChangeActionTypeDialog() {
  if (!m_showChangeActionDialog) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Change Point Action Type", &m_showChangeActionDialog)) {

    if (m_selectedPointName.empty()) {
      ImGui::Text("No point selected");
      ImGui::End();
      return;
    }

    const auto* point = m_referenceManager->GetPoint(m_selectedProductName, m_selectedPointName);
    if (!point) {
      ImGui::Text("Point not found");
      ImGui::End();
      return;
    }

    ImGui::Text("Point: %s", m_selectedPointName.c_str());
    ImGui::Text("Current Action: %s", point->GetActionTypeString().c_str());
    ImGui::Separator();

    ImGui::Text("Select New Action Type:");

    // Action type selection with descriptions
    static int selectedAction = 0;

    if (ImGui::RadioButton("Fiducial", selectedAction == 0)) selectedAction = 0;
    ImGui::SameLine();
    ImGui::TextDisabled("(Reference points for datum construction)");

    if (ImGui::RadioButton("Inspection", selectedAction == 1)) selectedAction = 1;
    ImGui::SameLine();
    ImGui::TextDisabled("(Points for vision inspection)");

    if (ImGui::RadioButton("Dispense", selectedAction == 2)) selectedAction = 2;
    ImGui::SameLine();
    ImGui::TextDisabled("(Points for material dispensing)");

    if (ImGui::RadioButton("Pick", selectedAction == 3)) selectedAction = 3;
    ImGui::SameLine();
    ImGui::TextDisabled("(Points for picking operations)");

    if (ImGui::RadioButton("Place", selectedAction == 4)) selectedAction = 4;
    ImGui::SameLine();
    ImGui::TextDisabled("(Points for placing operations)");

    ImGui::Spacing();
    ImGui::Separator();

    // Buttons
    if (ImGui::Button("Apply", ImVec2(120, 0))) {
      ProductReferenceManager::Point3D::ActionType newActionType =
        static_cast<ProductReferenceManager::Point3D::ActionType>(selectedAction);

      if (m_referenceManager->UpdatePointActionType(m_selectedProductName, m_selectedPointName, newActionType)) {
        ShowSuccessMessage("Action type updated successfully!");
        m_showChangeActionDialog = false;
      }
      else {
        ShowErrorMessage("Failed to update action type");
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      m_showChangeActionDialog = false;
    }
  }
  ImGui::End();
}

// =============================================================================
// ENHANCED ADD POINT DIALOG WITH ACTION TYPE
// =============================================================================

void DatumUI::RenderAddPointDialog() {
  if (!m_showAddPointDialog) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Add New Point", &m_showAddPointDialog)) {

    if (m_selectedProductName.empty()) {
      ImGui::Text("No product selected");
      ImGui::End();
      return;
    }

    // Point name
    ImGui::Text("Point Name:");
    ImGui::InputText("##PointName", m_pointNameBuffer, sizeof(m_pointNameBuffer));

    // Coordinates
    ImGui::Text("Coordinates:");
    ImGui::InputFloat3("X, Y, Z", m_pointCoords);

    // Action type selection
    ImGui::Text("Action Type:");
    static int selectedNewAction = 0;
    const char* actionItems[] = { "Fiducial", "Inspection", "Dispense", "Pick", "Place" };
    ImGui::Combo("##ActionType", &selectedNewAction, actionItems, IM_ARRAYSIZE(actionItems));

    // Description
    ImGui::Text("Description (optional):");
    ImGui::InputTextMultiline("##PointDesc", m_pointDescBuffer, sizeof(m_pointDescBuffer), ImVec2(-1, 60));

    ImGui::Spacing();
    ImGui::Separator();

    // Buttons
    if (ImGui::Button("Add Point", ImVec2(120, 0))) {
      std::string pointName = m_pointNameBuffer;
      std::string description = m_pointDescBuffer;

      if (pointName.empty()) {
        ShowErrorMessage("Point name cannot be empty");
      }
      else {
        ProductReferenceManager::Point3D::ActionType actionType =
          static_cast<ProductReferenceManager::Point3D::ActionType>(selectedNewAction);

        if (m_referenceManager->AddPoint(m_selectedProductName, pointName,
          m_pointCoords[0], m_pointCoords[1], m_pointCoords[2],
          actionType, description)) {
          ShowSuccessMessage("Point added successfully!");
          ClearInputBuffers();
          m_showAddPointDialog = false;
        }
        else {
          ShowErrorMessage("Failed to add point");
        }
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ClearInputBuffers();
      m_showAddPointDialog = false;
    }
  }
  ImGui::End();
}

// 3. ADD THIS HELPER METHOD (to any DatumUI .cpp file):
ImVec4 DatumUI::GetActionTypeColor(ProductReferenceManager::Point3D::ActionType actionType) const {
  switch (actionType) {
  case ProductReferenceManager::Point3D::ActionType::FIDUCIAL:
    return ImVec4(0.7f, 0.7f, 1.0f, 1.0f);  // Light blue
  case ProductReferenceManager::Point3D::ActionType::INSPECTION:
    return ImVec4(1.0f, 1.0f, 0.4f, 1.0f);  // Yellow
  case ProductReferenceManager::Point3D::ActionType::DISPENSE:
    return ImVec4(0.4f, 1.0f, 0.4f, 1.0f);  // Green
  case ProductReferenceManager::Point3D::ActionType::PICK:
    return ImVec4(1.0f, 0.6f, 0.4f, 1.0f);  // Orange
  case ProductReferenceManager::Point3D::ActionType::PLACE:
    return ImVec4(1.0f, 0.4f, 1.0f, 1.0f);  // Magenta
  default:
    return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);  // Gray
  }
}



// 5. ENHANCED RenderEdgeList() WITH BETTER DEBUGGING:
void DatumUI::RenderEdgeList() {
  if (m_selectedProductName.empty()) {
    ImGui::TextDisabled("No product selected");
    return;
  }

  const auto* product = m_referenceManager->GetProductReference(m_selectedProductName);
  if (!product) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Invalid product reference");
    return;
  }

  ImGui::Text("Edges (%zu total)", product->edges.size());
  ImGui::Separator();

  if (product->edges.empty()) {
    ImGui::TextDisabled("No edges created yet");
    ImGui::Spacing();
    ImGui::TextWrapped("Edges are automatically created when you build datum systems in the 'Datum System' tab.");
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "💡 Tip: Go to 'Datum System' tab and click 'Create Datum System'");
    return;
  }

  // Edges table
  if (ImGui::BeginTable("EdgesTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
    // Headers
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("From", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("To", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Length", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableHeadersRow();

    // Display edges
    for (const auto& edge : product->edges) {
      ImGui::TableNextRow();

      // Edge name - clickable for selection
      ImGui::TableSetColumnIndex(0);
      bool isSelected = (m_selectedEdgeName == edge.name);
      if (ImGui::Selectable(edge.name.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
        OnEdgeSelectionChanged(edge.name);
      }

      // From point
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%s", edge.point1Name.c_str());

      // To point
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%s", edge.point2Name.c_str());

      // Calculate and display length
      ImGui::TableSetColumnIndex(3);
      auto length = m_referenceManager->CalculateEdgeLength(m_selectedProductName, edge.name);
      if (length.has_value()) {
        ImGui::Text("%.3f", length.value());
      }
      else {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Error");
      }
    }

    ImGui::EndTable();
  }

  // Edge management buttons
  if (!m_selectedEdgeName.empty()) {
    ImGui::Spacing();
    if (ImGui::Button("Delete Edge", ImVec2(-1, 0))) {
      m_itemToDelete = m_selectedEdgeName;
      m_deleteType = DeleteType::EDGE;
      m_showDeleteConfirmDialog = true;
    }
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Edge Summary:");
  ImGui::Text("Total Edges: %zu", product->edges.size());

  // Show which edges belong to the datum system
  if (product->datum.constructionMethod != ProductReferenceManager::DatumReference::ConstructionMethod::NONE) {
    ImGui::Text("Datum Edges:");
    if (!product->datum.xAxisEdgeName.empty()) {
      ImGui::BulletText("X-Axis: %s", product->datum.xAxisEdgeName.c_str());
    }
    if (!product->datum.yAxisEdgeName.empty()) {
      ImGui::BulletText("Y-Axis: %s", product->datum.yAxisEdgeName.c_str());
    }
  }
}

// 3. ADD THESE MISSING DATUM METHODS:
void DatumUI::RenderDatumCreationControls() {
  if (m_selectedProductName.empty()) {
    ImGui::TextDisabled("No product selected");
    return;
  }

  const auto* product = m_referenceManager->GetProductReference(m_selectedProductName);
  if (!product) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Invalid product reference");
    return;
  }

  ImGui::Text("Create Datum System");
  ImGui::Separator();

  // Check if we have enough points
  if (product->points.size() < 2) {
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "Need at least 2 points to create datum");
    return;
  }

  // Datum creation method selection
  ImGui::Text("Datum Method:");
  if (ImGui::RadioButton("2-Point System (X-axis only)", m_datumMethod == DatumCreationMethod::TWO_POINT)) {
    m_datumMethod = DatumCreationMethod::TWO_POINT;
  }

  if (product->points.size() >= 3) {
    if (ImGui::RadioButton("3-Point System (X and Y axes)", m_datumMethod == DatumCreationMethod::THREE_POINT)) {
      m_datumMethod = DatumCreationMethod::THREE_POINT;
    }
  }
  else {
    ImGui::BeginDisabled();
    ImGui::RadioButton("3-Point System (Need 3+ points)", false);
    ImGui::EndDisabled();
  }

  ImGui::Spacing();

  // Point selection based on method
  if (m_datumMethod != DatumCreationMethod::NONE) {
    ImGui::Text("Select Points:");

    // Origin point selection
    ImGui::Text("Origin Point:");
    if (ImGui::BeginCombo("##OriginPoint", m_datumOriginPoint.empty() ? "Select..." : m_datumOriginPoint.c_str())) {
      for (const auto& point : product->points) {
        if (ImGui::Selectable(point.name.c_str(), m_datumOriginPoint == point.name)) {
          m_datumOriginPoint = point.name;
        }
      }
      ImGui::EndCombo();
    }

    // Second point selection
    ImGui::Text("X-Axis Point:");
    if (ImGui::BeginCombo("##Point2", m_datumPoint2.empty() ? "Select..." : m_datumPoint2.c_str())) {
      for (const auto& point : product->points) {
        if (point.name != m_datumOriginPoint) {
          if (ImGui::Selectable(point.name.c_str(), m_datumPoint2 == point.name)) {
            m_datumPoint2 = point.name;
          }
        }
      }
      ImGui::EndCombo();
    }

    // Third point selection (for 3-point method)
    if (m_datumMethod == DatumCreationMethod::THREE_POINT) {
      ImGui::Text("Y-Axis Point:");
      if (ImGui::BeginCombo("##Point3", m_datumPoint3.empty() ? "Select..." : m_datumPoint3.c_str())) {
        for (const auto& point : product->points) {
          if (point.name != m_datumOriginPoint && point.name != m_datumPoint2) {
            if (ImGui::Selectable(point.name.c_str(), m_datumPoint3 == point.name)) {
              m_datumPoint3 = point.name;
            }
          }
        }
        ImGui::EndCombo();
      }
    }

    ImGui::Spacing();

    // Create datum button
    bool canCreate = !m_datumOriginPoint.empty() && !m_datumPoint2.empty();
    if (m_datumMethod == DatumCreationMethod::THREE_POINT) {
      canCreate = canCreate && !m_datumPoint3.empty();
    }

    ImGui::BeginDisabled(!canCreate);
    if (ImGui::Button("Create Datum System", ImVec2(-1, 0))) {
      OnCreateDatum();
    }
    ImGui::EndDisabled();
  }

  ImGui::Spacing();

  // Clear datum button
  if (product->datum.constructionMethod != ProductReferenceManager::DatumReference::ConstructionMethod::NONE) {
    if (ImGui::Button("Clear Datum System", ImVec2(-1, 0))) {
      OnClearDatum();
    }
  }
}



// 4. UPDATE YOUR RenderCurrentDatumInfo() TO SHOW MORE DEBUG INFO:
void DatumUI::RenderCurrentDatumInfo() {
  if (m_selectedProductName.empty()) {
    return;
  }

  const auto* product = m_referenceManager->GetProductReference(m_selectedProductName);
  if (!product) {
    return;
  }

  ImGui::Text("Current Datum Status");
  ImGui::Separator();

  if (product->datum.constructionMethod == ProductReferenceManager::DatumReference::ConstructionMethod::NONE) {
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "No datum system configured");
    ImGui::Text("Use the datum creation buttons above to create a coordinate system.");
    return;
  }

  // Show datum info
  ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "✓ Datum System Active");

  const char* methodStr = (product->datum.constructionMethod ==
    ProductReferenceManager::DatumReference::ConstructionMethod::TWO_POINT) ? "2-Point" : "3-Point";

  ImGui::Text("Method: %s", methodStr);
  ImGui::Text("Datum Name: %s", product->datum.name.c_str());
  ImGui::Text("Origin Point: %s", product->datum.originPointName.c_str());

  if (!product->datum.xAxisEdgeName.empty()) {
    ImGui::Text("X-Axis Edge: %s", product->datum.xAxisEdgeName.c_str());
  }

  if (!product->datum.yAxisEdgeName.empty()) {
    ImGui::Text("Y-Axis Edge: %s", product->datum.yAxisEdgeName.c_str());
  }

  // Show edges created by datum
  ImGui::Spacing();
  ImGui::Text("Associated Edges: %zu", product->edges.size());
  for (const auto& edge : product->edges) {
    ImGui::BulletText("%s (%s → %s)", edge.name.c_str(), edge.point1Name.c_str(), edge.point2Name.c_str());
  }

  // Validation
  std::string errorMsg;
  bool isValid = m_referenceManager->ValidateDatumSystem(m_selectedProductName, errorMsg);

  ImGui::Spacing();
  ImGui::Separator();
  if (isValid) {
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "✓ Datum system is mathematically valid");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "✗ Datum system error:");
    ImGui::TextWrapped("%s", errorMsg.c_str());
  }
}

