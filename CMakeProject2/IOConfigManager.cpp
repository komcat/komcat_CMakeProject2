#include "IOConfigManager.h"
#include "include/eziio/PneumaticManager.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

IOConfigManager::IOConfigManager()
{
  // Default metadata
  m_metadata.version = "1.0";
  updateLastModified();
}

IOConfigManager::~IOConfigManager()
{
}

bool IOConfigManager::loadConfig(const std::string& filename)
{
  std::ifstream file(filename);
  if (!file.is_open()) {
    std::cerr << "Failed to open config file: " << filename << std::endl;
    return false;
  }

  try {
    nlohmann::json json;
    file >> json;
    file.close();

    return parseJSON(json);
  }
  catch (const std::exception& e) {
    std::cerr << "Error parsing JSON config file: " << e.what() << std::endl;
    return false;
  }
}

bool IOConfigManager::saveConfig(const std::string& filename)
{
  // Update the last modified timestamp
  updateLastModified();

  // Create the JSON document
  nlohmann::json json = createJSON();

  // Write to file
  std::ofstream file(filename);
  if (!file.is_open()) {
    std::cerr << "Failed to open config file for writing: " << filename << std::endl;
    return false;
  }

  try {
    file << std::setw(2) << json << std::endl;
    file.close();
    return true;
  }
  catch (const std::exception& e) {
    std::cerr << "Error writing JSON config file: " << e.what() << std::endl;
    return false;
  }
}

bool IOConfigManager::initializeIOManager(EziIOManager& ioManager)
{
  // Add all devices to the IO manager
  for (const auto& device : m_eziioDevices) {
    if (!ioManager.addDevice(
      device.deviceId,
      device.name,
      device.IP,
      device.inputCount,
      device.outputCount)) {
      std::cerr << "Failed to add device to IO manager: " << device.name << std::endl;
      return false;
    }
  }

  std::cout << "Initialized IO manager with " << m_eziioDevices.size() << " devices" << std::endl;
  return true;
}

bool IOConfigManager::initializePneumaticManager(PneumaticManager& pneumaticManager)
{
  // Load the configuration into the pneumatic manager
  return pneumaticManager.loadConfiguration(this);
}

std::string IOConfigManager::getPinName(const std::string& deviceName, bool isInput, int pinNumber) const
{
  // Find the device
  for (const auto& device : m_eziioDevices) {
    if (device.name == deviceName) {
      // Check pin type and find by number
      if (isInput) {
        for (const auto& pin : device.ioConfig.inputs) {
          if (pin.pin == pinNumber) {
            return pin.name;
          }
        }
      }
      else {
        for (const auto& pin : device.ioConfig.outputs) {
          if (pin.pin == pinNumber) {
            return pin.name;
          }
        }
      }

      // Pin not found with a name, return the generic name
      return std::string(isInput ? "Input" : "Output") + std::to_string(pinNumber);
    }
  }

  // Device not found
  return "Unknown Pin";
}

bool IOConfigManager::addPneumaticSlide(const PneumaticSlideConfig& slideConfig)
{
  // Check if a slide with this name already exists
  for (const auto& slide : m_pneumaticSlides) {
    if (slide.name == slideConfig.name) {
      std::cerr << "A pneumatic slide with name '" << slideConfig.name << "' already exists" << std::endl;
      return false;
    }
  }

  // Add the new slide
  m_pneumaticSlides.push_back(slideConfig);

  std::cout << "Added pneumatic slide: " << slideConfig.name << std::endl;
  return true;
}

bool IOConfigManager::removePneumaticSlide(const std::string& slideName)
{
  for (auto it = m_pneumaticSlides.begin(); it != m_pneumaticSlides.end(); ++it) {
    if (it->name == slideName) {
      m_pneumaticSlides.erase(it);
      std::cout << "Removed pneumatic slide: " << slideName << std::endl;
      return true;
    }
  }

  std::cerr << "Pneumatic slide not found: " << slideName << std::endl;
  return false;
}

bool IOConfigManager::updatePneumaticSlide(const PneumaticSlideConfig& slideConfig)
{
  for (auto& slide : m_pneumaticSlides) {
    if (slide.name == slideConfig.name) {
      slide = slideConfig;
      std::cout << "Updated pneumatic slide: " << slideConfig.name << std::endl;
      return true;
    }
  }

  std::cerr << "Pneumatic slide not found: " << slideConfig.name << std::endl;
  return false;
}

