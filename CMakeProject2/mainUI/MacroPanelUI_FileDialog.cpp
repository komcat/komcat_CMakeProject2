
#include "MacroPanelUI.h"
#include "MacroManager.h"
#include <filesystem>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

void MacroPanelUI::RenderFileDialog() {
  // Render load dialog
  if (m_showLoadDialog) {
    RenderLoadDialog();
  }

  // Render save dialog
  if (m_showSaveDialog) {
    RenderSaveDialog();
  }
}

void MacroPanelUI::RenderLoadDialog() {
  ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  bool open = true;
  if (ImGui::Begin("Load Macro File", &open, ImGuiWindowFlags_Modal | ImGuiWindowFlags_NoCollapse)) {

    // Current directory display
    ImGui::Text("Directory: %s", m_currentDirectory.c_str());
    ImGui::Separator();

    // Directory navigation buttons
    if (ImGui::Button("..") && m_currentDirectory != ".") {
      fs::path parentPath = fs::path(m_currentDirectory).parent_path();
      NavigateToDirectory(parentPath.string().empty() ? "." : parentPath.string());
    }
    ImGui::SameLine();
    if (ImGui::Button("macros")) {
      NavigateToDirectory("macros");
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
      RefreshDirectoryListing();
    }

    ImGui::Separator();

    // File listing
    ImGui::BeginChild("FileList", ImVec2(0, -40), true);

    if (m_directoryEntries.empty()) {
      ImGui::Text("No files found in directory");
    }
    else {
      for (int i = 0; i < m_directoryEntries.size(); i++) {
        const auto& entry = m_directoryEntries[i];
        std::string displayName = entry.path().filename().string();

        // Different icons for directories and files
        if (entry.is_directory()) {
          displayName = "[DIR] " + displayName;
        }
        else if (IsJsonFile(entry)) {
          displayName = "[JSON] " + displayName;
        }
        else {
          displayName = "[FILE] " + displayName;
        }

        // Selectable item
        bool isSelected = (i == m_selectedFileIndex);
        if (ImGui::Selectable(displayName.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
          m_selectedFileIndex = i;

          // Double-click handling
          if (ImGui::IsMouseDoubleClicked(0)) {
            if (entry.is_directory()) {
              // Navigate into directory
              NavigateToDirectory(entry.path().string());
              m_selectedFileIndex = -1;
            }
            else if (IsJsonFile(entry)) {
              // Load the selected file
              std::string filePath = entry.path().string();
              if (m_macroManager) {
                m_macroManager->LoadMacro(filePath);
                RefreshMacroList();
                RefreshAvailablePrograms();

                // Try to select the loaded macro
                std::string macroName = entry.path().stem().string();
                if (macroName.ends_with("_macro")) {
                  macroName = macroName.substr(0, macroName.length() - 6);
                }

                for (int j = 0; j < m_availableMacros.size(); j++) {
                  if (m_availableMacros[j] == macroName) {
                    m_selectedMacroIndex = j;
                    m_currentMacroName = macroName;
                    RefreshProgramItems();
                    break;
                  }
                }

                std::cout << "Loaded macro from: " << filePath << std::endl;
              }
              m_showLoadDialog = false;
            }
          }
        }
      }
    }

    ImGui::EndChild();

    // Bottom buttons
    ImGui::Separator();

    bool canLoad = (m_selectedFileIndex >= 0 &&
      m_selectedFileIndex < m_directoryEntries.size() &&
      IsJsonFile(m_directoryEntries[m_selectedFileIndex]));

    if (!canLoad) {
      ImGui::BeginDisabled();
    }

    if (ImGui::Button("Load Selected", ImVec2(120, 30))) {
      if (canLoad) {
        std::string filePath = m_directoryEntries[m_selectedFileIndex].path().string();
        if (m_macroManager) {
          m_macroManager->LoadMacro(filePath);
          RefreshMacroList();
          RefreshAvailablePrograms();

          // Try to select the loaded macro
          std::string macroName = m_directoryEntries[m_selectedFileIndex].path().stem().string();
          if (macroName.ends_with("_macro")) {
            macroName = macroName.substr(0, macroName.length() - 6);
          }

          for (int j = 0; j < m_availableMacros.size(); j++) {
            if (m_availableMacros[j] == macroName) {
              m_selectedMacroIndex = j;
              m_currentMacroName = macroName;
              RefreshProgramItems();
              break;
            }
          }

          std::cout << "Loaded macro from: " << filePath << std::endl;
        }
        m_showLoadDialog = false;
      }
    }

    if (!canLoad) {
      ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(80, 30))) {
      m_showLoadDialog = false;
    }

    // Help text
    if (m_selectedFileIndex >= 0 && m_selectedFileIndex < m_directoryEntries.size()) {
      ImGui::SameLine();
      ImGui::Text("Selected: %s", m_directoryEntries[m_selectedFileIndex].path().filename().string().c_str());
    }
  }
  ImGui::End();

  // Close dialog if user clicked X
  if (!open) {
    m_showLoadDialog = false;
  }
}

