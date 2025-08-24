#pragma once

#include "PneumaticSlide.h"
#include "EziIO_Manager.h"
#include "IOConfigManager.h"
#include <memory>
#include <map>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>

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

  // Helper to read the current state of input pin (returns error code)
  EziIOError readInputPin(const IOPinConfig& config, bool& state) const;

  // Helper to set the state of output pin (returns error code)
  EziIOError setOutputPin(const IOPinConfig& config, bool state) const;

  // Get last error message
  std::string getLastError() const { return m_lastError; }

  // Enable/disable logging
  void setLogging(bool enable) { m_enableLogging = enable; }

  // Get statistics
  struct Statistics {
    int totalSlides;
    int connectedDevices;
    int pollingErrors;
    int operationErrors;
  };
  Statistics getStatistics() const;

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
  mutable std::mutex m_slidesMutex;

  // Error tracking
  mutable std::string m_lastError;
  mutable std::atomic<int> m_pollingErrorCount{ 0 };
  mutable std::atomic<int> m_operationErrorCount{ 0 };
  bool m_enableLogging = true;

  // Thread function for polling
  void pollingThreadFunc();

  // Helper to log errors
  void logError(const std::string& operation, EziIOError error) const;
};