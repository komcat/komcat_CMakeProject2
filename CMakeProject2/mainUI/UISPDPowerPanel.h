// UISPDPowerPanel.h
#pragma once

#include <memory>
#include <string>
#include "imgui.h"
#include <chrono>  // Add this include
#include <unordered_map>  // Add this include

// Forward declarations
class SPDPowerSupplyManager;

class UISPDPowerPanel {
public:
  UISPDPowerPanel(SPDPowerSupplyManager& spdManager);
  ~UISPDPowerPanel() = default;

  // Disable copy/move to avoid issues with references
  UISPDPowerPanel(const UISPDPowerPanel&) = delete;
  UISPDPowerPanel& operator=(const UISPDPowerPanel&) = delete;
  UISPDPowerPanel(UISPDPowerPanel&&) = delete;
  UISPDPowerPanel& operator=(UISPDPowerPanel&&) = delete;

  // UI rendering
  void RenderUI();
  void ToggleWindow();
  bool IsVisible() const { return m_showWindow; }
  void SetVisible(bool visible) { m_showWindow = visible; }

  // Operating modes
  enum class OperatingMode {
    CONSTANT_VOLTAGE,
    CONSTANT_CURRENT
  };

private:
  // Reference to SPD manager
  SPDPowerSupplyManager& m_spdManager;

  // UI state
  bool m_showWindow = true;
  std::string m_selectedDeviceId;
  OperatingMode m_currentMode = OperatingMode::CONSTANT_VOLTAGE;

  // Panel rendering methods
  void RenderLeftPanel();      // Connected devices list
  void RenderMiddlePanel();    // Primary settings (CV/CC mode)
  void RenderRightPanel();     // Secondary settings and monitoring

  // Helper methods
  void RenderDeviceList();
  void RenderDeviceControls();
  void RenderGlobalControls();
  void RenderModeSelection();
  void RenderConstantVoltageControls();
  void RenderConstantCurrentControls();
  void RenderOutputControls();
  void RenderQuickPresets();
  void RenderAdvancedSettings();
  void RenderDeviceMonitoring();

  // Mode helpers
  const char* GetModeString(OperatingMode mode) const;
  ImVec4 GetModeColor(OperatingMode mode) const;

  // Cached device status (updated periodically, not every frame)
  struct CachedDeviceStatus {
    bool isValid = false;
    bool outputEnabled = false;
    double voltage = 0.0;
    double current = 0.0;
    std::chrono::steady_clock::time_point lastUpdate;

    bool needsUpdate() const {
      auto now = std::chrono::steady_clock::now();
      return !isValid || (now - lastUpdate) > std::chrono::milliseconds(500); // Update every 500ms
    }
  };

  std::unordered_map<std::string, CachedDeviceStatus> m_deviceStatusCache;

  // Helper method
  const CachedDeviceStatus& GetCachedStatus(const std::string& deviceId);
};