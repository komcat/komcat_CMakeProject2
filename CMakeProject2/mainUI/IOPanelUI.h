// IOPanelUI.h - UI panel for managing EziIO devices
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>

// Forward declarations
class EziIOManager;
class IOConfigManager;
enum class EziIOError;

class IOPanelUI {
public:
  IOPanelUI(EziIOManager& ioManager);
  ~IOPanelUI();

  // Disable copy/move to avoid issues with references
  IOPanelUI(const IOPanelUI&) = delete;
  IOPanelUI& operator=(const IOPanelUI&) = delete;
  IOPanelUI(IOPanelUI&&) = delete;
  IOPanelUI& operator=(IOPanelUI&&) = delete;

  // UI rendering
  void RenderUI();
  void ToggleWindow();
  bool IsVisible() const { return m_showWindow; }
  void SetVisible(bool visible) { m_showWindow = visible; }

  // Set configuration manager (optional, for pin naming)
  void SetConfigManager(IOConfigManager* configManager) { m_configManager = configManager; }

private:
  // Reference to IO manager
  EziIOManager& m_ioManager;

  // Optional config manager for pin naming
  IOConfigManager* m_configManager = nullptr;

  // UI state
  bool m_showWindow = true;
  std::string m_selectedDeviceName;
  bool m_autoRefresh = true;
  float m_refreshInterval = 0.2f;
  float m_refreshTimer = 0.0f;
  bool m_showDebugInfo = false;
  bool m_showErrorNotifications = true;

  // Device state cache with error tracking
  struct DeviceState {
    std::string name;
    int id;
    uint32_t inputs;
    uint32_t latch;
    uint32_t outputs;
    uint32_t outputStatus;
    int inputCount;
    int outputCount;
    bool connected;
    EziIOError lastInputError;
    EziIOError lastOutputError;
    std::string lastErrorMessage;
  };
  std::vector<DeviceState> m_deviceStates;

  // Error notification system
  struct ErrorNotification {
    std::string message;
    float timeRemaining;
    bool isError;
  };
  std::vector<ErrorNotification> m_errorNotifications;

  // Statistics
  int m_totalConnectedDevices = 0;
  int m_totalOperationErrors = 0;
  int m_refreshCount = 0;

  // Panel rendering methods
  void RenderLeftPanel();   // List of IO devices
  void RenderRightPanel();  // Selected device interface

  // Helper methods
  void RenderDeviceList();
  void RenderSelectedDeviceUI();
  void RenderNoSelectionMessage();
  void RenderErrorNotifications();
  void RefreshDeviceStates();

  // Device-specific UI rendering methods
  void RenderDeviceHeader(const DeviceState& device);
  void RenderConnectionControls(DeviceState& device);
  void RenderInputPins(const DeviceState& device);
  void RenderOutputPins(DeviceState& device);
  void RenderDeviceControls(DeviceState& device);
  void RenderUtilityControls();
  void RenderStatistics();

  // Helper functions
  bool IsPinOn(uint32_t value, int pin) const;
  uint32_t GetOutputPinMask(const std::string& deviceName, int pin) const;
  std::string GetPinName(const std::string& deviceName, bool isInput, int pin) const;

  // Error handling
  void AddNotification(const std::string& message, bool isError = true);
  void UpdateNotifications(float deltaTime);
  void ShowErrorTooltip(EziIOError error);
  std::string GetErrorString(EziIOError error) const;
};