// test_siglent_power_supply.cpp
#include "include/PowerSupplyDevice/Siglent/siglent_power_supply.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <vector>

// Color codes for terminal output
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define CYAN    "\033[36m"

class TestSiglentPowerSupply {
public:
  TestSiglentPowerSupply(const std::string& resource)
    : m_resource(resource), m_testsPassed(0), m_testsFailed(0) {
    m_ps = std::make_unique<SiglentPowerSupply>(resource);
  }

  void RunAllTests() {
    std::cout << CYAN << "\n=== SIGLENT POWER SUPPLY TEST SUITE ===" << RESET << std::endl;
    std::cout << "Resource: " << m_resource << std::endl;
    std::cout << "----------------------------------------\n" << std::endl;

    // Connection tests
    TestConnection();

    if (m_ps->IsConnected()) {
      // Device info test
      TestDeviceInfo();

      // Basic control tests
      TestVoltageControl();
      TestCurrentControl();
      TestOutputControl();

      // Mode tests
      TestConstantVoltagMode();
      TestConstantCurrentMode();

      // Read tests
      TestReadFunctions();

      // Sweep tests
      TestBlockingSweep();
      TestAsyncSweep();

      // Stress test
      TestRapidCommands();
    }

    // Print summary
    PrintTestSummary();
  }

private:
  std::unique_ptr<SiglentPowerSupply> m_ps;
  std::string m_resource;
  int m_testsPassed;
  int m_testsFailed;

  void PrintTestHeader(const std::string& testName) {
    std::cout << BLUE << "\n[TEST] " << testName << RESET << std::endl;
  }

  void PrintResult(bool passed, const std::string& message) {
    if (passed) {
      std::cout << GREEN << "  ✓ " << message << RESET << std::endl;
      m_testsPassed++;
    }
    else {
      std::cout << RED << "  ✗ " << message << RESET << std::endl;
      m_testsFailed++;
    }
  }

  void TestConnection() {
    PrintTestHeader("Connection Test");

    // Test initial state
    PrintResult(!m_ps->IsConnected(), "Initial state: disconnected");

    // Test connect
    bool connectResult = m_ps->Connect();
    PrintResult(connectResult, "Connect to device");

    if (connectResult) {
      PrintResult(m_ps->IsConnected(), "IsConnected returns true");

      // Test disconnect
      bool disconnectResult = m_ps->Disconnect();
      PrintResult(disconnectResult, "Disconnect from device");
      PrintResult(!m_ps->IsConnected(), "IsConnected returns false after disconnect");

      // Reconnect for other tests
      m_ps->Connect();
    }
  }

  void TestDeviceInfo() {
    PrintTestHeader("Device Info Test");

    auto info = m_ps->GetDeviceInfo();

    PrintResult(!info.name.empty(), "Device name: " + info.name);
    PrintResult(!info.model.empty(), "Model: " + info.model);
    PrintResult(!info.serialNumber.empty(), "Serial: " + info.serialNumber);
    PrintResult(!info.firmwareVersion.empty(), "Firmware: " + info.firmwareVersion);
  }

  void TestVoltageControl() {
    PrintTestHeader("Voltage Control Test");

    // Test setting different voltages
    std::vector<float> testVoltages = { 1.0f, 5.0f, 10.0f, 15.0f };

    for (float voltage : testVoltages) {
      bool result = m_ps->SetVoltage(voltage);
      PrintResult(result, "Set voltage to " + std::to_string(voltage) + "V");

      if (result) {
        // Give device time to settle
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Read back voltage
        float readVoltage = m_ps->ReadVoltage();
        float tolerance = 0.1f; // 100mV tolerance
        bool withinTolerance = std::abs(readVoltage - voltage) <= tolerance;

        std::stringstream ss;
        ss << "Read voltage: " << std::fixed << std::setprecision(3)
          << readVoltage << "V (set: " << voltage << "V)";
        PrintResult(withinTolerance, ss.str());
      }
    }
  }

