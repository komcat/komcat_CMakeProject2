#pragma once

// Only prevent winsock for this specific header
#ifdef _WIN32
#define _WINSOCKAPI_   // Prevent inclusion of winsock.h in this file only
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "SPDPowerSupply.h"
#include <nlohmann/json.hpp>
#include <memory>


#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>

// Use PowerSupply namespace
using PowerSupply::SPDPowerSupply;
using PowerSupply::SPDSweepResult; // Add this with other using declarations

// Forward declarations
class Logger;


// Add this structure for device status data
struct SPDDeviceStatus {
  std::string deviceName;
  bool outputEnabled = false;
  double voltage = 0.0;
  double current = 0.0;
  bool isConnected = false;
  std::chrono::steady_clock::time_point timestamp;
};

// Add this interface for subscribers
class ISPDStatusSubscriber {
public:
  virtual ~ISPDStatusSubscriber() = default;
  virtual void OnDeviceStatusUpdate(const SPDDeviceStatus& status) = 0;
  virtual void OnDeviceConnectionChange(const std::string& deviceName, bool connected) = 0;
};

/**
 * @brief Manager class for multiple SPD Power Supply devices
 *
 * This class provides centralized management of multiple SPD Power Supply devices,
 * including initialization from config files, device discovery, synchronized operations,
 * and UI rendering capabilities.
 */
class SPDPowerSupplyManager {
public:
  // === Construction & Destruction ===
  SPDPowerSupplyManager();
  ~SPDPowerSupplyManager();

  // Prevent copy construction and assignment
  SPDPowerSupplyManager(const SPDPowerSupplyManager&) = delete;
  SPDPowerSupplyManager& operator=(const SPDPowerSupplyManager&) = delete;

  // === Configuration & Initialization ===

  /**
   * @brief Initialize from JSON config file
   * @param configFile Path to config file (default: "spd_devices_config.json")
   * @return true if initialization successful, false otherwise
   */
  bool Initialize(const std::string& configFile = "spd_devices_config.json");

  /**
   * @brief Load default configuration when no config file is available
   */
  void LoadDefaultConfiguration();

  // === Device Management ===

  /**
   * @brief Add a device with the given name and connection details
   * @param name Device name/identifier
   * @param resourceString VISA resource string or IP address
   * @param port Port number (for TCP/IP connections, default: 5555)
   * @return true if device added successfully, false otherwise
   */
  bool AddDevice(const std::string& name, const std::string& resourceString, int port = 5555);

  /**
   * @brief Get a device by name
   * @param name Device name/identifier
   * @return Pointer to SPDPowerSupply device, nullptr if not found
   */
  SPDPowerSupply* GetDevice(const std::string& name);

  /**
   * @brief Remove a device by name
   * @param name Device name/identifier
   * @return true if device removed successfully, false if not found
   */
  bool RemoveDevice(const std::string& name);

  /**
   * @brief Get list of all device names
   * @return Vector containing all device names
   */
  std::vector<std::string> GetDeviceNames() const;

  /**
   * @brief Get total number of managed devices
   * @return Number of devices
   */
  size_t GetDeviceCount() const { return m_devices.size(); }

  /**
   * @brief Get number of connected devices
   * @return Number of connected devices
   */
  int GetConnectedCount() const;

  // === Connection Management ===

  /**
   * @brief Connect all devices
   * @return Number of devices successfully connected
   */
  int ConnectAll();

  /**
   * @brief Disconnect all devices
   */
  void DisconnectAll();

  /**
   * @brief Check if all devices are connected
   * @return true if all devices are connected, false otherwise
   */
  bool AreAllConnected() const;

  // === Device Discovery ===

  /**
   * @brief Discover and add SPD devices automatically
   * @param connectImmediately If true, connect to discovered devices immediately
   * @return Number of devices discovered and added
   */
  int AddDiscoveredDevices(bool connectImmediately = true);

  // === Synchronized Operations ===

  /**
   * @brief Set output enable/disable for all devices
   * @param enable true to enable outputs, false to disable
   * @return true if operation successful on all connected devices
   */
  bool SetAllOutputs(bool enable);

