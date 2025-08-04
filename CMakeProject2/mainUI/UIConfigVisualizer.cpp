// UIConfigVisualizer.cpp - Updated Implementation with Broadcasting Support
#include "include/motions/MotionTypes.h"
#include "include/motions/MotionConfigManager.h"
#include "UIConfigVisualizer.h"
#include "NodePropertiesHandler.h"
#include "imgui.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include "include/camera/CameraManager.h"

//==============================================================================
// UIConfigVisualizerSubscriber Implementation
//==============================================================================

UIConfigVisualizerSubscriber::UIConfigVisualizerSubscriber(const std::string& cameraId)
  : m_targetCameraId(cameraId) {
  UpdateSubscriberId();
  ResetState();

  std::cout << "[INFO] UIConfigVisualizerSubscriber created for camera: " << cameraId
    << " (ID: " << m_subscriberId << ")" << std::endl;
}

UIConfigVisualizerSubscriber::~UIConfigVisualizerSubscriber() {
  CleanupTexture();
  std::cout << "[INFO] UIConfigVisualizerSubscriber destroyed for camera: " << m_targetCameraId
    << " (Total frames: " << m_totalFramesReceived.load() << ")" << std::endl;
}

void UIConfigVisualizerSubscriber::OnNewFrame(const CameraFrameData& frameData) {
  if (frameData.cameraId != m_targetCameraId) {
    return; // Not interested in this camera
  }

  // Store frame data for texture update
  {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    m_latestFrame = frameData; // Deep copy
  }

  // Update atomic flags
  m_hasNewFrame.store(true);
  m_totalFramesReceived.fetch_add(1);
  m_lastFrameTimestamp.store(frameData.timestamp);

  // Mark texture for update (will be processed on main thread)
  if (frameData.IsValid() && frameData.channels == 3) {
    std::lock_guard<std::mutex> lock(m_textureMutex);
    m_pendingTextureFrame = frameData;
    m_needsTextureUpdate.store(true);
  }

  // Optional: Debug logging (enable only when needed)
#ifdef DEBUG_UI_CONFIG_FRAME_RECEPTION
  static uint64_t debugFrameCount = 0;
  debugFrameCount++;
  if ((debugFrameCount % 30) == 1) {
    std::cout << "[DEBUG] UIConfigVisualizer frame #" << debugFrameCount
      << " from camera: " << frameData.cameraId << " ("
      << frameData.width << "x" << frameData.height << ")" << std::endl;
  }
#endif
}

void UIConfigVisualizerSubscriber::OnCameraStatusChanged(const std::string& cameraId, bool connected, bool grabbing) {
  if (cameraId != m_targetCameraId) {
    return; // Not our camera
  }

  bool wasConnected = m_cameraConnected.exchange(connected);
  bool wasGrabbing = m_cameraGrabbing.exchange(grabbing);

  if (wasConnected != connected || wasGrabbing != grabbing) {
    std::cout << "[INFO] UIConfigVisualizer camera " << cameraId << " status changed: "
      << "connected=" << (connected ? "Yes" : "No")
      << ", grabbing=" << (grabbing ? "Yes" : "No") << std::endl;
  }

  // Reset frame flag if camera stopped grabbing
  if (wasGrabbing && !grabbing) {
    m_hasNewFrame.store(false);
  }
}

std::string UIConfigVisualizerSubscriber::GetSubscriberId() const {
  return m_subscriberId;
}

bool UIConfigVisualizerSubscriber::WantsFramesFromCamera(const std::string& cameraId) const {
  return cameraId == m_targetCameraId;
}

void UIConfigVisualizerSubscriber::SetTargetCamera(const std::string& cameraId) {
  if (m_targetCameraId == cameraId) {
    return; // No change needed
  }

  std::cout << "[INFO] UIConfigVisualizerSubscriber switching from '"
    << m_targetCameraId << "' to '" << cameraId << "'" << std::endl;

  m_targetCameraId = cameraId;
  UpdateSubscriberId();
  ResetState();
}

CameraFrameData UIConfigVisualizerSubscriber::GetLatestFrame() const {
  std::lock_guard<std::mutex> lock(m_frameMutex);
  return m_latestFrame; // Return copy
}

void UIConfigVisualizerSubscriber::UpdateTextureIfNeeded() {
  if (!m_needsTextureUpdate.load()) {
    return;
  }

  CameraFrameData frameData;
  {
    std::lock_guard<std::mutex> lock(m_textureMutex);
    frameData = m_pendingTextureFrame;
    m_needsTextureUpdate.store(false);
  }

  if (frameData.IsValid() && frameData.channels == 3) {
    CreateOrUpdateTexture(frameData);
  }
}

