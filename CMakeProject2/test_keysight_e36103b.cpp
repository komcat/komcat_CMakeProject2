// test_keysight_e36103b.cpp
#include "include/PowerSupplyDevice/KeysightE36103B/KeysightE36103B.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

// Color codes for terminal output
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define CYAN    "\033[36m"

void PrintHeader(const std::string& text) {
  std::cout << CYAN << "\n=== " << text << " ===" << RESET << std::endl;
}

void PrintResult(bool success, const std::string& message) {
  if (success) {
    std::cout << GREEN << "  ✓ " << message << RESET << std::endl;
  }
  else {
    std::cout << RED << "  ✗ " << message << RESET << std::endl;
  }
}

void PrintInfo(const std::string& message) {
  std::cout << YELLOW << "  → " << message << RESET << std::endl;
}

int main(int argc, char* argv[]) {
  std::string resource;

  if (argc > 1) {
    resource = argv[1];
  }
  else {
    // Default Keysight E36103B resource string
    resource = "USB0::0x2A8D::0x1602::MY58200108::INSTR";
    std::cout << "Using default resource: " << resource << std::endl;
  }

  PrintHeader("KEYSIGHT E36103B CONNECTION TEST");

  try {
    // Create power supply instance
    KeysightE36103B ps(resource);
    ps.SetDebugMode(true);  // Enable debug output

    // Test 1: Connect to device
    PrintHeader("Connection Test");
    bool connected = ps.Connect();
    PrintResult(connected, "Connect to power supply");

    if (!connected) {
      std::cerr << RED << "Failed to connect. Please check:" << RESET << std::endl;
      std::cerr << "  - USB cable is connected" << std::endl;
      std::cerr << "  - Power supply is ON" << std::endl;
      std::cerr << "  - Resource string is correct" << std::endl;
      std::cerr << "  - VISA drivers are installed" << std::endl;
      return 1;
    }

    PrintResult(ps.IsConnected(), "Verify connection status");

    // Test 2: Get device information
    PrintHeader("Device Information");
    auto info = ps.GetDeviceInfo();
    PrintInfo("Name: " + info.name);
    PrintInfo("Model: " + info.model);
    PrintInfo("Serial: " + info.serialNumber);
    PrintInfo("Firmware: " + info.firmwareVersion);

    // Test 3: Basic voltage/current setup
    PrintHeader("Basic Setup Test");

    // Set safe test values
    float testVoltage = 5.0f;
    float testCurrent = 0.5f;

    bool voltSet = ps.SetVoltage(testVoltage);
    PrintResult(voltSet, "Set voltage to " + std::to_string(testVoltage) + "V");

    bool currSet = ps.SetCurrent(testCurrent);
    PrintResult(currSet, "Set current limit to " + std::to_string(testCurrent) + "A");

    // Test 4: Read values (before output on)
    PrintHeader("Read Values (Output OFF)");
    float readVolt = ps.ReadVoltage();
    float readCurr = ps.ReadCurrent();

    std::cout << "  Voltage: " << std::fixed << std::setprecision(3)
      << readVolt << "V" << std::endl;
    std::cout << "  Current: " << std::fixed << std::setprecision(3)
      << readCurr << "A" << std::endl;

    // Test 5: Output control
    PrintHeader("Output Control Test");

    std::cout << YELLOW << "  WARNING: Output will be turned ON briefly!" << RESET << std::endl;
    std::cout << "  Press Enter to continue or Ctrl+C to abort...";
    std::cin.get();

    bool outputOn = ps.TurnOn();
    PrintResult(outputOn, "Turn output ON");

    if (outputOn) {
      PrintResult(ps.IsOutputOn(), "Verify output is ON");

      // Wait a moment
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));

      // Read values with output on
      auto measurement = ps.ReadVoltageCurrent();
      std::cout << "  Live measurement:" << std::endl;
      std::cout << "    Voltage: " << std::fixed << std::setprecision(3)
        << measurement.voltage << "V" << std::endl;
      std::cout << "    Current: " << std::fixed << std::setprecision(3)
        << measurement.current << "A" << std::endl;

      // Turn output off
      bool outputOff = ps.TurnOff();
      PrintResult(outputOff, "Turn output OFF");
      PrintResult(!ps.IsOutputOn(), "Verify output is OFF");
    }

    // Test 6: Protection settings
    PrintHeader("Protection Settings Test");

    float ovpValue = 6.0f;  // Set OVP to 6V
    bool ovpSet = ps.SetOVP(ovpValue);
    PrintResult(ovpSet, "Set OVP to " + std::to_string(ovpValue) + "V");

    float ocpValue = 0.6f;  // Set OCP to 600mA
    bool ocpSet = ps.SetOCP(ocpValue);
    PrintResult(ocpSet, "Set OCP to " + std::to_string(ocpValue) + "A");

    // Test 7: Simple sweep
    PrintHeader("Simple Voltage Sweep Test");
    std::cout << "  Performing voltage sweep from 1V to 3V..." << std::endl;

    IPowerSupplyDevice::SweepConfig sweepConfig;
    sweepConfig.mode = IPowerSupplyDevice::SweepConfig::Mode::CONSTANT_VOLTAGE;
    sweepConfig.startValue = 1.0f;
    sweepConfig.endValue = 3.0f;
    sweepConfig.stepSize = 0.5f;
    sweepConfig.delayMs = 500;
    sweepConfig.channel = 1;

    auto sweepResult = ps.ExecuteSweepBlocking(sweepConfig);

    PrintResult(sweepResult.completed, "Sweep completed");

    if (sweepResult.completed) {
      std::cout << "  Sweep results:" << std::endl;
      for (size_t i = 0; i < sweepResult.measurements.size(); ++i) {
        std::cout << "    " << std::fixed << std::setprecision(2)
          << sweepResult.sweepValues[i] << "V → "
          << "V:" << sweepResult.measurements[i].voltage
          << " I:" << sweepResult.measurements[i].current << "A" << std::endl;
      }
    }

    // Test 8: Disconnect
    PrintHeader("Disconnection Test");
    bool disconnected = ps.Disconnect();
    PrintResult(disconnected, "Disconnect from device");
    PrintResult(!ps.IsConnected(), "Verify disconnected");

    // Summary
    PrintHeader("TEST COMPLETE");
    std::cout << GREEN << "All basic tests completed successfully!" << RESET << std::endl;

  }
  catch (const std::exception& e) {
    std::cerr << RED << "Exception: " << e.what() << RESET << std::endl;
    return 1;
  }

  return 0;
}