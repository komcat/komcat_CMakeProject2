#include "PneumaticUI.h"
#include <iostream>

PneumaticUI::PneumaticUI(PneumaticManager& manager)
  : m_pneumaticManager(manager),
  m_showWindow(true),
  m_showDebugInfo(false)
{
  // Initialize color mapping for slide states
  m_stateColors[SlideState::UNKNOWN] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);    // Gray
  m_stateColors[SlideState::RETRACTED] = ImVec4(0.0f, 0.7f, 0.0f, 1.0f);  // Green
  m_stateColors[SlideState::EXTENDED] = ImVec4(0.0f, 0.4f, 0.8f, 1.0f);   // Blue
  m_stateColors[SlideState::MOVING] = ImVec4(0.8f, 0.8f, 0.0f, 1.0f);     // Yellow
  m_stateColors[SlideState::P_ERROR] = ImVec4(0.8f, 0.0f, 0.0f, 1.0f);      // Red

  // Register for state change notifications
  m_pneumaticManager.setStateChangeCallback([this](const std::string& slideName, SlideState state) {
    // Record timestamp for animation
    m_stateChangeTimestamp[slideName] = ImGui::GetTime();

    // Log the state change
    std::cout << "Pneumatic UI: Slide " << slideName << " changed to "
      << GetStateString(state) << std::endl;
  });

  std::cout << "PneumaticUI initialized" << std::endl;
}

PneumaticUI::~PneumaticUI()
{
}

void PneumaticUI::RenderUI()
{
  if (!m_showWindow) return;

  // Create the main window
  ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Pneumatic Controls", &m_showWindow, ImGuiWindowFlags_NoCollapse))
  {
    ImGui::End();
    return;
  }

  // Debug info toggle
  ImGui::Checkbox("Show Debug Info", &m_showDebugInfo);
  ImGui::SameLine();

  // Reset all button
  if (ImGui::Button("Reset All Slides")) {
    m_pneumaticManager.resetAllSlides();
  }

  ImGui::SameLine();

  // Start/stop polling
  bool isPolling = m_pneumaticManager.isPolling();
  if (isPolling) {
    if (ImGui::Button("Stop Status Updates")) {
      m_pneumaticManager.stopPolling();
    }
  }
  else {
    if (ImGui::Button("Start Status Updates")) {
      m_pneumaticManager.startPolling(50); // 50ms polling interval
    }
  }

  ImGui::Separator();

  // Get all slide names
  const auto slideNames = m_pneumaticManager.getSlideNames();

  if (slideNames.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No pneumatic slides configured!");
  }
  else {
    // Determine layout based on number of slides
    int columns = (std::min)(3, static_cast<int>(slideNames.size()));
    ImGui::Columns(columns, "pneumatic_columns", false);

    // Render each slide panel
    for (const auto& slideName : slideNames) {
      RenderSlidePanel(slideName);
      ImGui::NextColumn();
    }

    ImGui::Columns(1);
  }

  ImGui::End();
}

