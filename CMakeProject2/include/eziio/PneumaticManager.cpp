#include "PneumaticManager.h"
#include <iostream>
#include <thread>
#include <chrono>

PneumaticManager::PneumaticManager(EziIOManager& ioManager)
  : m_ioManager(ioManager),
  m_stopPolling(false),
  m_pollingInterval(50) // Default 50ms polling interval
{
  std::cout << "PneumaticManager initialized" << std::endl;
}

PneumaticManager::~PneumaticManager()
{
  // Stop polling thread if active
  stopPolling();

  // Clear all slides
  m_slides.clear();
}

bool PneumaticManager::loadConfiguration(IOConfigManager* configManager)
{
  if (!configManager) {
    std::cerr << "Error: IOConfigManager is null" << std::endl;
    return false;
  }

  // Clear existing configuration
  m_slides.clear();
  m_deviceIdMap.clear();
  m_inputPinMap.clear();
  m_outputPinMap.clear();

  // Load device ID mappings
  const auto& eziioDevices = configManager->getEziIODevices();
  for (const auto& device : eziioDevices) {
    m_deviceIdMap[device.name] = device.deviceId;

    // Load input pin mappings
    for (const auto& input : device.ioConfig.inputs) {
      m_inputPinMap[device.name][input.name] = input.pin;
    }

    // Load output pin mappings
    for (const auto& output : device.ioConfig.outputs) {
      m_outputPinMap[device.name][output.name] = output.pin;
    }
  }

  // Load pneumatic slides
  const auto& pneumaticSlides = configManager->getPneumaticSlides();
  for (const auto& slideConfig : pneumaticSlides) {
    // Create slide configuration objects
    IOPinConfig outputConfig = {
        slideConfig.output.deviceName,
        slideConfig.output.pinName
    };

    IOPinConfig extendedInputConfig = {
        slideConfig.extendedInput.deviceName,
        slideConfig.extendedInput.pinName
    };

    IOPinConfig retractedInputConfig = {
        slideConfig.retractedInput.deviceName,
        slideConfig.retractedInput.pinName
    };

    // Resolve the device IDs and pin numbers
    if (!resolvePinConfig(outputConfig) ||
      !resolvePinConfig(extendedInputConfig) ||
      !resolvePinConfig(retractedInputConfig)) {
      std::cerr << "Failed to resolve pin configuration for slide: " << slideConfig.name << std::endl;
      continue;
    }

    // Create the pneumatic slide object
    auto slide = std::make_shared<PneumaticSlide>(
      slideConfig.name,
      outputConfig,
      extendedInputConfig,
      retractedInputConfig,
      slideConfig.timeoutMs
    );

    // Set up state change callback
    slide->setStateChangeCallback([this](const std::string& name, SlideState state) {
      if (m_stateChangeCallback) {
        m_stateChangeCallback(name, state);
      }
    });

    // Add to our collection
    m_slides[slideConfig.name] = slide;

    std::cout << "Loaded pneumatic slide: " << slideConfig.name << std::endl;
  }

  std::cout << "Loaded " << m_slides.size() << " pneumatic slides" << std::endl;
  return !m_slides.empty();
}

bool PneumaticManager::initialize()
{
  // Update initial state of all slides
  updateAllSlideStates();
  return true;
}

bool PneumaticManager::extendSlide(const std::string& slideName)
{
  auto slide = getSlide(slideName);
  if (!slide) {
    std::cerr << "Cannot find slide: " << slideName << std::endl;
    return false;
  }

  // Set the output pin to extend the slide (typically ON)
  bool result = setOutputPin(slide->getOutputConfig(), true);
  if (result) {
    // Update the slide's internal state
    slide->extend();
  }

  return result;
}

bool PneumaticManager::retractSlide(const std::string& slideName)
{
  auto slide = getSlide(slideName);
  if (!slide) {
    std::cerr << "Cannot find slide: " << slideName << std::endl;
    return false;
  }

  // Set the output pin to retract the slide (typically OFF)
  bool result = setOutputPin(slide->getOutputConfig(), false);
  if (result) {
    // Update the slide's internal state
    slide->retract();
  }

  return result;
}

SlideState PneumaticManager::getSlideState(const std::string& slideName) const
{
  auto slide = getSlide(slideName);
  if (!slide) {
    std::cerr << "Cannot find slide: " << slideName << std::endl;
    return SlideState::P_ERROR;
  }

  return slide->getState();
}

std::shared_ptr<PneumaticSlide> PneumaticManager::getSlide(const std::string& slideName) const
{
  auto it = m_slides.find(slideName);
  if (it != m_slides.end()) {
    return it->second;
  }
  return nullptr;
}

