// MacroPanelUI.cpp
#include "MacroPanelUI.h"
#include "MacroManager.h"
#include "MachineBlockUI.h"
#include <algorithm>
#include <iostream>

MacroPanelUI::MacroPanelUI() {
  RefreshMacroList();
  RefreshAvailablePrograms();
}

void MacroPanelUI::SetMacroManager(MacroManager* macroManager) {
  m_macroManager = macroManager;
  RefreshMacroList();
}

void MacroPanelUI::SetMachineBlockUI(MachineBlockUI* blockUI) {
  m_blockUI = blockUI;
}

void MacroPanelUI::RenderUI() {
  if (!m_showWindow) return;

  // Get content region for 3-column layout with new proportions
  ImVec2 contentRegion = ImGui::GetContentRegionAvail();
  float leftWidth = contentRegion.x * 0.33f;   // 33% for left panel (was 25%)
  float middleWidth = contentRegion.x * 0.25f; // 25% for middle panel (was 50%) 
  float rightWidth = contentRegion.x * 0.42f;  // 42% for right panel (was 25%)

  // === LEFT PANEL (33%) ===
  ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, 0), true);
  RenderLeftPanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // === MIDDLE PANEL (25%) ===
  ImGui::BeginChild("MiddlePanel", ImVec2(middleWidth, 0), true);
  RenderMiddlePanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // === RIGHT PANEL (42%) ===
  ImGui::BeginChild("RightPanel", ImVec2(rightWidth, 0), true);
  RenderRightPanel();
  ImGui::EndChild();
}

// ============================================================================
// LEFT PANEL - Macro List & Controls
// ============================================================================
void MacroPanelUI::RenderLeftPanel() {
  ImGui::Text("Macro Management");
  ImGui::Separator();

  RenderMacroList();
  ImGui::Spacing();
  RenderNewMacroSection();
  ImGui::Spacing();
  RenderLoadSaveSection();

  // Move program selection from right panel to here
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Add Program:");

  if (!m_availablePrograms.empty()) {
    std::vector<const char*> items;
    for (const auto& program : m_availablePrograms) {
      items.push_back(program.c_str());
    }

    ImGui::PushItemWidth(-1);
    ImGui::Combo("##ProgramSelect", &m_selectedProgramIndex, items.data(), items.size());
    ImGui::PopItemWidth();

    if (ImGui::Button("Add", ImVec2(-1, 0))) {
      if (m_selectedProgramIndex >= 0 && m_selectedProgramIndex < m_availablePrograms.size() &&
        !m_currentMacroName.empty() && m_macroManager) {

        std::string selectedProgram = m_availablePrograms[m_selectedProgramIndex];

        // Check if program is already in the macro
        bool alreadyExists = false;
        for (const auto& item : m_programItems) {
          if (item.name == selectedProgram) {
            alreadyExists = true;
            break;
          }
        }

        if (!alreadyExists) {
          m_macroManager->AddProgramToMacro(m_currentMacroName, selectedProgram);
          RefreshProgramItems();
          std::cout << "Added program '" << selectedProgram << "' to macro '" << m_currentMacroName << "'" << std::endl;
        }
        else {
          std::cout << "Program '" << selectedProgram << "' already exists in macro" << std::endl;
        }
      }
    }
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No programs available");
    if (ImGui::Button("Scan", ImVec2(-1, 0))) {
      if (m_macroManager) {
        m_macroManager->ScanForPrograms();
        RefreshAvailablePrograms();
        std::cout << "Rescanned for available programs" << std::endl;
      }
    }
  }
}

void MacroPanelUI::RenderMacroList() {
  ImGui::Text("Select Macro:");

  if (!m_availableMacros.empty()) {
    std::vector<const char*> items;
    for (const auto& macro : m_availableMacros) {
      items.push_back(macro.c_str());
    }

    if (ImGui::Combo("##MacroSelect", &m_selectedMacroIndex, items.data(), items.size())) {
      if (m_selectedMacroIndex >= 0 && m_selectedMacroIndex < m_availableMacros.size()) {
        m_currentMacroName = m_availableMacros[m_selectedMacroIndex];
        RefreshProgramItems();
        std::cout << "Selected macro: " << m_currentMacroName << std::endl;
      }
    }
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No macros available");
  }

  if (ImGui::Button("Refresh", ImVec2(-1, 0))) {
    // Force scan for programs first
    if (m_macroManager) {
      m_macroManager->ScanForPrograms();
    }
    RefreshMacroList();
    RefreshAvailablePrograms();
    std::cout << "Refreshed macro and program lists" << std::endl;
  }
}

