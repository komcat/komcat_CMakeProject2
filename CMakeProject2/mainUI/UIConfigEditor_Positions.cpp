// UIConfigEditor_Positions.cpp - Position management functionality
#include "UIConfigEditor.h"
#include "imgui.h"
#include <cstring>

void UIConfigEditor::RenderPositionsTab() {
  // Left panel - Device selection  
  ImGui::BeginChild("PositionsDeviceList", ImVec2(200, 0), true);

  ImGui::Text("Select a Device:");
  ImGui::Separator();

  const auto& allDevices = configManager.GetAllDevices();

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
      m_editingPosition = PositionStruct();
    }

    ImGui::Separator();

    auto positionsOpt = configManager.GetDevicePositions(m_selectedDevice);
    if (positionsOpt.has_value()) {
      const auto& positions = positionsOpt.value().get();

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
  else {
    ImGui::Text("Select a device first.");
  }

  ImGui::EndChild();

  ImGui::SameLine();

  // Right panel - Position details  
  ImGui::BeginChild("PositionDetails", ImVec2(0, 0), true);

  if (!m_selectedDevice.empty()) {
    if (m_isAddingNewPosition) {
      RenderAddNewPositionUI();
    }
    else if (!m_selectedPosition.empty()) {
      RenderEditPositionUI();
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

void UIConfigEditor::RenderAddNewPositionUI() {
  ImGui::Text("Adding New Position for %s", m_selectedDevice.c_str());
  ImGui::Separator();

  char positionNameBuffer[64];
  strncpy_s(positionNameBuffer, sizeof(positionNameBuffer), m_newPositionName.c_str(), _TRUNCATE);
  positionNameBuffer[sizeof(positionNameBuffer) - 1] = '\0';

  if (ImGui::InputText("Position Name", positionNameBuffer, sizeof(positionNameBuffer))) {
    m_newPositionName = positionNameBuffer;
  }

  // Position coordinates  
  ImGui::Text("Coordinates:");

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

  if (ImGui::Button("Add Position")) {
    AddNewPosition();
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel")) {
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

    ImGui::Text("Coordinates:");

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
        configManager.AddPosition(m_selectedDevice, m_selectedPosition, m_editingPosition);
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