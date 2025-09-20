#include "PneumaticManager.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>

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
  std::lock_guard<std::mutex> lock(m_slidesMutex);
  m_slides.clear();
}

bool PneumaticManager::loadConfiguration(IOConfigManager* configManager)
{
  if (!configManager) {
    m_lastError = "IOConfigManager is null";
    std::cerr << "Error: " << m_lastError << std::endl;
    return false;
  }

  std::lock_guard<std::mutex> lock(m_slidesMutex);

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
  int loadedCount = 0;
  int failedCount = 0;

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
      failedCount++;
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
    loadedCount++;

    std::cout << "Loaded pneumatic slide: " << slideConfig.name << std::endl;
  }

  if (failedCount > 0) {
    std::cerr << "Warning: Failed to load " << failedCount << " slides" << std::endl;
  }

  std::cout << "Successfully loaded " << loadedCount << " pneumatic slides" << std::endl;
  return !m_slides.empty();
}

bool PneumaticManager::initialize()
{
  // Check if we have any slides configured
  if (m_slides.empty()) {
    m_lastError = "No slides configured";
    if (m_enableLogging) {
      std::cerr << "PneumaticManager: " << m_lastError << std::endl;
    }
    return false;
  }

  // Update initial state of all slides
  updateAllSlideStates();

  return true;
}

bool PneumaticManager::extendSlide(const std::string& slideName)
{
  auto slide = getSlide(slideName);
  if (!slide) {
    m_lastError = "Cannot find slide: " + slideName;
    m_operationErrorCount++;
    if (m_enableLogging) {
      std::cerr << m_lastError << std::endl;
    }
    return false;
  }

  // Set the output pin to extend the slide (typically ON)
  EziIOError result = setOutputPin(slide->getOutputConfig(), true);

  if (result == EziIOError::SUCCESS) {
    // Update the slide's internal state
    slide->extend();
    if (m_enableLogging) {
      std::cout << "Extended slide: " << slideName << std::endl;
    }
    return true;
  }
  else {
    m_operationErrorCount++;
    logError("extendSlide(" + slideName + ")", result);
    return false;
  }
}

bool PneumaticManager::retractSlide(const std::string& slideName)
{
  auto slide = getSlide(slideName);
  if (!slide) {
    m_lastError = "Cannot find slide: " + slideName;
    m_operationErrorCount++;
    if (m_enableLogging) {
      std::cerr << m_lastError << std::endl;
    }
    return false;
  }

  // Set the output pin to retract the slide (typically OFF)
  EziIOError result = setOutputPin(slide->getOutputConfig(), false);

  if (result == EziIOError::SUCCESS) {
    // Update the slide's internal state
    slide->retract();
    if (m_enableLogging) {
      std::cout << "Retracted slide: " << slideName << std::endl;
    }
    return true;
  }
  else {
    m_operationErrorCount++;
    logError("retractSlide(" + slideName + ")", result);
    return false;
  }
}

SlideState PneumaticManager::getSlideState(const std::string& slideName) const
{
  auto slide = getSlide(slideName);
  if (!slide) {
    m_lastError = "Cannot find slide: " + slideName;
    if (m_enableLogging) {
      std::cerr << m_lastError << std::endl;
    }
    return SlideState::P_ERROR;
  }

  return slide->getState();
}

std::shared_ptr<PneumaticSlide> PneumaticManager::getSlide(const std::string& slideName) const
{
  std::lock_guard<std::mutex> lock(m_slidesMutex);
  auto it = m_slides.find(slideName);
  if (it != m_slides.end()) {
    return it->second;
  }
  return nullptr;
}

std::vector<std::string> PneumaticManager::getSlideNames() const
{
  std::lock_guard<std::mutex> lock(m_slidesMutex);
  std::vector<std::string> names;
  names.reserve(m_slides.size());

  for (const auto& pair : m_slides) {
    names.push_back(pair.first);
  }
  return names;
}

void PneumaticManager::updateAllSlideStates()
{
  std::lock_guard<std::mutex> lock(m_slidesMutex);

  for (auto& [name, slide] : m_slides) {
    bool extendedSensor = false;
    bool retractedSensor = false;

    // Read the current state of the extended and retracted sensors
    EziIOError extResult = readInputPin(slide->getExtendedInputConfig(), extendedSensor);
    EziIOError retResult = readInputPin(slide->getRetractedInputConfig(), retractedSensor);

    // Enhanced logging - always log if slide is moving, otherwise every 20 polls
    if (m_enableLogging) {
      static std::map<std::string, int> logCounters;
      bool shouldLog = false;

      // Always log if slide is moving
      if (slide->getState() == SlideState::MOVING) {
        shouldLog = true;
      }
      else {
        // Otherwise log every 20 polls (1 second)
        if (logCounters[name]++ % 20 == 0) {
          shouldLog = true;
        }
      }

      if (shouldLog) {
        std::cout << "Slide " << name
          << " [" << GetStateString(slide->getState()) << "]"
          << " - Extended: " << (extendedSensor ? "ON" : "OFF")
          << ", Retracted: " << (retractedSensor ? "ON" : "OFF")
          << std::endl;
      }
    }

    // Check for errors
    if (extResult != EziIOError::SUCCESS || retResult != EziIOError::SUCCESS) {
      if (m_enableLogging) {
        std::cerr << "Error reading sensors for slide " << name
          << " - Extended: " << EziIOManager::getErrorString(extResult)
          << ", Retracted: " << EziIOManager::getErrorString(retResult) << std::endl;
      }
      m_pollingErrorCount++;
      continue;
    }

    // Update the slide's state based on sensor readings
    slide->updateState(extendedSensor, retractedSensor);
  }
}