void MacroPanelUI::RenderNewMacroSection() {
  ImGui::Text("Create New:");

  ImGui::Text("Name:");
  ImGui::InputText("##MacroName", m_macroNameBuffer, sizeof(m_macroNameBuffer));

  ImGui::Text("Description:");
  ImGui::InputTextMultiline("##MacroDesc", m_macroDescBuffer, sizeof(m_macroDescBuffer), ImVec2(-1, 60));

  if (ImGui::Button("Create", ImVec2(-1, 0))) {
    if (strlen(m_macroNameBuffer) > 0 && m_macroManager) {
      // Create the macro
      if (m_macroManager->CreateMacro(m_macroNameBuffer, m_macroDescBuffer)) {
        // Auto-save the newly created macro
        std::string fileName = "macros/" + std::string(m_macroNameBuffer) + "_macro.json";
        m_macroManager->SaveMacro(m_macroNameBuffer, fileName);
        std::cout << "Created and saved macro: " << m_macroNameBuffer << std::endl;

        // Set as current macro and refresh lists
        m_currentMacroName = m_macroNameBuffer;
        RefreshMacroList();
        RefreshProgramItems();

        // Find the new macro in the list and select it
        for (int i = 0; i < m_availableMacros.size(); i++) {
          if (m_availableMacros[i] == m_currentMacroName) {
            m_selectedMacroIndex = i;
            break;
          }
        }
      }

      // Clear input buffers
      memset(m_macroNameBuffer, 0, sizeof(m_macroNameBuffer));
      memset(m_macroDescBuffer, 0, sizeof(m_macroDescBuffer));
    }
  }
}

void MacroPanelUI::RenderLoadSaveSection() {
  ImGui::Text("File Operations:");

  // Load button (full width)
  if (ImGui::Button("Load", ImVec2(-1, 0))) {
    // Try to load the test_macro.json file we can see in your screenshot
    if (m_macroManager) {
      std::string macroFileName = "macros/test_macro.json";
      m_macroManager->LoadMacro(macroFileName);

      // After loading, refresh everything
      RefreshMacroList();
      RefreshAvailablePrograms();

      // Set the loaded macro as current if it exists
      if (!m_availableMacros.empty()) {
        for (int i = 0; i < m_availableMacros.size(); i++) {
          if (m_availableMacros[i] == "test") {  // "test_macro.json" becomes "test"
            m_selectedMacroIndex = i;
            m_currentMacroName = m_availableMacros[i];
            RefreshProgramItems();
            break;
          }
        }
      }

      std::cout << "Attempted to load macro: " << macroFileName << std::endl;
    }
  }

  // Save button (full width, UNDER Load button)
  if (ImGui::Button("Save", ImVec2(-1, 0))) {
    if (!m_currentMacroName.empty() && m_macroManager) {
      std::string fileName = "macros/" + m_currentMacroName + "_macro.json";
      m_macroManager->SaveMacro(m_currentMacroName, fileName);
      std::cout << "Saved macro: " << m_currentMacroName << " to " << fileName << std::endl;
    }
    else {
      std::cout << "No macro selected to save" << std::endl;
    }
  }
}

// ============================================================================
// MIDDLE PANEL - Program Items Table
// ============================================================================
void MacroPanelUI::RenderMiddlePanel() {
  ImGui::Text("Program Sequence");
  ImGui::Separator();

  // Show current macro name
  if (!m_currentMacroName.empty()) {
    ImGui::Text("Macro: %s", m_currentMacroName.c_str());
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No macro selected");
  }

  ImGui::Spacing();
  RenderExecutionControls();
  ImGui::Spacing();
  RenderProgramTable();
}

