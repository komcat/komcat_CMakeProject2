#include "VisionCameraExposureUI.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

//==============================================================================
// VisionExposureUISubscriber Implementation
//==============================================================================

VisionExposureUISubscriber::VisionExposureUISubscriber(const std::string& cameraId)
  : m_targetCameraId(cameraId) {
  m_subscriberId = "VisionExposureUI_" + cameraId;
}

VisionExposureUISubscriber::~VisionExposureUISubscriber() {
  CleanupTexture();
}

void VisionExposureUISubscriber::OnNewFrame(const CameraFrameData& frameData) {
  if (frameData.cameraId != m_targetCameraId) return;

  std::lock_guard<std::mutex> lock(m_frameMutex);
  m_latestFrame = frameData;
  m_hasNewFrame.store(true);
  m_frameCount.fetch_add(1);
}

void VisionExposureUISubscriber::OnCameraStatusChanged(const std::string& cameraId,
  bool connected, bool grabbing) {
  if (cameraId != m_targetCameraId) return;
  m_cameraConnected.store(connected);
  m_cameraGrabbing.store(grabbing);
}

bool VisionExposureUISubscriber::WantsFramesFromCamera(const std::string& cameraId) const {
  return cameraId == m_targetCameraId;
}

void VisionExposureUISubscriber::UpdateTextureIfNeeded() {
  if (!m_hasNewFrame.load()) return;

  CameraFrameData frameData;
  {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    frameData = m_latestFrame;
    m_hasNewFrame.store(false);
  }

  if (frameData.IsValid() && frameData.channels == 3) {
    CreateOrUpdateTexture(frameData);
  }
}

void VisionExposureUISubscriber::CreateOrUpdateTexture(const CameraFrameData& frameData) {
  if (!m_textureInitialized) {
    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    m_textureInitialized = true;
  }
  else {
    glBindTexture(GL_TEXTURE_2D, m_textureID);
  }

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frameData.width, frameData.height,
    0, GL_RGB, GL_UNSIGNED_BYTE, frameData.imageData.data());

  m_textureWidth = frameData.width;
  m_textureHeight = frameData.height;
  glBindTexture(GL_TEXTURE_2D, 0);
}

void VisionExposureUISubscriber::CleanupTexture() {
  if (m_textureInitialized) {
    glDeleteTextures(1, &m_textureID);
    m_textureInitialized = false;
  }
}

//==============================================================================
// VisionCameraExposureUI Implementation
//==============================================================================

VisionCameraExposureUI::VisionCameraExposureUI() {
  m_logger = Logger::GetInstance();
}

VisionCameraExposureUI::~VisionCameraExposureUI() {
  CleanupCameraFeed();
}

void VisionCameraExposureUI::SetExposureManager(VisionCameraExposureManager* manager) {
  m_exposureManager = manager;
  if (m_exposureManager && m_exposureManager->IsInitialized()) {
    m_logger->LogInfo("VisionCameraExposureUI: Exposure manager connected");
  }
}

void VisionCameraExposureUI::SetCameraManager(CameraManager* cameraManager) {
  m_cameraManager = cameraManager;
  if (m_cameraManager) {
    InitializeCameraFeed();
    m_logger->LogInfo("VisionCameraExposureUI: Camera manager connected");
  }
}

void VisionCameraExposureUI::SetMachineOperations(MachineOperations* machineOps) {
  m_machineOperations = machineOps;
  if (m_machineOperations) {
    m_logger->LogInfo("VisionCameraExposureUI: MachineOperations connected");
  }
}

void VisionCameraExposureUI::InitializeCameraFeed() {
  if (!m_cameraManager) return;

  CleanupCameraFeed();

  m_cameraSubscriber = std::make_shared<VisionExposureUISubscriber>(m_selectedCameraId);
  m_cameraManager->SubscribeToFrames(m_cameraSubscriber);
  m_cameraManager->StartBroadcastSystem();

  m_logger->LogInfo("VisionCameraExposureUI: Camera feed initialized");
}

