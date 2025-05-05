// pi_analog_manager.h
#pragma once

#include "pi_analog_reader.h"
#include "pi_controller_manager.h"
#include "MotionConfigManager.h"
#include "include/ui/ToolbarMenu.h" // For ITogglableUI interface
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <thread>

// Forward declaration
class GlobalDataStore;

// Make PIAnalogManager implement ITogglableUI interface
class PIAnalogManager : public ITogglableUI {
public:
  PIAnalogManager(PIControllerManager& controllerManager, MotionConfigManager& configManager);
  ~PIAnalogManager();

  // Initialize readers for all controllers
  void InitializeReaders();

  // Get a reader for a specific device
  PIAnalogReader* GetReader(const std::string& deviceName);

  // Update readings for all connected devices
  void UpdateAllReadings();

  // Start polling for analog readings at regular intervals
  void startPolling(unsigned int intervalMs = 100);

  // Stop polling for analog readings
  void stopPolling();

  // Check if polling is active
  bool isPolling() const;

  // Cleanup readers (for proper shutdown)
  void cleanupReaders();

  // Enable or disable debug logging
  void EnableDebugLogging(bool enable) { m_enableDebugLogging = enable; }

  // Render UI
  void RenderUI();

  // ITogglableUI interface implementation
  bool IsVisible() const override { return m_showWindow; }
  void ToggleWindow() override { m_showWindow = !m_showWindow; }
  const std::string& GetName() const override { return m_windowTitle; }

private:
  PIControllerManager& m_controllerManager;
  MotionConfigManager& m_configManager;
  std::map<std::string, std::unique_ptr<PIAnalogReader>> m_readers;
  Logger* m_logger;
  GlobalDataStore* m_dataStore = nullptr;
  bool m_enableDebugLogging = false;

  // Get a list of device names that have PI controllers
  std::vector<std::string> GetPIControllerDeviceNames() const;

  // UI state
  bool m_showWindow = true;
  std::string m_windowTitle = "PI Analog Monitor";

  // Polling thread
  std::thread* m_pollingThread = nullptr;
  std::atomic<bool> m_stopPolling{ false };
  unsigned int m_pollingInterval{ 100 };
  std::mutex m_readersMutex;

  // Thread function for polling
  void pollingThreadFunc();
};