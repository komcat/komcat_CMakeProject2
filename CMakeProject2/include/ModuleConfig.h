#pragma once
#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>

// Add ImGui include for the UI functionality
#include "imgui.h"

// ModuleConfig.h
class ModuleConfig {
private:
  std::map<std::string, bool> moduleStates;
  std::string configFilePath;

  // Trim whitespace from string
  std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
  }

public:
  ModuleConfig(const std::string& filePath = "module_config.ini")
    : configFilePath(filePath) {
    loadConfig();
  }

  // Load configuration from INI file
  bool loadConfig() {
    std::ifstream file(configFilePath);
    if (!file.is_open()) {
      std::cout << "Config file not found, creating default: " << configFilePath << std::endl;
      createDefaultConfig();
      return true;
    }

    std::string line;
    while (std::getline(file, line)) {
      line = trim(line);

      // Skip empty lines and comments
      if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') {
        continue;
      }

      // Parse key=value pairs
      size_t equalPos = line.find('=');
      if (equalPos != std::string::npos) {
        std::string key = trim(line.substr(0, equalPos));
        std::string value = trim(line.substr(equalPos + 1));

        // Convert value to boolean
        bool enabled = (value == "1" || value == "true" || value == "TRUE" || value == "True");
        moduleStates[key] = enabled;
      }
    }
    file.close();
    return true;
  }

  // Save configuration to INI file
  bool saveConfig() {
    std::ofstream file(configFilePath);
    if (!file.is_open()) {
      std::cerr << "Failed to save config file: " << configFilePath << std::endl;
      return false;
    }

    file << "; Module Configuration File\n";
    file << "; Set to 1 to enable, 0 to disable\n";
    file << "; Changes require application restart\n\n";

    file << "[MOTION_CONTROLLERS]\n";
    file << "PI_CONTROLLERS=" << (isEnabled("PI_CONTROLLERS") ? "1" : "0") << "\n";
    file << "ACS_CONTROLLERS=" << (isEnabled("ACS_CONTROLLERS") ? "1" : "0") << "\n";
    file << "MOTION_CONTROL_LAYER=" << (isEnabled("MOTION_CONTROL_LAYER") ? "1" : "0") << "\n\n";

    file << "[IO_SYSTEMS]\n";
    file << "EZIIO_MANAGER=" << (isEnabled("EZIIO_MANAGER") ? "1" : "0") << "\n";
    file << "PNEUMATIC_SYSTEM=" << (isEnabled("PNEUMATIC_SYSTEM") ? "1" : "0") << "\n";
    file << "IO_CONTROL_PANEL=" << (isEnabled("IO_CONTROL_PANEL") ? "1" : "0") << "\n\n";

    file << "[CAMERAS]\n";
    file << "PYLON_CAMERA=" << (isEnabled("PYLON_CAMERA") ? "1" : "0") << "\n";
    file << "CAMERA_EXPOSURE_TEST=" << (isEnabled("CAMERA_EXPOSURE_TEST") ? "1" : "0") << "\n\n";

    file << "[DATA_SYSTEMS]\n";
    file << "DATA_CLIENT_MANAGER=" << (isEnabled("DATA_CLIENT_MANAGER") ? "1" : "0") << "\n";
    file << "DATA_CHART_MANAGER=" << (isEnabled("DATA_CHART_MANAGER") ? "1" : "0") << "\n";
    file << "GLOBAL_DATA_STORE=" << (isEnabled("GLOBAL_DATA_STORE") ? "1" : "0") << "\n\n";

    file << "[SCANNING_SYSTEMS]\n";
    file << "SCANNING_UI_V1=" << (isEnabled("SCANNING_UI_V1") ? "1" : "0") << "\n";
    file << "OPTIMIZED_SCANNING_UI=" << (isEnabled("OPTIMIZED_SCANNING_UI") ? "1" : "0") << "\n\n";

    file << "[LASER_SYSTEMS]\n";
    file << "CLD101X_MANAGER=" << (isEnabled("CLD101X_MANAGER") ? "1" : "0") << "\n";
    file << "PYTHON_PROCESS_MANAGER=" << (isEnabled("PYTHON_PROCESS_MANAGER") ? "1" : "0") << "\n\n";

    file << "[USER_INTERFACE]\n";
    file << "VERTICAL_TOOLBAR=" << (isEnabled("VERTICAL_TOOLBAR") ? "1" : "0") << "\n";
    file << "CONFIG_EDITOR=" << (isEnabled("CONFIG_EDITOR") ? "1" : "0") << "\n";
    file << "GRAPH_VISUALIZER=" << (isEnabled("GRAPH_VISUALIZER") ? "1" : "0") << "\n";
    file << "GLOBAL_JOG_PANEL=" << (isEnabled("GLOBAL_JOG_PANEL") ? "1" : "0") << "\n\n";

    file << "[SCRIPTING]\n";
    file << "SCRIPT_EDITOR=" << (isEnabled("SCRIPT_EDITOR") ? "1" : "0") << "\n";
    file << "SCRIPT_RUNNER=" << (isEnabled("SCRIPT_RUNNER") ? "1" : "0") << "\n";
    file << "SCRIPT_PRINT_VIEWER=" << (isEnabled("SCRIPT_PRINT_VIEWER") ? "1" : "0") << "\n\n";

    file << "[PROCESSES]\n";
    file << "PROCESS_CONTROL_PANEL=" << (isEnabled("PROCESS_CONTROL_PANEL") ? "1" : "0") << "\n";
    file << "INITIALIZATION_WINDOW=" << (isEnabled("INITIALIZATION_WINDOW") ? "1" : "0") << "\n";
    file << "PRODUCT_CONFIG_MANAGER=" << (isEnabled("PRODUCT_CONFIG_MANAGER") ? "1" : "0") << "\n\n";

    file << "[OVERLAYS]\n";
    file << "FPS_OVERLAY=" << (isEnabled("FPS_OVERLAY") ? "1" : "0") << "\n";
    file << "CLOCK_OVERLAY=" << (isEnabled("CLOCK_OVERLAY") ? "1" : "0") << "\n";
    file << "DIGITAL_DISPLAY=" << (isEnabled("DIGITAL_DISPLAY") ? "1" : "0") << "\n";
    file << "MINIMIZE_EXIT_BUTTONS=" << (isEnabled("MINIMIZE_EXIT_BUTTONS") ? "1" : "0") << "\n\n";

    file.close();
    return true;
  }

  // Create default configuration
  void createDefaultConfig() {
    // Set all modules to enabled by default
    moduleStates["PI_CONTROLLERS"] = true;
    moduleStates["ACS_CONTROLLERS"] = true;
    moduleStates["MOTION_CONTROL_LAYER"] = true;
    moduleStates["EZIIO_MANAGER"] = true;
    moduleStates["PNEUMATIC_SYSTEM"] = true;
    moduleStates["IO_CONTROL_PANEL"] = true;
    moduleStates["PYLON_CAMERA"] = true;
    moduleStates["CAMERA_EXPOSURE_TEST"] = true;
    moduleStates["DATA_CLIENT_MANAGER"] = true;
    moduleStates["DATA_CHART_MANAGER"] = true;
    moduleStates["GLOBAL_DATA_STORE"] = true;
    moduleStates["SCANNING_UI_V1"] = true;
    moduleStates["OPTIMIZED_SCANNING_UI"] = true;
    moduleStates["CLD101X_MANAGER"] = true;
    moduleStates["PYTHON_PROCESS_MANAGER"] = true;
    moduleStates["VERTICAL_TOOLBAR"] = true;
    moduleStates["CONFIG_EDITOR"] = true;
    moduleStates["GRAPH_VISUALIZER"] = true;
    moduleStates["GLOBAL_JOG_PANEL"] = true;
    moduleStates["SCRIPT_EDITOR"] = true;
    moduleStates["SCRIPT_RUNNER"] = true;
    moduleStates["SCRIPT_PRINT_VIEWER"] = true;
    moduleStates["PROCESS_CONTROL_PANEL"] = true;
    moduleStates["INITIALIZATION_WINDOW"] = true;
    moduleStates["PRODUCT_CONFIG_MANAGER"] = true;
    moduleStates["FPS_OVERLAY"] = true;
    moduleStates["CLOCK_OVERLAY"] = true;
    moduleStates["DIGITAL_DISPLAY"] = true;
    moduleStates["MINIMIZE_EXIT_BUTTONS"] = true;

    saveConfig();
  }

  // Check if a module is enabled
  bool isEnabled(const std::string& moduleName) const {
    auto it = moduleStates.find(moduleName);
    return (it != moduleStates.end()) ? it->second : false;
  }

  // Enable/disable a module
  void setEnabled(const std::string& moduleName, bool enabled) {
    moduleStates[moduleName] = enabled;
  }

  // Get all module states
  const std::map<std::string, bool>& getAllModuleStates() const {
    return moduleStates;
  }

  // Print current configuration to console
  void printConfig() const {
    std::cout << "\n=== Module Configuration ===" << std::endl;
    std::cout << "Config file: " << configFilePath << std::endl;
    std::cout << "Modules loaded: " << moduleStates.size() << std::endl;
    std::cout << "Status:" << std::endl;

    for (const auto& [module, enabled] : moduleStates) {
      std::cout << "  " << module << " = " << (enabled ? "ENABLED" : "DISABLED") << std::endl;
    }
    std::cout << "===========================\n" << std::endl;
  }

  // Runtime configuration UI (ImGui)
  void renderConfigUI() {
    static bool showConfig = false;

    // Add menu to main menu bar
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("Configuration")) {
        if (ImGui::MenuItem("Module Settings")) {
          showConfig = !showConfig;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Reload Config")) {
          loadConfig();
        }
        if (ImGui::MenuItem("Save Config")) {
          saveConfig();
        }
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }

    if (showConfig) {
      ImGui::SetNextWindowSize(ImVec2(600, 800), ImGuiCond_FirstUseEver);
      ImGui::Begin("Module Configuration", &showConfig);

      ImGui::TextWrapped("Module Enable/Disable Settings");
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
        "Note: Changes require application restart to take effect");
      ImGui::Separator();

      bool changed = false;

      // Motion Controllers Section
      if (ImGui::CollapsingHeader("Motion Controllers", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool piEnabled = isEnabled("PI_CONTROLLERS");
        if (ImGui::Checkbox("PI Controllers", &piEnabled)) {
          setEnabled("PI_CONTROLLERS", piEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Hexapod motion controllers)");

        bool acsEnabled = isEnabled("ACS_CONTROLLERS");
        if (ImGui::Checkbox("ACS Controllers", &acsEnabled)) {
          setEnabled("ACS_CONTROLLERS", acsEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Gantry motion controllers)");

        bool motionEnabled = isEnabled("MOTION_CONTROL_LAYER");
        if (ImGui::Checkbox("Motion Control Layer", &motionEnabled)) {
          setEnabled("MOTION_CONTROL_LAYER", motionEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(High-level motion coordination)");
      }

      // IO Systems Section
      if (ImGui::CollapsingHeader("IO Systems")) {
        bool ioEnabled = isEnabled("EZIIO_MANAGER");
        if (ImGui::Checkbox("EziIO Manager", &ioEnabled)) {
          setEnabled("EZIIO_MANAGER", ioEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Digital I/O control system)");

        bool pneumaticEnabled = isEnabled("PNEUMATIC_SYSTEM");
        if (ImGui::Checkbox("Pneumatic System", &pneumaticEnabled)) {
          setEnabled("PNEUMATIC_SYSTEM", pneumaticEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Pneumatic slide controls)");

        bool ioControlEnabled = isEnabled("IO_CONTROL_PANEL");
        if (ImGui::Checkbox("IO Control Panel", &ioControlEnabled)) {
          setEnabled("IO_CONTROL_PANEL", ioControlEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Quick IO control interface)");
      }

      // Camera Systems Section
      if (ImGui::CollapsingHeader("Camera Systems")) {
        bool cameraEnabled = isEnabled("PYLON_CAMERA");
        if (ImGui::Checkbox("Pylon Camera", &cameraEnabled)) {
          setEnabled("PYLON_CAMERA", cameraEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Basler camera integration)");

        bool exposureTestEnabled = isEnabled("CAMERA_EXPOSURE_TEST");
        if (ImGui::Checkbox("Camera Exposure Test", &exposureTestEnabled)) {
          setEnabled("CAMERA_EXPOSURE_TEST", exposureTestEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Camera testing utilities)");
      }

      // Data Systems Section
      if (ImGui::CollapsingHeader("Data Systems")) {
        bool dataClientEnabled = isEnabled("DATA_CLIENT_MANAGER");
        if (ImGui::Checkbox("Data Client Manager", &dataClientEnabled)) {
          setEnabled("DATA_CLIENT_MANAGER", dataClientEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(TCP/IP data connections)");

        bool dataChartEnabled = isEnabled("DATA_CHART_MANAGER");
        if (ImGui::Checkbox("Data Chart Manager", &dataChartEnabled)) {
          setEnabled("DATA_CHART_MANAGER", dataChartEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Real-time data visualization)");

        bool globalDataEnabled = isEnabled("GLOBAL_DATA_STORE");
        if (ImGui::Checkbox("Global Data Store", &globalDataEnabled)) {
          setEnabled("GLOBAL_DATA_STORE", globalDataEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Central data repository)");
      }

      // Scanning Systems Section
      if (ImGui::CollapsingHeader("Scanning Systems")) {
        bool scanV1Enabled = isEnabled("SCANNING_UI_V1");
        if (ImGui::Checkbox("Scanning UI V1", &scanV1Enabled)) {
          setEnabled("SCANNING_UI_V1", scanV1Enabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Original scanning interface)");

        bool scanV2Enabled = isEnabled("OPTIMIZED_SCANNING_UI");
        if (ImGui::Checkbox("Optimized Scanning UI", &scanV2Enabled)) {
          setEnabled("OPTIMIZED_SCANNING_UI", scanV2Enabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Enhanced scanning interface)");
      }

      // Laser Systems Section
      if (ImGui::CollapsingHeader("Laser Systems")) {
        bool laserEnabled = isEnabled("CLD101X_MANAGER");
        if (ImGui::Checkbox("CLD101x Manager", &laserEnabled)) {
          setEnabled("CLD101X_MANAGER", laserEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Laser TEC controller)");

        bool pythonEnabled = isEnabled("PYTHON_PROCESS_MANAGER");
        if (ImGui::Checkbox("Python Process Manager", &pythonEnabled)) {
          setEnabled("PYTHON_PROCESS_MANAGER", pythonEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Python script integration)");
      }

      // User Interface Section
      if (ImGui::CollapsingHeader("User Interface")) {
        bool toolbarEnabled = isEnabled("VERTICAL_TOOLBAR");
        if (ImGui::Checkbox("Vertical Toolbar", &toolbarEnabled)) {
          setEnabled("VERTICAL_TOOLBAR", toolbarEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Main navigation toolbar)");

        bool configEditorEnabled = isEnabled("CONFIG_EDITOR");
        if (ImGui::Checkbox("Config Editor", &configEditorEnabled)) {
          setEnabled("CONFIG_EDITOR", configEditorEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Motion configuration editor)");

        bool graphVisualizerEnabled = isEnabled("GRAPH_VISUALIZER");
        if (ImGui::Checkbox("Graph Visualizer", &graphVisualizerEnabled)) {
          setEnabled("GRAPH_VISUALIZER", graphVisualizerEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Path visualization tool)");

        bool jogPanelEnabled = isEnabled("GLOBAL_JOG_PANEL");
        if (ImGui::Checkbox("Global Jog Panel", &jogPanelEnabled)) {
          setEnabled("GLOBAL_JOG_PANEL", jogPanelEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Manual motion control)");
      }

      // Scripting Section
      if (ImGui::CollapsingHeader("Scripting")) {
        bool scriptEditorEnabled = isEnabled("SCRIPT_EDITOR");
        if (ImGui::Checkbox("Script Editor", &scriptEditorEnabled)) {
          setEnabled("SCRIPT_EDITOR", scriptEditorEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Script development interface)");

        bool scriptRunnerEnabled = isEnabled("SCRIPT_RUNNER");
        if (ImGui::Checkbox("Script Runner", &scriptRunnerEnabled)) {
          setEnabled("SCRIPT_RUNNER", scriptRunnerEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Script execution engine)");

        bool scriptViewerEnabled = isEnabled("SCRIPT_PRINT_VIEWER");
        if (ImGui::Checkbox("Script Print Viewer", &scriptViewerEnabled)) {
          setEnabled("SCRIPT_PRINT_VIEWER", scriptViewerEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Script output console)");
      }

      // Process Control Section
      if (ImGui::CollapsingHeader("Process Control")) {
        bool processControlEnabled = isEnabled("PROCESS_CONTROL_PANEL");
        if (ImGui::Checkbox("Process Control Panel", &processControlEnabled)) {
          setEnabled("PROCESS_CONTROL_PANEL", processControlEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Main process control interface)");

        bool initWindowEnabled = isEnabled("INITIALIZATION_WINDOW");
        if (ImGui::Checkbox("Initialization Window", &initWindowEnabled)) {
          setEnabled("INITIALIZATION_WINDOW", initWindowEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(System initialization interface)");

        bool productConfigEnabled = isEnabled("PRODUCT_CONFIG_MANAGER");
        if (ImGui::Checkbox("Product Config Manager", &productConfigEnabled)) {
          setEnabled("PRODUCT_CONFIG_MANAGER", productConfigEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Product configuration management)");
      }

      // Overlays Section
      if (ImGui::CollapsingHeader("Overlays")) {
        bool fpsEnabled = isEnabled("FPS_OVERLAY");
        if (ImGui::Checkbox("FPS Overlay", &fpsEnabled)) {
          setEnabled("FPS_OVERLAY", fpsEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Performance monitoring)");

        bool clockEnabled = isEnabled("CLOCK_OVERLAY");
        if (ImGui::Checkbox("Clock Overlay", &clockEnabled)) {
          setEnabled("CLOCK_OVERLAY", clockEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(System time display)");

        bool digitalDisplayEnabled = isEnabled("DIGITAL_DISPLAY");
        if (ImGui::Checkbox("Digital Display", &digitalDisplayEnabled)) {
          setEnabled("DIGITAL_DISPLAY", digitalDisplayEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Data value displays)");

        bool exitButtonsEnabled = isEnabled("MINIMIZE_EXIT_BUTTONS");
        if (ImGui::Checkbox("Minimize/Exit Buttons", &exitButtonsEnabled)) {
          setEnabled("MINIMIZE_EXIT_BUTTONS", exitButtonsEnabled);
          changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Window control buttons)");
      }

      ImGui::Separator();

      // Configuration actions
      if (changed) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
        ImGui::Text("Configuration has been modified!");
        ImGui::PopStyleColor();

        if (ImGui::Button("Save Configuration", ImVec2(150, 0))) {
          if (saveConfig()) {
            ImGui::OpenPopup("Save Success");
          }
          else {
            ImGui::OpenPopup("Save Failed");
          }
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard Changes", ImVec2(150, 0))) {
          loadConfig(); // Reload from file
        }
      }
      else {
        if (ImGui::Button("Reload from File", ImVec2(150, 0))) {
          loadConfig();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset to Defaults", ImVec2(150, 0))) {
          ImGui::OpenPopup("Reset Confirm");
        }
      }

      // Popups
      if (ImGui::BeginPopupModal("Save Success", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Configuration saved successfully!");
        ImGui::Text("Restart the application to apply changes.");
        if (ImGui::Button("OK", ImVec2(120, 0))) {
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }

      if (ImGui::BeginPopupModal("Save Failed", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Failed to save configuration file!");
        ImGui::Text("Check file permissions and try again.");
        if (ImGui::Button("OK", ImVec2(120, 0))) {
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }

      if (ImGui::BeginPopupModal("Reset Confirm", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Reset all modules to default enabled state?");
        ImGui::Text("This will overwrite the current configuration.");
        ImGui::Separator();
        if (ImGui::Button("Reset", ImVec2(120, 0))) {
          createDefaultConfig();
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }

      ImGui::End();
    }
  }
};