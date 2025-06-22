
#pragma once
#include "imgui.h"
#include "raylibclass.h"
#include <memory>
#include "include/logger.h"
class RaylibToggleButton {
public:
  RaylibToggleButton(std::unique_ptr<RaylibWindow>* raylibWindow, Logger* logger)
    : m_raylibWindow(raylibWindow), m_logger(logger) {
  }

  void RenderUI() {
    if (ImGui::Begin("Aux Window Control")) {

      bool isOpen = m_raylibWindow && *m_raylibWindow && (*m_raylibWindow)->IsRunning();

      if (isOpen) {
        // Window is open - show close button
        if (ImGui::Button("Close Aux Window", ImVec2(150, 30))) {
          if (*m_raylibWindow) {
            (*m_raylibWindow)->Shutdown();
            m_raylibWindow->reset();
            if (m_logger) m_logger->LogInfo("Aux Window closed via toolbar");
          }
        }

        ImGui::Spacing();
        ImGui::Text("Status: Open");
        if ((*m_raylibWindow)->IsVisible()) {
          ImGui::Text("Visible: Yes");
        }
        else {
          ImGui::Text("Visible: No");
        }

      }
      else {
        // Window is closed - show open button
        if (ImGui::Button("Open Aux Window", ImVec2(150, 30))) {
          *m_raylibWindow = std::make_unique<RaylibWindow>();
          (*m_raylibWindow)->SetLogger(m_logger);

          if ((*m_raylibWindow)->Initialize()) {
            if (m_logger) m_logger->LogInfo("Aux Window opened via toolbar");
          }
          else {
            if (m_logger) m_logger->LogError("Failed to open Aux Window via toolbar");
            m_raylibWindow->reset();
          }
        }

        ImGui::Spacing();
        ImGui::Text("Status: Closed");
      }
    }
    ImGui::End();
  }

  bool IsVisible() const { return isVisible; }
  void ToggleWindow() { isVisible = !isVisible; }

private:
  std::unique_ptr<RaylibWindow>* m_raylibWindow;
  Logger* m_logger;
  bool isVisible = false;
};