void MacroPanelUI::RenderExecutionControls() {
  // Play, Pause, Stop buttons
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
  if (ImGui::Button("Play", ImVec2(60, 30))) {
    PlayExecution();
  }
  ImGui::PopStyleColor(2);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Execute selected programs in sequence");
  }

  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.7f, 0.3f, 1.0f));
  if (ImGui::Button("Pause", ImVec2(60, 30))) {
    PauseExecution();
  }
  ImGui::PopStyleColor(2);

  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
  if (ImGui::Button("Stop", ImVec2(60, 30))) {
    StopExecution();
  }
  ImGui::PopStyleColor(2);

  // Enhanced execution status
  ImGui::SameLine();
  if (m_isExecuting) {
    if (m_currentProgramIndex >= 0 && m_currentProgramIndex < m_programItems.size()) {
      ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Running: %s (%d/%d)",
        m_programItems[m_currentProgramIndex].name.c_str(),
        m_currentProgramIndex + 1,
        (int)m_programItems.size());
    }
    else {
      ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Starting...");
    }
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Stopped");
  }
}

void MacroPanelUI::RenderProgramTable() {
  if (m_programItems.empty()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No programs in this macro");
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Add programs from the left panel");
    return;
  }

  // Use proper table columns that fit in the 25% width
  ImGui::Columns(4, "ProgramColumns", true);
  ImGui::SetColumnWidth(0, 25);  // ID column - smaller
  ImGui::SetColumnWidth(1, 25);  // Checkbox column - smaller
  ImGui::SetColumnWidth(2, 80);  // Program name - smaller to fit 25% width
  ImGui::SetColumnWidth(3, 70);  // Run/Remove buttons - adjusted

  // Header
  ImGui::Text("ID");
  ImGui::NextColumn();
  ImGui::Text("✓");
  ImGui::NextColumn();
  ImGui::Text("Program");
  ImGui::NextColumn();
  ImGui::Text("Actions");
  ImGui::NextColumn();
  ImGui::Separator();

  // Program rows using column layout
  for (int i = 0; i < m_programItems.size(); i++) {
    RenderProgramRowColumns(i, m_programItems[i]);
  }

  ImGui::Columns(1); // Reset columns
}

void MacroPanelUI::RenderTableHeader() {
  ImGui::Columns(4, "ProgramColumns", true);
  ImGui::SetColumnWidth(0, 30);  // ID
  ImGui::SetColumnWidth(1, 30);  // Checkbox
  ImGui::SetColumnWidth(2, 150); // Name
  ImGui::SetColumnWidth(3, 60);  // Run button

  ImGui::Text("ID");
  ImGui::NextColumn();
  ImGui::Text("✓");
  ImGui::NextColumn();
  ImGui::Text("Program Name");
  ImGui::NextColumn();
  ImGui::Text("Run");
  ImGui::NextColumn();
  ImGui::Separator();
}

void MacroPanelUI::RenderProgramRow(int rowIndex, MacroProgramItem& item) {
  ImGui::PushID(rowIndex);

  // Highlight current executing row with background color
  if (m_isExecuting && rowIndex == m_currentProgramIndex) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f)); // Bright green text
    // Add background highlight
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, 25);
    ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
      IM_COL32(0, 100, 0, 80)); // Green background
  }

  // Create a single row with all elements using better spacing
  ImGui::Text("%d", rowIndex + 1);
  ImGui::SameLine(40);  // Position for checkbox

  ImGui::Checkbox("##selected", &item.selected);
  ImGui::SameLine(80);  // Position for program name

  // Show execution indicator next to program name
  if (m_isExecuting && rowIndex == m_currentProgramIndex) {
    ImGui::Text("▶ %s", item.name.c_str()); // Arrow indicator for currently running
  }
  else {
    ImGui::Text("%s", item.name.c_str());
  }

  // Calculate positions from the right side of the available space
  float contentWidth = ImGui::GetContentRegionAvail().x;
  float buttonStartPos = ImGui::GetCursorPosX() + contentWidth - 80; // 80 pixels for both buttons

  ImGui::SameLine(buttonStartPos); // Position for Run button

  // Run Button (>>) - Blue
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
  if (ImGui::Button((">>##run" + std::to_string(rowIndex)).c_str(), ImVec2(35, 22))) {
    ExecuteSingleProgram(item.name);
  }
  ImGui::PopStyleColor(2);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Run this program only");
  }

  ImGui::SameLine(); // Position for Remove button right next to Run button

  // Remove Button (X) - Red
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
  if (ImGui::Button(("X##remove" + std::to_string(rowIndex)).c_str(), ImVec2(30, 22))) {
    if (m_macroManager && !m_currentMacroName.empty()) {
      m_macroManager->RemoveProgramFromMacro(m_currentMacroName, rowIndex);
      RefreshProgramItems();
      std::cout << "Removed program '" << item.name << "' from macro" << std::endl;
    }
  }
  ImGui::PopStyleColor(2);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Remove from macro");
  }

  if (m_isExecuting && rowIndex == m_currentProgramIndex) {
    ImGui::PopStyleColor(); // Pop the text color
  }

  ImGui::PopID();
}

