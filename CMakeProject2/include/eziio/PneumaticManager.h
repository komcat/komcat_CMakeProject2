#pragma once

#include "PneumaticSlide.h"
#include "EziIO_Manager.h"
#include "IOConfigManager.h"
#include <memory>
#include <map>
#include <vector>
#include <mutex>

class PneumaticManager {
public:
  PneumaticManager(EziIOManager& ioManager);
  ~PneumaticManager();

  // Load pneumatic configuration from IOConfigManager
  bool loadConfiguration(IOConfigManager* configManager);

  // Initialize and connect to devices
  bool initialize();

  // Extend a specific slide
  bool extendSlide(const std::string& slideName);

  // Retract a specific slide
  bool retractSlide(const std::string& slideName);

  // Get the state of a specific slide
  SlideState getSlideState(const std::string& slideName) const;

  // Get a reference to a slide by name
  std::shared_ptr<PneumaticSlide> getSlide(const std::string& slideName) const;

  // Get all slide names
  std::vector<std::string> getSlideNames() const;

  // Update the state of all slides based on current sensor readings
  void updateAllSlideStates();

  // Subscribe to slide state changes
  void setStateChangeCallback(std::function<void(const std::string&, SlideState)> callback);

  // Reset all slides to UNKNOWN state
  void resetAllSlides();

  // Poll the IO status at regular intervals
  void startPolling(unsigned int intervalMs = 50);
  void stopPolling();
  bool isPolling() const;
  // Helper to resolve pin configurations
  bool resolvePinConfig(IOPinConfig& config);

  // Helper to read the current state of input pin
  bool readInputPin(const IOPinConfig& config) const;

  // Helper to set the state of output pin
  bool setOutputPin(const IOPinConfig& config, bool state) const;
private:
  // Reference to the EziIO manager
  EziIOManager& m_ioManager;

  // Map of slide name to pneumatic slide object
  std::map<std::string, std::shared_ptr<PneumaticSlide>> m_slides;

  // Map to look up device ID from device name
  std::map<std::string, int> m_deviceIdMap;

  // Maps to look up pin numbers from pin names (for each device)
  std::map<std::string, std::map<std::string, int>> m_inputPinMap;
  std::map<std::string, std::map<std::string, int>> m_outputPinMap;

  // Callback for slide state changes
  std::function<void(const std::string&, SlideState)> m_stateChangeCallback;



  // Polling thread
  std::thread* m_pollingThread = nullptr;
  std::atomic<bool> m_stopPolling;
  unsigned int m_pollingInterval;
  std::mutex m_slidesMutex;

  // Thread function for polling
  void pollingThreadFunc();
};