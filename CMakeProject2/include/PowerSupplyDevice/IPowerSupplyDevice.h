// IPowerSupplyDevice.h
#ifndef IPOWERSUPPLYDEVICE_H
#define IPOWERSUPPLYDEVICE_H

#include <string>
#include <vector>

class IPowerSupplyDevice {
public:
  struct DeviceInfo {
    std::string name;
    int id;
    std::string model;
    std::string serialNumber;
    std::string firmwareVersion;
  };

  struct Measurement {
    float voltage;
    float current;
  };

  struct SweepConfig {
    enum class Mode {
      CONSTANT_VOLTAGE,  // Sweep voltage, measure current
      CONSTANT_CURRENT   // Sweep current, measure voltage
    };

    Mode mode;
    float startValue;
    float endValue;
    float stepSize;
    int delayMs;  // Delay between steps in milliseconds
    int channel = 1;
  };

  struct SweepResult {
    std::vector<Measurement> measurements;
    std::vector<float> sweepValues;  // The actual swept values (V or I)
    SweepConfig config;
    bool completed;
    std::string errorMessage;  // Empty if successful
  };

  virtual ~IPowerSupplyDevice() = default;

  // Device information
  virtual DeviceInfo GetDeviceInfo() const = 0;

  // Connection management
  virtual bool Connect() = 0;
  virtual bool Disconnect() = 0;
  virtual bool IsConnected() const = 0;

  // Control functions
  virtual bool SetVoltage(float voltage, int channel = 1) = 0;
  virtual bool SetCurrent(float current, int channel = 1) = 0;
  virtual bool TurnOn(int channel = 1) = 0;
  virtual bool TurnOff(int channel = 1) = 0;
  virtual bool SetModeConstantVoltage(int channel = 1) = 0;
  virtual bool SetModeConstantCurrent(int channel = 1) = 0;

  // Read functions
  virtual float ReadVoltage(int channel = 1) const = 0;
  virtual float ReadCurrent(int channel = 1) const = 0;
  virtual Measurement ReadVoltageCurrent(int channel = 1) const = 0;

  // Sweep functions - self-contained and thread-safe
  virtual bool StartSweep(const SweepConfig& config) = 0;
  virtual bool StopSweep() = 0;
  virtual bool IsSweepRunning() const = 0;
  virtual float GetSweepProgress() const = 0;  // 0.0 to 1.0
  virtual SweepResult GetSweepResults() const = 0;  // Returns copy of current results

  // Blocking sweep - runs and returns when complete
  virtual SweepResult ExecuteSweepBlocking(const SweepConfig& config) = 0;
};

#endif // IPOWERSUPPLYDEVICE_H