void VisionCameraExposureUI::CleanupCameraFeed() {
  if (m_cameraSubscriber && m_cameraManager) {
    m_cameraManager->UnsubscribeFromFrames(m_cameraSubscriber->GetSubscriberId());
    m_cameraSubscriber.reset();
  }
}

void VisionCameraExposureUI::RenderUI() {
  if (!m_visible) return;

  ImGui::SetNextWindowSize(ImVec2(1400, 900), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Vision Camera Exposure Configuration", &m_visible)) {
    ImGui::End();
    return;
  }

  RenderTopControls();
  ImGui::Separator();

  // Three-panel layout
  ImVec2 contentSize = ImGui::GetContentRegionAvail();
  float leftWidth = 300.0f;
  float rightWidth = 350.0f;
  float centerWidth = contentSize.x - leftWidth - rightWidth - 20.0f;

  // Left Panel
  ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, contentSize.y), true);
  RenderLeftPanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // Center Panel
  ImGui::BeginChild("CenterPanel", ImVec2(centerWidth, contentSize.y), true);
  RenderCenterPanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // Right Panel
  ImGui::BeginChild("RightPanel", ImVec2(rightWidth, contentSize.y), true);
  RenderRightPanel();
  ImGui::EndChild();

  // Update test sequence if running
  if (m_testSequence.running) {
    UpdateTestSequence();
  }

  ImGui::End();
}

void VisionCameraExposureUI::RenderTopControls() {
  ImGui::Text("Graph:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(200);
  char graphBuffer[256];
  strcpy(graphBuffer, m_selectedGraph.c_str());
  if (ImGui::InputText("##Graph", graphBuffer, sizeof(graphBuffer))) {
    m_selectedGraph = graphBuffer;
  }

  ImGui::SameLine();
  ImGui::Text("Camera:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(150);
  char cameraBuffer[256];
  strcpy(cameraBuffer, m_selectedCameraId.c_str());
  if (ImGui::InputText("##Camera", cameraBuffer, sizeof(cameraBuffer))) {
    m_selectedCameraId = cameraBuffer;
  }

  // View mode tabs
  ImGui::SameLine(ImGui::GetWindowWidth() - 400);
  if (ImGui::Button("Node List", ImVec2(120, 0))) {
    m_currentView = ViewMode::NODE_LIST;
  }
  ImGui::SameLine();
  if (ImGui::Button("Test Sequence", ImVec2(120, 0))) {
    m_currentView = ViewMode::TEST_SEQUENCE;
  }
  ImGui::SameLine();
  if (ImGui::Button("Manual", ImVec2(120, 0))) {
    m_currentView = ViewMode::MANUAL_CONTROL;
  }
}

void VisionCameraExposureUI::RenderLeftPanel() {
  switch (m_currentView) {
  case ViewMode::NODE_LIST:
    RenderNodeListView();
    break;
  case ViewMode::TEST_SEQUENCE:
    RenderTestSequenceView();
    break;
  case ViewMode::MANUAL_CONTROL:
    RenderManualControlView();
    break;
  }
}

void VisionCameraExposureUI::RenderNodeListView() {
  ImGui::Text("Nodes in %s", m_selectedGraph.c_str());
  ImGui::Separator();

  if (!m_exposureManager || !m_exposureManager->IsInitialized()) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Exposure manager not ready");
    return;
  }

  auto configuredNodes = m_exposureManager->GetConfiguredNodes();
  auto graphNodes = GetNodesForGraph(m_selectedGraph);

  for (const auto& nodeId : graphNodes) {
    bool isConfigured = std::find(configuredNodes.begin(),
      configuredNodes.end(),
      nodeId) != configuredNodes.end();

    bool isSelected = (m_selectedNodeId == nodeId);

    if (isConfigured) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 1.0f, 0.5f, 1.0f));
    }

    if (ImGui::Selectable(nodeId.c_str(), isSelected)) {
      m_selectedNodeId = nodeId;
      LoadNodeSettings(nodeId);

      if (m_machineOperations) {
        m_machineOperations->MoveDeviceToNode("gantry-main",
          m_selectedGraph,
          nodeId,
          false,
          "VisionExposureUI");
      }
    }

    if (isConfigured) {
      ImGui::PopStyleColor();
      if (ImGui::IsItemHovered()) {
        VisionCameraExposureManager::NodeExposureSettings settings;
        if (m_exposureManager->GetNodeSettings(nodeId, settings)) {
          ImGui::SetTooltip("Exposure: %.1f us\nGain: %.1f",
            settings.exposure_time,
            settings.gain);
        }
      }
    }
  }
}