// Add this helper function to PneumaticManager
const char* PneumaticManager::GetStateString(SlideState state) const {
  switch (state) {
  case SlideState::EXTENDED:  return "EXTENDED";
  case SlideState::RETRACTED: return "RETRACTED";
  case SlideState::MOVING:    return "MOVING";
  case SlideState::P_ERROR:   return "ERROR";
  case SlideState::UNKNOWN:
  default:                    return "UNKNOWN";
  }
}

void PneumaticManager::setStateChangeCallback(std::function<void(const std::string&, SlideState)> callback)
{
  m_stateChangeCallback = callback;

  // Update all slide callbacks
  std::lock_guard<std::mutex> lock(m_slidesMutex);
  for (auto& [name, slide] : m_slides) {
    slide->setStateChangeCallback(m_stateChangeCallback);
  }
}

void PneumaticManager::resetAllSlides()
{
  std::lock_guard<std::mutex> lock(m_slidesMutex);
  for (auto& [name, slide] : m_slides) {
    slide->resetState();
  }

  // Reset error counters
  m_pollingErrorCount = 0;
  m_operationErrorCount = 0;
  m_lastError.clear();
}

bool PneumaticManager::resolvePinConfig(IOPinConfig& config)
{
  // Look up device ID
  auto deviceIt = m_deviceIdMap.find(config.deviceName);
  if (deviceIt == m_deviceIdMap.end()) {
    m_lastError = "Unknown device name: " + config.deviceName;
    if (m_enableLogging) {
      std::cerr << m_lastError << std::endl;
    }
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

  m_lastError = "Unknown pin name: " + config.pinName + " for device: " + config.deviceName;
  if (m_enableLogging) {
    std::cerr << m_lastError << std::endl;
  }
  return false;
}

EziIOError PneumaticManager::readInputPin(const IOPinConfig& config, bool& state) const
{
  uint32_t inputs = 0, latch = 0;

  // Read inputs from the device using the new error-returning API
  EziIOError result = m_ioManager.getLastInputStatus(config.deviceId, inputs, latch);

  if (result != EziIOError::SUCCESS) {
    state = false;
    return result;
  }

  // Check if the specific pin is high
  state = (inputs & (1 << config.pinNumber)) != 0;
  return EziIOError::SUCCESS;
}

EziIOError PneumaticManager::setOutputPin(const IOPinConfig& config, bool state) const
{
  // Use the EziIO manager to set the output pin with error handling
  return m_ioManager.setOutput(config.deviceId, config.pinNumber, state);
}

void PneumaticManager::startPolling(unsigned int intervalMs)
{
  // Don't start if already running
  if (m_pollingThread) {
    if (m_enableLogging) {
      std::cout << "PneumaticManager polling thread already running" << std::endl;
    }
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

    if (m_enableLogging) {
      std::cout << "PneumaticManager polling thread stopped" << std::endl;
    }
  }
}

bool PneumaticManager::isPolling() const
{
  return m_pollingThread != nullptr && !m_stopPolling;
}

void PneumaticManager::pollingThreadFunc()
{
  std::cout << "Pneumatic polling thread started" << std::endl;

  int consecutiveErrors = 0;
  const int maxConsecutiveErrors = 10;

  while (!m_stopPolling) {
    // Check if EziIO manager has connected devices
    if (m_ioManager.getConnectedDeviceCount() == 0) {
      // No devices connected, sleep longer
      if (m_enableLogging && consecutiveErrors == 0) {
        std::cout << "PneumaticManager: No devices connected, sleeping..." << std::endl;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      consecutiveErrors++;

      // After too many consecutive errors, reduce logging
      if (consecutiveErrors > maxConsecutiveErrors) {
        consecutiveErrors = maxConsecutiveErrors;
      }
      continue;
    }

    // Reset error counter when devices are connected
    if (consecutiveErrors > 0) {
      consecutiveErrors = 0;
      if (m_enableLogging) {
        std::cout << "PneumaticManager: Devices connected, resuming normal polling" << std::endl;
      }
    }

    // Update all slide states
    updateAllSlideStates();

    // Sleep for the specified interval
    std::this_thread::sleep_for(std::chrono::milliseconds(m_pollingInterval));
  }

  std::cout << "Pneumatic polling thread stopped" << std::endl;
}

void PneumaticManager::logError(const std::string& operation, EziIOError error) const
{
  std::stringstream ss;
  ss << "PneumaticManager::" << operation << " failed: "
    << EziIOManager::getErrorString(error);
  m_lastError = ss.str();

  if (m_enableLogging) {
    std::cerr << m_lastError << std::endl;
  }
}

PneumaticManager::Statistics PneumaticManager::getStatistics() const
{
  Statistics stats;
  stats.totalSlides = static_cast<int>(m_slides.size());
  stats.connectedDevices = m_ioManager.getConnectedDeviceCount();
  stats.pollingErrors = m_pollingErrorCount.load();
  stats.operationErrors = m_operationErrorCount.load();
  return stats;
}