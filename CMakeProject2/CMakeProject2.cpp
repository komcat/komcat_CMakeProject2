#include <algorithm> // For std::min and std::max 
#include <fstream>
#include <nlohmann/json.hpp>
#include "CMakeProject2.h"
//#include "include/client_manager.h" // Replace tcp_client.h with our new manager
#include "include/cld101x_client.h"
#include "include/cld101x_manager.h"
#include "include/data/data_client_manager.h"  // Add this at the top with other includes

#include "include/SMU/keithley2400_client.h"
#include "include/SMU/keithley2400_manager.h"
#include "include/SMU/keithley2400_operations.h"

#include "include/logger.h" // Include our new logger header
#include "include/motions/MotionTypes.h"  // Add this line - include MotionTypes.h first
#include "include/motions/MotionConfigEditor.h" // Include our new editor header
#include "include/motions/MotionConfigManager.h"
#include "include/motions/pi_controller.h" // Include the PI controller header
#include "include/motions/pi_controller_manager.h" // Include the PI controller manager header
#include "include/ui/toolbar.h" // Include the toolbar header
#include "include/ui/ToolbarMenu.h"
#include "include/ui/GraphVisualizer.h"
#include "include/ui/ToggleableUIAdapter.h" // Basic adapter
#include "include/ui/ControllerAdapters.h"  // Custom adapters for controllers
#include "include/camera/pylon_camera_test.h" // Include the Pylon camera test header


// Add these includes at the top with the other include statements
#include "include/motions/acs_controller.h"
#include "include/motions/acs_controller_manager.h"
#include "include/motions/motion_control_layer.h" // Add this include for the motion control layer

// ImGui and SDL includes
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

// Include SDL2
#include <SDL.h>
#include <SDL_opengl.h>

#include "include/eziio/EziIO_Manager.h"
#include "include/eziio/EziIO_UI.h"
#include "include/eziio/PneumaticManager.h"
#include "include/eziio/PneumaticUI.h"
#include "IOConfigManager.h"
#include "include/motions/pi_analog_reader.h"
#include "include/motions/pi_analog_manager.h"
#include "include/motions/global_jog_panel.h"
#include "include/data/product_config_manager.h"
#include "include/eziio/IOControlPanel.h"
// Add these includes at the top with the other include statements

#include "include/python_process_managaer.h"
// Add these includes at the top with the other include statements
#include "include/scanning/scanning_ui.h"
#include "include/scanning/scanning_algorithm.h"
#include "include/scanning/scanning_parameters.h"
#include "include/scanning/scan_data_collector.h"
#include "include/data/global_data_store.h" // Add this with your other includes
#include "implot/implot.h"
#include "include/data/DataChartManager.h"
#include "include/SequenceStep.h"
#include "include/machine_operations.h"
#include "InitializationWindow.h"
//#include "include/HexRightControllerWindow.h"

#include "include/ProcessControlPanel.h"
#include "include/cld101x_operations.h"
#include <iostream>
#include <memory>
#include "include/ui/VerticalToolbarMenu.h"
#include "include/ui/HierarchicalControllerAdapters.h" // Include the new adapter file
#include "include/ui/MotionControlHierarchicalAdapter.h"
#include "include/camera/PylonCameraAdapter.h"
// In your main.cpp or equivalent file
#include "include/script/script_editor_ui.h"
#include "include/ui/MotionGraphic.h"
#include "include/script/script_runner.h"
#include "include/script/ScriptRunnerAdapter.h"
#include "include/script/script_print_viewer.h"
#include "include/camera/CameraExposureTestUI.h"
#include "include/scanning/optimized_scanning_ui.h"
#include "include/ModuleConfig.h"
#include "Version.h"

// CMakeProject2.cpp - MachineBlockUI Integration Example

// Add this include at the top with other UI includes
#include "Programming/MachineBlockUI.h"
#include "Programming/virtual_machine_operations.h"
#include "Programming/virtual_machine_operations_adapter.h"
#include "include/data/DigitalDisplayWithChart.h"
#include "Programming/MacroManager.h"

#include "include/ui/MenuManager.h"





#pragma region header functions



void RenderDraggableOverlay() {
	// Set window position and style
	ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowBgAlpha(0.7f); // Semi-transparent background

	// Create a window with minimal decorations but allowing movement
	ImGui::Begin("DraggableOverlay", nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_AlwaysAutoResize);

	// Your overlay content here
	ImGui::Text("Drag me!");
	ImGui::Text("Status: Connected"); 

	ImGui::End();
}