  /**
   * @brief Reset all connected instruments to default state
   * @return true if operation successful on all connected devices
   */
  bool ResetAllInstruments();

  /**
   * @brief Set voltage for all devices (channel 1)
   * @param voltage Voltage to set (V)
   * @return true if operation successful on all connected devices
   */
  bool SetAllVoltages(double voltage);

  /**
   * @brief Set current limit for all devices (channel 1)
   * @param current Current limit to set (A)
   * @return true if operation successful on all connected devices
   */
  bool SetAllCurrentLimits(double current);

  /**
   * @brief Get status from all connected devices
   * @return Map of device name to status string
   */
  std::unordered_map<std::string, std::string> GetAllStatuses() const;

  // === Polling & Monitoring ===

  /**
   * @brief Start polling all connected devices for status updates
   * @param intervalMs Polling interval in milliseconds (default: 1000ms)
   */
  void StartAllPolling(int intervalMs = 1000);

  /**
   * @brief Stop polling all devices
   */
  void StopAllPolling();

  /**
   * @brief Check if polling is active
   * @return true if polling is running
   */
  bool IsPollingActive() const { return m_pollingActive.load(); }

  // === UI Management ===

  /**
   * @brief Render ImGui UI for all devices
   */
  void RenderUI();

  /**
   * @brief Toggle window visibility
   */
  void ToggleWindow();

  /**
   * @brief Check if window is visible
   * @return true if window is visible
   */
  bool IsVisible() const { return m_showWindow; }

  /**
   * @brief Get manager name
   * @return Manager name string
   */
  const std::string& GetName() const { return m_managerName; }

  // === Callbacks ===

  /**
   * @brief Set callback function for device status updates
   * @param callback Function to call when device status changes
   */
  void SetStatusUpdateCallback(std::function<void(const std::string&, const std::string&)> callback);

  /**
   * @brief Set callback function for connection state changes
   * @param callback Function to call when device connection state changes
   */
  void SetConnectionStateCallback(std::function<void(const std::string&, bool)> callback);

  // === Safety & Error Handling ===

  /**
   * @brief Emergency stop - disable all outputs immediately
   * @return true if emergency stop successful on all devices
   */
  bool EmergencyStop();

  /**
   * @brief Get last error message
   * @return Last error message string
   */
  const std::string& GetLastError() const { return m_lastError; }

  /**
   * @brief Clear last error message
   */
  void ClearError() { m_lastError.clear(); }

  // === Configuration Export/Import ===

  /**
   * @brief Export current configuration to JSON file
   * @param filename Output filename
   * @return true if export successful
   */
  bool ExportConfiguration(const std::string& filename) const;

  /**
   * @brief Get configuration as JSON string
   * @return JSON configuration string
   */
  std::string GetConfigurationJSON() const;

  // === Mode-Specific Operations ===

/**
 * @brief Set all devices to Constant Voltage (CV) mode
 * @param voltage Target voltage in volts
 * @param currentLimit Current compliance/limit in amps
 * @return true if operation successful on all connected devices
 */
  bool SetConstantVoltageMode(double voltage, double currentLimit);

  /**
   * @brief Set all devices to Constant Current (CC) mode
   * @param current Target current in amps
   * @param voltageLimit Voltage compliance/limit in volts
   * @return true if operation successful on all connected devices
   */
  bool SetConstantCurrentMode(double current, double voltageLimit);

  // === Subscription Management ===

    /**
     * @brief Subscribe to device status updates
     * @param subscriber Pointer to subscriber object
     * @param subscriberId Unique identifier for this subscription
     */
  void Subscribe(ISPDStatusSubscriber* subscriber, const std::string& subscriberId);

  /**
   * @brief Unsubscribe from status updates
   * @param subscriberId Subscriber ID to remove
   */
  void Unsubscribe(const std::string& subscriberId);

  /**
   * @brief Get list of active subscriber IDs
   * @return Vector of subscriber IDs
   */
  std::vector<std::string> GetSubscriberIds() const;


