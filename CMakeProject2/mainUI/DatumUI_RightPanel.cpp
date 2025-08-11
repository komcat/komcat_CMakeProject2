// DatumUI_RightPanel.cpp - Visual display and controls
#include "DatumUI.h"

// =============================================================================
// RIGHT PANEL COMPONENTS
// =============================================================================

void DatumUI::RenderRightPanel() {
  ImGui::Text("Visual Display");
  ImGui::Separator();

  RenderVisualDisplayControls();
  ImGui::Spacing();
  RenderVisualDisplay();
}


void DatumUI::RenderVisualDisplayControls() {
  // Display options
  ImGui::Checkbox("Show Point Labels", &m_showPointLabels);
  ImGui::SameLine();
  ImGui::Checkbox("Show Edge Labels", &m_showEdgeLabels);

  ImGui::Checkbox("Show Axes", &m_showAxes);
  ImGui::SameLine();
  ImGui::Checkbox("Show Grid", &m_showGrid);

  // Enhanced scale control with 1mm:100px option
  ImGui::SliderFloat("Scale", &m_visualScale, 0.1f, 200.0f, "%.2f");

  // Quick scale buttons
  ImGui::Spacing();
  if (ImGui::Button("1mm:100px", ImVec2(80, 0))) {
    m_visualScale = 100.0f; // 1mm = 100 pixels
  }
  ImGui::SameLine();
  if (ImGui::Button("1mm:50px", ImVec2(80, 0))) {
    m_visualScale = 50.0f;
  }
  ImGui::SameLine();
  if (ImGui::Button("1mm:200px", ImVec2(80, 0))) {
    m_visualScale = 200.0f;
  }

  // Offset controls
  ImGui::SliderFloat("Offset X", &m_visualOffsetX, -1000.0f, 1000.0f, "%.1f");
  ImGui::SliderFloat("Offset Y", &m_visualOffsetY, -1000.0f, 1000.0f, "%.1f");

  // Zoom buttons
  ImGui::Spacing();
  if (ImGui::Button("Zoom In", ImVec2(60, 0))) {
    m_visualScale *= 1.5f;
    if (m_visualScale > 500.0f) m_visualScale = 500.0f;
  }
  ImGui::SameLine();
  if (ImGui::Button("Zoom Out", ImVec2(60, 0))) {
    m_visualScale /= 1.5f;
    if (m_visualScale < 0.1f) m_visualScale = 0.1f;
  }
  ImGui::SameLine();
  if (ImGui::Button("Auto Zoom", ImVec2(70, 0))) {
    AutoZoomToFitPoints();
  }

  if (ImGui::Button("Reset View", ImVec2(-1, 0))) {
    m_visualScale = 100.0f; // Default to 1mm:100px
    m_visualOffsetX = 0.0f;
    m_visualOffsetY = 0.0f;
  }

  // Display current scale info
  ImGui::Spacing();
  ImGui::Text("Current Scale: 1mm = %.1f pixels", m_visualScale);
  float mmPerPixel = 1.0f / m_visualScale;
  ImGui::Text("Resolution: %.4f mm/pixel", mmPerPixel);
}



