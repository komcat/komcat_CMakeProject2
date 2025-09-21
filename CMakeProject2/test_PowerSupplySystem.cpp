// test_PowerSupplySystem.cpp
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>

#include "PowerSupplyDevice/IPowerSupplyDevice.h"
#include "PowerSupplyDevice/IPowerSupplyManager.h"
#include "PowerSupplyDevice/PowerSupplyManager.h"
#include "PowerSupplyDevice/MockPowerSupplyDevice.h"
#include "IResultStorage.h"
#include "FileResultStorage.h"

class PowerSupplySystemTest {
private:
  std::shared_ptr<PowerSupplyManager> manager;
  std::shared_ptr<FileResultStorage> storage;

  void PrintSeparator(const std::string& title = "") {
    std::cout << "\n========================================\n";
    if (!title.empty()) {
      std::cout << "  " << title << "\n";
      std::cout << "========================================\n";
    }
  }

  void PrintMeasurement(const std::string& deviceId, const IPowerSupplyDevice::Measurement& m) {
    std::cout << deviceId << ": V=" << std::fixed << std::setprecision(3)
      << m.voltage << "V, I=" << m.current << "A\n";
  }

public:
  bool Initialize() {
    PrintSeparator("Initializing Power Supply System");

    // Create manager
    manager = std::make_shared<PowerSupplyManager>();

    // Initialize storage
    storage = std::make_shared<FileResultStorage>();
    if (!storage->Initialize("./test_data")) {
      std::cerr << "Failed to initialize storage\n";
      return false;
    }
    manager->SetResultStorage(storage);
    std::cout << "✓ Storage initialized at ./test_data\n";

    // Add mock devices
    auto ps1 = std::make_shared<MockPowerSupplyDevice>("PS-Main", 1, "MOCK-1000");
    auto ps2 = std::make_shared<MockPowerSupplyDevice>("PS-Aux", 2, "MOCK-2000");
    auto ps3 = std::make_shared<MockPowerSupplyDevice>("PS-Test", 3, "MOCK-3000");

    manager->AddDevice(ps1, "MainSupply");
    manager->AddDevice(ps2, "AuxSupply");
    manager->AddDevice(ps3, "TestSupply");

    std::cout << "✓ Added " << manager->GetDeviceCount() << " devices\n";

    return true;
  }

  void TestConnectionManagement() {
    PrintSeparator("Testing Connection Management");

    // Connect all devices
    auto result = manager->ConnectAllDevices();
    std::cout << "Connect all: " << result.successCount << " succeeded, "
      << result.failureCount << " failed\n";

    // Check individual connections
    auto deviceIds = manager->GetDeviceIds();
    for (const auto& id : deviceIds) {
      bool connected = manager->IsDeviceConnected(id);
      std::cout << "  " << id << ": " << (connected ? "Connected" : "Not connected") << "\n";
    }
  }

  void TestBasicOperations() {
    PrintSeparator("Testing Basic Operations");

    // Set up MainSupply
    std::cout << "\nConfiguring MainSupply:\n";
    manager->SetModeConstantVoltage("MainSupply");
    manager->SetVoltage("MainSupply", 5.0f);
    manager->SetCurrent("MainSupply", 2.0f);
    manager->TurnOn("MainSupply");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto measurement = manager->ReadMeasurement("MainSupply");
    PrintMeasurement("MainSupply", measurement);

    // Store the measurement
    manager->StoreCurrentMeasurement("MainSupply", "initial_test");
    std::cout << "✓ Measurement stored\n";

    // Test batch operations
    std::cout << "\nBatch operations:\n";
    manager->SetVoltageAll(3.3f);
    manager->SetCurrentAll(1.0f);
    auto turnOnResult = manager->TurnOnAll();
    std::cout << "Turn on all: " << turnOnResult.successCount << " succeeded\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Read all measurements
    auto allMeasurements = manager->ReadAllMeasurements();
    std::cout << "\nAll measurements:\n";
    for (const auto& [id, m] : allMeasurements) {
      PrintMeasurement(id, m);
    }

    // Store all measurements
    manager->StoreAllCurrentMeasurements("batch_test");
    std::cout << "✓ All measurements stored\n";
  }

