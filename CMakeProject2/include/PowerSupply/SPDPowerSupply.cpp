#include "SPDPowerSupply.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <stdexcept>
#include <algorithm>

// VISA includes
#ifdef _WIN32
#include <visa.h>
//#pragma comment(lib, "visa32.lib")
#else
#include <visa.h>
#endif

namespace PowerSupply {

	// PIMPL implementation to hide VISA details
	class SPDPowerSupply::Impl {
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

	// 1. Constructor - change this line:
	SPDPowerSupply::SPDPowerSupply(const std::string& resource_string)
		: resource_string_(resource_string)
		, is_connected_(false)  // atomic initialization
		, pimpl_(std::make_unique<Impl>()) {
	}

	SPDPowerSupply::~SPDPowerSupply() {
		disconnect();
	}

	bool SPDPowerSupply::SetDebug(bool debugmode)
	{
		m_debugverbose = debugmode;
		return m_debugverbose;
	}

	std::vector<std::string> SPDPowerSupply::scanAvailableResources() {
		std::vector<std::string> resources;

		try {
			ViSession defaultRM;
			ViStatus status = viOpenDefaultRM(&defaultRM);
			if (status != VI_SUCCESS) {
				std::cerr << "Failed to open VISA Resource Manager: " << std::hex << status << std::endl;
				return resources;
			}

			ViFindList findList;
			ViUInt32 retCount;
			ViChar desc[VI_FIND_BUFLEN];

			// Search for all VISA resources
			status = viFindRsrc(defaultRM, (ViString)"?*", &findList, &retCount, desc);
			if (status == VI_SUCCESS) {
				resources.push_back(std::string(desc));

				// Get additional resources
				for (ViUInt32 i = 1; i < retCount; i++) {
					status = viFindNext(findList, desc);
					if (status == VI_SUCCESS) {
						resources.push_back(std::string(desc));
					}
				}
				viClose(findList);
			}
			else {
				std::cerr << "No VISA resources found: " << std::hex << status << std::endl;
			}

			viClose(defaultRM);

		}
		catch (const std::exception& e) {
			std::cerr << "Exception during resource scan: " << e.what() << std::endl;
		}

		return resources;
	}

	// 2. Move constructor - change these lines:
	SPDPowerSupply::SPDPowerSupply(SPDPowerSupply&& other) noexcept
		: resource_string_(std::move(other.resource_string_))
		, is_connected_(other.is_connected_.load())  // Change from: other.is_connected_
		, pimpl_(std::move(other.pimpl_)) {
		other.is_connected_.store(false);  // Change from: other.is_connected_ = false;
	}

	// 3. Move assignment - change these lines:
	SPDPowerSupply& SPDPowerSupply::operator=(SPDPowerSupply&& other) noexcept {
		if (this != &other) {
			disconnect();
			resource_string_ = std::move(other.resource_string_);
			is_connected_.store(other.is_connected_.load());  // Change from: is_connected_ = other.is_connected_;
			pimpl_ = std::move(other.pimpl_);
			other.is_connected_.store(false);  // Change from: other.is_connected_ = false;
		}
		return *this;
	}

