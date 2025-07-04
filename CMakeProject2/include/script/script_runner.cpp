// script_runner.cpp
#include "include/script/script_runner.h"
#include "include/script/print_operation.h"
#include "imgui.h"
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <mutex>
using json = nlohmann::json;

ScriptRunner::ScriptRunner(MachineOperations& machineOps, ScriptPrintViewer* printViewer)
  : m_machineOps(machineOps),
  m_printViewer(printViewer),
  m_isVisible(true),
  m_name("Script Runner"),
  m_showEditDialog(false),
  m_editingSlotIndex(-1),
  m_visibleSlotCount(10),
  m_showSettings(false)
{
  // Initialize slots
  for (int i = 0; i < NUM_SLOTS; i++) {
    m_slots[i].executor = std::make_unique<ScriptExecutor>(m_machineOps);

    // Set up callbacks
    m_slots[i].executor->SetExecutionCallback(
      [this, i](ScriptExecutor::ExecutionState state) {
      OnSlotExecutionStateChanged(i, state);
    }
    );

    m_slots[i].executor->SetLogCallback(
      [this, i](const std::string& message) {
      OnSlotLog(i, message);
    }
    );
  }

  // Initialize buffers
  memset(m_editNameBuffer, 0, sizeof(m_editNameBuffer));
  memset(m_editPathBuffer, 0, sizeof(m_editPathBuffer));
  memset(m_editDescriptionBuffer, 0, sizeof(m_editDescriptionBuffer));

  // Default visible slots
  m_visibleSlotCount = 10;
  m_showSettings = false;

  // Load configuration
  LoadConfiguration();

  // Get initial list of scripts
  RefreshFileList();
}

ScriptRunner::~ScriptRunner() {
  // Save configuration on exit
  SaveConfiguration();

  // Stop all running scripts
  for (int i = 0; i < NUM_SLOTS; i++) {
    if (m_slots[i].isExecuting) {
      StopSlot(i);
    }
  }
}

void ScriptRunner::LoadConfiguration() {
  try {
    std::ifstream file(CONFIG_FILE);
    if (!file.is_open()) {
      // Config file doesn't exist, use defaults
      return;
    }

    json config;
    file >> config;

    // Load slot configurations
    if (config.contains("slots") && config["slots"].is_array()) {
      const auto& slotsJson = config["slots"];
      for (size_t i = 0; i < (std::min)(slotsJson.size(), static_cast<size_t>(NUM_SLOTS)); i++) {
        const auto& slot = slotsJson[i];

        if (slot.contains("scriptPath") && slot["scriptPath"].is_string()) {
          m_slots[i].scriptPath = slot["scriptPath"];
        }

        if (slot.contains("displayName") && slot["displayName"].is_string()) {
          m_slots[i].displayName = slot["displayName"];
        }
        else {
          // Use filename as display name if not specified
          m_slots[i].displayName = std::filesystem::path(m_slots[i].scriptPath).filename().stem().string();
        }

        if (slot.contains("description") && slot["description"].is_string()) {
          m_slots[i].description = slot["description"];
        }

        if (slot.contains("enabled") && slot["enabled"].is_boolean()) {
          m_slots[i].enabled = slot["enabled"];
        }
      }
    }

    // Load UI settings
    if (config.contains("uiSettings")) {
      const auto& uiSettings = config["uiSettings"];
      if (uiSettings.contains("visibleSlotCount") && uiSettings["visibleSlotCount"].is_number_integer()) {
        m_visibleSlotCount = (std::min)(static_cast<int>(uiSettings["visibleSlotCount"]), NUM_SLOTS);
      }
    }
  }
  catch (const std::exception& e) {
    // Log error but continue with defaults
    m_machineOps.LogError("Failed to load script runner configuration: " + std::string(e.what()));
  }
}