void MacroPanelUI::RenderSaveDialog() {
  ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  bool open = true;
  if (ImGui::Begin("Save Macro File", &open, ImGuiWindowFlags_Modal | ImGuiWindowFlags_NoCollapse)) {

    // Current directory display
    ImGui::Text("Directory: %s", m_currentDirectory.c_str());
    ImGui::Separator();

    // Directory navigation buttons
    if (ImGui::Button("..") && m_currentDirectory != ".") {
      fs::path parentPath = fs::path(m_currentDirectory).parent_path();
      NavigateToDirectory(parentPath.string().empty() ? "." : parentPath.string());
    }
    ImGui::SameLine();
    if (ImGui::Button("macros")) {
      NavigateToDirectory("macros");
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
      RefreshDirectoryListing();
    }

    ImGui::Separator();

    // File name input
    ImGui::Text("File name:");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##FileName", m_saveFileName, sizeof(m_saveFileName));

    ImGui::Separator();

    // File listing (for reference)
    ImGui::Text("Existing files:");
    ImGui::BeginChild("FileList", ImVec2(0, -40), true);

    for (int i = 0; i < m_directoryEntries.size(); i++) {
      const auto& entry = m_directoryEntries[i];
      std::string displayName = entry.path().filename().string();

      if (entry.is_directory()) {
        displayName = "[DIR] " + displayName;
      }
      else if (IsJsonFile(entry)) {
        displayName = "[JSON] " + displayName;
      }
      else {
        displayName = "[FILE] " + displayName;
      }

      if (ImGui::Selectable(displayName.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
        if (ImGui::IsMouseDoubleClicked(0)) {
          if (entry.is_directory()) {
            NavigateToDirectory(entry.path().string());
          }
          else if (IsJsonFile(entry)) {
            // Set filename for overwrite
            std::string fileName = entry.path().filename().string();
            strncpy_s(m_saveFileName, sizeof(m_saveFileName), fileName.c_str(), sizeof(m_saveFileName) - 1);
          }
        }
      }
    }

    ImGui::EndChild();

    // Bottom buttons
    ImGui::Separator();

    bool canSave = (strlen(m_saveFileName) > 0 && !m_currentMacroName.empty());

    if (!canSave) {
      ImGui::BeginDisabled();
    }

    if (ImGui::Button("Save", ImVec2(80, 30))) {
      if (canSave && m_macroManager) {
        std::string fileName = m_saveFileName;

        // Ensure .json extension
        if (!fileName.ends_with(".json")) {
          fileName += ".json";
        }

        // Create full path
        fs::path fullPath = fs::path(m_currentDirectory) / fileName;
        std::string filePath = fullPath.string();

        // Check if file exists and confirm overwrite
        bool shouldSave = true;
        if (fs::exists(fullPath)) {
          // For now, just warn in console. You could add a confirmation dialog here
          std::cout << "Warning: File already exists, overwriting: " << filePath << std::endl;
        }

        if (shouldSave) {
          m_macroManager->SaveMacro(m_currentMacroName, filePath);
          std::cout << "Saved macro '" << m_currentMacroName << "' to: " << filePath << std::endl;
          RefreshDirectoryListing(); // Refresh to show the new file
          m_showSaveDialog = false;
        }
      }
    }

    if (!canSave) {
      ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(80, 30))) {
      m_showSaveDialog = false;
    }

    // Help text
    if (strlen(m_saveFileName) > 0) {
      ImGui::SameLine();
      ImGui::Text("Will save as: %s", m_saveFileName);
    }
  }
  ImGui::End();

  // Close dialog if user clicked X
  if (!open) {
    m_showSaveDialog = false;
  }
}

void MacroPanelUI::RefreshDirectoryListing() {
  m_directoryEntries.clear();
  m_selectedFileIndex = -1;

  try {
    if (fs::exists(m_currentDirectory) && fs::is_directory(m_currentDirectory)) {
      for (const auto& entry : fs::directory_iterator(m_currentDirectory)) {
        m_directoryEntries.push_back(entry);
      }

      // Sort: directories first, then files, alphabetically within each group
      std::sort(m_directoryEntries.begin(), m_directoryEntries.end(),
        [](const fs::directory_entry& a, const fs::directory_entry& b) {
        if (a.is_directory() && !b.is_directory()) return true;
        if (!a.is_directory() && b.is_directory()) return false;
        return a.path().filename().string() < b.path().filename().string();
      });
    }
  }
  catch (const fs::filesystem_error& e) {
    std::cout << "Error reading directory '" << m_currentDirectory << "': " << e.what() << std::endl;
  }
}

void MacroPanelUI::NavigateToDirectory(const std::string& path) {
  try {
    if (fs::exists(path) && fs::is_directory(path)) {
      m_currentDirectory = fs::canonical(path).string();
      RefreshDirectoryListing();
    }
    else {
      std::cout << "Invalid directory: " << path << std::endl;
    }
  }
  catch (const fs::filesystem_error& e) {
    std::cout << "Error navigating to directory '" << path << "': " << e.what() << std::endl;
  }
}

bool MacroPanelUI::IsJsonFile(const std::filesystem::directory_entry& entry) {
  if (entry.is_directory()) return false;
  std::string extension = entry.path().extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
  return extension == ".json";
}