void UIConfigVisualizerSubscriber::CreateOrUpdateTexture(const CameraFrameData& frameData) {
  if (!frameData.IsValid() || frameData.imageData.empty()) {
    return;
  }

  // Create texture if not initialized
  if (!m_textureInitialized) {
    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    m_textureInitialized = true;
    m_textureWidth = frameData.width;
    m_textureHeight = frameData.height;

    // Upload initial texture data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frameData.width, frameData.height,
      0, GL_RGB, GL_UNSIGNED_BYTE, frameData.imageData.data());

    std::cout << "[INFO] Created OpenGL texture " << m_textureID
      << " for UIConfigVisualizer (" << frameData.width << "x" << frameData.height << ")" << std::endl;
  }
  else {
    glBindTexture(GL_TEXTURE_2D, m_textureID);

    // Check if we need to resize texture
    if (frameData.width != m_textureWidth || frameData.height != m_textureHeight) {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frameData.width, frameData.height,
        0, GL_RGB, GL_UNSIGNED_BYTE, frameData.imageData.data());
      m_textureWidth = frameData.width;
      m_textureHeight = frameData.height;

      std::cout << "[INFO] Resized UIConfigVisualizer texture to "
        << frameData.width << "x" << frameData.height << std::endl;
    }
    else {
      // Update existing texture (more efficient)
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frameData.width, frameData.height,
        GL_RGB, GL_UNSIGNED_BYTE, frameData.imageData.data());
    }
  }

  glBindTexture(GL_TEXTURE_2D, 0);
}

void UIConfigVisualizerSubscriber::CleanupTexture() {
  if (m_textureInitialized) {
    glDeleteTextures(1, &m_textureID);
    m_textureInitialized = false;
    m_textureID = 0;
    m_textureWidth = 0;
    m_textureHeight = 0;

    std::cout << "[INFO] Cleaned up OpenGL texture for UIConfigVisualizer" << std::endl;
  }
}

void UIConfigVisualizerSubscriber::ResetState() {
  // Clear frame data
  {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    m_latestFrame = CameraFrameData();
  }

  // Reset atomic flags
  m_hasNewFrame.store(false);
  m_cameraConnected.store(false);
  m_cameraGrabbing.store(false);
  m_totalFramesReceived.store(0);
  m_lastFrameTimestamp.store(0);
  m_needsTextureUpdate.store(false);
}

void UIConfigVisualizerSubscriber::UpdateSubscriberId() {
  m_subscriberId = "UIConfigVisualizer_" + m_targetCameraId;
}

//==============================================================================
// UIConfigVisualizer Implementation
//==============================================================================

UIConfigVisualizer::UIConfigVisualizer(MotionConfigManager& configManager, CameraManager* cameraManager)
  : configManager(configManager), m_cameraManager(cameraManager), m_machineOperations(nullptr)
{
  m_logger = Logger::GetInstance();

  // Initialize action handlers
  m_propertiesHandler = std::make_unique<NodePropertiesHandler>(configManager, m_logger);

  // Initialize camera system if available
  if (m_cameraManager) {
    auto cameraIds = m_cameraManager->GetCameraIds();
    if (!cameraIds.empty()) {
      m_selectedCameraId = cameraIds[0];
      InitializeCameraFeed();
    }
    m_logger->LogInfo("UIConfigVisualizer initialized with camera broadcasting support");
  }
  else {
    m_logger->LogInfo("UIConfigVisualizer initialized without camera support");
  }
}

UIConfigVisualizer::~UIConfigVisualizer() {
  ClearCameraFeed();
  m_logger->LogInfo("UIConfigVisualizer destroyed");
}

void UIConfigVisualizer::InitializeCameraFeed() {
  if (m_selectedCameraId.empty() || !m_cameraManager) {
    m_cameraSystemInitialized = false;
    return;
  }

  // Clear existing subscription
  ClearCameraFeed();

  // Create new subscriber
  m_cameraSubscriber = std::make_shared<UIConfigVisualizerSubscriber>(m_selectedCameraId);

  // Subscribe to the broadcasting system
  m_cameraManager->SubscribeToFrames(m_cameraSubscriber);

  // Start broadcast system if not already active
  m_cameraManager->StartBroadcastSystem();

  m_cameraSystemInitialized = true;
  m_logger->LogInfo("UIConfigVisualizer camera feed initialized with broadcasting for: " + m_selectedCameraId);
}