void VisionCameraExposureUI::RenderTestSequenceView() {
  ImGui::Text("Test Sequence Control");
  ImGui::Separator();

  if (!m_testSequence.running) {
    if (ImGui::Button("Start Sequence", ImVec2(-1, 30))) {
      StartTestSequence();
    }

    ImGui::Text("Dwell Time (s):");
    ImGui::SliderFloat("##Dwell", &m_testSequence.dwellTimeSeconds, 1.0f, 10.0f);

    ImGui::Checkbox("Auto Apply Settings", &m_testSequence.autoApplySettings);
  }
  else {
    if (ImGui::Button(m_testSequence.paused ? "Resume" : "Pause", ImVec2(100, 30))) {
      m_testSequence.paused = !m_testSequence.paused;
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop", ImVec2(100, 30))) {
      StopTestSequence();
    }

    float progress = m_testSequence.nodeList.empty() ? 0.0f :
      (float)m_testSequence.currentIndex / (float)m_testSequence.nodeList.size();
    ImGui::ProgressBar(progress);

    if (!m_testSequence.nodeList.empty() && m_testSequence.currentIndex < m_testSequence.nodeList.size()) {
      ImGui::Text("Current: %s (%zu/%zu)",
        m_testSequence.nodeList[m_testSequence.currentIndex].c_str(),
        m_testSequence.currentIndex + 1,
        m_testSequence.nodeList.size());
    }

    ImGui::Text("Status: %s", m_testSequence.status.c_str());
  }
}

void VisionCameraExposureUI::RenderManualControlView() {
  ImGui::Text("Manual Control");
  ImGui::Separator();

  ImGui::Text("Enter node ID to move to:");
  static char nodeIdBuffer[256] = "";
  ImGui::InputText("##NodeId", nodeIdBuffer, sizeof(nodeIdBuffer));

  if (ImGui::Button("Move to Node", ImVec2(-1, 30))) {
    if (m_machineOperations && strlen(nodeIdBuffer) > 0) {
      m_selectedNodeId = nodeIdBuffer;
      LoadNodeSettings(m_selectedNodeId);
      m_machineOperations->MoveDeviceToNode("gantry-main",
        m_selectedGraph,
        m_selectedNodeId,
        false,
        "VisionExposureUI");
    }
  }

  ImGui::Separator();
  ImGui::Text("Quick Actions:");

  if (ImGui::Button("Apply Default Settings", ImVec2(-1, 25))) {
    if (m_exposureManager && m_cameraManager) {
      auto* camera = m_cameraManager->GetCameraHardware(m_selectedCameraId);
      if (camera) {
        m_exposureManager->ApplyDefaultSettings(*camera);
      }
    }
  }

  if (ImGui::Button("Read Current Settings", ImVec2(-1, 25))) {
    if (m_exposureManager && m_cameraManager) {
      auto* camera = m_cameraManager->GetCameraHardware(m_selectedCameraId);
      if (camera) {
        m_exposureManager->ReadCurrentCameraSettings(*camera, m_currentSettings);
        m_editingSettings = m_currentSettings;
      }
    }
  }
}

