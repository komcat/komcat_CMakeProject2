// ProgramManager.cpp
#include "ProgramManager.h"
#include <fstream>
#include <iostream>
#include <imgui.h>

ProgramManager::ProgramManager() {
  CreateProgramsDirectory();
  RefreshProgramList();
}

void ProgramManager::CreateProgramsDirectory() {
  if (!std::filesystem::exists(m_programsDirectory)) {
    std::filesystem::create_directory(m_programsDirectory);
    printf("[INFO] Created programs directory: %s\n", m_programsDirectory.c_str());
  }
}

void ProgramManager::RefreshProgramList() {
  m_programList.clear();

  try {
    for (const auto& entry : std::filesystem::directory_iterator(m_programsDirectory)) {
      if (entry.path().extension() == ".json") {
        ProgramInfo info = ExtractProgramInfo(entry.path().string());
        m_programList.push_back(info);
      }
    }

    // Sort by last modified (newest first)
    std::sort(m_programList.begin(), m_programList.end(),
      [](const ProgramInfo& a, const ProgramInfo& b) {
      return a.lastModified > b.lastModified;
    });

    printf("[INFO] Found %zu programs\n", m_programList.size());
  }
  catch (const std::exception& e) {
    printf("[ERROR] Error scanning programs directory: %s\n", e.what());
  }
}

bool ProgramManager::SaveProgram(const std::string& filename, const nlohmann::json& programData) {
  try {
    std::string filepath = m_programsDirectory + filename + ".json";
    std::ofstream outFile(filepath);
    outFile << programData.dump(2);
    outFile.close();

    m_currentProgram = filename;
    RefreshProgramList();

    printf("[SAVE] Saved program: %s\n", filename.c_str());
    return true;
  }
  catch (const std::exception& e) {
    printf("[ERROR] Error saving program %s: %s\n", filename.c_str(), e.what());
    return false;
  }
}

bool ProgramManager::LoadProgram(const std::string& filename, nlohmann::json& programData) {
  try {
    std::string filepath = m_programsDirectory + filename + ".json";
    std::ifstream inFile(filepath);

    if (!inFile.is_open()) {
      printf("[ERROR] Could not open program: %s\n", filename.c_str());
      return false;
    }

    inFile >> programData;
    inFile.close();

    m_currentProgram = filename;
    printf("[LOAD] Loaded program: %s\n", filename.c_str());
    return true;
  }
  catch (const std::exception& e) {
    printf("[ERROR] Error loading program %s: %s\n", filename.c_str(), e.what());
    return false;
  }
}

bool ProgramManager::DeleteProgram(const std::string& filename) {
  try {
    std::string filepath = m_programsDirectory + filename + ".json";

    if (std::filesystem::remove(filepath)) {
      RefreshProgramList();
      printf("[DELETE] Deleted program: %s\n", filename.c_str());
      return true;
    }
    return false;
  }
  catch (const std::exception& e) {
    printf("[ERROR] Error deleting program %s: %s\n", filename.c_str(), e.what());
    return false;
  }
}

bool ProgramManager::DuplicateProgram(const std::string& sourceFile, const std::string& newName) {
  try {
    nlohmann::json programData;
    if (LoadProgram(sourceFile, programData)) {
      // Update program metadata
      if (programData.contains("blocks")) {
        for (auto& block : programData["blocks"]) {
          if (block["type"] == "START" && block.contains("parameters")) {
            for (auto& param : block["parameters"]) {
              if (param["name"] == "program_name") {
                param["value"] = newName;
              }
            }
          }
        }
      }

      return SaveProgram(newName, programData);
    }
    return false;
  }
  catch (const std::exception& e) {
    printf("[ERROR] Error duplicating program: %s\n", e.what());
    return false;
  }
}

ProgramInfo ProgramManager::ExtractProgramInfo(const std::string& filepath) {
  ProgramInfo info;
  info.filename = std::filesystem::path(filepath).stem().string();
  info.name = info.filename;
  info.description = "No description";
  info.author = "Unknown";
  info.blockCount = 0;
  info.connectionCount = 0;

  try {
    // Get file modification time
    auto ftime = std::filesystem::last_write_time(filepath);
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
    info.lastModified = std::ctime(&cftime);

    // Parse JSON to extract metadata
    std::ifstream inFile(filepath);
    if (inFile.is_open()) {
      nlohmann::json programData;
      inFile >> programData;

      if (programData.contains("blocks")) {
        info.blockCount = programData["blocks"].size();

        // Extract program info from START block
        for (const auto& block : programData["blocks"]) {
          if (block["type"] == "START" && block.contains("parameters")) {
            for (const auto& param : block["parameters"]) {
              if (param["name"] == "program_name") {
                info.name = param["value"];
              }
              else if (param["name"] == "description") {
                info.description = param["value"];
              }
              else if (param["name"] == "author") {
                info.author = param["value"];
              }
            }
          }
        }
      }

      if (programData.contains("connections")) {
        info.connectionCount = programData["connections"].size();
      }
    }
  }
  catch (const std::exception& e) {
    printf("[WARN] Warning: Could not extract info from %s: %s\n", filepath.c_str(), e.what());
  }

  return info;
}

