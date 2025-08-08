// UIVisionPanel_Main.cpp - Constructor, destructor, and main UI logic
#include "UIVisionPanel.h"
#include "include/halcon/VisionCircleDetection.h"
#include "include/camera/CameraManager.h"
#include "include/motions/MotionConfigManager.h"
#include "include/machine_operations.h"
#include <iostream>

UIVisionPanel::UIVisionPanel() {
  std::cout << "[UIVisionPanel] Initializing Vision Panel with Circle Detection, Node Navigation, and Preset Management" << std::endl;

  // Initialize UI state
  m_showNodeList = false;

  // Initialize auto-execution settings
  m_autoExecute = false;
  m_autoExecuteInterval = 200.0f;
  m_lastAutoExecuteTime = 0.0f;

  // Initialize systems
  InitializeCircleDetection();
  InitializePresetManager();
  // Initialize node-preset system
  CreateNodePresetTable();
  LoadNodePresetMappings();
}

UIVisionPanel::~UIVisionPanel() {
  CleanupImageTexture();
  CleanupInvertedTexture();
  std::cout << "[UIVisionPanel] Vision Panel destroyed" << std::endl;
}

void UIVisionPanel::SetCameraManager(CameraManager* cameraManager) {
  m_cameraManager = cameraManager;

  if (m_cameraManager) {
    std::cout << "[UIVisionPanel] Camera Manager connected" << std::endl;

    auto cameras = GetAvailableCameras();
    if (!cameras.empty()) {
      m_selectedCameraId = cameras[0];
      std::cout << "[UIVisionPanel] Auto-selected camera: " << m_selectedCameraId << std::endl;
    }
  }
}

void UIVisionPanel::SetMotionConfigManager(MotionConfigManager* configManager) {
  m_configManager = configManager;
  if (m_configManager) {
    std::cout << "[UIVisionPanel] Motion Config Manager connected - loading nodes..." << std::endl;
    LoadNodesFromConfig();
    std::cout << "[UIVisionPanel] Loaded " << m_nodes.size() << " nodes from config" << std::endl;
  }
  else {
    std::cout << "[UIVisionPanel] Motion Config Manager is NULL!" << std::endl;
  }
}

void UIVisionPanel::SetMachineOperations(MachineOperations* machineOps) {
  m_machineOperations = machineOps;
  if (m_machineOperations) {
    std::cout << "[UIVisionPanel] Machine Operations connected" << std::endl;
  }
  else {
    std::cout << "[UIVisionPanel] Machine Operations is NULL!" << std::endl;
  }
}

void UIVisionPanel::RenderUI() {
  if (!m_showWindow) return;

  // Update auto-execution timer
  UpdateAutoExecution();

  ImGui::SetNextWindowSize(ImVec2(1400, 800), ImGuiCond_FirstUseEver);

  if (!ImGui::Begin("Vision Processing & Navigation with Presets", &m_showWindow)) {
    ImGui::End();
    return;
  }

  // Debug info at top
  if (m_showNodeList) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Node List: ENABLED (%zu nodes)", m_nodes.size());
  }
  else {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Node List: DISABLED");
  }
  ImGui::SameLine();
  ImGui::Text("| Presets: %s (%zu) | Config: %s | MachineOps: %s",
    m_presetManager && m_presetManager->IsInitialized() ? "OK" : "NULL", m_availablePresets.size(),
    m_configManager ? "OK" : "NULL",
    m_machineOperations ? "OK" : "NULL");

  ImGui::Separator();

  // Calculate layout based on whether node list is shown
  ImVec2 contentSize = ImGui::GetContentRegionAvail();

  if (m_showNodeList) {
    // Four-panel layout: Controls | Image | Results | Nodes
    float leftWidth = contentSize.x * 0.25f;
    float middleWidth = contentSize.x * 0.35f;
    float rightWidth = contentSize.x * 0.20f;
    float nodeWidth = contentSize.x * 0.20f;

    // Left Panel - Controls
    ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, contentSize.y), true);
    RenderLeftPanel();
    ImGui::EndChild();

    ImGui::SameLine();

    // Middle Panel - Image Display
    ImGui::BeginChild("ImagePanel", ImVec2(middleWidth, contentSize.y), true);
    RenderImageDisplay();
    ImGui::EndChild();

    ImGui::SameLine();

    // Right Panel - Results
    ImGui::BeginChild("RightPanel", ImVec2(rightWidth, contentSize.y), true);
    RenderRightPanel();
    ImGui::EndChild();

    ImGui::SameLine();

    // Node Panel - Navigation
    ImGui::BeginChild("NodePanel", ImVec2(nodeWidth, contentSize.y), true);
    RenderNodeListPanel();
    ImGui::EndChild();
  }
  else {
    // Original three-panel layout
    float leftWidth = contentSize.x * 0.3f;
    float middleWidth = contentSize.x * 0.45f;
    float rightWidth = contentSize.x * 0.25f;

    // Left Panel - Controls
    ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, contentSize.y), true);
    RenderLeftPanel();
    ImGui::EndChild();

    ImGui::SameLine();

    // Middle Panel - Image Display
    ImGui::BeginChild("ImagePanel", ImVec2(middleWidth, contentSize.y), true);
    RenderImageDisplay();
    ImGui::EndChild();

    ImGui::SameLine();

    // Right Panel - Results
    ImGui::BeginChild("RightPanel", ImVec2(rightWidth, contentSize.y), true);
    RenderRightPanel();
    ImGui::EndChild();
  }

  ImGui::End();

  RenderInvertPreviewDialog();

}


// UPDATE: Modify RenderLeftPanel() to include node-preset controls
void UIVisionPanel::RenderLeftPanel() {
  ImGui::Text("Vision & Navigation");
  ImGui::Separator();

  // Toggle node list panel
  if (ImGui::Checkbox("Show Node List", &m_showNodeList)) {
    std::cout << "[UIVisionPanel] Node list toggled: " << (m_showNodeList ? "ON" : "OFF") << std::endl;
  }

  ImGui::Spacing();

  // === NEW: Node-Preset Controls ===
  RenderNodePresetControls();

  ImGui::Spacing();
  ImGui::Separator();

  // Camera selection
  RenderCameraSelection();

  ImGui::Spacing();
  ImGui::Separator();

  // Detection controls
  RenderCircleDetectionControls();

  ImGui::Spacing();
  ImGui::Separator();

  // Preset controls
  RenderPresetControls();

  ImGui::Spacing();
  ImGui::Separator();

  // Parameter controls
  RenderCircleParameterControls();

  ImGui::Spacing();
  ImGui::Separator();

  // Auto-execution controls
  RenderAutoExecutionControls();

  // Node-Preset Dialog
  RenderNodePresetDialog();
}

// NEW: Node-Preset Controls UI

void UIVisionPanel::RenderRightPanel() {
  ImGui::Text("Detection Results");
  ImGui::Separator();

  RenderCircleDetectionResults();
}