  // === Sweep Operations ===

  /**
   * @brief Perform voltage sweep on specified device
   * @param deviceName Name of the device to perform sweep on
   * @param channel Channel number (typically 1)
   * @param startV Starting voltage (V)
   * @param stopV Ending voltage (V)
   * @param steps Number of steps in sweep
   * @param currentLimit Current limit/compliance (A)
   * @param delayMs Delay between steps (ms)
   * @param results Vector to store sweep results
   * @return true if sweep successful, false otherwise
   */
  bool PerformVoltageSweep(const std::string& deviceName, int channel,
    double startV, double stopV, int steps,
    double currentLimit, double delayMs,
    std::vector<SPDSweepResult>& results);

  /**
   * @brief Perform current sweep on specified device
   * @param deviceName Name of the device to perform sweep on
   * @param channel Channel number (typically 1)
   * @param startA Starting current (A)
   * @param stopA Ending current (A)
   * @param steps Number of steps in sweep
   * @param voltageLimit Voltage limit/compliance (V)
   * @param delayMs Delay between steps (ms)
   * @param results Vector to store sweep results
   * @return true if sweep successful, false otherwise
   */
  bool PerformCurrentSweep(const std::string& deviceName, int channel,
    double startA, double stopA, int steps,
    double voltageLimit, double delayMs,
    std::vector<SPDSweepResult>& results);

  /**
   * @brief Perform voltage sweep on all connected devices simultaneously
   * @param channel Channel number (typically 1)
   * @param startV Starting voltage (V)
   * @param stopV Ending voltage (V)
   * @param steps Number of steps in sweep
   * @param currentLimit Current limit/compliance (A)
   * @param delayMs Delay between steps (ms)
   * @param allResults Map of device name to sweep results
   * @return Number of successful sweeps performed
   */
  int PerformVoltageSweepAll(int channel, double startV, double stopV, int steps,
    double currentLimit, double delayMs,
    std::unordered_map<std::string, std::vector<SPDSweepResult>>& allResults);

  /**
   * @brief Perform current sweep on all connected devices simultaneously
   * @param channel Channel number (typically 1)
   * @param startA Starting current (A)
   * @param stopA Ending current (A)
   * @param steps Number of steps in sweep
   * @param voltageLimit Voltage limit/compliance (V)
   * @param delayMs Delay between steps (ms)
   * @param allResults Map of device name to sweep results
   * @return Number of successful sweeps performed
   */
  int PerformCurrentSweepAll(int channel, double startA, double stopA, int steps,
    double voltageLimit, double delayMs,
    std::unordered_map<std::string, std::vector<SPDSweepResult>>& allResults);



  // Add these methods to SPDPowerSupplyManager.h in the public section:

  // === Individual Device Reading Operations ===

  /**
   * @brief Read voltage from a specific device
   * @param deviceName Name of the device to read from
   * @param channel Channel number (typically 1)
   * @param voltage Reference to store the voltage reading
   * @return true if reading successful, false otherwise
   */
  bool ReadVoltage(const std::string& deviceName, int channel, double& voltage);

  /**
   * @brief Read current from a specific device
   * @param deviceName Name of the device to read from
   * @param channel Channel number (typically 1)
   * @param current Reference to store the current reading
   * @return true if reading successful, false otherwise
   */
  bool ReadCurrent(const std::string& deviceName, int channel, double& current);

  /**
   * @brief Check if output is enabled on a specific device
   * @param deviceName Name of the device to check
   * @param channel Channel number (typically 1)
   * @param outputEnabled Reference to store the output state
   * @return true if status check successful, false otherwise
   */
  bool IsOutputEnabled(const std::string& deviceName, int channel, bool& outputEnabled);

  /**
   * @brief Read both voltage and current from a specific device
   * @param deviceName Name of the device to read from
   * @param channel Channel number (typically 1)
   * @param voltage Reference to store the voltage reading
   * @param current Reference to store the current reading
   * @return true if both readings successful, false otherwise
   */
  bool ReadVoltageAndCurrent(const std::string& deviceName, int channel, double& voltage, double& current);