// New column-based row rendering method
void MacroPanelUI::RenderProgramRowColumns(int rowIndex, MacroProgramItem& item) {
  ImGui::PushID(rowIndex);

  // Highlight current executing row
  if (m_isExecuting && rowIndex == m_currentProgramIndex) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
  }

  // ID Column
  ImGui::Text("%d", rowIndex + 1);
  ImGui::NextColumn();

  // Checkbox Column
  ImGui::Checkbox("##selected", &item.selected);
  ImGui::NextColumn();

  // Program Name Column (truncated to fit)
  std::string displayName = item.name;
  if (displayName.length() > 8) {
    displayName = displayName.substr(0, 6) + "..";
  }

  if (m_isExecuting && rowIndex == m_currentProgramIndex) {
    ImGui::Text("▶%s", displayName.c_str());
  }
  else {
    ImGui::Text("%s", displayName.c_str());
  }
  ImGui::NextColumn();

  // Actions Column (Run and Remove buttons)
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
  if (ImGui::Button((">>##run" + std::to_string(rowIndex)).c_str(), ImVec2(25, 18))) {
    ExecuteSingleProgram(item.name);
  }
  ImGui::PopStyleColor();

  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
  if (ImGui::Button(("X##remove" + std::to_string(rowIndex)).c_str(), ImVec2(20, 18))) {
    if (m_macroManager && !m_currentMacroName.empty()) {
      m_macroManager->RemoveProgramFromMacro(m_currentMacroName, rowIndex);
      RefreshProgramItems();
      std::cout << "Removed program '" << item.name << "' from macro" << std::endl;
    }
  }
  ImGui::PopStyleColor();

  ImGui::NextColumn();

  if (m_isExecuting && rowIndex == m_currentProgramIndex) {
    ImGui::PopStyleColor();
  }

  ImGui::PopID();
}

// ============================================================================
// RIGHT PANEL - Properties & Actions
// ============================================================================
void MacroPanelUI::RenderRightPanel() {
  ImGui::Text("Properties");
  ImGui::Separator();

  RenderMacroProperties();
  ImGui::Spacing();
  RenderExecutionStatus();

  // Keep Canvas area but empty as requested
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Canvas");

  // Create an empty canvas area
  ImVec2 canvasSize = ImVec2(-1, 300); // Take full width, 300px height
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();

  ImGui::InvisibleButton("Canvas", canvasSize);

  // Draw empty canvas background
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  ImU32 canvasColor = IM_COL32(45, 45, 45, 255); // Dark gray background
  ImU32 borderColor = IM_COL32(100, 100, 100, 255); // Light gray border

  drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), canvasColor);
  drawList->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), borderColor, 0.0f, 0, 2.0f);

  // Keep canvas empty as requested - no visualization
}

void MacroPanelUI::RenderMacroProperties() {
  if (m_currentMacroName.empty()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No macro selected");
    return;
  }

  ImGui::Text("Macro: %s", m_currentMacroName.c_str());
  ImGui::Text("Programs: %zu", m_programItems.size());

  int selectedCount = 0;
  for (const auto& item : m_programItems) {
    if (item.selected) selectedCount++;
  }
  ImGui::Text("Selected: %d", selectedCount);
}

