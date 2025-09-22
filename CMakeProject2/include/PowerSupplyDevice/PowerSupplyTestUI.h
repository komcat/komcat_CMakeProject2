// PowerSupplyTestUI.h
#ifndef POWERSUPPLYTESTUI_H
#define POWERSUPPLYTESTUI_H

#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include "imgui.h"
#include "MenuManager_uaa3.h"
#include "PowerSupplyDevice/PowerSupplyManager.h"
#include "PowerSupplyDevice/MockPowerSupplyDevice.h"
#include "FileResultStorage.h"

class PowerSupplyTestUI : public IImguiUI {  // Inherit from IImguiUI
public:
  PowerSupplyTestUI();
  ~PowerSupplyTestUI() override;

  // IImguiUI interface implementation
  void Render() override;
  void Show() override { m_showWindow = true; }
  void Hide() override { m_showWindow = false; }
  bool IsVisible() const override { return m_showWindow; }
  const std::string& GetName() const override { return m_windowName; }
  void Toggle() override { m_showWindow = !m_showWindow; }

private:
  // Window name
  std::string m_windowName = "Power Supply Test System";

  // Core components
  std::shared_ptr<PowerSupplyManager> m_manager;
  std::shared_ptr<FileResultStorage> m_storage;
  bool m_showWindow = false;

  // UI State
  struct UIState {
    char deviceId[64] = "PS1";
    float voltage = 5.0f;
    float current = 1.0f;
    int channel = 1;
    bool autoRefresh = false;

    // Sweep parameters
    struct {
      float startValue = 0.0f;
      float endValue = 5.0f;
      float stepSize = 0.5f;
      int delayMs = 100;
      int mode = 0; // 0 = CV, 1 = CC
      bool running = false;
      float progress = 0.0f;
    } sweep;

    // Display
    struct {
      IPowerSupplyDevice::Measurement lastMeasurement;
      std::chrono::steady_clock::time_point lastUpdate;
      std::vector<std::string> logMessages;
      bool showStorageViewer = false;
      bool showSweepResults = false;
    } display;
  } m_state;

  // Device tracking
  int m_nextDeviceNum = 1;
  std::vector<std::string> m_deviceIds;
  IPowerSupplyDevice::SweepResult m_lastSweepResult;

  // Helper methods
  void RenderDeviceManagement();
  void RenderDeviceControl();
  void RenderMeasurementDisplay();
  void RenderSweepControl();
  void RenderStatusDisplay();
  void RenderStorageViewer();
  void RenderSweepResultsWindow();

  void AddLogMessage(const std::string& msg);
  void RefreshDeviceList();
  void UpdateMeasurement(const std::string& deviceId);
  void StartSweep();
  void UpdateSweepProgress();
};

#endif // POWERSUPPLYTESTUI_H