std::string ProgramManager::GenerateUniqueFilename(const std::string& baseName) {
  std::string filename = baseName;
  int counter = 1;

  while (std::filesystem::exists(m_programsDirectory + filename + ".json")) {
    filename = baseName + "_" + std::to_string(counter);
    counter++;
  }

  return filename;
}

void ProgramManager::RenderProgramBrowser() {
  static char searchBuffer[256] = "";
  static bool showDeleteConfirm = false;
  static std::string deleteTarget;

  ImGui::Text("Program Library (%zu programs)", m_programList.size());
  ImGui::Separator();

  // Search bar
  ImGui::SetNextItemWidth(200);
  ImGui::InputText("Search", searchBuffer, sizeof(searchBuffer));
  ImGui::SameLine();
  if (ImGui::Button("Refresh")) {
    RefreshProgramList();
  }

  // Program list
  ImGui::BeginChild("ProgramList", ImVec2(0, 300), true);

  std::string searchStr = std::string(searchBuffer);
  std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

  for (const auto& program : m_programList) {
    std::string displayName = program.name.empty() ? program.filename : program.name;
    std::string lowerName = displayName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    // Filter by search
    if (!searchStr.empty() && lowerName.find(searchStr) == std::string::npos) {
      continue;
    }

    bool isSelected = (program.filename == m_currentProgram);

    if (ImGui::Selectable(displayName.c_str(), isSelected)) {
      // Load program callback would go here
      printf("Selected program: %s\n", program.filename.c_str());
    }

    // Right-click context menu
    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("Load")) {
        if (m_loadCallback) {
          m_loadCallback(program.filename);
        }
        // Close the popup after loading
        ImGui::CloseCurrentPopup();
      }
      if (ImGui::MenuItem("Duplicate")) {
        std::string newName = GenerateUniqueFilename(program.filename + "_copy");
        DuplicateProgram(program.filename, newName);
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Delete", nullptr, false, program.filename != m_currentProgram)) {
        deleteTarget = program.filename;
        showDeleteConfirm = true;
      }
      ImGui::EndPopup();
    }

    // Show details on hover
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::Text("Description: %s", program.description.c_str());
      ImGui::Text("Author: %s", program.author.c_str());
      ImGui::Text("Blocks: %d", program.blockCount);
      ImGui::Text("Connections: %d", program.connectionCount);
      ImGui::Text("Modified: %s", program.lastModified.c_str());
      ImGui::EndTooltip();
    }
  }

  ImGui::EndChild();

  // Delete confirmation popup
  if (showDeleteConfirm) {
    ImGui::OpenPopup("Delete Program?");
  }

  if (ImGui::BeginPopupModal("Delete Program?", &showDeleteConfirm)) {
    ImGui::Text("Are you sure you want to delete:");
    ImGui::Text("%s", deleteTarget.c_str());
    ImGui::Text("This action cannot be undone!");
    ImGui::Separator();

    if (ImGui::Button("Delete", ImVec2(120, 0))) {
      DeleteProgram(deleteTarget);
      showDeleteConfirm = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      showDeleteConfirm = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void ProgramManager::RenderSaveAsDialog() {
  static char nameBuffer[256] = "";
  static char descBuffer[512] = "";
  static char authorBuffer[256] = "User";

  ImGui::Text("Save Program As");
  ImGui::Separator();

  ImGui::Text("Program Name:");
  ImGui::SetNextItemWidth(300);
  ImGui::InputText("##name", nameBuffer, sizeof(nameBuffer));

  ImGui::Text("Description:");
  ImGui::SetNextItemWidth(300);
  ImGui::InputTextMultiline("##desc", descBuffer, sizeof(descBuffer), ImVec2(300, 60));

  ImGui::Text("Author:");
  ImGui::SetNextItemWidth(300);
  ImGui::InputText("##author", authorBuffer, sizeof(authorBuffer));

  ImGui::Separator();

  if (ImGui::Button("Save", ImVec2(120, 0))) {
    std::string filename = GenerateUniqueFilename(std::string(nameBuffer));
    if (m_saveCallback) {
      m_saveCallback(filename);
    }
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel", ImVec2(120, 0))) {
    ImGui::CloseCurrentPopup();
  }
}