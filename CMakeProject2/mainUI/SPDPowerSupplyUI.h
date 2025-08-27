
#pragma once
#include "imgui.h"
#include "include/PowerSupply/SPDPowerSupplyManager.h"
#include "include/data/global_data_store.h"
#include "logger.h"

class SPDPowerSupplyUI {
private:
  SPDPowerSupplyManager* m_spdManager;

  // UI State
  bool m_outputEnabled = false;
  float m_cvVoltage = 5.0f;
  float m_cvCurrentLimit = 1.0f;
  float m_ccCurrent = 1.0f;
  float m_ccVoltageLimit = 10.0f;
  int m_pollingInterval = 500;

public:
  explicit SPDPowerSupplyUI(SPDPowerSupplyManager* spdManager = nullptr);
  ~SPDPowerSupplyUI() = default;

  // Core methods
  void SetSPDManager(SPDPowerSupplyManager* manager) { m_spdManager = manager; }
  void RenderUI();

private:
  // Rendering sections
  void RenderStatusSection();
  void RenderConnectionControls();
  void RenderOutputControl();
  void RenderModeControls();
  void RenderPollingControls();
  void RenderLiveDataDisplay();

  // Helper methods
  ImVec4 GetChannelColor(const std::string& channel, float value) const;
};