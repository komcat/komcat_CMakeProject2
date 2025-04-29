// IOConfigManager.h
#pragma once

#include <string>
#include <map>
#include "nlohmann/json.hpp"
#include "include/eziio/EziIO_Manager.h"

class IOConfigManager {
public:
  IOConfigManager();
  ~IOConfigManager();

  // Load configuration from JSON file
  bool loadConfig(const std::string& filename);

  // Initialize IO Manager with loaded configuration
  bool initializeIOManager(EziIOManager& ioManager);

  // Get pin name from configuration
  std::string getPinName(const std::string& deviceName, bool isInput, int pin) const;

  // Check if configuration was loaded successfully
  bool isConfigLoaded() const;

  // Get the raw config
  const nlohmann::json& getConfig() const { return m_config; }

private:
  nlohmann::json m_config;
  bool m_configLoaded;

  // Cached mappings for faster lookup
  std::map<std::string, std::map<int, std::string>> m_inputPinNames;
  std::map<std::string, std::map<int, std::string>> m_outputPinNames;

  // Helper functions
  void applyDefaultConfig(EziIOManager& ioManager);
  void buildPinNameMaps();
};