	bool SPDPowerSupply::connect(const std::string& resource_string) {
		if (is_connected_.load()) {  // Change from: if (is_connected_)
			return true;
		}

		// Update resource string if provided
		if (!resource_string.empty()) {
			resource_string_ = resource_string;
		}

		// Check if we have a valid resource string
		if (resource_string_.empty()) {
			std::cerr << "ERROR: No resource string provided. Use either constructor parameter or connect() parameter." << std::endl;
			return false;
		}

		if (m_debugverbose) std::cout << "DEBUG: Attempting to connect to: " << resource_string_ << std::endl;

		try {
			// Initialize VISA
			if (m_debugverbose) std::cout << "DEBUG: Opening default resource manager..." << std::endl;
			ViStatus status = viOpenDefaultRM(&pimpl_->default_rm);
			if (status != VI_SUCCESS) {
				std::cerr << "DEBUG: Failed to initialize VISA: 0x" << std::hex << status << std::endl;
				return false;
			}
			if (m_debugverbose) std::cout << "DEBUG: VISA resource manager opened successfully" << std::endl;

			pimpl_->visa_initialized = true;

			// Open instrument connection
			if (m_debugverbose) std::cout << "DEBUG: Opening instrument connection..." << std::endl;
			status = viOpen(pimpl_->default_rm,
				resource_string_.c_str(),
				VI_NULL,
				VI_NULL,
				&pimpl_->instrument);

			if (status != VI_SUCCESS) {
				std::cerr << "DEBUG: Failed to connect to instrument: 0x" << std::hex << status << std::endl;
				viClose(pimpl_->default_rm);
				pimpl_->default_rm = VI_NULL;
				pimpl_->visa_initialized = false;
				return false;
			}
			if (m_debugverbose) std::cout << "DEBUG: Instrument connection opened successfully" << std::endl;

			// Set connection flag BEFORE calling other methods that need it
			is_connected_.store(true);  // Change from: is_connected_ = true;

			// Set default timeout (10 seconds)
			if (m_debugverbose) std::cout << "DEBUG: Setting timeout..." << std::endl;
			setTimeout(10000);

			// Test connection by getting instrument ID
			if (m_debugverbose) std::cout << "DEBUG: Testing connection with *IDN? query..." << std::endl;
			std::string idn = query("*IDN?");
			if (idn.empty()) {
				std::cerr << "DEBUG: Failed to get instrument identification - query returned empty string" << std::endl;
				disconnect();
				return false;
			}

			std::cout << "DEBUG: Successfully connected to: " << idn << std::endl;
			return true;

		}
		catch (const std::exception& e) {
			std::cerr << "DEBUG: Exception during connection: " << e.what() << std::endl;
			disconnect();
			return false;
		}
	}


	void SPDPowerSupply::disconnect() {
		std::lock_guard<std::mutex> lock(visa_mutex_);  // Add this line

		if (pimpl_) {
			pimpl_->cleanup();
		}
		is_connected_.store(false);  // Change from: is_connected_ = false;
	}

	std::string SPDPowerSupply::getInstrumentID() const {
		return query("*IDN?");
	}

	bool SPDPowerSupply::setVoltage(int channel, double voltage) {
		if (!validateChannel(channel)) return false;

		// Use CH1:VOLTage format as shown in manual
		std::ostringstream cmd;
		cmd << "CH" << channel << ":VOLTage " << std::fixed << voltage;

		bool success = sendCommand(cmd.str());
		if (success && m_debugverbose) {
			if(m_debugverbose) std::cout << "Set CH" << channel << " voltage to " << voltage << "V" << std::endl;
		}
		return success;
	}

	bool SPDPowerSupply::setCurrent(int channel, double current) {
		if (!validateChannel(channel)) return false;

		// Use CH1:CURRent format as shown in manual

		std::ostringstream cmd;
		cmd << "CH" << channel << ":CURRent " << std::fixed << current;

		bool success = sendCommand(cmd.str());
		if (success && m_debugverbose) {
			std::cout << "Set CH" << channel << " current limit to " << current << "A" << std::endl;
		}
		return success;
	}

	bool SPDPowerSupply::setOutput(int channel, bool enabled) {
		if (!validateChannel(channel)) return false;

		// Use OUTPut CH1,ON format as shown in manual
		std::ostringstream cmd;
		cmd << "OUTPut CH" << channel << "," << (enabled ? "ON" : "OFF");

		bool success = sendCommand(cmd.str());
		if (success && m_debugverbose) {
			std::cout << "CH" << channel << " output: " << (enabled ? "ON" : "OFF") << std::endl;
		}
		return success;
	}

	std::optional<double> SPDPowerSupply::getVoltage(int channel) const {
		if (!validateChannel(channel)) return std::nullopt;

		// Use MEASure:VOLTage? CH1 format as shown in manual
		std::ostringstream cmd;
		cmd << "MEASure:VOLTage? CH" << channel;

		std::string response = query(cmd.str());
		return parseDoubleResponse(response);
	}

	std::optional<double> SPDPowerSupply::getCurrent(int channel) const {
		if (!validateChannel(channel)) return std::nullopt;

		// Use MEASure:CURRent? CH1 format as shown in manual
		std::ostringstream cmd;
		cmd << "MEASure:CURRent? CH" << channel;

		std::string response = query(cmd.str());
		return parseDoubleResponse(response);
	}

