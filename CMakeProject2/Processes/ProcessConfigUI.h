// ProcessConfigUI.h
#pragma once
#include "ProcessConfiguration.h"
#include "ProcessConfigBuilders.h"
#include "ProcessConfigManager.h"
#include <imgui.h>
#include <string>
#include <memory>

namespace UAA3ProcessBuilders {

  class ProcessConfigUI {
  private:
    ProcessConfiguration m_config;
    ProcessConfigManager m_configManager;


    std::string m_currentProcess;
    std::string m_selectedPreset;
    char m_configName[256];
    std::string m_statusMessage;
    float m_statusMessageTimer = 0.0f;

  public:
    ProcessConfigUI() : m_configManager("process_configs") {
      strcpy_s(m_configName, "my_config");

      // Start with pick & place config
      m_config = ProcessConfigBuilders::createPickPlaceConfig();
      m_currentProcess = "PickPlace";

      // Try to load last used config
      if (std::filesystem::exists("last_used_config.json")) {
        m_config.loadFromFile("last_used_config.json");
      }
    }

    // Add this method to set configuration
    void setConfiguration(const ProcessConfiguration& config) {
      m_config = config;
    }

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
    void renderProcessSelector() {
      ImGui::Text("Process Type:");
      ImGui::SameLine();

      if (ImGui::BeginCombo("##ProcessType", m_currentProcess.c_str())) {
        if (ImGui::Selectable("PickPlace", m_currentProcess == "PickPlace")) {
          m_config = ProcessConfigBuilders::createPickPlaceConfig();
          m_currentProcess = "PickPlace";
        }
        if (ImGui::Selectable("Reject", m_currentProcess == "Reject")) {
          m_config = ProcessConfigBuilders::createRejectConfig();
          m_currentProcess = "Reject";
        }
        if (ImGui::Selectable("UVCuring", m_currentProcess == "UVCuring")) {
          m_config = ProcessConfigBuilders::createUVCuringConfig();
          m_currentProcess = "UVCuring";
        }
        if (ImGui::Selectable("Probing", m_currentProcess == "Probing")) {
          m_config = ProcessConfigBuilders::createProbingConfig();
          m_currentProcess = "Probing";
        }
        ImGui::EndCombo();
      }
    }

    void renderPresetManager() {
      ImGui::Text("Saved Configurations:");

      auto configs = m_configManager.getAvailableConfigs();

      if (ImGui::BeginCombo("##LoadPreset",
        m_selectedPreset.empty() ? "Select preset..." : m_selectedPreset.c_str())) {
        for (const auto& name : configs) {
          if (ImGui::Selectable(name.c_str(), m_selectedPreset == name)) {
            m_selectedPreset = name;
            if (m_configManager.loadConfig(name, m_config)) {
              showStatus("Loaded: " + name);
            }
          }
        }
        ImGui::EndCombo();
      }

      //ImGui::SameLine();

      ImGui::InputText("Name", m_configName, sizeof(m_configName));

      //ImGui::SameLine();

      if (ImGui::Button("Save")) {
        if (strlen(m_configName) > 0) {
          if (m_configManager.saveConfig(m_configName, m_config)) {
            showStatus(std::string("Saved: ") + m_configName);
            m_selectedPreset = m_configName;
          }
        }
      }

      ImGui::SameLine();

      if (ImGui::Button("Delete") && !m_selectedPreset.empty()) {
        if (m_configManager.deleteConfig(m_selectedPreset)) {
          showStatus("Deleted: " + m_selectedPreset);
          m_selectedPreset.clear();
        }
      }
    }