void ScriptRunner::SaveConfiguration() {
  try {
    // Create script directory if it doesn't exist
    std::filesystem::path scriptDir = std::filesystem::path(CONFIG_FILE).parent_path();
    if (!std::filesystem::exists(scriptDir)) {
      std::filesystem::create_directories(scriptDir);
    }

    json config;
    json slotsJson = json::array();

    // Save slot configurations
    for (int i = 0; i < NUM_SLOTS; i++) {
      json slot;
      slot["scriptPath"] = m_slots[i].scriptPath;
      slot["displayName"] = m_slots[i].displayName;
      slot["description"] = m_slots[i].description;
      slot["enabled"] = m_slots[i].enabled;
      slotsJson.push_back(slot);
    }

    config["slots"] = slotsJson;
    config["version"] = "1.0";

    // Save UI settings
    json uiSettings;
    uiSettings["visibleSlotCount"] = m_visibleSlotCount;
    config["uiSettings"] = uiSettings;

    std::ofstream file(CONFIG_FILE);
    file << config.dump(4); // Pretty print with 4 spaces
  }
  catch (const std::exception& e) {
    m_machineOps.LogError("Failed to save script runner configuration: " + std::string(e.what()));
  }
}

void ScriptRunner::AssignScriptToSlot(int slotIndex, const std::string& scriptPath, const std::string& displayName) {
  if (slotIndex < 0 || slotIndex >= NUM_SLOTS) {
    return;
  }

  // Stop the slot if it's executing
  if (m_slots[slotIndex].isExecuting) {
    StopSlot(slotIndex);
  }

  m_slots[slotIndex].scriptPath = scriptPath;
  m_slots[slotIndex].displayName = displayName.empty() ?
    std::filesystem::path(scriptPath).filename().stem().string() : displayName;
  m_slots[slotIndex].enabled = true;
  m_slots[slotIndex].lastError.clear();

  // Note: description is set separately in the edit dialog

  SaveConfiguration();
}

void ScriptRunner::ClearSlot(int slotIndex) {
  if (slotIndex < 0 || slotIndex >= NUM_SLOTS) {
    return;
  }

  // Stop the slot if it's executing
  if (m_slots[slotIndex].isExecuting) {
    StopSlot(slotIndex);
  }

  m_slots[slotIndex].scriptPath.clear();
  m_slots[slotIndex].displayName.clear();
  m_slots[slotIndex].description.clear();
  m_slots[slotIndex].enabled = false;
  m_slots[slotIndex].lastError.clear();
  m_slots[slotIndex].lastState = ScriptExecutor::ExecutionState::Idle;

  SaveConfiguration();
}

