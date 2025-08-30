#include "Keithley6482.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <iomanip>

#ifdef _WIN32
#include <visa.h>
#else
#include <visa.h>
#endif

namespace Keithley {

  // PIMPL implementation
  class Keithley6482::Impl {
  public:
    ViSession default_rm;
    ViSession instrument;
    bool visa_initialized;

    Impl() : default_rm(VI_NULL), instrument(VI_NULL), visa_initialized(false) {}

    ~Impl() {
      cleanup();
    }

    void cleanup() {
      if (instrument != VI_NULL) {
        viClose(instrument);
        instrument = VI_NULL;
      }
      if (default_rm != VI_NULL) {
        viClose(default_rm);
        default_rm = VI_NULL;
      }
      visa_initialized = false;
    }
  };

  // Constructor
  Keithley6482::Keithley6482(const std::string& resource_string)
    : resource_string_(resource_string)
    , is_connected_(false)
    , polling_active_(false)
    , polling_interval_(100)
    , pimpl_(std::make_unique<Impl>()) {
  }

  // Destructor
  Keithley6482::~Keithley6482() {
    stopPolling();
    disconnect();
  }

  // Move constructor
  Keithley6482::Keithley6482(Keithley6482&& other) noexcept
    : resource_string_(std::move(other.resource_string_))
    , is_connected_(other.is_connected_.load())
    , polling_active_(false)
    , polling_interval_(other.polling_interval_.load())
    , pimpl_(std::move(other.pimpl_)) {
    other.is_connected_.store(false);
    other.polling_active_.store(false);
  }

  // Move assignment
  Keithley6482& Keithley6482::operator=(Keithley6482&& other) noexcept {
    if (this != &other) {
      disconnect();
      resource_string_ = std::move(other.resource_string_);
      is_connected_.store(other.is_connected_.load());
      polling_interval_.store(other.polling_interval_.load());
      pimpl_ = std::move(other.pimpl_);
      other.is_connected_.store(false);
    }
    return *this;
  }

  // Connect to instrument
  bool Keithley6482::connect(const std::string& resource_string) {
    // Use a scope block to manage lock lifetime
    {
      std::lock_guard<std::mutex> lock(visa_mutex_);

      if (is_connected_.load()) {
        return true;
      }

      if (!resource_string.empty()) {
        resource_string_ = resource_string;
      }

      if (resource_string_.empty()) {
        setError("No resource string specified");
        return false;
      }

      try {
        // Initialize VISA
        ViStatus status = viOpenDefaultRM(&pimpl_->default_rm);
        if (status != VI_SUCCESS) {
          setError("Failed to initialize VISA Resource Manager");
          return false;
        }

        // Open instrument
        status = viOpen(pimpl_->default_rm, (ViRsrc)resource_string_.c_str(),
          VI_NULL, VI_NULL, &pimpl_->instrument);
        if (status != VI_SUCCESS) {
          setError("Failed to open instrument: " + resource_string_);
          pimpl_->cleanup();
          return false;
        }

        pimpl_->visa_initialized = true;

        // Configure termination character
        viSetAttribute(pimpl_->instrument, VI_ATTR_TERMCHAR_EN, VI_TRUE);
        viSetAttribute(pimpl_->instrument, VI_ATTR_TERMCHAR, '\n');

        // Set timeout
        viSetAttribute(pimpl_->instrument, VI_ATTR_TMO_VALUE, 5000);

        // Check connection with IDN query (using internal query that doesn't take lock)
        std::string idn = queryInternal("*IDN?");
        if (idn.empty()) {
          setError("Failed to get instrument identification");
          pimpl_->cleanup();
          is_connected_.store(false);
          return false;
        }

        // Verify it's a Keithley 6482
        if (idn.find("6482") == std::string::npos) {
          setError("Instrument is not a Keithley 6482: " + idn);
          pimpl_->cleanup();
          is_connected_.store(false);
          return false;
        }

        is_connected_.store(true);
        std::cout << "Connected to: " << idn << std::endl;

      }
      catch (const std::exception& e) {
        setError("Exception during connection: " + std::string(e.what()));
        pimpl_->cleanup();
        is_connected_.store(false);
        return false;
      }
    } // Lock released here

    // Now call these methods which need their own locks
    reset();
    clearErrors();

    return true;
  }

