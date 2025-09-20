#include "SPDPowerSupply.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <vector>
#include <numeric>
#include <algorithm>
#include <sstream>
#include <string>

using namespace PowerSupply;

/**
 * @brief Test result tracking
 */
struct TestResult {
    std::string test_name;
    bool success;
    std::string details;
    std::chrono::steady_clock::time_point timestamp;

    TestResult(const std::string& name, bool result, const std::string& detail = "")
        : test_name(name), success(result), details(detail), timestamp(std::chrono::steady_clock::now()) {
    }
};

class TestRunner {
public:
    TestRunner(const std::string& device_resource)
        : device_resource_(device_resource), ps_(device_resource) {
        start_time_ = std::chrono::steady_clock::now();
    }

    void addResult(const std::string& test_name, bool success, const std::string& details = "") {
        results_.emplace_back(test_name, success, details);

        std::string status = success ? "✓ PASS" : "✗ FAIL";
        std::cout << status << ": " << test_name << std::endl;
        if (!details.empty()) {
            std::cout << "     " << details << std::endl;
        }
    }

    void printSummary() {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time_);

        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "C++ POWER SUPPLY TEST RESULTS SUMMARY" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        std::cout << "Test Duration: " << std::fixed << std::setprecision(1)
            << duration.count() / 1000.0 << " seconds" << std::endl;

        int passed = std::count_if(results_.begin(), results_.end(),
            [](const TestResult& r) { return r.success; });
        int failed = results_.size() - passed;

        std::cout << "Total Tests: " << results_.size() << std::endl;
        std::cout << "Passed: " << passed << std::endl;
        std::cout << "Failed: " << failed << std::endl;
        std::cout << "Success Rate: " << std::fixed << std::setprecision(1)
            << (results_.empty() ? 0.0 : (100.0 * passed / results_.size())) << "%" << std::endl;
        std::cout << std::endl;

        std::cout << "DETAILED RESULTS:" << std::endl;
        std::cout << std::string(60, '-') << std::endl;

        for (size_t i = 0; i < results_.size(); ++i) {
            const auto& result = results_[i];
            std::string status = result.success ? "PASS" : "FAIL";
            std::cout << std::setw(2) << (i + 1) << ". [" << status << "] "
                << result.test_name << std::endl;
            if (!result.details.empty()) {
                std::cout << "     → " << result.details << std::endl;
            }
        }