void DatumUI::AutoZoomToFitPoints() {
  if (m_selectedProductName.empty()) {
    return;
  }

  const auto* product = m_referenceManager->GetProductReference(m_selectedProductName);
  if (!product || product->points.empty()) {
    return;
  }

  // Find valid points and calculate bounding box
  std::vector<const ProductReferenceManager::Point3D*> validPoints;
  for (const auto& point : product->points) {
    if (point.isValid) {
      validPoints.push_back(&point);
    }
  }

  if (validPoints.empty()) {
    return;
  }

  // Calculate bounding box
  double minX = validPoints[0]->x, maxX = validPoints[0]->x;
  double minY = validPoints[0]->y, maxY = validPoints[0]->y;

  for (const auto* point : validPoints) {
    minX = std::min(minX, point->x);
    maxX = std::max(maxX, point->x);
    minY = std::min(minY, point->y);
    maxY = std::max(maxY, point->y);
  }

  // Calculate center and size
  double centerX = (minX + maxX) * 0.5;
  double centerY = (minY + maxY) * 0.5;
  double width = maxX - minX;
  double height = maxY - minY;

  // Add padding (20% on each side)
  width *= 1.4;
  height *= 1.4;

  // Handle case where all points are at the same location
  if (width < 0.1) width = 4.0;  // Default width if points are too close
  if (height < 0.1) height = 4.0;  // Default height if points are too close

  // Calculate scale to fit in canvas (assuming 400x400 pixel canvas)
  double canvasWidth = 400.0;
  double canvasHeight = 400.0;

  double scaleX = canvasWidth / width;
  double scaleY = canvasHeight / height;

  // Use the smaller scale to ensure everything fits
  m_visualScale = static_cast<float>(std::min(scaleX, scaleY));

  // Clamp scale to reasonable bounds
  if (m_visualScale < 0.1f) m_visualScale = 0.1f;
  if (m_visualScale > 10.0f) m_visualScale = 10.0f;

  // Center the view on the data
  m_visualOffsetX = static_cast<float>(-centerX * m_visualScale);
  m_visualOffsetY = static_cast<float>(centerY * m_visualScale);  // Flip Y for screen coordinates
}

