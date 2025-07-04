﻿// MotionConfigEditor.cpp
#include "include/motions/MotionConfigEditor.h"
#include "imgui.h"
#include <algorithm>
#include <iostream>
#include <cstring>

MotionConfigEditor::MotionConfigEditor(MotionConfigManager& configManager)
	: m_configManager(configManager)
	, m_logger(Logger::GetInstance())
{
	m_logger->LogInfo("MotionConfigEditor initialized");
}

void MotionConfigEditor::RenderUI() {
	if (!m_showWindow) return;

	ImGui::Begin("Motion Configuration Editor", &m_showWindow);

	// Tabs for different config sections
	if (ImGui::BeginTabBar("ConfigTabs")) {
		if (ImGui::BeginTabItem("Devices")) {
			m_showDevicesTab = true;
			m_showPositionsTab = false;
			m_showGraphsTab = false;
			m_showSettingsTab = false;
			RenderDevicesTab();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Positions")) {
			m_showDevicesTab = false;
			m_showPositionsTab = true;
			m_showGraphsTab = false;
			m_showSettingsTab = false;
			RenderPositionsTab();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Graphs")) {
			m_showDevicesTab = false;
			m_showPositionsTab = false;
			m_showGraphsTab = true;
			m_showSettingsTab = false;
			RenderGraphsTab();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Settings")) {
			m_showDevicesTab = false;
			m_showPositionsTab = false;
			m_showGraphsTab = false;
			m_showSettingsTab = true;
			RenderSettingsTab();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	// Save button at the bottom
	ImGui::Separator();
	if (ImGui::Button("Save Changes")) {
		SaveChanges();
	}

	// Debug section for clipboard content
	if (ImGui::CollapsingHeader("Debug Clipboard")) {
		if (ImGui::Button("Show Clipboard Content")) {
			std::string clipboardText = ImGui::GetClipboardText();
			m_logger->LogInfo("Current clipboard content: " + clipboardText);
		}

		ImGui::SameLine();

		if (ImGui::Button("Test Position JSON")) {
			// Set a test JSON to the clipboard (platform-dependent)
			const char* testJson = R"({
  "device": "gantry-main",
  "positions": {
    "X": 143.200000,
    "Y": 75.700000,
    "Z": 8.244764
  }
})";
			ImGui::SetClipboardText(testJson);
			m_logger->LogInfo("Set test JSON to clipboard");
		}
	}

	ImGui::End();

	// Handle the confirmation popup
	RenderClipboardConfirmationPopup();
}

void MotionConfigEditor::RenderDevicesTab() {
	const auto& allDevices = m_configManager.GetAllDevices();

	ImGui::BeginChild("DevicesList", ImVec2(200, 0), true);

	if (ImGui::Button("Add New Device")) {
		m_isAddingNewDevice = true;
		m_newDeviceName = "new_device";

		m_editingDevice = MotionDevice();
		m_editingDevice.Name = m_newDeviceName;
		m_editingDevice.IpAddress = "192.168.0.1";
		m_editingDevice.Port = 50000;
		if (allDevices.size() <= INT_MAX) {
			m_editingDevice.Id = static_cast<int>(allDevices.size());
		}
		else {
			// Handle the error - for example:
			m_logger->LogError("Device ID exceeds maximum integer value");
			m_editingDevice.Id = 0; // Or some other suitable default
		}
		strncpy_s(m_ipAddressBuffer, sizeof(m_ipAddressBuffer), m_editingDevice.IpAddress.c_str(), _TRUNCATE);
	}

	ImGui::Separator();

	for (const auto& [name, device] : allDevices) {
		bool isSelected = (m_selectedDevice == name);

		ImVec4 color = device.IsEnabled ? ImVec4(0.0f, 0.7f, 0.0f, 1.0f) : ImVec4(0.7f, 0.0f, 0.0f, 1.0f);
		ImGui::TextColored(color, device.IsEnabled ? "* " : "o ");
		ImGui::SameLine();

		if (ImGui::Selectable(name.c_str(), isSelected)) {
			m_selectedDevice = name;
			m_isAddingNewDevice = false;
			RefreshDeviceData();
		}
	}

	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("DeviceDetails", ImVec2(0, 0), true);

	if (m_isAddingNewDevice) {
		ImGui::Text("Adding New Device");
		ImGui::Separator();

		// Fix for E0167: Use a char buffer instead of std::string directly  
		char deviceNameBuffer[64];
		strncpy_s(deviceNameBuffer, sizeof(deviceNameBuffer), m_newDeviceName.c_str(), _TRUNCATE);
		deviceNameBuffer[sizeof(deviceNameBuffer) - 1] = '\0';

		if (ImGui::InputText("Device Name", deviceNameBuffer, sizeof(deviceNameBuffer))) {
			m_newDeviceName = deviceNameBuffer;
		}

		ImGui::InputText("IP Address", m_ipAddressBuffer, sizeof(m_ipAddressBuffer));
		m_editingDevice.IpAddress = m_ipAddressBuffer;

		int port = m_editingDevice.Port;
		if (ImGui::InputInt("Port", &port, 1, 100)) {
			m_editingDevice.Port = port;
		}

		int id = m_editingDevice.Id;
		if (ImGui::InputInt("Device ID", &id, 1, 1)) {
			m_editingDevice.Id = id;
		}

		bool isEnabled = m_editingDevice.IsEnabled;
		if (ImGui::Checkbox("Enabled", &isEnabled)) {
			m_editingDevice.IsEnabled = isEnabled;
		}

		ImGui::Separator();

		if (ImGui::Button("Add Device")) {
			AddNewDevice();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			m_isAddingNewDevice = false;
		}
	}
	else if (!m_selectedDevice.empty()) {
		auto deviceOpt = m_configManager.GetDevice(m_selectedDevice);
		if (deviceOpt.has_value()) {
			const auto& device = deviceOpt.value().get();

			ImGui::Text("Editing Device: %s", m_selectedDevice.c_str());
			ImGui::Separator();

			ImGui::Text("Device Name: %s", device.Name.c_str());

			if (m_ipAddressBuffer[0] == '\0') {
				strncpy_s(m_ipAddressBuffer, sizeof(m_ipAddressBuffer), device.IpAddress.c_str(), _TRUNCATE);
			}
			if (ImGui::InputText("IP Address", m_ipAddressBuffer, sizeof(m_ipAddressBuffer))) {
				m_editingDevice.IpAddress = m_ipAddressBuffer;
			}

			int port = m_editingDevice.Port;
			if (ImGui::InputInt("Port", &port, 1, 100)) {
				m_editingDevice.Port = port;
			}

			int id = m_editingDevice.Id;
			if (ImGui::InputInt("Device ID", &id, 1, 1)) {
				m_editingDevice.Id = id;
			}

			bool isEnabled = m_editingDevice.IsEnabled;
			if (ImGui::Checkbox("Enabled", &isEnabled)) {
				m_editingDevice.IsEnabled = isEnabled;
			}

			ImGui::Separator();

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			if (ImGui::Button("Delete Device")) {
				ImGui::OpenPopup("Delete Device?");
			}
			ImGui::PopStyleColor();

			if (ImGui::BeginPopupModal("Delete Device?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::Text("Are you sure you want to delete device '%s'?", m_selectedDevice.c_str());
				ImGui::Text("This operation cannot be undone!");
				ImGui::Separator();

				if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
					DeleteSelectedDevice();
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120, 0))) {
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}
	}
	else {
		ImGui::Text("Select a device from the list or add a new one.");
	}

	ImGui::EndChild();
}