  // Disconnect
  void Keithley6482::disconnect() {
    // First stop polling without lock (it has its own synchronization)
    stopPolling();

    // Then acquire lock for VISA cleanup
    std::lock_guard<std::mutex> lock(visa_mutex_);

    if (pimpl_) {
      pimpl_->cleanup();
    }
    is_connected_.store(false);
  }

  // Get instrument ID
  std::string Keithley6482::getInstrumentID() const {
    return query("*IDN?");
  }

  // Read both channels at once
  std::optional<CurrentMeasurement> Keithley6482::readBothChannels() const {
    if (!is_connected_.load()) {
      return std::nullopt;
    }

    CurrentMeasurement measurement;
    measurement.timestamp = std::chrono::steady_clock::now();

    // Configure for dual channel current measurement
    if (!sendCommand(":SENS:FUNC 'CURR:ALL'")) {
      // Try alternative: read channels separately
      auto ch1 = readCurrent(1);
      auto ch2 = readCurrent(2);

      if (ch1 && ch2) {
        measurement.channel1_current = *ch1;
        measurement.channel2_current = *ch2;

        // Get voltages if available
        auto v1 = getSourceVoltage(1);
        auto v2 = getSourceVoltage(2);
        measurement.channel1_voltage = v1.value_or(0.0);
        measurement.channel2_voltage = v2.value_or(0.0);

        return measurement;
      }
      return std::nullopt;
    }

    // Read both channels
    std::string response = query(":READ?");
    if (!response.empty()) {
      // Parse response: ch1_current, ch2_current, timestamp, status...
      std::istringstream iss(response);
      std::string value;

      // Get channel 1 current
      if (std::getline(iss, value, ',')) {
        try {
          measurement.channel1_current = std::stod(value);
        }
        catch (...) {
          return std::nullopt;
        }
      }

      // Get channel 2 current
      if (std::getline(iss, value, ',')) {
        try {
          measurement.channel2_current = std::stod(value);
        }
        catch (...) {
          return std::nullopt;
        }
      }

      // Get voltages
      auto v1 = getSourceVoltage(1);
      auto v2 = getSourceVoltage(2);
      measurement.channel1_voltage = v1.value_or(0.0);
      measurement.channel2_voltage = v2.value_or(0.0);

      return measurement;
    }

    return std::nullopt;
  }

  // Reset instrument
  bool Keithley6482::reset() {
    return sendCommand("*RST");
  }

  // Clear errors
  bool Keithley6482::clearErrors() {
    return sendCommand("*CLS");
  }

  // Set source voltage
  bool Keithley6482::setSourceVoltage(int channel, double voltage) {
    if (!validateChannel(channel)) return false;

    // Clamp voltage to valid range
    if (voltage < -30.0) voltage = -30.0;
    if (voltage > 30.0) voltage = 30.0;

    std::ostringstream cmd;
    cmd << ":SOUR" << channel << ":VOLT:LEV " << std::fixed << std::setprecision(3) << voltage;

    return sendCommand(cmd.str());
  }

  // Enable/disable source voltage
  bool Keithley6482::enableSourceVoltage(int channel, bool enable) {
    if (!validateChannel(channel)) return false;

    std::ostringstream cmd;
    cmd << ":SOUR" << channel << ":VOLT:STAT " << (enable ? "ON" : "OFF");

    return sendCommand(cmd.str());
  }

  // Get source voltage
  std::optional<double> Keithley6482::getSourceVoltage(int channel) const {
    if (!validateChannel(channel)) return std::nullopt;

    std::ostringstream cmd;
    cmd << ":SOUR" << channel << ":VOLT:LEV?";

    std::string response = query(cmd.str());
    return parseDoubleResponse(response);
  }

  // Read current
  std::optional<double> Keithley6482::readCurrent(int channel) const {
    if (!validateChannel(channel)) return std::nullopt;

    // For Keithley 6482, configure and read current
    // Select the channel and function
    std::ostringstream cmd;

    // Set to measure current on specified channel
    cmd << ":SENS:FUNC 'CURR" << channel << "'";
    if (!sendCommand(cmd.str())) {
      return std::nullopt;
    }

    // Initiate measurement and read
    std::string response = query(":READ?");

    // The response format is typically: current,timestamp,status
    // We only need the first value (current)
    if (!response.empty()) {
      size_t comma_pos = response.find(',');
      if (comma_pos != std::string::npos) {
        response = response.substr(0, comma_pos);
      }
      return parseDoubleResponse(response);
    }

    return std::nullopt;
  }

