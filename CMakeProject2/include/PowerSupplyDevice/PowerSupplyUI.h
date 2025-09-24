// PowerSupplyUI.h
#ifndef POWERSUPPLYUI_H
#define POWERSUPPLYUI_H

#include "imgui.h"
#include "MenuManager_uaa3.h"
#include "PowerSupplyDevice/PowerSupplyManager.h"
#include <memory>
#include <vector>
#include <string>

class PowerSupplyUI : public IImguiUI {
public:
  PowerSupplyUI();
  ~PowerSupplyUI() override;

  // IImguiUI interface
  void Render() override;
  void Show() override { m_showWindow = true; }
  void Hide() override { m_showWindow = false; }
  bool IsVisible() const override { return m_showWindow; }
  const std::string& GetName() const override { return m_windowName; }
  void Toggle() override { m_showWindow = !m_showWindow; }

  // Initialize with config file
  bool Initialize(const std::string& configFile);

  // Set external PowerSupplyManager (instead of creating own)
  void SetPowerSupplyManager(PowerSupplyManager* manager);

private:
  // UI Components
  void RenderDeviceList();
  void RenderDeviceControl(const std::string& deviceId);
  void RenderQuickControls();

  // Core
  PowerSupplyManager* m_manager = nullptr;  // Non-owning pointer
  std::string m_windowName = "Power Supply Control";
  bool m_showWindow = false;

  // State
  std::string m_selectedDevice;
  std::vector<std::string> m_deviceIds;

  // Control values for each device
  struct DeviceControl {
    float voltage = 0.0f;
    float current = 0.1f;
    int channel = 1;
    bool outputOn = false;
  };
  std::map<std::string, DeviceControl> m_deviceControls;

  // Update timer
  float m_updateTimer = 0.0f;
  const float m_updateInterval = 1.0f; // Update every second

  // Helper methods
  void RefreshDeviceList();
  void UpdateDeviceStatus();
};

#endif // POWERSUPPLYUI_H