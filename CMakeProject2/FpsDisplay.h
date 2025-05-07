// FpsDisplay.h
#pragma once

#include "imgui.h"
#include <string>
#include <chrono>
#include <deque>
#include <numeric>

class FpsDisplay {
public:
  FpsDisplay(size_t historySize = 60, float updateInterval = 0.5f)
    : m_historySize(historySize), m_updateInterval(updateInterval),
    m_fpsHistory(historySize, 0.0f), m_frameTime(0.0f), m_fps(0.0f),
    m_frameCounter(0), m_fpsTimer(0.0f), m_showDetails(false),
    m_position(ImVec2(10, 10)), m_bgAlpha(0.7f), m_showGraph(true),
    m_graphHeight(50), m_minFps(0.0f), m_maxFps(144.0f),
    m_windowTitle("Performance Metrics")
  {
    m_lastFrameTime = std::chrono::high_resolution_clock::now();
  }

  // Update FPS calculation - call this every frame
  void Update() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> delta = currentTime - m_lastFrameTime;
    m_frameTime = delta.count();
    m_lastFrameTime = currentTime;

    // Update frame counter
    m_frameCounter++;
    m_fpsTimer += m_frameTime;

    // Update FPS calculation every update interval
    if (m_fpsTimer >= m_updateInterval) {
      m_fps = m_frameCounter / m_fpsTimer;

      // Add to history
      m_fpsHistory.pop_front();
      m_fpsHistory.push_back(m_fps);

      // Reset counters
      m_frameCounter = 0;
      m_fpsTimer = 0.0f;

      // Auto-adjust max FPS for graph if needed
      if (m_fps > m_maxFps) {
        m_maxFps = m_fps * 1.2f; // Give some headroom
      }
    }
  }

  // Render the FPS display
  void Render(bool* pOpen = nullptr) {
    ImGui::SetNextWindowPos(m_position, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(m_bgAlpha);

    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize |
      ImGuiWindowFlags_NoSavedSettings;

    if (!m_showDetails) {
      flags |= ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoFocusOnAppearing;
    }

    if (ImGui::Begin(m_windowTitle.c_str(), pOpen, flags)) {
      // Basic FPS counter
      ImGui::Text("FPS: %.1f", m_fps);

      // Frame time
      ImGui::Text("Frame Time: %.2f ms", m_frameTime * 1000.0f);

      if (m_showDetails) {
        // Average FPS over history
        float avgFps = std::accumulate(m_fpsHistory.begin(), m_fpsHistory.end(), 0.0f) /
          static_cast<float>(m_fpsHistory.size());
        ImGui::Text("Avg FPS: %.1f", avgFps);

        // Min/Max FPS in history
        auto [minIt, maxIt] = std::minmax_element(m_fpsHistory.begin(), m_fpsHistory.end());
        ImGui::Text("Min/Max FPS: %.1f / %.1f", *minIt, *maxIt);

        // Show settings if in detailed mode
        if (ImGui::CollapsingHeader("Settings")) {
          ImGui::SliderFloat("BG Alpha", &m_bgAlpha, 0.1f, 1.0f);
          ImGui::SliderInt("Graph Height", &m_graphHeight, 30, 150);
          ImGui::SliderFloat("Min FPS", &m_minFps, 0.0f, 60.0f);
          ImGui::SliderFloat("Max FPS", &m_maxFps, 60.0f, 240.0f);
          ImGui::Checkbox("Show Graph", &m_showGraph);
        }

        ImGui::Separator();
        ImGui::Text("Right-click for options");

        // Context menu
        if (ImGui::BeginPopupContextWindow()) {
          ImGui::MenuItem("Show Details", NULL, &m_showDetails);
          ImGui::MenuItem("Show Graph", NULL, &m_showGraph);
          if (ImGui::MenuItem("Reset Position")) {
            m_position = ImVec2(10, 10);
          }
          ImGui::EndPopup();
        }
      }
      else {
        // Simple right-click menu
        if (ImGui::BeginPopupContextWindow()) {
          ImGui::MenuItem("Show Details", NULL, &m_showDetails);
          ImGui::EndPopup();
        }
      }

      // Draw FPS graph
      if (m_showGraph) {
        ImGui::PlotLines("", m_fpsHistory.data(), static_cast<int>(m_fpsHistory.size()),
          0, "FPS History", m_minFps, m_maxFps,
          ImVec2(ImGui::GetContentRegionAvail().x, static_cast<float>(m_graphHeight)));
      }
    }
    ImGui::End();
  }

  // Get the current FPS value
  float GetFps() const { return m_fps; }

  // Get the current frame time
  float GetFrameTime() const { return m_frameTime; }

  // Set window position
  void SetPosition(const ImVec2& pos) { m_position = pos; }

  // Set background alpha
  void SetBgAlpha(float alpha) { m_bgAlpha = alpha; }

  // Set whether to show details
  void SetShowDetails(bool show) { m_showDetails = show; }

  // Set window title
  void SetTitle(const std::string& title) { m_windowTitle = title; }

private:
  size_t m_historySize;                          // Size of FPS history buffer
  float m_updateInterval;                         // How often to update FPS calculation
  std::deque<float> m_fpsHistory;                // Circular buffer of FPS values
  std::chrono::time_point<std::chrono::high_resolution_clock> m_lastFrameTime;
  float m_frameTime;                             // Time between frames
  float m_fps;                                   // Current FPS
  int m_frameCounter;                            // Frames since last update
  float m_fpsTimer;                              // Time since last update
  bool m_showDetails;                            // Whether to show detailed stats
  ImVec2 m_position;                             // Window position
  float m_bgAlpha;                               // Background alpha
  bool m_showGraph;                              // Whether to show the graph
  int m_graphHeight;                             // Height of the FPS graph
  float m_minFps;                                // Minimum FPS for graph scale
  float m_maxFps;                                // Maximum FPS for graph scale
  std::string m_windowTitle;                     // Window title
};