  // Read voltage
  std::optional<double> Keithley6482::readVoltage(int channel) const {
    if (!validateChannel(channel)) return std::nullopt;

    // For 6482 in voltage source mode, read the source voltage level
    // If you want to measure voltage across the DUT, use VOLT function
    std::ostringstream cmd;

    // Try to read voltage measurement
    cmd << ":SENS:FUNC 'VOLT" << channel << "'";
    if (!sendCommand(cmd.str())) {
      // If voltage measurement not available, read source voltage setting
      cmd.str("");
      cmd << ":SOUR" << channel << ":VOLT:LEV?";
      std::string response = query(cmd.str());
      return parseDoubleResponse(response);
    }

    // Read voltage measurement
    std::string response = query(":READ?");

    if (!response.empty()) {
      size_t comma_pos = response.find(',');
      if (comma_pos != std::string::npos) {
        response = response.substr(0, comma_pos);
      }
      return parseDoubleResponse(response);
    }

    return std::nullopt;
  }

  // Set current range
  bool Keithley6482::setCurrentRange(int channel, double range) {
    if (!validateChannel(channel)) return false;

    // Valid ranges: 2nA, 20nA, 200nA, 2uA, 20uA, 200uA, 2mA, 20mA
    std::ostringstream cmd;
    cmd << ":SENS" << channel << ":CURR:RANG " << std::scientific << range;

    return sendCommand(cmd.str());
  }

  // Set auto range
  bool Keithley6482::setAutoRange(int channel, bool enable) {
    if (!validateChannel(channel)) return false;

    std::ostringstream cmd;
    cmd << ":SENS" << channel << ":CURR:RANG:AUTO " << (enable ? "ON" : "OFF");

    return sendCommand(cmd.str());
  }

  // Set integration time (NPLC)
  bool Keithley6482::setIntegrationTime(double nplc) {
    if (nplc < 0.01) nplc = 0.01;
    if (nplc > 10.0) nplc = 10.0;

    std::ostringstream cmd;
    cmd << ":SENS:CURR:NPLC " << std::fixed << std::setprecision(2) << nplc;

    return sendCommand(cmd.str());
  }

  // Set filter
  bool Keithley6482::setFilter(int channel, bool enable, int count) {
    if (!validateChannel(channel)) return false;

    std::ostringstream cmd;
    if (enable) {
      cmd << ":SENS" << channel << ":AVER:STAT ON";
      sendCommand(cmd.str());

      cmd.str("");
      cmd << ":SENS" << channel << ":AVER:COUN " << count;
      return sendCommand(cmd.str());
    }
    else {
      cmd << ":SENS" << channel << ":AVER:STAT OFF";
      return sendCommand(cmd.str());
    }
  }

  // Start polling
  void Keithley6482::startPolling(int interval_ms) {
    if (polling_active_.load()) {
      return;
    }

    polling_interval_.store(interval_ms);
    polling_active_.store(true);

    polling_thread_ = std::thread(&Keithley6482::pollingThreadFunction, this);
  }

  // Stop polling
  void Keithley6482::stopPolling() {
    if (!polling_active_.load()) {
      return;
    }

    polling_active_.store(false);

    if (polling_thread_.joinable()) {
      polling_thread_.join();
    }
  }

  // Get latest measurement
  std::optional<CurrentMeasurement> Keithley6482::getLatestMeasurement() const {
    std::lock_guard<std::mutex> lock(measurement_mutex_);

    if (latest_measurement_.timestamp.time_since_epoch().count() == 0) {
      return std::nullopt;
    }

    return latest_measurement_;
  }

  // Scan available resources
  std::vector<std::string> Keithley6482::scanAvailableResources() {
    std::vector<std::string> resources;

    try {
      ViSession defaultRM;
      ViStatus status = viOpenDefaultRM(&defaultRM);
      if (status != VI_SUCCESS) {
        return resources;
      }

      ViFindList findList;
      ViUInt32 retCount;
      ViChar desc[VI_FIND_BUFLEN];

      status = viFindRsrc(defaultRM, (ViString)"?*", &findList, &retCount, desc);
      if (status == VI_SUCCESS) {
        resources.push_back(std::string(desc));

        for (ViUInt32 i = 1; i < retCount; i++) {
          status = viFindNext(findList, desc);
          if (status == VI_SUCCESS) {
            resources.push_back(std::string(desc));
          }
        }
        viClose(findList);
      }

      viClose(defaultRM);

    }
    catch (const std::exception& e) {
      std::cerr << "Exception during resource scan: " << e.what() << std::endl;
    }

    return resources;
  }

