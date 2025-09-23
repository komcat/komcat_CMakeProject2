// MockPowerSupplyDevice.cpp
#include "MockPowerSupplyDevice.h"
#include <chrono>
#include <cmath>

MockPowerSupplyDevice::MockPowerSupplyDevice(const std::string& name, int id, const std::string& model)
  : rng(std::chrono::steady_clock::now().time_since_epoch().count()),
  noiseDist(-1.0f, 1.0f) {

  deviceInfo.name = name;
  deviceInfo.id = id;
  deviceInfo.model = model;
  deviceInfo.serialNumber = "SN" + std::to_string(id * 1000 + 123);
  deviceInfo.firmwareVersion = "1.0.0-mock";

  // Initialize 4 channels for testing
  for (int i = 1; i <= 4; ++i) {
    channels[i] = ChannelState();
  }
}

MockPowerSupplyDevice::~MockPowerSupplyDevice() {
  // Stop any running sweep first
  if (sweepRunning) {
    sweepRunning = false;  // Signal to stop
  }

  // Then join the thread
  if (sweepThread.joinable()) {
    sweepThread.join();
  }

  // Finally disconnect
  if (connected) {
    Disconnect();
  }
}


IPowerSupplyDevice::DeviceInfo MockPowerSupplyDevice::GetDeviceInfo() const {
  return deviceInfo;
}

bool MockPowerSupplyDevice::Connect() {
  // Simulate connection delay
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  connected = true;
  return true;
}

bool MockPowerSupplyDevice::Disconnect() {
  if (sweepRunning) {
    StopSweep();
  }

  // Turn off all channels
  for (auto& [ch, state] : channels) {
    state.outputOn = false;
  }

  connected = false;
  return true;
}

bool MockPowerSupplyDevice::IsConnected() const {
  return connected;
}

bool MockPowerSupplyDevice::SetVoltage(float voltage, int channel) {
  if (!connected) return false;

  std::lock_guard<std::mutex> lock(channelMutex);
  if (channels.find(channel) == channels.end()) return false;

  channels[channel].setVoltage = voltage;
  SimulateOutput(channel);
  return true;
}

bool MockPowerSupplyDevice::SetCurrent(float current, int channel) {
  if (!connected) return false;

  std::lock_guard<std::mutex> lock(channelMutex);
  if (channels.find(channel) == channels.end()) return false;

  channels[channel].setCurrent = current;
  SimulateOutput(channel);
  return true;
}

bool MockPowerSupplyDevice::TurnOn(int channel) {
  if (!connected) return false;

  std::lock_guard<std::mutex> lock(channelMutex);
  if (channels.find(channel) == channels.end()) return false;

  channels[channel].outputOn = true;
  SimulateOutput(channel);
  return true;
}

bool MockPowerSupplyDevice::TurnOff(int channel) {
  if (!connected) return false;

  std::lock_guard<std::mutex> lock(channelMutex);
  if (channels.find(channel) == channels.end()) return false;

  channels[channel].outputOn = false;
  channels[channel].actualVoltage = 0.0f;
  channels[channel].actualCurrent = 0.0f;
  return true;
}

void MockPowerSupplyDevice::SimulateOutput(int channel) {
  auto& state = channels[channel];

  if (!state.outputOn) {
    state.actualVoltage = 0.0f;
    state.actualCurrent = 0.0f;
    return;
  }

  if (state.constantVoltageMode) {
    // CV mode: voltage is controlled, current depends on "load"
    state.actualVoltage = AddNoise(state.setVoltage);
    // Simulate load: I = V/R, assume R = 10 ohms for simulation
    state.actualCurrent = AddNoise(state.actualVoltage / 10.0f);

    // Limit to set current
    if (state.actualCurrent > state.setCurrent) {
      state.actualCurrent = state.setCurrent;
      // In current limit, voltage would drop
      state.actualVoltage = AddNoise(state.actualCurrent * 10.0f);
    }
  }
  else {
    // CC mode: current is controlled, voltage depends on "load"
    state.actualCurrent = AddNoise(state.setCurrent);
    // V = I*R, assume R = 10 ohms
    state.actualVoltage = AddNoise(state.actualCurrent * 10.0f);

    // Limit to set voltage
    if (state.actualVoltage > state.setVoltage) {
      state.actualVoltage = state.setVoltage;
      state.actualCurrent = AddNoise(state.actualVoltage / 10.0f);
    }
  }
}

float MockPowerSupplyDevice::AddNoise(float value) {
  if (!addNoise) return value;

  float noise = noiseDist(rng) * noiseLevel * value;
  return value + noise;
}

float MockPowerSupplyDevice::ReadVoltage(int channel) const {
  if (!connected) return 0.0f;

  std::lock_guard<std::mutex> lock(channelMutex);
  auto it = channels.find(channel);
  if (it == channels.end()) return 0.0f;

  return it->second.actualVoltage;
}