	std::optional<bool> SPDPowerSupply::getOutputState(int channel) const {
		if (!validateChannel(channel)) return std::nullopt;

		// Add rate limiting - prevent excessive queries
		static std::chrono::steady_clock::time_point last_query;
		static std::mutex rate_mutex;

		{
			std::lock_guard<std::mutex> rate_lock(rate_mutex);
			auto now = std::chrono::steady_clock::now();
			if (now - last_query < std::chrono::milliseconds(200)) {  // Max 5 queries/second
				return std::nullopt;  // Skip if queried too recently
			}
			last_query = now;
		}

		// Add small delay to ensure device status is current
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		std::string response = query("SYSTem:STATus?");
		if (response.empty()) {
			std::cerr << "Failed to get system status" << std::endl;
			return std::nullopt;
		}

		try {
			// Response is in hex format like "0x0224"
			uint32_t status = std::stoul(response, nullptr, 16);

			// From manual: Bit 4: 0=Output OFF, 1=Output ON
			bool outputOn = (status & 0x10) != 0;

			if(m_debugverbose) std::cout << "DEBUG: System status: 0x" << std::hex << status
				<< ", Output state: " << (outputOn ? "ON" : "OFF") << std::endl;

			return outputOn;
		}
		catch (const std::exception& ) {
			std::cerr << "Failed to parse system status: " << response << std::endl;
			return std::nullopt;
		}
	}

	ChannelStatus SPDPowerSupply::getChannelStatus(int channel) const {
		ChannelStatus status;
		status.voltage = getVoltage(channel);
		status.current = getCurrent(channel);
		status.output_enabled = getOutputState(channel);
		return status;
	}

	bool SPDPowerSupply::setupChannel(int channel, double voltage, double current, bool enable) {
		bool success = true;
		success &= setVoltage(channel, voltage);
		success &= setCurrent(channel, current);
		if (enable) {
			success &= setOutput(channel, true);
		}
		return success;
	}

	std::vector<MeasurementSample> SPDPowerSupply::monitorChannel(int channel,
		double duration_seconds,
		int sample_interval_ms) const {
		std::vector<MeasurementSample> samples;

		if (!validateChannel(channel)) {
			return samples;
		}

		auto start_time = std::chrono::steady_clock::now();
		auto duration = std::chrono::milliseconds(static_cast<int>(duration_seconds * 1000));
		auto sample_interval = std::chrono::milliseconds(sample_interval_ms);

		while (std::chrono::steady_clock::now() - start_time < duration) {
			auto timestamp = std::chrono::steady_clock::now();

			auto voltage = getVoltage(channel);
			auto current = getCurrent(channel);
			auto output_state = getOutputState(channel);

			if (voltage && current && output_state) {
				samples.push_back({
						timestamp,
						*voltage,
						*current,
						*output_state
					});
			}

			std::this_thread::sleep_for(sample_interval);
		}

		return samples;
	}

	bool SPDPowerSupply::emergencyStop() {
		bool success = true;
		// For single channel device (SPD1305X), only turn off channel 1
		success &= setOutput(1, false);
		return success;
	}

	bool SPDPowerSupply::setTimeout(int timeout_ms) {
		if (!is_connected_.load() || !pimpl_->visa_initialized) {  // Change from: !is_connected_
			return false;
		}

		ViStatus status = viSetAttribute(pimpl_->instrument, VI_ATTR_TMO_VALUE, timeout_ms);
		return status == VI_SUCCESS;
	}

	bool SPDPowerSupply::sendCommand(const std::string& command) {
		if (!is_connected_.load() || !pimpl_->visa_initialized) {  // Change from: !is_connected_
			std::cerr << "Not connected to power supply" << std::endl;
			return false;
		}

		try {
			ViUInt32 bytes_written;
			std::string cmd = command + "\n";

			ViStatus status = viWrite(pimpl_->instrument,
				reinterpret_cast<ViBuf>(const_cast<char*>(cmd.c_str())),
				static_cast<ViUInt32>(cmd.length()),
				&bytes_written);

			if (status != VI_SUCCESS) {
				std::cerr << "Failed to send command: " << command << " (status: " << std::hex << status << ")" << std::endl;
				return false;
			}

			return bytes_written == cmd.length();

		}
		catch (const std::exception& e) {
			std::cerr << "Exception sending command: " << e.what() << std::endl;
			return false;
		}
	}