void RenderClockOverlay(int corner) {
	// Get current time
	time_t rawtime;
	struct tm timeinfo;
	char timeBuffer[80];
	char dateBuffer[80];

	time(&rawtime);

#ifdef _WIN32
	localtime_s(&timeinfo, &rawtime);
#else
	timeinfo = *localtime(&rawtime);
#endif

	// Format time (HH:MM:SS)
	strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);

	// Format date (Day Month Year)
	strftime(dateBuffer, sizeof(dateBuffer), "%d %b %Y", &timeinfo);

	// Get screen dimensions
	ImVec2 screenSize = ImGui::GetIO().DisplaySize;

	// Calculate position based on the selected corner
	// 0: top-left, 1: top-right, 2: bottom-left, 3: bottom-right
	ImVec2 pos;
	float padding = 30;
	float clockWidth = 150.0f;
	float clockHeight = 60.0f;

	switch (corner) {
	case 0: // Top-left
		pos = ImVec2(padding, padding);
		break;
	case 1: // Top-right
		pos = ImVec2(screenSize.x - clockWidth - padding, padding);
		break;
	case 2: // Bottom-left
		pos = ImVec2(padding, screenSize.y - clockHeight - padding);
		break;
	case 3: // Bottom-right
		pos = ImVec2(screenSize.x - clockWidth - 0/*padding*/, screenSize.y - clockHeight - padding);
		break;
	default: // Default to top-right
		pos = ImVec2(screenSize.x - clockWidth - padding, padding);
	}

	// Set window position and style
	ImGui::SetNextWindowPos(pos);
	ImGui::SetNextWindowBgAlpha(0.7f); // Semi-transparent background

	// Custom colors for the clock
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.1f, 0.7f)); // Dark blue background
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 1.0f, 1.0f)); // Light text

	// Create a window with no decorations and no movement
	ImGui::Begin("ClockOverlay", nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | // Prevent moving
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoBringToFrontOnFocus // Keeps it from being pushed back in z-order
	);

	// Add some padding and styling
	ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Use default font

	// Display time in a larger font
	ImGui::SetWindowFontScale(1.5f);
	ImGui::Text("%s", timeBuffer);

	// Reset font scale for date
	ImGui::SetWindowFontScale(1.0f);
	ImGui::Text("%s", dateBuffer);

	// Pop font
	ImGui::PopFont();

	ImGui::End();

	// Pop style colors
	ImGui::PopStyleColor(2);
}
void RenderGlobalDataValue(const std::string& dataName) {
	// Get the value from the global data store
	float value = GlobalDataStore::GetInstance()->GetValue(dataName);

	// Get the display name and unit (assuming you have a way to get these)
	std::string displayName = dataName;
	std::string unit = ""; // You might want to add a way to get the unit for each data name

	// If you have a mapping for prettier display names:
	std::map<std::string, std::pair<std::string, std::string>> displayMap = {
			{"GPIB-Current", {"Current", "A"}},
			{"hex-right-A-5", {"Voltage R5", "V"}},
			// Add more mappings as needed
	};

	// Look up display name and unit if available
	auto it = displayMap.find(dataName);
	if (it != displayMap.end()) {
		displayName = it->second.first;
		unit = it->second.second;
	}

	// Create a unique window name using the data name
	std::string windowName = "GlobalData_" + dataName;

	// Set window position and style
	ImGui::SetNextWindowPos(ImVec2(50, 150), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowBgAlpha(0.7f); // Semi-transparent background

	// Custom styling for the window
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.1f, 0.7f)); // Dark blue background
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 1.0f, 1.0f)); // Light text

	// Create a window with minimal decorations but allowing movement
	ImGui::Begin(windowName.c_str(), nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_AlwaysAutoResize);

	// Show the name of the data
	ImGui::Text("%s", displayName.c_str());

	// Set larger font scale (5x) for the value
	float originalFontScale = ImGui::GetFontSize();
	ImGui::SetWindowFontScale(5.0f);

	// Format the value based on its magnitude
	char valueBuffer[32];
	if (std::abs(value) < 0.01) {
		snprintf(valueBuffer, sizeof(valueBuffer), "%.5f", value);
	}
	else if (std::abs(value) < 1.0) {
		snprintf(valueBuffer, sizeof(valueBuffer), "%.4f", value);
	}
	else if (std::abs(value) < 10.0) {
		snprintf(valueBuffer, sizeof(valueBuffer), "%.3f", value);
	}
	else if (std::abs(value) < 100.0) {
		snprintf(valueBuffer, sizeof(valueBuffer), "%.2f", value);
	}
	else {
		snprintf(valueBuffer, sizeof(valueBuffer), "%.1f", value);
	}

	// Display the value
	ImGui::Text("%s", valueBuffer);

	// Reset font scale and display the unit
	ImGui::SetWindowFontScale(1.0f);
	ImGui::SameLine();
	ImGui::Text("%s", unit.c_str());

	ImGui::End();

	// Pop style colors
	ImGui::PopStyleColor(2);
}
void RenderGlobalDataValueSI(const std::string& dataName) {
	// Get the value from the global data store
	float value = GlobalDataStore::GetInstance()->GetValue(dataName);

	// Display name mapping
	std::string displayName = dataName;
	std::map<std::string, std::string> displayMap = {
			{"GPIB-Current", "Current"},
			{"hex-right-A-5", "Voltage R5"},
			// Add more mappings as needed
	};

	// Look up display name if available
	auto nameIt = displayMap.find(dataName);
	if (nameIt != displayMap.end()) {
		displayName = nameIt->second;
	}

	// Units and prefixes mapping - fixed to use proper float literals
	std::map<std::string, std::pair<std::string, std::vector<std::pair<float, std::string>>>> unitsMap = {
			{"GPIB-Current", {"A", {
					{1.0e-12f, "pA"},
					{1.0e-9f, "nA"},
					{1.0e-6f, "µA"},
					{1.0e-3f, "mA"},
					{1.0f, "A"}
			}}},
			{"hex-right-A-5", {"V", {
					{1.0e-12f, "pV"},
					{1.0e-9f, "nV"},
					{1.0e-6f, "µV"},
					{1.0e-3f, "mV"},
					{1.0f, "V"}
			}}},
		// Add more units as needed
	};
	// Get unit info
	std::string baseUnit = "";
	std::string unitPrefix = "";
	float scaledValue = value;

	auto unitIt = unitsMap.find(dataName);
	if (unitIt != unitsMap.end()) {
		baseUnit = unitIt->second.first;

		// Find the appropriate SI prefix
		for (const auto& prefix : unitIt->second.second) {
			if (std::abs(value) < prefix.first * 1000 ||
				prefix.first == unitIt->second.second.back().first) {
				unitPrefix = prefix.second;
				scaledValue = value / prefix.first;
				break;
			}
		}
	}

	// Create a unique window name
	std::string windowName = "SI_" + dataName;

	// Set window position and style
	ImGui::SetNextWindowPos(ImVec2(50, 150), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(200, 80), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowBgAlpha(0.7f); // Semi-transparent background

	// Custom styling
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.15f, 0.7f)); // Dark background
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 1.0f, 1.0f)); // Light text

	// Create a window with minimal decorations but allowing movement
	ImGui::Begin(windowName.c_str(), nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings);

	// Display the name with normal font
	ImGui::Text("%s", displayName.c_str());

	// Display the unit on the right side
	float windowWidth = ImGui::GetWindowSize().x;
	ImGui::SameLine(windowWidth - ImGui::CalcTextSize(unitPrefix.c_str()).x - 20);
	ImGui::Text("%s", unitPrefix.c_str());

	// Display value with larger font in monospaced style
	ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Assume monospace font is available

	// Format value based on magnitude (always showing leading zero for values < 1)
	char valueBuffer[32];
	if (std::abs(scaledValue) < 1.0) {
		// Format like "0.000000" - with appropriate number of decimal places
		// For small values, show more decimal places to maintain precision
		if (std::abs(scaledValue) < 0.01) {
			snprintf(valueBuffer, sizeof(valueBuffer), "0.%06d",
				static_cast<int>(std::abs(scaledValue) * 1000000));
		}
		else if (std::abs(scaledValue) < 0.1) {
			snprintf(valueBuffer, sizeof(valueBuffer), "0.%05d",
				static_cast<int>(std::abs(scaledValue) * 100000));
		}
		else {
			snprintf(valueBuffer, sizeof(valueBuffer), "0.%04d",
				static_cast<int>(std::abs(scaledValue) * 10000));
		}
	}
	else {
		// Format value with 5 digits total (combined whole number and decimal)
		int intPart = static_cast<int>(scaledValue);
		int decimalDigits = 5 - (intPart == 0 ? 1 :
			(intPart < 10 ? 1 :
				(intPart < 100 ? 2 :
					(intPart < 1000 ? 3 :
						(intPart < 10000 ? 4 : 5)))));

		if (decimalDigits <= 0) {
			snprintf(valueBuffer, sizeof(valueBuffer), "%d", intPart);
		}
		else {
			char formatStr[16];
			snprintf(formatStr, sizeof(formatStr), "%%.%df", decimalDigits);
			snprintf(valueBuffer, sizeof(valueBuffer), formatStr, scaledValue);
		}
	}

	// Set larger font for the value
	ImGui::SetWindowFontScale(3.0f);
	ImGui::Text("%s", valueBuffer);
	ImGui::SetWindowFontScale(1.0f);

	ImGui::PopFont();
	ImGui::End();

	// Pop style colors
	ImGui::PopStyleColor(2);
}

