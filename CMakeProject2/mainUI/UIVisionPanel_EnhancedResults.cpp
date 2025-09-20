// UIVisionPanel_EnhancedResults.cpp - Enhanced Results Display Methods
#include "UIVisionPanel.h"
#include "include/machine_operations.h"
#include "VisionCoordinateCalculator.h"
#include "imgui.h"
#include <iostream>
#include <iomanip>
#include <sstream>

void UIVisionPanel::RenderCircleDetectionResults() {
  if (!m_hasResult) {
    ImGui::Text("No detection performed yet.");
    ImGui::Text("Execute circle detection to see results.");
    return;
  }

  const auto& result = m_lastResult;

  ImGui::SetWindowFontScale(1.2f);
  if (result.found) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Circle Detected");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "✗ No Circle Found");
  }
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();

  if (result.found) {
    // Display options
    RenderResultsDisplayOptions();
    ImGui::Spacing();

    // Choose which table to show based on user preference
    if (m_showEnhancedResults) {
      RenderEnhancedResultsTable();
    }
    else {
      RenderBasicResultsTable();
    }

    ImGui::Spacing();
    RenderCoordinateActions();
  }
  else {
    ImGui::Text("Detection failed:");
    ImGui::BulletText("Candidates found: %d", result.numCandidates);

    if (!result.errorMessage.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Error: %s", result.errorMessage.c_str());
    }
  }
}

void UIVisionPanel::RenderResultsDisplayOptions() {
  ImGui::Text("Display Options:");

  if (ImGui::Checkbox("Enhanced Results", &m_showEnhancedResults)) {
    // User toggled enhanced results
  }

  if (m_showEnhancedResults) {
    ImGui::SameLine();
    if (ImGui::Button("Settings")) {
      ImGui::OpenPopup("ResultsDisplaySettings");
    }

    if (ImGui::BeginPopup("ResultsDisplaySettings")) {
      ImGui::Checkbox("Show Pixel Coordinates", &m_showPixelCoordinates);
      ImGui::Checkbox("Show Offset from Center", &m_showOffsetFromCenter);
      ImGui::Checkbox("Show Robot Position", &m_showRobotPosition);
      ImGui::Checkbox("Show Target Position", &m_showTargetPosition);
      ImGui::EndPopup();
    }
  }
}