void UIConfigVisualizer::SetSelectedCamera(const std::string& cameraId) {
  if (m_selectedCameraId == cameraId) {
    return; // No change
  }

  m_selectedCameraId = cameraId;

  if (m_cameraSubscriber) {
    // Update existing subscriber target
    m_cameraSubscriber->SetTargetCamera(cameraId);
    m_logger->LogInfo("UIConfigVisualizer camera switched to: " + cameraId);
  }
  else {
    // Initialize new feed
    InitializeCameraFeed();
  }
}

void UIConfigVisualizer::ClearCameraFeed() {
  if (m_cameraSubscriber && m_cameraManager) {
    m_cameraManager->UnsubscribeFromFrames(m_cameraSubscriber->GetSubscriberId());
    m_cameraSubscriber.reset();
    m_logger->LogInfo("UIConfigVisualizer camera feed cleared");
  }
  m_cameraSystemInitialized = false;
}

void UIConfigVisualizer::RenderUI() {
  if (!showWindow) return;

  // Render graph controls at the top
  RenderGraphControls();

  ImGui::Separator();

  // Display instructions
  ImGui::Text("Drag nodes to reposition them. Positions will be saved automatically.");
  ImGui::Text("Use middle mouse button to pan, mouse wheel to zoom.");

  ImGui::Separator();

  // NEW LAYOUT: Left panel (20%) + Middle canvas (55%) + Right panel (25%)
  ImVec2 contentSize = ImGui::GetContentRegionAvail();
  float leftPanelWidth = contentSize.x * 0.20f;
  float canvasWidth = contentSize.x * 0.55f;
  float rightPanelWidth = contentSize.x * 0.25f;

  // Left Panel - Node Actions and Camera Feed (20% width)
  ImGui::BeginChild("LeftPanel", ImVec2(leftPanelWidth, contentSize.y), true);
  RenderLeftPanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // Middle Panel - Graph Canvas (55% width)
  ImGui::BeginChild("MiddlePanel", ImVec2(canvasWidth, contentSize.y), false);
  RenderGraphCanvas();
  ImGui::EndChild();

  ImGui::SameLine();

  // Right Panel - Device Position Information (25% width)
  ImGui::BeginChild("RightPanel", ImVec2(rightPanelWidth, contentSize.y), true);
  RenderDevicePositionsPanel();
  ImGui::EndChild();

  // Render properties dialog (this renders on top of everything)
  if (m_propertiesHandler) {
    m_propertiesHandler->RenderPropertiesDialog();
  }
}

