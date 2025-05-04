#include "InitializationWindow.h"
#include "imgui.h"
#include <iostream>

InitializationWindow::InitializationWindow(MachineOperations& machineOps)
  : m_machineOps(machineOps)
{
  // Create the initialization sequence step
  m_initStep = std::make_unique<SequenceStep>("SystemInitialization", machineOps);

  // Add the required operations to the sequence
  // 1. Move gantry-main to safe position
  m_initStep->AddOperation(std::make_shared<MoveToNodeOperation>(
    "gantry-main", "Process_Flow", "node_4027"));

  // 2. Move hex-left to home position
  m_initStep->AddOperation(std::make_shared<MoveToNodeOperation>(
    "hex-left", "Process_Flow", "node_5480"));

  // 3. Move hex-right to home position
  m_initStep->AddOperation(std::make_shared<MoveToNodeOperation>(
    "hex-right", "Process_Flow", "node_5136"));

  // 4. Clear output L_Gripper (pin 0)
  m_initStep->AddOperation(std::make_shared<SetOutputOperation>(
    "IOBottom", 0, false));

  // 5. Clear output R_Gripper (pin 2)
  m_initStep->AddOperation(std::make_shared<SetOutputOperation>(
    "IOBottom", 2, false));

  // 6. Set output Vacuum_Base (pin 10)
  m_initStep->AddOperation(std::make_shared<SetOutputOperation>(
    "IOBottom", 10, true));

  // Set up the completion callback
  m_initStep->SetCompletionCallback([this](bool success) {
    m_isInitializing = false;
    if (success) {
      m_statusMessage = "Initialization completed successfully";
    }
    else {
      m_statusMessage = "Initialization failed";
    }
  });
}

void InitializationWindow::RenderUI() {
  // Skip if window is not visible
  if (!m_showWindow) return;

  // Create ImGui window
  ImGui::Begin("System Initialization", &m_showWindow);

  // Title with larger font
  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("System Initialization");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Separator();

  // Status display
  ImGui::Text("Status: %s", m_statusMessage.c_str());

  ImGui::Separator();

  // Enable/disable button based on current state
  if (m_isInitializing) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f)); // Gray button
    ImGui::Button("Initializing...", ImVec2(-1, 50));
    ImGui::PopStyleColor();
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 0.2f, 1.0f)); // Green button
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.8f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.6f, 0.1f, 1.0f));

    if (ImGui::Button("Initialize System", ImVec2(-1, 50))) {
      RunInitializationProcess();
    }

    ImGui::PopStyleColor(3);
  }

  // Description of what initialization does
  ImGui::Separator();
  ImGui::TextWrapped("This will perform the following operations:");
  ImGui::BulletText("Move gantry-main to safe position");
  ImGui::BulletText("Move hex-left to home position");
  ImGui::BulletText("Move hex-right to home position");
  ImGui::BulletText("Release left gripper");
  ImGui::BulletText("Release right gripper");
  ImGui::BulletText("Activate base vacuum");

  ImGui::End();
}

void InitializationWindow::RunInitializationProcess() {
  if (m_isInitializing) {
    return; // Already initializing
  }

  m_isInitializing = true;
  m_statusMessage = "Initializing...";

  // Start the initialization step in a new thread
  std::thread([this]() {
    m_initStep->Execute();
  }).detach();
}