void ScriptRunner::ExecuteSlot(int slotIndex) {
  if (slotIndex < 0 || slotIndex >= NUM_SLOTS) return;

  auto& slot = m_slots[slotIndex];

  if (!slot.enabled || slot.scriptPath.empty()) {
    return;
  }

  if (slot.isExecuting) {
    return; // Already executing
  }

  // IMPORTANT: Ensure previous executor is properly stopped and cleaned up
  if (slot.executor) {
    // Stop the previous execution if it's still running
    if (slot.executor->GetState() != ScriptExecutor::ExecutionState::Idle &&
      slot.executor->GetState() != ScriptExecutor::ExecutionState::Completed &&
      slot.executor->GetState() != ScriptExecutor::ExecutionState::Error) {
      slot.executor->Stop();
    }

    // Wait up to 3 seconds for the executor to fully stop
    auto startTime = std::chrono::steady_clock::now();
    while (slot.executor->GetState() != ScriptExecutor::ExecutionState::Idle &&
      std::chrono::steady_clock::now() - startTime < std::chrono::milliseconds(50)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Release the previous executor completely before creating a new one
    slot.executor.reset(nullptr);

    // Add a small delay to ensure complete cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // Load script content
  std::string scriptContent;
  if (!LoadScriptContent(slot.scriptPath, scriptContent)) {
    slot.lastError = "Failed to load script file";
    slot.lastState = ScriptExecutor::ExecutionState::Error;
    return;
  }

  // Create new executor
  slot.executor = std::make_unique<ScriptExecutor>(m_machineOps);


  // Set up print handler to use the print viewer
  if (m_printViewer) {
    slot.executor->SetPrintHandler([this](const std::string& message) {
      std::lock_guard<std::mutex> lock(m_printMutex);
      if (m_printViewer) {
        m_printViewer->AddPrintMessage(message);
      }
    });
  }

  // Set callbacks for execution state changes
  slot.executor->SetExecutionCallback([this, slotIndex](ScriptExecutor::ExecutionState state) {
    OnSlotExecutionStateChanged(slotIndex, state);
  });

  slot.executor->SetLogCallback([this, slotIndex](const std::string& message) {
    OnSlotLog(slotIndex, message);
  });

  // Execute script
  if (!slot.executor->ExecuteScript(scriptContent, true)) {
    slot.lastError = "Failed to parse script";
    slot.lastState = ScriptExecutor::ExecutionState::Error;
    slot.isExecuting = false;
    return;
  }

  slot.isExecuting = true;
}

void ScriptRunner::StopSlot(int slotIndex) {
  if (slotIndex < 0 || slotIndex >= NUM_SLOTS) {
    return;
  }

  auto& slot = m_slots[slotIndex];

  if (slot.isExecuting) {
    slot.executor->Stop();
    slot.isExecuting = false;
  }
}

void ScriptRunner::RenderUI() {
  if (!m_isVisible) {
    return;
  }

  ImGuiWindowFlags windowFlags = ImGuiWindowFlags_None;
  ImGui::Begin("Script Runner", &m_isVisible, windowFlags);

  // Track overall progress and status
  int totalExecuting = 0;
  int totalCompleted = 0;
  int totalError = 0;
  float totalProgress = 0.0f;

  for (int i = 0; i < NUM_SLOTS; i++) {
    if (m_slots[i].enabled && !m_slots[i].scriptPath.empty()) {
      if (m_slots[i].isExecuting) {
        totalExecuting++;
        totalProgress += m_slots[i].executor->GetProgress();
      }
      else if (m_slots[i].lastState == ScriptExecutor::ExecutionState::Completed) {
        totalCompleted++;
        totalProgress += 1.0f; // Completed = 100%
      }
      else if (m_slots[i].lastState == ScriptExecutor::ExecutionState::Error) {
        totalError++;
        totalProgress += m_slots[i].executor->GetProgress();
      }
    }
  }

  // Show overall progress bar at the top
  int totalTracked = totalExecuting + totalCompleted + totalError;
  if (totalTracked > 0) {
    float averageProgress = totalProgress / totalTracked;

    // Color the progress bar based on state
    ImVec4 barColor;
    if (totalError > 0) {
      barColor = ImVec4(0.9f, 0.2f, 0.2f, 1.0f); // Red for errors
    }
    else if (totalExecuting > 0) {
      barColor = ImVec4(0.2f, 0.7f, 0.2f, 1.0f); // Green for running
    }
    else {
      barColor = ImVec4(0.2f, 0.5f, 0.9f, 1.0f); // Blue for completed
    }

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);

    // Status text
    std::string statusText;
    if (totalExecuting > 0) {
      statusText = "Running: " + std::to_string(totalExecuting);
    }
    if (totalCompleted > 0) {
      if (!statusText.empty()) statusText += ", ";
      statusText += "Completed: " + std::to_string(totalCompleted);
    }
    if (totalError > 0) {
      if (!statusText.empty()) statusText += ", ";
      statusText += "Error: " + std::to_string(totalError);
    }

    ImGui::Text("Overall Progress (%s):", statusText.c_str());
    ImGui::ProgressBar(averageProgress, ImVec2(-1, 25));

    ImGui::PopStyleColor();
    ImGui::Separator();
  }
  else {
    // Show empty progress bar when no scripts are loaded
    ImGui::Text("Overall Progress (No active scripts):");
    ImGui::ProgressBar(0.0f, ImVec2(-1, 25));
    ImGui::Separator();
  }

  // Header controls
  if (ImGui::Button("Refresh Scripts")) {
    RefreshFileList();
  }

  ImGui::SameLine();
  if (ImGui::Button("Settings")) {
    m_showSettings = !m_showSettings;
  }

  ImGui::SameLine();
  if (ImGui::Button("Reset All")) {
    // Reset all slot states
    for (int i = 0; i < NUM_SLOTS; i++) {
      if (m_slots[i].enabled && !m_slots[i].isExecuting) {
        m_slots[i].lastState = ScriptExecutor::ExecutionState::Idle;
      }
    }
  }

  ImGui::Separator();

  // Rest of the UI remains the same...
  // Settings popup
  if (m_showSettings) {
    ImGui::BeginChild("Settings", ImVec2(ImGui::GetContentRegionAvail().x, 80), true);
    ImGui::Text("Number of slots to display:");
    ImGui::SameLine();
    if (ImGui::InputInt("##SlotCount", &m_visibleSlotCount)) {
      m_visibleSlotCount = std::clamp(m_visibleSlotCount, 1, NUM_SLOTS);
      SaveConfiguration();
    }
    ImGui::EndChild();
    ImGui::Separator();
  }

  // Split layout: slots on left, progress on right
  float totalWidth = ImGui::GetContentRegionAvail().x;
  float slotsWidth = totalWidth * 0.65f;
  float progressWidth = totalWidth * 0.35f - 10.0f; // Minus spacing

  // Left column - Slots
  ImGui::BeginChild("Slots", ImVec2(slotsWidth, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

  for (int i = 0; i < m_visibleSlotCount; i++) {
    ImGui::PushID(i);
    ImGui::BeginChild("SlotFrame", ImVec2(ImGui::GetContentRegionAvail().x, 120), true);

    RenderSlot(i);

    ImGui::EndChild();
    ImGui::PopID();

    ImGui::Spacing();
  }

  ImGui::EndChild();

  ImGui::SameLine();

  // Right column - Progress panel
  ImGui::BeginChild("ProgressPanel", ImVec2(progressWidth, 0), true);
  RenderProgressPanel();
  ImGui::EndChild();

  // Render edit dialog if open
  if (m_showEditDialog) {
    RenderEditDialog();
  }

  ImGui::End();
}



void ScriptRunner::RenderProgressPanel() {
  ImGui::Text("Execution Progress");
  ImGui::Separator();

  // Find all currently executing slots
  std::vector<int> executingSlots;
  for (int i = 0; i < NUM_SLOTS; i++) {
    if (m_slots[i].isExecuting) {
      executingSlots.push_back(i);
    }
  }

  if (executingSlots.empty()) {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No scripts currently executing");
  }
  else {
    // Show progress for each executing slot
    for (int slotIndex : executingSlots) {
      auto& slot = m_slots[slotIndex];

      ImGui::PushID(slotIndex);

      // Slot header with status
      ImGui::Text("Slot %d: %s", slotIndex + 1, slot.displayName.c_str());

      // Status indicator
      ImVec4 statusColor;
      const char* statusText = "";
      switch (slot.executor->GetState()) {
      case ScriptExecutor::ExecutionState::Running:
        statusColor = ImVec4(0.0f, 0.7f, 0.0f, 1.0f);
        statusText = "Running";
        break;
      case ScriptExecutor::ExecutionState::Paused:
        statusColor = ImVec4(0.9f, 0.7f, 0.0f, 1.0f);
        statusText = "Paused";
        break;
      default:
        statusColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        statusText = "Unknown";
      }

      ImGui::SameLine();
      ImGui::TextColored(statusColor, "[%s]", statusText);

      // Progress bar
      float progress = slot.executor->GetProgress();
      ImGui::ProgressBar(progress, ImVec2(-1, 0));

      // Current operation
      ImGui::Text("Line %d/%d",
        slot.executor->GetCurrentLine(),
        slot.executor->GetTotalLines());

      ImGui::TextWrapped("Current: %s", slot.executor->GetCurrentOperation().c_str());

      // Execution time
      if (m_executionStartTimes.find(slotIndex) != m_executionStartTimes.end()) {
        auto duration = std::chrono::steady_clock::now() - m_executionStartTimes[slotIndex];
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        ImGui::Text("Time: %lld seconds", seconds);
      }

      ImGui::Separator();
      ImGui::PopID();
    }
  }

  // Log display section
  ImGui::Spacing();
  ImGui::Text("Execution Log");
  ImGui::Separator();

  // Add a dropdown to select which slot's log to view
  static int selectedLogSlot = -1;
  if (ImGui::BeginCombo("Select Slot Log", selectedLogSlot >= 0 ?
    ("Slot " + std::to_string(selectedLogSlot + 1)).c_str() : "None")) {
    for (int i = 0; i < NUM_SLOTS; i++) {
      if (m_slots[i].enabled) {
        std::string slotLabel = "Slot " + std::to_string(i + 1) + ": " + m_slots[i].displayName;
        if (ImGui::Selectable(slotLabel.c_str(), selectedLogSlot == i)) {
          selectedLogSlot = i;
        }
      }
    }
    ImGui::EndCombo();
  }

  if (selectedLogSlot >= 0 && selectedLogSlot < NUM_SLOTS) {
    // Log display area
    ImGui::BeginChild("LogArea", ImVec2(0, 200), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    // Display the executor's log for the selected slot
    const auto& log = m_slots[selectedLogSlot].executor->GetLog();
    for (const auto& logEntry : log) {
      // Color errors differently
      if (logEntry.find("ERROR:") != std::string::npos) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", logEntry.c_str());
      }
      else {
        ImGui::TextUnformatted(logEntry.c_str());
      }
    }

    // Auto-scroll to bottom
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10) {
      ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
  }
  else {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Select a slot to view its log");
  }
}


void ScriptRunner::RenderSlot(int slotIndex) {
  auto& slot = m_slots[slotIndex];

  // Get available content region
  float contentWidth = ImGui::GetContentRegionAvail().x;
  float buttonWidth = 120.0f; // Width for the execute button
  float spacing = 8.0f;
  float textAreaWidth = contentWidth - buttonWidth - spacing;

  // Calculate row height based on content
  float baseRowHeight = 100.0f; // Minimum height

  // Start horizontal layout
  ImGui::BeginGroup();

  // Left side - Execute Button
  ImGui::BeginChild(("ButtonArea" + std::to_string(slotIndex)).c_str(),
    ImVec2(buttonWidth, baseRowHeight), false);

  if (!slot.scriptPath.empty() && slot.enabled) {
    // Create button text from description (first line or display name)
    std::string buttonText;
    if (!slot.description.empty()) {
      // Use first line of description, truncated if too long
      size_t firstNewline = slot.description.find('\n');
      buttonText = slot.description.substr(0, firstNewline);
      if (buttonText.length() > 15) {
        buttonText = buttonText.substr(0, 12) + "...";
      }
    }
    else {
      // Fall back to display name
      buttonText = slot.displayName;
      if (buttonText.length() > 15) {
        buttonText = buttonText.substr(0, 12) + "...";
      }
    }

    // Color and state based on execution status
    ImVec4 buttonColor;
    if (slot.isExecuting) {
      buttonColor = ImVec4(0.8f, 0.4f, 0.4f, 1.0f); // Red-ish for stop
      buttonText = "STOP\n" + buttonText;
    }
    else {
      buttonColor = ImVec4(0.4f, 0.8f, 0.4f, 1.0f); // Green-ish for run
      buttonText = "RUN\n" + buttonText;
    }

    ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
      ImVec4(buttonColor.x * 1.1f, buttonColor.y * 1.1f,
        buttonColor.z * 1.1f, buttonColor.w));

    // Center the button vertically
    float buttonHeight = baseRowHeight - 10.0f;
    ImGui::SetCursorPosY(5.0f);

    if (ImGui::Button(buttonText.c_str(), ImVec2(buttonWidth - 5, buttonHeight))) {
      if (slot.isExecuting) {
        StopSlot(slotIndex);
      }
      else {
        ExecuteSlot(slotIndex);
      }
    }

    ImGui::PopStyleColor(2);
  }
  else {
    // Disabled or empty slot
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));

    ImGui::SetCursorPosY(5.0f);
    ImGui::Button("EMPTY\nSLOT", ImVec2(buttonWidth - 5, baseRowHeight - 10.0f));

    ImGui::PopStyleColor(2);
  }

  ImGui::EndChild();

  ImGui::SameLine();

  // Right side - Script Information
  ImGui::BeginChild(("InfoArea" + std::to_string(slotIndex)).c_str(),
    ImVec2(textAreaWidth, baseRowHeight), false);

  // Slot header with number and enable checkbox
  ImGui::Text("Slot %d", slotIndex + 1);
  ImGui::SameLine(textAreaWidth - 30);

  if (ImGui::Checkbox(("##enabled" + std::to_string(slotIndex)).c_str(), &slot.enabled)) {
    SaveConfiguration();
  }

  ImGui::Separator();

  // Display name and status
  if (!slot.scriptPath.empty()) {
    // Display name
    ImGui::Text("%s", slot.displayName.c_str());

    // Status indicator
    if (slot.isExecuting) {
      ImVec4 color;
      const char* statusText;

      switch (slot.executor->GetState()) {
      case ScriptExecutor::ExecutionState::Running:
        color = ImVec4(0.0f, 0.7f, 0.0f, 1.0f);
        statusText = "Running";
        break;
      case ScriptExecutor::ExecutionState::Paused:
        color = ImVec4(0.9f, 0.7f, 0.0f, 1.0f);
        statusText = "Paused";
        break;
      default:
        color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        statusText = "Unknown";
        break;
      }

      ImGui::SameLine();
      ImGui::TextColored(color, "[%s - Line %d/%d]", statusText,
        slot.executor->GetCurrentLine(), slot.executor->GetTotalLines());
    }

    // Description (wrap text)
    if (!slot.description.empty()) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
      ImGui::TextWrapped("%s", slot.description.c_str());
      ImGui::PopStyleColor();
    }
  }
  else {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(Empty Slot)");
    ImGui::TextWrapped("Click Edit to assign a script");
  }

  // Edit button at bottom right
  ImGui::SetCursorPos(ImVec2(textAreaWidth - 80, baseRowHeight - 25));
  if (ImGui::Button(("Edit##" + std::to_string(slotIndex)).c_str(), ImVec2(75, 20))) {
    m_editingSlotIndex = slotIndex;
    m_showEditDialog = true;

    // Pre-fill buffers
    strcpy_s(m_editNameBuffer, sizeof(m_editNameBuffer), slot.displayName.c_str());
    strcpy_s(m_editPathBuffer, sizeof(m_editPathBuffer), slot.scriptPath.c_str());
    strcpy_s(m_editDescriptionBuffer, sizeof(m_editDescriptionBuffer), slot.description.c_str());

    RefreshFileList();
  }

  ImGui::EndChild();

  ImGui::EndGroup();
}



