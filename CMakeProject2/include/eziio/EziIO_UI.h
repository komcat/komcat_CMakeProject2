#pragma once

#include "EziIO_Manager.h"
#include "IOConfigManager.h" // Add this
#include "imgui.h"
#include <string>
#include <memory>
#include <vector>
#include <functional>

class EziIO_UI {
public:
  EziIO_UI(EziIOManager& manager);
  ~EziIO_UI();
  // Add this method to set the config manager
  void setConfigManager(IOConfigManager* configManager) {
    m_configManager = configManager;
  }
  // Render the ImGui UI window
  void RenderUI();

  // Set callback for input pin change events
  void SetInputChangeCallback(std::function<void(const std::string&, int, bool)> callback);

  // Set callback for output pin change events requested through UI
  void SetOutputChangeCallback(std::function<void(const std::string&, int, bool)> callback);

private:
  // Reference to the EziIO manager
  EziIOManager& m_ioManager;
  // Add this for configuration
  IOConfigManager* m_configManager = nullptr;
  // UI state
  bool m_showWindow;
  bool m_autoRefresh;
  float m_refreshInterval;  // in seconds
  float m_refreshTimer;
  bool m_showDebugInfo;     // Option to show debug information

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
  };
  std::vector<DeviceState> m_deviceStates;

  // Callbacks for pin changes
  std::function<void(const std::string&, int, bool)> m_inputChangeCallback;
  std::function<void(const std::string&, int, bool)> m_outputChangeCallback;

  // Helper methods
  void RefreshDeviceStates();
  void RenderDevicePanel(DeviceState& device);
  void RenderInputPins(DeviceState& device);
  void RenderOutputPins(DeviceState& device);
  bool IsPinOn(uint32_t value, int pin) const;
  uint32_t GetOutputPinMask(const std::string& deviceName, int pin) const;

  // Add this method to get pin name from config
  std::string GetPinName(const std::string& deviceName, bool isInput, int pin) const;

};