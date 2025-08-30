#pragma once

#include "Keithley6482.h"
#include "IK6482MeasurementSubscriber.h"  // Include the subscriber interface
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>

namespace Keithley {

  /**
   * @brief Device information structure
   */
  struct K6482DeviceInfo {
    std::unique_ptr<Keithley6482> device;
    std::string resourceString;
    std::string description;
    bool autoConnect;
    int pollingInterval;
  };

  /**
   * @brief Manager class for multiple Keithley 6482 devices
   *
   * Provides centralized control of multiple Keithley 6482 picoammeters
   * with publish-subscribe pattern for measurement updates
   */
  class Keithley6482Manager {
  public:
    // === Construction & Destruction ===
    Keithley6482Manager();
    ~Keithley6482Manager();

    // Prevent copy
    Keithley6482Manager(const Keithley6482Manager&) = delete;
    Keithley6482Manager& operator=(const Keithley6482Manager&) = delete;

    // === Subscriber Management ===

    /**
     * @brief Subscribe to measurement updates
     * @param subscriberName Unique name for the subscriber
     * @param subscriber Pointer to subscriber implementation
     * @return true if subscription successful
     */
    bool Subscribe(const std::string& subscriberName,
      std::shared_ptr<IK6482MeasurementSubscriber> subscriber);

    /**
     * @brief Unsubscribe from updates
     * @param subscriberName Name of subscriber to remove
     * @return true if unsubscription successful
     */
    bool Unsubscribe(const std::string& subscriberName);

    /**
     * @brief Get list of all subscriber names
     * @return Vector of subscriber names
     */
    std::vector<std::string> GetSubscriberNames() const;

    /**
     * @brief Get subscriber count
     * @return Number of active subscribers
     */
    size_t GetSubscriberCount() const;

    // === Device Management ===

    /**
     * @brief Add a device
     * @param name Device identifier
     * @param resourceString VISA resource string (e.g., "GPIB0::1::INSTR")
     * @return true if device added successfully
     */
    bool AddDevice(const std::string& name, const std::string& resourceString);

    /**
     * @brief Get device by name
     * @param name Device identifier
     * @return Pointer to device, nullptr if not found
     */
    Keithley6482* GetDevice(const std::string& name);

    /**
     * @brief Remove device
     * @param name Device identifier
     * @return true if device removed
     */
    bool RemoveDevice(const std::string& name);

    /**
     * @brief Get all device names
     * @return Vector of device names
     */
    std::vector<std::string> GetDeviceNames() const;

    /**
     * @brief Get device count
     * @return Number of managed devices
     */
    size_t GetDeviceCount() const { return m_devices.size(); }

    /**
     * @brief Get connected device count
     * @return Number of connected devices
     */
    int GetConnectedCount() const;

    // === Connection Management ===

    /**
     * @brief Connect all devices
     * @return Number of devices connected
     */
    int ConnectAll();

    /**
     * @brief Disconnect all devices
     */
    void DisconnectAll();

    /**
     * @brief Check if all devices connected
     * @return true if all connected
     */
    bool AreAllConnected() const;

    // === Device Discovery ===

    /**
     * @brief Discover and add Keithley devices
     * @param connectImmediately Connect after discovery
     * @return Number of devices found
     */
    int DiscoverDevices(bool connectImmediately = true);

    // === Synchronized Operations ===

    /**
     * @brief Set source voltage on all devices
     * @param channel Channel number (1 or 2)
     * @param voltage Voltage in volts
     * @return true if successful on all devices
     */
    bool SetAllSourceVoltages(int channel, double voltage);

    /**
     * @brief Enable/disable source voltage on all devices
     * @param channel Channel number
     * @param enable Enable state
     * @return true if successful
     */
    bool EnableAllSourceVoltages(int channel, bool enable);

    /**
     * @brief Set current range on all devices
     * @param channel Channel number
     * @param range Current range in amps
     * @return true if successful
     */
    bool SetAllCurrentRanges(int channel, double range);