void RenderDigitalDisplaySI(const std::string& dataName) {
  // Static variables to persist between calls
  static std::string selectedDataName = dataName;
  static std::vector<std::pair<std::string, std::string>> availableChannels;
  static bool channelsLoaded = false;
  static bool showPopup = false;

  // Load channels from config file AND global data store
  auto loadChannelsFromConfig = []() {
    availableChannels.clear();
    std::set<std::string> channelIds; // To track duplicates

    // First, load from config file
    try {
      std::ifstream configFile("data_display_config.json");
      if (configFile.is_open()) {
        nlohmann::json config;
        configFile >> config;

        if (config.contains("channels") && config["channels"].is_array()) {
          for (const auto& channel : config["channels"]) {
            std::string id = channel.value("id", "");
            std::string displayName = channel.value("displayName", id);
            bool enabled = channel.value("enable", true);

            if (!id.empty() && enabled) {
              availableChannels.emplace_back(id, displayName);
              channelIds.insert(id); // Track this ID as already added
            }
          }
        }
        configFile.close();
      }
    }
    catch (const std::exception& e) {
      // If config loading fails, we'll still add global store channels below
    }

    // Second, add channels from global data store that aren't in the config
    if (GlobalDataStore::GetInstance()) {
      std::vector<std::string> globalChannels = GlobalDataStore::GetInstance()->GetAvailableChannels();

      for (const std::string& channelId : globalChannels) {
        // Only add if not already in the list from config file
        if (channelIds.find(channelId) == channelIds.end()) {
          // Use the channel ID as display name, but make it more readable
          std::string displayName = channelId;

          // Optional: Make display names more readable
          // Replace underscores with spaces and capitalize
          std::replace(displayName.begin(), displayName.end(), '_', ' ');
          std::replace(displayName.begin(), displayName.end(), '-', ' ');

          // Capitalize first letter of each word
          bool capitalizeNext = true;
          for (char& c : displayName) {
            if (capitalizeNext && std::isalpha(c)) {
              c = std::toupper(c);
              capitalizeNext = false;
            }
            else if (c == ' ') {
              capitalizeNext = true;
            }
          }

          availableChannels.emplace_back(channelId, displayName + " (Auto)");
          channelIds.insert(channelId); // Track this ID
        }
      }
    }

    // Fallback channels if both config and global store are empty
    if (availableChannels.empty()) {
      availableChannels = {
        {"GPIB-Current", "Current Reading"},
        {"SMU1-Current", "SMU1 Current"},
        {"SMU1-Voltage", "SMU1 Voltage"},
        {"SMU1-Resistance", "SMU1 Resistance"},
        {"SMU1-Power", "SMU1 Power"}
      };
    }

    channelsLoaded = true;
  };

  // Load channels if not loaded yet
  if (!channelsLoaded) {
    loadChannelsFromConfig();
    selectedDataName = dataName; // Initialize with passed dataName
  }

  // Use the selected data name for display
  std::string currentDataName = selectedDataName;

  // Get the value from the global data store
  float value = GlobalDataStore::GetInstance()->GetValue(currentDataName);
  bool isNegative = (value < 0);
  float absValue = std::abs(value);

  // Name and unit mapping with SI prefixes
  std::string displayName = currentDataName;

  // Units and prefixes mapping
  struct UnitInfo {
    std::string baseUnit;
    std::vector<std::pair<float, std::string>> prefixes;
  };

  std::map<std::string, UnitInfo> unitsMap = {
      {"GPIB-Current", {"A", {
          {1e-12, "pA"},
          {1e-9, "nA"},
          {1e-6, "uA"},
          {1e-3, "mA"},
          {1, "A"}
      }}},
      {"SMU1-Current", {"A", {
          {1e-12, "pA"},
          {1e-9, "nA"},
          {1e-6, "uA"},
          {1e-3, "mA"},
          {1, "A"}
      }}},
      {"SMU1-Voltage", {"V", {
          {1e-12, "pV"},
          {1e-9, "nV"},
          {1e-6, "uV"},
          {1e-3, "mV"},
          {1, "V"}
      }}},
      {"SMU1-Resistance", {"Ω", {
          {1e-3, "mΩ"},
          {1, "Ω"},
          {1e3, "kΩ"},
          {1e6, "MΩ"}
      }}},
      {"SMU1-Power", {"W", {
          {1e-12, "pW"},
          {1e-9, "nW"},
          {1e-6, "uW"},
          {1e-3, "mW"},
          {1, "W"}
      }}},
      {"hex-right-A-5", {"V", {
          {1e-12, "pV"},
          {1e-9, "nV"},
          {1e-6, "uV"},
          {1e-3, "mV"},
          {1, "V"}
      }}},
      {"hex-left-A-5", {"V", {
          {1e-12, "pV"},
          {1e-9, "nV"},
          {1e-6, "uV"},
          {1e-3, "mV"},
          {1, "V"}
      }}},
      {"hex-right-A-6", {"V", {
          {1e-12, "pV"},
          {1e-9, "nV"},
          {1e-6, "uV"},
          {1e-3, "mV"},
          {1, "V"}
      }}},
      {"hex-left-A-6", {"V", {
          {1e-12, "pV"},
          {1e-9, "nV"},
          {1e-6, "uV"},
          {1e-3, "mV"},
          {1, "V"}
      }}},
      {"gantry", {"", {
          {1, ""}  // No unit for gantry
      }}},
    // Add more units as needed - the function will auto-detect channels not listed here
  };

  // Display name mapping
  std::map<std::string, std::string> displayMap = {
      {"GPIB-Current", "Current"},
      {"SMU1-Current", "SMU1 Current"},
      {"SMU1-Voltage", "SMU1 Voltage"},
      {"SMU1-Resistance", "SMU1 Resistance"},
      {"SMU1-Power", "SMU1 Power"},
      {"hex-right-A-5", "Voltage R5"},
      {"hex-left-A-5", "Voltage L5"},
      {"hex-right-A-6", "Voltage R6"},
      {"hex-left-A-6", "Voltage L6"},
      {"gantry", "gantry"},
      // Add more mappings as needed
  };

  // Look up display name if available
  auto nameIt = displayMap.find(currentDataName);
  if (nameIt != displayMap.end()) {
    displayName = nameIt->second;
  }

  // Get unit info and scale value
  std::string unitDisplay = "";
  float scaledValue = absValue;

  auto unitIt = unitsMap.find(currentDataName);
  if (unitIt != unitsMap.end()) {
    auto& prefixes = unitIt->second.prefixes;

    // Find the appropriate SI prefix
    for (size_t i = 0; i < prefixes.size(); i++) {
      const auto& prefix = prefixes[i];
      float threshold = 2000.0f; // Threshold to move to next unit

      // Check if this is the correct unit for the value
      if (i < prefixes.size() - 1) {
        // If value is less than threshold times this unit, use this unit
        if (absValue < prefix.first * threshold) {
          unitDisplay = prefix.second;
          scaledValue = absValue / prefix.first;
          break;
        }
        // Otherwise, try the next larger unit
      }
      else {
        // Last prefix in the list, use it regardless
        unitDisplay = prefix.second;
        scaledValue = absValue / prefix.first;
        break;
      }
    }
  }
  else {
    // For unknown channels, try to guess the unit based on the name
    std::string lowerName = currentDataName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    if (lowerName.find("current") != std::string::npos || lowerName.find("ampere") != std::string::npos) {
      // Assume it's current
      if (absValue < 1e-9) {
        scaledValue = absValue * 1e12;
        unitDisplay = "pA";
      }
      else if (absValue < 1e-6) {
        scaledValue = absValue * 1e9;
        unitDisplay = "nA";
      }
      else if (absValue < 1e-3) {
        scaledValue = absValue * 1e6;
        unitDisplay = "uA";
      }
      else if (absValue < 1) {
        scaledValue = absValue * 1e3;
        unitDisplay = "mA";
      }
      else {
        scaledValue = absValue;
        unitDisplay = "A";
      }
    }
    else if (lowerName.find("voltage") != std::string::npos || lowerName.find("volt") != std::string::npos) {
      // Assume it's voltage
      if (absValue < 1e-3) {
        scaledValue = absValue * 1e6;
        unitDisplay = "uV";
      }
      else if (absValue < 1) {
        scaledValue = absValue * 1e3;
        unitDisplay = "mV";
      }
      else {
        scaledValue = absValue;
        unitDisplay = "V";
      }
    }
    else {
      // Unknown unit, just display the raw value
      scaledValue = absValue;
      unitDisplay = "";
    }
  }

  // Create window name
  std::string windowName = "Digital_" + currentDataName;

  // Set window appearance
  ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(280, 120), ImGuiCond_FirstUseEver);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.15f, 0.2f, 0.95f)); // Dark blue/gray
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f); // Sharp corners
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f); // Thin border

  // Create window - resizable
  ImGui::Begin(windowName.c_str(), nullptr,
    ImGuiWindowFlags_NoTitleBar |
    ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoCollapse);

  // Check for right-click to open selection menu
  if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    showPopup = true;
    loadChannelsFromConfig(); // Reload config and global store channels when right-clicked
    ImGui::OpenPopup("SelectDataSource");
  }

  // Render the popup menu
  if (ImGui::BeginPopup("SelectDataSource")) {
    ImGui::Text("Select Data Source:");
    ImGui::Separator();

    // Group channels by source (config vs auto-detected)
    bool hasConfigChannels = false;
    bool hasAutoChannels = false;

    // Check what types of channels we have
    for (const auto& [id, displayName] : availableChannels) {
      if (displayName.find("(Auto)") != std::string::npos) {
        hasAutoChannels = true;
      }
      else {
        hasConfigChannels = true;
      }
    }

    // Show config channels first
    if (hasConfigChannels) {
      ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "Configured Channels:");
      for (const auto& [id, displayName] : availableChannels) {
        if (displayName.find("(Auto)") == std::string::npos) {
          bool isSelected = (id == currentDataName);

          if (ImGui::Selectable((displayName + "##" + id).c_str(), isSelected)) {
            selectedDataName = id;
            showPopup = false;
          }

          // Show the ID in tooltip
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("ID: %s", id.c_str());
          }
        }
      }
    }

    // Show auto-detected channels
    if (hasAutoChannels) {
      if (hasConfigChannels) {
        ImGui::Separator();
      }
      ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.0f, 1.0f), "Auto-detected Channels:");
      for (const auto& [id, displayName] : availableChannels) {
        if (displayName.find("(Auto)") != std::string::npos) {
          bool isSelected = (id == currentDataName);

          if (ImGui::Selectable((displayName + "##" + id).c_str(), isSelected)) {
            selectedDataName = id;
            showPopup = false;
          }

          // Show the ID in tooltip
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("ID: %s", id.c_str());
          }
        }
      }
    }

    // Add option to reload config and refresh global store
    ImGui::Separator();
    if (ImGui::Selectable("Refresh All Channels")) {
      channelsLoaded = false; // Force reload on next call
      loadChannelsFromConfig();
    }

    ImGui::EndPopup();
  }

  // Display name on left
  // Push custom color for display name
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.7f, 1.0f, 1.0f)); // Blue color
  // Display name on left
  ImGui::SetWindowFontScale(2.0f); // Make it larger
  ImGui::Text("%s", displayName.c_str());
  // Pop color
  ImGui::PopStyleColor();

  // Display unit on right (if any)
  if (!unitDisplay.empty()) {
    float windowWidth = ImGui::GetWindowSize().x;
    ImGui::SameLine(windowWidth - ImGui::CalcTextSize(unitDisplay.c_str()).x - 20);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.7f, 1.0f, 1.0f)); // Blue color
    ImGui::SetWindowFontScale(2.0f); // Make it larger
    ImGui::Text("%s", unitDisplay.c_str());
    ImGui::PopStyleColor();
  }

  // Add separator line
  ImGui::Separator();

  // Format value with only 2 decimal places
  char valueStr[32];
  snprintf(valueStr, sizeof(valueStr), "%.2f", scaledValue);

  // Set a monospaced font and large size for the digital display look
  ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Assume monospace font
  ImGui::SetWindowFontScale(7.0f); // Make it larger

  // Calculate width for right alignment of the value
  float windowWidth = ImGui::GetWindowSize().x;
  float valueWidth = ImGui::CalcTextSize(valueStr).x;
  float signWidth = ImGui::CalcTextSize("-").x;

  // Display negative sign separately if needed
  if (isNegative) {
    // Position for the negative sign
    ImGui::SetCursorPosX((windowWidth - valueWidth - signWidth) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f)); // Red for negative
    ImGui::Text("-");
    ImGui::PopStyleColor();

    // Position for the value (aligned right after the sign)
    ImGui::SameLine(0, 0);
  }
  else {
    // Position for positive value (centered, with space for a potential sign)
    ImGui::SetCursorPosX((windowWidth - valueWidth) * 0.5f + (signWidth * 0.5f));
  }

  // Draw the digital-style text in white
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); // Bright white text
  ImGui::Text("%s", valueStr);
  ImGui::PopStyleColor();

  // Reset font settings
  ImGui::SetWindowFontScale(1.0f);
  ImGui::PopFont();

  ImGui::End();

  // Pop styles
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor();
}

