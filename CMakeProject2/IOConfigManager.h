#pragma once

#include <string>
#include <vector>
#include <map>
#include "include/eziio/EziIO_Manager.h"
#include "nlohmann/json.hpp"

// Forward declaration
class PneumaticManager;

// Pin mapping structure
struct IOPin {
  int pin;
  std::string name;
};

// Device IO configuration
struct IODeviceConfig {
  std::vector<IOPin> inputs;
  std::vector<IOPin> outputs;
};

// EziIO device full configuration
struct EziIODeviceConfig {
  int deviceId;
  std::string name;
  std::string IP;
  int inputCount;
  int outputCount;
  IODeviceConfig ioConfig;
};

// IO pin reference structure (for pneumatic slides)
struct IOPinRef {
  std::string deviceName;
  std::string pinName;
};

// Pneumatic slide configuration
struct PneumaticSlideConfig {
  std::string name;
  IOPinRef output;
  IOPinRef extendedInput;
  IOPinRef retractedInput;
  int timeoutMs;
};

class IOConfigManager {
public:
  IOConfigManager();
  ~IOConfigManager();

  // Load the configuration from a JSON file
  bool loadConfig(const std::string& filename);

  // Save the configuration to a JSON file
  bool saveConfig(const std::string& filename);

  // Initialize the EziIO manager with the loaded configuration
  bool initializeIOManager(EziIOManager& ioManager);

  // Initialize the Pneumatic manager with the loaded configuration
  bool initializePneumaticManager(PneumaticManager& pneumaticManager);

  // Get all configured EziIO devices
  const std::vector<EziIODeviceConfig>& getEziIODevices() const { return m_eziioDevices; }

  // Get all configured pneumatic slides
  const std::vector<PneumaticSlideConfig>& getPneumaticSlides() const { return m_pneumaticSlides; }

  // Get pin name from device and pin number
  std::string getPinName(const std::string& deviceName, bool isInput, int pinNumber) const;

  // Add a new pneumatic slide configuration
  bool addPneumaticSlide(const PneumaticSlideConfig& slideConfig);

  // Remove a pneumatic slide configuration
  bool removePneumaticSlide(const std::string& slideName);

  // Update an existing pneumatic slide configuration
  bool updatePneumaticSlide(const PneumaticSlideConfig& slideConfig);

  void updateLastModified();
private:
  // Loaded configuration
  std::vector<EziIODeviceConfig> m_eziioDevices;
  std::vector<PneumaticSlideConfig> m_pneumaticSlides;

  // Configuration metadata
  struct MetaData {
    std::string version;
    std::string lastUpdated;
  } m_metadata;

  // Helper function to parse JSON
  bool parseJSON(const nlohmann::json& json);

  // Helper function to create JSON
  nlohmann::json createJSON() const;
};