void UIConfigVisualizer::RenderLeftPanel() {
  ImVec2 leftPanelSize = ImGui::GetContentRegionAvail();

  // Calculate camera canvas size first based on aspect ratio
  const float CAMERA_ASPECT_RATIO = 1280.0f / 1024.0f; // 1.25
  float availableWidth = leftPanelSize.x;
  float cameraCanvasHeight = availableWidth / CAMERA_ASPECT_RATIO + 80.0f; // +80 for dropdown and spacing

  // Node info gets the remaining space
  float nodeInfoHeight = leftPanelSize.y - cameraCanvasHeight - 10.0f; // 10px spacing between sections

  // Ensure minimum height for node info section
  if (nodeInfoHeight < 100.0f) {
    nodeInfoHeight = 100.0f;
    cameraCanvasHeight = leftPanelSize.y - nodeInfoHeight - 10.0f;
  }

  // === NODE INFORMATION SECTION (TOP - DYNAMIC HEIGHT) ===
  ImGui::BeginChild("NodeInfoSection", ImVec2(0, nodeInfoHeight), true);

  ImGui::Text("Node Information");
  ImGui::Separator();

  // Show node information and actions if a node is selected
  if (!m_selectedNodeId.empty()) {
    // Get node information for display
    auto graphOpt = configManager.GetGraph(m_activeGraph);
    if (graphOpt.has_value()) {
      const auto& graph = graphOpt.value().get();

      // Find the selected node
      const Node* selectedNode = nullptr;
      for (const auto& node : graph.Nodes) {
        if (node.Id == m_selectedNodeId) {
          selectedNode = &node;
          break;
        }
      }

      if (selectedNode) {
        // Display selected node information
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "Selected Node:");
        ImGui::Text("ID: %s", selectedNode->Id.c_str());

        if (!selectedNode->Label.empty()) {
          ImGui::Text("Label: %s", selectedNode->Label.c_str());
        }

        ImGui::Text("Device: %s", selectedNode->Device.c_str());
        ImGui::Text("Position: %s", selectedNode->Position.c_str());
        ImGui::Text("Graph Pos: (%d, %d)", selectedNode->X, selectedNode->Y);

        // Show position coordinates directly in the panel
        ImGui::Separator();
        ImGui::Text("Position Coordinates:");

        // Get the position data if available
        if (!selectedNode->Device.empty() && !selectedNode->Position.empty()) {
          auto positionOpt = configManager.GetNamedPosition(selectedNode->Device, selectedNode->Position);
          if (positionOpt.has_value()) {
            const auto& position = positionOpt.value().get();

            // Display coordinates with proper formatting
            ImGui::Text("X: %.6f", position.x);
            ImGui::Text("Y: %.6f", position.y);
            ImGui::Text("Z: %.6f", position.z);

            // Show U, V, W for hex devices
            if (selectedNode->Device.find("hex") != std::string::npos) {
              ImGui::Text("U: %.6f", position.u);
              ImGui::Text("V: %.6f", position.v);
              ImGui::Text("W: %.6f", position.w);
            }
          }
          else {
            ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), "Position data not found");
          }
        }
        else {
          ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), "No device/position assigned");
        }


        ImGui::Separator();
        ImGui::Text("Actions:");

        // Move Device To Node button (using MoveToNode function)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));

        if (ImGui::Button("-> Move Device To Node", ImVec2(-1, 30))) {
          if (m_machineOperations && !selectedNode->Device.empty()) {
            // Use MoveToNode function with device, graph, and node ID
            m_logger->LogInfo(">>> Executing MoveToNode for node: " + m_selectedNodeId +
              " (Device: " + selectedNode->Device + ", Graph: " + m_activeGraph + ")");

            // Call the correct MoveToNode method
            m_machineOperations->MoveDeviceToNode(selectedNode->Device, m_activeGraph, m_selectedNodeId);
          }
          else if (!selectedNode->Device.empty()) {
            // MachineOperations not available - log only
            m_logger->LogInfo(">>> MoveToNode SELECTED for node: " + m_selectedNodeId +
              " (Device: " + selectedNode->Device + ", Graph: " + m_activeGraph + ") - MachineOperations not available");
          }
          else {
            // No device assigned
            m_logger->LogWarning("Cannot move device: Node " + m_selectedNodeId + " has no device assigned");
          }
        }

        ImGui::PopStyleColor(3);

        // Show tooltip for Move To Node button
        if (ImGui::IsItemHovered()) {
          if (m_machineOperations) {
            ImGui::SetTooltip("Move device to this node using graph navigation\n(MachineOperations available)");
          }
          else {
            ImGui::SetTooltip("Movement functionality not available\n(MachineOperations not set)");
          }
        }

        // Add some spacing between buttons
        ImGui::Spacing();

        // Move To Point Name button (using MoveToPointName function)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.9f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.7f, 1.0f));

        if (ImGui::Button("-> Move To Point Name", ImVec2(-1, 30))) {
          if (m_machineOperations && !selectedNode->Device.empty() && !selectedNode->Position.empty()) {
            // Use MoveToPointName function with device and position name
            m_logger->LogInfo(">>> Executing MoveToPointName for node: " + m_selectedNodeId +
              " (Device: " + selectedNode->Device + ", Position: " + selectedNode->Position + ")");

            // Call the MoveToPointName method
            m_machineOperations->MoveToPointName(selectedNode->Device, selectedNode->Position, false, "UIConfigVisualizer");
          }
          else if (!selectedNode->Device.empty() && !selectedNode->Position.empty()) {
            // MachineOperations not available - log only
            m_logger->LogInfo(">>> MoveToPointName SELECTED for node: " + m_selectedNodeId +
              " (Device: " + selectedNode->Device + ", Position: " + selectedNode->Position + ") - MachineOperations not available");
          }
          else {
            // No device/position assigned
            m_logger->LogWarning("Cannot move to point: Node " + m_selectedNodeId + " has no device or position assigned");
          }
        }

        ImGui::PopStyleColor(3);

        // Show tooltip for Move To Point Name button
        if (ImGui::IsItemHovered()) {
          if (m_machineOperations) {
            ImGui::SetTooltip("Move device directly to the named position\n(MachineOperations available)");
          }
          else {
            ImGui::SetTooltip("Movement functionality not available\n(MachineOperations not set)");
          }
        }
      }
    }
  }
  else {
    // No node selected - show placeholder
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No node selected");
    ImGui::Spacing();
    ImGui::TextWrapped("Click on a node in the graph to view its information and coordinates.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Graph Statistics:");

    // Show some graph statistics
    auto graphOpt = configManager.GetGraph(m_activeGraph);
    if (graphOpt.has_value()) {
      const auto& graph = graphOpt.value().get();
      ImGui::Text("Nodes: %zu", graph.Nodes.size());
      ImGui::Text("Edges: %zu", graph.Edges.size());
    }
  }

  ImGui::EndChild();

  // === CAMERA CANVAS SECTION (BOTTOM - FIXED POSITION) ===
  ImGui::Spacing();
  RenderCameraCanvas(cameraCanvasHeight);
}

