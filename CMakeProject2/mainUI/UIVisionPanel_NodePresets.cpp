// =============================================================================
// UIVisionPanel_NodePresets.cpp - Fixed Database Implementation with Delete
// =============================================================================

#include "UIVisionPanel.h"
#include <sqlite3.h>
#include <filesystem>
#include <fstream>
#include <iostream>

// Initialize the node-preset table in database
void UIVisionPanel::CreateNodePresetTable() {
  // Open the same database file that VisionPresetManager uses
  std::string dbPath = "vision_presets.db";

  sqlite3* db = nullptr;
  int result = sqlite3_open(dbPath.c_str(), &db);

  if (result != SQLITE_OK) {
    std::cerr << "[UIVisionPanel] Cannot open database: " << sqlite3_errmsg(db) << std::endl;
    if (db) {
      sqlite3_close(db);
    }
    return;
  }

  const char* createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS node_preset_mappings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            node_id TEXT NOT NULL UNIQUE,
            preset_id INTEGER NOT NULL,
            preset_name TEXT NOT NULL,
            guidance_image_path TEXT,
            auto_load INTEGER DEFAULT 1,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (preset_id) REFERENCES vision_presets (id) ON DELETE CASCADE
        );
    )";

  char* errorMessage = nullptr;
  result = sqlite3_exec(db, createTableSQL, nullptr, nullptr, &errorMessage);

  if (result != SQLITE_OK) {
    std::cerr << "[UIVisionPanel] Failed to create node_preset_mappings table: "
      << (errorMessage ? errorMessage : "Unknown error") << std::endl;
    if (errorMessage) {
      sqlite3_free(errorMessage);
    }
  }
  else {
    std::cout << "[UIVisionPanel] Node preset mappings table ready" << std::endl;
  }

  sqlite3_close(db);
}

// Load node-preset mappings from database
void UIVisionPanel::LoadNodePresetMappings() {
  std::string dbPath = "vision_presets.db";
  sqlite3* db = nullptr;

  int result = sqlite3_open(dbPath.c_str(), &db);
  if (result != SQLITE_OK) {
    std::cerr << "[UIVisionPanel] Cannot open database for loading mappings: "
      << sqlite3_errmsg(db) << std::endl;
    if (db) sqlite3_close(db);
    return;
  }

  m_nodePresetMappings.clear();
  m_nodeToPresetMap.clear();

  const char* selectSQL = R"(
        SELECT npm.node_id, npm.preset_id, npm.preset_name, 
               npm.guidance_image_path, npm.auto_load
        FROM node_preset_mappings npm
        ORDER BY npm.node_id;
    )";

  sqlite3_stmt* stmt;
  result = sqlite3_prepare_v2(db, selectSQL, -1, &stmt, nullptr);

  if (result != SQLITE_OK) {
    std::cerr << "[UIVisionPanel] Failed to prepare select statement: "
      << sqlite3_errmsg(db) << std::endl;
    sqlite3_close(db);
    return;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    NodePresetMapping mapping;
    mapping.nodeId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    mapping.presetId = sqlite3_column_int(stmt, 1);
    mapping.presetName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

    const char* imagePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    mapping.guidanceImagePath = imagePath ? imagePath : "";

    mapping.autoLoad = sqlite3_column_int(stmt, 4) == 1;

    m_nodePresetMappings.push_back(mapping);
    m_nodeToPresetMap[mapping.nodeId] = mapping.presetId;
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  std::cout << "[UIVisionPanel] Loaded " << m_nodePresetMappings.size()
    << " node-preset mappings from database" << std::endl;
}

