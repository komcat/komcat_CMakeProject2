#pragma once

#include <string>
#include <memory>
#include <optional>
#include <mutex>
#include <atomic>
#include <vector>
#include <chrono>

namespace Keithley {

  /**
   * @brief Measurement data structure
   */
  struct CurrentMeasurement {
    double channel1_current;  // Current in Amps
    double channel2_current;  // Current in Amps
    double channel1_voltage; // Voltage in Volts (if source voltage mode)
    double channel2_voltage; // Voltage in Volts
    std::chrono::steady_clock::time_point timestamp;
  };

  /**
   * @brief Channel configuration
   */
  struct ChannelConfig {
    int channel;           // 1 or 2
    double range;         // Current range (2e-9 to 20e-3)
    double sourceVoltage; // Source voltage (-30 to +30V)
    bool filterEnabled;   // Enable measurement filter
  };

  /**
   * @brief VISA-based Keithley 6482 Dual Channel Picoammeter Controller
   *
   * Controls Keithley 6482 via GPIB/VISA interface
   */
  class Keithley6482 {
  public:
    // Constructor/Destructor
    explicit Keithley6482(const std::string& resource_string = "GPIB0::1::INSTR");
    ~Keithley6482();

    // Disable copy
    Keithley6482(const Keithley6482&) = delete;
    Keithley6482& operator=(const Keithley6482&) = delete;

    // Enable move
    Keithley6482(Keithley6482&& other) noexcept;
    Keithley6482& operator=(Keithley6482&& other) noexcept;

    // === Connection Management ===
    bool connect(const std::string& resource_string = "");
    void disconnect();
    bool isConnected() const { return is_connected_.load(); }
    std::string getResourceString() const { return resource_string_; }
    std::string getInstrumentID() const;

    // === Basic Operations ===
    bool reset();
    bool clearErrors();
    std::string getLastError() const { return last_error_; }

    // === Source Voltage Control ===
    bool setSourceVoltage(int channel, double voltage);  // -30V to +30V
    bool enableSourceVoltage(int channel, bool enable);
    std::optional<double> getSourceVoltage(int channel) const;

    // === Current Measurement ===
    std::optional<double> readCurrent(int channel) const;
    std::optional<double> readVoltage(int channel) const;
    std::optional<CurrentMeasurement> readBothChannels() const;  // Read both channels at once

    // === Range Control ===
    bool setCurrentRange(int channel, double range);  // 2nA to 20mA
    bool setAutoRange(int channel, bool enable);

    // === Measurement Configuration ===
    bool setIntegrationTime(double nplc);  // 0.01 to 10 PLC
    bool setFilter(int channel, bool enable, int count = 10);

    // === Polling Control ===
    void startPolling(int interval_ms = 100);
    void stopPolling();
    bool isPolling() const { return polling_active_.load(); }

    // Get latest measurement
    std::optional<CurrentMeasurement> getLatestMeasurement() const;

    // === Utility ===
    static std::vector<std::string> scanAvailableResources();
    bool setTimeout(int timeout_ms);

  private:
    // VISA implementation details
    class Impl;
    std::unique_ptr<Impl> pimpl_;

    // Connection state
    std::string resource_string_;
    std::atomic<bool> is_connected_;
    mutable std::mutex visa_mutex_;

    // Polling state
    std::atomic<bool> polling_active_;
    std::thread polling_thread_;
    std::atomic<int> polling_interval_;

    // Latest measurement
    mutable std::mutex measurement_mutex_;
    CurrentMeasurement latest_measurement_;

    // Error handling
    mutable std::string last_error_;

    // Helper methods
    bool sendCommand(const std::string& command) const;
    std::string query(const std::string& command) const;
    std::string queryInternal(const std::string& command) const;  // Internal version without lock
    bool validateChannel(int channel) const;
    std::optional<double> parseDoubleResponse(const std::string& response) const;
    void pollingThreadFunction();
    void setError(const std::string& error) const;
  };

} // namespace Keithley