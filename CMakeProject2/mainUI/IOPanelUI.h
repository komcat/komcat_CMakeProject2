// IOPanelUI.h - UI panel for managing EziIO devices with Observer Pattern
#pragma once

// Prevent winsock conflicts before any includes
#ifdef _WIN32
#define _WINSOCKAPI_   // Prevent winsock.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif



#include <memory>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include "include/eziio/EziIO_Observer.h"

// Forward declarations
class EziIOManager;
class IOConfigManager;
enum class EziIOError;

class IOPanelUI : public IEziIOObserver {
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

  // Observer pattern implementation
  void onPinStateChanged(const PinChangeEvent& event) override;

  // Subscribe/unsubscribe to IO events
  void SubscribeToIOEvents();
  void UnsubscribeFromIOEvents();

  // Enable/disable live updates from observer
  void SetLiveUpdatesEnabled(bool enabled) { m_liveUpdatesEnabled = enabled; }
  bool IsLiveUpdatesEnabled() const { return m_liveUpdatesEnabled; }

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
  bool m_liveUpdatesEnabled = true;
  bool m_isSubscribed = false;

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

    // Pin change tracking
    std::chrono::steady_clock::time_point lastUpdateTime;
    std::vector<int> recentlyChangedInputPins;
    std::vector<int> recentlyChangedOutputPins;
  };
  std::vector<DeviceState> m_deviceStates;
  std::mutex m_deviceStatesMutex;  // Thread safety for observer updates

  // Error notification system
  struct ErrorNotification {
    std::string message;
    float timeRemaining;
    bool isError;
  };
  std::vector<ErrorNotification> m_errorNotifications;

  // Pin change history for visualization
  struct PinChangeHistory {
    std::string deviceName;
    int pinNumber;
    bool isInput;
    bool newState;
    std::chrono::steady_clock::time_point timestamp;
  };
  std::vector<PinChangeHistory> m_pinChangeHistory;
  size_t m_maxHistorySize = 100;

  // Statistics
  int m_totalConnectedDevices = 0;
  int m_totalOperationErrors = 0;
  int m_refreshCount = 0;
  int m_totalPinChanges = 0;
  std::map<std::string, int> m_deviceChangeCount;

  // Panel rendering methods
  void RenderLeftPanel();   // List of IO devices
  void RenderRightPanel();  // Selected device interface

  // Helper methods
  void RenderDeviceList();
  void RenderSelectedDeviceUI();
  void RenderNoSelectionMessage();
  void RenderErrorNotifications();
  void RenderPinChangeHistory();
  void RenderLiveUpdateIndicator();
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
  void HighlightRecentChange(const DeviceState& device, int pin, bool isInput) const;

  // Error handling
  void AddNotification(const std::string& message, bool isError = true);
  void UpdateNotifications(float deltaTime);
  void ShowErrorTooltip(EziIOError error);
  std::string GetErrorString(EziIOError error) const;

  // Thread-safe state update from observer
  void UpdateDeviceStateFromEvent(const PinChangeEvent& event);
  void AddToPinHistory(const PinChangeEvent& event);

  void RefreshSingleDevice(DeviceState& device);
};