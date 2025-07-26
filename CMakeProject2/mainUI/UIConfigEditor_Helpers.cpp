// UIConfigEditor_Helpers.cpp - Enhanced helper methods and clipboard functionality
#include "UIConfigEditor.h"
#include "imgui.h"

void UIConfigEditor::ProcessClipboardData() {
    if (m_selectedDevice.empty()) {
        m_logger->LogError("No device selected");
        return;
    }

    std::string clipboardText = ImGui::GetClipboardText();
    m_logger->LogInfo("Clipboard content: " + clipboardText);

    if (clipboardText.empty()) {
        m_logger->LogError("Clipboard is empty");
        return;
    }

    try {
        json clipboardJson = json::parse(clipboardText);
        m_logger->LogInfo("Successfully parsed JSON from clipboard");

        if (!clipboardJson.contains("device") || !clipboardJson.contains("positions")) {
            m_logger->LogError("Invalid clipboard format: missing 'device' or 'positions'");
            return;
        }

        std::string deviceName = clipboardJson["device"];

        if (deviceName != m_selectedDevice) {
            m_logger->LogWarning("Device in clipboard (" + deviceName +
                ") doesn't match selected device (" + m_selectedDevice + ")");
        }

        m_oldPosition = m_editingPosition;
        m_newPosition = m_oldPosition;

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

void UIConfigEditor::RenderClipboardConfirmationPopup() {
    // Set font scale for consistency
    ImGui::SetWindowFontScale(1.50f);

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_Appearing);

    bool isOpen = true;
    if (ImGui::BeginPopupModal("Confirm Position Update", &isOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Update position values from clipboard?");
        ImGui::Separator();

        ImGui::Text("Device: %s", m_selectedDevice.c_str());
        ImGui::Text("Position: %s",
            m_isAddingNewPosition ? m_newPositionName.c_str() : m_selectedPosition.c_str());

        ImGui::Spacing();

        if (ImGui::BeginTable("PositionValuesTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Axis", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableSetupColumn("Current Value", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn("New Value", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn("Difference", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableHeadersRow();

            ImVec4 changedColor = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
            ImVec4 unchangedColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

            auto addRow = [&](const char* axis, double oldVal, double newVal) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", axis);

                ImGui::TableNextColumn();
                ImGui::Text("%.6f", oldVal);

                ImGui::TableNextColumn();
                ImGui::Text("%.6f", newVal);

                float diff = static_cast<float>(newVal - oldVal);
                ImGui::TableNextColumn();
                if (std::abs(diff) > 0.000001) {
                    ImGui::TextColored(changedColor, "%+.6f", diff);
                }
                else {
                    ImGui::TextColored(unchangedColor, "%.6f", diff);
                }
                };

            addRow("X", m_oldPosition.x, m_newPosition.x);
            addRow("Y", m_oldPosition.y, m_newPosition.y);
            addRow("Z", m_oldPosition.z, m_newPosition.z);

            // Only show U, V, W for hex devices
            if (m_selectedDevice.find("hex") != std::string::npos) {
                addRow("U", m_oldPosition.u, m_newPosition.u);
                addRow("V", m_oldPosition.v, m_newPosition.v);
                addRow("W", m_oldPosition.w, m_newPosition.w);
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Center the buttons
        float windowWidth = ImGui::GetWindowSize().x;
        float buttonWidth = 120.0f;
        float totalButtonWidth = buttonWidth * 2 + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCursorPosX((windowWidth - totalButtonWidth) * 0.5f);

        // Confirm button with green styling
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
        if (ImGui::Button("Confirm", ImVec2(buttonWidth, 0))) {
            m_editingPosition = m_newPosition;

            if (!m_isAddingNewPosition && !m_selectedPosition.empty()) {
                try {
                    configManager.AddPosition(m_selectedDevice, m_selectedPosition, m_editingPosition);
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
        ImGui::PopStyleColor(2);

        ImGui::SameLine();

        // Cancel button with default styling
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
            m_showClipboardConfirmation = false;
            ImGui::CloseCurrentPopup();
        }

        if (!isOpen) {
            m_showClipboardConfirmation = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    else if (m_showClipboardConfirmation) {
        ImGui::OpenPopup("Confirm Position Update");
    }
}