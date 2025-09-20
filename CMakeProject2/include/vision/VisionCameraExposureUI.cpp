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

  // Get configured nodes from exposure manager
  auto configuredNodes = m_exposureManager->GetConfiguredNodes();

  // Get nodes from graph (for now using configured nodes as fallback)
  auto graphNodes = GetNodesForGraph(m_selectedGraph);

  // If no graph nodes available, use configured nodes
  if (graphNodes.empty()) {
    graphNodes = configuredNodes;
  }

  // Display node list with scroll region
  ImGui::BeginChild("NodeList", ImVec2(0, ImGui::GetContentRegionAvail().y - 150), true,
    ImGuiWindowFlags_HorizontalScrollbar);

  for (const auto& nodeId : graphNodes) {
    // Check if this node has configured settings
    bool isConfigured = std::find(configuredNodes.begin(),
      configuredNodes.end(),
      nodeId) != configuredNodes.end();

    bool isSelected = (m_selectedNodeId == nodeId);

    // Color configured nodes green
    if (isConfigured) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 1.0f, 0.5f, 1.0f));
    }

    // Selectable node item
    if (ImGui::Selectable(nodeId.c_str(), isSelected)) {
      m_selectedNodeId = nodeId;
      LoadNodeSettings(nodeId);

      // Move to node if MachineOperations is available
      if (m_machineOperations) {
        m_logger->LogInfo("Moving to node: " + nodeId);
        m_machineOperations->MoveDeviceToNode("gantry-main",
          m_selectedGraph,
          nodeId,
          false,
          "VisionExposureUI");
      }
    }

    if (isConfigured) {
      ImGui::PopStyleColor();

      // Show tooltip with current settings on hover
      if (ImGui::IsItemHovered()) {
        VisionCameraExposureManager::NodeExposureSettings settings;
        if (m_exposureManager->GetNodeSettings(nodeId, settings)) {
          ImGui::BeginTooltip();
          ImGui::Text("Node: %s", nodeId.c_str());
          ImGui::Separator();
          ImGui::Text("Exposure: %.1f us %s",
            settings.exposure_time,
            settings.auto_exposure ? "(Auto)" : "(Manual)");
          ImGui::Text("Gain: %.1f %s",
            settings.gain,
            settings.auto_gain ? "(Auto)" : "(Manual)");
          if (!settings.description.empty()) {
            ImGui::Separator();
            ImGui::Text("%s", settings.description.c_str());
          }
          ImGui::EndTooltip();
        }
      }
    }
    else {
      // Show tooltip for unconfigured nodes
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("No exposure settings configured\nClick to select and configure");
      }
    }
  }

  ImGui::EndChild();

  // Add new node section
  ImGui::Separator();
  ImGui::Text("Add New Node:");

  static char newNodeIdBuffer[256] = "";
  ImGui::SetNextItemWidth(-1);
  ImGui::InputText("##NewNodeId", newNodeIdBuffer, sizeof(newNodeIdBuffer));

  // Add node with default settings button
  if (ImGui::Button("Add with Defaults", ImVec2(-1, 25))) {
    if (strlen(newNodeIdBuffer) > 0) {
      AddNewNodeSettings(newNodeIdBuffer);
      memset(newNodeIdBuffer, 0, sizeof(newNodeIdBuffer));
    }
    else {
      ImGui::OpenPopup("Empty Node ID");
    }
  }

  // Copy from selected node button (only enabled if a node is selected)
  if (!m_selectedNodeId.empty()) {
    if (ImGui::Button("Copy from Selected", ImVec2(-1, 25))) {
      if (strlen(newNodeIdBuffer) > 0) {
        CopyNodeSettings(m_selectedNodeId, newNodeIdBuffer);
        memset(newNodeIdBuffer, 0, sizeof(newNodeIdBuffer));
      }
      else {
        ImGui::OpenPopup("Empty Node ID");
      }
    }

    // Show what node will be copied from
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
      "Will copy from: %s", m_selectedNodeId.c_str());
  }
  else {
    // Disabled button when no node is selected
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::Button("Copy from Selected", ImVec2(-1, 25));
    ImGui::PopStyleColor(2);
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
      "Select a node first to copy");
  }

  // Error popup for empty node ID
  if (ImGui::BeginPopupModal("Empty Node ID", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Please enter a node ID");
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  // Show statistics
  ImGui::Separator();
  ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
    "Total nodes: %zu | Configured: %zu",
    graphNodes.size(),
    configuredNodes.size());
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


// Update RenderSettingsEditor to include the delete button with confirmation
void VisionCameraExposureUI::RenderSettingsEditor() {
  ImGui::Text("Exposure Settings");
  ImGui::Separator();

  if (m_selectedNodeId.empty()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "Select a node");
    return;
  }

  ImGui::Text("Node: %s", m_selectedNodeId.c_str());

  // Existing exposure controls
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

  // Save/Apply buttons
  ImGui::Separator();
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

  // Delete node button with confirmation
  ImGui::Separator();
  ImGui::Text("Node Management:");

  // Static variable to track delete confirmation state
  static bool showDeleteConfirmation = false;
  static std::string nodeToDelete;

  // Delete button
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));

  if (ImGui::Button("Delete Node Settings", ImVec2(-1, 25))) {
    showDeleteConfirmation = true;
    nodeToDelete = m_selectedNodeId;
  }

  ImGui::PopStyleColor(3);

  // Delete confirmation popup
  if (showDeleteConfirmation) {
    ImGui::OpenPopup("Delete Confirmation");
  }

  // Center the popup
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal("Delete Confirmation", nullptr,
    ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Are you sure you want to delete settings for:");
    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
    ImGui::Text("%s", nodeToDelete.c_str());
    ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::Text("This action cannot be undone!");

    ImGui::Spacing();

    // Confirmation buttons
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
      DeleteNodeSettings(nodeToDelete);
      showDeleteConfirmation = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      showDeleteConfirmation = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
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

// Add new node with default settings
void VisionCameraExposureUI::AddNewNodeSettings(const std::string& nodeId) {
  if (!m_exposureManager || nodeId.empty()) {
    m_logger->LogWarning("Cannot add node settings: invalid parameters");
    return;
  }

  // Check if node already exists
  VisionCameraExposureManager::NodeExposureSettings existingSettings;
  if (m_exposureManager->GetNodeSettings(nodeId, existingSettings)) {
    m_logger->LogWarning("Node settings already exist for: " + nodeId);
    // Could show a popup here asking if user wants to overwrite
    return;
  }

  // Create new settings based on defaults
  VisionCameraExposureManager::NodeExposureSettings newSettings;
  newSettings = m_exposureManager->GetDefaultSettings();
  newSettings.nodeId = nodeId;
  newSettings.description = "Settings for " + nodeId + " (created " + GetCurrentTimestamp() + ")";

  // You might want to copy current camera settings if camera is connected
  if (m_cameraManager) {
    auto* camera = m_cameraManager->GetCameraHardware(m_selectedCameraId);
    if (camera && camera->IsConnected()) {
      // Option to use current camera settings instead of defaults
      VisionCameraExposureManager::NodeExposureSettings currentCameraSettings;
      if (m_exposureManager->ReadCurrentCameraSettings(*camera, currentCameraSettings)) {
        newSettings.exposure_time = currentCameraSettings.exposure_time;
        newSettings.gain = currentCameraSettings.gain;
        newSettings.auto_exposure = currentCameraSettings.auto_exposure;
        newSettings.auto_gain = currentCameraSettings.auto_gain;
        newSettings.description = "Settings for " + nodeId + " (from current camera)";
      }
    }
  }

  // Save to manager
  if (m_exposureManager->UpdateNodeSettings(nodeId, newSettings)) {
    // Auto-save configuration to file
    if (SaveConfiguration()) {
      m_logger->LogInfo("Successfully added new node settings: " + nodeId);

      // Select the new node
      m_selectedNodeId = nodeId;
      LoadNodeSettings(nodeId);

      // Optionally move to the new node if it exists in the graph
      if (m_machineOperations) {
        m_machineOperations->MoveDeviceToNode("gantry-main",
          m_selectedGraph,
          nodeId,
          false,
          "VisionExposureUI");
      }
    }
    else {
      m_logger->LogError("Failed to save configuration after adding node: " + nodeId);
    }
  }
  else {
    m_logger->LogError("Failed to add node settings: " + nodeId);
  }
}

// Copy settings from source node to target node
void VisionCameraExposureUI::CopyNodeSettings(const std::string& sourceNodeId,
  const std::string& targetNodeId) {
  if (!m_exposureManager || sourceNodeId.empty() || targetNodeId.empty()) {
    m_logger->LogWarning("Cannot copy node settings: invalid parameters");
    return;
  }

  if (sourceNodeId == targetNodeId) {
    m_logger->LogWarning("Cannot copy node to itself");
    return;
  }

  // Get source settings
  VisionCameraExposureManager::NodeExposureSettings sourceSettings;
  if (!m_exposureManager->GetNodeSettings(sourceNodeId, sourceSettings)) {
    m_logger->LogError("Source node settings not found: " + sourceNodeId);
    return;
  }

  // Check if target already exists
  VisionCameraExposureManager::NodeExposureSettings existingSettings;
  bool targetExists = m_exposureManager->GetNodeSettings(targetNodeId, existingSettings);

  if (targetExists) {
    // Could show a confirmation dialog here
    m_logger->LogWarning("Overwriting existing settings for: " + targetNodeId);
  }

  // Copy settings but update the node ID and description
  VisionCameraExposureManager::NodeExposureSettings targetSettings = sourceSettings;
  targetSettings.nodeId = targetNodeId;
  targetSettings.description = "Copied from " + sourceNodeId + " (" + GetCurrentTimestamp() + ")";

  // Save to manager
  if (m_exposureManager->UpdateNodeSettings(targetNodeId, targetSettings)) {
    // Auto-save configuration to file
    if (SaveConfiguration()) {
      m_logger->LogInfo("Successfully copied settings from " + sourceNodeId + " to " + targetNodeId);

      // Select the new node
      m_selectedNodeId = targetNodeId;
      LoadNodeSettings(targetNodeId);

      // Optionally move to the new node
      if (m_machineOperations) {
        m_machineOperations->MoveDeviceToNode("gantry-main",
          m_selectedGraph,
          targetNodeId,
          false,
          "VisionExposureUI");
      }
    }
    else {
      m_logger->LogError("Failed to save configuration after copying node");
    }
  }
  else {
    m_logger->LogError("Failed to copy node settings");
  }
}

// Save configuration to file
bool VisionCameraExposureUI::SaveConfiguration() {
  if (!m_exposureManager) {
    m_logger->LogError("Cannot save configuration: exposure manager not available");
    return false;
  }

  // Save to JSON file
  if (m_exposureManager->SaveConfiguration()) {
    m_logger->LogInfo("Configuration saved to camera_exposure_config.json");

    // Show visual feedback in UI (optional)
    m_lastSaveTime = std::chrono::steady_clock::now();
    m_showSaveSuccess = true;

    return true;
  }
  else {
    m_logger->LogError("Failed to save configuration to file");
    m_lastError = m_exposureManager->GetLastError();
    return false;
  }
}

// Helper function to get current timestamp
std::string VisionCameraExposureUI::GetCurrentTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;

  std::tm timeinfo;
#ifdef _WIN32
  localtime_s(&timeinfo, &time);
#else
  timeinfo = *localtime(&time);
#endif

  ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

// Update the SaveNodeSettings function to use the common save method
void VisionCameraExposureUI::SaveNodeSettings() {
  if (!m_exposureManager || m_selectedNodeId.empty()) {
    m_logger->LogWarning("Cannot save node settings: no node selected");
    return;
  }

  m_editingSettings.nodeId = m_selectedNodeId;
  m_editingSettings.description = "Updated " + GetCurrentTimestamp();

  if (m_exposureManager->UpdateNodeSettings(m_selectedNodeId, m_editingSettings)) {
    m_currentSettings = m_editingSettings;
    m_hasUnsavedChanges = false;

    // Auto-save to JSON file
    if (SaveConfiguration()) {
      m_logger->LogInfo("Saved and persisted exposure settings for node: " + m_selectedNodeId);
    }
    else {
      m_logger->LogWarning("Settings saved in memory but failed to persist to file");
    }
  }
  else {
    m_logger->LogError("Failed to save node settings");
  }
}

// Optional: Show save status in UI
void VisionCameraExposureUI::RenderSaveStatus() {
  if (m_showSaveSuccess) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastSaveTime).count();

    if (elapsed < 3) {  // Show for 3 seconds
      ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ Configuration saved");
    }
    else {
      m_showSaveSuccess = false;
    }
  }

  if (!m_lastError.empty()) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", m_lastError.c_str());
  }
}