void UIVisionPanel::RenderEnhancedResultsTable() {
  if (!m_coordinateCalculator) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
      "Coordinate calculator not initialized");
    return;
  }

  // Calculate real-world coordinates
  auto coords = m_coordinateCalculator->CalculateCoordinates(
    m_lastResult.centerX, m_lastResult.centerY, m_lastResult.radius,
    m_imageWidth, m_imageHeight);

  // Enhanced results table with 3 columns
  if (ImGui::BeginTable("EnhancedResults", 3,
    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {

    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 140.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 100.0f);
    ImGui::TableSetupColumn("Unit/Info", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    // === PIXEL COORDINATES SECTION ===
    if (m_showPixelCoordinates) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "PIXEL COORDINATES");
      ImGui::TableNextColumn();
      ImGui::TableNextColumn();

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Center X");
      ImGui::TableNextColumn();
      ImGui::Text("%.1f", coords.pixelX);
      ImGui::TableNextColumn();
      ImGui::Text("pixels");

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Center Y");
      ImGui::TableNextColumn();
      ImGui::Text("%.1f", coords.pixelY);
      ImGui::TableNextColumn();
      ImGui::Text("pixels");

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Radius");
      ImGui::TableNextColumn();
      ImGui::Text("%.1f", coords.pixelRadius);
      ImGui::TableNextColumn();
      ImGui::Text("pixels");

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Image Size");
      ImGui::TableNextColumn();
      ImGui::Text("%dx%d", coords.imageWidth, coords.imageHeight);
      ImGui::TableNextColumn();
      ImGui::Text("pixels");
    }

    // === OFFSET FROM CENTER SECTION ===
    if (m_showOffsetFromCenter) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.8f, 1.0f), "OFFSET FROM CENTER");
      ImGui::TableNextColumn();
      ImGui::TableNextColumn();

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Offset X");
      ImGui::TableNextColumn();
      ImGui::Text("%.1f", coords.offsetPixelX);
      ImGui::TableNextColumn();
      ImGui::Text("pixels (%.3f mm)", coords.offsetMmX);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Offset Y");
      ImGui::TableNextColumn();
      ImGui::Text("%.1f", coords.offsetPixelY);
      ImGui::TableNextColumn();
      ImGui::Text("pixels (%.3f mm)", coords.offsetMmY);
    }

    // === ROBOT COORDINATES SECTION ===
    if (m_showRobotPosition) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      if (coords.hasRobotPosition) {
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "ROBOT POSITION");
      }
      else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "ROBOT POSITION (N/A)");
      }
      ImGui::TableNextColumn();
      ImGui::TableNextColumn();

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Current X");
      ImGui::TableNextColumn();
      if (coords.hasRobotPosition) {
        ImGui::Text("%.3f", coords.robotX);
      }
      else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "N/A");
      }
      ImGui::TableNextColumn();
      ImGui::Text("mm");

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Current Y");
      ImGui::TableNextColumn();
      if (coords.hasRobotPosition) {
        ImGui::Text("%.3f", coords.robotY);
      }
      else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "N/A");
      }
      ImGui::TableNextColumn();
      ImGui::Text("mm");

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Current Z");
      ImGui::TableNextColumn();
      if (coords.hasRobotPosition) {
        ImGui::Text("%.3f", coords.robotZ);
      }
      else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "N/A");
      }
      ImGui::TableNextColumn();
      ImGui::Text("mm");
    }

    // === TARGET COORDINATES SECTION ===
    if (m_showTargetPosition) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.8f, 1.0f), "TARGET POSITION");
      ImGui::TableNextColumn();
      ImGui::TableNextColumn();
      ImGui::Text("(Robot + Offset)");

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Target X");
      ImGui::TableNextColumn();
      ImGui::Text("%.3f", coords.targetX);
      ImGui::TableNextColumn();
      ImGui::Text("mm");

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Target Y");
      ImGui::TableNextColumn();
      ImGui::Text("%.3f", coords.targetY);
      ImGui::TableNextColumn();
      ImGui::Text("mm");

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Target Z");
      ImGui::TableNextColumn();
      ImGui::Text("%.3f", coords.targetZ);
      ImGui::TableNextColumn();
      ImGui::Text("mm");
    }

    // === CALIBRATION INFO SECTION ===
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (coords.hasCalibration) {
      ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "CALIBRATION INFO");
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "CALIBRATION (DEFAULT)");
    }
    ImGui::TableNextColumn();
    ImGui::TableNextColumn();

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Pixel to MM X");
    ImGui::TableNextColumn();
    ImGui::Text("%.6f", coords.pixelToMmFactorX);
    ImGui::TableNextColumn();
    ImGui::Text("mm/pixel");

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Pixel to MM Y");
    ImGui::TableNextColumn();
    ImGui::Text("%.6f", coords.pixelToMmFactorY);
    ImGui::TableNextColumn();
    ImGui::Text("mm/pixel");

    // === ADDITIONAL INFO ===
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "DETECTION INFO");
    ImGui::TableNextColumn();
    ImGui::TableNextColumn();

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Confidence");
    ImGui::TableNextColumn();
    ImGui::Text("%.1f%%", m_lastResult.confidence * 100.0);
    ImGui::TableNextColumn();
    ImGui::Text("detection quality");

    ImGui::EndTable();
  }
}