void RenderDigitalDisplay(const std::string& dataName) {
	// Get the value from the global data store
	float value = GlobalDataStore::GetInstance()->GetValue(dataName);

	// Name and unit mapping
	std::string displayName = dataName;
	std::string unit = "";

	std::map<std::string, std::pair<std::string, std::string>> displayMap = {
			{"GPIB-Current", {"Current", "A"}},
			{"gantry", {"gantry", ""}},
			// Add more mappings as needed
	};

	// Look up display name and unit if available
	auto it = displayMap.find(dataName);
	if (it != displayMap.end()) {
		displayName = it->second.first;
		unit = it->second.second;
	}

	// Create window name
	std::string windowName = "Digital_" + dataName;

	// Set window appearance
	ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(280, 90), ImGuiCond_FirstUseEver);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.15f, 0.2f, 0.95f)); // Dark blue/gray
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f); // Sharp corners
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f); // Thin border

	// Create window
	ImGui::Begin(windowName.c_str(), nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoCollapse);

	// Display name on left
	ImGui::Text("%s", displayName.c_str());

	// Display unit on right
	float windowWidth = ImGui::GetWindowSize().x;
	ImGui::SameLine(windowWidth - ImGui::CalcTextSize(unit.c_str()).x - 20);
	ImGui::Text("%s", unit.c_str());

	// Add separator line
	ImGui::Separator();

	// Format value string (for display like "0.000000")
	char valueStr[32];
	snprintf(valueStr, sizeof(valueStr), "%.6f", value);

	// Calculate position for centered text - the digital display should be centered
	float textWidth = ImGui::CalcTextSize(valueStr).x * 2.5f; // Accounting for larger font
	ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);

	// Set a monospaced font and large size for the digital display look
	ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Assume monospace font
	ImGui::SetWindowFontScale(2.5f);

	// Draw the digital-style text
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); // Bright white text
	ImGui::Text("%s", valueStr);
	ImGui::PopStyleColor();

	// Reset font settings
	ImGui::SetWindowFontScale(1.0f);
	ImGui::PopFont();

	ImGui::End();

	// Pop styles
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();
}
// Example of creating and using the initialization process
void RunInitializationProcess(MachineOperations& machineOps) {
	// Method 1: Using the dedicated InitializationStep class
	std::cout << "Running initialization using dedicated class..." << std::endl;

	auto initStep = std::make_unique<InitializationStep>(machineOps);

	// Set completion callback
	initStep->SetCompletionCallback([](bool success) {
		std::cout << "Initialization " << (success ? "succeeded" : "failed") << std::endl;
	});

	// Execute the step
	initStep->Execute();

	// Method 2: Using the more flexible SequenceStep class
	std::cout << "\nRunning initialization using sequence step..." << std::endl;

	auto sequenceStep = std::make_unique<SequenceStep>("Initialization", machineOps);

	// Add operations in sequence
	sequenceStep->AddOperation(std::make_shared<MoveToNodeOperation>(
		"gantry-main", "Process_Flow", "node_4027"));

	sequenceStep->AddOperation(std::make_shared<MoveToNodeOperation>(
		"hex-left", "Process_Flow", "node_5480"));

	sequenceStep->AddOperation(std::make_shared<MoveToNodeOperation>(
		"hex-right", "Process_Flow", "node_5136"));

	sequenceStep->AddOperation(std::make_shared<SetOutputOperation>(
		"IOBottom", 0, false)); // L_Gripper OFF

	sequenceStep->AddOperation(std::make_shared<SetOutputOperation>(
		"IOBottom", 2, false)); // R_Gripper OFF

	sequenceStep->AddOperation(std::make_shared<SetOutputOperation>(
		"IOBottom", 10, true)); // Vacuum_Base ON

	// Set completion callback
	sequenceStep->SetCompletionCallback([](bool success) {
		std::cout << "Sequence " << (success ? "succeeded" : "failed") << std::endl;
	});

	// Execute the step
	sequenceStep->Execute();
}

void RenderMinimizeExitButtons(SDL_Window* window, bool& done) {
	// Set window position to top-right corner
	ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 210, 0), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(200, 60), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.8f); // Slightly more opaque background

	// Create window with specific flags to keep it on top and prevent user from moving it
	ImGuiWindowFlags window_flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoBringToFrontOnFocus; // Keeps it from being pushed back in z-order

	// Start the window
	ImGui::Begin("Controls", nullptr, window_flags);

	// Add minimize button
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f)); // Blue button
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.3f, 0.7f, 1.0f));

	if (ImGui::Button("Minimize", ImVec2(80, 40))) {
		// Call SDL function to minimize the window
		SDL_MinimizeWindow(window);
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();

	// Style the exit button to be more noticeable
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); // Red button
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));

	// Make the button fill most of the window
	if (ImGui::Button("Exit", ImVec2(80, 40))) {
		// Set the done flag to true to exit the application
		done = true;
	}
	ImGui::PopStyleColor(3); // Remove the 3 style colors we pushed

	ImGui::End();
}

void RenderFPSoverlay(float m_fps)
{
	// Create a performance overlay
	ImGui::SetNextWindowPos(ImVec2(310, 0), ImGuiCond_FirstUseEver); // Only set position on first use
	ImGui::SetNextWindowBgAlpha(0.25f); // Transparent background
	ImGui::Begin("Performance", nullptr,
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse
	);

	// Push larger font size
	ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Use default font
	ImGui::SetWindowFontScale(5.0f); // Scale font to 2x size

	ImGui::Text("FPS: %.1f", m_fps);

	// Pop font changes
	ImGui::SetWindowFontScale(1.0f);
	ImGui::PopFont();

	ImGui::End();
}


void CheckImGuiVersion() {
	std::cout << "ImGui Version: " << IMGUI_VERSION << std::endl;
}


void DebugImGuiIDs() {
  // Enable ImGui debug logging
  static bool enableDebug = true;
  if (enableDebug) {
    // This will print ID stack information to help find conflicts
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
      printf("=== ImGui ID Stack Debug ===\n");
      // Toggle debug mode
      enableDebug = !enableDebug;
    }
  }
}

void DisableImGuiIDConflictWarnings() {
  // This will stop the annoying error popup
  ImGui::GetIO().ConfigDebugHighlightIdConflicts = false;
}
#pragma endregion


