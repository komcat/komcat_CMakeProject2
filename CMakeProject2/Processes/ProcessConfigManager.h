// ProcessConfigManager.h
#pragma once
#include "ProcessConfiguration.h"
#include <nlohmann/json.hpp>
#include <filesystem>

namespace UAA3ProcessBuilders {

  class ProcessConfigManager {
  private:
    std::map<std::string, ProcessConfiguration> m_savedConfigs;
    std::string m_configDirectory;

  public:
    ProcessConfigManager(const std::string& configDir = "configs")
      : m_configDirectory(configDir) {

      // Create config directory if it doesn't exist
      std::filesystem::create_directories(configDir);

      // Load all existing configs
      loadAllConfigs();
    }

    void loadAllConfigs() {
      m_savedConfigs.clear();

      for (const auto& entry : std::filesystem::directory_iterator(m_configDirectory)) {
        if (entry.path().extension() == ".json") {
          ProcessConfiguration config;
          if (config.loadFromFile(entry.path().string())) {
            std::string name = entry.path().stem().string();
            m_savedConfigs[name] = config;
          }
        }
      }
    }

    bool saveConfig(const std::string& name, const ProcessConfiguration& config) {
      std::string filepath = m_configDirectory + "/" + name + ".json";
      if (config.saveToFile(filepath)) {
        m_savedConfigs[name] = config;
        return true;
      }
      return false;
    }

    bool loadConfig(const std::string& name, ProcessConfiguration& config) {
      if (m_savedConfigs.find(name) != m_savedConfigs.end()) {
        config = m_savedConfigs[name];
        return true;
      }

      // Try loading from file
      std::string filepath = m_configDirectory + "/" + name + ".json";
      return config.loadFromFile(filepath);
    }

    std::vector<std::string> getAvailableConfigs() const {
      std::vector<std::string> names;
      for (const auto& [name, _] : m_savedConfigs) {
        names.push_back(name);
      }
      return names;
    }

    bool deleteConfig(const std::string& name) {
      std::string filepath = m_configDirectory + "/" + name + ".json";
      if (std::filesystem::remove(filepath)) {
        m_savedConfigs.erase(name);
        return true;
      }
      return false;
    }

    // Export all configs to a single file
    bool exportAllConfigs(const std::string& filename) {
      try {
        json j;
        for (const auto& [name, config] : m_savedConfigs) {
          j[name] = config.toJson();
        }

        std::ofstream file(filename);
        file << j.dump(4);
        return true;
      }
      catch (const std::exception& ) {
        return false;
      }
    }

    // Import configs from a file
    bool importConfigs(const std::string& filename) {
      try {
        std::ifstream file(filename);
        json j;
        file >> j;

        for (auto& [name, configJson] : j.items()) {
          ProcessConfiguration config;
          config.fromJson(configJson);
          m_savedConfigs[name] = config;

          // Also save to individual file
          saveConfig(name, config);
        }
        return true;
      }
      catch (const std::exception& ) {
        return false;
      }
    }
  };

} // namespace