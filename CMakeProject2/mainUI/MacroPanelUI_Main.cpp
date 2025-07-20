// MacroPanelUI_Main.cpp - Core functionality and constructor
#include "MacroPanelUI.h"
#include "MacroManager.h"
#include "MachineBlockUI.h"
#include "MacroPanelCameraHandler.h"
#include "include/camera/CameraManager.h"
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <cctype>

// ============================================================================
// CONSTRUCTOR & CORE METHODS
// ============================================================================

MacroPanelUI::MacroPanelUI(CameraManager* cameraManager) {
  RefreshMacroList();
  RefreshAvailablePrograms();

  // Initialize camera handler
  m_cameraHandler = std::make_unique<MacroPanelCameraHandler>(cameraManager);
}

// Add explicit destructor for std::unique_ptr with forward declaration
MacroPanelUI::~MacroPanelUI() = default;

void MacroPanelUI::SetMacroManager(MacroManager* macroManager) {
  m_macroManager = macroManager;
  RefreshMacroList();
}

void MacroPanelUI::SetMachineBlockUI(MachineBlockUI* blockUI) {
  m_blockUI = blockUI;
}

void MacroPanelUI::SetCameraManager(CameraManager* cameraManager) {
  if (m_cameraHandler) {
    m_cameraHandler->SetCameraManager(cameraManager);
  }
}

// ============================================================================
// MAIN RENDER METHOD - RENAMED to avoid conflicts
// ============================================================================

void MacroPanelUI::RenderMacroPanel() {
  if (!m_showWindow) return;

  // Get content region for 3-column layout with new proportions
  ImVec2 contentRegion = ImGui::GetContentRegionAvail();
  float leftWidth = contentRegion.x * 0.15f;   // 33% for left panel (was 25%)
  float middleWidth = contentRegion.x * 0.40f; // 25% for middle panel (was 50%) 
  float rightWidth = contentRegion.x * 0.55f;  // 42% for right panel (was 25%)

  // === LEFT PANEL ===
  ImGui::BeginChild("MacroLeftPanel", ImVec2(leftWidth, 0), true);
  RenderMacroLeftPanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // === MIDDLE PANEL ===
  ImGui::BeginChild("MacroMiddlePanel", ImVec2(middleWidth, 0), true);
  RenderMacroMiddlePanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // === RIGHT PANEL  ===
  ImGui::BeginChild("MacroRightPanel", ImVec2(rightWidth, 0), true);
  RenderMacroRightPanel();
  ImGui::EndChild();


  // NEW: Render load dialog as modal popup
  RenderLoadDialog();
}

// ============================================================================
// HELPER METHODS
// ============================================================================

void MacroPanelUI::RefreshMacroList() {
  m_availableMacros = GetAvailableMacros();
}

void MacroPanelUI::RefreshProgramItems() {
  m_programItems.clear();

  if (m_currentMacroName.empty() || !m_macroManager) return;

  auto* macro = m_macroManager->GetMacro(m_currentMacroName);
  if (!macro) return;

  for (int i = 0; i < macro->programs.size(); i++) {
    MacroProgramItem item;
    item.id = i;
    item.name = macro->programs[i].name;
    item.filePath = macro->programs[i].filePath;
    item.selected = false;
    m_programItems.push_back(item);
  }
}

void MacroPanelUI::RefreshAvailablePrograms() {
  m_availablePrograms = GetAvailablePrograms();
}

std::vector<std::string> MacroPanelUI::GetAvailableMacros() {
  if (m_macroManager) {
    return m_macroManager->GetMacroNames();
  }
  return {};
}

std::vector<std::string> MacroPanelUI::GetAvailablePrograms() {
  if (m_macroManager) {
    return m_macroManager->GetProgramNames();
  }
  return {};
}


void MacroPanelUI::RefreshMacroFiles() {
  m_availableMacroFiles = GetMacroFilesFromDirectory();
  m_selectedFileIndex = 0;
  memset(m_loadDialogSearchBuffer, 0, sizeof(m_loadDialogSearchBuffer));
}

std::vector<std::string> MacroPanelUI::GetMacroFilesFromDirectory() {
  std::vector<std::string> files;

  try {
    // Use filesystem to scan for .json files in macros directory
    std::filesystem::path macrosDir = "macros";

    if (std::filesystem::exists(macrosDir) && std::filesystem::is_directory(macrosDir)) {
      for (const auto& entry : std::filesystem::directory_iterator(macrosDir)) {
        if (entry.is_regular_file()) {
          std::string filename = entry.path().filename().string();

          // Only include .json files
          if (filename.ends_with(".json")) {
            files.push_back(filename);
          }
        }
      }
    }

    // Sort files alphabetically
    std::sort(files.begin(), files.end());

    std::cout << "Found " << files.size() << " macro files in /macros/ directory" << std::endl;
  }
  catch (const std::exception& e) {
    std::cout << "Error scanning macros directory: " << e.what() << std::endl;
  }

  return files;
}

