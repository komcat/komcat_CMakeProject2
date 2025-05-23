#pragma once

#include "CameraExposureManager.h"
#include "pylon_camera.h"
#include <string>
#include <vector>
#include <mutex>

// Forward declaration
class MachineOperations;

struct CachedNodeInfo {
  std::string nodeId;
  std::string displayName;
  CameraExposureSettings settings;
  std::string buttonLabel;
  bool hasValidSettings;
};

class CameraExposureTestUI {
public:
  CameraExposureTestUI(MachineOperations& machineOps);
  ~CameraExposureTestUI() = default;

  // Main UI rendering method
  void RenderUI();

  // Control visibility
  void ToggleWindow() { m_showUI = !m_showUI; }
  bool IsVisible() const { return m_showUI; }

private:
  // Update cached settings on-demand
  void UpdateCacheIfNeeded();

  // Generate button label for a node
  std::string GenerateButtonLabel(const std::string& displayName, const CameraExposureSettings& settings);

  // Reference to machine operations
  MachineOperations& m_machineOps;

  // UI state
  bool m_showUI;
  bool m_cacheValid;

  // Cached settings data
  std::vector<CachedNodeInfo> m_cachedNodes;
  CameraExposureSettings m_cachedDefaultSettings;
  std::string m_defaultButtonLabel;

  // Thread safety for cache access
  std::mutex m_cacheMutex;

  // Node definitions
  static const std::vector<std::pair<std::string, std::string>> s_nodeDefinitions;
};