  /**
  
	@brieft Get count of connected devices
  */
  int GetConnectedDeviceCount() const;

  /**
 * @brief Set output state for a specific device and channel
 * @param deviceName Name of the device
 * @param channel Channel number (typically 1)
 * @param enable true to enable, false to disable
 * @return true if successful
 */
  bool SetOutput(const std::string& deviceName, int channel, bool enable);

  /**
   * @brief Set CV mode on a specific device
   * @param deviceName Name of the device
   * @param voltage Target voltage in volts
   * @param currentLimit Current limit in amps
   * @return true if successful
   */
  bool SetConstantVoltageMode(const std::string& deviceName, double voltage, double currentLimit);

  /**
   * @brief Set CC mode on a specific device
   * @param deviceName Name of the device
   * @param current Target current in amps
   * @param voltageLimit Voltage limit in volts
   * @return true if successful
   */
  bool SetConstantCurrentMode(const std::string& deviceName, double current, double voltageLimit);

private:
  // === Private Types ===
  struct DeviceInfo {
    std::unique_ptr<SPDPowerSupply> device;
    std::string resourceString;
    int port;
    bool autoConnect;
    int pollingInterval;
    std::string description;
    std::chrono::steady_clock::time_point lastUpdate;

    DeviceInfo(const std::string& resource, int p = 5555)
      : resourceString(resource), port(p), autoConnect(true),
      pollingInterval(1000), lastUpdate(std::chrono::steady_clock::now()) {
    }
  };

  // Add subscriber management members
  std::unordered_map<std::string, ISPDStatusSubscriber*> m_subscribers;
  mutable std::mutex m_subscribersMutex;

  // === Private Members ===
  std::unordered_map<std::string, std::unique_ptr<DeviceInfo>> m_devices;
  mutable std::mutex m_devicesMutex;

  // Manager configuration
  std::string m_managerName;
  std::string m_configFile;
  bool m_autoConnect;
  int m_defaultPollingInterval;
  bool m_autoDiscovery;
  bool m_safetyMode;

  // UI state
  bool m_showWindow;

  // Polling thread management
  std::atomic<bool> m_pollingActive;
  std::thread m_pollingThread;
  std::atomic<int> m_pollingInterval;

  // Callbacks
  std::function<void(const std::string&, const std::string&)> m_statusUpdateCallback;
  std::function<void(const std::string&, bool)> m_connectionStateCallback;

  // Error handling
  mutable std::string m_lastError;
  mutable std::mutex m_errorMutex;

  // Logger
  Logger* m_logger;

  // === Private Methods ===

  /**
   * @brief Set error message thread-safely
   * @param error Error message
   */
  void SetError(const std::string& error) const;

  /**
   * @brief Log message if logger is available
   * @param level Log level
   * @param message Message to log
   */
  void LogMessage(const std::string& level, const std::string& message) const;

  /**
   * @brief Polling thread function
   */
  void PollingThreadFunction();

  /**
   * @brief Parse JSON configuration file
   * @param configFile Path to configuration file
   * @return true if parsing successful
   */
  bool ParseConfigurationFile(const std::string& configFile);

  /**
   * @brief Create device from configuration
   * @param name Device name
   * @param config Device configuration object
   * @return true if device created successfully
   */
  bool CreateDeviceFromConfig(const std::string& name, const nlohmann::json& config);

  /**
   * @brief Render device UI panel
   * @param name Device name
   * @param deviceInfo Device information
   */
  void RenderDevicePanel(const std::string& name, DeviceInfo& deviceInfo);

  /**
   * @brief Get VISA resource strings for SPD devices
   * @return Vector of VISA resource strings
   */
  std::vector<std::string> GetAvailableSPDDevices() const;



  /**
   * @brief Validate device configuration
   * @param config Configuration to validate
   * @return true if configuration is valid
   */
  bool ValidateDeviceConfig(const nlohmann::json& config) const;

  void NotifySubscribers(const std::unordered_map<std::string, SPDDeviceStatus>& statuses);
};