    void renderConfigEditor() {
      // Show modified count
      auto modified = m_config.getModifiedValues();
      if (!modified.empty()) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1),
          "%d values modified from defaults", (int)modified.size());
      }

      // Group configurations by category
      auto categories = m_config.getCategories();

      for (const auto& category : categories) {
        bool hasModified = false;
        for (const auto& [key, value] : modified) {
          if (m_config.getAllConfigs()[key].category == category) {
            hasModified = true;
            break;
          }
        }

        // Show modified indicator in header
        std::string headerLabel = hasModified ?
          category + " (modified)" : category;

        if (ImGui::CollapsingHeader(headerLabel.c_str(),
          ImGuiTreeNodeFlags_DefaultOpen)) {

          // Use a table for better alignment
          if (ImGui::BeginTable("ConfigTable", 3, ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 60.0f);

            // Find all configs in this category
            for (auto& [key, entry] : m_config.getAllConfigs()) {
              if (entry.category != category) continue;

              ImGui::TableNextRow();
              ImGui::PushID(key.c_str());

              // Column 1: Label
              ImGui::TableSetColumnIndex(0);
              ImGui::Text("%s:", entry.description.c_str());

              // Show modified indicator
              if (entry.value != entry.defaultValue) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "*");
                if (ImGui::IsItemHovered()) {
                  ImGui::SetTooltip("Default: %s", entry.defaultValue.c_str());
                }
              }

              // Column 2: Value input
              ImGui::TableSetColumnIndex(1);
              bool valueChanged = false;

              if (entry.type == "node" || entry.type == "string") {
                char buffer[256];
                strcpy_s(buffer, entry.value.c_str());

                // Check if node is invalid
                bool isInvalidNode = (entry.type == "node" && !m_config.validateNode(entry.value));

                if (isInvalidNode) {
                  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
                }

                ImGui::SetNextItemWidth(-FLT_MIN); // Use full available width
                if (ImGui::InputText("##value", buffer, sizeof(buffer))) {
                  entry.value = buffer;
                  valueChanged = true;
                }

                if (isInvalidNode) {
                  ImGui::PopStyleColor();
                }
              }
              else if (entry.type == "int") {
                int value = std::stoi(entry.value);
                ImGui::SetNextItemWidth(-FLT_MIN);

                // Use - and + buttons for compact int editing
                if (ImGui::Button("-")) {
                  value--;
                  entry.value = std::to_string(value);
                  valueChanged = true;
                }
                ImGui::SameLine();

                ImGui::SetNextItemWidth(60);
                if (ImGui::InputInt("##value", &value, 0, 0)) {
                  entry.value = std::to_string(value);
                  valueChanged = true;
                }

                ImGui::SameLine();
                if (ImGui::Button("+")) {
                  value++;
                  entry.value = std::to_string(value);
                  valueChanged = true;
                }
              }
              else if (entry.type == "float") {
                float value = std::stof(entry.value);
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::InputFloat("##value", &value, 0.001f, 0.01f, "%.4f")) {
                  entry.value = std::to_string(value);
                  valueChanged = true;
                }
              }
              else if (entry.type == "bool") {
                bool value = (entry.value == "true");
                if (ImGui::Checkbox("##value", &value)) {
                  entry.value = value ? "true" : "false";
                  valueChanged = true;
                }
              }

              // Column 3: Reset button
              ImGui::TableSetColumnIndex(2);
              if (ImGui::Button("Reset")) {
                entry.value = entry.defaultValue;
                valueChanged = true;
              }

              if (valueChanged) {
                // Auto-save to last_used
                m_config.saveToFile("last_used_config.json");
              }

              ImGui::PopID();
            }

            ImGui::EndTable();
          }
        }
      }
    }



    void renderActions() {
      if (ImGui::Button("Reset All to Defaults", ImVec2(150, 0))) {
        m_config.resetToDefaults();
        showStatus("Reset to default values");
      }

      ImGui::SameLine();

      if (ImGui::Button("Validate Nodes", ImVec2(150, 0))) {
        auto invalid = m_config.validateAllNodes();
        if (invalid.empty()) {
          showStatus("All nodes valid");
        }
        else {
          showStatus(std::to_string(invalid.size()) + " invalid nodes found");
        }
      }

      ImGui::SameLine();

      if (ImGui::Button("Export All", ImVec2(150, 0))) {
        if (m_configManager.exportAllConfigs("all_configs_backup.json")) {
          showStatus("Exported all configurations");
        }
      }
    }

    void showStatus(const std::string& message, float duration = 3.0f) {
      m_statusMessage = message;
      m_statusMessageTimer = duration;
    }

  public:
    ProcessConfiguration& getConfig() { return m_config; }
    const ProcessConfiguration& getConfig() const { return m_config; }

    // Save current config when closing
    void onShutdown() {
      m_config.saveToFile("last_used_config.json");
    }
  };

} // namespace