void VisionCameraExposureUI::RenderCenterPanel() {
  ImGui::Text("Camera Preview");
  ImGui::Separator();

  if (m_cameraSubscriber) {
    m_cameraSubscriber->UpdateTextureIfNeeded();
  }

  RenderCameraPreview();
}

void VisionCameraExposureUI::RenderCameraPreview() {
  ImVec2 availSize = ImGui::GetContentRegionAvail();

  if (m_cameraSubscriber && m_cameraSubscriber->HasValidTexture()) {
    float aspectRatio = (float)m_cameraSubscriber->GetTextureWidth() /
      (float)m_cameraSubscriber->GetTextureHeight();

    float displayWidth = availSize.x;
    float displayHeight = displayWidth / aspectRatio;

    if (displayHeight > availSize.y) {
      displayHeight = availSize.y;
      displayWidth = displayHeight * aspectRatio;
    }

    float offsetX = (availSize.x - displayWidth) * 0.5f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

    ImGui::Image((ImTextureID)(intptr_t)m_cameraSubscriber->GetTextureID(),
      ImVec2(displayWidth, displayHeight));

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Resolution: %ux%u\nFrames: %llu",
        m_cameraSubscriber->GetTextureWidth(),
        m_cameraSubscriber->GetTextureHeight(),
        m_cameraSubscriber->GetFrameCount());
    }
  }
  else {
    ImGui::Text("No camera feed available");
    if (m_cameraManager && ImGui::Button("Start Camera")) {
      m_cameraManager->StartGrabbing(m_selectedCameraId);
    }
  }
}

void VisionCameraExposureUI::RenderRightPanel() {
  RenderSettingsEditor();
  ImGui::Separator();
  RenderQuickActions();
}

void VisionCameraExposureUI::RenderSettingsEditor() {
  ImGui::Text("Exposure Settings");
  ImGui::Separator();

  if (m_selectedNodeId.empty()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "Select a node");
    return;
  }

  ImGui::Text("Node: %s", m_selectedNodeId.c_str());

  if (ImGui::Checkbox("Auto Exposure", &m_editingSettings.auto_exposure)) {
    m_hasUnsavedChanges = true;
  }

  if (!m_editingSettings.auto_exposure) {
    float exposure = static_cast<float>(m_editingSettings.exposure_time);
    if (ImGui::SliderFloat("Exposure (us)", &exposure, 100, 100000, "%.0f",
      ImGuiSliderFlags_Logarithmic)) {
      m_editingSettings.exposure_time = exposure;
      m_hasUnsavedChanges = true;
    }
  }

  if (ImGui::Checkbox("Auto Gain", &m_editingSettings.auto_gain)) {
    m_hasUnsavedChanges = true;
  }

  if (!m_editingSettings.auto_gain) {
    float gain = static_cast<float>(m_editingSettings.gain);
    if (ImGui::SliderFloat("Gain", &gain, 0, 10, "%.1f")) {
      m_editingSettings.gain = gain;
      m_hasUnsavedChanges = true;
    }
  }

  if (m_hasUnsavedChanges) {
    if (ImGui::Button("Apply to Camera", ImVec2(-1, 30))) {
      ApplySettingsToCamera();
    }

    if (ImGui::Button("Save to Node", ImVec2(-1, 30))) {
      SaveNodeSettings();
    }

    if (ImGui::Button("Discard Changes", ImVec2(-1, 25))) {
      m_editingSettings = m_currentSettings;
      m_hasUnsavedChanges = false;
    }
  }
}

void VisionCameraExposureUI::RenderQuickActions() {
  ImGui::Text("Quick Actions");

  if (ImGui::Button("Test Current Settings", ImVec2(-1, 25))) {
    ApplySettingsToCamera();
  }

  if (ImGui::Button("Reload from Config", ImVec2(-1, 25))) {
    if (m_exposureManager) {
      m_exposureManager->ReloadConfiguration();
      LoadNodeSettings(m_selectedNodeId);
    }
  }
}