void ScriptRunner::RenderEditDialog() {
  ImGui::Begin("Edit Script Slot", &m_showEditDialog);

  if (m_editingSlotIndex >= 0 && m_editingSlotIndex < NUM_SLOTS) {
    ImGui::Text("Editing Slot %d", m_editingSlotIndex + 1);
    ImGui::Separator();

    // Display name
    ImGui::InputText("Display Name", m_editNameBuffer, sizeof(m_editNameBuffer));

    // Script path
    ImGui::InputText("Script Path", m_editPathBuffer, sizeof(m_editPathBuffer));

    // Description
    ImGui::Text("Description:");
    ImGui::InputTextMultiline("##Description", m_editDescriptionBuffer,
      sizeof(m_editDescriptionBuffer),
      ImVec2(0, 80));

    // File browser
    ImGui::Text("Available Scripts:");
    ImGui::BeginChild("ScriptList", ImVec2(0, 150), true);

    for (const auto& script : m_availableScripts) {
      if (ImGui::Selectable(script.c_str(), script == m_editPathBuffer)) {
        strcpy_s(m_editPathBuffer, sizeof(m_editPathBuffer), script.c_str());

        // Auto-fill display name if empty
        if (strlen(m_editNameBuffer) == 0) {
          std::string displayName = std::filesystem::path(script).filename().stem().string();
          strcpy_s(m_editNameBuffer, sizeof(m_editNameBuffer), displayName.c_str());
        }
      }
    }

    ImGui::EndChild();

    ImGui::Separator();

    // Buttons
    if (ImGui::Button("Save", ImVec2(100, 0))) {
      AssignScriptToSlot(m_editingSlotIndex, m_editPathBuffer, m_editNameBuffer);
      m_slots[m_editingSlotIndex].description = m_editDescriptionBuffer;
      SaveConfiguration();
      m_showEditDialog = false;
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear Slot", ImVec2(100, 0))) {
      ClearSlot(m_editingSlotIndex);
      m_showEditDialog = false;
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0))) {
      m_showEditDialog = false;
    }
  }

  ImGui::End();
}

