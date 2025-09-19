// ProcessConfiguration.h
#pragma once
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>
#include "AppContext.h"



//  // Forward declaration to avoid circular dependencies
//class AppContext;
//class MotionConfig;

namespace UAA3ProcessBuilders {

  using json = nlohmann::json;



  class ProcessConfiguration {
  public:
    struct ConfigEntry {
      std::string value;
      std::string category;
      std::string description;
      std::string defaultValue;
      std::string type;  // "node", "int", "float", "bool"
    };

  private:
    std::map<std::string, ConfigEntry> m_configs;
    std::string m_processName;

  public:
    ProcessConfiguration(const std::string& processName = "Default")
      : m_processName(processName) {
    }

    // Add configuration entries
    void addNode(const std::string& device, const std::string& action, const std::string& nodeId) {
      std::string key = device + "." + action;
      m_configs[key] = {
          nodeId,                    // value
          device,                    // category
          action + " position",      // description
          nodeId,                    // default
          "node"                     // type
      };
    }

    void addParameter(const std::string& category, const std::string& param,
      const std::string& value, const std::string& type = "string") {
      std::string key = category + "." + param;
      m_configs[key] = {
          value,
          category,
          param,
          value,
          type
      };
    }

    // Getters
    std::string get(const std::string& key) const {
      auto it = m_configs.find(key);
      if (it != m_configs.end()) {
        return it->second.value;
      }
      return "";
    }

    std::string getNode(const std::string& device, const std::string& action) const {
      return get(device + "." + action);
    }

    int getInt(const std::string& category, const std::string& param) const {
      std::string value = get(category + "." + param);
      return value.empty() ? 0 : std::stoi(value);
    }

    float getFloat(const std::string& category, const std::string& param) const {
      std::string value = get(category + "." + param);
      return value.empty() ? 0.0f : std::stof(value);
    }

    bool getBool(const std::string& category, const std::string& param) const {
      std::string value = get(category + "." + param);
      return value == "true" || value == "1";
    }

    // Setters
    void set(const std::string& key, const std::string& value) {
      if (m_configs.find(key) != m_configs.end()) {
        m_configs[key].value = value;
      }
    }

    void setNode(const std::string& device, const std::string& action, const std::string& nodeId) {
      set(device + "." + action, nodeId);
    }

    // Access for UI
    std::map<std::string, ConfigEntry>& getAllConfigs() { return m_configs; }
    const std::map<std::string, ConfigEntry>& getAllConfigs() const { return m_configs; }

    // Get categories
    std::vector<std::string> getCategories() const {
      std::vector<std::string> categories;
      std::map<std::string, bool> seen;
      for (const auto& [key, entry] : m_configs) {
        if (seen.find(entry.category) == seen.end()) {
          categories.push_back(entry.category);
          seen[entry.category] = true;
        }
      }
      return categories;
    }

    // Reset to defaults
    void resetToDefaults() {
      for (auto& [key, entry] : m_configs) {
        entry.value = entry.defaultValue;
      }
    }

    // Persistence with nlohmann/json
    bool saveToFile(const std::string& filename) const {
      try {
        json j;
        j["processName"] = m_processName;
        j["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();

        // Save configurations
        json configs;
        for (const auto& [key, entry] : m_configs) {
          configs[key] = {
              {"value", entry.value},
              {"category", entry.category},
              {"description", entry.description},
              {"default", entry.defaultValue},
              {"type", entry.type}
          };
        }
        j["configurations"] = configs;

        // Write to file with pretty printing
        std::ofstream file(filename);
        if (!file.is_open()) {
          std::cerr << "Failed to open file for writing: " << filename << std::endl;
          return false;
        }

        file << j.dump(4);  // 4 spaces indentation for pretty printing
        file.close();

        std::cout << "Configuration saved to: " << filename << std::endl;
        return true;
      }
      catch (const std::exception& e) {
        std::cerr << "Error saving configuration: " << e.what() << std::endl;
        return false;
      }
    }

    bool loadFromFile(const std::string& filename) {
      try {
        std::ifstream file(filename);
        if (!file.is_open()) {
          std::cerr << "File not found: " << filename << std::endl;
          return false;
        }

        json j;
        file >> j;
        file.close();

        // Load process name
        if (j.contains("processName")) {
          m_processName = j["processName"].get<std::string>();
        }

        // Load configurations
        if (j.contains("configurations")) {
          m_configs.clear();
          for (auto& [key, value] : j["configurations"].items()) {
            m_configs[key] = {
                value["value"].get<std::string>(),
                value["category"].get<std::string>(),
                value["description"].get<std::string>(),
                value["default"].get<std::string>(),
                value["type"].get<std::string>()
            };
          }
        }

        std::cout << "Configuration loaded from: " << filename << std::endl;
        return true;
      }
      catch (const std::exception& e) {
        std::cerr << "Error loading configuration: " << e.what() << std::endl;
        return false;
      }
    }

    // Export configuration as json object (for advanced usage)
    json toJson() const {
      json j;
      j["processName"] = m_processName;

      for (const auto& [key, entry] : m_configs) {
        j["configs"][key] = {
            {"value", entry.value},
            {"category", entry.category},
            {"description", entry.description},
            {"default", entry.defaultValue},
            {"type", entry.type}
        };
      }

      return j;
    }

    // Import configuration from json object
    void fromJson(const json& j) {
      if (j.contains("processName")) {
        m_processName = j["processName"].get<std::string>();
      }

      if (j.contains("configs")) {
        m_configs.clear();
        for (auto& [key, value] : j["configs"].items()) {
          m_configs[key] = {
              value["value"].get<std::string>(),
              value["category"].get<std::string>(),
              value["description"].get<std::string>(),
              value["default"].get<std::string>(),
              value["type"].get<std::string>()
          };
        }
      }
    }


    // Then update the validateNode method:
    bool validateNode(const std::string& nodeName) const {
      // Get AppContext instance (following your example pattern)
      AppContext& context = AppContext::GetInstance();

      // Get motion config from context
      auto* motionConfig = context.GetMotionConfig();  // or GetMotionConfig() - check your AppContext
      if (!motionConfig) {
        // Fallback to basic validation
        return !nodeName.empty() && nodeName.find("node_") != std::string::npos;
      }


      return motionConfig->NodeExists(nodeName);

    }


    // Validate all nodes in configuration
    std::vector<std::string> validateAllNodes() const {
      std::vector<std::string> invalidNodes;
      for (const auto& [key, entry] : m_configs) {
        if (entry.type == "node" && !validateNode(entry.value)) {
          invalidNodes.push_back(key + " = " + entry.value);
        }
      }
      return invalidNodes;
    }

    // Clone configuration
    ProcessConfiguration clone() const {
      ProcessConfiguration newConfig(m_processName);
      newConfig.m_configs = m_configs;
      return newConfig;
    }

    // Merge with another configuration (useful for partial updates)
    void merge(const ProcessConfiguration& other) {
      for (const auto& [key, entry] : other.m_configs) {
        if (m_configs.find(key) != m_configs.end()) {
          m_configs[key].value = entry.value;
        }
      }
    }

    // Get differences from default
    std::vector<std::pair<std::string, std::string>> getModifiedValues() const {
      std::vector<std::pair<std::string, std::string>> modified;
      for (const auto& [key, entry] : m_configs) {
        if (entry.value != entry.defaultValue) {
          modified.push_back({ key, entry.value });
        }
      }
      return modified;
    }
  };

} // namespace