// Save node-preset mapping to database
bool UIVisionPanel::AssignPresetToNode(const std::string& nodeId, int presetId) {
  std::string dbPath = "vision_presets.db";
  sqlite3* db = nullptr;

  int result = sqlite3_open(dbPath.c_str(), &db);
  if (result != SQLITE_OK) {
    std::cerr << "[UIVisionPanel] Cannot open database for assignment: "
      << sqlite3_errmsg(db) << std::endl;
    if (db) sqlite3_close(db);
    return false;
  }

  // Get preset name
  std::string presetName = "Unknown";
  for (const auto& preset : m_availablePresets) {
    if (preset.id == presetId) {
      presetName = preset.name;
      break;
    }
  }

  const char* insertSQL = R"(
        INSERT OR REPLACE INTO node_preset_mappings 
        (node_id, preset_id, preset_name, updated_at) 
        VALUES (?, ?, ?, CURRENT_TIMESTAMP);
    )";

  sqlite3_stmt* stmt;
  result = sqlite3_prepare_v2(db, insertSQL, -1, &stmt, nullptr);

  if (result != SQLITE_OK) {
    std::cerr << "[UIVisionPanel] Failed to prepare insert statement: "
      << sqlite3_errmsg(db) << std::endl;
    sqlite3_close(db);
    return false;
  }

  sqlite3_bind_text(stmt, 1, nodeId.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, presetId);
  sqlite3_bind_text(stmt, 3, presetName.c_str(), -1, SQLITE_STATIC);

  result = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  if (result == SQLITE_DONE) {
    std::cout << "[UIVisionPanel] Assigned preset '" << presetName
      << "' (ID: " << presetId << ") to node '" << nodeId << "'" << std::endl;

    // Update in-memory mappings
    LoadNodePresetMappings();
    return true;
  }
  else {
    std::cerr << "[UIVisionPanel] Failed to assign preset to node: "
      << sqlite3_errmsg(db) << std::endl;
    return false;
  }
}

// Delete node-preset mapping from database
bool UIVisionPanel::DeleteNodePresetMapping(const std::string& nodeId) {
  std::string dbPath = "vision_presets.db";
  sqlite3* db = nullptr;

  int result = sqlite3_open(dbPath.c_str(), &db);
  if (result != SQLITE_OK) {
    std::cerr << "[UIVisionPanel] Cannot open database for deletion: "
      << sqlite3_errmsg(db) << std::endl;
    if (db) sqlite3_close(db);
    return false;
  }

  const char* deleteSQL = "DELETE FROM node_preset_mappings WHERE node_id = ?;";

  sqlite3_stmt* stmt;
  result = sqlite3_prepare_v2(db, deleteSQL, -1, &stmt, nullptr);

  if (result != SQLITE_OK) {
    std::cerr << "[UIVisionPanel] Failed to prepare delete statement: "
      << sqlite3_errmsg(db) << std::endl;
    sqlite3_close(db);
    return false;
  }

  sqlite3_bind_text(stmt, 1, nodeId.c_str(), -1, SQLITE_STATIC);

  result = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  if (result == SQLITE_DONE) {
    std::cout << "[UIVisionPanel] Deleted mapping for node '" << nodeId << "'" << std::endl;

    // Update in-memory mappings
    LoadNodePresetMappings();
    return true;
  }
  else {
    std::cerr << "[UIVisionPanel] Failed to delete mapping for node '" << nodeId << "'" << std::endl;
    return false;
  }
}

