// KeysightE36103B.h
#ifndef KEYSIGHT_E36103B_H
#define KEYSIGHT_E36103B_H

#include "../include/PowerSupplyDevice/IPowerSupplyDevice.h"
#include <visa.h>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <string>

class KeysightE36103B : public IPowerSupplyDevice {
public:
  // Constructor with VISA resource string
  explicit KeysightE36103B(const std::string& resource_string = "");
  virtual ~KeysightE36103B();

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

  // Additional Keysight-specific methods
  void SetResourceString(const std::string& resource);
  std::string GetResourceString() const;
  bool IsOutputOn() const;
  bool SetOVP(float voltage);  // Overvoltage protection
  bool SetOCP(float current);  // Overcurrent protection
  bool ClearProtection();      // Clear protection trip
  std::string GetErrorString() const;
  void SetRemoteSense(bool enable);

  // Debug mode
  void SetDebugMode(bool enable) { m_debugMode = enable; }

private:
  // VISA communication members
  ViSession m_defaultRM;  // Resource manager
  ViSession m_instr;      // Instrument session
  std::string m_resourceString;
  mutable std::mutex m_visaMutex;  // Protect VISA calls
  bool m_connected;
  bool m_debugMode;

  // Device info cache
  mutable DeviceInfo m_deviceInfo;
  mutable bool m_deviceInfoCached;

  // Sweep management
  mutable std::mutex m_sweepMutex;
  std::atomic<bool> m_sweepRunning;
  std::atomic<bool> m_sweepStopRequested;
  std::atomic<float> m_sweepProgress;
  std::unique_ptr<std::thread> m_sweepThread;
  SweepResult m_currentSweepResult;

  // Helper methods
  void ParseDeviceInfo(const std::string& idn);
  void RunSweepThread(const SweepConfig& config);
  std::string SendQuery(const std::string& cmd) const;
  bool SendCommand(const std::string& cmd) const;
  bool CheckError() const;

  // E36103B specifications from datasheet
  static constexpr float MAX_VOLTAGE = 20.0f;
  static constexpr float MAX_CURRENT = 2.0f;
  static constexpr float MAX_POWER = 40.0f;
  static constexpr float VOLTAGE_RESOLUTION = 0.001f;  // 1mV
  static constexpr float CURRENT_RESOLUTION = 0.001f;  // 1mA
  static constexpr float OVP_MIN = 0.0f;
  static constexpr float OVP_MAX = 22.0f;  // 110% of max voltage

  // VISA timeout in ms
  static constexpr int VISA_TIMEOUT = 5000;
};

#endif // KEYSIGHT_E36103B_H