// JsonEditorUI.cpp
#include "JsonEditorUI.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

JsonEditorUI::JsonEditorUI() {
  ScanForJsonFiles();
}


void JsonEditorUI::Render() {
  if (!m_visible) return;

  // Main window
  ImGui::SetNextWindowSize(ImVec2(1200, 700), ImGuiCond_FirstUseEver);

  if (ImGui::Begin(m_name.c_str(), &m_visible)) {
    // Status message (always reserve space to prevent UI jumping)
    if (m_statusMessageTimer > 0) {
      ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%s", m_statusMessage.c_str());
      m_statusMessageTimer -= ImGui::GetIO().DeltaTime;
    }
    else {
      ImGui::Text(" "); // Empty line to maintain consistent spacing
    }

    // Left panel for file list
    ImGui::BeginChild("FileListPanel", ImVec2(m_leftPanelWidth, 0), true);
    RenderFileList();
    ImGui::EndChild();

    ImGui::SameLine();

    // Splitter
    ImGui::Button("||", ImVec2(8, -1));
    if (ImGui::IsItemActive()) {
      m_leftPanelWidth += ImGui::GetIO().MouseDelta.x;
      if (m_leftPanelWidth < 150.0f) m_leftPanelWidth = 150.0f;
      if (m_leftPanelWidth > 400.0f) m_leftPanelWidth = 400.0f;
    }

    ImGui::SameLine();

    // Right panel for editor
    ImGui::BeginGroup();

    // Buttons row
    RenderButtons();

    ImGui::Separator();

    // Editor area
    ImGui::BeginChild("EditorPanel", ImVec2(0, 0), true);
    RenderEditor();
    ImGui::EndChild();

    ImGui::EndGroup();
  }
  ImGui::End();
}


void JsonEditorUI::RenderFileList() {
  ImGui::Text("JSON Files");
  ImGui::Separator();

  // Refresh button
  if (ImGui::Button("Refresh", ImVec2(-1, 0))) {
    ScanForJsonFiles();
  }

  // New file button
  if (ImGui::Button("New File", ImVec2(-1, 0))) {
    CreateNewFile();
  }

  ImGui::Separator();

  // File list
  for (size_t i = 0; i < m_jsonFiles.size(); i++) {
    bool isSelected = (static_cast<int>(i) == m_selectedFileIndex);

    // Extract just filename from path
    fs::path p(m_jsonFiles[i]);
    std::string filename = p.filename().string();

    if (ImGui::Selectable(filename.c_str(), isSelected)) {
      // Check if current content is modified
      if (m_contentModified) {
        // In production, you'd want a confirmation dialog here
        // For simplicity, we'll just save automatically
        SaveJsonFile();
      }

      m_selectedFileIndex = static_cast<int>(i);
      LoadJsonFile(m_jsonFiles[i]);
    }

    // Tooltip with full path
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", m_jsonFiles[i].c_str());
    }
  }
}

void JsonEditorUI::RenderEditor() {
  if (m_selectedFileIndex < 0) {
    ImGui::Text("Select a JSON file from the list or create a new one");
    return;
  }

  // File info
  ImGui::Text("Editing: %s", m_currentFilePath.c_str());
  if (m_contentModified) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(Modified)");
  }
  ImGui::Separator();

  // Text editor
  static char editBuffer[1024 * 64]; // 64KB buffer

  // Copy content to buffer on file load (one-time)
  static int lastSelectedIndex = -1;
  if (lastSelectedIndex != m_selectedFileIndex) {
    strncpy(editBuffer, m_editorContent.c_str(), sizeof(editBuffer) - 1);
    editBuffer[sizeof(editBuffer) - 1] = '\0';
    lastSelectedIndex = m_selectedFileIndex;
  }

  // Multi-line text input
  ImVec2 availSize = ImGui::GetContentRegionAvail();
  if (ImGui::InputTextMultiline("##editor", editBuffer, sizeof(editBuffer),
    availSize,
    ImGuiInputTextFlags_AllowTabInput)) {
    m_editorContent = editBuffer;
    m_contentModified = true;
  }
}