void DatumUI::RenderVisualDisplay() {
  if (m_selectedProductName.empty()) {
    ImGui::TextDisabled("Select a product to view visualization");
    return;
  }

  const auto* product = m_referenceManager->GetProductReference(m_selectedProductName);
  if (!product) return;

  // Get drawing canvas
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();
  ImVec2 canvasSize = ImGui::GetContentRegionAvail();
  canvasSize.y -= 40; // Leave space for coordinate info at bottom

  if (canvasSize.x < 50 || canvasSize.y < 50) {
    ImGui::TextDisabled("Canvas too small");
    return;
  }

  ImDrawList* drawList = ImGui::GetWindowDrawList();

  // Draw canvas background
  ImU32 canvasBg = ImGui::GetColorU32(ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
  drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), canvasBg);

  // Draw border
  ImU32 borderColor = ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
  drawList->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), borderColor);

  // Draw grid if enabled
  if (m_showGrid) {
    DrawGrid(drawList, canvasPos, canvasSize);
  }

  // Draw coordinate axes if enabled and datum exists
  if (m_showAxes && product->datum.constructionMethod != ProductReferenceManager::DatumReference::ConstructionMethod::NONE) {
    DrawCoordinateAxes(drawList, canvasPos, canvasSize);
  }

  // Draw edges
  for (const auto& edge : product->edges) {
    const auto* point1 = m_referenceManager->GetPoint(m_selectedProductName, edge.point1Name);
    const auto* point2 = m_referenceManager->GetPoint(m_selectedProductName, edge.point2Name);

    if (point1 && point2 && point1->isValid && point2->isValid) {
      ImU32 edgeColor = ImGui::GetColorU32(ImVec4(0.7f, 0.7f, 0.7f, 1.0f));

      // Highlight datum axes
      if (edge.name == product->datum.xAxisEdgeName) {
        edgeColor = ImGui::GetColorU32(ImVec4(1.0f, 0.3f, 0.3f, 1.0f)); // Red for X-axis
      }
      else if (edge.name == product->datum.yAxisEdgeName) {
        edgeColor = ImGui::GetColorU32(ImVec4(0.3f, 1.0f, 0.3f, 1.0f)); // Green for Y-axis
      }

      DrawEdge(drawList, *point1, *point2, canvasPos, canvasSize, edgeColor);

      // Draw edge labels if enabled
      if (m_showEdgeLabels) {
        ImVec2 midPoint = WorldToScreen(*point1, canvasPos, canvasSize);
        ImVec2 endPoint = WorldToScreen(*point2, canvasPos, canvasSize);
        midPoint.x = (midPoint.x + endPoint.x) * 0.5f;
        midPoint.y = (midPoint.y + endPoint.y) * 0.5f;

        ImU32 labelColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.8f));
        drawList->AddText(midPoint, labelColor, edge.name.c_str());
      }
    }
  }

  // Draw points (on top of edges)
  for (const auto& point : product->points) {
    if (point.isValid) {
      ImU32 pointColor = ImGui::GetColorU32(ImVec4(0.8f, 0.8f, 0.8f, 1.0f));

      // Highlight origin point
      if (point.name == product->datum.originPointName) {
        pointColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 0.3f, 1.0f)); // Yellow for origin
      }

      // Highlight selected point
      if (point.name == m_selectedPointName) {
        pointColor = ImGui::GetColorU32(ImVec4(0.3f, 0.7f, 1.0f, 1.0f)); // Blue for selected
      }

      DrawPoint(drawList, point, canvasPos, canvasSize, pointColor);

      // Draw point labels if enabled
      //if (m_showPointLabels) {
      //  ImVec2 pointPos = WorldToScreen(point, canvasPos, canvasSize);
      //  pointPos.y -= 15; // Offset label above point
      //  ImU32 labelColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
      //  drawList->AddText(pointPos, labelColor, point.name.c_str());
      //}
    }
  }

  // Make canvas area interactive for future expansion (clicking to select points)
  ImGui::InvisibleButton("Canvas", canvasSize);

  // Handle mouse wheel zoom
  if (ImGui::IsItemHovered()) {
    float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0f) {
      // Zoom towards mouse position
      ImVec2 mousePos = ImGui::GetMousePos();
      ImVec2 canvasMousePos = ImVec2(mousePos.x - canvasPos.x, mousePos.y - canvasPos.y);

      float oldScale = m_visualScale;
      float zoomFactor = wheel > 0 ? 1.2f : 1.0f / 1.2f;
      m_visualScale *= zoomFactor;

      // Clamp scale
      if (m_visualScale < 0.1f) m_visualScale = 0.1f;
      if (m_visualScale > 10.0f) m_visualScale = 10.0f;

      // Adjust offset to zoom towards mouse position
      float scaleChange = m_visualScale / oldScale;
      m_visualOffsetX = (m_visualOffsetX - (canvasMousePos.x - canvasSize.x * 0.5f)) * scaleChange + (canvasMousePos.x - canvasSize.x * 0.5f);
      m_visualOffsetY = (m_visualOffsetY - (canvasMousePos.y - canvasSize.y * 0.5f)) * scaleChange + (canvasMousePos.y - canvasSize.y * 0.5f);
    }

    // Handle middle mouse button drag for panning
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
      ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
      m_visualOffsetX += delta.x;
      m_visualOffsetY += delta.y;
      ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
    }
  }

  // Display coordinate info and legend at bottom
  ImGui::Spacing();

  // Selected point info
  if (!m_selectedPointName.empty()) {
    const auto* selectedPoint = m_referenceManager->GetPoint(m_selectedProductName, m_selectedPointName);
    if (selectedPoint && selectedPoint->isValid) {
      ImGui::Text("Selected: %s (%.3f, %.3f, %.3f)",
        selectedPoint->name.c_str(), selectedPoint->x, selectedPoint->y, selectedPoint->z);
    }
  }

  // Color legend
  ImGui::Separator();
  ImGui::Text("Legend:");

  // Origin point
  if (!product->datum.originPointName.empty()) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "@ Origin");
  }

  // X-axis
  if (!product->datum.xAxisEdgeName.empty()) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "— X-axis");
  }

  // Y-axis  
  if (!product->datum.yAxisEdgeName.empty()) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "— Y-axis");
  }

  // Selected point
  if (!m_selectedPointName.empty()) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "@ Selected");
  }
}