void UIConfigVisualizer::RenderCameraCanvas(float height) {
  static bool cameraExpanded = true;

  if (ImGui::CollapsingHeader("Camera / Image Feed", ImGuiTreeNodeFlags_DefaultOpen)) {
    cameraExpanded = true;

    // Camera source selection
    static int selectedSource = 0;
    const char* sources[] = {
        "Live Camera Feed",
        "Node Reference Image",
        "Saved Image 1",
        "Saved Image 2"
    };

    ImGui::Text("Source:");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##CameraSource", &selectedSource, sources, IM_ARRAYSIZE(sources))) {
      m_logger->LogInfo("Camera source changed to: " + std::string(sources[selectedSource]));
    }

    // Camera controls (only for live feed)
    if (selectedSource == 0) {
      RenderCameraControls();
    }

    ImGui::Spacing();

    // Calculate canvas size with proper aspect ratio
    const float CAMERA_ASPECT_RATIO = 1280.0f / 1024.0f;
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float availableHeight = height - 120.0f;

    ImVec2 canvasSize;
    canvasSize.x = availableWidth;
    canvasSize.y = availableWidth / CAMERA_ASPECT_RATIO;

    if (canvasSize.x < 160.0f) {
      canvasSize.x = 160.0f;
      canvasSize.y = 160.0f / CAMERA_ASPECT_RATIO;
    }

    // Create camera canvas
    ImGui::BeginChild("CameraCanvas", canvasSize, false, ImGuiWindowFlags_NoScrollbar);

    if (selectedSource == 0) {
      // Live camera feed using broadcasting
      RenderCameraFeedFromSubscriber(canvasSize);
    }
    else {
      // Placeholder for other sources
      RenderCameraPlaceholder(canvasSize, std::string(sources[selectedSource]) + "\n(Coming Soon)");
    }

    ImGui::EndChild();
  }
}

void UIConfigVisualizer::RenderCameraFeedFromSubscriber(ImVec2 canvasSize) {
  // Update texture from subscriber if needed (must be done on main thread)
  if (m_cameraSubscriber) {
    m_cameraSubscriber->UpdateTextureIfNeeded();
  }

  if (m_cameraSubscriber && m_cameraSubscriber->HasValidTexture()) {
    // Calculate display size maintaining aspect ratio
    float aspectRatio = (float)m_cameraSubscriber->GetTextureWidth() /
      (float)m_cameraSubscriber->GetTextureHeight();

    float displayWidth = canvasSize.x;
    float displayHeight = displayWidth / aspectRatio;

    if (displayHeight > canvasSize.y) {
      displayHeight = canvasSize.y;
      displayWidth = displayHeight * aspectRatio;
    }

    // Center the image
    float offsetX = (canvasSize.x - displayWidth) * 0.5f;
    float offsetY = (canvasSize.y - displayHeight) * 0.5f;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);

    // Display the image
    ImGui::Image((ImTextureID)(intptr_t)m_cameraSubscriber->GetTextureID(),
      ImVec2(displayWidth, displayHeight));

    // Add debug overlay on hover
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Live Camera Feed (Broadcasting)\nCamera: %s\nResolution: %ux%u\nFrames: %llu",
        m_selectedCameraId.c_str(),
        m_cameraSubscriber->GetTextureWidth(),
        m_cameraSubscriber->GetTextureHeight(),
        m_cameraSubscriber->GetTotalFramesReceived());
    }
  }
  else {
    // Show status-based placeholder
    std::string message;
    if (!m_cameraSystemInitialized) {
      message = "Camera System Not Initialized\nNo camera manager available";
    }
    else if (!m_cameraSubscriber) {
      message = "No Camera Subscriber\nSelect a camera to begin";
    }
    else if (!m_cameraSubscriber->IsCameraConnected()) {
      message = "Camera Disconnected\nCamera: " + m_selectedCameraId + "\nCheck camera connection";
    }
    else if (!m_cameraSubscriber->IsCameraGrabbing()) {
      message = "Camera Not Grabbing\nCamera: " + m_selectedCameraId + "\nStart live feed to begin";
    }
    else {
      message = "Waiting for Video Frames...\nCamera: " + m_selectedCameraId +
        "\nFrames received: " + std::to_string(m_cameraSubscriber->GetTotalFramesReceived());
    }

    RenderCameraPlaceholder(canvasSize, message);
  }
}

