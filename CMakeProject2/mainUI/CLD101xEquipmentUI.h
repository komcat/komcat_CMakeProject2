// ==============================================================================
// HEADER FILE: CLD101xEquipmentUI.h - Redesigned with 3-Column Layout
// ==============================================================================
#pragma once

#include "imgui.h"
#include <string>
#include <memory>
#include <iostream>
#include <chrono>

// Forward declarations for integration
class CLD101xManager;
class CLD101xClient;

class CLD101xEquipmentUI {
public:
  CLD101xEquipmentUI();
  ~CLD101xEquipmentUI() = default;

  // Main render method
  void Render();

  // Visibility control
  bool IsVisible() const { return m_showWindow; }
  void ToggleWindow() { m_showWindow = !m_showWindow; }
  void Show() { m_showWindow = true; }
  void Hide() { m_showWindow = false; }

  // Get component name (for consistency with other UI classes)
  const std::string& GetName() const { return m_name; }

  // Manager integration
  void SetCLD101xManager(CLD101xManager* manager);
  bool IsManagerAvailable() const { return m_cld101xManager != nullptr; }

private:
  // UI state
  bool m_showWindow = true;
  std::string m_name = "CLD101x Equipment Control";

  // Connection settings
  char m_ipAddress[64] = "127.0.0.11";
  int m_port = 65432;

  // Control settings
  float m_currentSetpoint = 0.15f;
  int m_currentMA = 150;
  float m_tempSetpoint = 25.0f;
  int m_tempInt = 25;
  int m_currentPresetIndex = 4; // Default to 150 mA

  // Real status (updated from actual hardware)
  bool m_isConnected = false;
  bool m_laserOn = false;
  bool m_tecOn = false;
  bool m_interlockClosed = true;
  float m_currentTemperature = 25.0f;
  float m_currentLaserCurrent = 0.0f;

  // Enhanced UI state for debugging and error handling
  float m_lastConnectionTime = 0.0f;
  std::string m_lastError = "";
  std::string m_debugOutput = "";
  bool m_autoRefresh = false;
  float m_lastStatusUpdate = 0.0f;
  float m_refreshRate = 2.0f; // Configurable refresh rate in seconds

  // Manager integration
  CLD101xManager* m_cld101xManager = nullptr;

  // === NEW: 3-Panel Layout Rendering Methods ===
  void RenderLeftPanel();     // Connection & Status (30%)
  void RenderMiddlePanel();   // Laser Controls (35%)
  void RenderRightPanel();    // TEC & Debug (35%)

  // === NEW: Section Rendering Methods ===
  void RenderConnectionSection();
  void RenderCurrentReadingsSection();
  void RenderPollingControlsSection();
  void RenderLaserControlSection();
  void RenderCurrentControlSection();
  void RenderTECControlSection();
  void RenderCompactDebugSection();

  // === LEGACY: Keep existing methods for backward compatibility ===
  void RenderLaserControlSection_Legacy();  // Original laser control section
  void RenderTECControlSection_Legacy();    // Original TEC control section
  void RenderStatusSection();               // Original status section
  void RenderDebugSection();                // Original debug section

  // Helper methods
  void UpdateCurrentFromMA();
  void UpdateMAFromCurrent();
  void AddDebugOutput(const std::string& message);
  void UpdateStatusFromManager();

  // Real event handlers (connected to CLD101xManager)
  void OnConnect();
  void OnDisconnect();
  void OnTestConnection();
  void OnLaserOn();
  void OnLaserOff();
  void OnTECOn();
  void OnTECOff();
  void OnSetCurrent();
  void OnSetTemperature();
  void OnSendDebugCommand(const std::string& command);
  void OnGetStatus();
  void OnCheckErrors();
  void ForceImmediateRefresh();
  void UpdateUIFromPollingCache();

  void SyncUIWithHardware();


  // NEW: Status synchronization methods
  void OnSyncHardwareStatus();
  void OnForceStatusQuery();
  void RenderStatusValidationControls();  // Enhanced status section with validation

  void OnAnalyzeTECBehavior();

};