// UIVisionPanel_Detection.cpp - Circle detection UI and execution
#include "UIVisionPanel.h"
#include <iostream>

void UIVisionPanel::RenderCircleDetectionControls() {
  ImGui::Text("Circle Detection");

  if (!m_circleDetector) {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Circle detector not initialized");
    return;
  }

  // Main execute button
  bool canExecute = m_cameraManager && !m_selectedCameraId.empty();

  if (!canExecute) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }

  // Change button color if auto-execution is active
  if (m_autoExecute) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.4f, 0.2f, 1.0f));  // Orange for auto mode
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.5f, 0.3f, 1.0f));
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));  // Green for manual
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
  }

  std::string buttonText = m_autoExecute ? "Auto Detection Running" : "Manual Detection";
  if (ImGui::Button(buttonText.c_str(), ImVec2(-1, 40))) {
    if (canExecute && !m_autoExecute) {  // Only allow manual execution when auto is off
      ExecuteCircleDetection();
    }
  }

  ImGui::PopStyleColor(2);

  if (!canExecute) {
    ImGui::PopStyleVar();
  }

  if (!canExecute) {
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Select and connect a camera first");
  }
  else if (m_autoExecute) {
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Auto-execution active (%.0f ms intervals)", m_autoExecuteInterval);
  }

  // Processing time display
  if (m_hasResult) {
    ImGui::Text("Last processing time: %.1f ms", m_circleDetector->GetLastProcessingTime());
  }
}

void UIVisionPanel::RenderCircleDetectionResults() {
  if (!m_hasResult) {
    ImGui::Text("No detection results yet.");
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
    if (ImGui::BeginTable("Results", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120.0f);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Center X");
      ImGui::TableNextColumn();
      ImGui::Text("%.1f pixels", result.centerX);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Center Y");
      ImGui::TableNextColumn();
      ImGui::Text("%.1f pixels", result.centerY);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Radius");
      ImGui::TableNextColumn();
      ImGui::Text("%.1f pixels", result.radius);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Confidence");
      ImGui::TableNextColumn();
      ImGui::Text("%.1f%%", result.confidence * 100.0);

      ImGui::EndTable();
    }

    ImGui::Spacing();

    if (ImGui::Button("Send to Robot", ImVec2(-1, 30))) {
      std::cout << "[UIVisionPanel] Sending coordinates to robot: ("
        << result.centerX << ", " << result.centerY << ")" << std::endl;
    }
  }
  else {
    ImGui::Text("Detection failed:");
    ImGui::BulletText("Candidates found: %d", result.numCandidates);

    if (!result.errorMessage.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Error: %s", result.errorMessage.c_str());
    }
  }
}

void UIVisionPanel::ExecuteCircleDetection() {
  if (!m_circleDetector || !m_cameraManager || m_selectedCameraId.empty()) {
    std::cout << "[UIVisionPanel] Cannot execute: missing components" << std::endl;
    return;
  }

  std::vector<uint8_t> imageBuffer;
  int width, height, channels;

  if (!CaptureImageFromCamera(imageBuffer, width, height, channels)) {
    return;
  }

  m_lastResult = m_circleDetector->DetectFromBuffer(imageBuffer.data(), width, height, channels);
  m_hasResult = true;

  if (m_lastResult.found) {
    // Optional: uncomment for debug output
    // std::cout << "[UIVisionPanel] Circle detected at (" << m_lastResult.centerX
    //           << ", " << m_lastResult.centerY << ") with radius " << m_lastResult.radius << std::endl;
  }
  else {
    // std::cout << "[UIVisionPanel] No circle detected. Candidates: " << m_lastResult.numCandidates << std::endl;
  }
}