void ScriptRunner::RefreshFileList() {
  m_availableScripts = GetScriptFiles();
}

void ScriptRunner::OnSlotExecutionStateChanged(int slotIndex, ScriptExecutor::ExecutionState state) {
  if (slotIndex < 0 || slotIndex >= NUM_SLOTS) {
    return;
  }

  auto& slot = m_slots[slotIndex];
  slot.lastState = state;

  if (state == ScriptExecutor::ExecutionState::Completed ||
    state == ScriptExecutor::ExecutionState::Error ||
    state == ScriptExecutor::ExecutionState::Idle) {
    slot.isExecuting = false;

    // Calculate execution time
    if (m_executionStartTimes.find(slotIndex) != m_executionStartTimes.end()) {
      auto duration = std::chrono::steady_clock::now() - m_executionStartTimes[slotIndex];
      auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

      m_machineOps.LogInfo("Slot " + std::to_string(slotIndex + 1) +
        " execution time: " + std::to_string(seconds) + " seconds");
    }
  }
}

void ScriptRunner::OnSlotLog(int slotIndex, const std::string& message) {
  // Add slot identifier to log messages
  m_machineOps.LogInfo("[Slot " + std::to_string(slotIndex + 1) + "] " + message);
}

bool ScriptRunner::LoadScriptContent(const std::string& scriptPath, std::string& content) {
  try {
    std::ifstream file(scriptPath);
    if (!file.is_open()) {
      return false;
    }

    content = std::string((std::istreambuf_iterator<char>(file)),
      std::istreambuf_iterator<char>());

    return true;
  }
  catch (const std::exception& e) {
    m_machineOps.LogError("Failed to load script: " + std::string(e.what()));
    return false;
  }
}

std::vector<std::string> ScriptRunner::GetScriptFiles(const std::string& directory) {
  std::vector<std::string> scripts;

  try {
    if (std::filesystem::exists(directory)) {
      for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".aas") {
          scripts.push_back(entry.path().string());
        }
      }
    }

    // Sort alphabetically
    std::sort(scripts.begin(), scripts.end());
  }
  catch (const std::exception& e) {
    m_machineOps.LogError("Error scanning for scripts: " + std::string(e.what()));
  }

  return scripts;
}

