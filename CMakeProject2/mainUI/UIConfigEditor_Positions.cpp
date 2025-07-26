// UIConfigEditor_Positions.cpp - Enhanced position management functionality
#include "UIConfigEditor.h"
#include "imgui.h"
#include <cstring>

void UIConfigEditor::RenderPositionsTab() {
    // Set font scale for better readability
    ImGui::SetWindowFontScale(1.50f);

    // Get available content region
    ImVec2 contentRegion = ImGui::GetContentRegionAvail();
    float totalWidth = contentRegion.x;

    // Calculate column widths: 15%, 45%, 40%
    float leftColumnWidth = totalWidth * 0.15f;
    float middleColumnWidth = totalWidth * 0.45f;
    float rightColumnWidth = totalWidth * 0.40f;

    // Left panel - Device selection (15%)
    ImGui::BeginChild("PositionsDeviceList", ImVec2(leftColumnWidth, 0), true);

    ImGui::Text("Select a Device:");
    ImGui::Separator();

    const auto& allDevices = configManager.GetAllDevices();

    for (const auto& [name, device] : allDevices) {
        bool isSelected = (m_selectedDevice == name);

        // Show device status with colored indicator
        ImVec4 color = device.IsEnabled ? ImVec4(0.0f, 0.7f, 0.0f, 1.0f) : ImVec4(0.7f, 0.0f, 0.0f, 1.0f);
        ImGui::TextColored(color, device.IsEnabled ? "● " : "○ ");
        ImGui::SameLine();

        if (ImGui::Selectable(name.c_str(), isSelected)) {
            m_selectedDevice = name;
            m_selectedPosition.clear();
            m_isAddingNewPosition = false;
        }
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // Middle panel - Position list for selected device (45%)
    ImGui::BeginChild("PositionsList", ImVec2(middleColumnWidth, 0), true);

    if (!m_selectedDevice.empty()) {
        ImGui::Text("Positions for %s:", m_selectedDevice.c_str());

        // Action buttons row
        if (ImGui::Button("Add New Position")) {
            m_isAddingNewPosition = true;
            m_newPositionName = "new_position";
            m_editingPosition = PositionStruct();
        }

        ImGui::SameLine();

        // Delete All Positions button with confirmation
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
        if (ImGui::Button("Delete All Positions")) {
            ImGui::OpenPopup("Delete All Positions?");
        }
        ImGui::PopStyleColor(2);

        // Confirmation popup for deleting all positions
        if (ImGui::BeginPopupModal("Delete All Positions?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            auto positionsOpt = configManager.GetDevicePositions(m_selectedDevice);
            int positionCount = 0;

            if (positionsOpt.has_value()) {
                const auto& positions = positionsOpt.value().get();
                positionCount = static_cast<int>(positions.size());
            }

            ImGui::Text("Are you sure you want to delete ALL %d positions", positionCount);
            ImGui::Text("from device '%s'?", m_selectedDevice.c_str());
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "This operation cannot be undone!");
            ImGui::Separator();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Yes, Delete All", ImVec2(140, 0))) {
                DeleteAllPositions();
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor();

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(140, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Separator();

        auto positionsOpt = configManager.GetDevicePositions(m_selectedDevice);
        if (positionsOpt.has_value()) {
            const auto& positions = positionsOpt.value().get();

            if (positions.empty()) {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No positions defined");
            }
            else {
                for (const auto& [name, position] : positions) {
                    bool isSelected = (m_selectedPosition == name);
                    if (ImGui::Selectable(name.c_str(), isSelected)) {
                        m_selectedPosition = name;
                        m_isAddingNewPosition = false;
                        m_editingPosition = position;
                    }
                }
            }
        }
    }
    else {
        ImGui::TextWrapped("Select a device first.");
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // Right panel - Position details (40%)
    ImGui::BeginChild("PositionDetails", ImVec2(rightColumnWidth, 0), true);

    if (!m_selectedDevice.empty()) {
        if (m_isAddingNewPosition) {
            RenderAddNewPositionUI();
        }
        else if (!m_selectedPosition.empty()) {
            RenderEditPositionUI();
        }
        else {
            ImGui::TextWrapped("Select a position or add a new one.");
        }
    }
    else {
        ImGui::TextWrapped("Select a device first.");
    }

    ImGui::EndChild();
}

void UIConfigEditor::RenderAddNewPositionUI() {
    ImGui::Text("Adding New Position for %s", m_selectedDevice.c_str());
    ImGui::Separator();

    char positionNameBuffer[64];
    strncpy_s(positionNameBuffer, sizeof(positionNameBuffer), m_newPositionName.c_str(), _TRUNCATE);
    positionNameBuffer[sizeof(positionNameBuffer) - 1] = '\0';

    if (ImGui::InputText("Position Name", positionNameBuffer, sizeof(positionNameBuffer))) {
        m_newPositionName = positionNameBuffer;
    }

    ImGui::Spacing();

    // Position coordinates with improved layout
    ImGui::Text("Coordinates:");

    // Paste button with better styling
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.9f, 1.0f));
    if (ImGui::Button("Paste from Clipboard")) {
        ProcessClipboardData();
    }
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    // Always show X, Y, Z
    ImGui::DragScalar("X", ImGuiDataType_Double, &m_editingPosition.x, 0.1f, nullptr, nullptr, "%.6f");
    ImGui::DragScalar("Y", ImGuiDataType_Double, &m_editingPosition.y, 0.1f, nullptr, nullptr, "%.6f");
    ImGui::DragScalar("Z", ImGuiDataType_Double, &m_editingPosition.z, 0.1f, nullptr, nullptr, "%.6f");

    // Only show U, V, W for hex devices
    if (m_selectedDevice.find("hex") != std::string::npos) {
        ImGui::Separator();
        ImGui::Text("Rotational Axes:");
        ImGui::DragScalar("U", ImGuiDataType_Double, &m_editingPosition.u, 0.1f, nullptr, nullptr, "%.6f");
        ImGui::DragScalar("V", ImGuiDataType_Double, &m_editingPosition.v, 0.1f, nullptr, nullptr, "%.6f");
        ImGui::DragScalar("W", ImGuiDataType_Double, &m_editingPosition.w, 0.1f, nullptr, nullptr, "%.6f");
    }

    ImGui::Separator();

    // Action buttons with improved styling
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    if (ImGui::Button("Add Position", ImVec2(120, 0))) {
        AddNewPosition();
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        m_isAddingNewPosition = false;
        m_editingPosition = PositionStruct();
    }
}

void UIConfigEditor::RenderEditPositionUI() {
    auto posOpt = configManager.GetNamedPosition(m_selectedDevice, m_selectedPosition);
    if (posOpt.has_value()) {
        ImGui::Text("Editing Position: %s", m_selectedPosition.c_str());
        ImGui::Separator();

        ImGui::Text("Position Name: %s", m_selectedPosition.c_str());
        ImGui::Text("Device: %s", m_selectedDevice.c_str());

        ImGui::Spacing();

        ImGui::Text("Coordinates:");

        // Paste button with better styling
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.9f, 1.0f));
        if (ImGui::Button("Paste from Clipboard")) {
            ProcessClipboardData();
        }
        ImGui::PopStyleColor(2);

        ImGui::Spacing();

        bool changed = false;
        changed |= ImGui::DragScalar("X", ImGuiDataType_Double, &m_editingPosition.x, 0.1f, nullptr, nullptr, "%.6f");
        changed |= ImGui::DragScalar("Y", ImGuiDataType_Double, &m_editingPosition.y, 0.1f, nullptr, nullptr, "%.6f");
        changed |= ImGui::DragScalar("Z", ImGuiDataType_Double, &m_editingPosition.z, 0.1f, nullptr, nullptr, "%.6f");

        // Only show U, V, W for hex devices
        if (m_selectedDevice.find("hex") != std::string::npos) {
            ImGui::Separator();
            ImGui::Text("Rotational Axes:");
            changed |= ImGui::DragScalar("U", ImGuiDataType_Double, &m_editingPosition.u, 0.1f, nullptr, nullptr, "%.6f");
            changed |= ImGui::DragScalar("V", ImGuiDataType_Double, &m_editingPosition.v, 0.1f, nullptr, nullptr, "%.6f");
            changed |= ImGui::DragScalar("W", ImGuiDataType_Double, &m_editingPosition.w, 0.1f, nullptr, nullptr, "%.6f");
        }

        ImGui::Separator();

        // Apply changes immediately with visual feedback
        if (changed) {
            try {
                configManager.AddPosition(m_selectedDevice, m_selectedPosition, m_editingPosition);
                m_logger->LogInfo("Updated position: " + m_selectedPosition + " for device: " + m_selectedDevice);

                // Visual feedback for successful update
                ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "✓ Position updated");
            }
            catch (const std::exception& e) {
                m_logger->LogError("Failed to update position: " + std::string(e.what()));
                ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1.0f), "✗ Update failed");
            }
        }

        ImGui::Spacing();

        // Delete button with improved styling
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Delete Position")) {
            ImGui::OpenPopup("Delete Position?");
        }
        ImGui::PopStyleColor(2);

        // Confirmation dialog with enhanced styling
        if (ImGui::BeginPopupModal("Delete Position?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want to delete position '%s'?", m_selectedPosition.c_str());
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "This operation cannot be undone!");
            ImGui::Separator();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
                DeleteSelectedPosition();
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor();

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}