bool IOConfigManager::parseJSON(const nlohmann::json& json)
{
  // Clear existing configuration
  m_eziioDevices.clear();
  m_pneumaticSlides.clear();

  try {
    // Parse metadata
    if (json.contains("metadata")) {
      const auto& metadata = json["metadata"];
      m_metadata.version = metadata.value("version", "1.0");
      m_metadata.lastUpdated = metadata.value("lastUpdated", "");
    }

    // Parse EziIO devices
    if (json.contains("eziio")) {
      const auto& eziioArray = json["eziio"];
      for (const auto& deviceJson : eziioArray) {
        EziIODeviceConfig device;
        device.deviceId = deviceJson.value("deviceId", 0);
        device.name = deviceJson.value("name", "");
        device.IP = deviceJson.value("IP", "");
        device.inputCount = deviceJson.value("inputCount", 0);
        device.outputCount = deviceJson.value("outputCount", 0);

        // Parse IO configuration
        if (deviceJson.contains("ioConfig")) {
          const auto& ioConfig = deviceJson["ioConfig"];

          // Parse inputs
          if (ioConfig.contains("inputs")) {
            for (const auto& inputJson : ioConfig["inputs"]) {
              IOPin pin;
              pin.pin = inputJson.value("pin", 0);
              pin.name = inputJson.value("name", "");
              device.ioConfig.inputs.push_back(pin);
            }
          }

          // Parse outputs
          if (ioConfig.contains("outputs")) {
            for (const auto& outputJson : ioConfig["outputs"]) {
              IOPin pin;
              pin.pin = outputJson.value("pin", 0);
              pin.name = outputJson.value("name", "");
              device.ioConfig.outputs.push_back(pin);
            }
          }
        }

        m_eziioDevices.push_back(device);
      }
    }

    // Parse pneumatic slides
    if (json.contains("pneumaticSlides")) {
      const auto& slidesArray = json["pneumaticSlides"];
      for (const auto& slideJson : slidesArray) {
        PneumaticSlideConfig slide;
        slide.name = slideJson.value("name", "");
        slide.timeoutMs = slideJson.value("timeoutMs", 5000);

        // Parse output pin reference
        if (slideJson.contains("output")) {
          const auto& outputJson = slideJson["output"];
          slide.output.deviceName = outputJson.value("deviceName", "");
          slide.output.pinName = outputJson.value("pinName", "");
        }

        // Parse extended input pin reference
        if (slideJson.contains("extendedInput")) {
          const auto& inputJson = slideJson["extendedInput"];
          slide.extendedInput.deviceName = inputJson.value("deviceName", "");
          slide.extendedInput.pinName = inputJson.value("pinName", "");
        }

        // Parse retracted input pin reference
        if (slideJson.contains("retractedInput")) {
          const auto& inputJson = slideJson["retractedInput"];
          slide.retractedInput.deviceName = inputJson.value("deviceName", "");
          slide.retractedInput.pinName = inputJson.value("pinName", "");
        }

        m_pneumaticSlides.push_back(slide);
      }
    }

    // Log the loaded configuration
    std::cout << "Loaded " << m_eziioDevices.size() << " EziIO devices and "
      << m_pneumaticSlides.size() << " pneumatic slides" << std::endl;

    return true;
  }
  catch (const std::exception& e) {
    std::cerr << "Error parsing JSON content: " << e.what() << std::endl;
    return false;
  }
}

nlohmann::json IOConfigManager::createJSON() const
{
  nlohmann::json json;

  // Add metadata
  json["metadata"] = {
      {"version", m_metadata.version},
      {"lastUpdated", m_metadata.lastUpdated}
  };

  // Add EziIO devices
  json["eziio"] = nlohmann::json::array();
  for (const auto& device : m_eziioDevices) {
    nlohmann::json deviceJson;
    deviceJson["deviceId"] = device.deviceId;
    deviceJson["name"] = device.name;
    deviceJson["IP"] = device.IP;
    deviceJson["inputCount"] = device.inputCount;
    deviceJson["outputCount"] = device.outputCount;

    // Add IO configuration
    nlohmann::json ioConfig;

    // Add inputs
    ioConfig["inputs"] = nlohmann::json::array();
    for (const auto& input : device.ioConfig.inputs) {
      ioConfig["inputs"].push_back({
          {"pin", input.pin},
          {"name", input.name}
        });
    }

    // Add outputs
    ioConfig["outputs"] = nlohmann::json::array();
    for (const auto& output : device.ioConfig.outputs) {
      ioConfig["outputs"].push_back({
          {"pin", output.pin},
          {"name", output.name}
        });
    }

    deviceJson["ioConfig"] = ioConfig;
    json["eziio"].push_back(deviceJson);
  }

  // Add pneumatic slides
  json["pneumaticSlides"] = nlohmann::json::array();
  for (const auto& slide : m_pneumaticSlides) {
    nlohmann::json slideJson;
    slideJson["name"] = slide.name;
    slideJson["timeoutMs"] = slide.timeoutMs;

    // Add output reference
    slideJson["output"] = {
        {"deviceName", slide.output.deviceName},
        {"pinName", slide.output.pinName}
    };

    // Add extended input reference
    slideJson["extendedInput"] = {
        {"deviceName", slide.extendedInput.deviceName},
        {"pinName", slide.extendedInput.pinName}
    };

    // Add retracted input reference
    slideJson["retractedInput"] = {
        {"deviceName", slide.retractedInput.deviceName},
        {"pinName", slide.retractedInput.pinName}
    };

    json["pneumaticSlides"].push_back(slideJson);
  }

  return json;
}

void IOConfigManager::updateLastModified()
{
  // Get current time
  auto now = std::chrono::system_clock::now();
  auto nowTimeT = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << std::put_time(std::gmtime(&nowTimeT), "%Y-%m-%dT%H:%M:%SZ");
  m_metadata.lastUpdated = ss.str();
}