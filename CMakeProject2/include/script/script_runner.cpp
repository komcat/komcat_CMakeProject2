// script_runner.cpp
#include "include/script/script_runner.h"
#include "imgui.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <algorithm>
#include <iomanip>

using json = nlohmann::json;

ScriptRunner::ScriptRunner(MachineOperations& machineOps)
  : m_isVisible(false),
  m_name("Script Runner"),
  m_machineOps(machineOps),
  m_showEditDialog(false),
  m_editingSlotIndex(-1)
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
  if (slotIndex < 0 || slotIndex >= NUM_SLOTS) {
    return;
  }

  auto& slot = m_slots[slotIndex];

  if (!slot.enabled || slot.scriptPath.empty()) {
    return;
  }

  if (slot.isExecuting) {
    m_machineOps.LogWarning("Slot " + std::to_string(slotIndex + 1) + " is already executing");
    return;
  }

  // Load the script content
  std::string scriptContent;
  if (!LoadScriptContent(slot.scriptPath, scriptContent)) {
    slot.lastError = "Failed to load script file";
    return;
  }

  // Execute the script
  slot.lastError.clear();
  slot.isExecuting = true;
  m_executionStartTimes[slotIndex] = std::chrono::steady_clock::now();

  if (!slot.executor->ExecuteScript(scriptContent, true)) {
    slot.isExecuting = false;
    slot.lastError = "Failed to start script execution";
    slot.lastState = ScriptExecutor::ExecutionState::Error;
  }
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

  // Header controls
  if (ImGui::Button("Refresh Scripts")) {
    RefreshFileList();
  }

  ImGui::SameLine();
  if (ImGui::Button("Settings")) {
    m_showSettings = !m_showSettings;
  }

  ImGui::Separator();

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

  // Single column layout for slots
  ImGui::BeginChild("Slots", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

  for (int i = 0; i < m_visibleSlotCount; i++) {
    ImGui::PushID(i);
    ImGui::BeginChild("SlotFrame", ImVec2(ImGui::GetContentRegionAvail().x, 120), true);

    RenderSlot(i);

    ImGui::EndChild();
    ImGui::PopID();

    ImGui::Spacing();
  }

  ImGui::EndChild();

  // Render edit dialog if open
  if (m_showEditDialog) {
    RenderEditDialog();
  }

  ImGui::End();
}

void ScriptRunner::RenderSlot(int slotIndex) {
  auto& slot = m_slots[slotIndex];

  // Slot header with number and enable checkbox
  ImGui::Text("Slot %d", slotIndex + 1);
  ImGui::SameLine(ImGui::GetWindowWidth() - 30);

  if (ImGui::Checkbox(("##enabled" + std::to_string(slotIndex)).c_str(), &slot.enabled)) {
    SaveConfiguration();
  }

  ImGui::Separator();

  // Display name
  if (!slot.scriptPath.empty()) {
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Assuming default font
    ImGui::Text("%s", slot.displayName.c_str());
    ImGui::PopFont();

    // Description (wrap text)
    if (!slot.description.empty()) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
      ImGui::TextWrapped("%s", slot.description.c_str());
      ImGui::PopStyleColor();
    }

    // Status
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

      ImGui::TextColored(color, "%s", statusText);

      // Progress bar
      float progress = slot.executor->GetProgress();
      ImGui::ProgressBar(progress, ImVec2(-1, 0));
    }
  }
  else {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(Empty Slot)");
    ImGui::TextWrapped("Click Edit to assign a script");
  }

  // Control buttons at bottom
  ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 30);

  if (!slot.scriptPath.empty() && slot.enabled) {
    if (slot.isExecuting) {
      if (ImGui::Button(("Stop##" + std::to_string(slotIndex)).c_str(), ImVec2(80, 0))) {
        StopSlot(slotIndex);
      }
    }
    else {
      if (ImGui::Button(("Run##" + std::to_string(slotIndex)).c_str(), ImVec2(80, 0))) {
        ExecuteSlot(slotIndex);
      }
    }
  }

  ImGui::SameLine();
  if (ImGui::Button(("Edit##" + std::to_string(slotIndex)).c_str(), ImVec2(80, 0))) {
    m_editingSlotIndex = slotIndex;
    m_showEditDialog = true;

    // Pre-fill buffers
    strcpy_s(m_editNameBuffer, sizeof(m_editNameBuffer), slot.displayName.c_str());
    strcpy_s(m_editPathBuffer, sizeof(m_editPathBuffer), slot.scriptPath.c_str());
    strcpy_s(m_editDescriptionBuffer, sizeof(m_editDescriptionBuffer), slot.description.c_str());

    RefreshFileList();
  }
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