void VisionCameraExposureUI::LoadNodeSettings(const std::string& nodeId) {
  if (!m_exposureManager) return;

  if (!m_exposureManager->GetNodeSettings(nodeId, m_currentSettings)) {
    m_currentSettings = m_exposureManager->GetDefaultSettings();
    m_currentSettings.nodeId = nodeId;
  }

  m_editingSettings = m_currentSettings;
  m_hasUnsavedChanges = false;

  if (m_cameraManager) {
    auto* camera = m_cameraManager->GetCameraHardware(m_selectedCameraId);
    if (camera && camera->IsConnected()) {
      m_exposureManager->ApplySettingsToCamera(*camera, m_currentSettings);
    }
  }
}

void VisionCameraExposureUI::SaveNodeSettings() {
  if (!m_exposureManager || m_selectedNodeId.empty()) return;

  m_editingSettings.nodeId = m_selectedNodeId;
  m_editingSettings.description = "Settings for " + m_selectedNodeId;

  if (m_exposureManager->UpdateNodeSettings(m_selectedNodeId, m_editingSettings)) {
    m_currentSettings = m_editingSettings;
    m_hasUnsavedChanges = false;
    m_logger->LogInfo("Saved exposure settings for node: " + m_selectedNodeId);
  }
}

void VisionCameraExposureUI::ApplySettingsToCamera() {
  if (!m_cameraManager || !m_exposureManager) return;

  auto* camera = m_cameraManager->GetCameraHardware(m_selectedCameraId);
  if (camera && camera->IsConnected()) {
    if (m_exposureManager->ApplySettingsToCamera(*camera, m_editingSettings)) {
      m_logger->LogInfo("Applied exposure settings to camera");
    }
  }
}

void VisionCameraExposureUI::StartTestSequence() {
  m_testSequence.nodeList = GetNodesForGraph(m_selectedGraph);
  if (m_testSequence.nodeList.empty()) return;

  m_testSequence.running = true;
  m_testSequence.paused = false;
  m_testSequence.currentIndex = 0;
  m_testSequence.startTime = std::chrono::steady_clock::now();
  m_testSequence.status = "Starting sequence...";

  MoveToNextTestNode();
}

void VisionCameraExposureUI::StopTestSequence() {
  m_testSequence.running = false;
  m_testSequence.status = "Sequence stopped";
}

void VisionCameraExposureUI::UpdateTestSequence() {
  if (!m_testSequence.running || m_testSequence.paused) return;

  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
    now - m_testSequence.nodeStartTime).count();

  if (elapsed >= m_testSequence.dwellTimeSeconds) {
    m_testSequence.currentIndex++;
    if (m_testSequence.currentIndex >= m_testSequence.nodeList.size()) {
      StopTestSequence();
      m_testSequence.status = "Sequence complete";
    }
    else {
      MoveToNextTestNode();
    }
  }
}

void VisionCameraExposureUI::MoveToNextTestNode() {
  if (m_testSequence.currentIndex >= m_testSequence.nodeList.size()) return;

  const std::string& nodeId = m_testSequence.nodeList[m_testSequence.currentIndex];
  m_selectedNodeId = nodeId;

  if (m_testSequence.autoApplySettings) {
    LoadNodeSettings(nodeId);
  }

  if (m_machineOperations) {
    m_machineOperations->MoveDeviceToNode("gantry-main",
      m_selectedGraph,
      nodeId,
      false,
      "VisionExposureUI");
  }

  m_testSequence.nodeStartTime = std::chrono::steady_clock::now();
  m_testSequence.status = "At node: " + nodeId;
}

std::vector<std::string> VisionCameraExposureUI::GetNodesForGraph(const std::string& graphName) {
  std::vector<std::string> nodes;

  // This would normally query your graph system
  // For now, return example nodes or configured nodes
  if (m_exposureManager) {
    return m_exposureManager->GetConfiguredNodes();
  }

  return nodes;
}