  void TestCurrentControl() {
    PrintTestHeader("Current Control Test");

    // Test setting different current limits
    std::vector<float> testCurrents = { 0.1f, 0.5f, 1.0f, 2.0f };

    for (float current : testCurrents) {
      bool result = m_ps->SetCurrent(current);
      std::stringstream ss;
      ss << "Set current limit to " << std::fixed << std::setprecision(3)
        << current << "A";
      PrintResult(result, ss.str());
    }
  }

  void TestOutputControl() {
    PrintTestHeader("Output Control Test");

    // Turn off first
    bool offResult = m_ps->TurnOff();
    PrintResult(offResult, "Turn output OFF");

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Turn on
    bool onResult = m_ps->TurnOn();
    PrintResult(onResult, "Turn output ON");

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Turn off again
    offResult = m_ps->TurnOff();
    PrintResult(offResult, "Turn output OFF again");
  }

  void TestConstantVoltagMode() {
    PrintTestHeader("Constant Voltage Mode Test");

    // Set voltage and current first
    m_ps->SetVoltage(5.0f);
    m_ps->SetCurrent(1.0f);

    bool result = m_ps->SetModeConstantVoltage();
    PrintResult(result, "Set constant voltage mode");

    if (result) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      // Verify we can read voltage
      float voltage = m_ps->ReadVoltage();
      PrintResult(voltage >= 0, "Read voltage in CV mode: " +
        std::to_string(voltage) + "V");
    }
  }

  void TestConstantCurrentMode() {
    PrintTestHeader("Constant Current Mode Test");

    // Set voltage and current first
    m_ps->SetVoltage(10.0f);
    m_ps->SetCurrent(0.5f);

    bool result = m_ps->SetModeConstantCurrent();
    PrintResult(result, "Set constant current mode");

    if (result) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      // Verify we can read current
      float current = m_ps->ReadCurrent();
      PrintResult(current >= 0, "Read current in CC mode: " +
        std::to_string(current) + "A");
    }
  }

  void TestReadFunctions() {
    PrintTestHeader("Read Functions Test");

    // Set known values
    m_ps->SetVoltage(3.3f);
    m_ps->SetCurrent(0.5f);
    m_ps->TurnOn();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Test individual reads
    float voltage = m_ps->ReadVoltage();
    PrintResult(voltage >= 0, "ReadVoltage: " + std::to_string(voltage) + "V");

    float current = m_ps->ReadCurrent();
    PrintResult(current >= 0, "ReadCurrent: " + std::to_string(current) + "A");

    // Test combined read
    auto measurement = m_ps->ReadVoltageCurrent();
    std::stringstream ss;
    ss << "ReadVoltageCurrent: " << std::fixed << std::setprecision(3)
      << measurement.voltage << "V, " << measurement.current << "A";
    PrintResult(measurement.voltage >= 0 && measurement.current >= 0, ss.str());

    m_ps->TurnOff();
  }

  void TestBlockingSweep() {
    PrintTestHeader("Blocking Sweep Test");

    IPowerSupplyDevice::SweepConfig config;
    config.mode = IPowerSupplyDevice::SweepConfig::Mode::CONSTANT_VOLTAGE;
    config.startValue = 1.0f;
    config.endValue = 5.0f;
    config.stepSize = 1.0f;
    config.delayMs = 100;
    config.channel = 1;

    std::cout << "  Starting voltage sweep: 1V to 5V in 1V steps..." << std::endl;

    auto result = m_ps->ExecuteSweepBlocking(config);

    PrintResult(result.completed, "Sweep completed");
    PrintResult(result.errorMessage.empty(), "No errors reported");

    if (result.completed) {
      std::cout << "  Sweep results:" << std::endl;
      for (size_t i = 0; i < result.measurements.size(); ++i) {
        std::cout << "    Step " << (i + 1) << ": Set="
          << std::fixed << std::setprecision(2) << result.sweepValues[i] << "V"
          << ", Measured=" << result.measurements[i].voltage << "V, "
          << result.measurements[i].current << "A" << std::endl;
      }

      PrintResult(result.measurements.size() == 5,
        "Got expected number of measurements: " +
        std::to_string(result.measurements.size()));
    }
  }

  void TestAsyncSweep() {
    PrintTestHeader("Asynchronous Sweep Test");

    IPowerSupplyDevice::SweepConfig config;
    config.mode = IPowerSupplyDevice::SweepConfig::Mode::CONSTANT_CURRENT;
    config.startValue = 0.1f;
    config.endValue = 0.5f;
    config.stepSize = 0.1f;
    config.delayMs = 200;
    config.channel = 1;

    // Set voltage limit for current sweep
    m_ps->SetVoltage(10.0f);

    std::cout << "  Starting current sweep: 0.1A to 0.5A in 0.1A steps..." << std::endl;

    bool startResult = m_ps->StartSweep(config);
    PrintResult(startResult, "Sweep started");

    if (startResult) {
      PrintResult(m_ps->IsSweepRunning(), "IsSweepRunning returns true");

      // Monitor progress
      std::cout << "  Progress: ";
      while (m_ps->IsSweepRunning()) {
        float progress = m_ps->GetSweepProgress();
        std::cout << std::fixed << std::setprecision(0)
          << (progress * 100) << "% ";
        std::cout.flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
      }
      std::cout << std::endl;

      PrintResult(!m_ps->IsSweepRunning(), "Sweep finished");

      // Get results
      auto result = m_ps->GetSweepResults();
      PrintResult(result.completed || !result.errorMessage.empty(),
        "Got sweep results");

      if (result.completed) {
        std::cout << "  Sweep results:" << std::endl;
        for (size_t i = 0; i < result.measurements.size(); ++i) {
          std::cout << "    Step " << (i + 1) << ": Set="
            << std::fixed << std::setprecision(2) << result.sweepValues[i] << "A"
            << ", Measured=" << result.measurements[i].voltage << "V, "
            << result.measurements[i].current << "A" << std::endl;
        }
      }
    }

    // Test stop functionality
    std::cout << "\n  Testing sweep stop functionality..." << std::endl;
    config.delayMs = 1000; // Long delay to allow stopping
    config.endValue = 1.0f;

    if (m_ps->StartSweep(config)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1500));
      bool stopResult = m_ps->StopSweep();
      PrintResult(stopResult, "Sweep stopped successfully");
      PrintResult(!m_ps->IsSweepRunning(), "IsSweepRunning returns false after stop");
    }
  }

  void TestRapidCommands() {
    PrintTestHeader("Rapid Command Test (Stress Test)");

    std::cout << "  Sending rapid commands..." << std::endl;

    int successCount = 0;
    int totalCommands = 20;

    for (int i = 0; i < totalCommands; ++i) {
      float voltage = 1.0f + (i % 10) * 0.5f;
      if (m_ps->SetVoltage(voltage)) {
        successCount++;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    PrintResult(successCount == totalCommands,
      "Rapid voltage commands: " + std::to_string(successCount) +
      "/" + std::to_string(totalCommands) + " successful");
  }

  void PrintTestSummary() {
    std::cout << "\n" << CYAN << "=== TEST SUMMARY ===" << RESET << std::endl;
    std::cout << GREEN << "Passed: " << m_testsPassed << RESET << std::endl;
    std::cout << RED << "Failed: " << m_testsFailed << RESET << std::endl;

    if (m_testsFailed == 0) {
      std::cout << GREEN << "\nAll tests passed! ✓" << RESET << std::endl;
    }
    else {
      std::cout << YELLOW << "\nSome tests failed. Please review the output above."
        << RESET << std::endl;
    }
  }
};

int main(int argc, char* argv[]) {
  std::string resource;

  if (argc > 1) {
    resource = argv[1];
  }
  else {
    std::cout << "Usage: " << argv[0] << " <VISA_RESOURCE_STRING>" << std::endl;
    std::cout << "Example: " << argv[0] << " \"USB0::0xF4EC::0x1410::SPD13DCQ7R0719::INSTR\"" << std::endl;
    std::cout << "\nAttempting to use default resource string..." << std::endl;
    resource = "USB0::0xF4EC::0x1410::SPD13DCQ7R0719::INSTR";
  }

  try {
    TestSiglentPowerSupply tester(resource);
    tester.RunAllTests();
  }
  catch (const std::exception& e) {
    std::cerr << RED << "Exception: " << e.what() << RESET << std::endl;
    return 1;
  }

  return 0;
}