void UIConfigVisualizer::RenderCameraPlaceholder(ImVec2 canvasSize, const std::string& message) {
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();
  ImVec2 canvasMax = ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);

  // Background
  drawList->AddRectFilled(canvasPos, canvasMax, IM_COL32(80, 80, 80, 255));
  drawList->AddRect(canvasPos, canvasMax, IM_COL32(100, 150, 200, 255), 0.0f, 0, 2.0f);

  // Status text
  ImVec2 textSize = ImGui::CalcTextSize(message.c_str());
  ImVec2 textPos = ImVec2(
    canvasPos.x + (canvasSize.x - textSize.x) * 0.5f,
    canvasPos.y + (canvasSize.y - textSize.y) * 0.5f
  );
  drawList->AddText(textPos, IM_COL32(200, 200, 200, 255), message.c_str());
}

void UIConfigVisualizer::RenderCameraControls() {
  if (!m_cameraManager) {
    ImGui::Text("Camera Manager not available");
    return;
  }

  auto cameraIds = m_cameraManager->GetCameraIds();
  if (!cameraIds.empty()) {
    ImGui::Text("Camera:");

    // Find current selection index
    int currentCameraIndex = 0;
    for (size_t i = 0; i < cameraIds.size(); i++) {
      if (cameraIds[i] == m_selectedCameraId) {
        currentCameraIndex = (int)i;
        break;
      }
    }

    // Create camera selection array
    std::vector<const char*> cameraNames;
    for (const auto& id : cameraIds) {
      cameraNames.push_back(id.c_str());
    }

    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##CameraSelection", &currentCameraIndex, cameraNames.data(), (int)cameraNames.size())) {
      SetSelectedCamera(cameraIds[currentCameraIndex]);
    }

    // Camera status and controls
    if (!m_selectedCameraId.empty()) {
      ImGui::Spacing();
      auto status = m_cameraManager->GetCameraStatus(m_selectedCameraId);

      // Connection status
      ImGui::Text("Status: %s", status.connected ? "Connected" : "Disconnected");

      if (status.connected) {
        ImGui::SameLine();
        ImGui::Text("| Grabbing: %s", status.grabbing ? "Yes" : "No");

        // Broadcasting status
        if (m_cameraSubscriber) {
          ImGui::SameLine();
          ImGui::TextColored(ImVec4(0, 1, 0, 1), "| Broadcasting");
        }

        // Start/Stop grabbing controls
        if (!status.grabbing) {
          if (ImGui::Button("Start Live Feed", ImVec2(120, 25))) {
            m_cameraManager->StartGrabbing(m_selectedCameraId);
            m_logger->LogInfo("Started grabbing for camera: " + m_selectedCameraId);
          }
        }
        else {
          if (ImGui::Button("Stop Live Feed", ImVec2(120, 25))) {
            m_cameraManager->StopGrabbing(m_selectedCameraId);
            m_logger->LogInfo("Stopped grabbing for camera: " + m_selectedCameraId);
          }
        }

        ImGui::SameLine();
        if (ImGui::Button("Reconnect", ImVec2(80, 25))) {
          m_cameraManager->ConnectCamera(m_selectedCameraId);
          m_logger->LogInfo("Reconnecting camera: " + m_selectedCameraId);
        }
      }
      else {
        if (ImGui::Button("Connect Camera", ImVec2(120, 25))) {
          m_cameraManager->ConnectCamera(m_selectedCameraId);
          m_logger->LogInfo("Connecting camera: " + m_selectedCameraId);
        }
      }

      // Show subscriber statistics
      if (m_cameraSubscriber) {
        ImGui::Spacing();
        ImGui::Text("Feed Stats:");
        ImGui::Text("  Frames: %llu", m_cameraSubscriber->GetTotalFramesReceived());
        ImGui::Text("  Connected: %s", m_cameraSubscriber->IsCameraConnected() ? "Yes" : "No");
        ImGui::Text("  Grabbing: %s", m_cameraSubscriber->IsCameraGrabbing() ? "Yes" : "No");
        ImGui::Text("  Has Texture: %s", m_cameraSubscriber->HasValidTexture() ? "Yes" : "No");

        if (m_cameraSubscriber->HasValidTexture()) {
          ImGui::Text("  Resolution: %ux%u",
            m_cameraSubscriber->GetTextureWidth(),
            m_cameraSubscriber->GetTextureHeight());
        }
      }
    }
  }
  else {
    ImGui::Text("No cameras available");
    if (ImGui::Button("Refresh Camera List", ImVec2(-1, 25))) {
      // Trigger camera manager to refresh
      m_logger->LogInfo("Refreshing camera list");
    }
  }
}

