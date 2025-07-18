// ==============================================================================
// HEADER FILE: CLD101xEquipmentUI.h - Complete Update
// ==============================================================================
#pragma once

#include "imgui.h"
#include <string>
#include <memory>
#include <iostream>

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

  // Manager integration
  CLD101xManager* m_cld101xManager = nullptr;

  // Private render methods
  void RenderConnectionSection();
  void RenderLaserControlSection();
  void RenderTECControlSection();
  void RenderStatusSection();
  void RenderDebugSection();

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
};