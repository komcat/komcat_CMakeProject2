// ProcessConfigUI.cpp
#include "ProcessConfigUI.h"
#include <algorithm>
#include <cctype>

namespace UAA3ProcessBuilders {

  void ProcessConfigUI::setCurrentProcess(const std::string& processName) {
    // Check if this is actually a different process
    if (m_currentProcessName == processName) {
      return;  // Already set, avoid redundant work
    }

    // Save current config before switching
    if (!m_currentProcessName.empty()) {
      std::string saveFile = "last_" + m_currentProcessName + "_config.json";
      m_config.saveToFile(saveFile);
    }

    // Update the process name
    m_currentProcessName = processName;

    // Determine config type from process name
    std::string newProcessType;

    // Make case-insensitive comparison
    std::string lowerName = processName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    if (lowerName.find("uvcuring") != std::string::npos ||
      lowerName.find("uv_curing") != std::string::npos) {
      newProcessType = "UVCuring";
    }
    else if (lowerName.find("pickplace") != std::string::npos ||
      lowerName.find("pick_place") != std::string::npos) {
      newProcessType = "PickPlace";
    }
    else if (lowerName.find("reject") != std::string::npos) {
      newProcessType = "Reject";
    }
    else if (lowerName.find("probing") != std::string::npos ||
      lowerName.find("probe") != std::string::npos) {
      newProcessType = "Probing";
    }
    else {
      // Unknown process type
      showStatus("Unknown process type: " + processName, 2.0f);
      return;
    }

    // Update process type
    m_currentProcess = newProcessType;

    // Try to load saved config for this specific process
    std::string processConfigFile = "last_" + processName + "_config.json";

    if (std::filesystem::exists(processConfigFile)) {
      // Config file exists - load it directly
      // Since loadFromFile() clears m_configs first, this will completely replace the configuration
      if (m_config.loadFromFile(processConfigFile)) {
        showStatus("Loaded saved config for " + processName);
      }
    }
    else {
      // No saved config - create new one of the correct type
      if (newProcessType == "UVCuring") {
        m_config = ProcessConfigBuilders::createUVCuringConfig();
      }
      else if (newProcessType == "PickPlace") {
        m_config = ProcessConfigBuilders::createPickPlaceConfig();
      }
      else if (newProcessType == "Reject") {
        m_config = ProcessConfigBuilders::createRejectConfig();
      }
      else if (newProcessType == "Probing") {
        m_config = ProcessConfigBuilders::createProbingConfig();
      }

      showStatus("Using default " + newProcessType + " config");
    }
  }

  bool ProcessConfigUI::shouldUpdateConfig(const std::string& processName) const {
    // Don't update if it's the same process
    if (m_currentProcessName == processName) {
      return false;
    }

    // Determine the type of the new process
    std::string lowerName = processName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    std::string processType;
    if (lowerName.find("uvcuring") != std::string::npos) {
      processType = "UVCuring";
    }
    else if (lowerName.find("pickplace") != std::string::npos) {
      processType = "PickPlace";
    }
    else if (lowerName.find("reject") != std::string::npos) {
      processType = "Reject";
    }
    else if (lowerName.find("probing") != std::string::npos) {
      processType = "Probing";
    }
    else {
      return false;  // Unknown type, don't update
    }

    // Only update if type is different
    return (processType != m_currentProcess);
  }

  void ProcessConfigUI::autoSaveConfig() {
    if (!m_autoSave) return;

    // Save to process-specific file if we have a current process
    if (!m_currentProcessName.empty()) {
      std::string processConfigFile = "last_" + m_currentProcessName + "_config.json";
      m_config.saveToFile(processConfigFile);
    }

    // Also save to type-specific file
    if (!m_currentProcess.empty()) {
      std::string typeConfigFile = "last_" + m_currentProcess + "_config.json";
      m_config.saveToFile(typeConfigFile);
    }

    // Always save to general last_used file
    m_config.saveToFile("last_used_config.json");
  }