void UIConfigVisualizer::RenderGraphControls() {
  // Graph selection dropdown
  const auto& allGraphs = configManager.GetAllGraphs();
  if (ImGui::BeginCombo("Select Graph", m_activeGraph.c_str())) {
    for (const auto& [graphName, graph] : allGraphs) {
      bool isSelected = (m_activeGraph == graphName);
      if (ImGui::Selectable(graphName.c_str(), isSelected)) {
        SetActiveGraph(graphName);
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  ImGui::SameLine();

  // Zoom controls
  if (ImGui::Button("Zoom In")) {
    m_zoomLevel = (std::min)(m_zoomLevel * 1.2f, 3.0f);
  }
  ImGui::SameLine();
  if (ImGui::Button("Zoom Out")) {
    m_zoomLevel = (std::max)(m_zoomLevel / 1.2f, 0.3f);
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset View")) {
    m_zoomLevel = 1.0f;
    m_panOffset = ImVec2(0, 0);
  }

  // Display current zoom level
  ImGui::SameLine();
  ImGui::Text("Zoom: %.1f%%", m_zoomLevel * 100.0f);
}

void UIConfigVisualizer::RenderGraphCanvas() {
  // Calculate canvas size - it should fill the remaining space in the middle panel
  ImVec2 canvasSize = ImGui::GetContentRegionAvail();

  // Ensure we have at least some space to draw
  if (canvasSize.x < 50.0f) canvasSize.x = 50.0f;
  if (canvasSize.y < 50.0f) canvasSize.y = 50.0f;

  // Create a child frame for the canvas
  ImGui::BeginChildFrame(ImGui::GetID("GraphCanvas"), canvasSize,
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

  // Get the canvas position for coordinate calculations
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();

  // Track if the canvas is hovered
  m_isCanvasHovered = ImGui::IsWindowHovered();

  // Get the draw list for custom rendering
  ImDrawList* drawList = ImGui::GetWindowDrawList();

  // Handle input first
  HandleInput(canvasPos, canvasSize);

  // Render the graph
  RenderBackground(drawList, canvasPos, canvasSize);

  // Only draw graph content if we have a selected graph
  if (!m_activeGraph.empty()) {
    RenderEdges(drawList, canvasPos);
    RenderNodes(drawList, canvasPos);
  }
  else {
    // Show message when no graph is selected
    ImVec2 textSize = ImGui::CalcTextSize("No graph selected");
    ImVec2 textPos = ImVec2(
      canvasPos.x + (canvasSize.x - textSize.x) * 0.5f,
      canvasPos.y + (canvasSize.y - textSize.y) * 0.5f
    );
    drawList->AddText(textPos, IM_COL32(150, 150, 150, 255), "No graph selected");
  }

  ImGui::EndChildFrame();
}

void UIConfigVisualizer::RenderDevicePositionsPanel() {
  ImGui::Text("Device Positions");
  ImGui::Separator();

  if (!m_machineOperations) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "MachineOperations not available");
    ImGui::TextWrapped("Device position information requires MachineOperations to be initialized.");
    return;
  }

  // Get current positions from MachineOperations
  auto currentPositions = m_machineOperations->GetCurrentPositions();

  if (currentPositions.empty()) {
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "No device positions available");
    if (ImGui::Button("Refresh Positions", ImVec2(-1, 25))) {
      m_machineOperations->UpdateAllCurrentPositions();
      RefreshPositionNames();
    }
    return;
  }

  // Add refresh button
  if (ImGui::Button("Refresh", ImVec2(-1, 25))) {
    m_machineOperations->UpdateAllCurrentPositions();
    RefreshPositionNames();
  }

  ImGui::Separator();

  // Update position names cache if needed
  auto now = std::chrono::steady_clock::now();
  if (now - m_lastPositionNameUpdate > POSITION_NAME_CACHE_TIMEOUT) {
    RefreshPositionNames();
  }

  // Create table for device positions
  if (ImGui::BeginTable("DevicePositionsTable", 1, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders)) {

    size_t deviceIndex = 0;
    size_t totalDevices = currentPositions.size();

    for (const auto& [deviceName, position] : currentPositions) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();

      // Device header
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 1.0f, 0.8f, 1.0f));
      ImGui::Text("%s", deviceName.c_str());
      ImGui::PopStyleColor();

      // Get current position name from cache
      std::string currentPosName;
      auto it = m_cachedPositionNames.find(deviceName);
      if (it != m_cachedPositionNames.end()) {
        currentPosName = it->second;
      }

      // Position name
      if (!currentPosName.empty()) {
        ImGui::Text("Position: %s", currentPosName.c_str());
      }
      else {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Position: Not at named position");
      }

      // Node name - simple implementation
      std::string currentNodeName = GetDeviceCurrentNodeName(deviceName);
      if (!currentNodeName.empty()) {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "Node: %s", currentNodeName.c_str());
      }
      else {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Node: Not at any node");
      }

      // Coordinates
      ImGui::Text("X: %.3f", position.x);
      ImGui::Text("Y: %.3f", position.y);
      ImGui::Text("Z: %.3f", position.z);

      // Show rotation for hex devices
      if (deviceName.find("hex") != std::string::npos) {
        ImGui::Text("U: %.3f", position.u);
        ImGui::Text("V: %.3f", position.v);
        ImGui::Text("W: %.3f", position.w);
      }

      // Add some spacing between devices
      ImGui::Spacing();
      deviceIndex++;
      if (deviceIndex < totalDevices) {
        ImGui::Separator();
      }
    }

    ImGui::EndTable();
  }

  // Auto-refresh checkbox with configurable rate
  ImGui::Separator();
  static bool autoRefresh = false;
  static int refreshRateMs = 1000; // Default 1 second

  ImGui::Checkbox("Auto-refresh", &autoRefresh);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80);
  ImGui::InputInt("ms", &refreshRateMs, 100, 500);
  refreshRateMs = (std::max)(100, (std::min)(refreshRateMs, 5000)); // Clamp between 100ms and 5s

  // Handle auto-refresh with configurable rate
  if (autoRefresh) {
    static auto lastRefresh = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRefresh).count();

    if (elapsed >= refreshRateMs) {
      m_machineOperations->UpdateAllCurrentPositions();
      RefreshPositionNames();
      lastRefresh = now;
    }

    // Show next refresh countdown
    auto remaining = refreshRateMs - elapsed;
    ImGui::Text("Next refresh: %dms", (int)remaining);
  }
}

