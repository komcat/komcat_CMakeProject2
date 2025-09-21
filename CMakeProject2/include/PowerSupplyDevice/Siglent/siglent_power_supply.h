// siglent_power_supply.h
#ifndef SIGLENT_POWER_SUPPLY_H
#define SIGLENT_POWER_SUPPLY_H

#include "../include/PowerSupplyDevice/IPowerSupplyDevice.h"
#include "../include/PowerSupply/SPDPowerSupply.h"
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <map>

class SiglentPowerSupply : public IPowerSupplyDevice {
public:
  // Constructor with optional VISA resource string
  explicit SiglentPowerSupply(const std::string& resource_string = "");

  // Destructor
  virtual ~SiglentPowerSupply();

  // IPowerSupplyDevice interface implementation
  virtual DeviceInfo GetDeviceInfo() const override;
  virtual bool Connect() override;
  virtual bool Disconnect() override;
  virtual bool IsConnected() const override;

  // Control functions
  virtual bool SetVoltage(float voltage, int channel = 1) override;
  virtual bool SetCurrent(float current, int channel = 1) override;
  virtual bool TurnOn(int channel = 1) override;
  virtual bool TurnOff(int channel = 1) override;
  virtual bool SetModeConstantVoltage(int channel = 1) override;
  virtual bool SetModeConstantCurrent(int channel = 1) override;

  // Read functions
  virtual float ReadVoltage(int channel = 1) const override;
  virtual float ReadCurrent(int channel = 1) const override;
  virtual Measurement ReadVoltageCurrent(int channel = 1) const override;

  // Sweep functions
  virtual bool StartSweep(const SweepConfig& config) override;
  virtual bool StopSweep() override;
  virtual bool IsSweepRunning() const override;
  virtual float GetSweepProgress() const override;
  virtual SweepResult GetSweepResults() const override;
  virtual SweepResult ExecuteSweepBlocking(const SweepConfig& config) override;

  // Additional methods specific to Siglent
  void SetResourceString(const std::string& resource);
  std::string GetResourceString() const;
  void SetDebugMode(bool enable);

private:
  // The actual SPD power supply implementation
  std::unique_ptr<PowerSupply::SPDPowerSupply> m_spd;

  // Device info cache
  mutable DeviceInfo m_deviceInfo;
  mutable bool m_deviceInfoCached;

  // Resource string
  std::string m_resourceString;

  // Sweep management
  mutable std::mutex m_sweepMutex;
  std::atomic<bool> m_sweepRunning;
  std::atomic<bool> m_sweepStopRequested;
  std::unique_ptr<std::thread> m_sweepThread;

  // Current sweep configuration and results
  SweepConfig m_currentSweepConfig;
  SweepResult m_currentSweepResult;
  std::atomic<float> m_sweepProgress;

  // Helper methods
  void ParseDeviceInfo(const std::string& idn);
  void RunSweepThread(const SweepConfig& config);

  // Store last mode for each channel (for SetMode functions)
  mutable std::map<int, float> m_lastVoltageSettings;
  mutable std::map<int, float> m_lastCurrentSettings;
};

#endif // SIGLENT_POWER_SUPPLY_H