float MockPowerSupplyDevice::ReadCurrent(int channel) const {
  if (!connected) return 0.0f;

  std::lock_guard<std::mutex> lock(channelMutex);
  auto it = channels.find(channel);
  if (it == channels.end()) return 0.0f;

  return it->second.actualCurrent;
}

IPowerSupplyDevice::Measurement MockPowerSupplyDevice::ReadVoltageCurrent(int channel) const {
  if (!connected) return { 0.0f, 0.0f };

  std::lock_guard<std::mutex> lock(channelMutex);
  auto it = channels.find(channel);
  if (it == channels.end()) return { 0.0f, 0.0f };

  return { it->second.actualVoltage, it->second.actualCurrent };
}

bool MockPowerSupplyDevice::StartSweep(const SweepConfig& config) {
  if (!connected) return false;
  if (sweepRunning) return false;

  // IMPORTANT: Clean up any previous thread before starting a new one
  if (sweepThread.joinable()) {
    sweepThread.join();  // Wait for previous thread to complete
  }

  sweepRunning = true;
  sweepProgress = 0.0f;

  // Now safe to start new thread
  sweepThread = std::thread(&MockPowerSupplyDevice::RunSweepInternal, this, config);

  return true;
}



void MockPowerSupplyDevice::RunSweepInternal(const SweepConfig& config) {
  std::lock_guard<std::mutex> lock(sweepMutex);

  currentSweepResult.config = config;
  currentSweepResult.measurements.clear();
  currentSweepResult.sweepValues.clear();
  currentSweepResult.completed = false;
  currentSweepResult.errorMessage.clear();

  // Calculate number of steps
  int numSteps = static_cast<int>((config.endValue - config.startValue) / config.stepSize) + 1;

  for (int step = 0; step < numSteps && sweepRunning; ++step) {
    float sweepValue = config.startValue + step * config.stepSize;

    // Set the sweep value
    if (config.mode == SweepConfig::Mode::CONSTANT_VOLTAGE) {
      SetVoltage(sweepValue, config.channel);
    }
    else {
      SetCurrent(sweepValue, config.channel);
    }

    // Wait for settling
    std::this_thread::sleep_for(std::chrono::milliseconds(config.delayMs));

    // Read measurement
    auto measurement = ReadVoltageCurrent(config.channel);

    currentSweepResult.sweepValues.push_back(sweepValue);
    currentSweepResult.measurements.push_back(measurement);

    // Update progress
    sweepProgress = static_cast<float>(step + 1) / numSteps;
  }

  currentSweepResult.completed = sweepRunning;  // Only complete if not stopped
  sweepRunning = false;
  sweepProgress = 1.0f;
}

bool MockPowerSupplyDevice::StopSweep() {
  if (!sweepRunning) return false;

  sweepRunning = false;  // Signal thread to stop

  if (sweepThread.joinable()) {
    sweepThread.join();  // Wait for thread to finish
  }

  return true;
}


IPowerSupplyDevice::SweepResult MockPowerSupplyDevice::GetSweepResults() const {
  std::lock_guard<std::mutex> lock(sweepMutex);
  return currentSweepResult;
}

IPowerSupplyDevice::SweepResult MockPowerSupplyDevice::ExecuteSweepBlocking(const SweepConfig& config) {
  if (!connected) {
    SweepResult result;
    result.completed = false;
    result.errorMessage = "Device not connected";
    return result;
  }

  StartSweep(config);

  // Wait for completion
  while (sweepRunning) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  return GetSweepResults();
}

bool MockPowerSupplyDevice::IsSweepRunning() const {
  return sweepRunning;
}

float MockPowerSupplyDevice::GetSweepProgress() const {
  return sweepProgress;
}

bool MockPowerSupplyDevice::SetModeConstantVoltage(int channel) {
  if (!connected) return false;

  std::lock_guard<std::mutex> lock(channelMutex);
  if (channels.find(channel) == channels.end()) return false;

  channels[channel].constantVoltageMode = true;
  return true;
}

bool MockPowerSupplyDevice::SetModeConstantCurrent(int channel) {
  if (!connected) return false;

  std::lock_guard<std::mutex> lock(channelMutex);
  if (channels.find(channel) == channels.end()) return false;

  channels[channel].constantVoltageMode = false;
  return true;
}

void MockPowerSupplyDevice::SetMaxChannels(int maxChannels) {
  std::lock_guard<std::mutex> lock(channelMutex);

  // Clear existing channels
  channels.clear();

  // Initialize new channels up to maxChannels
  for (int i = 1; i <= maxChannels; ++i) {
    channels[i] = ChannelState();
  }
}