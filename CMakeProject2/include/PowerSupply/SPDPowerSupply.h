#pragma once

#include <string>
#include <memory>
#include <optional>
#include <vector>
#include <chrono>
// Add these includes at the top:
#include <mutex>
#include <atomic>

namespace PowerSupply {

  /**
   * @brief Structure to hold channel status information
   */
  struct ChannelStatus {
    std::optional<double> voltage;        // Actual output voltage (V)
    std::optional<double> current;        // Actual output current (A)
    std::optional<bool> output_enabled;   // Output state (true=ON, false=OFF)

    bool isValid() const {
      return voltage.has_value() && current.has_value() && output_enabled.has_value();
    }
  };

  /**
   * @brief Structure to hold measurement data over time
   */
  struct MeasurementSample {
    std::chrono::steady_clock::time_point timestamp;
    double voltage;
    double current;
    bool output_state;
  };

  /**
   * @brief VISA-based power supply controller for SPD series (Siglent)
   *
   * This class provides a C++ interface to control programmable power supplies
   * via VISA (Virtual Instrument Software Architecture). Tested with Siglent SPD series.
   */
  class SPDPowerSupply {
  public:
    /**
     * @brief Constructor
     * @param resource_string VISA resource string (e.g., "USB0::0xF4EC::0x1410::SPD13DCQ7R0719::INSTR")
     */
    explicit SPDPowerSupply(const std::string& resource_string = "");

    /**
     * @brief Destructor - automatically disconnects if connected
     */
    ~SPDPowerSupply();

    // Disable copy constructor and assignment operator
    SPDPowerSupply(const SPDPowerSupply&) = delete;
    SPDPowerSupply& operator=(const SPDPowerSupply&) = delete;

    // Enable move constructor and assignment
    SPDPowerSupply(SPDPowerSupply&& other) noexcept;
    SPDPowerSupply& operator=(SPDPowerSupply&& other) noexcept;

    /**
     * @brief Connect to the power supply
     * @param resource_string Optional VISA resource string. If empty, uses the one from constructor
     * @return true if connection successful, false otherwise
     */
    bool connect(const std::string& resource_string = "");

    /**
     * @brief Disconnect from the power supply
     */
    void disconnect();

    /**
     * @brief Check if currently connected
     * @return true if connected, false otherwise
     */
    bool isConnected() const { return is_connected_.load(); }

    /**
     * @brief Get current resource string
     * @return The resource string currently being used
     */
    std::string getResourceString() const { return resource_string_; }

    /**
     * @brief Get instrument identification string
     * @return Instrument ID string, empty if not connected or error
     */
    std::string getInstrumentID() const;

    /**
     * @brief Set output voltage for specified channel
     * @param channel Channel number (typically 1)
     * @param voltage Voltage to set in volts
     * @return true if successful, false otherwise
     */
    bool setVoltage(int channel, double voltage);

    /**
     * @brief Set current limit for specified channel
     * @param channel Channel number (typically 1)
     * @param current Current limit in amperes
     * @return true if successful, false otherwise
     */
    bool setCurrent(int channel, double current);

    /**
     * @brief Turn channel output on or off
     * @param channel Channel number
     * @param enabled true for ON, false for OFF
     * @return true if successful, false otherwise
     */
    bool setOutput(int channel, bool enabled);

    /**
     * @brief Get actual output voltage
     * @param channel Channel number
     * @return Voltage value, or nullopt if error
     */
    std::optional<double> getVoltage(int channel) const;

    /**
     * @brief Get actual output current
     * @param channel Channel number
     * @return Current value, or nullopt if error
     */
    std::optional<double> getCurrent(int channel) const;

    /**
     * @brief Get output state (on/off) using system status
     * @param channel Channel number
     * @return true if on, false if off, nullopt if error
     */
    std::optional<bool> getOutputState(int channel) const;

    /**
     * @brief Get comprehensive channel status
     * @param channel Channel number
     * @return ChannelStatus structure with voltage, current, and output state
     */
    ChannelStatus getChannelStatus(int channel) const;

    /**
     * @brief Convenience method to setup a channel with voltage, current, and output state
     * @param channel Channel number
     * @param voltage Voltage setting in volts
     * @param current Current limit in amperes
     * @param enable Whether to enable output immediately
     * @return true if all operations successful
     */
    bool setupChannel(int channel, double voltage, double current, bool enable = false);

    /**
     * @brief Monitor channel for specified duration with polling
     * @param channel Channel number to monitor
     * @param duration_seconds Duration to monitor in seconds
     * @param sample_interval_ms Sampling interval in milliseconds
     * @return Vector of measurement samples
     */
    std::vector<MeasurementSample> monitorChannel(int channel,
      double duration_seconds,
      int sample_interval_ms = 1000) const;

    /**
     * @brief Emergency stop - turn off all outputs immediately
     * @return true if all outputs successfully disabled
     */
    bool emergencyStop();

    /**
     * @brief Set timeout for VISA operations
     * @param timeout_ms Timeout in milliseconds
     * @return true if successful
     */
    bool setTimeout(int timeout_ms);

    /**
     * @brief Send raw SCPI command
     * @param command SCPI command string
     * @return true if successful
     */
    bool sendCommand(const std::string& command);

    /**
     * @brief Send SCPI query and get response
     * @param query SCPI query string
     * @return Response string, empty if error
     */
    std::string query(const std::string& query) const;

    /**
     * @brief Scan for available VISA resources
     * @return Vector of resource strings
     */
    static std::vector<std::string> scanAvailableResources();
    bool SetDebug(bool debugmode);
  private:
    std::string resource_string_;
    std::atomic<bool> is_connected_;  // Change from: bool is_connected_;
    mutable std::mutex visa_mutex_;   // Add this line
    
    // PIMPL idiom to hide VISA implementation details
    class Impl;
    std::unique_ptr<Impl> pimpl_;

    // Helper methods
    bool validateChannel(int channel) const;
    std::optional<double> parseDoubleResponse(const std::string& response) const;
    std::optional<bool> parseBoolResponse(const std::string& response) const;

    bool m_debugverbose = false;
  };

  /**
   * @brief RAII helper class for automatic output control
   *
   * Ensures that power supply output is turned off when going out of scope,
   * providing additional safety for test applications.
   */
  class SafeOutputControl {
  public:
    SafeOutputControl(SPDPowerSupply& ps, int channel, double voltage, double current);
    ~SafeOutputControl();

    // Disable copy/move
    SafeOutputControl(const SafeOutputControl&) = delete;
    SafeOutputControl& operator=(const SafeOutputControl&) = delete;
    SafeOutputControl(SafeOutputControl&&) = delete;
    SafeOutputControl& operator=(SafeOutputControl&&) = delete;

    bool isOutputEnabled() const { return output_enabled_; }

  private:
    SPDPowerSupply& power_supply_;
    int channel_;
    bool output_enabled_;
  };

} // namespace PowerSupply