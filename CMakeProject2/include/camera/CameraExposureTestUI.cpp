#include "CameraExposureTestUI.h"
#include "include/machine_operations.h"
#include "imgui.h"
#include <iostream>
#include <iomanip>
#include <sstream>

// Static node definitions
const std::vector<std::pair<std::string, std::string>> CameraExposureTestUI::s_nodeDefinitions = {
    {"node_4083", "Sled"},
    {"node_4107", "PIC"},
    {"node_4137", "Coll Lens"},
    {"node_4156", "Focus Lens"},
    {"node_4186", "Pick Coll"},
    {"node_4209", "Pick Focus"},
    {"node_4500", "Serial Number"}
};

CameraExposureTestUI::CameraExposureTestUI(MachineOperations& machineOps)
  : m_machineOps(machineOps)
  , m_showUI(false)
  , m_cacheValid(false)
{
  // Initialize empty cache
  m_cachedNodes.reserve(s_nodeDefinitions.size());
  for (const auto& [nodeId, displayName] : s_nodeDefinitions) {
    CachedNodeInfo nodeInfo;
    nodeInfo.nodeId = nodeId;
    nodeInfo.displayName = displayName;
    nodeInfo.hasValidSettings = false;
    m_cachedNodes.push_back(nodeInfo);
  }

  std::cout << "CameraExposureTestUI initialized (on-demand updates)" << std::endl;
}

void CameraExposureTestUI::UpdateCacheIfNeeded() {
  std::lock_guard<std::mutex> lock(m_cacheMutex);

  auto* expManager = m_machineOps.GetCameraExposureManager();
  if (!expManager) {
    return;
  }

  std::cout << "Updating camera exposure cache..." << std::endl;

  // Update default settings
  try {
    m_cachedDefaultSettings = expManager->GetDefaultSettings();
    m_defaultButtonLabel = "Apply Default (" +
      std::to_string(static_cast<int>(m_cachedDefaultSettings.exposure_time / 1000.0)) +
      "ms, gain " +
      std::to_string(m_cachedDefaultSettings.gain).substr(0, 3) + ")";
  }
  catch (const std::exception& e) {
    std::cerr << "Error updating default settings: " << e.what() << std::endl;
  }

  // Update node settings
  for (auto& nodeInfo : m_cachedNodes) {
    try {
      nodeInfo.hasValidSettings = expManager->HasSettingsForNode(nodeInfo.nodeId);

      if (nodeInfo.hasValidSettings) {
        nodeInfo.settings = expManager->GetSettingsForNode(nodeInfo.nodeId);
      }
      else {
        nodeInfo.settings = m_cachedDefaultSettings;
      }

      nodeInfo.buttonLabel = GenerateButtonLabel(nodeInfo.displayName, nodeInfo.settings);
    }
    catch (const std::exception& e) {
      std::cerr << "Error updating settings for node " << nodeInfo.nodeId << ": " << e.what() << std::endl;
      nodeInfo.hasValidSettings = false;
      nodeInfo.buttonLabel = nodeInfo.displayName + " (Error)";
    }
  }

  m_cacheValid = true;
  std::cout << "Camera exposure cache updated successfully" << std::endl;
}

std::string CameraExposureTestUI::GenerateButtonLabel(const std::string& displayName, const CameraExposureSettings& settings) {
  std::ostringstream oss;
  oss << "Test " << displayName << " (";
  oss << std::fixed << std::setprecision(0) << (settings.exposure_time / 1000.0) << "ms, ";
  oss << "gain " << std::fixed << std::setprecision(1) << settings.gain << ")";
  return oss.str();
}