void MacroPanelUI::RenderLoadDialog() {
  if (!m_showLoadDialog) return;

  // Center the dialog
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

  bool dialogOpen = true;
  if (ImGui::Begin("Load Macro File", &dialogOpen, ImGuiWindowFlags_Modal | ImGuiWindowFlags_NoResize)) {

    ImGui::Text("Select a macro file from /macros/ directory:");
    ImGui::Separator();
    ImGui::Spacing();

    // Search filter
    ImGui::Text("Filter:");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##SearchFilter", m_loadDialogSearchBuffer, sizeof(m_loadDialogSearchBuffer));

    ImGui::Spacing();

    // File list
    ImGui::Text("Available Files (%zu found):", m_availableMacroFiles.size());

    // Create filtered list based on search
    std::vector<std::string> filteredFiles;
    std::string searchTerm = std::string(m_loadDialogSearchBuffer);
    std::transform(searchTerm.begin(), searchTerm.end(), searchTerm.begin(), ::tolower);

    for (const auto& file : m_availableMacroFiles) {
      if (searchTerm.empty()) {
        filteredFiles.push_back(file);
      }
      else {
        std::string lowerFile = file;
        std::transform(lowerFile.begin(), lowerFile.end(), lowerFile.begin(), ::tolower);
        if (lowerFile.find(searchTerm) != std::string::npos) {
          filteredFiles.push_back(file);
        }
      }
    }

    // File selection listbox
    ImGui::BeginChild("FileList", ImVec2(0, 250), true);

    if (filteredFiles.empty()) {
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No macro files found");
      if (searchTerm.empty()) {
        ImGui::Text("Create some macros first, or check that /macros/ directory exists");
      }
      else {
        ImGui::Text("No files match the search filter");
      }
    }
    else {
      for (int i = 0; i < filteredFiles.size(); i++) {
        bool isSelected = (i == m_selectedFileIndex);

        if (ImGui::Selectable(filteredFiles[i].c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
          m_selectedFileIndex = i;

          // Double-click to load
          if (ImGui::IsMouseDoubleClicked(0)) {
            std::string selectedFile = "macros/" + filteredFiles[i];

            if (m_macroManager) {
              m_macroManager->LoadMacro(selectedFile);
              RefreshMacroList();
              RefreshAvailablePrograms();

              // Extract macro name from filename (remove _macro.json suffix)
              std::string macroName = filteredFiles[i];
              if (macroName.ends_with("_macro.json")) {
                macroName = macroName.substr(0, macroName.length() - 11);
              }
              else if (macroName.ends_with(".json")) {
                macroName = macroName.substr(0, macroName.length() - 5);
              }

              // Set as current macro
              for (int j = 0; j < m_availableMacros.size(); j++) {
                if (m_availableMacros[j] == macroName) {
                  m_selectedMacroIndex = j;
                  m_currentMacroName = macroName;
                  RefreshProgramItems();
                  break;
                }
              }

              std::cout << "Loaded macro: " << selectedFile << " as '" << macroName << "'" << std::endl;
            }

            m_showLoadDialog = false;
            break;
          }
        }

        if (isSelected) {
          ImGui::SetItemDefaultFocus();
        }
      }
    }

    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Selected file info
    if (!filteredFiles.empty() && m_selectedFileIndex >= 0 && m_selectedFileIndex < filteredFiles.size()) {
      ImGui::Text("Selected: %s", filteredFiles[m_selectedFileIndex].c_str());

      // Show file path
      std::string fullPath = "macros/" + filteredFiles[m_selectedFileIndex];
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Path: %s", fullPath.c_str());
    }

    ImGui::Spacing();

    // Buttons
    float buttonWidth = 100.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float totalWidth = buttonWidth * 3 + spacing * 2;
    float startX = (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f;

    ImGui::SetCursorPosX(startX);

    // Load button
    bool canLoad = !filteredFiles.empty() && m_selectedFileIndex >= 0 && m_selectedFileIndex < filteredFiles.size();

    if (!canLoad) {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    }

    if (ImGui::Button("Load", ImVec2(buttonWidth, 0)) && canLoad) {
      std::string selectedFile = "macros/" + filteredFiles[m_selectedFileIndex];

      if (m_macroManager) {
        m_macroManager->LoadMacro(selectedFile);
        RefreshMacroList();
        RefreshAvailablePrograms();

        // Extract macro name and set as current
        std::string macroName = filteredFiles[m_selectedFileIndex];
        if (macroName.ends_with("_macro.json")) {
          macroName = macroName.substr(0, macroName.length() - 11);
        }
        else if (macroName.ends_with(".json")) {
          macroName = macroName.substr(0, macroName.length() - 5);
        }

        for (int j = 0; j < m_availableMacros.size(); j++) {
          if (m_availableMacros[j] == macroName) {
            m_selectedMacroIndex = j;
            m_currentMacroName = macroName;
            RefreshProgramItems();
            break;
          }
        }

        std::cout << "Loaded macro: " << selectedFile << " as '" << macroName << "'" << std::endl;
      }

      m_showLoadDialog = false;
    }

    if (!canLoad) {
      ImGui::PopStyleVar();
    }

    ImGui::SameLine();

    // Refresh button
    if (ImGui::Button("Refresh", ImVec2(buttonWidth, 0))) {
      RefreshMacroFiles();
      std::cout << "Refreshed macro file list" << std::endl;
    }

    ImGui::SameLine();

    // Cancel button
    if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
      m_showLoadDialog = false;
    }
  }
  ImGui::End();

  // Close dialog if user clicked the X or pressed Escape
  if (!dialogOpen) {
    m_showLoadDialog = false;
  }
}

