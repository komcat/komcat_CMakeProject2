// UIConfigEditor_Devices.cpp - Enhanced device management functionality
#include "UIConfigEditor.h"
#include "imgui.h"
#include <cstring>

void UIConfigEditor::RenderDevicesTab() {
    // Set font scale for better readability
    ImGui::SetWindowFontScale(1.50f);

    // Get available content region
    ImVec2 contentRegion = ImGui::GetContentRegionAvail();
    float totalWidth = contentRegion.x;

    // Calculate column widths: 15%, 85% (2 columns for devices)
    float leftColumnWidth = totalWidth * 0.30f;  // Device list gets 30%
    float rightColumnWidth = totalWidth * 0.70f; // Device details gets 70%

    const auto& allDevices = configManager.GetAllDevices();

    ImGui::BeginChild("DevicesList", ImVec2(leftColumnWidth, 0), true);

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
            m_logger->LogError("Device ID exceeds maximum integer value");
            m_editingDevice.Id = 0;
        }
        strncpy_s(m_ipAddressBuffer, sizeof(m_ipAddressBuffer), m_editingDevice.IpAddress.c_str(), _TRUNCATE);
    }

    ImGui::Separator();

    for (const auto& [name, device] : allDevices) {
        bool isSelected = (m_selectedDevice == name);

        ImVec4 color = device.IsEnabled ? ImVec4(0.0f, 0.7f, 0.0f, 1.0f) : ImVec4(0.7f, 0.0f, 0.0f, 1.0f);
        ImGui::TextColored(color, device.IsEnabled ? "● " : "○ ");
        ImGui::SameLine();

        if (ImGui::Selectable(name.c_str(), isSelected)) {
            m_selectedDevice = name;
            m_isAddingNewDevice = false;
            RefreshDeviceData();
        }
    }

    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("DeviceDetails", ImVec2(rightColumnWidth, 0), true);

    if (m_isAddingNewDevice) {
        ImGui::Text("Adding New Device");
        ImGui::Separator();

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
        auto deviceOpt = configManager.GetDevice(m_selectedDevice);
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
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "This operation cannot be undone!");
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
        ImGui::TextWrapped("Select a device from the list or add a new one.");
    }

    ImGui::EndChild();
}

void UIConfigEditor::RefreshDeviceData() {
    if (!m_selectedDevice.empty()) {
        auto deviceOpt = configManager.GetDevice(m_selectedDevice);
        if (deviceOpt.has_value()) {
            const auto& device = deviceOpt.value().get();

            m_editingDevice.IsEnabled = device.IsEnabled;
            m_editingDevice.IpAddress = device.IpAddress;
            m_editingDevice.Port = device.Port;
            m_editingDevice.Id = device.Id;
            m_editingDevice.Name = device.Name;

            strncpy_s(m_ipAddressBuffer, sizeof(m_ipAddressBuffer), device.IpAddress.c_str(), _TRUNCATE);
            m_ipAddressBuffer[sizeof(m_ipAddressBuffer) - 1] = '\0';
        }
    }
}

void UIConfigEditor::DeleteSelectedDevice() {
    if (m_selectedDevice.empty()) {
        return;
    }

    try {
        bool success = configManager.DeleteDevice(m_selectedDevice);
        if (success) {
            m_logger->LogInfo("Device deleted: " + m_selectedDevice);
            m_selectedDevice.clear();
        }
        else {
            m_logger->LogError("Failed to delete device: " + m_selectedDevice);
        }
    }
    catch (const std::exception& e) {
        m_logger->LogError("Error deleting device: " + std::string(e.what()));
    }
}

void UIConfigEditor::AddNewDevice() {
    if (m_newDeviceName.empty()) {
        m_logger->LogError("Cannot add device with empty name");
        return;
    }

    const auto& allDevices = configManager.GetAllDevices();
    if (allDevices.find(m_newDeviceName) != allDevices.end()) {
        m_logger->LogError("Device already exists: " + m_newDeviceName);
        return;
    }

    m_logger->LogInfo("Adding new device: " + m_newDeviceName);

    m_editingDevice.Name = m_newDeviceName;

    try {
        configManager.AddDevice(m_newDeviceName, m_editingDevice);
        m_selectedDevice = m_newDeviceName;
        m_logger->LogInfo("Device added successfully: " + m_newDeviceName);
    }
    catch (const std::exception& e) {
        m_logger->LogError("Failed to add device: " + std::string(e.what()));
    }

    m_isAddingNewDevice = false;
    m_newDeviceName.clear();
    std::memset(m_ipAddressBuffer, 0, sizeof(m_ipAddressBuffer));
}