// Add this method to VisionCameraExposureUI.cpp
void VisionCameraExposureUI::DeleteNodeSettings(const std::string& nodeId) {
  if (!m_exposureManager || nodeId.empty()) {
    m_logger->LogWarning("Cannot delete node settings: invalid parameters");
    return;
  }

  // Check if node exists
  VisionCameraExposureManager::NodeExposureSettings settings;
  if (!m_exposureManager->GetNodeSettings(nodeId, settings)) {
    m_logger->LogWarning("Node settings not found: " + nodeId);
    return;
  }

  // Remove from manager
  if (m_exposureManager->RemoveNodeSettings(nodeId)) {
    // Auto-save configuration to file
    if (SaveConfiguration()) {
      m_logger->LogInfo("Successfully deleted node settings: " + nodeId);

      // Clear selection if we deleted the selected node
      if (m_selectedNodeId == nodeId) {
        m_selectedNodeId.clear();
        m_currentSettings = VisionCameraExposureManager::NodeExposureSettings();
        m_editingSettings = m_currentSettings;
        m_hasUnsavedChanges = false;
      }
    }
    else {
      m_logger->LogError("Failed to save configuration after deleting node: " + nodeId);
    }
  }
  else {
    m_logger->LogError("Failed to delete node settings: " + nodeId);
  }
}