    /**
     * @brief Set auto range on all devices
     * @param channel Channel number
     * @param enable Enable auto range
     * @return true if successful
     */
    bool SetAllAutoRange(int channel, bool enable);

    /**
     * @brief Reset all devices
     * @return true if successful
     */
    bool ResetAll();

    // === Measurement & Polling ===

    /**
     * @brief Start polling all devices
     * @param intervalMs Polling interval in milliseconds
     */
    void StartAllPolling(int intervalMs = 100);

    /**
     * @brief Stop polling all devices
     */
    void StopAllPolling();

    /**
     * @brief Check if polling active
     * @return true if polling
     */
    bool IsPollingActive() const { return m_pollingActive.load(); }

    /**
     * @brief Get all device statuses
     * @return Map of device name to status
     */
    std::unordered_map<std::string, K6482DeviceStatus> GetAllStatuses() const;

    /**
     * @brief Read current from all devices
     * @param channel Channel number
     * @return Map of device name to current value
     */
    std::unordered_map<std::string, double> ReadAllCurrents(int channel) const;

    // === Callbacks (Legacy - use Subscribe instead) ===

    /**
     * @brief Set status update callback (deprecated - use Subscribe)
     * @param callback Function called on status updates
     */
    void SetStatusUpdateCallback(
      std::function<void(const std::string&, const K6482DeviceStatus&)> callback) {
      m_statusUpdateCallback = callback;
    }

    /**
     * @brief Set connection state callback (deprecated - use Subscribe)
     * @param callback Function called on connection changes
     */
    void SetConnectionStateCallback(
      std::function<void(const std::string&, bool)> callback) {
      m_connectionStateCallback = callback;
    }

    // === Configuration & Initialization ===

    /**
     * @brief Initialize from JSON config file
     * @param configFile Path to config file (default: "keithley6482_devices_config.json")
     * @return true if initialization successful, false otherwise
     */
    bool Initialize(const std::string& configFile = "keithley6482_devices_config.json");

    /**
     * @brief Load default configuration when no config file is available
     */
    void LoadDefaultConfiguration();

    /**
     * @brief Load configuration from JSON file
     * @param configFile Path to config file
     * @return true if successful
     */
    bool LoadConfiguration(const std::string& configFile);

    /**
     * @brief Save current configuration
     * @param configFile Path to save config
     * @return true if successful
     */
    bool SaveConfiguration(const std::string& configFile) const;

    /**
     * @brief Generate default configuration file
     * @param configFile Path to save default config
     * @return true if successful
     */
    bool GenerateDefaultConfigFile(const std::string& configFile) const;

    // === Error Handling ===

    /**
     * @brief Get last error message
     * @return Error string
     */
    std::string GetLastError() const { return m_lastError; }

  private:
    // Device storage
    mutable std::mutex m_devicesMutex;
    std::unordered_map<std::string, std::unique_ptr<K6482DeviceInfo>> m_devices;

    // Subscriber storage
    mutable std::mutex m_subscribersMutex;
    std::unordered_map<std::string, std::shared_ptr<IK6482MeasurementSubscriber>> m_subscribers;

    // Polling control
    std::atomic<bool> m_pollingActive;
    std::thread m_pollingThread;
    std::atomic<int> m_pollingInterval;

    // Callbacks (legacy support)
    std::function<void(const std::string&, const K6482DeviceStatus&)> m_statusUpdateCallback;
    std::function<void(const std::string&, bool)> m_connectionStateCallback;

    // Error handling
    mutable std::string m_lastError;
    mutable std::mutex m_errorMutex;

    // Helper methods
    void SetError(const std::string& error) const;
    void PollingThreadFunction();
    std::vector<std::string> FindKeithleyDevices() const;

    // Notification methods
    void NotifyMeasurementUpdate(const K6482MeasurementData& data);
    void NotifyDeviceStatusUpdate(const K6482DeviceStatus& status);
    void NotifyConnectionChange(const std::string& deviceName, bool connected);
    void NotifyPollingStarted(const std::string& deviceName, int intervalMs);
    void NotifyPollingStopped(const std::string& deviceName);
  };

} // namespace Keithley