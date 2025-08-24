// UIVisionPanel_AutoExecution.cpp - Auto-execution functionality
#include "UIVisionPanel.h"
#include <iostream>

void UIVisionPanel::UpdateAutoExecution() {
  if (!m_autoExecute) {
    return;
  }

  bool canExecute = m_cameraManager && !m_selectedCameraId.empty() && m_circleDetector;
  if (!canExecute) {
    return;
  }

  float currentTime = static_cast<float>(ImGui::GetTime()) * 1000.0f;

  if (currentTime - m_lastAutoExecuteTime >= m_autoExecuteInterval) {
    ExecuteCircleDetection();
    m_lastAutoExecuteTime = currentTime;
  }
}

void UIVisionPanel::RenderAutoExecutionControls() {
  ImGui::Text("Auto Execution");

  bool oldAutoExecute = m_autoExecute;
  if (ImGui::Checkbox("Enable Auto Execution", &m_autoExecute)) {
    if (m_autoExecute && !oldAutoExecute) {
      m_lastAutoExecuteTime = static_cast<float>(ImGui::GetTime()) * 1000.0f;
      std::cout << "[UIVisionPanel] Auto-execution started (interval: " << m_autoExecuteInterval << "ms)" << std::endl;
    }
    else if (!m_autoExecute && oldAutoExecute) {
      std::cout << "[UIVisionPanel] Auto-execution stopped" << std::endl;
    }
  }

  if (m_autoExecute) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "ACTIVE");
  }

  float intervalMs = m_autoExecuteInterval;
  if (ImGui::SliderFloat("Interval (ms)", &intervalMs, 50.0f, 2000.0f, "%.0f ms")) {
    m_autoExecuteInterval = intervalMs;
  }

  ImGui::Text("Quick Presets:");
  if (ImGui::Button("50ms", ImVec2(45, 25))) {
    m_autoExecuteInterval = 50.0f;
  }
  ImGui::SameLine();
  if (ImGui::Button("100ms", ImVec2(50, 25))) {
    m_autoExecuteInterval = 100.0f;
  }
  ImGui::SameLine();
  if (ImGui::Button("200ms", ImVec2(50, 25))) {
    m_autoExecuteInterval = 200.0f;
  }
  ImGui::SameLine();
  if (ImGui::Button("500ms", ImVec2(50, 25))) {
    m_autoExecuteInterval = 500.0f;
  }
  ImGui::SameLine();
  if (ImGui::Button("1s", ImVec2(35, 25))) {
    m_autoExecuteInterval = 1000.0f;
  }

  if (m_autoExecute) {
    float fps = 1000.0f / m_autoExecuteInterval;
    ImGui::Text("Execution rate: %.1f Hz (%.0f ms)", fps, m_autoExecuteInterval);

    float currentTime = static_cast<float>(ImGui::GetTime()) * 1000.0f;
    float timeUntilNext = m_autoExecuteInterval - (currentTime - m_lastAutoExecuteTime);
    if (timeUntilNext > 0) {
      ImGui::Text("Next execution in: %.0f ms", timeUntilNext);
    }
    else {
      ImGui::Text("Executing...");
    }
  }
  else {
    ImGui::Text("Manual execution only");
  }

  if (m_autoExecuteInterval < 100.0f) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Warning: High frequency may impact performance");
  }
}