void PneumaticUI::RenderSlidePanel(const std::string& slideName)
{
  auto slide = m_pneumaticManager.getSlide(slideName);
  if (!slide) return;

  SlideState state = slide->getState();

  // Panel border color based on state
  ImGui::PushStyleColor(ImGuiCol_Border, GetStateColor(state));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);

  // Create a child window for this slide
  if (ImGui::BeginChild(slideName.c_str(), ImVec2(180, 250), true))
  {
    // Slide title
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::TextColored(GetStateColor(state), "%s", slideName.c_str());
    ImGui::PopFont();

    ImGui::Separator();

    // Current state
    ImGui::Text("State: ");
    ImGui::SameLine();
    ImGui::TextColored(GetStateColor(state), "%s", GetStateString(state));

    //// Visual representation of the slide
    //ImGui::Dummy(ImVec2(0.0f, 10.0f));

    //// Draw a visual representation of the pneumatic slide
    const float width = ImGui::GetContentRegionAvail().x;
    //const float slideHeight = 120.0f;
    //const ImVec2 pos = ImGui::GetCursorScreenPos();

    //ImDrawList* drawList = ImGui::GetWindowDrawList();

    //// Draw slide housing
    //drawList->AddRect(
    //  ImVec2(pos.x + 10, pos.y),
    //  ImVec2(pos.x + width - 10, pos.y + slideHeight),
    //  IM_COL32(100, 100, 100, 255),
    //  3.0f, 0, 2.0f
    //);

    //// Draw piston rod - position depends on state
    //float pistonPos = 0.5f; // Default position (middle)

    //if (state == SlideState::RETRACTED) {
    //  pistonPos = 0.1f; // Near the top
    //}
    //else if (state == SlideState::EXTENDED) {
    //  pistonPos = 0.9f; // Near the bottom
    //}
    //else if (state == SlideState::MOVING) {
    //  // Animate the piston movement based on time
    //  if (IsAnimating(slideName)) {
    //    float progress = GetAnimationProgress(slideName);

    //    // If we were retracted and now moving, animate going down
    //    auto oldState = slide->getState();
    //    if (oldState == SlideState::RETRACTED) {
    //      pistonPos = 0.1f + progress * 0.8f;
    //    }
    //    // If we were extended and now moving, animate going up
    //    else if (oldState == SlideState::EXTENDED) {
    //      pistonPos = 0.9f - progress * 0.8f;
    //    }
    //    // Default animation
    //    else {
    //      pistonPos = 0.5f + 0.3f * sinf(progress * 10.0f);
    //    }
    //  }
    //}

    //// Draw piston
    //float pistonY = pos.y + pistonPos * slideHeight;
    //ImVec4 pistonColor = GetStateColor(state);

    //// Rod
    //drawList->AddLine(
    //  ImVec2(pos.x + width / 2, pos.y + 5),
    //  ImVec2(pos.x + width / 2, pistonY),
    //  IM_COL32(200, 200, 200, 255),
    //  3.0f
    //);

    //// Piston head
    //drawList->AddCircleFilled(
    //  ImVec2(pos.x + width / 2, pistonY),
    //  10.0f,
    //  IM_COL32(
    //    static_cast<int>(pistonColor.x * 255),
    //    static_cast<int>(pistonColor.y * 255),
    //    static_cast<int>(pistonColor.z * 255),
    //    255
    //  )
    //);

    // Add space after the visual
    //ImGui::Dummy(ImVec2(0.0f, slideHeight + 10.0f));

    // Retract button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 0.0f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.8f, 0.0f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.9f, 0.0f, 1.0f));

    if (ImGui::Button(("Retract##" + slideName).c_str(), ImVec2(width - 20, 30))) {
      m_pneumaticManager.retractSlide(slideName);
    }
    ImGui::PopStyleColor(3);

    ImGui::PopStyleVar(); // Frame rounding

    ImGui::Spacing();
    // Control buttons
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

    // Extend button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.4f, 0.8f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.5f, 0.9f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.6f, 1.0f, 1.0f));

    if (ImGui::Button(("Extend##" + slideName).c_str(), ImVec2(width - 20, 30))) {
      m_pneumaticManager.extendSlide(slideName);
    }
    ImGui::PopStyleColor(3);
    

    // Debug info
    if (m_showDebugInfo) {
      ImGui::Separator();

      // Add sensor state info
      bool extendedSensor = m_pneumaticManager.readInputPin(slide->getExtendedInputConfig());
      bool retractedSensor = m_pneumaticManager.readInputPin(slide->getRetractedInputConfig());

      ImGui::Text("Extended Sensor: %s", extendedSensor ? "ON" : "OFF");
      ImGui::Text("Retracted Sensor: %s", retractedSensor ? "ON" : "OFF");

      // Add pin info
      const auto& outConfig = slide->getOutputConfig();
      const auto& extConfig = slide->getExtendedInputConfig();
      const auto& retConfig = slide->getRetractedInputConfig();

      ImGui::Text("Pin Details:");
      ImGui::Text("Out: %s.%d", outConfig.deviceName.c_str(), outConfig.pinNumber);
      ImGui::Text("Ext: %s.%d", extConfig.deviceName.c_str(), extConfig.pinNumber);
      ImGui::Text("Ret: %s.%d", retConfig.deviceName.c_str(), retConfig.pinNumber);
    }
  }
  ImGui::EndChild();

  ImGui::PopStyleVar();   // Frame border size
  ImGui::PopStyleColor(); // Border color
}

ImVec4 PneumaticUI::GetStateColor(SlideState state) const
{
  auto it = m_stateColors.find(state);
  if (it != m_stateColors.end()) {
    return it->second;
  }
  return ImVec4(0.5f, 0.5f, 0.5f, 1.0f); // Default: Gray
}

const char* PneumaticUI::GetStateString(SlideState state) const
{
  switch (state) {
  case SlideState::UNKNOWN:
    return "Unknown";
  case SlideState::RETRACTED:
    return "Retracted (Up)";
  case SlideState::EXTENDED:
    return "Extended (Down)";
  case SlideState::MOVING:
    return "Moving";
  case SlideState::P_ERROR:
    return "ERROR";
  default:
    return "Invalid";
  }
}

bool PneumaticUI::IsAnimating(const std::string& slideName) const
{
  auto it = m_stateChangeTimestamp.find(slideName);
  if (it == m_stateChangeTimestamp.end()) {
    return false;
  }

  // Animation lasts for 2 seconds
  float elapsed = ImGui::GetTime() - it->second;
  return elapsed < 2.0f;
}

float PneumaticUI::GetAnimationProgress(const std::string& slideName) const
{
  auto it = m_stateChangeTimestamp.find(slideName);
  if (it == m_stateChangeTimestamp.end()) {
    return 0.0f;
  }

  // Animation progress from 0.0 to 1.0
  float elapsed = ImGui::GetTime() - it->second;
  return (std::min)(elapsed / 2.0f, 1.0f);
}