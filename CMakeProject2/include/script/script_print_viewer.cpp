// script_print_viewer.cpp
#include "include/script/script_print_viewer.h"
#include "imgui.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

ScriptPrintViewer::ScriptPrintViewer()
  : m_isVisible(false), m_name("Script Print Output")
{
}

ScriptPrintViewer::~ScriptPrintViewer() {
}

void ScriptPrintViewer::AddPrintMessage(const std::string& message) {
  std::lock_guard<std::mutex> lock(m_mutex);

  PrintEntry entry;
  entry.message = message;
  entry.timestamp = std::chrono::system_clock::now();

  m_printHistory.push_back(entry);

  // Limit history size
  while (m_printHistory.size() > m_maxEntries) {
    m_printHistory.pop_front();
  }
}

void ScriptPrintViewer::Clear() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_printHistory.clear();
}

void ScriptPrintViewer::RenderUI() {
  if (!m_isVisible) {
    return;
  }

  ImGui::Begin("Script Print Output", &m_isVisible);

  // Controls
  if (ImGui::Button("Clear")) {
    Clear();
  }

  ImGui::SameLine();
  ImGui::Checkbox("Auto-scroll", &m_autoScroll);

  ImGui::SameLine();
  ImGui::Checkbox("Show Timestamps", &m_showTimestamps);

  // Filter
  ImGui::InputText("Filter", m_filterBuffer, sizeof(m_filterBuffer));

  ImGui::Separator();

  // Print output area
  ImGui::BeginChild("PrintOutput", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

  {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& entry : m_printHistory) {
      // Filter check
      if (strlen(m_filterBuffer) > 0) {
        if (entry.message.find(m_filterBuffer) == std::string::npos) {
          continue;
        }
      }

      // Format output
      if (m_showTimestamps) {
        // Convert timestamp to string
        auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);

        std::stringstream timestampStr;
        timestampStr << std::put_time(std::localtime(&time_t), "%H:%M:%S");

        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%s]", timestampStr.str().c_str());
        ImGui::SameLine();
      }

      ImGui::TextWrapped("%s", entry.message.c_str());
    }
  }

  // Auto-scroll to bottom
  if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
    ImGui::SetScrollHereY(1.0f);
  }

  ImGui::EndChild();

  ImGui::End();
}