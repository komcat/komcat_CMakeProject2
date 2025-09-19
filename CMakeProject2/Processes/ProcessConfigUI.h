// ProcessConfigUI.h
#pragma once
#include "ProcessConfiguration.h"
#include "ProcessConfigBuilders.h"
#include "ProcessConfigManager.h"
#include <imgui.h>
#include <string>
#include <memory>
#include <filesystem>

namespace UAA3ProcessBuilders {

  class ProcessConfigUI {
  private:
    ProcessConfiguration m_config;
    ProcessConfigManager m_configManager;

    std::string m_currentProcess;
    std::string m_currentProcessName;  // Tracks which configurable process is selected
    std::string m_selectedPreset;
    char m_configName[256];
    std::string m_statusMessage;
    float m_statusMessageTimer = 0.0f;
    bool m_showModifiedCount = false;  // Set to false to hide by default
    bool m_autoSave = true;  // Auto-save flag
    std::string m_lastSavedProcess;  // Track last saved process

  public:
    ProcessConfigUI() : m_configManager("process_configs") {
      strcpy_s(m_configName, "my_config");

      // Start with pick & place config
      m_config = ProcessConfigBuilders::createPickPlaceConfig();
      m_currentProcess = "PickPlace";
      m_currentProcessName = "";

      // Try to load last used config
      if (std::filesystem::exists("last_used_config.json")) {
        m_config.loadFromFile("last_used_config.json");
      }
    }

    // Add this method to set configuration
    void setConfiguration(const ProcessConfiguration& config) {
      m_config = config;
    }

    // Sync with selected process
    void setCurrentProcess(const std::string& processName);

    // Check if we should update config for a new process
    bool shouldUpdateConfig(const std::string& processName) const;

    // Auto-save configuration
    void autoSaveConfig();

    // Set auto-save enabled/disabled
    void setAutoSave(bool enabled) { m_autoSave = enabled; }
    bool isAutoSaveEnabled() const { return m_autoSave; }

    void render(float deltaTime, bool embedded = false) {
      // Update status message timer
      if (m_statusMessageTimer > 0) {
        m_statusMessageTimer -= deltaTime;
        if (m_statusMessageTimer <= 0) {
          m_statusMessage.clear();
        }
      }

      // If embedded, don't create a new window
      bool windowOpen = true;
      if (!embedded) {
        windowOpen = ImGui::Begin("Process Configuration");
      }

      if (windowOpen) {
        // Show current process if set
        if (!m_currentProcessName.empty()) {
          ImGui::Text("Current Process: %s", m_currentProcessName.c_str());
          ImGui::Separator();
        }

        renderProcessSelector();
        ImGui::Separator();

        renderPresetManager();
        ImGui::Separator();

        renderConfigEditor();
        ImGui::Separator();

        renderActions();

        // Status message with fade effect
        if (!m_statusMessage.empty() && m_statusMessageTimer > 0) {
          float alpha = (std::min)(m_statusMessageTimer, 1.0f);
          ImGui::TextColored(ImVec4(0, 1, 0, alpha), "%s", m_statusMessage.c_str());
        }
      }

      if (!embedded) {
        ImGui::End();
      }
    }

  private:
    void renderProcessSelector();
    void renderPresetManager();
    void renderConfigEditor();
    void renderActions();
    void showStatus(const std::string& message, float duration = 3.0f);

  public:
    ProcessConfiguration& getConfig() { return m_config; }
    const ProcessConfiguration& getConfig() const { return m_config; }

    // Get current process type
    const std::string& getCurrentProcessType() const { return m_currentProcess; }

    // Check if a configurable process is active
    bool hasConfigurableProcess() const {
      return !m_currentProcessName.empty() &&
        m_currentProcessName.find("_Configurable") != std::string::npos;
    }

    // Save current config when closing
    void onShutdown() {
      autoSaveConfig();
      m_config.saveToFile("last_used_config.json");
    }
  };

} // namespace