std::vector<std::string> PneumaticManager::getSlideNames() const
{
  std::vector<std::string> names;
  for (const auto& pair : m_slides) {
    names.push_back(pair.first);
  }
  return names;
}

void PneumaticManager::updateAllSlideStates()
{
  std::lock_guard<std::mutex> lock(m_slidesMutex);

  for (auto& [name, slide] : m_slides) {
    // Read the current state of the extended and retracted sensors
    bool extendedSensor = readInputPin(slide->getExtendedInputConfig());
    bool retractedSensor = readInputPin(slide->getRetractedInputConfig());

    // Update the slide's state based on sensor readings
    slide->updateState(extendedSensor, retractedSensor);
  }
}

void PneumaticManager::setStateChangeCallback(std::function<void(const std::string&, SlideState)> callback)
{
  m_stateChangeCallback = callback;

  // Update all slide callbacks
  for (auto& [name, slide] : m_slides) {
    slide->setStateChangeCallback(m_stateChangeCallback);
  }
}

void PneumaticManager::resetAllSlides()
{
  for (auto& [name, slide] : m_slides) {
    slide->resetState();
  }
}

bool PneumaticManager::resolvePinConfig(IOPinConfig& config)
{
  // Look up device ID
  auto deviceIt = m_deviceIdMap.find(config.deviceName);
  if (deviceIt == m_deviceIdMap.end()) {
    std::cerr << "Unknown device name: " << config.deviceName << std::endl;
    return false;
  }
  config.deviceId = deviceIt->second;

  // Look up pin number (check both input and output maps)
  auto inputDeviceIt = m_inputPinMap.find(config.deviceName);
  if (inputDeviceIt != m_inputPinMap.end()) {
    auto pinIt = inputDeviceIt->second.find(config.pinName);
    if (pinIt != inputDeviceIt->second.end()) {
      config.pinNumber = pinIt->second;
      return true;
    }
  }

  auto outputDeviceIt = m_outputPinMap.find(config.deviceName);
  if (outputDeviceIt != m_outputPinMap.end()) {
    auto pinIt = outputDeviceIt->second.find(config.pinName);
    if (pinIt != outputDeviceIt->second.end()) {
      config.pinNumber = pinIt->second;
      return true;
    }
  }

  std::cerr << "Unknown pin name: " << config.pinName << " for device: " << config.deviceName << std::endl;
  return false;
}

bool PneumaticManager::readInputPin(const IOPinConfig& config) const
{
  uint32_t inputs = 0, latch = 0;

  // Read inputs from the device
  bool success = m_ioManager.getLastInputStatus(config.deviceId, inputs, latch);
  if (!success) {
    std::cerr << "Failed to read inputs from device ID: " << config.deviceId << std::endl;
    return false;
  }

  // Check if the specific pin is high
  return (inputs & (1 << config.pinNumber)) != 0;
}

bool PneumaticManager::setOutputPin(const IOPinConfig& config, bool state) const
{
  // Use the EziIO manager to set the output pin
  return m_ioManager.setOutput(config.deviceId, config.pinNumber, state);
}

void PneumaticManager::startPolling(unsigned int intervalMs)
{
  // Don't start if already running
  if (m_pollingThread) {
    return;
  }

  // Set the polling interval
  m_pollingInterval = intervalMs;

  // Reset the stop flag
  m_stopPolling = false;

  // Start the polling thread
  m_pollingThread = new std::thread(&PneumaticManager::pollingThreadFunc, this);

  std::cout << "PneumaticManager polling thread started with interval "
    << m_pollingInterval << "ms" << std::endl;
}

void PneumaticManager::stopPolling()
{
  // Signal the thread to stop
  m_stopPolling = true;

  // Wait for the thread to exit
  if (m_pollingThread) {
    if (m_pollingThread->joinable()) {
      m_pollingThread->join();
    }
    delete m_pollingThread;
    m_pollingThread = nullptr;
  }
}

bool PneumaticManager::isPolling() const
{
  return m_pollingThread != nullptr && !m_stopPolling;
}

void PneumaticManager::pollingThreadFunc()
{
  std::cout << "Pneumatic polling thread started" << std::endl;

  while (!m_stopPolling) {
    // Update all slide states
    updateAllSlideStates();

    // Sleep for the specified interval
    std::this_thread::sleep_for(std::chrono::milliseconds(m_pollingInterval));
  }

  std::cout << "Pneumatic polling thread stopped" << std::endl;
}