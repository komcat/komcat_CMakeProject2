
#pragma once
#include "IDataProvider.h"
#include "SPDPowerSupplyManager.h"

class SPDDataProviderAdapter : public IDataProvider {
private:
  SPDPowerSupplyManager* m_spdManager;
  std::string m_providerName;

public:
  explicit SPDDataProviderAdapter(SPDPowerSupplyManager* manager, const std::string& name = "SPDPowerSupply")
    : m_spdManager(manager), m_providerName(name) {
  }

  std::string GetProviderName() const override {
    return m_providerName;
  }

  bool StartDataCollection(int intervalMs) override {
    if (m_spdManager) {
      m_spdManager->StartAllPolling(intervalMs);
      return true;
    }
    return false;
  }

  void StopDataCollection() override {
    if (m_spdManager) {
      m_spdManager->StopAllPolling();
    }
  }

  bool IsDataCollectionActive() const override {
    return m_spdManager ? m_spdManager->IsPollingActive() : false;
  }

  void SetDataUpdateCallback(std::function<void(const std::string&, const std::string&)> callback) override {
    if (m_spdManager) {
      m_spdManager->SetStatusUpdateCallback(callback);
    }
  }

  std::vector<std::string> GetDeviceNames() const override {
    return m_spdManager ? m_spdManager->GetDeviceNames() : std::vector<std::string>{};
  }

  std::map<std::string, std::string> GetChannelSuffixes() const override {
    return {
        {"Voltage", "Output voltage in volts"},
        {"Current", "Output current in amperes"},
        {"Output", "Output state (1=ON, 0=OFF)"},
        {"Power", "Calculated power in watts (V*I)"}
    };
  }
};