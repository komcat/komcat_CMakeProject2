// ToolbarStateManager.h
#pragma once

#include <string>
#include <unordered_map>
#include <fstream>
#include "include/logger.h"
#include "nlohmann/json.hpp" // Include JSON library

// Simple manager for saving and loading toolbar window states
class ToolbarStateManager {
public:
  static ToolbarStateManager& GetInstance() {
    static ToolbarStateManager instance;
    return instance;
  }

  // Initialize with config file path
  void Initialize(const std::string& configFilePath = "toolbar_state.json") {
    m_configFilePath = configFilePath;
    m_logger = Logger::GetInstance();
    LoadState();
  }

  // Save a window's visibility state
  void SaveWindowState(const std::string& windowName, bool isVisible) {
    m_windowStates[windowName] = isVisible;
    SaveState(); // Save immediately for simplicity
  }

  // Get a window's visibility state (returns default if not found)
  bool GetWindowState(const std::string& windowName, bool defaultState = false) const {
    auto it = m_windowStates.find(windowName);
    if (it != m_windowStates.end()) {
      return it->second;
    }
    return defaultState;
  }

  // Save all states to file
  void SaveState() {
    try {
      nlohmann::json json;
      for (const auto& [name, visible] : m_windowStates) {
        json["windows"][name] = visible;
      }

      std::ofstream file(m_configFilePath);
      if (file.is_open()) {
        file << json.dump(2); // Pretty print with 2-space indentation
        file.close();
      }
    }
    catch (const std::exception& e) {
      if (m_logger) m_logger->LogError("Error saving toolbar state: " + std::string(e.what()));
    }
  }

  // Load states from file
  void LoadState() {
    try {
      std::ifstream file(m_configFilePath);
      if (file.is_open()) {
        nlohmann::json json;
        file >> json;

        if (json.contains("windows") && json["windows"].is_object()) {
          for (auto& [name, visible] : json["windows"].items()) {
            m_windowStates[name] = visible.get<bool>();
          }
        }

        file.close();
      }
    }
    catch (const std::exception& e) {
      if (m_logger) m_logger->LogWarning("Error loading toolbar state: " + std::string(e.what()));
    }
  }

private:
  ToolbarStateManager() = default;
  ~ToolbarStateManager() = default;

  std::string m_configFilePath = "toolbar_state.json";
  std::unordered_map<std::string, bool> m_windowStates;
  Logger* m_logger = nullptr;
};