	std::string SPDPowerSupply::query(const std::string& query_cmd) const {
		if (!is_connected_.load() || !pimpl_->visa_initialized) {  // Change from: !is_connected_
			std::cerr << "DEBUG: Query called but not connected (is_connected_="
				<< is_connected_.load() << ", visa_initialized=" << pimpl_->visa_initialized << ")" << std::endl;
			return "";
		}

		if(m_debugverbose) std::cout << "DEBUG: Sending query: " << query_cmd << std::endl;

		try {
			// Send query
			std::string cmd = query_cmd + "\n";
			ViUInt32 bytes_written;

			ViStatus status = viWrite(pimpl_->instrument,
				reinterpret_cast<ViBuf>(const_cast<char*>(cmd.c_str())),
				static_cast<ViUInt32>(cmd.length()),
				&bytes_written);

			if (status != VI_SUCCESS) {
				std::cerr << "DEBUG: Failed to send query: 0x" << std::hex << status << std::endl;
				return "";
			}
			if (m_debugverbose) std::cout << "DEBUG: Query sent, " << bytes_written << " bytes written" << std::endl;

			// Read response
			char buffer[1024];
			ViUInt32 bytes_read;

			status = viRead(pimpl_->instrument,
				reinterpret_cast<ViBuf>(buffer),
				sizeof(buffer) - 1,
				&bytes_read);

			if (status != VI_SUCCESS) {
				std::cerr << "DEBUG: Failed to read query response: 0x" << std::hex << status << std::endl;
				return "";
			}

			buffer[bytes_read] = '\0';
			if(m_debugverbose) std::cout << "DEBUG: Received " << bytes_read << " bytes: " << buffer << std::endl;

			// Remove trailing whitespace
			std::string result(buffer);
			result.erase(std::find_if(result.rbegin(), result.rend(),
				[](unsigned char ch) { return !std::isspace(ch); }).base(),
				result.end());

			return result;

		}
		catch (const std::exception& e) {
			std::cerr << "DEBUG: Exception during query: " << e.what() << std::endl;
			return "";
		}
	}

	bool SPDPowerSupply::validateChannel(int channel) const {
		if (!is_connected_.load()) {  // Change from: !is_connected_
			std::cerr << "Not connected to power supply" << std::endl;
			return false;
		}

		// SPD1305X is single channel, so only channel 1 is valid
		if (channel != 1) {
			std::cerr << "Invalid channel number: " << channel << " (SPD1305X only supports channel 1)" << std::endl;
			return false;
		}

		return true;
	}

	std::optional<double> SPDPowerSupply::parseDoubleResponse(const std::string& response) const {
		if (response.empty()) {
			return std::nullopt;
		}

		try {
			return std::stod(response);
		}
		catch (const std::exception&) {
			std::cerr << "Failed to parse double from response: " << response << std::endl;
			return std::nullopt;
		}
	}

	std::optional<bool> SPDPowerSupply::parseBoolResponse(const std::string& response) const {
		if (response.empty()) {
			return std::nullopt;
		}

		std::string trimmed = response;
		// Convert to uppercase for comparison
		std::transform(trimmed.begin(), trimmed.end(), trimmed.begin(), ::toupper);

		if (trimmed == "ON" || trimmed == "1") {
			return true;
		}
		else if (trimmed == "OFF" || trimmed == "0") {
			return false;
		}

		std::cerr << "Failed to parse boolean from response: " << response << std::endl;
		return std::nullopt;
	}


	// Add these to SPDPowerSupply.cpp