        std::cout << std::string(60, '=') << std::endl;
    }

    void runAllTests() {
        std::cout << std::string(60, '=') << std::endl;
        std::cout << "AUTOMATED C++ POWER SUPPLY TEST" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        std::cout << "Device: " << device_resource_ << std::endl;
        std::cout << std::string(60, '=') << std::endl;

        try {
            // Test 1: Connection
            std::cout << "\n1. Testing Connection..." << std::endl;
            bool connected = ps_.connect();
            addResult("Connection Test", connected,
                connected ? "Successfully connected to power supply" : "Failed to connect");

            if (!connected) {
                std::cout << "Cannot continue tests without connection!" << std::endl;
                return;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Test 2: Get Instrument ID
            std::cout << "\n2. Testing Instrument Identification..." << std::endl;
            std::string idn = ps_.getInstrumentID();
            bool idn_success = !idn.empty();
            addResult("Instrument ID Query", idn_success,
                idn_success ? "ID: " + idn : "Failed to get instrument ID");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Test 3: Set Voltage (CH1)
            std::cout << "\n3. Testing Set Voltage (CH1 = 3.3V)..." << std::endl;
            bool voltage_set = ps_.setVoltage(1, 3.3);
            addResult("Set Voltage CH1", voltage_set,
                voltage_set ? "Set CH1 to 3.3V" : "Failed to set voltage");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Test 4: Set Current Limit (CH1)
            std::cout << "\n4. Testing Set Current Limit (CH1 = 0.5A)..." << std::endl;
            bool current_set = ps_.setCurrent(1, 0.5);
            addResult("Set Current CH1", current_set,
                current_set ? "Set CH1 current limit to 0.5A" : "Failed to set current");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Test 5: Turn Output ON (CH1)
            std::cout << "\n5. Testing Turn Output ON (CH1)..." << std::endl;
            bool output_on = ps_.setOutput(1, true);
            addResult("Turn Output ON CH1", output_on,
                output_on ? "CH1 output enabled" : "Failed to enable output");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Wait for output to stabilize

            // Test 6: Read Voltage/Current with 5-second polling (CH1)
            std::cout << "\n6. Testing Read Voltage/Current (CH1) - Polling for 5 seconds..." << std::endl;
            testPollingMeasurements(1, 5);

            // Test 7: Turn Output OFF (CH1)
            std::cout << "\n7. Testing Turn Output OFF (CH1)..." << std::endl;
            bool output_off = ps_.setOutput(1, false);
            addResult("Turn Output OFF CH1", output_off,
                output_off ? "CH1 output disabled" : "Failed to disable output");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Test 8: Setup Channel Function (CH1 only - this model has single channel)
            std::cout << "\n8. Testing Setup Channel Function (CH1 = 5V, 1A)..." << std::endl;
            bool setup_success = ps_.setupChannel(1, 5.0, 1.0, false);
            addResult("Setup Channel CH1", setup_success,
                setup_success ? "Setup CH1: 5V, 1A limit" : "Failed to setup channel");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Test 9: Get Channel Status (CH1 only)
            std::cout << "\n9. Testing Get Channel Status (CH1 only)..." << std::endl;
            testChannelStatus(1);

            // Test 10: Monitor Channel Function
            std::cout << "\n10. Testing Monitor Channel Function (CH1: 5V, 0.2A for 3 seconds)..." << std::endl;
            testMonitorChannel();

            // Test 11: SafeOutputControl RAII
            std::cout << "\n11. Testing SafeOutputControl RAII..." << std::endl;
            testSafeOutputControl();

            // Test 12: Emergency Stop
            std::cout << "\n12. Testing Emergency Stop..." << std::endl;
            bool emergency_stop = ps_.emergencyStop();
            addResult("Emergency Stop", emergency_stop,
                emergency_stop ? "All outputs disabled" : "Failed to disable all outputs");

        }
        catch (const std::exception& e) {
            std::cout << "\nUnexpected exception during testing: " << e.what() << std::endl;
            addResult("Test Completion", false, "Exception: " + std::string(e.what()));
        }

        // Always disconnect
        std::cout << "\nDisconnecting from power supply..." << std::endl;
        ps_.disconnect();
        addResult("Disconnection", true, "Safely disconnected from power supply");

        printSummary();
    }

private:
    std::string device_resource_;
    SPDPowerSupply ps_;
    std::vector<TestResult> results_;
    std::chrono::steady_clock::time_point start_time_;

    void testPollingMeasurements(int channel, int duration_seconds) {
        std::vector<double> voltages, currents;
        bool polling_success = true;

        for (int i = 0; i < duration_seconds; ++i) {
            auto voltage = ps_.getVoltage(channel);
            auto current = ps_.getCurrent(channel);

            if (voltage && current) {
                voltages.push_back(*voltage);
                currents.push_back(*current);
                std::cout << "     T+" << (i + 1) << "s: V=" << std::fixed << std::setprecision(3)
                    << *voltage << "V, I=" << *current << "A" << std::endl;
            }
            else {
                std::cout << "     T+" << (i + 1) << "s: Error reading measurements" << std::endl;
                polling_success = false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

        if (polling_success && !voltages.empty()) {
            double avg_voltage = std::accumulate(voltages.begin(), voltages.end(), 0.0) / voltages.size();
            double avg_current = std::accumulate(currents.begin(), currents.end(), 0.0) / currents.size();

            std::ostringstream details;
            details << duration_seconds << "s polling complete. Avg: V="
                << std::fixed << std::setprecision(3) << avg_voltage
                << "V, I=" << avg_current << "A";
            addResult("Read Measurements CH" + std::to_string(channel) + " (5s polling)",
                true, details.str());
        }
        else {
            addResult("Read Measurements CH" + std::to_string(channel) + " (5s polling)",
                false, "Polling failed - some readings were invalid");
        }
    }

    void testChannelStatus(int channel) {
        // Ensure output is ON for accurate readings
        ps_.setOutput(channel, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        ChannelStatus status = ps_.getChannelStatus(channel);
        bool success = status.isValid();

        std::ostringstream details;
        if (success) {
            details << "CH" << channel << ": " << std::fixed << std::setprecision(2)
                << *status.voltage << "V, " << std::setprecision(3) << *status.current
                << "A, " << (*status.output_enabled ? "ON" : "OFF");
        }
        else {
            details << "CH" << channel << ": Error reading status - ";
            if (!status.voltage.has_value()) details << "voltage query failed ";
            if (!status.current.has_value()) details << "current query failed ";
            if (!status.output_enabled.has_value()) details << "output state query failed ";
        }

        addResult("Get Status CH" + std::to_string(channel), success, details.str());

        // Turn off after test
        ps_.setOutput(channel, false);
    }

    void testMonitorChannel() {
        // Setup channel for monitoring - use lower current for safety
        bool setup = ps_.setupChannel(1, 5.0, 0.2, true);
        if (!setup) {
            addResult("Monitor Channel Test", false, "Failed to setup test parameters");
            return;
        }

        std::cout << "   Output enabled, waiting for stabilization..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(2000)); // Wait longer for stabilization

        // Monitor for 3 seconds with 1-second intervals
        auto samples = ps_.monitorChannel(1, 3.0, 1000);

        // Turn off output
        ps_.setOutput(1, false);

        if (!samples.empty()) {
            std::cout << "   Collected " << samples.size() << " samples:" << std::endl;
            for (size_t i = 0; i < samples.size(); ++i) {
                std::cout << "     Sample " << (i + 1) << ": "
                    << std::fixed << std::setprecision(2) << samples[i].voltage
                    << "V, " << std::setprecision(3) << samples[i].current << "A" << std::endl;
            }

            // Calculate averages
            double avg_v = 0, avg_i = 0;
            for (const auto& sample : samples) {
                avg_v += sample.voltage;
                avg_i += sample.current;
            }
            avg_v /= samples.size();
            avg_i /= samples.size();

            std::ostringstream details;
            details << "Collected " << samples.size() << " samples. Avg: "
                << std::fixed << std::setprecision(3) << avg_v << "V, " << avg_i << "A";
            addResult("Monitor Channel Test", true, details.str());
        }
        else {
            addResult("Monitor Channel Test", false, "No samples collected - check if monitoring function is working correctly");
        }
    }

    void testSafeOutputControl() {
        try {
            {
                // RAII scope - output should be automatically turned off when exiting scope
                SafeOutputControl safe_control(ps_, 1, 3.3, 0.1);
                std::cout << "   SafeOutputControl created" << std::endl;

                // Wait a bit for the setup to complete
                std::this_thread::sleep_for(std::chrono::milliseconds(200));

                // Check if output is enabled
                auto output_state = ps_.getOutputState(1);
                if (output_state && *output_state) {
                    std::cout << "   Output is ON as expected" << std::endl;
                }
                else {
                    std::cout << "   Warning: Output should be ON but appears OFF" << std::endl;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            } // SafeOutputControl goes out of scope here - should auto-disable output

            std::cout << "   SafeOutputControl destroyed" << std::endl;

            // Verify output is now off
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            auto output_state = ps_.getOutputState(1);
            bool success = output_state.has_value() && !(*output_state);

            if (!output_state.has_value()) {
                addResult("SafeOutputControl RAII", false, "Failed to query output state after RAII");
            }
            else {
                addResult("SafeOutputControl RAII", success,
                    success ? "Output automatically disabled on scope exit" :
                    "Output still enabled - RAII failed to disable automatically");
            }

        }
        catch (const std::exception& e) {
            addResult("SafeOutputControl RAII", false, "Exception: " + std::string(e.what()));
        }
    }
};

int main() {
    // Your device resource string
    const std::string RESOURCE_STRING = "USB0::0xF4EC::0x1410::SPD13DCQ7R0719::INSTR";

    std::cout << "Starting automated C++ power supply tests..." << std::endl;
    std::cout << "This will test all functions automatically and show results.\n" << std::endl;

    try {
        TestRunner runner(RESOURCE_STRING);
        runner.runAllTests();

        std::cout << "\nC++ Test completed successfully!" << std::endl;
        std::cout << "Check the detailed results above." << std::endl;

    }
    catch (const std::exception& e) {
        std::cerr << "\nFatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}