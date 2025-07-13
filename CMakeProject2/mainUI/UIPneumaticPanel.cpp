// UIPneumaticPanel.cpp - Implementation of simple pneumatic panel UI
#include "UIPneumaticPanel.h"
#include "include/eziio/PneumaticManager.h"
#include "include/eziio/PneumaticSlide.h"
#include "imgui.h"
#include <iostream>

UIPneumaticPanel::UIPneumaticPanel(PneumaticManager& pneumaticManager)
  : m_pneumaticManager(pneumaticManager) {
  // Constructor
}

UIPneumaticPanel::~UIPneumaticPanel() {
  // Destructor - no cleanup needed
}

void UIPneumaticPanel::RenderUI() {
  if (!m_showWindow) {
    return;
  }

  ImGui::SetWindowFontScale(1.2f);
  ImGui::Text("Pneumatic Slide Control");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();

  // Get all slide names
  const auto slideNames = m_pneumaticManager.getSlideNames();

  if (slideNames.empty()) {
    RenderNoSlidesMessage();
    return;
  }

  // Global controls
  if (ImGui::Button("Reset All Slides", ImVec2(150, 30))) {
    m_pneumaticManager.resetAllSlides();
    std::cout << "Reset all pneumatic slides" << std::endl;
  }

  ImGui::SameLine();

  // Polling status
  bool isPolling = m_pneumaticManager.isPolling();
  if (isPolling) {
    if (ImGui::Button("Stop Updates", ImVec2(120, 30))) {
      m_pneumaticManager.stopPolling();
    }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "● Active");
  }
  else {
    if (ImGui::Button("Start Updates", ImVec2(120, 30))) {
      m_pneumaticManager.startPolling(50);
    }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "○ Stopped");
  }

  ImGui::Separator();
  ImGui::Spacing();

  // Create table for slides
  if (ImGui::BeginTable("PneumaticTable", 5,
    ImGuiTableFlags_Borders |
    ImGuiTableFlags_RowBg |
    ImGuiTableFlags_Resizable |
    ImGuiTableFlags_SizingFixedFit)) {

    // Table headers
    ImGui::TableSetupColumn("Slide Name", ImGuiTableColumnFlags_WidthFixed, 150);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 120);
    ImGui::TableSetupColumn("Extend", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("Retract", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    // Render each slide as a row
    for (const auto& slideName : slideNames) {
      RenderSlideRow(slideName);
    }

    ImGui::EndTable();
  }

  ImGui::Spacing();
  ImGui::Text("Total Slides: %zu", slideNames.size());
}

void UIPneumaticPanel::RenderSlideRow(const std::string& slideName) {
  auto slide = m_pneumaticManager.getSlide(slideName);
  if (!slide) return;

  SlideState state = slide->getState();

  // Update state timestamp tracking
  UpdateStateTimestamp(slideName, state);

  ImVec4 stateColor = GetStateColor(state);

  ImGui::TableNextRow();
  ImGui::PushID(slideName.c_str());

  // Column 1: Slide Name
  ImGui::TableNextColumn();
  ImGui::Text("%s", slideName.c_str());

  // Column 2: Status (with elapsed time for moving state)
  ImGui::TableNextColumn();
  ImGui::PushStyleColor(ImGuiCol_Text, stateColor);
  std::string statusText = GetStateStringWithTime(slideName, state);
  ImGui::Text("%s", statusText.c_str());
  ImGui::PopStyleColor();

  // Column 3: Extend Button
  ImGui::TableNextColumn();
  bool canExtend = (state == SlideState::RETRACTED || state == SlideState::UNKNOWN);

  if (!canExtend) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.4f, 0.8f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.5f, 0.9f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.6f, 1.0f, 1.0f));
  }

  if (ImGui::Button("Extend", ImVec2(70, 25)) && canExtend) {
    bool success = m_pneumaticManager.extendSlide(slideName);
    std::cout << "Extend " << slideName << ": " << (success ? "Success" : "Failed") << std::endl;
  }

  if (!canExtend) {
    ImGui::PopStyleVar();
  }
  else {
    ImGui::PopStyleColor(3);
  }

  // Tooltip for disabled button
  if (!canExtend && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("Cannot extend - slide is already extended or in error state");
  }

  // Column 4: Retract Button
  ImGui::TableNextColumn();
  bool canRetract = (state == SlideState::EXTENDED || state == SlideState::UNKNOWN);

  if (!canRetract) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 0.0f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.8f, 0.0f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.9f, 0.0f, 1.0f));
  }

  if (ImGui::Button("Retract", ImVec2(70, 25)) && canRetract) {
    bool success = m_pneumaticManager.retractSlide(slideName);
    std::cout << "Retract " << slideName << ": " << (success ? "Success" : "Failed") << std::endl;
  }

  if (!canRetract) {
    ImGui::PopStyleVar();
  }
  else {
    ImGui::PopStyleColor(3);
  }

  // Tooltip for disabled button
  if (!canRetract && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("Cannot retract - slide is already retracted or in error state");
  }

  // Column 5: Info/Status Text
  ImGui::TableNextColumn();

  // Display additional information based on slide state
  switch (state) {
  case SlideState::EXTENDED:
  {
    auto durationIt = m_lastMovementDuration.find(slideName);
    if (durationIt != m_lastMovementDuration.end()) {
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Extended in %.1fs", durationIt->second);
    }
    else {
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Ready to retract");
    }
  }
  break;
  case SlideState::RETRACTED:
  {
    auto durationIt = m_lastMovementDuration.find(slideName);
    if (durationIt != m_lastMovementDuration.end()) {
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Retracted in %.1fs", durationIt->second);
    }
    else {
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Ready to extend");
    }
  }
  break;
  case SlideState::MOVING:
  {
    float elapsed = GetElapsedTime(slideName);
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.0f, 1.0f), "Moving... %.1fs", elapsed);
  }
  break;
  case SlideState::P_ERROR:
    ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1.0f), "Check sensors/wiring");
    break;
  case SlideState::UNKNOWN:
  default:
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Status unknown");
    break;
  }

  ImGui::PopID();
}