  void ProcessConfigUI::renderProcessSelector() {
    ImGui::Text("Process Type:");
    ImGui::SameLine();

    // NO LONGER LOCKED - Allow switching between config types freely
    if (ImGui::BeginCombo("##ProcessType", m_currentProcess.c_str())) {
      if (ImGui::Selectable("PickPlace", m_currentProcess == "PickPlace")) {
        if (m_currentProcess != "PickPlace") {
          // Save current config before switching
          autoSaveConfig();

          m_config = ProcessConfigBuilders::createPickPlaceConfig();
          m_currentProcess = "PickPlace";
          showStatus("Switched to PickPlace configuration");

          // Try to load saved PickPlace config if available
          std::string typeConfigFile = "last_PickPlace_config.json";
          if (std::filesystem::exists(typeConfigFile)) {
            m_config.loadFromFile(typeConfigFile);
            showStatus("Loaded saved PickPlace config");
          }
        }
      }
      if (ImGui::Selectable("Reject", m_currentProcess == "Reject")) {
        if (m_currentProcess != "Reject") {
          autoSaveConfig();

          m_config = ProcessConfigBuilders::createRejectConfig();
          m_currentProcess = "Reject";
          showStatus("Switched to Reject configuration");

          std::string typeConfigFile = "last_Reject_config.json";
          if (std::filesystem::exists(typeConfigFile)) {
            m_config.loadFromFile(typeConfigFile);
            showStatus("Loaded saved Reject config");
          }
        }
      }
      if (ImGui::Selectable("UVCuring", m_currentProcess == "UVCuring")) {
        if (m_currentProcess != "UVCuring") {
          autoSaveConfig();

          m_config = ProcessConfigBuilders::createUVCuringConfig();
          m_currentProcess = "UVCuring";
          showStatus("Switched to UVCuring configuration");

          std::string typeConfigFile = "last_UVCuring_config.json";
          if (std::filesystem::exists(typeConfigFile)) {
            m_config.loadFromFile(typeConfigFile);
            showStatus("Loaded saved UVCuring config");
          }
        }
      }
      if (ImGui::Selectable("Probing", m_currentProcess == "Probing")) {
        if (m_currentProcess != "Probing") {
          autoSaveConfig();

          m_config = ProcessConfigBuilders::createProbingConfig();
          m_currentProcess = "Probing";
          showStatus("Switched to Probing configuration");

          std::string typeConfigFile = "last_Probing_config.json";
          if (std::filesystem::exists(typeConfigFile)) {
            m_config.loadFromFile(typeConfigFile);
            showStatus("Loaded saved Probing config");
          }
        }
      }
      ImGui::EndCombo();
    }

    // Show current process name if set (from a configurable process selection)
    if (!m_currentProcessName.empty()) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "[%s]", m_currentProcessName.c_str());
    }
  }

  void ProcessConfigUI::renderPresetManager() {
    ImGui::Text("Saved Configurations:");

    auto configs = m_configManager.getAvailableConfigs();

    // Filter configs for current process type
    std::vector<std::string> filteredConfigs;
    std::string prefix = m_currentProcess + "_";

    for (const auto& config : configs) {
      // Show configs that match current type or have no prefix (legacy)
      if (config.find(prefix) == 0 || config.find("_") == std::string::npos) {
        filteredConfigs.push_back(config);
      }
    }

    if (ImGui::BeginCombo("##LoadPreset",
      m_selectedPreset.empty() ? "Select preset..." : m_selectedPreset.c_str())) {
      for (const auto& name : filteredConfigs) {
        // Display name without prefix for cleaner UI
        std::string displayName = name;
        if (name.find(prefix) == 0) {
          displayName = name.substr(prefix.length());
        }

        if (ImGui::Selectable(displayName.c_str(), m_selectedPreset == name)) {
          m_selectedPreset = name;
          if (m_configManager.loadConfig(name, m_config)) {
            showStatus("Loaded: " + displayName);

            // Save to process-specific files after loading preset
            if (!m_currentProcessName.empty()) {
              std::string processFile = "last_" + m_currentProcessName + "_config.json";
              m_config.saveToFile(processFile);
            }

            if (!m_currentProcess.empty()) {
              std::string typeFile = "last_" + m_currentProcess + "_config.json";
              m_config.saveToFile(typeFile);
            }

            m_config.saveToFile("last_used_config.json");
          }
        }
      }
      ImGui::EndCombo();
    }

    // Pre-populate the name field with the prefix
    std::string prefixedName = prefix + std::string(m_configName);

    ImGui::Text("Name: %s", prefix.c_str());
    ImGui::SameLine();

    ImGui::SetNextItemWidth(200);
    ImGui::InputText("##NameInput", m_configName, sizeof(m_configName));

    if (ImGui::Button("Save")) {
      if (strlen(m_configName) > 0) {
        // Save with prefix
        std::string fullName = prefix + std::string(m_configName);

        if (m_configManager.saveConfig(fullName, m_config)) {
          showStatus("Saved: " + std::string(m_configName) + " (as " + fullName + ")");
          m_selectedPreset = fullName;

          // Also update process-specific files
          if (!m_currentProcessName.empty()) {
            std::string processFile = "last_" + m_currentProcessName + "_config.json";
            m_config.saveToFile(processFile);
          }

          if (!m_currentProcess.empty()) {
            std::string typeFile = "last_" + m_currentProcess + "_config.json";
            m_config.saveToFile(typeFile);
          }
        }
      }
    }

    ImGui::SameLine();

    if (ImGui::Button("Delete") && !m_selectedPreset.empty()) {
      // Show the actual filename being deleted
      std::string displayName = m_selectedPreset;
      if (displayName.find(prefix) == 0) {
        displayName = displayName.substr(prefix.length());
      }

      if (m_configManager.deleteConfig(m_selectedPreset)) {
        showStatus("Deleted: " + displayName);
        m_selectedPreset.clear();
      }
    }

    // Show hint about prefixes
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
      "Configs are saved with '%s' prefix", prefix.c_str());
  }

  void ProcessConfigUI::renderConfigEditor() {
    // Get modified values once at the start
    auto modified = m_config.getModifiedValues();

    // Status message area with fixed height (always at top)
    ImGui::BeginChild("StatusArea", ImVec2(0, 25), false, ImGuiWindowFlags_NoScrollbar);
    {
      if (!m_statusMessage.empty() && m_statusMessageTimer > 0) {
        float alpha = (std::min)(m_statusMessageTimer, 1.0f);
        ImGui::TextColored(ImVec4(0, 1, 0, alpha), "%s", m_statusMessage.c_str());
      }
      else {
        // Placeholder to maintain consistent height
        ImGui::Text(" "); // Empty space
      }
    }
    ImGui::EndChild();

    ImGui::Separator();

    // Show modified count (only if flag is true)
    if (m_showModifiedCount) {
      if (!modified.empty()) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1),
          "%d values modified from defaults", (int)modified.size());
      }
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
              // Auto-save when values change
              autoSaveConfig();
            }

            ImGui::PopID();
          }

          ImGui::EndTable();
        }
      }
    }
  }

  void ProcessConfigUI::renderActions() {
    if (ImGui::Button("Reset All to Defaults", ImVec2(150, 0))) {
      m_config.resetToDefaults();
      showStatus("Reset to default values");
      autoSaveConfig();
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

    // Add auto-save toggle
    ImGui::SameLine();
    if (ImGui::Checkbox("Auto-save", &m_autoSave)) {
      showStatus(m_autoSave ? "Auto-save enabled" : "Auto-save disabled");
    }
  }

  void ProcessConfigUI::showStatus(const std::string& message, float duration) {
    m_statusMessage = message;
    m_statusMessageTimer = duration;
  }

} // namespace UAA3ProcessBuilders