void UIConfigVisualizer::SetActiveGraph(const std::string& graphName) {
  if (m_activeGraph != graphName) {
    m_activeGraph = graphName;
    m_zoomLevel = 1.0f;
    m_panOffset = ImVec2(0, 0);
    // Clear selection when changing graphs
    m_selectedNodeId.clear();
    m_showNodeActions = false;
    m_logger->LogInfo("Active graph set to: " + graphName);
  }
}

void UIConfigVisualizer::ToggleWindow() {
  showWindow = !showWindow;
}

void UIConfigVisualizer::SetMachineOperations(MachineOperations* machineOps) {
  m_machineOperations = machineOps;

  if (m_machineOperations) {
    m_logger->LogInfo("UIConfigVisualizer: MachineOperations set successfully");
  }
  else {
    m_logger->LogInfo("UIConfigVisualizer: MachineOperations cleared");
  }
}

// Helper methods that don't need changes for camera migration
std::string UIConfigVisualizer::GetDeviceCurrentNodeName(const std::string& deviceName) {
  if (!m_machineOperations || m_activeGraph.empty()) {
    return "";
  }

  try {
    // Get current node from active graph
    std::string nodeId = m_machineOperations->GetDeviceCurrentNode(deviceName, m_activeGraph);
    if (nodeId.empty()) {
      return "";
    }

    // Get node label from graph if available
    auto graphOpt = configManager.GetGraph(m_activeGraph);
    if (graphOpt.has_value()) {
      const auto& graph = graphOpt.value().get();
      for (const auto& node : graph.Nodes) {
        if (node.Id == nodeId && node.Device == deviceName) {
          // Return label if available, otherwise return node ID
          return node.Label.empty() ? nodeId : node.Label;
        }
      }
    }

    // Fallback to just node ID
    return nodeId;
  }
  catch (...) {
    // Return empty string if any error occurs
    return "";
  }
}

void UIConfigVisualizer::RefreshPositionNames() {
  if (!m_machineOperations) return;

  auto currentPositions = m_machineOperations->GetCurrentPositions();
  m_cachedPositionNames.clear();

  for (const auto& [deviceName, position] : currentPositions) {
    std::string posName = m_machineOperations->GetDeviceCurrentPositionName(deviceName);
    m_cachedPositionNames[deviceName] = posName;
  }

  m_lastPositionNameUpdate = std::chrono::steady_clock::now();
}

// Note: The methods RenderBackground, RenderNodes, RenderEdges, HandleInput, 
// HandleZooming, HandlePanning, HandleNodeDragging, HandleNodeSelection,
// GraphToCanvas, CanvasToGraph, GetNodePosition, SaveNodePosition, DrawArrow,
// and GetNodeAtPosition remain unchanged as they are implemented in the 
// separate files (UIConfigVisualizer_Graph.cpp, UIConfigVisualizer_Input.cpp, 
// UIConfigVisualizer_Helpers.cpp) and don't need camera-related updates.