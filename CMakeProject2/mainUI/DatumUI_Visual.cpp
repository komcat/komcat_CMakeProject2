// DatumUI_Visual.cpp - 2D drawing functions for visual display
#include "DatumUI.h"
#include <algorithm>
#include <cmath>

// =============================================================================
// VISUAL DISPLAY HELPER METHODS
// =============================================================================


void DatumUI::DrawPoint(ImDrawList* drawList, const ProductReferenceManager::Point3D& point,
  ImVec2 canvasPos, ImVec2 canvasSize, ImU32 color) {
  ImVec2 pointPos = WorldToScreen(point, canvasPos, canvasSize);

  // Larger point size for better visibility
  float pointRadius = 8.0f;

  // Draw filled circle for point
  drawList->AddCircleFilled(pointPos, pointRadius, color);

  // Draw border around point
  ImU32 borderColor = ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
  drawList->AddCircle(pointPos, pointRadius, borderColor, 0, 2.0f);

  // Draw crosshair to indicate exact center
  float crosshairSize = 12.0f;
  ImU32 crosshairColor = ImGui::GetColorU32(ImVec4(0.1f, 0.1f, 0.1f, 0.8f));

  // Horizontal line
  drawList->AddLine(
    ImVec2(pointPos.x - crosshairSize, pointPos.y),
    ImVec2(pointPos.x + crosshairSize, pointPos.y),
    crosshairColor, 1.0f
  );

  // Vertical line
  drawList->AddLine(
    ImVec2(pointPos.x, pointPos.y - crosshairSize),
    ImVec2(pointPos.x, pointPos.y + crosshairSize),
    crosshairColor, 1.0f
  );

  // Draw coordinate label (X, Y values)
  if (m_showPointLabels) {
    // Format coordinates to 3 decimal places
    char coordText[64];
    snprintf(coordText, sizeof(coordText), "(%.3f, %.3f)", point.x, point.y);

    // Position label below and to the right of point
    ImVec2 labelPos = ImVec2(pointPos.x + pointRadius + 5, pointPos.y + pointRadius + 5);

    // Background for better readability
    ImVec2 textSize = ImGui::CalcTextSize(coordText);
    ImU32 bgColor = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.7f));
    drawList->AddRectFilled(
      ImVec2(labelPos.x - 2, labelPos.y - 2),
      ImVec2(labelPos.x + textSize.x + 2, labelPos.y + textSize.y + 2),
      bgColor
    );

    // Draw coordinate text
    ImU32 labelColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    drawList->AddText(labelPos, labelColor, coordText);

    // Also draw point name above the coordinates
    ImVec2 namePos = ImVec2(pointPos.x - textSize.x * 0.5f, pointPos.y - pointRadius - 20);
    drawList->AddText(namePos, labelColor, point.name.c_str());
  }
}

void DatumUI::DrawEdge(ImDrawList* drawList,
  const ProductReferenceManager::Point3D& point1,
  const ProductReferenceManager::Point3D& point2,
  ImVec2 canvasPos, ImVec2 canvasSize, ImU32 color, float thickness) {
  ImVec2 pos1 = WorldToScreen(point1, canvasPos, canvasSize);
  ImVec2 pos2 = WorldToScreen(point2, canvasPos, canvasSize);

  drawList->AddLine(pos1, pos2, color, thickness);
}



