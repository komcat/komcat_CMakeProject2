// IOConfigManager.cpp
#include "IOConfigManager.h"
#include <fstream>
#include <iostream>

IOConfigManager::IOConfigManager() : m_configLoaded(false) {
}

IOConfigManager::~IOConfigManager() {
}

bool IOConfigManager::loadConfig(const std::string& filename) {
  try {
    std::ifstream file(filename);
    if (!file.is_open()) {
      std::cerr << "Failed to open config file: " << filename << std::endl;
      return false;
    }

    file >> m_config;
    m_configLoaded = true;
    std::cout << "Successfully loaded IO configuration from " << filename << std::endl;

    // Build the pin name maps for quick lookup
    buildPinNameMaps();
    return true;
  }
  catch (const std::exception& e) {
    std::cerr << "Error parsing JSON config: " << e.what() << std::endl;
    m_configLoaded = false;
    return false;
  }
}

void IOConfigManager::buildPinNameMaps() {
  if (!m_configLoaded) return;

  // Clear existing maps
  m_inputPinNames.clear();
  m_outputPinNames.clear();

  try {
    // Iterate through each device in the config
    for (const auto& device : m_config["eziio"]) {
      std::string deviceName = device["name"];

      // Process inputs
      if (device.contains("ioConfig") && device["ioConfig"].contains("inputs")) {
        for (const auto& input : device["ioConfig"]["inputs"]) {
          int pin = input["pin"];
          std::string name = input["name"];
          m_inputPinNames[deviceName][pin] = name;
        }
      }

      // Process outputs
      if (device.contains("ioConfig") && device["ioConfig"].contains("outputs")) {
        for (const auto& output : device["ioConfig"]["outputs"]) {
          int pin = output["pin"];
          std::string name = output["name"];
          m_outputPinNames[deviceName][pin] = name;
        }
      }
    }
  }
  catch (const std::exception& e) {
    std::cerr << "Error building pin name maps: " << e.what() << std::endl;
  }
}

bool IOConfigManager::initializeIOManager(EziIOManager& ioManager) {
  if (!m_configLoaded) {
    std::cerr << "Configuration not loaded, using default settings" << std::endl;
    applyDefaultConfig(ioManager);
    return false;
  }

  try {
    // Add devices from configuration
    if (m_config.contains("eziio") && m_config["eziio"].is_array()) {
      for (const auto& device : m_config["eziio"]) {
        int deviceId = device["deviceId"];
        std::string name = device["name"];
        std::string ip = device["IP"];
        int inputCount = device["inputCount"];
        int outputCount = device["outputCount"];

        std::cout << "Adding device from config: " << name << " (ID: " << deviceId
          << ") at IP " << ip << std::endl;

        ioManager.addDevice(deviceId, name, ip, inputCount, outputCount);
      }
      return true;
    }
    else {
      std::cerr << "Invalid or missing 'eziio' section in config" << std::endl;
      applyDefaultConfig(ioManager);
      return false;
    }
  }
  catch (const std::exception& e) {
    std::cerr << "Error processing device configuration: " << e.what() << std::endl;
    std::cerr << "Falling back to default configuration" << std::endl;
    applyDefaultConfig(ioManager);
    return false;
  }
}

std::string IOConfigManager::getPinName(const std::string& deviceName, bool isInput, int pin) const {
  if (!m_configLoaded) {
    return std::string("Pin ") + std::to_string(pin);
  }

  try {
    const auto& pinMap = isInput ? m_inputPinNames : m_outputPinNames;

    // Look for device in the map
    auto deviceIt = pinMap.find(deviceName);
    if (deviceIt != pinMap.end()) {
      // Look for pin in the device's map
      auto pinIt = deviceIt->second.find(pin);
      if (pinIt != deviceIt->second.end()) {
        return pinIt->second;
      }
    }
  }
  catch (const std::exception& e) {
    std::cerr << "Error getting pin name: " << e.what() << std::endl;
  }

  // Fallback to a generic name if not found
  return std::string("Pin ") + std::to_string(pin);
}

bool IOConfigManager::isConfigLoaded() const {
  return m_configLoaded;
}

void IOConfigManager::applyDefaultConfig(EziIOManager& ioManager) {
  // Default configuration if JSON loading fails
  ioManager.addDevice(0, "IOBottom", "192.168.0.3", 0, 16);
  ioManager.addDevice(1, "IOTop", "192.168.0.5", 8, 8);
}