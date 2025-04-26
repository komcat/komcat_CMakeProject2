// MotionConfigEditor.cpp
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

    ImGui::End();
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
       m_editingDevice.Id = allDevices.size();  

       std::strncpy(m_ipAddressBuffer, m_editingDevice.IpAddress.c_str(), sizeof(m_ipAddressBuffer) - 1);  
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

   ImGui::BeginChild("DeviceDetails", ImVec2(0, 0), true);  

   if (m_isAddingNewDevice) {  
       ImGui::Text("Adding New Device");  
       ImGui::Separator();  

       // Fix for E0167: Use a char buffer instead of std::string directly  
       char deviceNameBuffer[64];  
       std::strncpy(deviceNameBuffer, m_newDeviceName.c_str(), sizeof(deviceNameBuffer) - 1);  
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
   } else if (!m_selectedDevice.empty()) {  
       auto deviceOpt = m_configManager.GetDevice(m_selectedDevice);  
       if (deviceOpt.has_value()) {  
           const auto& device = deviceOpt.value().get();  

           ImGui::Text("Editing Device: %s", m_selectedDevice.c_str());  
           ImGui::Separator();  

           ImGui::Text("Device Name: %s", device.Name.c_str());  

           if (m_ipAddressBuffer[0] == '\0') {  
               std::strncpy(m_ipAddressBuffer, device.IpAddress.c_str(), sizeof(m_ipAddressBuffer) - 1);  
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
   } else {  
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
           m_editingPosition = Position();  
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
           std::strncpy(positionNameBuffer, m_newPositionName.c_str(), sizeof(positionNameBuffer) - 1);  
           positionNameBuffer[sizeof(positionNameBuffer) - 1] = '\0';  

           if (ImGui::InputText("Position Name", positionNameBuffer, sizeof(positionNameBuffer))) {  
               m_newPositionName = positionNameBuffer;  
           }  

           // Position coordinates  
           ImGui::Text("Coordinates:");  
           ImGui::DragScalar("X", ImGuiDataType_Double, &m_editingPosition.x, 0.1);  
           ImGui::DragScalar("Y", ImGuiDataType_Double, &m_editingPosition.y, 0.1);  
           ImGui::DragScalar("Z", ImGuiDataType_Double, &m_editingPosition.z, 0.1);  
           ImGui::DragScalar("U", ImGuiDataType_Double, &m_editingPosition.u, 0.1);  
           ImGui::DragScalar("V", ImGuiDataType_Double, &m_editingPosition.v, 0.1);  
           ImGui::DragScalar("W", ImGuiDataType_Double, &m_editingPosition.w, 0.1);  

           ImGui::Separator();  

           // Add or Cancel buttons  
           if (ImGui::Button("Add Position")) {  
               AddNewPosition();  
           }  
           ImGui::SameLine();  
           if (ImGui::Button("Cancel")) {  
               m_isAddingNewPosition = false;  
               m_editingPosition = Position();  
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
               bool changed = false;  
               changed |= ImGui::DragScalar("X", ImGuiDataType_Double, &m_editingPosition.x, 0.1);  
               changed |= ImGui::DragScalar("Y", ImGuiDataType_Double, &m_editingPosition.y, 0.1);  
               changed |= ImGui::DragScalar("Z", ImGuiDataType_Double, &m_editingPosition.z, 0.1);  
               changed |= ImGui::DragScalar("U", ImGuiDataType_Double, &m_editingPosition.u, 0.1);  
               changed |= ImGui::DragScalar("V", ImGuiDataType_Double, &m_editingPosition.v, 0.1);  
               changed |= ImGui::DragScalar("W", ImGuiDataType_Double, &m_editingPosition.w, 0.1);  

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
            std::strncpy(m_ipAddressBuffer, device.IpAddress.c_str(), sizeof(m_ipAddressBuffer) - 1);
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

void MotionConfigEditor::RenderNodeList() {
    ImGui::Text("Nodes for %s", m_selectedGraph.c_str());

    if (ImGui::Button("Add New Node")) {
        m_isAddingNewNode = true;
        m_isAddingNewEdge = false;
        m_selectedNode.clear();
        m_selectedEdge.clear();

        // Initialize with default values
        m_editingNode = Node();
        m_newNodeId = "node_" + std::to_string(time(nullptr) % 10000);
        m_newNodeLabel = "New Node";
        m_newNodeDevice = "";
        m_newNodePosition = "";

        // Update buffers
        std::strncpy(m_nodeIdBuffer, m_newNodeId.c_str(), sizeof(m_nodeIdBuffer) - 1);
        std::strncpy(m_nodeLabelBuffer, m_newNodeLabel.c_str(), sizeof(m_nodeLabelBuffer) - 1);
        std::strncpy(m_nodeDeviceBuffer, m_newNodeDevice.c_str(), sizeof(m_nodeDeviceBuffer) - 1);
        std::strncpy(m_nodePositionBuffer, m_newNodePosition.c_str(), sizeof(m_nodePositionBuffer) - 1);
    }

    ImGui::Separator();

    auto graphOpt = m_configManager.GetGraph(m_selectedGraph);
    if (graphOpt.has_value()) {
        const auto& graph = graphOpt.value().get();

        for (const auto& node : graph.Nodes) {
            bool isSelected = (m_selectedNode == node.Id);

            // Create a more informative label with device and position
            std::string displayLabel = node.Id;
            if (!node.Device.empty() && !node.Position.empty()) {
                displayLabel += " (" + node.Device + "." + node.Position + ")";
            }

            if (ImGui::Selectable(displayLabel.c_str(), isSelected)) {
                m_selectedNode = node.Id;
                m_selectedEdge.clear();
                m_isAddingNewNode = false;
                m_isAddingNewEdge = false;

                // Copy node data
                m_editingNode = node;

                // Update buffers
                std::strncpy(m_nodeIdBuffer, node.Id.c_str(), sizeof(m_nodeIdBuffer) - 1);
                std::strncpy(m_nodeLabelBuffer, node.Label.c_str(), sizeof(m_nodeLabelBuffer) - 1);
                std::strncpy(m_nodeDeviceBuffer, node.Device.c_str(), sizeof(m_nodeDeviceBuffer) - 1);
                std::strncpy(m_nodePositionBuffer, node.Position.c_str(), sizeof(m_nodePositionBuffer) - 1);
            }
        }
    }
}

void MotionConfigEditor::RenderEdgeList() {
    ImGui::Text("Edges for %s", m_selectedGraph.c_str());

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
        std::strncpy(m_edgeIdBuffer, m_newEdgeId.c_str(), sizeof(m_edgeIdBuffer) - 1);
        std::strncpy(m_edgeLabelBuffer, m_newEdgeLabel.c_str(), sizeof(m_edgeLabelBuffer) - 1);
        std::strncpy(m_edgeSourceBuffer, m_newEdgeSource.c_str(), sizeof(m_edgeSourceBuffer) - 1);
        std::strncpy(m_edgeTargetBuffer, m_newEdgeTarget.c_str(), sizeof(m_edgeTargetBuffer) - 1);
    }

    ImGui::Separator();

    auto graphOpt = m_configManager.GetGraph(m_selectedGraph);
    if (graphOpt.has_value()) {
        const auto& graph = graphOpt.value().get();

        for (const auto& edge : graph.Edges) {
            bool isSelected = (m_selectedEdge == edge.Id);

            // Create a more informative label that shows if it's bidirectional
            std::string directionSymbol = edge.Conditions.IsBidirectional ? " <-> " : " -> ";
            std::string label = edge.Id + " (" + edge.Source + directionSymbol + edge.Target + ")";

            if (ImGui::Selectable(label.c_str(), isSelected)) {
                m_selectedEdge = edge.Id;
                m_selectedNode.clear();
                m_isAddingNewNode = false;
                m_isAddingNewEdge = false;

                // Copy edge data
                m_editingEdge = edge;

                // Update buffers
                std::strncpy(m_edgeIdBuffer, edge.Id.c_str(), sizeof(m_edgeIdBuffer) - 1);
                std::strncpy(m_edgeLabelBuffer, edge.Label.c_str(), sizeof(m_edgeLabelBuffer) - 1);
                std::strncpy(m_edgeSourceBuffer, edge.Source.c_str(), sizeof(m_edgeSourceBuffer) - 1);
                std::strncpy(m_edgeTargetBuffer, edge.Target.c_str(), sizeof(m_edgeTargetBuffer) - 1);
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
                std::strncpy(m_nodeDeviceBuffer, deviceName.c_str(), sizeof(m_nodeDeviceBuffer) - 1);
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
                        std::strncpy(m_nodePositionBuffer, posName.c_str(), sizeof(m_nodePositionBuffer) - 1);

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
                    std::strncpy(m_edgeSourceBuffer, node.Id.c_str(), sizeof(m_edgeSourceBuffer) - 1);
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
                    std::strncpy(m_edgeTargetBuffer, node.Id.c_str(), sizeof(m_edgeTargetBuffer) - 1);
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