// Node-Preset Mappings Dialog
void UIVisionPanel::RenderNodePresetDialog() {
  if (m_showNodePresetDialog) {
    ImGui::OpenPopup("Node-Preset Mappings");
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(700, 500));

  if (ImGui::BeginPopupModal("Node-Preset Mappings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("All Node-Preset Associations");
    ImGui::Separator();

    if (m_nodePresetMappings.empty()) {
      ImGui::Text("No node-preset mappings configured yet.");
      ImGui::Text("Navigate to nodes and assign presets to create mappings.");
    }
    else {
      // Table of mappings
      if (ImGui::BeginTable("NodePresetTable", 4,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {

        ImGui::TableSetupColumn("Node ID", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Preset", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Guide Image", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < m_nodePresetMappings.size(); i++) {
          const auto& mapping = m_nodePresetMappings[i];
          ImGui::TableNextRow();

          // Node ID
          ImGui::TableNextColumn();
          if (mapping.nodeId == m_selectedNodeId) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", mapping.nodeId.c_str());
          }
          else {
            ImGui::Text("%s", mapping.nodeId.c_str());
          }

          // Preset name
          ImGui::TableNextColumn();
          ImGui::Text("%s", mapping.presetName.c_str());

          // Guidance image indicator
          ImGui::TableNextColumn();
          if (!mapping.guidanceImagePath.empty()) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "📷 Yes");
          }
          else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No");
          }

          // Actions
          ImGui::TableNextColumn();
          if (ImGui::Button(("Go##" + std::to_string(i)).c_str(), ImVec2(40, 20))) {
            NavigateToNode(mapping.nodeId);
            m_showNodePresetDialog = false;
            ImGui::CloseCurrentPopup();
          }
          ImGui::SameLine();
          if (ImGui::Button(("Del##" + std::to_string(i)).c_str(), ImVec2(40, 20))) {
            // Delete the mapping
            if (DeleteNodePresetMapping(mapping.nodeId)) {
              std::cout << "[UIVisionPanel] Successfully deleted mapping for node: " << mapping.nodeId << std::endl;
              // Break out of the loop since we modified the vector
              break;
            }
            else {
              std::cout << "[UIVisionPanel] Failed to delete mapping for node: " << mapping.nodeId << std::endl;
            }
          }
        }
        ImGui::EndTable();
      }
    }

    ImGui::Spacing();
    if (ImGui::Button("Close", ImVec2(120, 0))) {
      m_showNodePresetDialog = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

// Node-Preset Controls UI
void UIVisionPanel::RenderNodePresetControls() {
  ImGui::Text("Node-Preset Association");
  ImGui::Separator();

  // Show current node info
  if (!m_selectedNodeId.empty()) {
    ImGui::Text("Current Node: %s", m_selectedNodeId.c_str());

    // Check if node has assigned preset
    auto it = m_nodeToPresetMap.find(m_selectedNodeId);
    if (it != m_nodeToPresetMap.end()) {
      // Find preset name
      std::string presetName = "Unknown";
      for (const auto& preset : m_availablePresets) {
        if (preset.id == it->second) {
          presetName = preset.name;
          break;
        }
      }
      ImGui::TextColored(ImVec4(0, 1, 0, 1), "Assigned: %s", presetName.c_str());

      // Show guidance image if exists
      for (const auto& mapping : m_nodePresetMappings) {
        if (mapping.nodeId == m_selectedNodeId && !mapping.guidanceImagePath.empty()) {
          ImGui::Text("📷 Guidance image saved");
          break;
        }
      }
    }
    else {
      ImGui::TextColored(ImVec4(1, 1, 0, 1), "No preset assigned");
    }

    ImGui::Spacing();

    // Assign current preset to node
    if (m_selectedPresetId >= 0) {
      std::string buttonText = "Assign Current Preset to Node";
      if (ImGui::Button(buttonText.c_str(), ImVec2(-1, 25))) {
        if (AssignPresetToNode(m_selectedNodeId, m_selectedPresetId)) {
          std::cout << "[UIVisionPanel] Successfully assigned preset to node" << std::endl;
        }
      }

      // Save guidance image
      if (m_hasImageData) {
        if (ImGui::Button("Save Current Image as Guide", ImVec2(-1, 25))) {
          SaveGuidanceImageForNode(m_selectedNodeId);
        }
      }
    }
    else {
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Select a preset first");
    }
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Navigate to a node first");
  }

  ImGui::Spacing();

  // Node-Preset mappings summary
  if (!m_nodePresetMappings.empty()) {
    if (ImGui::Button("View All Mappings", ImVec2(-1, 25))) {
      m_showNodePresetDialog = true;
    }
    ImGui::Text("Total mappings: %zu", m_nodePresetMappings.size());
  }
}