	bool SPDPowerSupply::voltageSweep(int channel, double startV, double stopV, int steps,
		double currentLimit, double delayMs,
		std::vector<SPDSweepResult>& results) {
		if (m_debugverbose) {
			std::cout << "SPD Voltage Sweep: " << startV << "V to " << stopV << "V, "
				<< steps << " steps, " << currentLimit << "A limit" << std::endl;
		}

		// Validate inputs
		if (!validateChannel(channel)) {
			std::cerr << "SPD Voltage Sweep: Invalid channel " << channel << std::endl;
			return false;
		}

		if (steps < 2) {
			std::cerr << "SPD Voltage Sweep: Need at least 2 steps" << std::endl;
			return false;
		}

		if (delayMs < 0) {
			std::cerr << "SPD Voltage Sweep: Delay must be >= 0" << std::endl;
			return false;
		}

		try {
			results.clear();
			results.reserve(steps);

			// Set current limit first
			if (!setCurrent(channel, currentLimit)) {
				std::cerr << "SPD Voltage Sweep: Failed to set current limit" << std::endl;
				return false;
			}

			// Calculate voltage step size
			double stepSize = (stopV - startV) / (steps - 1);

			// Perform sweep
			for (int i = 0; i < steps; ++i) {
				double targetVoltage = startV + (stepSize * i);

				// Set voltage for this step
				if (!setVoltage(channel, targetVoltage)) {
					std::cerr << "SPD Voltage Sweep: Failed to set voltage at step " << i << std::endl;
					return false;
				}

				// Enable output if not already enabled
				if (!setOutput(channel, true)) {
					std::cerr << "SPD Voltage Sweep: Failed to enable output at step " << i << std::endl;
					return false;
				}

				// Wait for settling if delay specified
				if (delayMs > 0) {
					std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(delayMs)));
				}

				// Take measurements
				auto voltage = getVoltage(channel);
				auto current = getCurrent(channel);
				auto timestamp = std::chrono::steady_clock::now();

				if (voltage && current) {
					SPDSweepResult result;
					result.setValue = targetVoltage;
					result.measuredVoltage = *voltage;
					result.measuredCurrent = *current;
					result.timestamp = timestamp;
					results.push_back(result);

					if (m_debugverbose) {
						std::cout << "Step " << (i + 1) << "/" << steps
							<< ": Set=" << targetVoltage << "V"
							<< ", Meas=" << *voltage << "V, " << *current << "A" << std::endl;
					}
				}
				else {
					std::cerr << "SPD Voltage Sweep: Failed to read measurements at step " << i << std::endl;
					return false;
				}
			}

			if (m_debugverbose) {
				std::cout << "SPD Voltage Sweep completed successfully with "
					<< results.size() << " points" << std::endl;
			}