void DatumUI::DrawCoordinateAxes(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize) {
  const auto* product = m_referenceManager->GetProductReference(m_selectedProductName);
  if (!product || product->datum.constructionMethod == ProductReferenceManager::DatumReference::ConstructionMethod::NONE) {
    return;
  }

  const auto* originPoint = m_referenceManager->GetPoint(m_selectedProductName, product->datum.originPointName);
  if (!originPoint || !originPoint->isValid) {
    return;
  }

  ImVec2 origin = WorldToScreen(*originPoint, canvasPos, canvasSize);

  // Draw X-axis (red) - Connect directly to the actual X-axis point
  if (!product->datum.xAxisEdgeName.empty()) {
    const auto* xAxisEdge = m_referenceManager->GetEdge(m_selectedProductName, product->datum.xAxisEdgeName);
    if (xAxisEdge) {
      const ProductReferenceManager::Point3D* xAxisPoint = nullptr;
      if (xAxisEdge->point1Name == product->datum.originPointName) {
        xAxisPoint = m_referenceManager->GetPoint(m_selectedProductName, xAxisEdge->point2Name);
      }
      else {
        xAxisPoint = m_referenceManager->GetPoint(m_selectedProductName, xAxisEdge->point1Name);
      }

      if (xAxisPoint && xAxisPoint->isValid) {
        // Connect directly to the actual axis point (no fixed length)
        ImVec2 xAxisEnd = WorldToScreen(*xAxisPoint, canvasPos, canvasSize);
        ImU32 xAxisColor = ImGui::GetColorU32(ImVec4(1.0f, 0.3f, 0.3f, 1.0f));

        // Draw thin line connecting actual points
        drawList->AddLine(origin, xAxisEnd, xAxisColor, 1.5f);

        // Calculate direction for arrow (only if points are different)
        float dx = xAxisEnd.x - origin.x;
        float dy = xAxisEnd.y - origin.y;
        float length = sqrt(dx * dx + dy * dy);

        if (length > 5.0f) { // Only draw arrow if line is long enough
          dx /= length;
          dy /= length;

          float arrowSize = 8.0f;
          ImVec2 arrowP1 = ImVec2(xAxisEnd.x - dx * arrowSize + dy * arrowSize * 0.5f,
            xAxisEnd.y - dy * arrowSize - dx * arrowSize * 0.5f);
          ImVec2 arrowP2 = ImVec2(xAxisEnd.x - dx * arrowSize - dy * arrowSize * 0.5f,
            xAxisEnd.y - dy * arrowSize + dx * arrowSize * 0.5f);
          drawList->AddTriangleFilled(xAxisEnd, arrowP1, arrowP2, xAxisColor);

          // Draw X label near the endpoint
          ImVec2 labelPos = ImVec2(xAxisEnd.x + 10, xAxisEnd.y - 10);
          drawList->AddText(labelPos, xAxisColor, "X");
        }
      }
    }
  }

  // Draw Y-axis (green) - Connect directly to the actual Y-axis point
  if (!product->datum.yAxisEdgeName.empty()) {
    const auto* yAxisEdge = m_referenceManager->GetEdge(m_selectedProductName, product->datum.yAxisEdgeName);
    if (yAxisEdge) {
      const ProductReferenceManager::Point3D* yAxisPoint = nullptr;
      if (yAxisEdge->point1Name == product->datum.originPointName) {
        yAxisPoint = m_referenceManager->GetPoint(m_selectedProductName, yAxisEdge->point2Name);
      }
      else {
        yAxisPoint = m_referenceManager->GetPoint(m_selectedProductName, yAxisEdge->point1Name);
      }

      if (yAxisPoint && yAxisPoint->isValid) {
        // Connect directly to the actual axis point (no fixed length)
        ImVec2 yAxisEnd = WorldToScreen(*yAxisPoint, canvasPos, canvasSize);
        ImU32 yAxisColor = ImGui::GetColorU32(ImVec4(0.3f, 1.0f, 0.3f, 1.0f));

        // Draw thin line connecting actual points
        drawList->AddLine(origin, yAxisEnd, yAxisColor, 1.5f);

        // Calculate direction for arrow (only if points are different)
        float dx = yAxisEnd.x - origin.x;
        float dy = yAxisEnd.y - origin.y;
        float length = sqrt(dx * dx + dy * dy);

        if (length > 5.0f) { // Only draw arrow if line is long enough
          dx /= length;
          dy /= length;

          float arrowSize = 8.0f;
          ImVec2 arrowP1 = ImVec2(yAxisEnd.x - dx * arrowSize + dy * arrowSize * 0.5f,
            yAxisEnd.y - dy * arrowSize - dx * arrowSize * 0.5f);
          ImVec2 arrowP2 = ImVec2(yAxisEnd.x - dx * arrowSize - dy * arrowSize * 0.5f,
            yAxisEnd.y - dy * arrowSize + dx * arrowSize * 0.5f);
          drawList->AddTriangleFilled(yAxisEnd, arrowP1, arrowP2, yAxisColor);

          // Draw Y label near the endpoint
          ImVec2 labelPos = ImVec2(yAxisEnd.x + 10, yAxisEnd.y - 10);
          drawList->AddText(labelPos, yAxisColor, "Y");
        }
      }
    }
  }
}