void CameraExposureTestUI::RenderUI() {
  if (!m_showUI) {
    return;
  }

  ImGui::Begin("Camera Exposure Testing", &m_showUI);

  // Camera Status section
  ImGui::Text("Camera Status:");
  if (ImGui::Button("Show Complete Camera Status")) {
    if (m_machineOps.GetCameraExposureManager()) {
      std::cout << "Show camera status requested" << std::endl;
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Read Current Camera Settings")) {
    m_machineOps.TestCameraSettings();
  }

  ImGui::SameLine();
  if (ImGui::Button("Refresh Settings")) {
    m_cacheValid = false; // Invalidate cache
    UpdateCacheIfNeeded(); // Update immediately
  }

  ImGui::Separator();

  // Test Node Settings section
  ImGui::Text("Test Node Settings:");

  auto* expManager = m_machineOps.GetCameraExposureManager();
  if (!expManager) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Camera Exposure Manager not available");
    ImGui::End();
    return;
  }

  // Update cache if not valid (first time or after refresh)
  if (!m_cacheValid) {
    UpdateCacheIfNeeded();
  }

  // Use cached data (thread-safe read)
  std::lock_guard<std::mutex> lock(m_cacheMutex);

  if (!m_cacheValid) {
    ImGui::Text("Failed to load settings...");
  }
  else {
    for (const auto& nodeInfo : m_cachedNodes) {
      if (nodeInfo.hasValidSettings) {
        if (ImGui::Button(nodeInfo.buttonLabel.c_str())) {
          m_machineOps.TestCameraSettings(nodeInfo.nodeId);
        }

        // Show description on the same line if available
        if (!nodeInfo.settings.description.empty()) {
          ImGui::SameLine();
          ImGui::TextDisabled("(%s)", nodeInfo.settings.description.c_str());
        }
      }
      else {
        // Show disabled button for nodes without settings
        ImGui::BeginDisabled();
        ImGui::Button((nodeInfo.displayName + " (No settings)").c_str());
        ImGui::EndDisabled();
      }
    }
  }

  ImGui::Separator();
  ImGui::Text("Manual Apply (without gantry movement):");

  // Use cached default settings
  if (!m_defaultButtonLabel.empty()) {
    if (ImGui::Button(m_defaultButtonLabel.c_str())) {
      m_machineOps.ApplyDefaultCameraExposure();
    }
  }
  else {
    if (ImGui::Button("Apply Default Exposure")) {
      m_machineOps.ApplyDefaultCameraExposure();
    }
  }

  // Quick access buttons for common nodes using cached data
  bool sledFound = false, focusFound = false;
  for (const auto& nodeInfo : m_cachedNodes) {
    if (nodeInfo.nodeId == "node_4083" && nodeInfo.hasValidSettings) {
      std::string sledLabel = "Apply Sled (" +
        std::to_string(static_cast<int>(nodeInfo.settings.exposure_time / 1000.0)) +
        "ms, gain " +
        std::to_string(nodeInfo.settings.gain).substr(0, 3) + ")";

      if (ImGui::Button(sledLabel.c_str())) {
        m_machineOps.ApplyCameraExposureForNode("node_4083");
      }
      sledFound = true;
    }
    else if (nodeInfo.nodeId == "node_4156" && nodeInfo.hasValidSettings) {
      if (sledFound) ImGui::SameLine();

      std::string focusLabel = "Apply Focus (" +
        std::to_string(static_cast<int>(nodeInfo.settings.exposure_time / 1000.0)) +
        "ms, gain " +
        std::to_string(nodeInfo.settings.gain).substr(0, 3) + ")";

      if (ImGui::Button(focusLabel.c_str())) {
        m_machineOps.ApplyCameraExposureForNode("node_4156");
      }
      focusFound = true;
    }

    if (sledFound && focusFound) break;
  }

  ImGui::Separator();
  ImGui::Text("Auto Exposure Control:");

  // Show current auto exposure status (no caching needed for this)
  bool autoEnabled = m_machineOps.IsAutoExposureEnabled();
  ImGui::Text("Auto Exposure: %s", autoEnabled ? "ENABLED" : "DISABLED");

  if (ImGui::Button(autoEnabled ? "Disable Auto Exposure" : "Enable Auto Exposure")) {
    m_machineOps.SetAutoExposureEnabled(!autoEnabled);
  }

  if (autoEnabled) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Camera exposure will change automatically when gantry moves");
  }
  else {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Camera exposure is manual only");
  }

  ImGui::Separator();
  ImGui::Text("Quick Actions:");

  if (ImGui::Button("Reload Config from File")) {
    if (expManager->LoadConfiguration("camera_exposure_config.json")) {
      std::cout << "[yes] Configuration reloaded successfully" << std::endl;
      m_cacheValid = false; // Invalidate cache so it refreshes next time UI is used
    }
    else {
      std::cout << "[no] Failed to reload configuration" << std::endl;
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Test Auto Exposure")) {
    std::cout << "Testing automatic camera exposure integration..." << std::endl;

    if (expManager) {
      std::cout << "[yes] Camera exposure manager exists" << std::endl;

      if (autoEnabled) {
        std::cout << "[yes] Auto exposure is enabled" << std::endl;
        std::cout << "Move the gantry to different nodes to see automatic exposure changes" << std::endl;
      }
      else {
        std::cout << "[no] Auto exposure is disabled - enable it to test automatic changes" << std::endl;
      }
    }
    else {
      std::cout << "[no] Camera exposure manager not found" << std::endl;
    }
  }

  // Simple debug section
  if (ImGui::CollapsingHeader("Debug Information")) {
    ImGui::Text("Cache status: %s", m_cacheValid ? "Valid" : "Invalid");
    ImGui::Text("Cached nodes: %zu", m_cachedNodes.size());

    if (ImGui::Button("Force Cache Refresh")) {
      m_cacheValid = false;
      UpdateCacheIfNeeded();
    }

    if (m_cacheValid) {
      ImGui::Text("Node settings:");
      for (const auto& nodeInfo : m_cachedNodes) {
        ImGui::Text("  %s: %s",
          nodeInfo.nodeId.c_str(),
          nodeInfo.hasValidSettings ? "Valid" : "No settings");
      }
    }
  }

  // Simple status
  ImGui::Separator();
  ImGui::TextColored(ImVec4(0, 1, 0, 1), "Status: Ready (on-demand updates)");

  ImGui::End();
}