const char* UIPneumaticPanel::GetStateString(SlideState state) const {
  switch (state) {
  case SlideState::EXTENDED:  return "Extended";
  case SlideState::RETRACTED: return "Retracted";
  case SlideState::MOVING:    return "Moving";
  case SlideState::P_ERROR:   return "ERROR";
  case SlideState::UNKNOWN:
  default:                    return "Unknown";
  }
}

ImVec4 UIPneumaticPanel::GetStateColor(SlideState state) const {
  switch (state) {
  case SlideState::EXTENDED:  return ImVec4(0.0f, 0.4f, 0.8f, 1.0f);   // Blue
  case SlideState::RETRACTED: return ImVec4(0.0f, 0.7f, 0.0f, 1.0f);   // Green
  case SlideState::MOVING:    return ImVec4(0.8f, 0.8f, 0.0f, 1.0f);   // Yellow
  case SlideState::P_ERROR:   return ImVec4(0.8f, 0.0f, 0.0f, 1.0f);   // Red
  case SlideState::UNKNOWN:
  default:                    return ImVec4(0.7f, 0.7f, 0.7f, 1.0f);   // Gray
  }
}

void UIPneumaticPanel::RenderNoSlidesMessage() {
  ImGui::Spacing();
  ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No pneumatic slides configured!");

  ImGui::Spacing();
  ImGui::Text("This typically means:");
  ImGui::BulletText("Pneumatic system is not initialized");
  ImGui::BulletText("No slides configured in pneumatic config");
  ImGui::BulletText("IO devices are not connected");
  ImGui::BulletText("Check pneumatic configuration file");
}

void UIPneumaticPanel::ToggleWindow() {
  m_showWindow = !m_showWindow;
}

std::string UIPneumaticPanel::GetStateStringWithTime(const std::string& slideName, SlideState state) const {
  std::string baseStatus = GetStateString(state);

  if (state == SlideState::MOVING) {
    float elapsedTime = GetElapsedTime(slideName);
    char timeBuffer[32];
    snprintf(timeBuffer, sizeof(timeBuffer), " (%.1fs)", elapsedTime);
    return baseStatus + timeBuffer;
  }

  return baseStatus;
}

void UIPneumaticPanel::UpdateStateTimestamp(const std::string& slideName, SlideState currentState) {
  auto lastStateIt = m_lastKnownState.find(slideName);

  // If this is the first time we see this slide, or state has changed
  if (lastStateIt == m_lastKnownState.end() || lastStateIt->second != currentState) {
    float currentTime = ImGui::GetTime();

    // If we were moving and now we're not, calculate the movement duration
    if (lastStateIt != m_lastKnownState.end() &&
      lastStateIt->second == SlideState::MOVING &&
      currentState != SlideState::MOVING) {

      auto timestampIt = m_stateChangeTimestamp.find(slideName);
      if (timestampIt != m_stateChangeTimestamp.end()) {
        float movementDuration = currentTime - timestampIt->second;
        m_lastMovementDuration[slideName] = movementDuration;
      }
    }

    m_stateChangeTimestamp[slideName] = currentTime;
    m_lastKnownState[slideName] = currentState;
  }
}

float UIPneumaticPanel::GetElapsedTime(const std::string& slideName) const {
  auto timestampIt = m_stateChangeTimestamp.find(slideName);
  if (timestampIt != m_stateChangeTimestamp.end()) {
    return ImGui::GetTime() - timestampIt->second;
  }
  return 0.0f;
}