  void TestSweepOperation() {
    PrintSeparator("Testing Sweep Operation");

    IPowerSupplyDevice::SweepConfig config;
    config.mode = IPowerSupplyDevice::SweepConfig::Mode::CONSTANT_VOLTAGE;
    config.startValue = 0.0f;
    config.endValue = 5.0f;
    config.stepSize = 1.0f;
    config.delayMs = 100;
    config.channel = 1;

    std::cout << "Starting voltage sweep on MainSupply (0V to 5V):\n";
    manager->StartSweep("MainSupply", config);

    // Monitor progress
    while (manager->IsSweepRunning("MainSupply")) {
      float progress = manager->GetSweepProgress("MainSupply");
      std::cout << "\rProgress: " << std::fixed << std::setprecision(1)
        << (progress * 100) << "%  " << std::flush;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "\n";

    // Get and display results
    auto sweepResult = manager->GetSweepResults("MainSupply");
    if (sweepResult.completed) {
      std::cout << "Sweep completed successfully:\n";
      for (size_t i = 0; i < sweepResult.measurements.size(); ++i) {
        std::cout << "  V_set=" << sweepResult.sweepValues[i]
          << "V → V_meas=" << sweepResult.measurements[i].voltage
          << "V, I=" << sweepResult.measurements[i].current << "A\n";
      }

      // Store sweep results
      manager->StoreSweepResults("MainSupply", "voltage_sweep_test");
      std::cout << "✓ Sweep results stored\n";
    }
  }

  void TestBlockingSweep() {
    PrintSeparator("Testing Blocking Sweep");

    IPowerSupplyDevice::SweepConfig config;
    config.mode = IPowerSupplyDevice::SweepConfig::Mode::CONSTANT_CURRENT;
    config.startValue = 0.1f;
    config.endValue = 0.5f;
    config.stepSize = 0.1f;
    config.delayMs = 50;
    config.channel = 1;

    std::cout << "Executing blocking current sweep on AuxSupply:\n";
    auto result = manager->ExecuteSweepBlocking("AuxSupply", config);

    if (result.completed) {
      std::cout << "Sweep completed:\n";
      for (size_t i = 0; i < result.measurements.size(); ++i) {
        std::cout << "  I_set=" << result.sweepValues[i]
          << "A → V=" << result.measurements[i].voltage
          << "V, I_meas=" << result.measurements[i].current << "A\n";
      }

      manager->StoreSweepResults("AuxSupply", "current_sweep_test");
      std::cout << "✓ Results stored\n";
    }
  }

  void TestStorageQuery() {
    PrintSeparator("Testing Storage Query");

    // Query recent measurements
    IResultStorage::QueryFilter filter;
    filter.deviceType = "PowerSupply";
    filter.resultType = "measurement";
    filter.maxResults = 10;

    auto results = manager->QueryStoredResults(filter);
    std::cout << "Found " << results.size() << " measurement records:\n";

    for (const auto& record : results) {
      auto time_t = std::chrono::system_clock::to_time_t(record.timestamp);
      std::tm tm{}; // Declare and initialize the tm struct

      // Use the safer, thread-safe localtime_s
      localtime_s(&tm, &time_t);

      std::cout << "  [" << std::put_time(&tm, "%H:%M:%S") << "] "
        << record.deviceId << " - " << record.label;

      auto voltIt = record.numericValues.find("voltage");
      auto currIt = record.numericValues.find("current");
      if (voltIt != record.numericValues.end() && currIt != record.numericValues.end()) {
        std::cout << " (V=" << voltIt->second << ", I=" << currIt->second << ")";
      }
      std::cout << "\n";
    }

    // Query sweep results
    filter.resultType = "sweep";
    auto sweepResults = manager->QueryStoredResults(filter);
    std::cout << "\nFound " << sweepResults.size() << " sweep records\n";
  }

  void TestStatusMonitoring() {
    PrintSeparator("Testing Status Monitoring");

    auto allStatus = manager->GetAllDeviceStatus();

    std::cout << "Device Status:\n";
    for (const auto& [id, status] : allStatus) {
      std::cout << "\n" << id << ":\n";
      std::cout << "  Connected: " << (status.connected ? "Yes" : "No") << "\n";
      std::cout << "  Sweep running: " << (status.sweepRunning ? "Yes" : "No") << "\n";

      for (const auto& [channel, on] : status.channelOutputOn) {
        std::cout << "  Channel " << channel << ": " << (on ? "ON" : "OFF") << "\n";
      }

      std::cout << "  Last measurement: V=" << status.lastMeasurement.voltage
        << "V, I=" << status.lastMeasurement.current << "A\n";
    }
  }

  void TestErrorHandling() {
    PrintSeparator("Testing Error Handling");

    // Try to operate on non-existent device
    bool result = manager->SetVoltage("NonExistent", 5.0f);
    if (!result) {
      std::cout << "✓ Correctly failed on non-existent device\n";
      std::cout << "  Error: " << manager->GetLastError() << "\n";
    }

    manager->ClearErrors();

    // Try to start sweep while one is running
    IPowerSupplyDevice::SweepConfig config;
    config.mode = IPowerSupplyDevice::SweepConfig::Mode::CONSTANT_VOLTAGE;
    config.startValue = 0;
    config.endValue = 1;
    config.stepSize = 0.1f;
    config.delayMs = 100;

    manager->StartSweep("TestSupply", config);
    bool secondStart = manager->StartSweep("TestSupply", config);
    if (!secondStart) {
      std::cout << "✓ Correctly prevented concurrent sweeps\n";
    }

    manager->StopSweep("TestSupply");
  }

  void Cleanup() {
    PrintSeparator("Cleanup");

    // Turn off all channels
    auto result = manager->TurnOffAll();
    std::cout << "Turned off " << result.successCount << " devices\n";

    // Disconnect all
    result = manager->DisconnectAllDevices();
    std::cout << "Disconnected " << result.successCount << " devices\n";

    // Export data
    IResultStorage::QueryFilter filter;
    storage->ExportToFile("./test_export.json", filter);
    std::cout << "✓ Data exported to test_export.json\n";

    auto storageSize = storage->GetStorageSize();
    std::cout << "Total storage size: " << (storageSize / 1024.0) << " KB\n";
  }

  void RunAllTests() {
    if (!Initialize()) {
      std::cerr << "Initialization failed!\n";
      return;
    }

    TestConnectionManagement();
    TestBasicOperations();
    TestSweepOperation();
    TestBlockingSweep();
    TestStorageQuery();
    TestStatusMonitoring();
    TestErrorHandling();
    Cleanup();

    PrintSeparator("All Tests Completed");
  }
};

int main() {
  std::cout << "Power Supply System Test Suite\n";
  std::cout << "==============================\n\n";

  PowerSupplySystemTest tester;
  tester.RunAllTests();

  return 0;
}