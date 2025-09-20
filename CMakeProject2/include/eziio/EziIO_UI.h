#pragma once

#include "EziIO_Manager.h"
#include "IOConfigManager.h"
#include "imgui.h"
#include <string>
#include <memory>
#include <vector>
#include <functional>

class EziIO_UI {
public:
  EziIO_UI(EziIOManager& manager);
  ~EziIO_UI();

  void setConfigManager(IOConfigManager* configManager) {
    m_configManager = configManager;
  }

  void RenderUI();

  void SetInputChangeCallback(std::function<void(const std::string&, int, bool)> callback);
  void SetOutputChangeCallback(std::function<void(const std::string&, int, bool)> callback);

  bool IsVisible() const { return m_showWindow; }
  void ToggleWindow() { m_showWindow = !m_showWindow; }

private:
  EziIOManager& m_ioManager;
  IOConfigManager* m_configManager = nullptr;

  // UI state
  bool m_showWindow;
  bool m_autoRefresh;
  float m_refreshInterval;
  float m_refreshTimer;
  bool m_showDebugInfo;

  // Cached state for UI
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
    EziIOError lastError = EziIOError::SUCCESS;  // Add error tracking
  };
  std::vector<DeviceState> m_deviceStates;

  // Callbacks
  std::function<void(const std::string&, int, bool)> m_inputChangeCallback;
  std::function<void(const std::string&, int, bool)> m_outputChangeCallback;

  // Helper methods
  void RefreshDeviceStates();
  void RenderDevicePanel(DeviceState& device);
  void RenderInputPins(DeviceState& device);
  void RenderOutputPins(DeviceState& device);
  bool IsPinOn(uint32_t value, int pin) const;
  uint32_t GetOutputPinMask(const std::string& deviceName, int pin) const;
  std::string GetPinName(const std::string& deviceName, bool isInput, int pin) const;

  // New helper for error display
  void ShowErrorTooltip(EziIOError error);
  void HandleOutputControl(DeviceState& device, int pin, bool state);
};