void MotionConfigEditor::RenderPositionsTab() {
	// Left panel - Device selection  
	ImGui::BeginChild("PositionsDeviceList", ImVec2(200, 0), true);

	ImGui::Text("Select a Device:");
	ImGui::Separator();

	const auto& allDevices = m_configManager.GetAllDevices();

	for (const auto& [name, device] : allDevices) {
		bool isSelected = (m_selectedDevice == name);
		if (ImGui::Selectable(name.c_str(), isSelected)) {
			m_selectedDevice = name;
			m_selectedPosition.clear();
			m_isAddingNewPosition = false;
		}
	}

	ImGui::EndChild();

	ImGui::SameLine();

	// Middle panel - Position list for selected device  
	ImGui::BeginChild("PositionsList", ImVec2(200, 0), true);

	if (!m_selectedDevice.empty()) {
		ImGui::Text("Positions for %s:", m_selectedDevice.c_str());

		if (ImGui::Button("Add New Position")) {
			m_isAddingNewPosition = true;
			m_newPositionName = "new_position";

			// Initialize with default values  
			m_editingPosition = PositionStruct();
		}

		ImGui::Separator();

		auto positionsOpt = m_configManager.GetDevicePositions(m_selectedDevice);
		if (positionsOpt.has_value()) {
			const auto& positions = positionsOpt.value().get();

			for (const auto& [name, position] : positions) {
				bool isSelected = (m_selectedPosition == name);
				if (ImGui::Selectable(name.c_str(), isSelected)) {
					m_selectedPosition = name;
					m_isAddingNewPosition = false;

					// Copy position data  
					m_editingPosition = position;
				}
			}
		}
	}
	else {
		ImGui::Text("Select a device first.");
	}

	ImGui::EndChild();

	ImGui::SameLine();

	// Right panel - Position details  
	ImGui::BeginChild("PositionDetails", ImVec2(0, 0), true);

	if (!m_selectedDevice.empty()) {
		if (m_isAddingNewPosition) {
			ImGui::Text("Adding New Position for %s", m_selectedDevice.c_str());
			ImGui::Separator();

			// Fix for E0167: Use a char buffer instead of std::string directly  
			char positionNameBuffer[64];
			strncpy_s(positionNameBuffer, sizeof(positionNameBuffer), m_newPositionName.c_str(), _TRUNCATE);
			positionNameBuffer[sizeof(positionNameBuffer) - 1] = '\0';

			if (ImGui::InputText("Position Name", positionNameBuffer, sizeof(positionNameBuffer))) {
				m_newPositionName = positionNameBuffer;
			}

			// Position coordinates  
			ImGui::Text("Coordinates:");

			// Add "Paste from Clipboard" button for new positions
			if (ImGui::Button("Paste from Clipboard")) {
				ProcessClipboardData();
			}

			ImGui::DragScalar("X", ImGuiDataType_Double, &m_editingPosition.x, 0.1f);
			ImGui::DragScalar("Y", ImGuiDataType_Double, &m_editingPosition.y, 0.1f);
			ImGui::DragScalar("Z", ImGuiDataType_Double, &m_editingPosition.z, 0.1f);
			// Only show U, V, W for hex devices
			if (m_selectedDevice.find("hex") != std::string::npos) {
				ImGui::DragScalar("U", ImGuiDataType_Double, &m_editingPosition.u, 0.1f);
				ImGui::DragScalar("V", ImGuiDataType_Double, &m_editingPosition.v, 0.1f);
				ImGui::DragScalar("W", ImGuiDataType_Double, &m_editingPosition.w, 0.1f);
			}

			ImGui::Separator();

			// Add or Cancel buttons  
			if (ImGui::Button("Add Position")) {
				AddNewPosition();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				m_isAddingNewPosition = false;
				m_editingPosition = PositionStruct();
			}
		}
		else if (!m_selectedPosition.empty()) {
			// Get the position data  
			auto posOpt = m_configManager.GetNamedPosition(m_selectedDevice, m_selectedPosition);
			if (posOpt.has_value()) {
				ImGui::Text("Editing Position: %s", m_selectedPosition.c_str());
				ImGui::Separator();

				// Position name (read-only for now)  
				ImGui::Text("Position Name: %s", m_selectedPosition.c_str());

				// Position coordinates  
				ImGui::Text("Coordinates:");

				// Add "Paste from Clipboard" button for existing positions
				if (ImGui::Button("Paste from Clipboard")) {
					ProcessClipboardData();
				}

				bool changed = false;
				changed |= ImGui::DragScalar("X", ImGuiDataType_Double, &m_editingPosition.x, 0.1f);
				changed |= ImGui::DragScalar("Y", ImGuiDataType_Double, &m_editingPosition.y, 0.1f);
				changed |= ImGui::DragScalar("Z", ImGuiDataType_Double, &m_editingPosition.z, 0.1f);

				// Only show U, V, W for hex devices
				if (m_selectedDevice.find("hex") != std::string::npos) {
					changed |= ImGui::DragScalar("U", ImGuiDataType_Double, &m_editingPosition.u, 0.1f);
					changed |= ImGui::DragScalar("V", ImGuiDataType_Double, &m_editingPosition.v, 0.1f);
					changed |= ImGui::DragScalar("W", ImGuiDataType_Double, &m_editingPosition.w, 0.1f);
				}

				ImGui::Separator();

				// Apply changes immediately  
				if (changed) {
					try {
						m_configManager.AddPosition(m_selectedDevice, m_selectedPosition, m_editingPosition);
						m_logger->LogInfo("Updated position: " + m_selectedPosition + " for device: " + m_selectedDevice);
					}
					catch (const std::exception& e) {
						m_logger->LogError("Failed to update position: " + std::string(e.what()));
					}
				}

				// Delete button  
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
				if (ImGui::Button("Delete Position")) {
					ImGui::OpenPopup("Delete Position?");
				}
				ImGui::PopStyleColor();

				// Confirmation dialog  
				if (ImGui::BeginPopupModal("Delete Position?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
					ImGui::Text("Are you sure you want to delete position '%s'?", m_selectedPosition.c_str());
					ImGui::Text("This operation cannot be undone!");
					ImGui::Separator();

					if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
						DeleteSelectedPosition();
						ImGui::CloseCurrentPopup();
					}
					ImGui::SameLine();
					if (ImGui::Button("Cancel", ImVec2(120, 0))) {
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndPopup();
				}
			}
		}
		else {
			ImGui::Text("Select a position or add a new one.");
		}
	}
	else {
		ImGui::Text("Select a device first.");
	}

	ImGui::EndChild();
}


// New function to process clipboard data
void MotionConfigEditor::ProcessClipboardData() {
	if (m_selectedDevice.empty()) {
		m_logger->LogError("No device selected");
		return;
	}

	// Get clipboard text from ImGui
	std::string clipboardText = ImGui::GetClipboardText();

	// Log the clipboard content for debugging
	m_logger->LogInfo("Clipboard content: " + clipboardText);

	if (clipboardText.empty()) {
		m_logger->LogError("Clipboard is empty");
		return;
	}

	try {
		// Parse JSON from clipboard
		json clipboardJson = json::parse(clipboardText);

		// Log successful parsing
		m_logger->LogInfo("Successfully parsed JSON from clipboard");

		// Validate the structure
		if (!clipboardJson.contains("device") || !clipboardJson.contains("positions")) {
			m_logger->LogError("Invalid clipboard format: missing 'device' or 'positions'");
			return;
		}

		std::string deviceName = clipboardJson["device"];

		// Check if the device in clipboard matches the selected device
		if (deviceName != m_selectedDevice) {
			m_logger->LogWarning("Device in clipboard (" + deviceName +
				") doesn't match selected device (" + m_selectedDevice + ")");
		}

		// Save the original position values
		m_oldPosition = m_editingPosition;

		// Initialize new position with old values as base
		m_newPosition = m_oldPosition;

		// Update position values from clipboard
		auto& positions = clipboardJson["positions"];
		if (positions.contains("X")) {
			m_newPosition.x = positions["X"];
			m_logger->LogInfo("Setting X value from clipboard: " + std::to_string(m_newPosition.x));
		}
		if (positions.contains("Y")) {
			m_newPosition.y = positions["Y"];
			m_logger->LogInfo("Setting Y value from clipboard: " + std::to_string(m_newPosition.y));
		}
		if (positions.contains("Z")) {
			m_newPosition.z = positions["Z"];
			m_logger->LogInfo("Setting Z value from clipboard: " + std::to_string(m_newPosition.z));
		}

		// Check for U, V, W (for hex devices)
		if (positions.contains("U")) {
			m_newPosition.u = positions["U"];
			m_logger->LogInfo("Setting U value from clipboard: " + std::to_string(m_newPosition.u));
		}
		if (positions.contains("V")) {
			m_newPosition.v = positions["V"];
			m_logger->LogInfo("Setting V value from clipboard: " + std::to_string(m_newPosition.v));
		}
		if (positions.contains("W")) {
			m_newPosition.w = positions["W"];
			m_logger->LogInfo("Setting W value from clipboard: " + std::to_string(m_newPosition.w));
		}

		// Show confirmation popup
		m_showClipboardConfirmation = true;
		m_logger->LogInfo("Opening confirmation popup");
		ImGui::OpenPopup("Confirm Position Update");
	}
	catch (const json::exception& e) {
		m_logger->LogError("Failed to parse clipboard data: " + std::string(e.what()));
	}
	catch (const std::exception& e) {
		m_logger->LogError("Error processing clipboard: " + std::string(e.what()));
	}
}

// New function to render the confirmation popup
void MotionConfigEditor::RenderClipboardConfirmationPopup() {
	// Always set modal flag to ensure it doesn't get lost
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_Appearing);

	// Begin the popup conditionally
	bool isOpen = true;
	if (ImGui::BeginPopupModal("Confirm Position Update", &isOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Update position values from clipboard?");
		ImGui::Separator();

		// Display device and position name
		ImGui::Text("Device: %s", m_selectedDevice.c_str());
		ImGui::Text("Position: %s",
			m_isAddingNewPosition ? m_newPositionName.c_str() : m_selectedPosition.c_str());

		ImGui::Spacing();

		// Create a table for side-by-side comparison
		if (ImGui::BeginTable("PositionValuesTable", 4, ImGuiTableFlags_Borders)) {
			// Headers
			ImGui::TableSetupColumn("Axis");
			ImGui::TableSetupColumn("Current Value");
			ImGui::TableSetupColumn("New Value");
			ImGui::TableSetupColumn("Difference");
			ImGui::TableHeadersRow();

			// Highlight colors for differences
			ImVec4 changedColor = ImVec4(1.0f, 0.8f, 0.0f, 1.0f); // Yellow
			ImVec4 unchangedColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White

			// X row
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("X");
			ImGui::TableNextColumn(); ImGui::Text("%.6f", m_oldPosition.x);
			ImGui::TableNextColumn(); ImGui::Text("%.6f", m_newPosition.x);

			// Highlight difference if values are different
			float xDiff = static_cast<float>(m_newPosition.x - m_oldPosition.x);
			ImGui::TableNextColumn();
			if (std::abs(xDiff) > 0.000001) {
				ImGui::TextColored(changedColor, "%.6f", xDiff);
			}
			else {
				ImGui::TextColored(unchangedColor, "%.6f", xDiff);
			}

			// Y row
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("Y");
			ImGui::TableNextColumn(); ImGui::Text("%.6f", m_oldPosition.y);
			ImGui::TableNextColumn(); ImGui::Text("%.6f", m_newPosition.y);

			// Highlight difference if values are different
			float yDiff = static_cast<float>( m_newPosition.y - m_oldPosition.y);
			ImGui::TableNextColumn();
			if (std::abs(yDiff) > 0.000001) {
				ImGui::TextColored(changedColor, "%.6f", yDiff);
			}
			else {
				ImGui::TextColored(unchangedColor, "%.6f", yDiff);
			}

			// Z row
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("Z");
			ImGui::TableNextColumn(); ImGui::Text("%.6f", m_oldPosition.z);
			ImGui::TableNextColumn(); ImGui::Text("%.6f", m_newPosition.z);

			// Highlight difference if values are different
			float zDiff = static_cast<float>(m_newPosition.z - m_oldPosition.z);
			ImGui::TableNextColumn();
			if (std::abs(zDiff) > 0.000001) {
				ImGui::TextColored(changedColor, "%.6f", zDiff);
			}
			else {
				ImGui::TextColored(unchangedColor, "%.6f", zDiff);
			}

			// Only show U, V, W for hex devices
			if (m_selectedDevice.find("hex") != std::string::npos) {
				// U row
				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::Text("U");
				ImGui::TableNextColumn(); ImGui::Text("%.6f", m_oldPosition.u);
				ImGui::TableNextColumn(); ImGui::Text("%.6f", m_newPosition.u);

				float uDiff = static_cast<float>(m_newPosition.u - m_oldPosition.u);
				ImGui::TableNextColumn();
				if (std::abs(uDiff) > 0.000001) {
					ImGui::TextColored(changedColor, "%.6f", uDiff);
				}
				else {
					ImGui::TextColored(unchangedColor, "%.6f", uDiff);
				}

				// V row
				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::Text("V");
				ImGui::TableNextColumn(); ImGui::Text("%.6f", m_oldPosition.v);
				ImGui::TableNextColumn(); ImGui::Text("%.6f", m_newPosition.v);

				float vDiff = static_cast<float>(m_newPosition.v - m_oldPosition.v);
				ImGui::TableNextColumn();
				if (std::abs(vDiff) > 0.000001) {
					ImGui::TextColored(changedColor, "%.6f", vDiff);
				}
				else {
					ImGui::TextColored(unchangedColor, "%.6f", vDiff);
				}

				// W row
				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::Text("W");
				ImGui::TableNextColumn(); ImGui::Text("%.6f", m_oldPosition.w);
				ImGui::TableNextColumn(); ImGui::Text("%.6f", m_newPosition.w);

				float wDiff = static_cast<float>(m_newPosition.w - m_oldPosition.w);
				ImGui::TableNextColumn();
				if (std::abs(wDiff) > 0.000001) {
					ImGui::TextColored(changedColor, "%.6f", wDiff);
				}
				else {
					ImGui::TextColored(unchangedColor, "%.6f", wDiff);
				}
			}

			ImGui::EndTable();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// Buttons for confirmation or cancellation
		ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 250) / 2); // Center buttons
		if (ImGui::Button("Confirm", ImVec2(120, 0))) {
			// Apply new position values
			m_editingPosition = m_newPosition;

			// If not adding a new position, update the existing one
			if (!m_isAddingNewPosition && !m_selectedPosition.empty()) {
				try {
					m_configManager.AddPosition(m_selectedDevice, m_selectedPosition, m_editingPosition);
					m_logger->LogInfo("Updated position from clipboard: " + m_selectedPosition +
						" for device: " + m_selectedDevice);
				}
				catch (const std::exception& e) {
					m_logger->LogError("Failed to update position: " + std::string(e.what()));
				}
			}

			m_showClipboardConfirmation = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();

		if (ImGui::Button("Cancel", ImVec2(120, 0))) {
			m_showClipboardConfirmation = false;
			ImGui::CloseCurrentPopup();
		}

		// If dialog is closed by any means, reset the flag
		if (!isOpen) {
			m_showClipboardConfirmation = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	// If the popup flag is set but the popup isn't showing, force open it
	else if (m_showClipboardConfirmation) {
		ImGui::OpenPopup("Confirm Position Update");
	}
}



void MotionConfigEditor::RenderSettingsTab() {
	ImGui::Text("Settings editing is not implemented yet.");

	// Display current settings
	const auto& settings = m_configManager.GetSettings();

	ImGui::Text("Current Settings:");
	ImGui::BulletText("Default Speed: %.2f", settings.DefaultSpeed);
	ImGui::BulletText("Default Acceleration: %.2f", settings.DefaultAcceleration);
	ImGui::BulletText("Log Level: %s", settings.LogLevel.c_str());
	ImGui::BulletText("Auto Reconnect: %s", settings.AutoReconnect ? "Yes" : "No");
	ImGui::BulletText("Connection Timeout: %d ms", settings.ConnectionTimeout);
	ImGui::BulletText("Position Tolerance: %.3f", settings.PositionTolerance);
}

void MotionConfigEditor::RefreshDeviceData() {
	if (!m_selectedDevice.empty()) {
		auto deviceOpt = m_configManager.GetDevice(m_selectedDevice);
		if (deviceOpt.has_value()) {
			const auto& device = deviceOpt.value().get();

			// Copy data to editing structure
			m_editingDevice.IsEnabled = device.IsEnabled;
			m_editingDevice.IpAddress = device.IpAddress;
			m_editingDevice.Port = device.Port;
			m_editingDevice.Id = device.Id;
			m_editingDevice.Name = device.Name;

			// Update IP address buffer
			strncpy_s(m_ipAddressBuffer, sizeof(m_ipAddressBuffer), device.IpAddress.c_str(), _TRUNCATE);
			m_ipAddressBuffer[sizeof(m_ipAddressBuffer) - 1] = '\0';  // Ensure null termination
		}
	}
}

void MotionConfigEditor::SaveChanges() {
	try {
		bool success = m_configManager.SaveConfig();
		if (success) {
			m_logger->LogInfo("Configuration saved successfully");
		}
		else {
			m_logger->LogError("Failed to save configuration");
		}
	}
	catch (const std::exception& e) {
		m_logger->LogError("Exception while saving configuration: " + std::string(e.what()));
	}
}

void MotionConfigEditor::DeleteSelectedDevice() {
	if (m_selectedDevice.empty()) {
		return;
	}

	try {
		bool success = m_configManager.DeleteDevice(m_selectedDevice);
		if (success) {
			m_logger->LogInfo("Device deleted: " + m_selectedDevice);
			m_selectedDevice.clear(); // Clear selection
		}
		else {
			m_logger->LogError("Failed to delete device: " + m_selectedDevice);
		}
	}
	catch (const std::exception& e) {
		m_logger->LogError("Error deleting device: " + std::string(e.what()));
	}
}

void MotionConfigEditor::DeleteSelectedPosition() {
	if (m_selectedDevice.empty() || m_selectedPosition.empty()) {
		return;
	}

	try {
		bool success = m_configManager.DeletePosition(m_selectedDevice, m_selectedPosition);
		if (success) {
			m_logger->LogInfo("Position deleted: " + m_selectedPosition + " from device: " + m_selectedDevice);
			m_selectedPosition.clear(); // Clear selection

			SaveChanges();
		}
		else {
			m_logger->LogError("Failed to delete position: " + m_selectedPosition);
		}
	}
	catch (const std::exception& e) {
		m_logger->LogError("Error deleting position: " + std::string(e.what()));
	}
}

void MotionConfigEditor::AddNewDevice() {
	if (m_newDeviceName.empty()) {
		m_logger->LogError("Cannot add device with empty name");
		return;
	}

	// Check if device already exists
	const auto& allDevices = m_configManager.GetAllDevices();
	if (allDevices.find(m_newDeviceName) != allDevices.end()) {
		m_logger->LogError("Device already exists: " + m_newDeviceName);
		return;
	}

	m_logger->LogInfo("Adding new device: " + m_newDeviceName);

	// Set device name from input
	m_editingDevice.Name = m_newDeviceName;

	try {
		// Add the new device to the config manager
		m_configManager.AddDevice(m_newDeviceName, m_editingDevice);

		// Set the new device as the selected device
		m_selectedDevice = m_newDeviceName;

		m_logger->LogInfo("Device added successfully: " + m_newDeviceName);
	}
	catch (const std::exception& e) {
		m_logger->LogError("Failed to add device: " + std::string(e.what()));
	}

	// Reset state
	m_isAddingNewDevice = false;
	m_newDeviceName.clear();
	std::memset(m_ipAddressBuffer, 0, sizeof(m_ipAddressBuffer));
}

void MotionConfigEditor::AddNewPosition() {
	if (m_newPositionName.empty() || m_selectedDevice.empty()) {
		m_logger->LogError("Cannot add position: Invalid device or position name");
		return;
	}

	// Check if position already exists
	auto positionsOpt = m_configManager.GetDevicePositions(m_selectedDevice);
	if (positionsOpt.has_value()) {
		const auto& positions = positionsOpt.value().get();
		if (positions.find(m_newPositionName) != positions.end()) {
			m_logger->LogError("Position already exists: " + m_newPositionName);
			return;
		}
	}

	try {
		m_configManager.AddPosition(m_selectedDevice, m_newPositionName, m_editingPosition);
		m_logger->LogInfo("Added new position: " + m_newPositionName + " to device: " + m_selectedDevice);

		// Update selection
		m_selectedPosition = m_newPositionName;
		m_isAddingNewPosition = false;

		SaveChanges();
		RefreshGraphData();
	}
	catch (const std::exception& e) {
		m_logger->LogError("Failed to add position: " + std::string(e.what()));
	}
}

void MotionConfigEditor::RenderGraphsTab() {
	// Left panel - Graph list
	ImGui::BeginChild("GraphList", ImVec2(200, 0), true);
	RenderGraphList();
	ImGui::EndChild();

	ImGui::SameLine();

	// Add resize handle/splitter
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.7f, 0.7f, 0.7f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.9f, 0.9f, 0.9f));
	ImGui::Button("##splitter", ImVec2(8.0f, -1));
	if (ImGui::IsItemActive()) {
		// Resize based on mouse movement
		m_middleColumnWidth += ImGui::GetIO().MouseDelta.x;
		// Enforce minimum width
		m_middleColumnWidth = std::max(m_middleColumnWidth, 100.0f);
		// You could also set a maximum width if needed
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();

	// Middle panel - Nodes/Edges selection with dynamic width
	ImGui::BeginChild("NodesEdgesList", ImVec2(m_middleColumnWidth, 0), true);

	if (!m_selectedGraph.empty()) {
		// Tabs for nodes and edges
		if (ImGui::BeginTabBar("GraphElementsTab")) {
			if (ImGui::BeginTabItem("Nodes")) {
				RenderNodeList();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Edges")) {
				RenderEdgeList();
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}
	}
	else {
		ImGui::Text("Select a graph first.");
	}

	ImGui::EndChild();

	ImGui::SameLine();

	// Right panel - Node/Edge details
	ImGui::BeginChild("ElementDetails", ImVec2(0, 0), true);

	if (!m_selectedGraph.empty()) {
		if (!m_selectedNode.empty()) {
			RenderNodeDetails();
		}
		else if (!m_selectedEdge.empty()) {
			RenderEdgeDetails();
		}
		else if (m_isAddingNewNode) {
			RenderNodeDetails();
		}
		else if (m_isAddingNewEdge) {
			RenderEdgeDetails();
		}
		else {
			ImGui::Text("Select a node or edge to edit its details.");
		}
	}
	else {
		ImGui::Text("Select a graph first.");
	}

	ImGui::EndChild();
}

void MotionConfigEditor::RenderGraphList() {
	ImGui::Text("Available Graphs");

	ImGui::Separator();

	const auto& graphs = m_configManager.GetAllGraphs();

	for (const auto& [name, graph] : graphs) {
		bool isSelected = (m_selectedGraph == name);
		if (ImGui::Selectable(name.c_str(), isSelected)) {
			m_selectedGraph = name;
			m_selectedNode.clear();
			m_selectedEdge.clear();
			m_isAddingNewNode = false;
			m_isAddingNewEdge = false;
			RefreshGraphData();
		}
	}
}

// In MotionConfigEditor.cpp, update the RenderNodeList function


// First, make sure to include the set header at the top of MotionConfigEditor.cpp:
// Add this with the other includes at the top of the file
#include <set>

// Add a member variable to MotionConfigEditor.h in the private section:
// std::string m_deviceFilter = ""; // For filtering nodes/edges by device

// Then update the RenderNodeList function in MotionConfigEditor.cpp:

void MotionConfigEditor::RenderNodeList() {
	ImGui::Text("Nodes for %s", m_selectedGraph.c_str());

	// Add device filter dropdown
	if (ImGui::BeginCombo("Filter by Device", m_deviceFilter.empty() ? "All Devices" : m_deviceFilter.c_str())) {
		// Add "All Devices" option
		bool isSelected = m_deviceFilter.empty();
		if (ImGui::Selectable("All Devices", isSelected)) {
			m_deviceFilter = "";
		}
		if (isSelected) {
			ImGui::SetItemDefaultFocus();
		}

		// Get all unique devices from nodes in the current graph
		std::set<std::string> devices;
		auto graphOpt = m_configManager.GetGraph(m_selectedGraph);
		if (graphOpt.has_value()) {
			const auto& graph = graphOpt.value().get();
			for (const auto& node : graph.Nodes) {
				if (!node.Device.empty()) {
					devices.insert(node.Device);
				}
			}
		}

		// Add each device to the dropdown
		for (const auto& device : devices) {
			isSelected = (m_deviceFilter == device);
			if (ImGui::Selectable(device.c_str(), isSelected)) {
				m_deviceFilter = device;
			}
			if (isSelected) {
				ImGui::SetItemDefaultFocus();
			}
		}

		ImGui::EndCombo();
	}

	if (ImGui::Button("Add New Node")) {
		m_isAddingNewNode = true;
		m_isAddingNewEdge = false;
		m_selectedNode.clear();
		m_selectedEdge.clear();

		// Initialize with default values
		m_editingNode = Node();
		m_newNodeId = "node_" + std::to_string(time(nullptr) % 10000);
		m_newNodeLabel = "New Node";
		m_newNodeDevice = m_deviceFilter; // Use selected device as default if filtering
		m_newNodePosition = "";

		// Update buffers
		strncpy_s(m_nodeIdBuffer, sizeof(m_nodeIdBuffer), m_newNodeId.c_str(), _TRUNCATE);
		strncpy_s(m_nodeLabelBuffer, sizeof(m_nodeLabelBuffer), m_newNodeLabel.c_str(), _TRUNCATE);
		strncpy_s(m_nodeDeviceBuffer, sizeof(m_nodeDeviceBuffer), m_newNodeDevice.c_str(), _TRUNCATE);
		strncpy_s(m_nodePositionBuffer, sizeof(m_nodePositionBuffer), m_newNodePosition.c_str(), _TRUNCATE);
	}

	ImGui::Separator();

	auto graphOpt = m_configManager.GetGraph(m_selectedGraph);
	if (graphOpt.has_value()) {
		const auto& graph = graphOpt.value().get();

		for (const auto& node : graph.Nodes) {
			// Skip nodes that don't match the device filter
			if (!m_deviceFilter.empty() && node.Device != m_deviceFilter) {
				continue;
			}

			bool isSelected = (m_selectedNode == node.Id);

			// Create a more informative display format: label (id) - device.position
			// If label is empty, just show the ID
			std::string displayText;
			if (!node.Label.empty()) {
				displayText = node.Label + " (" + node.Id + ")";
			}
			else {
				displayText = node.Id;
			}

			// Add device and position info if available
			if (!node.Device.empty() && !node.Position.empty()) {
				displayText += " - " + node.Device + "." + node.Position;
			}

			if (ImGui::Selectable(displayText.c_str(), isSelected)) {
				m_selectedNode = node.Id;
				m_selectedEdge.clear();
				m_isAddingNewNode = false;
				m_isAddingNewEdge = false;

				// Copy node data
				m_editingNode = node;

				// Update buffers
				strncpy_s(m_nodeIdBuffer, sizeof(m_nodeIdBuffer), node.Id.c_str(), _TRUNCATE);
				strncpy_s(m_nodeLabelBuffer, sizeof(m_nodeLabelBuffer), node.Label.c_str(), _TRUNCATE);
				strncpy_s(m_nodeDeviceBuffer, sizeof(m_nodeDeviceBuffer), node.Device.c_str(), _TRUNCATE);
				strncpy_s(m_nodePositionBuffer, sizeof(m_nodePositionBuffer), node.Position.c_str(), _TRUNCATE);
			}
		}
	}
}

// Now update the RenderEdgeList function in MotionConfigEditor.cpp:

void MotionConfigEditor::RenderEdgeList() {
	ImGui::Text("Edges for %s", m_selectedGraph.c_str());

	// Add device filter dropdown
	if (ImGui::BeginCombo("Filter by Device", m_deviceFilter.empty() ? "All Devices" : m_deviceFilter.c_str())) {
		// Add "All Devices" option
		bool isSelected = m_deviceFilter.empty();
		if (ImGui::Selectable("All Devices", isSelected)) {
			m_deviceFilter = "";
		}
		if (isSelected) {
			ImGui::SetItemDefaultFocus();
		}

		// Get all unique devices from nodes in the current graph
		std::set<std::string> devices;
		auto graphOpt = m_configManager.GetGraph(m_selectedGraph);
		if (graphOpt.has_value()) {
			const auto& graph = graphOpt.value().get();
			for (const auto& node : graph.Nodes) {
				if (!node.Device.empty()) {
					devices.insert(node.Device);
				}
			}
		}

		// Add each device to the dropdown
		for (const auto& device : devices) {
			isSelected = (m_deviceFilter == device);
			if (ImGui::Selectable(device.c_str(), isSelected)) {
				m_deviceFilter = device;
			}
			if (isSelected) {
				ImGui::SetItemDefaultFocus();
			}
		}

		ImGui::EndCombo();
	}

	if (ImGui::Button("Add New Edge")) {
		m_isAddingNewEdge = true;
		m_isAddingNewNode = false;
		m_selectedEdge.clear();
		m_selectedNode.clear();

		// Initialize with default values
		m_editingEdge = Edge();
		m_newEdgeId = "edge_" + std::to_string(time(nullptr) % 10000);
		m_newEdgeLabel = "New Edge";
		m_newEdgeSource = "";
		m_newEdgeTarget = "";

		// Update buffers
		strncpy_s(m_edgeIdBuffer, sizeof(m_edgeIdBuffer), m_newEdgeId.c_str(), _TRUNCATE);
		strncpy_s(m_edgeLabelBuffer, sizeof(m_edgeLabelBuffer), m_newEdgeLabel.c_str(), _TRUNCATE);
		strncpy_s(m_edgeSourceBuffer, sizeof(m_edgeSourceBuffer), m_newEdgeSource.c_str(), _TRUNCATE);
		strncpy_s(m_edgeTargetBuffer, sizeof(m_edgeTargetBuffer), m_newEdgeTarget.c_str(), _TRUNCATE);
	}

	ImGui::Separator();

	auto graphOpt = m_configManager.GetGraph(m_selectedGraph);
	if (graphOpt.has_value()) {
		const auto& graph = graphOpt.value().get();

		// Create a map of node IDs to node info for quick lookup
		std::map<std::string, const Node*> nodeMap;
		for (const auto& node : graph.Nodes) {
			nodeMap[node.Id] = &node;
		}

		for (const auto& edge : graph.Edges) {
			// If a device filter is active, check if either the source or target node
			// belongs to the filtered device
			if (!m_deviceFilter.empty()) {
				bool matchesFilter = false;

				// Check source node
				auto sourceIt = nodeMap.find(edge.Source);
				if (sourceIt != nodeMap.end() && sourceIt->second->Device == m_deviceFilter) {
					matchesFilter = true;
				}

				// Check target node
				if (!matchesFilter) {
					auto targetIt = nodeMap.find(edge.Target);
					if (targetIt != nodeMap.end() && targetIt->second->Device == m_deviceFilter) {
						matchesFilter = true;
					}
				}

				// Skip edges that don't match the filter
				if (!matchesFilter) {
					continue;
				}
			}

			bool isSelected = (m_selectedEdge == edge.Id);

			// Get source and target node labels
			std::string sourceLabel = "unknown";
			std::string targetLabel = "unknown";

			auto sourceIt = nodeMap.find(edge.Source);
			if (sourceIt != nodeMap.end()) {
				sourceLabel = sourceIt->second->Label.empty() ? edge.Source : sourceIt->second->Label;
			}

			auto targetIt = nodeMap.find(edge.Target);
			if (targetIt != nodeMap.end()) {
				targetLabel = targetIt->second->Label.empty() ? edge.Target : targetIt->second->Label;
			}

			// Create direction symbol based on bidirectional property
			std::string directionSymbol = edge.Conditions.IsBidirectional ? " <-> " : " -> ";

			// Create edge display text: edge_label (source_label direction_symbol target_label)
			std::string edgeLabel = edge.Label.empty() ? edge.Id : edge.Label;
			std::string displayText = edgeLabel + " (" + sourceLabel + directionSymbol + targetLabel + ")";

			if (ImGui::Selectable(displayText.c_str(), isSelected)) {
				m_selectedEdge = edge.Id;
				m_selectedNode.clear();
				m_isAddingNewNode = false;
				m_isAddingNewEdge = false;

				// Copy edge data
				m_editingEdge = edge;

				// Update buffers
				strncpy_s(m_edgeIdBuffer, sizeof(m_edgeIdBuffer), edge.Id.c_str(), _TRUNCATE);
				strncpy_s(m_edgeLabelBuffer, sizeof(m_edgeLabelBuffer), edge.Label.c_str(), _TRUNCATE);
				strncpy_s(m_edgeSourceBuffer, sizeof(m_edgeSourceBuffer), edge.Source.c_str(), _TRUNCATE);
				strncpy_s(m_edgeTargetBuffer, sizeof(m_edgeTargetBuffer), edge.Target.c_str(), _TRUNCATE);
			}
		}
	}
}





void MotionConfigEditor::RenderNodeDetails() {
	if (m_isAddingNewNode) {
		ImGui::Text("Adding New Node to %s", m_selectedGraph.c_str());
	}
	else {
		ImGui::Text("Editing Node: %s", m_selectedNode.c_str());
	}

	ImGui::Separator();

	// Node ID
	if (m_isAddingNewNode) {
		if (ImGui::InputText("Node ID", m_nodeIdBuffer, sizeof(m_nodeIdBuffer))) {
			m_newNodeId = m_nodeIdBuffer;
		}
	}
	else {
		ImGui::Text("Node ID: %s", m_nodeIdBuffer);
	}

	// Node Label
	if (ImGui::InputText("Label", m_nodeLabelBuffer, sizeof(m_nodeLabelBuffer))) {
		if (m_isAddingNewNode) {
			m_newNodeLabel = m_nodeLabelBuffer;
		}
		else {
			m_editingNode.Label = m_nodeLabelBuffer;
		}
	}

	// Device selection dropdown
	if (ImGui::BeginCombo("Device", m_nodeDeviceBuffer)) {
		const auto& devices = m_configManager.GetAllDevices();
		for (const auto& [deviceName, device] : devices) {
			bool isSelected = (deviceName == std::string(m_nodeDeviceBuffer));
			if (ImGui::Selectable(deviceName.c_str(), isSelected)) {
				strncpy_s(m_nodeDeviceBuffer, sizeof(m_nodeDeviceBuffer), deviceName.c_str(), _TRUNCATE);
				if (m_isAddingNewNode) {
					m_newNodeDevice = deviceName;
				}
				else {
					m_editingNode.Device = deviceName;
				}
			}
			if (isSelected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	// Position selection dropdown (based on selected device)
	if (ImGui::BeginCombo("Position", m_nodePositionBuffer)) {
		std::string selectedDevice = m_isAddingNewNode ? m_newNodeDevice : m_editingNode.Device;
		if (!selectedDevice.empty()) {
			auto positionsOpt = m_configManager.GetDevicePositions(selectedDevice);
			if (positionsOpt.has_value()) {
				const auto& positions = positionsOpt.value().get();
				for (const auto& [posName, pos] : positions) {
					bool isSelected = (posName == std::string(m_nodePositionBuffer));
					if (ImGui::Selectable(posName.c_str(), isSelected)) {
						strncpy_s(m_nodePositionBuffer, sizeof(m_nodePositionBuffer), posName.c_str(), _TRUNCATE);
						// Removed auto-generation code

						if (m_isAddingNewNode) {
							m_newNodePosition = posName;
						}
						else {
							m_editingNode.Position = posName;
						}
					}
					if (isSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}
			}
		}
		ImGui::EndCombo();
	}

	// Node position in graph
	int x = m_isAddingNewNode ? 100 : m_editingNode.X;
	int y = m_isAddingNewNode ? 100 : m_editingNode.Y;

	if (ImGui::InputInt("X Position", &x, 10, 50)) {
		if (m_isAddingNewNode) {
			m_editingNode.X = x;
		}
		else {
			m_editingNode.X = x;
		}
	}

	if (ImGui::InputInt("Y Position", &y, 10, 50)) {
		if (m_isAddingNewNode) {
			m_editingNode.Y = y;
		}
		else {
			m_editingNode.Y = y;
		}
	}

	ImGui::Separator();

	// Add/Update or Delete buttons
	if (m_isAddingNewNode) {
		if (ImGui::Button("Add Node")) {
			AddNewNode();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			m_isAddingNewNode = false;
		}
	}
	else if (!m_selectedNode.empty()) {
		if (ImGui::Button("Update Node")) {
			// Update the node in the graph
			UpdateGraph();
			m_logger->LogInfo("Updated node: " + m_selectedNode + " in graph: " + m_selectedGraph);

			// Add this line to refresh the UI
			RefreshGraphData();
		}

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
		if (ImGui::Button("Delete Node")) {
			ImGui::OpenPopup("Delete Node?");
		}
		ImGui::PopStyleColor();

		if (ImGui::BeginPopupModal("Delete Node?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Are you sure you want to delete node '%s'?", m_selectedNode.c_str());
			ImGui::Text("This operation cannot be undone!");
			ImGui::Separator();

			if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
				DeleteSelectedNode();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}
}

void MotionConfigEditor::RenderEdgeDetails() {
	if (m_isAddingNewEdge) {
		ImGui::Text("Adding New Edge to %s", m_selectedGraph.c_str());
	}
	else {
		ImGui::Text("Editing Edge: %s", m_selectedEdge.c_str());
	}

	ImGui::Separator();

	// Edge ID
	if (m_isAddingNewEdge) {
		if (ImGui::InputText("Edge ID", m_edgeIdBuffer, sizeof(m_edgeIdBuffer))) {
			m_newEdgeId = m_edgeIdBuffer;
		}
	}
	else {
		ImGui::Text("Edge ID: %s", m_edgeIdBuffer);
	}

	// Edge Label
	if (ImGui::InputText("Label", m_edgeLabelBuffer, sizeof(m_edgeLabelBuffer))) {
		if (m_isAddingNewEdge) {
			m_newEdgeLabel = m_edgeLabelBuffer;
		}
		else {
			m_editingEdge.Label = m_edgeLabelBuffer;
		}
	}

	// Source Node dropdown
// For Source Node dropdown
	std::string sourceNodeLabel = "Source Node";
	std::string sourceNodeId = m_isAddingNewEdge ? m_newEdgeSource : m_editingEdge.Source;
	if (!sourceNodeId.empty() && !m_selectedGraph.empty()) {
		auto nodePtr = m_configManager.GetNodeById(m_selectedGraph, sourceNodeId);
		if (nodePtr && !nodePtr->Device.empty() && !nodePtr->Position.empty()) {
			sourceNodeLabel += " (" + nodePtr->Device + "." + nodePtr->Position + ")";
		}
	}
	if (ImGui::BeginCombo(sourceNodeLabel.c_str(), m_edgeSourceBuffer)) {
		auto graphOpt = m_configManager.GetGraph(m_selectedGraph);
		if (graphOpt.has_value()) {
			const auto& graph = graphOpt.value().get();
			for (const auto& node : graph.Nodes) {
				// Create display text with device and position info
				std::string displayText = node.Id;
				if (!node.Device.empty() && !node.Position.empty()) {
					displayText += " (" + node.Device + "." + node.Position + ")";
				}

				bool isSelected = (node.Id == std::string(m_edgeSourceBuffer));
				if (ImGui::Selectable(displayText.c_str(), isSelected)) {
					strncpy_s(m_edgeSourceBuffer, sizeof(m_edgeSourceBuffer), node.Id.c_str(), _TRUNCATE);
					if (m_isAddingNewEdge) {
						m_newEdgeSource = node.Id;
					}
					else {
						m_editingEdge.Source = node.Id;
					}
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
		}
		ImGui::EndCombo();
	}

	// Target Node dropdown - Apply the same change
	std::string targetNodeLabel = "Target Node";
	std::string targetNodeId = m_isAddingNewEdge ? m_newEdgeTarget : m_editingEdge.Target;
	if (!targetNodeId.empty() && !m_selectedGraph.empty()) {
		auto nodePtr = m_configManager.GetNodeById(m_selectedGraph, targetNodeId);
		if (nodePtr && !nodePtr->Device.empty() && !nodePtr->Position.empty()) {
			targetNodeLabel += " (" + nodePtr->Device + "." + nodePtr->Position + ")";
		}
	}

	if (ImGui::BeginCombo(targetNodeLabel.c_str(), m_edgeTargetBuffer)) {
		auto graphOpt = m_configManager.GetGraph(m_selectedGraph);
		if (graphOpt.has_value()) {
			const auto& graph = graphOpt.value().get();
			for (const auto& node : graph.Nodes) {
				// Create display text with device and position info
				std::string displayText = node.Id;
				if (!node.Device.empty() && !node.Position.empty()) {
					displayText += " (" + node.Device + "." + node.Position + ")";
				}

				bool isSelected = (node.Id == std::string(m_edgeTargetBuffer));
				if (ImGui::Selectable(displayText.c_str(), isSelected)) {
					strncpy_s(m_edgeTargetBuffer, sizeof(m_edgeTargetBuffer), node.Id.c_str(), _TRUNCATE);
					if (m_isAddingNewEdge) {
						m_newEdgeTarget = node.Id;
					}
					else {
						m_editingEdge.Target = node.Id;
					}
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
		}
		ImGui::EndCombo();
	}

	// Edge Conditions
	ImGui::Text("Edge Conditions:");

	bool requiresApproval = m_isAddingNewEdge ? false : m_editingEdge.Conditions.RequiresOperatorApproval;
	if (ImGui::Checkbox("Requires Operator Approval", &requiresApproval)) {
		if (m_isAddingNewEdge) {
			m_editingEdge.Conditions.RequiresOperatorApproval = requiresApproval;
		}
		else {
			m_editingEdge.Conditions.RequiresOperatorApproval = requiresApproval;
		}
	}

	int timeout = m_isAddingNewEdge ? 30 : m_editingEdge.Conditions.TimeoutSeconds;
	if (ImGui::InputInt("Timeout (seconds)", &timeout, 5, 30)) {
		if (timeout < 0) timeout = 0;
		if (m_isAddingNewEdge) {
			m_editingEdge.Conditions.TimeoutSeconds = timeout;
		}
		else {
			m_editingEdge.Conditions.TimeoutSeconds = timeout;
		}
	}

	// Add this after the timeout input in RenderEdgeDetails()
	bool isBidirectional = m_isAddingNewEdge ? false : m_editingEdge.Conditions.IsBidirectional;
	if (ImGui::Checkbox("Bidirectional", &isBidirectional)) {
		if (m_isAddingNewEdge) {
			m_editingEdge.Conditions.IsBidirectional = isBidirectional;
		}
		else {
			m_editingEdge.Conditions.IsBidirectional = isBidirectional;
		}
	}

	ImGui::Separator();

	// Add/Update or Delete buttons
	if (m_isAddingNewEdge) {
		if (ImGui::Button("Add Edge")) {
			AddNewEdge();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			m_isAddingNewEdge = false;
		}
	}
	else if (!m_selectedEdge.empty()) {
		if (ImGui::Button("Update Edge")) {
			// Update the edge in the graph
			UpdateGraph();
			m_logger->LogInfo("Updated edge: " + m_selectedEdge + " in graph: " + m_selectedGraph);
		}

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
		if (ImGui::Button("Delete Edge")) {
			ImGui::OpenPopup("Delete Edge?");
		}
		ImGui::PopStyleColor();

		if (ImGui::BeginPopupModal("Delete Edge?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Are you sure you want to delete edge '%s'?", m_selectedEdge.c_str());
			ImGui::Text("This operation cannot be undone!");
			ImGui::Separator();

			if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
				DeleteSelectedEdge();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}
}

void MotionConfigEditor::AddNewNode() {
	if (m_selectedGraph.empty() || m_newNodeId.empty()) {
		m_logger->LogError("Cannot add node: Invalid graph or node ID");
		return;
	}

	// Check if node ID already exists
	auto graphOpt = m_configManager.GetGraph(m_selectedGraph);
	if (!graphOpt.has_value()) {
		m_logger->LogError("Graph not found: " + m_selectedGraph);
		return;
	}

	const auto& graph = graphOpt.value().get();
	for (const auto& node : graph.Nodes) {
		if (node.Id == m_newNodeId) {
			m_logger->LogError("Node ID already exists: " + m_newNodeId);
			return;
		}
	}

	// Set node properties
	m_editingNode.Id = m_newNodeId;
	m_editingNode.Label = m_newNodeLabel;
	m_editingNode.Device = m_newNodeDevice;
	m_editingNode.Position = m_newNodePosition;

	// Add the node to the graph (we'll need to extend MotionConfigManager for this)
	UpdateGraph();

	m_logger->LogInfo("Added new node: " + m_newNodeId + " to graph: " + m_selectedGraph);

	// Update selection
	m_selectedNode = m_newNodeId;
	m_isAddingNewNode = false;

	SaveChanges();
	RefreshGraphData();
}

void MotionConfigEditor::DeleteSelectedNode() {
	if (m_selectedGraph.empty() || m_selectedNode.empty()) {
		return;
	}

	// Check if node is used in any edges
	auto graphOpt = m_configManager.GetGraph(m_selectedGraph);
	if (!graphOpt.has_value()) {
		return;
	}

	const auto& graph = graphOpt.value().get();
	for (const auto& edge : graph.Edges) {
		if (edge.Source == m_selectedNode || edge.Target == m_selectedNode) {
			m_logger->LogError("Cannot delete node: " + m_selectedNode + " because it is used in edge: " + edge.Id);
			return;
		}
	}

	// Create an updated graph without this node
	Graph updatedGraph = graph;
	auto beforeSize = updatedGraph.Nodes.size();

	updatedGraph.Nodes.erase(
		std::remove_if(updatedGraph.Nodes.begin(), updatedGraph.Nodes.end(),
			[this](const Node& node) { return node.Id == m_selectedNode; }),
		updatedGraph.Nodes.end());

	auto afterSize = updatedGraph.Nodes.size();

	if (beforeSize == afterSize) {
		m_logger->LogWarning("Node not found for deletion: " + m_selectedNode);
		return;
	}

	// Update the graph in the config manager
	try {
		m_configManager.UpdateGraph(m_selectedGraph, updatedGraph);
		m_logger->LogInfo("Deleted node: " + m_selectedNode + " from graph: " + m_selectedGraph);
	}
	catch (const std::exception& e) {
		m_logger->LogError("Failed to delete node: " + std::string(e.what()));
		return;
	}

	// Clear selection 
	m_selectedNode.clear();

	// Force the UI to refresh
	RefreshGraphData();

	// Save changes to persist them
	SaveChanges();
}

void MotionConfigEditor::AddNewEdge() {
	if (m_selectedGraph.empty() || m_newEdgeId.empty() || m_newEdgeSource.empty() || m_newEdgeTarget.empty()) {
		m_logger->LogError("Cannot add edge: Missing required fields");
		return;
	}

	// Check if edge ID already exists
	auto graphOpt = m_configManager.GetGraph(m_selectedGraph);
	if (!graphOpt.has_value()) {
		m_logger->LogError("Graph not found: " + m_selectedGraph);
		return;
	}

	const auto& graph = graphOpt.value().get();
	for (const auto& edge : graph.Edges) {
		if (edge.Id == m_newEdgeId) {
			m_logger->LogError("Edge ID already exists: " + m_newEdgeId);
			return;
		}
	}

	// Set edge properties
	m_editingEdge.Id = m_newEdgeId;
	m_editingEdge.Label = m_newEdgeLabel;
	m_editingEdge.Source = m_newEdgeSource;
	m_editingEdge.Target = m_newEdgeTarget;

	// Add the edge to the graph (requires extending MotionConfigManager)
	UpdateGraph();

	m_logger->LogInfo("Added new edge: " + m_newEdgeId + " to graph: " + m_selectedGraph);

	// Update selection
	m_selectedEdge = m_newEdgeId;
	m_isAddingNewEdge = false;


	SaveChanges();
	RefreshGraphData();
}

void MotionConfigEditor::DeleteSelectedEdge() {
	if (m_selectedGraph.empty() || m_selectedEdge.empty()) {
		return;
	}

	// Get the current graph
	auto graphOpt = m_configManager.GetGraph(m_selectedGraph);
	if (!graphOpt.has_value()) {
		return;
	}

	// Create an updated graph without the selected edge
	Graph updatedGraph = graphOpt.value().get();
	auto beforeSize = updatedGraph.Edges.size();

	updatedGraph.Edges.erase(
		std::remove_if(updatedGraph.Edges.begin(), updatedGraph.Edges.end(),
			[this](const Edge& edge) { return edge.Id == m_selectedEdge; }),
		updatedGraph.Edges.end());

	auto afterSize = updatedGraph.Edges.size();

	if (beforeSize == afterSize) {
		m_logger->LogWarning("Edge not found for deletion: " + m_selectedEdge);
		return;
	}

	// Update the graph in the config manager
	try {
		m_configManager.UpdateGraph(m_selectedGraph, updatedGraph);
		m_logger->LogInfo("Deleted edge: " + m_selectedEdge + " from graph: " + m_selectedGraph);
	}
	catch (const std::exception& e) {
		m_logger->LogError("Failed to delete edge: " + std::string(e.what()));
		return;
	}

	// Clear selection
	m_selectedEdge.clear();

	// Force UI refresh
	RefreshGraphData();

	// Save changes to persist them to disk
	SaveChanges();
}


void MotionConfigEditor::RefreshGraphData() {
	// Clear any cached data that might be stale
	m_selectedNode.clear();
	m_selectedEdge.clear();
	m_isAddingNewNode = false;
	m_isAddingNewEdge = false;

	// Clear any input buffers
	m_nodeIdBuffer[0] = '\0';
	m_nodeLabelBuffer[0] = '\0';
	m_nodeDeviceBuffer[0] = '\0';
	m_nodePositionBuffer[0] = '\0';
	m_edgeIdBuffer[0] = '\0';
	m_edgeLabelBuffer[0] = '\0';
	m_edgeSourceBuffer[0] = '\0';
	m_edgeTargetBuffer[0] = '\0';

	// Force a refresh from the config manager
	// This is crucial to update the display with the current graph data
	m_logger->LogInfo("Refreshing graph data for " + m_selectedGraph);
}

void MotionConfigEditor::UpdateGraph() {
	if (m_selectedGraph.empty()) {
		m_logger->LogError("Cannot update graph: No graph selected");
		return;
	}

	auto graphOpt = m_configManager.GetGraph(m_selectedGraph);
	if (!graphOpt.has_value()) {
		m_logger->LogError("Graph not found: " + m_selectedGraph);
		return;
	}

	// Get the current graph
	Graph updatedGraph = graphOpt.value().get();

	// If we are editing a node, update or add it
	if (!m_selectedNode.empty() || m_isAddingNewNode) {
		// Determine if we're adding or updating
		bool isAdding = true;

		// If updating, remove the old node first
		if (!m_selectedNode.empty()) {
			isAdding = false;

			// Remove the existing node
			updatedGraph.Nodes.erase(
				std::remove_if(updatedGraph.Nodes.begin(), updatedGraph.Nodes.end(),
					[this](const Node& node) { return node.Id == m_selectedNode; }),
				updatedGraph.Nodes.end());
		}

		// Add the updated or new node
		Node nodeToAdd = m_editingNode;
		if (m_isAddingNewNode) {
			nodeToAdd.Id = m_newNodeId;
			nodeToAdd.Label = m_newNodeLabel;
			nodeToAdd.Device = m_newNodeDevice;
			nodeToAdd.Position = m_newNodePosition;
		}

		updatedGraph.Nodes.push_back(nodeToAdd);

		m_logger->LogInfo(isAdding ?
			"Added new node: " + nodeToAdd.Id + " to graph: " + m_selectedGraph :
			"Updated node: " + nodeToAdd.Id + " in graph: " + m_selectedGraph);
	}

	// If we are editing an edge, update or add it
	if (!m_selectedEdge.empty() || m_isAddingNewEdge) {
		// Determine if we're adding or updating
		bool isAdding = true;

		// If updating, remove the old edge first
		if (!m_selectedEdge.empty()) {
			isAdding = false;

			// Remove the existing edge
			updatedGraph.Edges.erase(
				std::remove_if(updatedGraph.Edges.begin(), updatedGraph.Edges.end(),
					[this](const Edge& edge) { return edge.Id == m_selectedEdge; }),
				updatedGraph.Edges.end());
		}

		// Add the updated or new edge
		Edge edgeToAdd = m_editingEdge;
		if (m_isAddingNewEdge) {
			edgeToAdd.Id = m_newEdgeId;
			edgeToAdd.Label = m_newEdgeLabel;
			edgeToAdd.Source = m_newEdgeSource;
			edgeToAdd.Target = m_newEdgeTarget;
		}

		updatedGraph.Edges.push_back(edgeToAdd);

		m_logger->LogInfo(isAdding ?
			"Added new edge: " + edgeToAdd.Id + " to graph: " + m_selectedGraph :
			"Updated edge: " + edgeToAdd.Id + " in graph: " + m_selectedGraph);
	}

	// Update the graph in the config manager
	try {
		m_configManager.UpdateGraph(m_selectedGraph, updatedGraph);
	}
	catch (const std::exception& e) {
		m_logger->LogError("Failed to update graph: " + std::string(e.what()));
		return;
	}
}