void JsonEditorUI::RenderButtons() {
  // Load button
  if (ImGui::Button("Load")) {
    if (m_selectedFileIndex >= 0) {
      LoadJsonFile(m_jsonFiles[m_selectedFileIndex]);
      SetStatusMessage("File loaded");
    }
  }

  ImGui::SameLine();

  // Save button
  if (ImGui::Button("Save")) {
    SaveJsonFile();
  }

  ImGui::SameLine();

  // Save As button
  if (ImGui::Button("Save As...")) {
    SaveJsonFileAs();
  }

  ImGui::SameLine();

  // Format JSON button (optional - would need JSON parser)
  if (ImGui::Button("Format")) {
    // In production, you'd parse and pretty-print the JSON
    SetStatusMessage("Format feature not yet implemented");
  }

  ImGui::SameLine();

  // Validate button (optional - would need JSON parser)
  if (ImGui::Button("Validate")) {
    // In production, you'd validate the JSON syntax
    SetStatusMessage("Validation feature not yet implemented");
  }
}

void JsonEditorUI::ScanForJsonFiles() {
  m_jsonFiles.clear();

  try {
    // Scan root application directory
    for (const auto& entry : fs::directory_iterator(".")) {
      if (entry.is_regular_file()) {
        std::string path = entry.path().string();
        std::string extension = entry.path().extension().string();

        // Check for .json extension (case insensitive)
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        if (extension == ".json") {
          m_jsonFiles.push_back(path);
        }
      }
    }

    // Sort files alphabetically
    std::sort(m_jsonFiles.begin(), m_jsonFiles.end());

  }
  catch (const std::exception& e) {
    std::cerr << "Error scanning for JSON files: " << e.what() << std::endl;
  }
}

void JsonEditorUI::LoadJsonFile(const std::string& filepath) {
  try {
    std::ifstream file(filepath);
    if (!file.is_open()) {
      SetStatusMessage("Failed to open file: " + filepath);
      return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    m_editorContent = buffer.str();
    m_currentFilePath = filepath;
    m_contentModified = false;

    SetStatusMessage("Loaded: " + filepath);

  }
  catch (const std::exception& e) {
    SetStatusMessage(std::string("Error loading file: ") + e.what());
  }
}

void JsonEditorUI::SaveJsonFile() {
  if (m_currentFilePath.empty()) {
    SaveJsonFileAs();
    return;
  }

  try {
    std::ofstream file(m_currentFilePath);
    if (!file.is_open()) {
      SetStatusMessage("Failed to save file");
      return;
    }

    file << m_editorContent;
    file.close();

    m_contentModified = false;
    SetStatusMessage("Saved: " + m_currentFilePath);

  }
  catch (const std::exception& e) {
    SetStatusMessage(std::string("Error saving file: ") + e.what());
  }
}

void JsonEditorUI::SaveJsonFileAs() {
  // In production, you'd use a file dialog here
  // For now, we'll create a simple input for filename
  static char filename[256] = "new_file.json";

  ImGui::OpenPopup("Save As");
  if (ImGui::BeginPopupModal("Save As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Enter filename:");
    ImGui::InputText("##filename", filename, sizeof(filename));

    if (ImGui::Button("Save")) {
      m_currentFilePath = filename;
      SaveJsonFile();
      ScanForJsonFiles(); // Refresh file list

      // Find and select the new file
      auto it = std::find(m_jsonFiles.begin(), m_jsonFiles.end(), m_currentFilePath);
      if (it != m_jsonFiles.end()) {
        m_selectedFileIndex = static_cast<int>(std::distance(m_jsonFiles.begin(), it));
      }

      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

void JsonEditorUI::CreateNewFile() {
  // Reset editor with default JSON content
  m_editorContent = "{\n    \"name\": \"value\"\n}";
  m_currentFilePath = "";
  m_contentModified = true;
  m_selectedFileIndex = -1;

  SetStatusMessage("New file created - use Save As to save");
}

void JsonEditorUI::SetStatusMessage(const std::string& msg) {
  m_statusMessage = msg;
  m_statusMessageTimer = 3.0f; // Show for 3 seconds
}