void MacroPanelUI::RenderAvailablePrograms() {
  ImGui::Text("Add Program:");

  if (!m_availablePrograms.empty()) {
    std::vector<const char*> items;
    for (const auto& program : m_availablePrograms) {
      items.push_back(program.c_str());
    }

    ImGui::PushItemWidth(-1);
    ImGui::Combo("##ProgramSelect", &m_selectedProgramIndex, items.data(), items.size());
    ImGui::PopItemWidth();

    if (ImGui::Button("Add", ImVec2(-1, 0))) {
      if (m_selectedProgramIndex >= 0 && m_selectedProgramIndex < m_availablePrograms.size() &&
        !m_currentMacroName.empty() && m_macroManager) {

        std::string selectedProgram = m_availablePrograms[m_selectedProgramIndex];

        // Check if program is already in the macro
        bool alreadyExists = false;
        for (const auto& item : m_programItems) {
          if (item.name == selectedProgram) {
            alreadyExists = true;
            break;
          }
        }

        if (!alreadyExists) {
          m_macroManager->AddProgramToMacro(m_currentMacroName, selectedProgram);
          RefreshProgramItems();
          std::cout << "Added program '" << selectedProgram << "' to macro '" << m_currentMacroName << "'" << std::endl;
        }
        else {
          std::cout << "Program '" << selectedProgram << "' already exists in macro" << std::endl;
        }
      }
    }
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No programs available");
    if (ImGui::Button("Scan", ImVec2(-1, 0))) {
      if (m_macroManager) {
        m_macroManager->ScanForPrograms();
        RefreshAvailablePrograms();
        std::cout << "Rescanned for available programs" << std::endl;
      }
    }
  }
}