void UIConfigEditor::DeleteSelectedPosition() {
    if (m_selectedDevice.empty() || m_selectedPosition.empty()) {
        return;
    }

    try {
        bool success = configManager.DeletePosition(m_selectedDevice, m_selectedPosition);
        if (success) {
            m_logger->LogInfo("Position deleted: " + m_selectedPosition + " from device: " + m_selectedDevice);
            m_selectedPosition.clear();
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

void UIConfigEditor::AddNewPosition() {
    if (m_newPositionName.empty() || m_selectedDevice.empty()) {
        m_logger->LogError("Cannot add position: Invalid device or position name");
        return;
    }

    auto positionsOpt = configManager.GetDevicePositions(m_selectedDevice);
    if (positionsOpt.has_value()) {
        const auto& positions = positionsOpt.value().get();
        if (positions.find(m_newPositionName) != positions.end()) {
            m_logger->LogError("Position already exists: " + m_newPositionName);
            return;
        }
    }

    try {
        configManager.AddPosition(m_selectedDevice, m_newPositionName, m_editingPosition);
        m_logger->LogInfo("Added new position: " + m_newPositionName + " to device: " + m_selectedDevice);

        m_selectedPosition = m_newPositionName;
        m_isAddingNewPosition = false;

        SaveChanges();
        RefreshGraphData();
    }
    catch (const std::exception& e) {
        m_logger->LogError("Failed to add position: " + std::string(e.what()));
    }
}

void UIConfigEditor::DeleteAllPositions() {
    if (m_selectedDevice.empty()) {
        m_logger->LogError("Cannot delete positions: No device selected");
        return;
    }

    auto positionsOpt = configManager.GetDevicePositions(m_selectedDevice);
    if (!positionsOpt.has_value()) {
        m_logger->LogWarning("No positions found for device: " + m_selectedDevice);
        return;
    }

    const auto& positions = positionsOpt.value().get();
    if (positions.empty()) {
        m_logger->LogInfo("No positions to delete for device: " + m_selectedDevice);
        return;
    }

    int deletedCount = 0;
    std::vector<std::string> positionNames;

    // Collect all position names first
    for (const auto& [name, position] : positions) {
        positionNames.push_back(name);
    }

    // Delete each position
    for (const auto& positionName : positionNames) {
        try {
            bool success = configManager.DeletePosition(m_selectedDevice, positionName);
            if (success) {
                deletedCount++;
                m_logger->LogInfo("Deleted position: " + positionName + " from device: " + m_selectedDevice);
            }
            else {
                m_logger->LogError("Failed to delete position: " + positionName);
            }
        }
        catch (const std::exception& e) {
            m_logger->LogError("Error deleting position " + positionName + ": " + std::string(e.what()));
        }
    }

    // Clear selection and refresh
    m_selectedPosition.clear();
    m_isAddingNewPosition = false;

    if (deletedCount > 0) {
        m_logger->LogInfo("Successfully deleted " + std::to_string(deletedCount) +
            " positions from device: " + m_selectedDevice);
        SaveChanges();
        RefreshGraphData();
    }
}