void UIVisionPanel::RenderBasicResultsTable() {
  // Simple table showing only pixel coordinates (original functionality)
  if (ImGui::BeginTable("BasicResults", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Center X");
    ImGui::TableNextColumn();
    ImGui::Text("%.1f pixels", m_lastResult.centerX);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Center Y");
    ImGui::TableNextColumn();
    ImGui::Text("%.1f pixels", m_lastResult.centerY);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Radius");
    ImGui::TableNextColumn();
    ImGui::Text("%.1f pixels", m_lastResult.radius);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Confidence");
    ImGui::TableNextColumn();
    ImGui::Text("%.1f%%", m_lastResult.confidence * 100.0);

    ImGui::EndTable();
  }
}

void UIVisionPanel::RenderCoordinateActions() {
  if (!m_coordinateCalculator || !m_lastResult.found) {
    return;
  }

  // Calculate coordinates for actions
  auto coords = m_coordinateCalculator->CalculateCoordinates(
    m_lastResult.centerX, m_lastResult.centerY, m_lastResult.radius,
    m_imageWidth, m_imageHeight);

  // Static variables to track move status across frames
  static bool showMoveSuccess = false;
  static bool showMoveError = false;
  static VisionCoordinateCalculator::CoordinateSet lastMoveCoords;
  static double lastUsedZ = 0.0;

  // Action button with conditional enabling
  ImGui::BeginDisabled(!m_machineOperations);

  if (ImGui::Button("Send to Robot", ImVec2(120, 25))) {
    if (m_machineOperations) {
      // Debug: Log all the coordinate values
      std::cout << "[UIVisionPanel] Debug Coordinates:" << std::endl;
      std::cout << "  Detection pixel: (" << m_lastResult.centerX << ", " << m_lastResult.centerY << ")" << std::endl;
      std::cout << "  Image center: (" << coords.imageCenterX << ", " << coords.imageCenterY << ")" << std::endl;
      std::cout << "  Pixel offset: (" << coords.offsetPixelX << ", " << coords.offsetPixelY << ")" << std::endl;
      std::cout << "  MM offset: (" << coords.offsetMmX << ", " << coords.offsetMmY << ")" << std::endl;

      if (coords.hasRobotPosition) {
        std::cout << "  Current robot: (" << coords.robotX << ", " << coords.robotY << ", " << coords.robotZ << ")" << std::endl;
      }

      std::cout << "  Target position: (" << coords.targetX << ", " << coords.targetY << ", " << coords.targetZ << ")" << std::endl;

      // The target should be: current_position + offset
      // If coords.targetX/Y are wrong, calculate them here:
      double correctedTargetX = coords.hasRobotPosition ? (coords.robotX + coords.offsetMmX) : coords.offsetMmX;
      double correctedTargetY = coords.hasRobotPosition ? (coords.robotY + coords.offsetMmY) : coords.offsetMmY;
      double targetZ = coords.hasRobotPosition ? coords.robotZ : coords.targetZ;

      std::cout << "[UIVisionPanel] Corrected target: ("
        << correctedTargetX << ", " << correctedTargetY << ", " << targetZ << ")" << std::endl;

      // Create PositionStruct for the move
      PositionStruct targetPosition;
      targetPosition.x = correctedTargetX;  // Use corrected values
      targetPosition.y = correctedTargetY;
      targetPosition.z = targetZ;

      // Move to new position
      bool moveSuccess = m_machineOperations->MoveDeviceToPosition(
        "gantry-main",
        targetPosition,
        true,
        "VisionCorrection"
      );

      if (moveSuccess) {
        std::cout << "[UIVisionPanel] Robot movement command sent successfully" << std::endl;
        showMoveSuccess = true;
        showMoveError = false;
        lastMoveCoords = coords;
        lastUsedZ = targetZ;
      }
      else {
        std::cout << "[UIVisionPanel] Failed to send robot movement command" << std::endl;
        showMoveSuccess = false;
        showMoveError = true;
      }
    }
  }

  ImGui::EndDisabled();

  // Show informative text if machine operations not set
  if (!m_machineOperations) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
      "Machine operations not configured!");

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("SetMachineOperations() needs to be called during initialization");
    }
  }

  // Copy coordinates button
  ImGui::SameLine();
  if (ImGui::Button("Copy Coordinates", ImVec2(120, 30))) {
    std::string coordText = VisionCoordinateCalculator::FormatCoordinateSet(coords);
    ImGui::SetClipboardText(coordText.c_str());
    std::cout << "[UIVisionPanel] Coordinates copied to clipboard: " << coordText << std::endl;
  }

  // Show details button
  ImGui::SameLine();
  if (ImGui::Button("Show Details", ImVec2(100, 30))) {
    ImGui::OpenPopup("CoordinateDetails");
  }

  // Handle popups - open them after button checks
  if (showMoveSuccess) {
    ImGui::OpenPopup("RobotMoveSuccess");
    showMoveSuccess = false;
  }

  if (showMoveError) {
    ImGui::OpenPopup("RobotMoveError");
    showMoveError = false;
  }

  // Success popup
  if (ImGui::BeginPopupModal("RobotMoveSuccess", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
      "✓ Command sent to robot");
    ImGui::Text("Target: (%.3f, %.3f, %.3f)",
      lastMoveCoords.targetX, lastMoveCoords.targetY, lastUsedZ);
    ImGui::Text("Move type: Vision-guided correction");
    ImGui::Text("Context: VisionCorrection");

    ImGui::Spacing();
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  // Error popup
  if (ImGui::BeginPopupModal("RobotMoveError", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
      "✗ Failed to send command");
    ImGui::Text("Please check:");
    ImGui::BulletText("Robot connection status");
    ImGui::BulletText("Device 'gantry-main' is available");
    ImGui::BulletText("Target position is within limits");

    ImGui::Spacing();
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  // Coordinate details popup (non-modal)
  if (ImGui::BeginPopup("CoordinateDetails")) {
    ImGui::Text("Detailed Coordinate Information");
    ImGui::Separator();

    ImGui::Text("Image Center: (%.1f, %.1f)", coords.imageCenterX, coords.imageCenterY);
    ImGui::Text("Detection: (%.1f, %.1f) pixels", coords.pixelX, coords.pixelY);
    ImGui::Text("Detection Offset: (%.1f, %.1f) pixels", coords.offsetPixelX, coords.offsetPixelY);
    ImGui::Text("Offset in MM: (%.3f, %.3f)", coords.offsetMmX, coords.offsetMmY);

    ImGui::Separator();
    if (coords.hasRobotPosition) {
      ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "Robot Position Available");
      ImGui::Text("Current Robot: (%.3f, %.3f, %.3f) mm", coords.robotX, coords.robotY, coords.robotZ);
      ImGui::Text("Target Position: (%.3f, %.3f, %.3f) mm", coords.targetX, coords.targetY, coords.robotZ);
      ImGui::Text("Move Distance: %.3f mm",
        VisionCoordinateCalculator::CalculateDistance2D(
          coords.robotX, coords.robotY, coords.targetX, coords.targetY));
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Robot Position Not Available");
      ImGui::Text("Using offset from image center");
      ImGui::Text("Target Offset: (%.3f, %.3f) mm", coords.targetX, coords.targetY);
    }

    ImGui::Separator();
    ImGui::Text("Calibration: %s", coords.hasCalibration ? "Custom Loaded" : "Default Values");
    ImGui::Text("Scale Factor X: %.6f mm/pixel", coords.pixelToMmFactorX);
    ImGui::Text("Scale Factor Y: %.6f mm/pixel", coords.pixelToMmFactorY);

    ImGui::Spacing();
    if (ImGui::Button("Close", ImVec2(80, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}
void UIVisionPanel::RenderCoordinateSystemInfo() {
  if (!m_coordinateCalculator) {
    return;
  }

  const auto& calibData = m_coordinateCalculator->GetCalibrationData();

  ImGui::Text("Coordinate System Status:");

  if (calibData.isValid) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Calibration Loaded");
    ImGui::Text("Source: %s", calibData.sourceFile.c_str());
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "! Using Default Values");
  }

  bool hasRobotPos = false;
  double robotX, robotY, robotZ;
  hasRobotPos = m_coordinateCalculator->GetCurrentRobotPosition(robotX, robotY, robotZ);

  if (hasRobotPos) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Robot Position Available");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "! Robot Position N/A");
    if (!m_coordinateCalculator->GetValidationMessage().empty()) {
      ImGui::Text("Reason: %s", m_coordinateCalculator->GetValidationMessage().c_str());
    }
  }
}