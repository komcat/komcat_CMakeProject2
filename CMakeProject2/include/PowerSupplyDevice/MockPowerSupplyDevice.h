// MockPowerSupplyDevice.h
#ifndef MOCKPOWERSUPPLYDEVICE_H
#define MOCKPOWERSUPPLYDEVICE_H

#include "PowerSupplyDevice/IPowerSupplyDevice.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <random>

class MockPowerSupplyDevice : public IPowerSupplyDevice {
private:
  // Device info
  DeviceInfo deviceInfo;

  // Connection state
  std::atomic<bool> connected{ false };

  // Channel states (support up to 4 channels for testing)
  struct ChannelState {
    float setVoltage = 0.0f;
    float setCurrent = 0.0f;
    bool outputOn = false;
    bool constantVoltageMode = true;  // true = CV, false = CC
    float actualVoltage = 0.0f;
    float actualCurrent = 0.0f;
  };
  std::map<int, ChannelState> channels;
  mutable std::mutex channelMutex;

  // Sweep state
  std::atomic<bool> sweepRunning{ false };
  std::atomic<float> sweepProgress{ 0.0f };
  SweepResult currentSweepResult;
  std::thread sweepThread;
  mutable std::mutex sweepMutex;

  // Simulation parameters
  bool addNoise = true;
  float noiseLevel = 0.01f;  // 1% noise
  std::mt19937 rng;
  std::uniform_real_distribution<float> noiseDist;

  // Helper methods
  void SimulateOutput(int channel);
  float AddNoise(float value);
  void RunSweepInternal(const SweepConfig& config);

public:
  MockPowerSupplyDevice(const std::string& name = "MockPS",
    int id = 1,
    const std::string& model = "MOCK-1000");
  virtual ~MockPowerSupplyDevice();

  // Configuration methods for testing
  void SetNoiseEnabled(bool enable) { addNoise = enable; }
  void SetNoiseLevel(float level) { noiseLevel = level; }
  void SimulateConnectionFailure(bool fail);

  // IPowerSupplyDevice implementation
  DeviceInfo GetDeviceInfo() const override;

  bool Connect() override;
  bool Disconnect() override;
  bool IsConnected() const override;

  bool SetVoltage(float voltage, int channel = 1) override;
  bool SetCurrent(float current, int channel = 1) override;
  bool TurnOn(int channel = 1) override;
  bool TurnOff(int channel = 1) override;
  bool SetModeConstantVoltage(int channel = 1) override;
  bool SetModeConstantCurrent(int channel = 1) override;

  float ReadVoltage(int channel = 1) const override;
  float ReadCurrent(int channel = 1) const override;
  Measurement ReadVoltageCurrent(int channel = 1) const override;

  bool StartSweep(const SweepConfig& config) override;
  bool StopSweep() override;
  bool IsSweepRunning() const override;
  float GetSweepProgress() const override;
  SweepResult GetSweepResults() const override;
  SweepResult ExecuteSweepBlocking(const SweepConfig& config) override;

  // Add these new methods:
  void SetDeviceName(const std::string& name) { deviceInfo.name = name; }
  void SetMaxChannels(int maxChannels);
};

#endif // MOCKPOWERSUPPLYDEVICE_H