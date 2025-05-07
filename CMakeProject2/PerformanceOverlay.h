// PerformanceOverlay.h
#pragma once

#include "imgui.h"
#include "include/logger.h"
#include "include/ui/ToolbarMenu.h" // For ITogglableUI interface
#include <deque>
#include <vector>
#include <string>
#include <SDL.h>
#include <algorithm>

class PerformanceOverlay : public ITogglableUI {
public:
  PerformanceOverlay(size_t historySize = 60, const std::string& name = "Performance")
    : m_historySize(historySize),
    m_fpsHistory(historySize, 0.0f),
    m_frameTime(0.0f),
    m_fps(0.0f),
    m_frameCounter(0),
    m_fpsTimer(0.0f),
    m_fpsUpdateInterval(0.5f),
    m_position(ImVec2(10, 10)),
    m_bgAlpha(0.35f),
    m_showGraph(true),
    m_expanded(false),
    m_visible(false),
    m_name(name)
  {
    m_lastFrameTime = SDL_GetPerformanceCounter();
    m_logger = Logger::GetInstance();
    m_logger->LogInfo("PerformanceOverlay initialized");
  }

  virtual ~PerformanceOverlay() {
    if (m_logger) {
      m_logger->LogInfo("PerformanceOverlay destroyed");
    }
  }

  // Update FPS calculation - call this every frame
  void Update() {
    Uint64 currentFrameTime = SDL_GetPerformanceCounter();
    m_frameTime = (currentFrameTime - m_lastFrameTime) / (float)SDL_GetPerformanceFrequency();
    m_lastFrameTime = currentFrameTime;

    // Update frame counter
    m_frameCounter++;
    m_fpsTimer += m_frameTime;

    // Update FPS calculation every update interval
    if (m_fpsTimer >= m_fpsUpdateInterval) {
      m_fps = m_frameCounter / m_fpsTimer;

      // Add to history
      m_fpsHistory.pop_front();
      m_fpsHistory.push_back(m_fps);

      // Reset counters
      m_frameCounter = 0;
      m_fpsTimer = 0.0f;
    }
  }

  // Render the FPS display
  void RenderUI() {
    if (!m_visible) return;

    ImGui::SetNextWindowPos(m_position, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(m_bgAlpha);

    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize |
      ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoFocusOnAppearing;

    if (!m_expanded) {
      flags |= ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove;
    }

    if (ImGui::Begin(m_name.c_str(), m_expanded ? &m_visible : nullptr, flags)) {
      // Basic FPS counter
      ImGui::Text("FPS: %.1f", m_fps);

      // Show frame time
      ImGui::Text("Frame Time: %.2f ms", m_frameTime * 1000.0f);

      // Show expanded details
      if (m_expanded) {
        // Calculate average FPS
        float sum = 0.0f;
        for (float fps : m_fpsHistory) {
          sum += fps;
        }
        float avgFps = sum / static_cast<float>(m_fpsHistory.size());
        ImGui::Text("Avg FPS: %.1f", avgFps);

        // Calculate min/max FPS
        float minFps = *std::min_element(m_fpsHistory.begin(), m_fpsHistory.end());
        float maxFps = *std::max_element(m_fpsHistory.begin(), m_fpsHistory.end());
        ImGui::Text("Min/Max FPS: %.1f / %.1f", minFps, maxFps);

        // Show graph - convert deque to vector for plotting
        if (m_showGraph) {
          // Copy data from deque to vector (since ImGui needs contiguous data)
          std::vector<float> plotData(m_fpsHistory.begin(), m_fpsHistory.end());

          ImGui::PlotLines("##FPSHistory",
            plotData.data(),
            static_cast<int>(plotData.size()),
            0, nullptr, 0.0f, maxFps * 1.2f,
            ImVec2(250, 80));
        }

        // Options
        if (ImGui::CollapsingHeader("Settings")) {
          ImGui::Checkbox("Show Graph", &m_showGraph);
          ImGui::SliderFloat("BG Alpha", &m_bgAlpha, 0.1f, 1.0f);

          // Log button
          if (ImGui::Button("Log FPS")) {
            m_logger->LogInfo("Current FPS: " + std::to_string(m_fps));
          }
        }
      }

      // Right-click menu
      if (ImGui::BeginPopupContextWindow()) {
        ImGui::MenuItem("Expanded View", nullptr, &m_expanded);
        ImGui::MenuItem("Show Graph", nullptr, &m_showGraph);
        ImGui::SliderFloat("Background Alpha", &m_bgAlpha, 0.1f, 1.0f);
        ImGui::EndPopup();
      }
    }
    ImGui::End();
  }

  // Get the current FPS
  float GetFPS() const {
    return m_fps;
  }

  // Get the current frame time in milliseconds
  float GetFrameTimeMs() const {
    return m_frameTime * 1000.0f;
  }

  // Set position
  void SetPosition(const ImVec2& position) {
    m_position = position;
  }

  // Log current FPS if debug is enabled
  void LogFPSIfDebug(bool debugEnabled) {
    if (debugEnabled) {
      m_logger->LogInfo("FPS: " + std::to_string(m_fps));
    }
  }

  // ITogglableUI interface implementation
  bool IsVisible() const override {
    return m_visible;
  }

  void ToggleWindow() override {
    m_visible = !m_visible;
    m_logger->LogInfo(std::string("PerformanceOverlay ") +
      (m_visible ? "shown" : "hidden"));
  }

  const std::string& GetName() const override {
    return m_name;
  }

private:
  size_t m_historySize;              // Size of FPS history buffer
  std::deque<float> m_fpsHistory;    // Circular buffer of FPS values
  Uint64 m_lastFrameTime;            // Last frame time in SDL performance counter units
  float m_frameTime;                 // Time between frames in seconds
  float m_fps;                       // Current FPS
  int m_frameCounter;                // Frame counter since last FPS update
  float m_fpsTimer;                  // Time since last FPS update
  float m_fpsUpdateInterval;         // How often to update FPS calculation
  ImVec2 m_position;                 // Window position
  float m_bgAlpha;                   // Window background opacity
  bool m_showGraph;                  // Whether to show the graph
  bool m_expanded;                   // Whether to show expanded view
  bool m_visible;                    // Whether the overlay is visible
  std::string m_name;                // Component name for the toolbar
  Logger* m_logger;                  // Logger instance
};