  // Set timeout
  bool Keithley6482::setTimeout(int timeout_ms) {
    if (!is_connected_.load() || !pimpl_->visa_initialized) {
      return false;
    }

    ViStatus status = viSetAttribute(pimpl_->instrument, VI_ATTR_TMO_VALUE, timeout_ms);
    return status == VI_SUCCESS;
  }

  // === Private Helper Methods ===

  // Send command
  bool Keithley6482::sendCommand(const std::string& command) const {
    if (!is_connected_.load()) {
      setError("Not connected");
      return false;
    }

    std::lock_guard<std::mutex> lock(visa_mutex_);

    ViUInt32 written;
    ViStatus status = viWrite(pimpl_->instrument,
      (ViBuf)command.c_str(),
      (ViUInt32)command.length(),
      &written);

    if (status != VI_SUCCESS) {
      setError("Failed to send command: " + command);
      return false;
    }

    return true;
  }

  // Internal query without lock (must be called with visa_mutex_ already locked)
  std::string Keithley6482::queryInternal(const std::string& command) const {
    if (!pimpl_->visa_initialized) {
      setError("VISA not initialized");
      return "";
    }

    // Send command
    ViUInt32 written;
    ViStatus status = viWrite(pimpl_->instrument,
      (ViBuf)command.c_str(),
      (ViUInt32)command.length(),
      &written);

    if (status != VI_SUCCESS) {
      setError("Failed to send query: " + command);
      return "";
    }

    // Read response
    ViChar buffer[1024];
    ViUInt32 read;
    status = viRead(pimpl_->instrument, (ViBuf)buffer, sizeof(buffer) - 1, &read);

    if (status != VI_SUCCESS && status != VI_SUCCESS_TERM_CHAR) {
      setError("Failed to read response for: " + command);
      return "";
    }

    buffer[read] = '\0';

    // Remove trailing whitespace
    std::string response(buffer);
    response.erase(response.find_last_not_of(" \n\r\t") + 1);

    return response;
  }

  // Query instrument (public version with lock)
  std::string Keithley6482::query(const std::string& command) const {
    if (!is_connected_.load()) {
      setError("Not connected");
      return "";
    }

    std::lock_guard<std::mutex> lock(visa_mutex_);
    return queryInternal(command);
  }

  // Validate channel
  bool Keithley6482::validateChannel(int channel) const {
    if (channel < 1 || channel > 2) {
      setError("Invalid channel: " + std::to_string(channel));
      return false;
    }
    return true;
  }

  // Parse double response
  std::optional<double> Keithley6482::parseDoubleResponse(const std::string& response) const {
    if (response.empty()) {
      return std::nullopt;
    }

    try {
      return std::stod(response);
    }
    catch (const std::exception&) {
      setError("Failed to parse response: " + response);
      return std::nullopt;
    }
  }

  // Polling thread function
  void Keithley6482::pollingThreadFunction() {
    while (polling_active_.load()) {
      if (is_connected_.load()) {
        CurrentMeasurement measurement;
        measurement.timestamp = std::chrono::steady_clock::now();

        // Read both channels
        auto ch1_current = readCurrent(1);
        auto ch2_current = readCurrent(2);
        auto ch1_voltage = getSourceVoltage(1);
        auto ch2_voltage = getSourceVoltage(2);

        if (ch1_current && ch2_current && ch1_voltage && ch2_voltage) {
          measurement.channel1_current = *ch1_current;
          measurement.channel2_current = *ch2_current;
          measurement.channel1_voltage = *ch1_voltage;
          measurement.channel2_voltage = *ch2_voltage;

          std::lock_guard<std::mutex> lock(measurement_mutex_);
          latest_measurement_ = measurement;
        }
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(polling_interval_.load()));
    }
  }

  // Set error
  void Keithley6482::setError(const std::string& error) const {
    last_error_ = error;
    std::cerr << "Keithley6482 Error: " << error << std::endl;
  }

} // namespace Keithley