void MacroPanelUI::RenderExecutionStatus() {
  ImGui::Text("Status:");
  if (m_isExecuting) {
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Executing");
    if (m_currentProgramIndex >= 0) {
      ImGui::Text("Step: %d/%zu", m_currentProgramIndex + 1, m_programItems.size());
    }
  }
  else {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Ready");
  }
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

// ============================================================================
// EXECUTION METHODS
// ============================================================================
void MacroPanelUI::ExecuteSelectedPrograms() {
  if (m_currentMacroName.empty() || !m_macroManager) return;

  // Count selected programs
  int selectedCount = 0;
  for (const auto& item : m_programItems) {
    if (item.selected) selectedCount++;
  }

  if (selectedCount == 0) {
    std::cout << "No programs selected for execution" << std::endl;
    return;
  }

  // Execute the macro with selected programs
  m_isExecuting = true;
  m_currentProgramIndex = 0;

  std::cout << "Starting macro execution: " << m_currentMacroName << " with " << selectedCount << " selected programs" << std::endl;

  // Set up callback to track execution progress
  m_macroManager->ExecuteMacro(m_currentMacroName, [this](bool success) {
    m_isExecuting = false;
    m_currentProgramIndex = -1;
    std::cout << "Macro execution completed: " << (success ? "SUCCESS" : "FAILED") << std::endl;
  });
}

void MacroPanelUI::ExecuteSingleProgram(const std::string& programName) {
  if (m_macroManager) {
    std::cout << "Executing single program: " << programName << std::endl;
    m_macroManager->ExecuteSingleProgram(programName);
  }
}

void MacroPanelUI::PlayExecution() {
  std::cout << "Play execution requested" << std::endl;
  ExecuteSelectedPrograms();
}

void MacroPanelUI::PauseExecution() {
  m_isPaused = !m_isPaused;
  std::cout << "Pause execution: " << (m_isPaused ? "Paused" : "Resumed") << std::endl;
  // TODO: Implement pause functionality in MacroManager
}

void MacroPanelUI::StopExecution() {
  std::cout << "Stop execution requested" << std::endl;
  if (m_macroManager) {
    m_macroManager->StopExecution();
  }
  m_isExecuting = false;
  m_currentProgramIndex = -1;
}

// Add the macro visualization method
void MacroPanelUI::RenderMacroVisualization(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize) {
  if (m_programItems.empty()) return;

  float boxWidth = 80.0f;
  float boxHeight = 30.0f;
  float spacing = 20.0f;
  float startY = canvasPos.y + 20.0f;

  // Calculate total width needed and starting X position for centering
  float totalWidth = (boxWidth * m_programItems.size()) + (spacing * (m_programItems.size() - 1));
  float startX = canvasPos.x + (canvasSize.x - totalWidth) * 0.5f;

  for (int i = 0; i < m_programItems.size(); i++) {
    float x = startX + i * (boxWidth + spacing);
    float y = startY;

    // Determine box color based on state
    ImU32 boxColor;
    ImU32 textColor = IM_COL32(255, 255, 255, 255);

    if (m_isExecuting && i == m_currentProgramIndex) {
      boxColor = IM_COL32(0, 150, 0, 255); // Green for currently executing
    }
    else if (m_programItems[i].selected) {
      boxColor = IM_COL32(0, 100, 200, 255); // Blue for selected
    }
    else {
      boxColor = IM_COL32(80, 80, 80, 255); // Gray for unselected
    }

    // Draw program box
    ImVec2 boxMin = ImVec2(x, y);
    ImVec2 boxMax = ImVec2(x + boxWidth, y + boxHeight);

    drawList->AddRectFilled(boxMin, boxMax, boxColor, 5.0f);
    drawList->AddRect(boxMin, boxMax, IM_COL32(200, 200, 200, 255), 5.0f, 0, 1.5f);

    // Draw program name (truncated if too long)
    std::string displayName = m_programItems[i].name;
    if (displayName.length() > 10) {
      displayName = displayName.substr(0, 8) + "..";
    }

    ImVec2 textSize = ImGui::CalcTextSize(displayName.c_str());
    ImVec2 textPos = ImVec2(x + (boxWidth - textSize.x) * 0.5f, y + (boxHeight - textSize.y) * 0.5f);
    drawList->AddText(textPos, textColor, displayName.c_str());

    // Draw arrow to next program
    if (i < m_programItems.size() - 1) {
      float arrowStartX = x + boxWidth + 5;
      float arrowEndX = x + boxWidth + spacing - 5;
      float arrowY = y + boxHeight * 0.5f;

      drawList->AddLine(ImVec2(arrowStartX, arrowY), ImVec2(arrowEndX, arrowY),
        IM_COL32(150, 150, 150, 255), 2.0f);

      // Arrow head
      drawList->AddLine(ImVec2(arrowEndX - 5, arrowY - 3), ImVec2(arrowEndX, arrowY),
        IM_COL32(150, 150, 150, 255), 2.0f);
      drawList->AddLine(ImVec2(arrowEndX - 5, arrowY + 3), ImVec2(arrowEndX, arrowY),
        IM_COL32(150, 150, 150, 255), 2.0f);
    }
  }

  // Add legend at the bottom
  float legendY = startY + boxHeight + 40.0f;
  ImVec2 legendPos = ImVec2(canvasPos.x + 10, legendY);

  // Legend items
  drawList->AddRectFilled(ImVec2(legendPos.x, legendPos.y), ImVec2(legendPos.x + 15, legendPos.y + 15),
    IM_COL32(0, 150, 0, 255), 2.0f);
  drawList->AddText(ImVec2(legendPos.x + 20, legendPos.y), IM_COL32(200, 200, 200, 255), "Running");

  drawList->AddRectFilled(ImVec2(legendPos.x + 80, legendPos.y), ImVec2(legendPos.x + 95, legendPos.y + 15),
    IM_COL32(0, 100, 200, 255), 2.0f);
  drawList->AddText(ImVec2(legendPos.x + 100, legendPos.y), IM_COL32(200, 200, 200, 255), "Selected");

  drawList->AddRectFilled(ImVec2(legendPos.x + 170, legendPos.y), ImVec2(legendPos.x + 185, legendPos.y + 15),
    IM_COL32(80, 80, 80, 255), 2.0f);
  drawList->AddText(ImVec2(legendPos.x + 190, legendPos.y), IM_COL32(200, 200, 200, 255), "Unselected");
}