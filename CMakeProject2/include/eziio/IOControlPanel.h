#pragma once

#include "EziIO_Manager.h"
#include "imgui.h"
#include <string>
#include <vector>
#include <map>
#include <chrono>

// Simple class for displaying and controlling specific IO pins
class IOControlPanel {
public:
  IOControlPanel(EziIOManager& manager);
  ~IOControlPanel() = default;

  // Render the control panel UI
  void RenderUI();

  // Show/hide the window
  bool IsVisible() const { return m_showWindow; }
  void ToggleWindow() { m_showWindow = !m_showWindow; }
  void Show() { m_showWindow = true; }
  void Hide() { m_showWindow = false; }

  // Get component name (needed for ToolbarMenu compatibility)
  const std::string& GetName() const { return m_name; }

  // Load configuration from JSON file
  bool LoadConfiguration(const std::string& filename);

private:
  // Reference to the IO manager
  EziIOManager& m_ioManager;

  // Window visibility state
  bool m_showWindow = true;

  // Component name for ToolbarMenu
  std::string m_name = "IO Quick Control";

  // Structure to hold pin configuration
  struct PinConfig {
    std::string deviceName;
    int deviceId;
    int pinNumber;
    std::string label;
    bool currentState = false;
    EziIOError lastError = EziIOError::SUCCESS;
    std::chrono::steady_clock::time_point lastToggleTime;
  };

  // List of output pins to control
  std::vector<PinConfig> m_outputPins;

  // UI State
  bool m_autoRefresh = true;
  float m_refreshInterval = 0.5f;
  float m_refreshTimer = 0.0f;
  bool m_showDebugInfo = false;
  bool m_compactMode = false;
  int m_totalErrors = 0;
  int m_successfulOperations = 0;

  // Error notification
  struct Notification {
    std::string message;
    float timeRemaining;
    bool isError;
  };
  std::vector<Notification> m_notifications;

  // Initialize the list of pins to control
  void InitializePins();

  // Helper methods
  void RefreshPinStates();
  void RenderNotifications();
  void AddNotification(const std::string& message, bool isError = true);
  void UpdateNotifications(float deltaTime);
  uint32_t GetPinMask(int deviceId, int pinNumber) const;
  void RenderPinControl(PinConfig& pin);
  void RenderStatistics();
  std::string GetErrorString(EziIOError error) const;

  // Default configuration filename
  static const std::string DEFAULT_CONFIG_FILE;
};