			return true;

		}
		catch (const std::exception& e) {
			std::cerr << "SPD Voltage Sweep: Exception - " << e.what() << std::endl;
			setOutput(channel, false); // Safety disable
			return false;
		}
	}

	bool SPDPowerSupply::currentSweep(int channel, double startA, double stopA, int steps,
		double voltageLimit, double delayMs,
		std::vector<SPDSweepResult>& results) {
		if (m_debugverbose) {
			std::cout << "SPD Current Sweep: " << startA << "A to " << stopA << "A, "
				<< steps << " steps, " << voltageLimit << "V limit" << std::endl;
		}

		// Validate inputs
		if (!validateChannel(channel)) {
			std::cerr << "SPD Current Sweep: Invalid channel " << channel << std::endl;
			return false;
		}

		if (steps < 2) {
			std::cerr << "SPD Current Sweep: Need at least 2 steps" << std::endl;
			return false;
		}

		if (delayMs < 0) {
			std::cerr << "SPD Current Sweep: Delay must be >= 0" << std::endl;
			return false;
		}

		try {
			results.clear();
			results.reserve(steps);

			// Set voltage limit first
			if (!setVoltage(channel, voltageLimit)) {
				std::cerr << "SPD Current Sweep: Failed to set voltage limit" << std::endl;
				return false;
			}

			// Calculate current step size
			double stepSize = (stopA - startA) / (steps - 1);

			// Perform sweep
			for (int i = 0; i < steps; ++i) {
				double targetCurrent = startA + (stepSize * i);

				// Set current for this step
				if (!setCurrent(channel, targetCurrent)) {
					std::cerr << "SPD Current Sweep: Failed to set current at step " << i << std::endl;
					return false;
				}

				// Enable output if not already enabled
				if (!setOutput(channel, true)) {
					std::cerr << "SPD Current Sweep: Failed to enable output at step " << i << std::endl;
					return false;
				}

				// Wait for settling if delay specified
				if (delayMs > 0) {
					std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(delayMs)));
				}

				// Take measurements
				auto voltage = getVoltage(channel);
				auto current = getCurrent(channel);
				auto timestamp = std::chrono::steady_clock::now();

				if (voltage && current) {
					SPDSweepResult result;
					result.setValue = targetCurrent;
					result.measuredVoltage = *voltage;
					result.measuredCurrent = *current;
					result.timestamp = timestamp;
					results.push_back(result);

					if (m_debugverbose) {
						std::cout << "Step " << (i + 1) << "/" << steps
							<< ": Set=" << targetCurrent << "A"
							<< ", Meas=" << *voltage << "V, " << *current << "A" << std::endl;
					}
				}
				else {
					std::cerr << "SPD Current Sweep: Failed to read measurements at step " << i << std::endl;
					return false;
				}
			}

			if (m_debugverbose) {
				std::cout << "SPD Current Sweep completed successfully with "
					<< results.size() << " points" << std::endl;
			}

			return true;

		}
		catch (const std::exception& e) {
			std::cerr << "SPD Current Sweep: Exception - " << e.what() << std::endl;
			setOutput(channel, false); // Safety disable
			return false;
		}
	}

	bool SPDPowerSupply::setConstantVoltageMode(int channel, double voltage, double currentLimit) {
		if (!validateChannel(channel)) {
			std::cerr << "setConstantVoltageMode: Invalid channel " << channel << std::endl;
			return false;
		}

		if (!isConnected()) {
			std::cerr << "setConstantVoltageMode: Device not connected" << std::endl;
			return false;
		}

		try {
			// Set voltage
			if (!setVoltage(channel, voltage)) {
				std::cerr << "setConstantVoltageMode: Failed to set voltage" << std::endl;
				return false;
			}

			// Set current limit
			if (!setCurrent(channel, currentLimit)) {
				std::cerr << "setConstantVoltageMode: Failed to set current limit" << std::endl;
				return false;
			}

			if (m_debugverbose) {
				std::cout << "SPD CV mode set: " << voltage << "V, " << currentLimit << "A limit" << std::endl;
			}

			return true;
		}
		catch (const std::exception& e) {
			std::cerr << "setConstantVoltageMode: Exception - " << e.what() << std::endl;
			return false;
		}
	}

	bool SPDPowerSupply::setConstantCurrentMode(int channel, double current, double voltageLimit) {
		if (!validateChannel(channel)) {
			std::cerr << "setConstantCurrentMode: Invalid channel " << channel << std::endl;
			return false;
		}

		if (!isConnected()) {
			std::cerr << "setConstantCurrentMode: Device not connected" << std::endl;
			return false;
		}

		try {
			// Set current
			if (!setCurrent(channel, current)) {
				std::cerr << "setConstantCurrentMode: Failed to set current" << std::endl;
				return false;
			}

			// Set voltage limit
			if (!setVoltage(channel, voltageLimit)) {
				std::cerr << "setConstantCurrentMode: Failed to set voltage limit" << std::endl;
				return false;
			}

			if (m_debugverbose) {
				std::cout << "SPD CC mode set: " << current << "A, " << voltageLimit << "V limit" << std::endl;
			}

			return true;
		}
		catch (const std::exception& e) {
			std::cerr << "setConstantCurrentMode: Exception - " << e.what() << std::endl;
			return false;
		}
	}

	// SafeOutputControl implementation
	SafeOutputControl::SafeOutputControl(SPDPowerSupply& ps, int channel, double voltage, double current)
		: power_supply_(ps), channel_(channel), output_enabled_(false) {

		if (power_supply_.setupChannel(channel_, voltage, current, true)) {
			output_enabled_ = true;
			std::cout << "SafeOutputControl: CH" << channel_ << " enabled ("
				<< voltage << "V, " << current << "A)" << std::endl;
		}
		else {
			std::cerr << "SafeOutputControl: Failed to enable CH" << channel_ << std::endl;
		}
	}

	SafeOutputControl::~SafeOutputControl() {
		if (output_enabled_) {
			bool success = power_supply_.setOutput(channel_, false);
			if (success) {
				// Wait for the command to take effect and device status to update
				std::this_thread::sleep_for(std::chrono::milliseconds(150));
				std::cout << "SafeOutputControl: CH" << channel_ << " safely disabled" << std::endl;
			}
			else {
				std::cerr << "SafeOutputControl: Failed to disable CH" << channel_ << std::endl;
			}
		}
	}

} // namespace PowerSupply