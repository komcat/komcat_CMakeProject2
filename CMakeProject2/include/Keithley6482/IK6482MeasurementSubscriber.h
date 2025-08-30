#pragma once

#include <string>
#include <chrono>

namespace Keithley {

  /**
   * @brief Measurement data structure for subscribers
   *
   * Contains all measurement information from a single channel reading
   */
  struct K6482MeasurementData {
    std::string deviceName;       // Name of the device
    int channel;                  // Channel number (1 or 2)
    double current;               // Current in Amps
    double voltage;               // Voltage in Volts
    std::string currentRange;     // Current range setting (e.g., "AUTO", "20nA", "2mA")
    std::chrono::steady_clock::time_point timestamp;
  };

  /**
   * @brief Device status structure for monitoring
   *
   * Contains complete status information for a device
   */
  struct K6482DeviceStatus {
    std::string deviceName;
    bool isConnected;
    double channel1Current;       // Channel 1 current in Amps
    double channel2Current;       // Channel 2 current in Amps
    double channel1Voltage;       // Channel 1 voltage in Volts
    double channel2Voltage;       // Channel 2 voltage in Volts
    std::chrono::steady_clock::time_point timestamp;
  };

  /**
   * @brief Interface for Keithley 6482 measurement subscribers
   *
   * Implement this interface to receive real-time updates from the Keithley6482Manager.
   * The manager will call these methods when relevant events occur.
   *
   * Usage example:
   * @code
   * class MyDataHandler : public IK6482MeasurementSubscriber {
   *     void OnMeasurementUpdate(const K6482MeasurementData& data) override {
   *         // Process new measurement data
   *         std::cout << "New reading: " << data.current << " A" << std::endl;
   *     }
   *     // ... implement other methods
   * };
   *
   * auto handler = std::make_shared<MyDataHandler>();
   * manager->Subscribe("MyHandler", handler);
   * @endcode
   */
  class IK6482MeasurementSubscriber {
  public:
    virtual ~IK6482MeasurementSubscriber() = default;

    /**
     * @brief Called when new measurement data is available
     *
     * This method is called for each channel measurement during polling.
     * It will be called twice per polling interval (once for each channel).
     *
     * @param data Measurement data containing current, voltage, and metadata
     *
     * @note This method is called from the polling thread, so implementations
     *       should be thread-safe and avoid blocking operations.
     */
    virtual void OnMeasurementUpdate(const K6482MeasurementData& data) = 0;

    /**
     * @brief Called when device status changes
     *
     * This provides a complete status update for a device, including both channels.
     * Called once per device per polling interval.
     *
     * @param status Complete device status with both channel measurements
     */
    virtual void OnDeviceStatusUpdate(const K6482DeviceStatus& status) = 0;

    /**
     * @brief Called when device connection state changes
     *
     * Notified when a device connects or disconnects from the system.
     *
     * @param deviceName Name of the device that changed connection state
     * @param connected True if device connected, false if disconnected
     */
    virtual void OnDeviceConnectionChange(const std::string& deviceName, bool connected) = 0;

    /**
     * @brief Called when polling starts for a device
     *
     * Notified when measurement polling begins for a specific device.
     *
     * @param deviceName Name of the device starting polling
     * @param intervalMs Polling interval in milliseconds
     */
    virtual void OnPollingStarted(const std::string& deviceName, int intervalMs) = 0;

    /**
     * @brief Called when polling stops for a device
     *
     * Notified when measurement polling ends for a specific device.
     *
     * @param deviceName Name of the device stopping polling
     */
    virtual void OnPollingStopped(const std::string& deviceName) = 0;
  };

} // namespace Keithley