int main(int argc, char* argv[])
{
  // Initialize the module configuration system
  ModuleConfig moduleConfig("module_config.ini");
  moduleConfig.printConfig(); // Print current configuration to console

  // Get the logger instance
  Logger* logger = Logger::GetInstance();
  logger->Log("Application started with module configuration");
  logger->ToggleMinimize();
  // Setup SDL (always enabled)
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
  {
    printf("Error: %s\n", SDL_GetError());
    return -1;
  }

  // Setup SDL window (always enabled)
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
    SDL_WINDOW_ALLOW_HIGHDPI);

  SDL_Window* window = SDL_CreateWindow(Version::getWindowTitle().c_str(),
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    1280, 720,
    window_flags);
  if (window == nullptr)
  {
    printf("Error creating window: %s\n", SDL_GetError());
    SDL_Quit();
    return -1;
  }

  // Set application icon
  SDL_Surface* iconSurface = SDL_LoadBMP("resources/icon.bmp");
  if (iconSurface) {
    SDL_SetWindowIcon(window, iconSurface);
    SDL_FreeSurface(iconSurface);
  }

  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  SDL_GL_MakeCurrent(window, gl_context);
  SDL_GL_SetSwapInterval(1);

  // Setup Dear ImGui context (always enabled)
  IMGUI_CHECKVERSION();
  CheckImGuiVersion();
  ImGui::CreateContext();
	DisableImGuiIDConflictWarnings();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  ImGui::StyleColorsLight();
  ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL3_Init("#version 130");

  ImGuiStyle& style = ImGui::GetStyle();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
  {
    SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
    SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
  }

  ImPlot::CreateContext();

  // Initialize Python Process Manager (conditionally)
  std::unique_ptr<PythonProcessManager> pythonManager;
  if (moduleConfig.isEnabled("PYTHON_PROCESS_MANAGER")) {
    pythonManager = std::make_unique<PythonProcessManager>();

    if (!pythonManager->StartCLD101xServer()) {
      logger->LogWarning("Failed to start CLD101x server script, will continue without it");
    }
    else {
      logger->LogInfo("CLD101x server script started successfully");
    }
  }
  else {
    logger->LogInfo("Python Process Manager disabled in configuration");
  }

  // FPS calculation variables (always enabled)
  float frameTime = 0.0f;
  float fps = 0.0f;
  float fpsUpdateInterval = 0.5f;
  float fpsTimer = 0.0f;
  int frameCounter = 0;
  Uint64 lastFrameTime = SDL_GetPerformanceCounter();

  // Motion Configuration (always needed for other modules)
  std::string configPath = "motion_config.json";
  MotionConfigManager configManager(configPath);

  // Motion Configuration Editor (conditional)
  std::unique_ptr<MotionConfigEditor> configEditor;
  if (moduleConfig.isEnabled("CONFIG_EDITOR")) {
    configEditor = std::make_unique<MotionConfigEditor>(configManager);
    logger->LogInfo("MotionConfigEditor initialized");
  }

  // Graph Visualizer (conditional)
  std::unique_ptr<GraphVisualizer> graphVisualizer;
  if (moduleConfig.isEnabled("GRAPH_VISUALIZER")) {
    graphVisualizer = std::make_unique<GraphVisualizer>(configManager);
    logger->LogInfo("GraphVisualizer initialized");
  }

  // PI Controller Manager (conditional)
  std::unique_ptr<PIControllerManager> piControllerManager;
  if (moduleConfig.isEnabled("PI_CONTROLLERS")) {
    piControllerManager = std::make_unique<PIControllerManager>(configManager);
    if (piControllerManager->ConnectAll()) {
      logger->LogInfo("Successfully connected to all enabled PI controllers");
    }
    else {
      logger->LogWarning("Failed to connect to some PI controllers");
    }
  }

  // ACS Controller Manager (conditional)
  std::unique_ptr<ACSControllerManager> acsControllerManager;
  if (moduleConfig.isEnabled("ACS_CONTROLLERS")) {
    acsControllerManager = std::make_unique<ACSControllerManager>(configManager);
    if (acsControllerManager->ConnectAll()) {
      logger->LogInfo("Successfully connected to all enabled ACS controllers");
    }
    else {
      logger->LogWarning("Failed to connect to some ACS controllers");
    }
  }

  // Motion Control Layer (conditional)
  std::unique_ptr<MotionControlLayer> motionControlLayer;
  if (moduleConfig.isEnabled("MOTION_CONTROL_LAYER") && piControllerManager && acsControllerManager) {
    motionControlLayer = std::make_unique<MotionControlLayer>(
      configManager, *piControllerManager, *acsControllerManager);

    motionControlLayer->SetPathCompletionCallback([&logger](bool success) {
      if (success) {
        logger->LogInfo("Path execution completed successfully");
      }
      else {
        logger->LogWarning("Path execution failed or was cancelled");
      }
    });
    logger->LogInfo("MotionControlLayer initialized");
  }

  // Global Data Store (conditional)
  GlobalDataStore* dataStore = nullptr;
  if (moduleConfig.isEnabled("GLOBAL_DATA_STORE")) {
    dataStore = GlobalDataStore::GetInstance();
    logger->LogInfo("GlobalDataStore initialized");
  }

  // Scanning UIs (conditional)
  std::unique_ptr<ScanningUI> hexapodScanningUI;
  std::unique_ptr<OptimizedScanningUI> optimizedScanningUI;
  if (moduleConfig.isEnabled("SCANNING_UI_V1") && piControllerManager && dataStore) {
    hexapodScanningUI = std::make_unique<ScanningUI>(*piControllerManager, *dataStore);
    logger->LogInfo("Hexapod Scanning UI initialized");
  }
  if (moduleConfig.isEnabled("OPTIMIZED_SCANNING_UI") && piControllerManager && dataStore) {
    optimizedScanningUI = std::make_unique<OptimizedScanningUI>(*piControllerManager, *dataStore);
    logger->LogInfo("OptimizedScanningUI initialized");
  }

  // Pylon Camera (conditional)
  std::unique_ptr<PylonCameraTest> pylonCameraTest;
  if (moduleConfig.isEnabled("PYLON_CAMERA")) {
    pylonCameraTest = std::make_unique<PylonCameraTest>();
    logger->LogInfo("PylonCameraTest initialized");
  }

  // EziIO Manager (conditional)
  std::unique_ptr<EziIOManager> ioManager;
  std::unique_ptr<IOConfigManager> ioconfigManager;
  std::unique_ptr<IOControlPanel> ioControlPanel;
  if (moduleConfig.isEnabled("EZIIO_MANAGER")) {
    ioManager = std::make_unique<EziIOManager>();
    if (!ioManager->initialize()) {
      logger->LogError("Failed to initialize EziIO manager");
    }
    else {
      ioconfigManager = std::make_unique<IOConfigManager>();
      if (!ioconfigManager->loadConfig("IOConfig.json")) {
        logger->LogWarning("Failed to load IO configuration, using default settings");
      }
      ioconfigManager->initializeIOManager(*ioManager);

      if (moduleConfig.isEnabled("IO_CONTROL_PANEL")) {
        ioControlPanel = std::make_unique<IOControlPanel>(*ioManager);
        logger->LogInfo("IOControlPanel initialized for quick output control");
      }

      if (!ioManager->connectAll()) {
        logger->LogWarning("Failed to connect to all IO devices");
      }
      ioManager->startPolling(100);
      logger->LogInfo("EziIO system initialized");
    }
  }

  // Pneumatic System (conditional)
  std::unique_ptr<PneumaticManager> pneumaticManager;
  std::unique_ptr<PneumaticUI> pneumaticUI;
  if (moduleConfig.isEnabled("PNEUMATIC_SYSTEM") && ioManager && ioconfigManager) {
    pneumaticManager = std::make_unique<PneumaticManager>(*ioManager);
    if (!ioconfigManager->initializePneumaticManager(*pneumaticManager)) {
      logger->LogWarning("Failed to initialize pneumatic manager");
    }
    else {
      pneumaticManager->initialize();
      pneumaticManager->startPolling(50);
      pneumaticUI = std::make_unique<PneumaticUI>(*pneumaticManager);

      pneumaticManager->setStateChangeCallback([&logger](const std::string& slideName, SlideState state) {
        std::string stateStr;
        switch (state) {
        case SlideState::EXTENDED: stateStr = "Extended (Down)"; break;
        case SlideState::RETRACTED: stateStr = "Retracted (Up)"; break;
        case SlideState::MOVING: stateStr = "Moving"; break;
        case SlideState::P_ERROR: stateStr = "ERROR"; break;
        default: stateStr = "Unknown";
        }
        logger->LogInfo("Pneumatic slide '" + slideName + "' changed state to: " + stateStr);
      });
      logger->LogInfo("Pneumatic control system initialized");
    }
  }

  // Data Systems (conditional)
  std::unique_ptr<DataClientManager> dataClientManager;
  std::unique_ptr<DataChartManager> dataChartManager;
  if (moduleConfig.isEnabled("DATA_CLIENT_MANAGER")) {
    dataClientManager = std::make_unique<DataClientManager>("DataServerConfig.json");
    dataClientManager->ConnectAutoClients();
    dataClientManager->ToggleWindow();
    logger->LogInfo("DataClientManager initialized");
  }
  if (moduleConfig.isEnabled("DATA_CHART_MANAGER")) {
    dataChartManager = std::make_unique<DataChartManager>("data_display_config.json");
    dataChartManager->ToggleWindow();
    logger->LogInfo("DataChartManager initialized");
  }

  // Product Config Manager (conditional)
  std::unique_ptr<ProductConfigManager> productConfigManager;
  if (moduleConfig.isEnabled("PRODUCT_CONFIG_MANAGER")) {
    productConfigManager = std::make_unique<ProductConfigManager>(configManager);
    productConfigManager->ToggleWindow();
    logger->LogInfo("ProductConfigManager initialized");
  }

  // CLD101x System (conditional)
  std::unique_ptr<CLD101xManager> cld101xManager;
  std::unique_ptr<CLD101xOperations> laserOps;
  if (moduleConfig.isEnabled("CLD101X_MANAGER")) {
    cld101xManager = std::make_unique<CLD101xManager>();
    cld101xManager->Initialize();
    laserOps = std::make_unique<CLD101xOperations>(*cld101xManager);
    logger->LogInfo("CLD101x system initialized");
  }

  // Keithley 2400 Manager (conditional)
  std::unique_ptr<Keithley2400Manager> keithleyManager;
  std::unique_ptr<Keithley2400Operations> smuOps;
  if (moduleConfig.isEnabled("KEITHLEY_2400_MANAGER")) {
    keithleyManager = std::make_unique<Keithley2400Manager>();

    // Initialize from config file
    if (keithleyManager->Initialize("smu_config.json")) {
      logger->LogInfo("Keithley2400Manager initialized from config file");

      smuOps = std::make_unique<Keithley2400Operations>(*keithleyManager);

      // Try to connect based on config
      if (keithleyManager->ConnectAll()) {
        logger->LogInfo("Successfully connected to Keithley 2400 servers");
        //keithleyManager->StartAllPolling(1000); // Poll every 1 second
      }
      else {
        logger->LogWarning("Failed to connect to some Keithley 2400 servers");
      }
    }
    else {
      logger->LogWarning("Failed to load Keithley config, using defaults");
      // Fallback to manual setup if config fails
      keithleyManager->AddClient("Keithley-Main", "127.0.0.101", 8888);
      if (keithleyManager->ConnectAll()) {
        logger->LogInfo("Successfully connected to Keithley 2400 servers (fallback)");
        keithleyManager->StartAllPolling(1000);
      }
    }
  }


  // Global Jog Panel (conditional)
  std::unique_ptr<GlobalJogPanel> globalJogPanel;
  if (moduleConfig.isEnabled("GLOBAL_JOG_PANEL") && piControllerManager && acsControllerManager) {
    globalJogPanel = std::make_unique<GlobalJogPanel>(
      configManager, *piControllerManager, *acsControllerManager);
    logger->LogInfo("GlobalJogPanel initialized");
  }

  // Machine Operations - Real or Virtual based on configuration
  std::unique_ptr<MachineOperations> machineOps;
  std::unique_ptr<VirtualMachineOperationsAdapter> virtualOps;

  bool useRealHardware = moduleConfig.isEnabled("REAL_HARDWARE");

  if (useRealHardware && motionControlLayer && piControllerManager &&
    (ioManager || !moduleConfig.isEnabled("EZIIO_MANAGER")) &&
    (pneumaticManager || !moduleConfig.isEnabled("PNEUMATIC_SYSTEM"))) {

    // Use real machine operations
    machineOps = std::make_unique<MachineOperations>(
      *motionControlLayer, 
      *piControllerManager,
      ioManager ? *ioManager : *(EziIOManager*)nullptr,
      pneumaticManager ? *pneumaticManager : *(PneumaticManager*)nullptr,
      laserOps.get(), 
      pylonCameraTest.get(),
      smuOps.get()  // Pass SMU operations
    );
    logger->LogInfo("Real MachineOperations initialized");
  }
  else {
    // Use virtual machine operations
    virtualOps = std::make_unique<VirtualMachineOperationsAdapter>();
    logger->LogInfo("Virtual MachineOperations initialized");
  }

  // Process Control Panel (conditional)
  std::unique_ptr<ProcessControlPanel> processControlPanel;
  if (moduleConfig.isEnabled("PROCESS_CONTROL_PANEL") && machineOps) {
    processControlPanel = std::make_unique<ProcessControlPanel>(*machineOps);
    logger->LogInfo("ProcessControlPanel initialized");
  }

  // Initialization Window (conditional)
  std::unique_ptr<InitializationWindow> initWindow;
  if (moduleConfig.isEnabled("INITIALIZATION_WINDOW") && machineOps) {
    initWindow = std::make_unique<InitializationWindow>(*machineOps);
    logger->LogInfo("InitializationWindow initialized");
  }

  // Script System (conditional)
  std::unique_ptr<ScriptPrintViewer> scriptPrintViewer;
  std::unique_ptr<ScriptEditorUI> scriptEditor;
  std::unique_ptr<ScriptRunner> scriptRunner;
  if (moduleConfig.isEnabled("SCRIPT_PRINT_VIEWER")) {
    scriptPrintViewer = std::make_unique<ScriptPrintViewer>();
    logger->LogInfo("ScriptPrintViewer initialized");
  }
  if (moduleConfig.isEnabled("SCRIPT_EDITOR") && machineOps && scriptPrintViewer) {
    scriptEditor = std::make_unique<ScriptEditorUI>(*machineOps, scriptPrintViewer.get());
    logger->LogInfo("ScriptEditorUI initialized");
  }
  if (moduleConfig.isEnabled("SCRIPT_RUNNER") && machineOps && scriptPrintViewer) {
    scriptRunner = std::make_unique<ScriptRunner>(*machineOps, scriptPrintViewer.get());
    scriptRunner->ToggleWindow();
    logger->LogInfo("ScriptRunner initialized");
  }

  // Motion Graphic (conditional)
  std::unique_ptr<MotionGraphic> motionGraphic;
  if (moduleConfig.isEnabled("MOTION_GRAPHIC") && motionControlLayer && machineOps) {
    motionGraphic = std::make_unique<MotionGraphic>(configManager, *motionControlLayer, *machineOps);
    logger->LogInfo("MotionGraphic initialized");
  }

  // Camera Exposure Test UI (conditional)
  std::unique_ptr<CameraExposureTestUI> cameraExposureTestUI;
  if (moduleConfig.isEnabled("CAMERA_EXPOSURE_TEST") && machineOps) {
    cameraExposureTestUI = std::make_unique<CameraExposureTestUI>(*machineOps);
    logger->LogInfo("CameraExposureTestUI initialized");
  }




  // ADD MACHINE BLOCK UI INITIALIZATION
  // ADD MACHINE BLOCK UI INITIALIZATION
  std::unique_ptr<MachineBlockUI> machineBlockUI;
  if (moduleConfig.isEnabled("MACHINE_BLOCK_UI")) {
    machineBlockUI = std::make_unique<MachineBlockUI>();

    if (machineOps) {
      machineBlockUI->SetMachineOperations(machineOps.get());
      logger->LogInfo("MachineBlockUI initialized with REAL machine operations");
    }
    else if (virtualOps) {
      // For now, just log that virtual ops are ready
      // You can extend MachineBlockUI to accept VirtualMachineOperationsAdapter
      logger->LogInfo("MachineBlockUI initialized - Virtual operations available");
      machineBlockUI->SetVirtualMachineOperations(virtualOps.get());
      logger->LogInfo("Note: Extend MachineBlockUI to use VirtualMachineOperationsAdapter for real execution");
    }
    else {
      logger->LogInfo("MachineBlockUI initialized in debug mode only");
    }
  }


  // ADD MACRO MANAGER INITIALIZATION (add this after MachineBlockUI section)
  std::unique_ptr<MacroManager> macroManager;
  if (moduleConfig.isEnabled("MACRO_MANAGER") && machineBlockUI) {
    macroManager = std::make_unique<MacroManager>();

    // Connect the macro manager to the block UI
    macroManager->SetMachineBlockUI(machineBlockUI.get());

    // Add your saved programs to the macro manager
    macroManager->AddProgram("Test Sequence", "Test sequence.json");
    macroManager->AddProgram("User Prompt Test", "test user prompt.json");
    macroManager->AddProgram("Slide Test", "test_slides.json");

    logger->LogInfo("MacroManager initialized with saved programs");
  }




  // Vertical Toolbar (conditional)
  std::unique_ptr<VerticalToolbarMenu> toolbarVertical;
  if (moduleConfig.isEnabled("VERTICAL_TOOLBAR")) {
    toolbarVertical = std::make_unique<VerticalToolbarMenu>();
    toolbarVertical->SetWidth(200);

    // Initialize with state tracking (no longer auto-adds missing items)
    toolbarVertical->InitializeStateTracking("toolbar_state.json");

    // Create categories
    auto motorsCategory = toolbarVertical->CreateCategory("Motors");
    auto manualCategory = toolbarVertical->CreateCategory("Manual");
    auto dataCategory = toolbarVertical->CreateCategory("Data");
    auto productsCategory = toolbarVertical->CreateCategory("Products");
    auto miscCategory = toolbarVertical->CreateCategory("General");

    // Add components conditionally based on what's enabled
    if (processControlPanel) {
      toolbarVertical->AddReference(CreateHierarchicalUI(*processControlPanel, "Process Control"));
    }
    if (hexapodScanningUI) {
      toolbarVertical->AddReference(CreateHierarchicalUI(*hexapodScanningUI, "Scanning V1"));
    }
    if (optimizedScanningUI) {
      toolbarVertical->AddReference(CreateHierarchicalUI(*optimizedScanningUI, "Scanning V2 (test)"));
    }
    if (globalJogPanel) {
      toolbarVertical->AddReference(CreateHierarchicalUI(*globalJogPanel, "Global Jog Panel"));
    }
    if (pylonCameraTest) {
      toolbarVertical->AddReference(CreatePylonCameraAdapter(*pylonCameraTest, "Top Camera"));
    }

    // Add components to Motors category
    if (piControllerManager) {
      toolbarVertical->AddReferenceToCategory("Motors",
        CreateHierarchicalPIControllerAdapter(*piControllerManager, "PI"));
    }
    if (acsControllerManager) {
      toolbarVertical->AddReferenceToCategory("Motors",
        CreateHierarchicalACSControllerAdapter(*acsControllerManager, "Gantry"));
    }
    if (motionControlLayer) {
      toolbarVertical->AddReferenceToCategory("Motors",
        CreateHierarchicalMotionControlAdapter(*motionControlLayer, "Motion Control"));
    }




    if (pneumaticUI) {
      toolbarVertical->AddReferenceToCategory("Manual",
        CreateHierarchicalUI(*pneumaticUI, "Pneumatic"));
    }
    if (ioControlPanel) {
      toolbarVertical->AddReferenceToCategory("Manual",
        CreateHierarchicalUI(*ioControlPanel, "IO Quick Control"));
    }

    // Add components to Data category
    if (dataChartManager) {
      toolbarVertical->AddReferenceToCategory("Data",
        CreateHierarchicalUI(*dataChartManager, "Data Chart"));
    }
    if (dataClientManager) {
      toolbarVertical->AddReferenceToCategory("Data",
        CreateHierarchicalUI(*dataClientManager, "Data TCP/IP"));
    }
    if (keithleyManager) {
      toolbarVertical->AddReferenceToCategory("Data",
        CreateHierarchicalUI(*keithleyManager, "Keithley 2400"));
    }

    // Add components to Products category
    if (productConfigManager) {
      toolbarVertical->AddReferenceToCategory("Products",
        CreateHierarchicalUI(*productConfigManager, "Products Config"));
    }
    if (configEditor) {
      toolbarVertical->AddReferenceToCategory("Products",
        CreateHierarchicalUI(*configEditor, "Config Editor"));
    }
    if (graphVisualizer) {
      toolbarVertical->AddReferenceToCategory("Products",
        CreateHierarchicalUI(*graphVisualizer, "Graph Visualizer"));
    }
    if (scriptEditor) {
      toolbarVertical->AddReferenceToCategory("Products",
        CreateHierarchicalUI(*scriptEditor, "Script Editor"));
    }
    if (scriptRunner) {
      toolbarVertical->AddReferenceToCategory("Products",
        CreateScriptRunnerAdapter(*scriptRunner, "Script Runner"));
    }
    if (scriptPrintViewer) {
      toolbarVertical->AddReferenceToCategory("Products",
        CreateHierarchicalUI(*scriptPrintViewer, "Script Output"));
    }
    if (motionGraphic) {
      toolbarVertical->AddReferenceToCategory("Products",
        CreateHierarchicalUI(*motionGraphic, "Motion Graphic"));
    }
    if (machineBlockUI) {
      toolbarVertical->AddReferenceToCategory("Products",
        CreateHierarchicalUI(*machineBlockUI, "Block Programming"));
    }
    if (cameraExposureTestUI) {
      toolbarVertical->AddReferenceToCategory("Products",
        CreateCameraExposureTestUIAdapter(*cameraExposureTestUI, "Camera Testing"));
    }

    // Add components to General category
    if (cld101xManager) {
      toolbarVertical->AddReferenceToCategory("General",
        CreateHierarchicalUI(*cld101xManager, "Laser TEC Cntrl"));
    }
    if (machineOps && machineOps->GetCameraExposureManager()) {
      toolbarVertical->AddReferenceToCategory("Products",
        CreateHierarchicalUI(*machineOps->GetCameraExposureManager(), "Camera Exposure"));
    }
    if (macroManager) {
      toolbarVertical->AddReferenceToCategory("Products",
        CreateHierarchicalUI(*macroManager, "Macro Programming"));
    }

    // NOTE: No longer automatically adding missing items from toolbar_state.json
    // Only components that are actually initialized and enabled will appear in the toolbar

    logger->LogInfo("VerticalToolbarMenu initialized with " +
      std::to_string(toolbarVertical->GetComponentCount()) + " total components (" +
      std::to_string(toolbarVertical->GetVisibleWindowCount()) + " visible)");
  }





  // Create persistent EziIO UI for toolbar
  std::unique_ptr<EziIO_UI> toolbarIoUI;
  if (ioManager) {
    toolbarIoUI = std::make_unique<EziIO_UI>(*ioManager);
    if (ioconfigManager) {
      toolbarIoUI->setConfigManager(ioconfigManager.get());
    }
    toolbarVertical->AddReferenceToCategory("Manual",
      CreateHierarchicalUI(*toolbarIoUI, "IO Control"));
  }





  // Create one display for current monitoring
  auto currentDisplay = std::make_unique<DigitalDisplayWithChart>("GPIB-Current");


  std::unique_ptr<MenuManager> menuManager;
  if (logger && machineOps) {
    menuManager = std::make_unique<MenuManager>(logger, machineOps.get());

    // Set toolbar reference if available
    if (toolbarVertical) {
      menuManager->SetVerticalToolbar(toolbarVertical.get());
    }

    // Set UI component references in MachineOperations (one-time setup)
    if (configEditor) {
      machineOps->SetMotionConfigEditor(configEditor.get());
    }

    if (graphVisualizer) {
      machineOps->SetGraphVisualizer(graphVisualizer.get());
    }

    logger->LogInfo("MenuManager initialized with unified MachineOperations interface");
  }






  // Main loop
  bool done = false;
  bool cameraDisconnectedWarningShown = false;

  while (!done)
  {
    // Poll and handle events
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT)
        done = true;
      if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
        event.window.windowID == SDL_GetWindowID(window))
        done = true;

      // Process key events for global jog panel
      if (globalJogPanel && (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)) {
        globalJogPanel->ProcessKeyInput(
          event.key.keysym.sym,
          event.type == SDL_KEYDOWN
        );
      }
    }


    // ADD DEBUG FUNCTION HERE - at the start of each frame
    DebugImGuiIDs();

    // Calculate delta time and FPS
    Uint64 currentFrameTime = SDL_GetPerformanceCounter();
    float deltaTime = (currentFrameTime - lastFrameTime) / (float)SDL_GetPerformanceFrequency();
    lastFrameTime = currentFrameTime;

    frameCounter++;
    fpsTimer += deltaTime;

    if (fpsTimer >= fpsUpdateInterval) {
      fps = frameCounter / fpsTimer;
      frameCounter = 0;
      fpsTimer = 0;
    }

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // Create dockspace
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID);

    // Render module configuration UI
    moduleConfig.renderConfigUI();

    // Render overlays (conditional)
    if (moduleConfig.isEnabled("FPS_OVERLAY")) {
      RenderFPSoverlay(fps);
    }
    if (moduleConfig.isEnabled("MINIMIZE_EXIT_BUTTONS")) {
      RenderMinimizeExitButtons(window, done);
    }
    if (moduleConfig.isEnabled("CLOCK_OVERLAY")) {
      RenderClockOverlay(3);
    }
    if (moduleConfig.isEnabled("DIGITAL_DISPLAY") && dataStore) {
      RenderDigitalDisplaySI("GPIB-Current");
      //RenderDigitalDisplaySI("SMU1-Voltage");
    }

    // Render logger UI (always enabled)
    logger->RenderUI();

    currentDisplay->Render();

    // Render component UIs conditionally
    if (toolbarVertical) {
      toolbarVertical->RenderUI();
    }

    if (configEditor) {
      configEditor->RenderUI();
    }
    if (graphVisualizer) {
      graphVisualizer->RenderUI();
    }

    // Render PI controllers
    if (piControllerManager) {
      piControllerManager->RenderUI();
      for (const auto& [name, device] : configManager.GetAllDevices()) {
        if (device.Port == 50000 && device.IsEnabled) {
          PIController* controller = piControllerManager->GetController(name);
          if (controller && controller->IsConnected()) {
            controller->RenderUI();
          }
        }
      }
    }

    // Render ACS controllers
    if (acsControllerManager) {
      acsControllerManager->RenderUI();
      for (const auto& [name, device] : configManager.GetAllDevices()) {
        if (device.Port != 50000 && device.IsEnabled) {
          ACSController* controller = acsControllerManager->GetController(name);
          if (controller && controller->IsConnected()) {
            controller->RenderUI();
          }
        }
      }
    }

    // Render motion control layer
    if (motionControlLayer && motionControlLayer->IsVisible()) {
      motionControlLayer->RenderUI();
    }

    // Render camera
    if (pylonCameraTest && machineOps) {
      pylonCameraTest->RenderUIWithMachineOps(machineOps.get());
    }

    //// Render IO systems
    //if (ioManager) {
    //  // Create and render EziIO_UI if not already in toolbar
    //  static std::unique_ptr<EziIO_UI> mainIoUI;
    //  if (!mainIoUI) {
    //    mainIoUI = std::make_unique<EziIO_UI>(*ioManager);
    //    if (ioconfigManager) {
    //      mainIoUI->setConfigManager(ioconfigManager.get());
    //    }
    //    // REMOVE THIS SECTION - let the toolbar control visibility
    //    // if (mainIoUI->IsVisible()) {
    //    //   mainIoUI->ToggleWindow(); // Hide by default
    //    // }
    //  }
    //  mainIoUI->RenderUI();
    //}

    // Add this instead:
    if (toolbarIoUI) {
      toolbarIoUI->RenderUI();
    }

    if (pneumaticUI) {
      pneumaticUI->RenderUI();
    }
    if (ioControlPanel) {
      ioControlPanel->RenderUI();
    }

    // Update and render data systems
    if (dataClientManager) {
      dataClientManager->UpdateClients();
      dataClientManager->RenderUI();
    }
    if (dataChartManager) {
      dataChartManager->Update();
      dataChartManager->RenderUI();
    }
    // Render Keithley system
    if (keithleyManager) {
      keithleyManager->RenderUI();

      // Render individual client UIs
      for (const auto& clientName : keithleyManager->GetClientNames()) {
        Keithley2400Client* client = keithleyManager->GetClient(clientName);
        if (client && client->IsVisible()) {
          client->RenderUI();
        }
      }
    }

    // Render other components
    if (productConfigManager) {
      productConfigManager->RenderUI();
    }
    if (cld101xManager) {
      cld101xManager->RenderUI();
    }
    if (globalJogPanel) {
      globalJogPanel->RenderUI();
    }
    if (hexapodScanningUI) {
      hexapodScanningUI->RenderUI();
    }
    if (optimizedScanningUI) {
      optimizedScanningUI->RenderUI();
    }

    // Render process windows
    if (initWindow) {
      initWindow->RenderUI();
    }
    if (processControlPanel) {
      processControlPanel->RenderUI();
    }

    // Render script system
    if (scriptEditor) {
      scriptEditor->RenderUI();
    }
    if (scriptRunner) {
      scriptRunner->RenderUI();
    }
    if (scriptPrintViewer) {
      scriptPrintViewer->RenderUI();
    }
    if (motionGraphic) {
      motionGraphic->RenderUI();
    }



    // ADD MACHINE BLOCK UI RENDERING
    if (machineBlockUI) {
      machineBlockUI->RenderUI();
      // CRITICAL: Add this line to render the prompt UI
      machineBlockUI->RenderPromptUI();
    }
    // Render camera exposure system
    if (machineOps && machineOps->GetCameraExposureManager()) {
      machineOps->GetCameraExposureManager()->RenderUI();
    }
    if (cameraExposureTestUI) {
      cameraExposureTestUI->RenderUI();
    }

    // ADD MACRO MANAGER RENDERING (add this with other RenderUI calls)
    if (macroManager) {
      macroManager->RenderUI();
    }



    // In main loop:
    if (menuManager) {
      menuManager->RenderMainMenuBar();
    }


    
    // Rendering
    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Update and Render additional Platform Windows
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
      SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
      SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
      ImGui::UpdatePlatformWindows();
      ImGui::RenderPlatformWindowsDefault();
      SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
    }

    SDL_GL_SwapWindow(window);
  }

  // Cleanup phase
  logger->Log("Application shutting down");

  // Stop Python scripts first
  if (pythonManager) {
    logger->LogInfo("Stopping Python processes...");
    pythonManager->StopAllProcesses();
    logger->LogInfo("Wait 2 sec for python process to close..");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  }

  // Cleanup machine operations
  if (machineOps) {
    logger->LogInfo("Deconstructing MachineOperations..");
    machineOps.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  // Cleanup camera
  if (pylonCameraTest) {
    pylonCameraTest->GetCamera().StopGrabbing();
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    pylonCameraTest->GetCamera().Disconnect();
  }

  // Cleanup laser system
  if (cld101xManager) {
    cld101xManager->DisconnectAll();
  }
  // Cleanup Keithley system
  if (keithleyManager) {
    logger->LogInfo("Shutting down Keithley 2400 system...");
    keithleyManager->StopAllPolling();
    keithleyManager->DisconnectAll();
  }
  // Cleanup controllers
  try {
    if (piControllerManager) {
      piControllerManager->DisconnectAll();
    }
    if (acsControllerManager) {
      acsControllerManager->DisconnectAll();
    }
  }
  catch (const std::exception& e) {
    std::cout << "Exception during controller shutdown: " << e.what() << std::endl;
  }

  // Cleanup pneumatic system
  if (pneumaticManager) {
    pneumaticManager->stopPolling();
  }

  // Cleanup IO system
  if (ioManager) {
    ioManager->stopPolling();
    ioManager->disconnectAll();
  }

  // Final ImGui cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  ImPlot::DestroyContext();

  SDL_GL_DeleteContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}