void DatumUI::DrawGrid(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize) {
  ImU32 gridColor = ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
  ImU32 majorGridColor = ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 0.8f));

  // Grid spacing based on metric units (mm)
  // For 1mm:100px scale, show grid every 1mm (100px) with major grid every 10mm (1000px)
  float gridSpacing_mm = 1.0f; // 1mm grid spacing
  float majorGridSpacing_mm = 10.0f; // 10mm major grid spacing

  float gridSpacing = gridSpacing_mm * m_visualScale;
  float majorGridSpacing = majorGridSpacing_mm * m_visualScale;

  // Adaptive grid spacing based on zoom level
  if (gridSpacing < 10.0f) {
    gridSpacing_mm = 5.0f;
    majorGridSpacing_mm = 50.0f;
    gridSpacing = gridSpacing_mm * m_visualScale;
    majorGridSpacing = majorGridSpacing_mm * m_visualScale;
  }
  if (gridSpacing < 10.0f) {
    gridSpacing_mm = 10.0f;
    majorGridSpacing_mm = 100.0f;
    gridSpacing = gridSpacing_mm * m_visualScale;
    majorGridSpacing = majorGridSpacing_mm * m_visualScale;
  }

  // Calculate grid origin (center of canvas plus offsets)
  ImVec2 gridOrigin = ImVec2(canvasPos.x + canvasSize.x * 0.5f + m_visualOffsetX,
    canvasPos.y + canvasSize.y * 0.5f + m_visualOffsetY);

  // Draw vertical grid lines
  for (float x = gridOrigin.x; x <= canvasPos.x + canvasSize.x; x += gridSpacing) {
    bool isMajor = (fmod(x - gridOrigin.x, majorGridSpacing) < 1.0f);
    ImU32 color = isMajor ? majorGridColor : gridColor;
    drawList->AddLine(ImVec2(x, canvasPos.y), ImVec2(x, canvasPos.y + canvasSize.y), color);
  }

  for (float x = gridOrigin.x - gridSpacing; x >= canvasPos.x; x -= gridSpacing) {
    bool isMajor = (fmod(gridOrigin.x - x, majorGridSpacing) < 1.0f);
    ImU32 color = isMajor ? majorGridColor : gridColor;
    drawList->AddLine(ImVec2(x, canvasPos.y), ImVec2(x, canvasPos.y + canvasSize.y), color);
  }

  // Draw horizontal grid lines
  for (float y = gridOrigin.y; y <= canvasPos.y + canvasSize.y; y += gridSpacing) {
    bool isMajor = (fmod(y - gridOrigin.y, majorGridSpacing) < 1.0f);
    ImU32 color = isMajor ? majorGridColor : gridColor;
    drawList->AddLine(ImVec2(canvasPos.x, y), ImVec2(canvasPos.x + canvasSize.x, y), color);
  }

  for (float y = gridOrigin.y - gridSpacing; y >= canvasPos.y; y -= gridSpacing) {
    bool isMajor = (fmod(gridOrigin.y - y, majorGridSpacing) < 1.0f);
    ImU32 color = isMajor ? majorGridColor : gridColor;
    drawList->AddLine(ImVec2(canvasPos.x, y), ImVec2(canvasPos.x + canvasSize.x, y), color);
  }

  // Draw grid scale indicator
  ImVec2 scalePos = ImVec2(canvasPos.x + 10, canvasPos.y + canvasSize.y - 30);
  char scaleText[64];
  snprintf(scaleText, sizeof(scaleText), "Grid: %.1fmm", gridSpacing_mm);
  ImU32 scaleTextColor = ImGui::GetColorU32(ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
  drawList->AddText(scalePos, scaleTextColor, scaleText);
}


ImVec2 DatumUI::WorldToScreen(const ProductReferenceManager::Point3D& point,
  ImVec2 canvasPos, ImVec2 canvasSize) {
  // Convert 3D world coordinates to 2D screen coordinates
  // Use double precision to avoid data loss, then convert to float at the end

  // Scale the coordinates with double precision
  double screenX = static_cast<double>(point.x) * static_cast<double>(m_visualScale);
  double screenY = static_cast<double>(point.y) * static_cast<double>(m_visualScale);

  // Flip Y coordinate (ImGui screen coordinates have Y increasing downward)
  screenY = -screenY;

  // Apply offsets and center on canvas (convert to double for precision)
  screenX += static_cast<double>(canvasPos.x) + static_cast<double>(canvasSize.x) * 0.5 + static_cast<double>(m_visualOffsetX);
  screenY += static_cast<double>(canvasPos.y) + static_cast<double>(canvasSize.y) * 0.5 + static_cast<double>(m_visualOffsetY);

  // Only convert to float at the final step for ImGui
  return ImVec2(static_cast<float>(screenX), static_cast<float>(screenY));
}