void RenderValueDisplay() {
	// Access values from the global store
	float currentValue = GlobalDataStore::GetInstance()->GetValue("GPIB-Current");

	// Create a window to display the value
	ImGui::Begin("Current Reading");

	// Format and display the value with appropriate unit
	ImGui::Text("Current: %.4f A", currentValue);

	ImGui::End();
}

// Simple ImPlot test with 10 data points
// Add this to your main loop
// Add this function where ImGui is initialized (before the main loop)
void InitializeImPlot() {
	// Create the ImPlot context
	ImPlot::CreateContext();
	Logger::GetInstance()->LogInfo("ImPlot context created successfully");
}

// Add this function where ImGui is shut down
void ShutdownImPlot() {
	// Destroy the ImPlot context
	ImPlot::DestroyContext();
	Logger::GetInstance()->LogInfo("ImPlot context destroyed");
}
void RenderSimpleChart() {
	try {
		// Create static data for the chart (only initialize once)
		static float x_data[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
		static float y_data[10] = { 0, 1, 4, 9, 16, 25, 36, 49, 64, 81 }; // y = x^2
		static bool initialized = false;

		// You could also create animated data that changes each frame
		static float animated_data[10];
		static float time = 0.0f;

		// Update the animated data each frame
		time += ImGui::GetIO().DeltaTime;
		for (int i = 0; i < 10; i++) {
			animated_data[i] = sinf(x_data[i] * 0.5f + time) * 5.0f;
		}

		// Create a window
		ImGui::Begin("Simple ImPlot Test");

		ImGui::Text("Testing ImPlot with simple arrays");

		// Create a plot inside the window
		if (ImPlot::BeginPlot("Simple Plot")) {
			// Plot the static data (x² function)
			ImPlot::PlotLine("x²", x_data, y_data, 10);

			// Plot the animated data (sine wave)
			ImPlot::PlotLine("sin(x)", x_data, animated_data, 10);

			ImPlot::EndPlot();
		}

		ImGui::End();
	}
	catch (const std::exception& e) {
		ImGui::Begin("Error");
		ImGui::Text("Exception in RenderSimpleChart: %s", e.what());
		ImGui::End();
	}
	catch (...) {
		ImGui::Begin("Error");
		ImGui::Text("Unknown exception in RenderSimpleChart");
		ImGui::End();
	}
}

// Then in your main loop, add:
// RenderSimpleChart();



