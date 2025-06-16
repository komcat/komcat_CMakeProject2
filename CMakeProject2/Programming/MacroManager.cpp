#ifdef _WIN32
#include <windows.h>
#else
#include <filesystem>
#endif

#include "MacroManager.h"
#include "MachineBlockUI.h"
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include "imgui.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include "MacroEditState.h"




MacroManager::MacroManager() {
  m_isExecuting = false;
  m_nextMacroId = 1;
  m_currentProgramIndex = -1;
  m_currentExecutingProgram = "";
  m_debugMode = true;
  m_forceRescanMacros = false;


  // ADD THIS LINE:
  InitializeFeedbackUI();
  // Auto-scan for programs on startup
  ScanForPrograms();
}

MacroManager::~MacroManager() {
  // Clean up any running execution
}

// Fix 4: Updated AddProgram() - Store actual filename
void MacroManager::AddProgram(const std::string& programName, const std::string& filePath) {
  SavedProgram program;
  program.name = programName;

  // ✅ FIX: Extract just filename without path and extension for filePath storage
  std::string filename = filePath;
  size_t lastSlash = filename.find_last_of("/\\");
  if (lastSlash != std::string::npos) {
    filename = filename.substr(lastSlash + 1);
  }
  size_t lastDot = filename.find_last_of(".");
  if (lastDot != std::string::npos) {
    filename = filename.substr(0, lastDot);
  }

  program.filePath = filename;  // ✅ Store just filename for ProgramManager compatibility
  program.description = "";

  m_savedPrograms[programName] = program;

  if (m_debugMode) {
    printf("[MACRO DEBUG] [ADD] Added program: %s -> %s\n", programName.c_str(), filename.c_str());
  }
}


// Fix 1: Updated ScanForPrograms() - Only scan programs/ folder, use actual filenames
void MacroManager::ScanForPrograms() {
  if (m_debugMode) {
    printf("[MACRO DEBUG] [SCAN] Scanning for program files in programs/ folder...\n");
  }

  // Save manually added programs before clearing
  std::map<std::string, SavedProgram> manualPrograms = m_savedPrograms;
  m_savedPrograms.clear();

  // ✅ FIX: Only scan programs/ folder (remove other paths)
  std::vector<std::string> searchPaths = {
      "programs/"
  };

  int foundCount = 0;

  for (const auto& path : searchPaths) {
    if (m_debugMode) {
      printf("[MACRO DEBUG] [SCAN] Scanning directory: %s\n", path.c_str());
    }

#ifdef _WIN32
    // Windows directory scanning
    std::string searchPattern = path + "*.json";
    WIN32_FIND_DATAA findFileData;
    HANDLE hFind = FindFirstFileA(searchPattern.c_str(), &findFileData);

    if (hFind != INVALID_HANDLE_VALUE) {
      do {
        if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
          std::string fileName = findFileData.cFileName;
          std::string fullPath = path + fileName;

          // ✅ FIX: Use actual filename without extension as key and display name
          std::string fileNameOnly = fileName;
          size_t lastDot = fileNameOnly.find_last_of(".");
          if (lastDot != std::string::npos) {
            fileNameOnly = fileNameOnly.substr(0, lastDot);
          }

          SavedProgram program;
          program.name = fileNameOnly;           // ✅ Use actual filename (no fancy display name)
          program.filePath = fileNameOnly;       // ✅ Store just filename for ProgramManager compatibility
          program.description = "Program from " + path;

          m_savedPrograms[fileNameOnly] = program;
          foundCount++;

          if (m_debugMode) {
            printf("[MACRO DEBUG] [FOUND] Found program: %s -> %s\n",
              fileNameOnly.c_str(), fullPath.c_str());
          }
        }
      } while (FindNextFileA(hFind, &findFileData) != 0);
      FindClose(hFind);
    }
#else
    // Linux/Unix directory scanning
    try {
      if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
          if (entry.is_regular_file() && entry.path().extension() == ".json") {
            std::string fileName = entry.path().filename().string();

            // ✅ FIX: Use actual filename without extension
            std::string fileNameOnly = fileName;
            size_t lastDot = fileNameOnly.find_last_of(".");
            if (lastDot != std::string::npos) {
              fileNameOnly = fileNameOnly.substr(0, lastDot);
            }

            SavedProgram program;
            program.name = fileNameOnly;           // ✅ Use actual filename
            program.filePath = fileNameOnly;       // ✅ Store just filename
            program.description = "Program from " + path;

            m_savedPrograms[fileNameOnly] = program;
            foundCount++;

            if (m_debugMode) {
              printf("[MACRO DEBUG] [FOUND] Found program: %s -> %s\n",
                fileNameOnly.c_str(), entry.path().string().c_str());
            }
          }
        }
      }
    }
    catch (const std::exception& e) {
      if (m_debugMode) {
        printf("[MACRO DEBUG] [ERROR] Error scanning %s: %s\n", path.c_str(), e.what());
      }
    }
#endif
  }

  // Add back any manually added programs
  for (const auto& [name, program] : manualPrograms) {
    if (m_savedPrograms.find(name) == m_savedPrograms.end()) {
      m_savedPrograms[name] = program;
      if (m_debugMode) {
        printf("[MACRO DEBUG] [RESTORE] Restored manual program: %s\n", name.c_str());
      }
    }
  }

  if (m_debugMode) {
    printf("[MACRO DEBUG] [SCAN] Scan complete. Found %d programs, total available: %zu\n",
      foundCount, m_savedPrograms.size());
  }
}


bool MacroManager::CreateMacro(const std::string& macroName, const std::string& description) {
  if (m_macros.find(macroName) != m_macros.end()) {
    printf("[MACRO] Macro '%s' already exists\n", macroName.c_str());
    return false;
  }

  MacroProgram macro;
  macro.id = m_nextMacroId++;
  macro.name = macroName;
  macro.description = description;
  macro.programs.clear();

  m_macros[macroName] = macro;
  printf("[MACRO] Created macro: %s\n", macroName.c_str());
  return true;
}


// 3. UPDATED: Override existing methods to update edit state
bool MacroManager::AddProgramToMacro(const std::string& macroName, const std::string& programName) {
  auto macroIt = m_macros.find(macroName);
  if (macroIt == m_macros.end()) {
    printf("[MACRO] Macro '%s' not found\n", macroName.c_str());
    return false;
  }

  auto programIt = m_savedPrograms.find(programName);
  if (programIt == m_savedPrograms.end()) {
    printf("[MACRO] Program '%s' not found\n", programName.c_str());
    return false;
  }

  macroIt->second.programs.push_back(programIt->second);

  // Update edit state
  if (m_macroEditStates.find(macroName) != m_macroEditStates.end()) {
    m_macroEditStates[macroName].SetProgramCount(macroIt->second.programs.size());
  }

  printf("[MACRO] Added program '%s' to macro '%s'\n", programName.c_str(), macroName.c_str());
  return true;
}

bool MacroManager::RemoveProgramFromMacro(const std::string& macroName, int index) {
  auto macroIt = m_macros.find(macroName);
  if (macroIt == m_macros.end()) {
    printf("[MACRO] Macro '%s' not found\n", macroName.c_str());
    return false;
  }

  if (index < 0 || index >= macroIt->second.programs.size()) {
    printf("[MACRO] Invalid program index %d\n", index);
    return false;
  }

  macroIt->second.programs.erase(macroIt->second.programs.begin() + index);

  // Update edit state
  if (m_macroEditStates.find(macroName) != m_macroEditStates.end()) {
    m_macroEditStates[macroName].SetProgramCount(macroIt->second.programs.size());
  }

  printf("[MACRO] Removed program at index %d from macro '%s'\n", index, macroName.c_str());
  return true;
}

// 6. ADD BETTER LOGGING TO ExecuteMacro method:
void MacroManager::ExecuteMacro(const std::string& macroName, std::function<void(bool)> callback) {
  auto it = m_macros.find(macroName);
  if (it == m_macros.end()) {
    printf("[MACRO ERROR] Macro '%s' not found\n", macroName.c_str());
    AddExecutionLog("ERROR: Macro '" + macroName + "' not found");
    if (callback) callback(false);
    return;
  }

  if (m_isExecuting) {
    printf("[MACRO ERROR] Another macro is already executing\n");
    AddExecutionLog("ERROR: Another macro is already executing");
    if (callback) callback(false);
    return;
  }

  const MacroProgram& macro = it->second;
  m_isExecuting = true;
  m_currentMacro = macroName;
  m_currentProgramIndex = -1;
  m_currentExecutingProgram = "";

  // These should now show up in UI
  AddExecutionLog("=== STARTING MACRO: " + macroName + " ===");
  AddExecutionLog("Programs to execute: " + std::to_string(macro.programs.size()));

  printf("[MACRO] Starting execution of macro '%s' with %zu programs:\n",
    macroName.c_str(), macro.programs.size());

  std::thread executionThread([this, macro, callback]() {
    bool success = true;

    for (size_t i = 0; i < macro.programs.size(); i++) {
      if (!m_isExecuting) {
        AddExecutionLog("STOPPED: Execution stopped by user at program " + std::to_string(i + 1));
        success = false;
        break;
      }

      const SavedProgram& program = macro.programs[i];
      m_currentProgramIndex = static_cast<int>(i);
      m_currentExecutingProgram = program.name;

      // These messages should now appear in UI
      AddExecutionLog("Loading program " + std::to_string(i + 1) + "/" +
        std::to_string(macro.programs.size()) + ": " + program.name);

      m_blockUI->LoadProgram(program.name);

      AddExecutionLog("Program loaded, starting execution...");
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      bool programSuccess = false;
      bool executionComplete = false;

      // Execute with callback
      m_blockUI->ExecuteProgramAsSequence([&programSuccess, &executionComplete, &program, this](bool result) {
        programSuccess = result;
        executionComplete = true;

        std::string resultMsg = result ? "SUCCESS" : "FAILED";
        AddExecutionLog("Program '" + program.name + "' completed: " + resultMsg);
      });

      // Wait for completion with periodic updates
      int timeout = 0;
      const int maxTimeout = 300;

      while (!executionComplete && timeout < maxTimeout && m_isExecuting) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        timeout++;

        if (timeout % 50 == 0) {
          AddExecutionLog("Still executing '" + program.name + "' (" +
            std::to_string(timeout / 10) + "s elapsed)");
        }
      }

      if (!executionComplete) {
        AddExecutionLog("TIMEOUT: Program '" + program.name + "' timed out");
        success = false;
        break;
      }

      if (!programSuccess) {
        AddExecutionLog("FAILED: Program '" + program.name + "' execution failed");
        success = false;
        break;
      }

      AddExecutionLog("SUCCESS: Program '" + program.name + "' completed successfully");

      if (i < macro.programs.size() - 1) {
        AddExecutionLog("Waiting before next program...");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      }
    }

    // Final cleanup
    m_isExecuting = false;
    m_currentProgramIndex = -1;
    m_currentExecutingProgram = "";

    if (success) {
      AddExecutionLog("=== MACRO COMPLETED SUCCESSFULLY ===");
    }
    else {
      AddExecutionLog("=== MACRO EXECUTION FAILED ===");
    }

    if (callback) {
      callback(success);
    }
  });

  executionThread.detach();
}


// Fix 5: Updated SaveMacro() - Save with actual filenames
void MacroManager::SaveMacro(const std::string& macroName, const std::string& filePath) {
  auto it = m_macros.find(macroName);
  if (it == m_macros.end()) {
    printf("[MACRO] Error: Macro '%s' not found for saving\n", macroName.c_str());
    return;
  }

  try {
    const MacroProgram& macro = it->second;

    nlohmann::json macroJson = {
      {"file_type", "macro"},
      {"macro_id", macro.id},
      {"name", macro.name},
      {"description", macro.description},
      {"programs", nlohmann::json::array()}
    };

    // ✅ FIX: Save programs with actual filenames
    for (const auto& program : macro.programs) {
      macroJson["programs"].push_back({
        {"name", program.name},
        {"file_path", program.filePath},  // ✅ Use actual filename (not full path)
        {"description", program.description}
        });
    }

    std::ofstream file(filePath);
    if (!file.is_open()) {
      printf("[MACRO] Error: Could not create macro file at %s\n", filePath.c_str());
      printf("       Try manually creating the 'macros/' folder\n");
      return;
    }

    file << macroJson.dump(2);
    file.close();
    printf("[MACRO] Saved macro '%s' (ID: %d) to %s\n", macroName.c_str(), macro.id, filePath.c_str());

    m_forceRescanMacros = true;
  }
  catch (const std::exception& e) {
    printf("[MACRO] Error saving macro: %s\n", e.what());
  }
}



void MacroManager::LoadMacro(const std::string& filePath) {
  try {
    std::ifstream file(filePath);
    nlohmann::json macroJson;
    file >> macroJson;
    file.close();

    // VALIDATE FILE TYPE
    if (macroJson.contains("file_type")) {
      std::string fileType = macroJson["file_type"];
      if (fileType != "macro") {
        printf("[MACRO] [WARNING] File '%s' is not a macro file (type: %s)\n",
          filePath.c_str(), fileType.c_str());
        return;
      }
    }
    else {
      // Check if it looks like a program file
      if (macroJson.contains("blocks") || macroJson.contains("program_id")) {
        printf("[MACRO] [WARNING] File '%s' appears to be a program file, not a macro\n",
          filePath.c_str());
        return;
      }

      // Legacy macro file without file_type - allow it but warn
      if (m_debugMode) {
        printf("[MACRO DEBUG] [LOAD] Loading legacy macro file without file_type identifier\n");
      }
    }

    MacroProgram macro;
    macro.name = macroJson["name"];
    macro.description = macroJson["description"];

    // Use macro_id if available, otherwise fall back to id
    if (macroJson.contains("macro_id")) {
      macro.id = macroJson["macro_id"];
    }
    else {
      macro.id = macroJson.contains("id") ? macroJson["id"] : m_nextMacroId++;
    }

    if (macroJson.contains("programs")) {
      for (const auto& programJson : macroJson["programs"]) {
        SavedProgram program;
        program.name = programJson["name"];
        program.filePath = programJson["file_path"];
        program.description = programJson.contains("description") ?
          programJson["description"] : "";
        macro.programs.push_back(program);
      }
    }

    m_macros[macro.name] = macro;
    printf("[MACRO] Loaded macro '%s' (ID: %d) with %zu programs\n",
      macro.name.c_str(), macro.id, macro.programs.size());

  }
  catch (const std::exception& e) {
    printf("[MACRO] Error loading macro: %s\n", e.what());
  }
}


// Fix 2: Updated ExecuteSingleProgram() - Use actual filename directly
void MacroManager::ExecuteSingleProgram(const std::string& programName) {
  auto programIt = m_savedPrograms.find(programName);
  if (programIt == m_savedPrograms.end()) {
    printf("[MACRO DEBUG] [ERROR] Single program '%s' not found\n", programName.c_str());
    return;
  }

  if (!m_blockUI) {
    printf("[MACRO DEBUG] [ERROR] MachineBlockUI not set for single program execution\n");
    return;
  }

  if (m_isExecuting) {
    printf("[MACRO DEBUG] [ERROR] Cannot execute single program '%s' - macro is currently running: '%s'\n",
      programName.c_str(), m_currentMacro.c_str());
    return;
  }

  printf("[MACRO DEBUG] [START] Executing single program: %s\n", programName.c_str());
  printf("[MACRO DEBUG] [INFO] File path: %s\n", programIt->second.filePath.c_str());

  printf("[MACRO DEBUG] [LOAD] Loading program into BlockUI...\n");

  // ✅ FIX: Use actual filename directly (not display name)
  m_blockUI->LoadProgram(programIt->second.filePath);

  printf("[MACRO DEBUG] [WAIT] Waiting 100ms for UI update...\n");
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  printf("[MACRO DEBUG] [RUN] Starting program execution...\n");
  m_blockUI->ExecuteProgramAsSequence([programName](bool success) {
    if (success) {
      printf("[MACRO DEBUG] [COMPLETE] [SUCCESS] Single program '%s' completed successfully\n", programName.c_str());
    }
    else {
      printf("[MACRO DEBUG] [COMPLETE] [FAILED] Single program '%s' failed\n", programName.c_str());
    }
  });
}

// UPDATED: Graceful StopExecution method
void MacroManager::StopExecution() {
  if (m_isExecuting) {
    printf("[MACRO DEBUG] [GRACEFUL_STOP] User requested graceful stop of macro: %s\n", m_currentMacro.c_str());
    printf("[MACRO DEBUG] [INFO] Currently executing program %d: %s\n",
      m_currentProgramIndex + 1, m_currentExecutingProgram.c_str());
    printf("[MACRO DEBUG] [INFO] Will stop after current program completes\n");

    m_stopRequested = true;  // Set the graceful stop flag
    // Note: m_isExecuting stays true until the current program finishes
  }
  else {
    printf("[MACRO DEBUG] [WARNING] Stop requested but no macro is currently executing\n");
  }
}

std::vector<std::string> MacroManager::GetMacroNames() const {
  std::vector<std::string> names;
  for (const auto& pair : m_macros) {
    names.push_back(pair.first);
  }
  return names;
}

std::vector<std::string> MacroManager::GetProgramNames() const {
  std::vector<std::string> names;
  for (const auto& pair : m_savedPrograms) {
    names.push_back(pair.first);
  }
  return names;
}

MacroProgram* MacroManager::GetMacro(const std::string& macroName) {
  auto it = m_macros.find(macroName);
  return (it != m_macros.end()) ? &it->second : nullptr;
}

bool MacroManager::DeleteMacro(const std::string& macroName) {
  auto it = m_macros.find(macroName);
  if (it != m_macros.end()) {
    m_macros.erase(it);
    printf("[MACRO] Deleted macro '%s'\n", macroName.c_str());
    return true;
  }
  return false;
}

void MacroManager::SetMachineBlockUI(MachineBlockUI* blockUI) {
  m_blockUI = blockUI;
}

bool MacroManager::IsExecuting() const {
  return m_isExecuting;
}

std::string MacroManager::GetCurrentMacro() const {
  return m_currentMacro;
}


// Add this new method to MacroManager class

void MacroManager::ScanForMacroFiles(std::vector<std::string>& availableMacroFiles) {
  availableMacroFiles.clear();

  std::vector<std::string> searchPaths = {
      "macros/",
      "programs/macros/",
      "Programs/Macros/",
      "Programs/",
      "./"
  };

  for (const auto& path : searchPaths) {
    if (m_debugMode) {
      printf("[MACRO DEBUG] [SCAN] Scanning for macro files in: %s\n", path.c_str());
    }

    // Method 1: Try to scan directory for all .json files
    std::vector<std::string> foundFiles;

#ifdef _WIN32
    // Windows directory scanning
    std::string searchPattern = path + "*.json";
    WIN32_FIND_DATAA findFileData;
    HANDLE hFind = FindFirstFileA(searchPattern.c_str(), &findFileData);

    if (hFind != INVALID_HANDLE_VALUE) {
      do {
        if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
          std::string fileName = findFileData.cFileName;
          foundFiles.push_back(fileName);
          if (m_debugMode) {
            printf("[MACRO DEBUG] [SCAN] Found JSON file: %s\n", fileName.c_str());
          }
        }
      } while (FindNextFileA(hFind, &findFileData) != 0);
      FindClose(hFind);
    }
#else
    // Linux/Mac directory scanning using C++17 filesystem
    try {
      if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
          if (entry.is_regular_file() && entry.path().extension() == ".json") {
            std::string fileName = entry.path().filename().string();
            foundFiles.push_back(fileName);
            if (m_debugMode) {
              printf("[MACRO DEBUG] [SCAN] Found JSON file: %s\n", fileName.c_str());
            }
          }
        }
      }
    }
    catch (const std::exception& e) {
      if (m_debugMode) {
        printf("[MACRO DEBUG] [SCAN] Error scanning directory %s: %s\n", path.c_str(), e.what());
      }
    }
#endif

    // Fallback: If directory scanning fails, use hardcoded patterns
    if (foundFiles.empty()) {
      if (m_debugMode) {
        printf("[MACRO DEBUG] [SCAN] Directory scanning failed, using fallback for: %s\n", path.c_str());
      }

      std::vector<std::string> possibleMacroNames = {
          "macro1", "macro2", "macro3", "test_macro", "example_macro",
          "workflow_macro", "production_macro", "calibration_macro",
          "initialization_macro", "setup_macro", "demo_macro",
          "macro slide and stop"  // Add the one we just created
      };

      for (const auto& macroName : possibleMacroNames) {
        std::string testFile = macroName + "_macro.json";
        std::string fullPath = path + testFile;
        std::ifstream file(fullPath);
        if (file.good()) {
          foundFiles.push_back(testFile);
          if (m_debugMode) {
            printf("[MACRO DEBUG] [SCAN] Fallback found: %s\n", testFile.c_str());
          }
        }
        file.close();
      }

      std::vector<std::string> standaloneMacros = {
          "main_workflow.json",
          "test_sequence.json",
          "production_line.json"
      };

      for (const auto& fileName : standaloneMacros) {
        std::string fullPath = path + fileName;
        std::ifstream file(fullPath);
        if (file.good()) {
          foundFiles.push_back(fileName);
          if (m_debugMode) {
            printf("[MACRO DEBUG] [SCAN] Fallback standalone found: %s\n", fileName.c_str());
          }
        }
        file.close();
      }
    }

    // Process all found files and filter by type
    for (const auto& fileName : foundFiles) {
      std::string fullPath = path + fileName;

      // Check if this is actually a macro file
      try {
        std::ifstream file(fullPath);
        if (file.good()) {
          nlohmann::json testJson;
          file >> testJson;
          file.close();

          // Check file type
          bool isMacroFile = false;

          if (testJson.contains("file_type")) {
            // New format with file_type identifier
            std::string fileType = testJson["file_type"];
            isMacroFile = (fileType == "macro");

            if (m_debugMode && !isMacroFile) {
              printf("[MACRO DEBUG] [SKIP] Skipping %s (type: %s)\n",
                fileName.c_str(), fileType.c_str());
            }
          }
          else {
            // Legacy format - check structure
            bool hasBlocks = testJson.contains("blocks");
            bool hasProgramId = testJson.contains("program_id");
            bool hasPrograms = testJson.contains("programs");
            bool hasName = testJson.contains("name");

            // Likely a macro if it has programs array and name, but no blocks
            isMacroFile = (hasPrograms && hasName && !hasBlocks && !hasProgramId);

            if (m_debugMode && !isMacroFile) {
              printf("[MACRO DEBUG] [SKIP] Skipping %s (appears to be program file)\n",
                fileName.c_str());
            }
          }

          if (isMacroFile) {
            availableMacroFiles.push_back(fullPath);
            if (m_debugMode) {
              printf("[MACRO DEBUG] [FOUND] Found macro file: %s\n", fullPath.c_str());
            }
          }
        }
      }
      catch (const std::exception& e) {
        if (m_debugMode) {
          printf("[MACRO DEBUG] [ERROR] Could not parse %s: %s\n",
            fileName.c_str(), e.what());
        }
      }
    }
  }

  if (m_debugMode) {
    printf("[MACRO DEBUG] [SCAN] Total macro files found: %zu\n", availableMacroFiles.size());
  }
}

// Implementation in MacroManager.cpp:

void MacroManager::RenderDebugSection() {
  ImGui::Checkbox("[DEBUG] Debug Mode", &m_debugMode);
  ImGui::SameLine();
  ImGui::TextDisabled("(shows detailed execution info)");

  if (m_debugMode && ImGui::CollapsingHeader("[DEBUG] Debug Information", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::BeginChild("DebugInfo", ImVec2(-1, 100), true);

    ImGui::Columns(2, "DebugColumns", true);

    ImGui::Text("Execution State:");
    ImGui::Text("  Is Executing: %s", m_isExecuting ? "YES" : "NO");
    ImGui::Text("  Current Macro: %s", m_currentMacro.empty() ? "None" : m_currentMacro.c_str());
    ImGui::Text("  Program Index: %d", m_currentProgramIndex);
    ImGui::Text("  Current Program: %s", m_currentExecutingProgram.empty() ? "None" : m_currentExecutingProgram.c_str());

    ImGui::NextColumn();

    ImGui::Text("System State:");
    ImGui::Text("  BlockUI Connected: %s", m_blockUI ? "YES" : "NO");
    ImGui::Text("  Total Macros: %zu", m_macros.size());
    ImGui::Text("  Available Programs: %zu", m_savedPrograms.size());

    static std::string lastExecutionLog = "No executions yet";
    ImGui::Text("Last Action: %s", lastExecutionLog.c_str());

    ImGui::Columns(1);
    ImGui::EndChild();
    ImGui::Separator();
  }
}


// UPDATED: Enhanced execution status rendering to show graceful stop state
void MacroManager::RenderExecutionStatus() {
  if (m_isExecuting) {
    // Change background color if stop is requested
    ImVec4 bgColor = m_stopRequested ?
      ImVec4(0.6f, 0.3f, 0.1f, 0.8f) :  // Darker orange for "stopping"
      ImVec4(0.2f, 0.2f, 0.2f, 0.8f);   // Dark gray for normal execution

    ImGui::PushStyleColor(ImGuiCol_ChildBg, bgColor);
    ImGui::BeginChild("ExecutionStatus", ImVec2(-1, 80), true);

    if (m_stopRequested) {
      ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "[STOPPING] MACRO: %s", m_currentMacro.c_str());
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "Finishing current program, then will stop...");
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "[EXECUTING] MACRO: %s", m_currentMacro.c_str());
    }

    if (m_currentProgramIndex >= 0) {
      auto macroIt = m_macros.find(m_currentMacro);
      if (macroIt != m_macros.end() && m_currentProgramIndex < macroIt->second.programs.size()) {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 1.0f, 1.0f),
          "Program %d/%zu: %s",
          m_currentProgramIndex + 1,
          macroIt->second.programs.size(),
          m_currentExecutingProgram.c_str());
      }
    }

    // Show appropriate button based on state
    if (m_stopRequested) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
      ImGui::Button("[STOPPING] Please wait...");
      ImGui::PopStyleColor();
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Stop requested - waiting for current program to complete");
      }
    }
    else {
      if (ImGui::Button("[STOP] Stop After Current Program")) {
        StopExecution();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Stop gracefully after current program completes");
      }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Spacing();
  }
}

void MacroManager::RenderCreateMacroSection() {
  if (ImGui::CollapsingHeader("[+] Create New Macro", ImGuiTreeNodeFlags_DefaultOpen)) {
    static char macroName[256] = "";
    static char macroDesc[512] = "";

    ImGui::Columns(2, "CreateMacroColumns", false);
    ImGui::SetColumnWidth(0, 100);

    ImGui::Text("Name:");
    ImGui::NextColumn();
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##MacroName", macroName, sizeof(macroName));
    ImGui::PopItemWidth();
    ImGui::NextColumn();

    ImGui::Text("Description:");
    ImGui::NextColumn();
    ImGui::PushItemWidth(-1);
    ImGui::InputTextMultiline("##MacroDesc", macroDesc, sizeof(macroDesc), ImVec2(-1, 60));
    ImGui::PopItemWidth();
    ImGui::Columns(1);

    ImGui::Spacing();
    if (ImGui::Button("Create Macro", ImVec2(120, 30)) && strlen(macroName) > 0) {
      if (CreateMacro(macroName, macroDesc)) {
        memset(macroName, 0, sizeof(macroName));
        memset(macroDesc, 0, sizeof(macroDesc));
      }
    }
    ImGui::Separator();
  }
}




// 1. FIX: Replace this section in RenderEditMacrosSection()
void MacroManager::RenderEditMacrosSection() {
  if (ImGui::CollapsingHeader("[EDIT] Edit Macros", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (m_macros.empty()) {
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No macros created yet. Create one above!");
      return;
    }

    // FIXED: Use class members instead of static variables
    for (auto& [name, macro] : m_macros) {
      // Initialize states for this macro if not exists
      if (m_macroEditStates.find(name) == m_macroEditStates.end()) {
        m_macroEditStates[name].SetProgramCount(macro.programs.size());
        m_editModeStates[name] = false; // Default to execution mode
        m_selectedProgramIndices[name] = 0;
      }

      MacroEditState& editState = m_macroEditStates[name];
      bool& isEditMode = m_editModeStates[name]; // FIXED: Use class member
      editState.SetProgramCount(macro.programs.size());

      ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.4f, 0.8f, 0.4f));
      bool isOpen = ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
      ImGui::PopStyleColor();

      if (isOpen) {
        ImGui::Indent();

        // Macro info
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Description: %s", macro.description.c_str());
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "Programs: %zu", macro.programs.size());
        ImGui::Spacing();

        if (!macro.programs.empty()) {
          // === EDIT MODE TOGGLE ===
          ImGui::Spacing();

          if (ImGui::Checkbox(("Edit Mode##" + name).c_str(), &isEditMode)) {
            // Toggle edit mode - clear selections when switching
            editState.SetMode(ExecutionMode::SINGLE_PROGRAM);
          }

          ImGui::SameLine();
          ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            isEditMode ? "(Click X to remove, dropdown to add)" : "(Click programs to select for execution)");

          ImGui::Spacing();

          // === EXECUTION MODE SELECTION ===
          if (!isEditMode) {
            ImGui::Text("Execution Mode:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.3f, 1.0f), "[%s]", editState.GetModeDescription().c_str());
            ImGui::Spacing();
          }

          // === VISUAL PROGRAM FLOW ===
          ImGui::Text("Program Flow:");
          ImGui::BeginChild(("Flow_" + name).c_str(), ImVec2(-1, 120), true, ImGuiWindowFlags_HorizontalScrollbar);

          for (size_t i = 0; i < macro.programs.size(); i++) {
            const auto& program = macro.programs[i];
            bool isSelected = editState.IsProgramSelected(i);

            if (isEditMode) {
              // === EDIT MODE: Show remove buttons ===
              ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.3f, 0.3f, 0.8f));
              ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.4f, 0.4f, 1.0f));

              std::string buttonId = "X" + program.name + "##" + std::to_string(i);
              if (ImGui::Button(buttonId.c_str(), ImVec2(140, 40))) {
                // Remove this program
                RemoveProgramFromMacro(name, i);
                editState.SetProgramCount(macro.programs.size() - 1);
                ImGui::PopStyleColor(2);
                break; // Exit loop since we modified the container
              }

              // RIGHT-CLICK MENU for edit mode
              if (ImGui::BeginPopupContextItem()) {
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Program: %s", program.name.c_str());
                ImGui::Separator();

                if (ImGui::MenuItem("[Del] Delete Program")) {
                  RemoveProgramFromMacro(name, i);
                  editState.SetProgramCount(macro.programs.size() - 1);
                  ImGui::EndPopup();
                  ImGui::PopStyleColor(2);
                  break; // Exit loop since we modified the container
                }

                if (ImGui::MenuItem("Program Info")) {
                  // Show program details
                  printf("[MACRO INFO] Program: %s | File: %s | Description: %s\n",
                    program.name.c_str(), program.filePath.c_str(), program.description.c_str());
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Test Run")) {
                  ExecuteSingleProgram(program.name);
                }

                ImGui::EndPopup();
              }

            }
            else {
              // === EXECUTION MODE: Show selection ===
              if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.7f, 0.3f, 0.9f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
              }
              else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 0.6f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
              }

              std::string buttonId = program.name + "##" + std::to_string(i);
              if (ImGui::Button(buttonId.c_str(), ImVec2(120, 40))) {
                editState.SelectSingleProgram(i);
              }

              // Right click menu
              if (ImGui::BeginPopupContextItem()) {
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Program: %s", program.name.c_str());
                ImGui::Separator();

                if (ImGui::MenuItem("Run This Only")) {
                  editState.SelectSingleProgram(i);
                  ExecuteSingleProgram(program.name);
                }

                if (ImGui::MenuItem("Run From Here to End")) {
                  editState.SetRunFromIndex(i);
                  // Give visual feedback
                  AddExecutionLog("Set to run from '" + program.name + "' to end");
                }

                if (ImGui::MenuItem("☑️ Toggle Selection")) {
                  editState.ToggleProgramSelection(i);
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Program Info")) {
                  printf("[MACRO INFO] Program: %s | File: %s\n",
                    program.name.c_str(), program.filePath.c_str());
                }

                ImGui::EndPopup();
              }
            }

            ImGui::PopStyleColor(2);

            // Arrow between programs
            if (i < macro.programs.size() - 1) {
              ImGui::SameLine();
              ImGui::Text("→");
              ImGui::SameLine();
            }
          }
          ImGui::EndChild();

          // === ADD PROGRAM SECTION ===
          ImGui::Spacing();
          ImGui::Text("Add Program:");

          int& selectedProgram = m_selectedProgramIndices[name];

          std::vector<std::string> availablePrograms;
          for (const auto& [progName, savedProg] : m_savedPrograms) {
            bool alreadyInMacro = false;
            for (const auto& existingProg : macro.programs) {
              if (existingProg.name == progName) {
                alreadyInMacro = true;
                break;
              }
            }
            if (!alreadyInMacro) {
              availablePrograms.push_back(progName);
            }
          }

          if (!availablePrograms.empty()) {
            if (selectedProgram >= availablePrograms.size()) {
              selectedProgram = 0;
            }

            ImGui::PushItemWidth(200);
            std::vector<const char*> items;
            for (const auto& progName : availablePrograms) {
              items.push_back(progName.c_str());
            }

            ImGui::Combo(("##AddProgram_" + name).c_str(), &selectedProgram, items.data(), items.size());
            ImGui::PopItemWidth();

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 0.8f));
            if (ImGui::Button(("Add##" + name).c_str(), ImVec2(60, 25))) {
              if (selectedProgram >= 0 && selectedProgram < availablePrograms.size()) {
                AddProgramToMacro(name, availablePrograms[selectedProgram]);
                editState.SetProgramCount(macro.programs.size() + 1);
              }
            }
            ImGui::PopStyleColor();
          }
          else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "All available programs already added");
          }

          // === EXECUTION CONTROLS (only in execution mode) ===
          if (!isEditMode) {
            ImGui::Spacing();
            ImGui::Text("Execution Mode:");

            if (ImGui::Button("Single", ImVec2(60, 25))) {
              // Keep current selection - shows only selected program
            }
            ImGui::SameLine();

            if (ImGui::Button("All", ImVec2(60, 25))) {
              editState.SelectAllPrograms();
            }
            ImGui::SameLine();

            // IMPROVED: Better "Run From" button with clearer text
            if (ImGui::Button("From►", ImVec2(60, 25))) {
              // Set to run from the first selected program
              auto executionIndices = editState.GetExecutionIndices(macro.programs.size());
              if (!executionIndices.empty()) {
                editState.SetRunFromIndex(executionIndices[0]);
              }
              else {
                editState.SetRunFromIndex(0); // Default to first program
              }
            }
            if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip("Run from clicked program to the end\n1. Click a program block\n2. Click 'From►' to run from that point");
            }

            ImGui::SameLine();

            if (ImGui::Button("Custom", ImVec2(60, 25))) {
              editState.SetMode(ExecutionMode::CUSTOM_SELECTION);
            }
            if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip("Toggle individual programs on/off");
            }

            ImGui::Spacing();

            // === HELPFUL INSTRUCTIONS ===
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "How to use:");
            ImGui::BulletText("Single: Click a program → only that one runs");
            ImGui::BulletText("All: Run all programs in sequence");
            ImGui::BulletText("From►: Click a program → run from there to end");
            ImGui::BulletText("Custom: Right-click programs for advanced options");

            ImGui::Separator();

            // === EXECUTION PREVIEW ===
            auto executionIndices = editState.GetExecutionIndices(macro.programs.size());
            if (!executionIndices.empty()) {
              ImGui::Text("Will Execute:");
              ImGui::SameLine();

              std::string previewText;
              for (size_t i = 0; i < executionIndices.size(); i++) {
                if (i > 0) previewText += " → ";
                if (executionIndices[i] < macro.programs.size()) {
                  previewText += macro.programs[executionIndices[i]].name;
                }
                if (previewText.length() > 80) {
                  previewText += "...";
                  break;
                }
              }

              ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.9f, 1.0f), "%s", previewText.c_str());
              ImGui::Text("(%zu programs)", executionIndices.size());

              ImGui::Spacing();
              if (ImGui::Button(("Execute Selected##" + name).c_str(), ImVec2(140, 30))) {
                ExecuteMacroWithIndices(name, executionIndices);
              }

              if (m_isExecuting && m_currentMacro == name) {
                ImGui::SameLine();
                if (ImGui::Button("Stop", ImVec2(60, 30))) {
                  StopExecution();
                }
              }
            }
            else {
              ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), "No programs selected for execution");
            }
          }
          else {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.3f, 1.0f), "Edit Mode: Add programs above, click ✖ to remove");

            // === SAVE MACRO BUTTON (prominent in edit mode) ===
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.5f, 0.8f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.6f, 0.9f, 1.0f));
            if (ImGui::Button(("Save Macro##" + name).c_str(), ImVec2(120, 35))) {
              std::string saveFileName = "macros/" + name + "_macro.json";
              SaveMacro(name, saveFileName);

              // Show feedback
              AddExecutionLog("Saved macro '" + name + "' to " + saveFileName);
            }
            ImGui::PopStyleColor(2);

            if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip("Save current macro configuration\nFile: macros/%s_macro.json", name.c_str());
            }

            // === ADDITIONAL EDIT MODE ACTIONS ===
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.4f, 0.8f, 0.8f));
            if (ImGui::Button(("Copy Macro##" + name).c_str(), ImVec2(120, 35))) {
              std::string copyName = name + "_copy";
              int copyNumber = 1;

              // Find unique name
              while (m_macros.find(copyName) != m_macros.end()) {
                copyName = name + "_copy" + std::to_string(copyNumber);
                copyNumber++;
              }

              if (CreateMacro(copyName, macro.description + " (Copy)")) {
                // Copy all programs
                for (const auto& program : macro.programs) {
                  AddProgramToMacro(copyName, program.name);
                }
                AddExecutionLog("Created copy: '" + copyName + "'");
              }
            }
            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip("Create a copy of this macro");
            }

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 0.9f));
            if (ImGui::Button(("[Del] Delete Macro##" + name).c_str(), ImVec2(120, 35))) {
              // Add confirmation popup logic here if needed
              DeleteMacro(name);
              AddExecutionLog("Deleted macro: '" + name + "'");
              ImGui::PopStyleColor(2);
              ImGui::Unindent();
              ImGui::TreePop();
              break; // Exit loop since we deleted the macro
            }
            ImGui::PopStyleColor(2);

            if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip("⚠️ Delete this macro permanently");
            }
          }
        }

        ImGui::Unindent();
        ImGui::TreePop();
      }

      ImGui::Spacing();
    }
  }
}

// NEW: Edit mode management methods
bool MacroManager::IsEditMode(const std::string& macroName) const {
  auto it = m_editModeStates.find(macroName);
  return (it != m_editModeStates.end()) ? it->second : false;
}

void MacroManager::SetEditMode(const std::string& macroName, bool editMode) {
  m_editModeStates[macroName] = editMode;

  // Clear selections when switching modes
  if (m_macroEditStates.find(macroName) != m_macroEditStates.end()) {
    m_macroEditStates[macroName].SetMode(ExecutionMode::SINGLE_PROGRAM);
  }
}

MacroEditState& MacroManager::GetEditState(const std::string& macroName) {
  // Initialize if doesn't exist
  if (m_macroEditStates.find(macroName) == m_macroEditStates.end()) {
    auto macroIt = m_macros.find(macroName);
    size_t programCount = (macroIt != m_macros.end()) ? macroIt->second.programs.size() : 0;
    m_macroEditStates[macroName].SetProgramCount(programCount);
  }

  return m_macroEditStates[macroName];
}

// New method to execute macro with specific program indices
void MacroManager::ExecuteMacroWithIndices(const std::string& macroName, const std::vector<int>& indices) {
  auto it = m_macros.find(macroName);
  if (it == m_macros.end()) {
    printf("[MACRO ERROR] Macro '%s' not found\n", macroName.c_str());
    return;
  }

  if (m_isExecuting) {
    printf("[MACRO ERROR] Another macro is already executing\n");
    return;
  }

  const MacroProgram& macro = it->second;

  // Create subset of programs to execute
  std::vector<SavedProgram> programsToExecute;
  for (int index : indices) {
    if (index >= 0 && index < macro.programs.size()) {
      programsToExecute.push_back(macro.programs[index]);
    }
  }

  if (programsToExecute.empty()) {
    printf("[MACRO ERROR] No valid programs selected\n");
    return;
  }

  // Execute the selected programs (similar to ExecuteMacro but with custom list)
  m_isExecuting = true;
  m_currentMacro = macroName;

  AddExecutionLog("=== EXECUTING SELECTED PROGRAMS ===");
  AddExecutionLog("Macro: " + macroName);
  AddExecutionLog("Programs: " + std::to_string(programsToExecute.size()) + "/" + std::to_string(macro.programs.size()));

  std::thread executionThread([this, programsToExecute, macroName]() {
    bool success = true;

    for (size_t i = 0; i < programsToExecute.size(); i++) {
      if (!m_isExecuting) {
        AddExecutionLog("STOPPED: Execution stopped by user");
        success = false;
        break;
      }

      const SavedProgram& program = programsToExecute[i];
      AddExecutionLog("Executing " + std::to_string(i + 1) + "/" +
        std::to_string(programsToExecute.size()) + ": " + program.name);

      m_blockUI->LoadProgram(program.name);
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      bool programSuccess = false;
      bool executionComplete = false;

      m_blockUI->ExecuteProgramAsSequence([&programSuccess, &executionComplete](bool result) {
        programSuccess = result;
        executionComplete = true;
      });

      // Wait for completion (simplified timeout logic)
      int timeout = 0;
      while (!executionComplete && timeout < 300 && m_isExecuting) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        timeout++;
      }

      if (!m_isExecuting || !executionComplete || !programSuccess) {
        success = false;
        break;
      }
    }

    m_isExecuting = false;
    m_currentMacro = "";

    std::string result = success ? "SUCCESS" : "FAILED";
    AddExecutionLog("=== EXECUTION " + result + " ===");
  });

  executionThread.detach();
}

void MacroManager::RenderLoadMacroSection() {
  if (ImGui::CollapsingHeader("[LOAD] Load Macro")) {
    static char filename[256] = "";
    static std::vector<std::string> availableMacroFiles;
    static bool filesScanned = false;

    // Simplified scanning call
    if (!filesScanned || m_forceRescanMacros) {
      filesScanned = false;
      m_forceRescanMacros = false;

      // Call the separated scanning method
      ScanForMacroFiles(availableMacroFiles);

      filesScanned = true;
    }

    // Manual filename input
    ImGui::Columns(3, "LoadMacroColumns", false);
    ImGui::SetColumnWidth(0, 80);
    ImGui::SetColumnWidth(1, 300);

    ImGui::Text("File:");
    ImGui::NextColumn();
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##LoadFilename", filename, sizeof(filename));
    ImGui::PopItemWidth();
    ImGui::NextColumn();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.8f, 0.8f));
    if (ImGui::Button("[LOAD] Load", ImVec2(80, 25)) && strlen(filename) > 0) {
      LoadMacro(filename);
    }
    ImGui::PopStyleColor();
    ImGui::Columns(1);

    // Available macro files dropdown
    if (!availableMacroFiles.empty()) {
      ImGui::Spacing();
      ImGui::Text("Available Macro Files:");

      static int selectedMacroFile = 0;
      std::vector<const char*> macroFileItems;
      std::vector<std::string> displayNames;

      for (const auto& filePath : availableMacroFiles) {
        size_t lastSlash = filePath.find_last_of("/\\");
        std::string displayName = (lastSlash != std::string::npos) ?
          filePath.substr(lastSlash + 1) : filePath;
        displayNames.push_back(displayName);
        macroFileItems.push_back(displayNames.back().c_str());
      }

      ImGui::Columns(2, "MacroFileColumns", false);
      ImGui::SetColumnWidth(0, 300);

      ImGui::PushItemWidth(-1);
      ImGui::Combo("##MacroFileSelect", &selectedMacroFile, macroFileItems.data(), macroFileItems.size());
      ImGui::PopItemWidth();
      ImGui::NextColumn();

      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 0.8f));
      if (ImGui::Button("[LOAD] Load Selected", ImVec2(-1, 25))) {
        if (selectedMacroFile >= 0 && selectedMacroFile < availableMacroFiles.size()) {
          LoadMacro(availableMacroFiles[selectedMacroFile]);
        }
      }
      ImGui::PopStyleColor();
      ImGui::Columns(1);

      if (selectedMacroFile >= 0 && selectedMacroFile < availableMacroFiles.size()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
          "Path: %s", availableMacroFiles[selectedMacroFile].c_str());
      }
    }
    else {
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.2f, 1.0f), "[!] No macro files found in common directories");
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Searched: macros/, programs/macros/, Programs/");
    }

    ImGui::Spacing();
    if (ImGui::Button("[REFRESH] Refresh File List")) {
      filesScanned = false;  // This will trigger ScanForMacroFiles() on next frame
    }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(rescans directories for new files)");
  }
}

void MacroManager::RenderAvailableProgramsSection() {
  if (ImGui::CollapsingHeader("[PROGRAMS] Available Programs")) {
    if (ImGui::Button("[SCAN] Scan for Programs")) {
      ScanForPrograms();
    }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(scans programs/ folder)");

    ImGui::Spacing();

    if (m_savedPrograms.empty()) {
      ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1.0f), "[!] No programs available. Save some programs first!");
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Or click 'Scan for Programs' to find existing files.");
    }
    else {
      ImGui::BeginChild("ProgramsList", ImVec2(-1, 120), true);

      ImGui::Columns(4, "ProgramsColumns", true);
      ImGui::SetColumnWidth(0, 150);
      ImGui::SetColumnWidth(1, 250);
      ImGui::SetColumnWidth(2, 60);
      ImGui::SetColumnWidth(3, 40);

      ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Program Name");
      ImGui::NextColumn();
      ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "File Path");
      ImGui::NextColumn();
      ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Execute");
      ImGui::NextColumn();
      ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Test");
      ImGui::NextColumn();
      ImGui::Separator();

      for (const auto& [name, program] : m_savedPrograms) {
        ImGui::Text("[PROG] %s", name.c_str());
        ImGui::NextColumn();

        std::ifstream testFile(program.filePath);
        bool fileExists = testFile.good();
        testFile.close();

        if (fileExists) {
          ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", program.filePath.c_str());
        }
        else {
          ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1.0f), "[X] %s", program.filePath.c_str());
        }
        ImGui::NextColumn();

        if (fileExists) {
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 0.7f));
          if (ImGui::SmallButton(("[RUN]##exec_" + name).c_str())) {
            ExecuteSingleProgram(name);
          }
          ImGui::PopStyleColor();
        }
        else {
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 0.5f));
          ImGui::SmallButton("[X]");
          ImGui::PopStyleColor();
        }

        if (ImGui::IsItemHovered()) {
          if (fileExists) {
            ImGui::SetTooltip("Execute '%s'", name.c_str());
          }
          else {
            ImGui::SetTooltip("File not found: %s", program.filePath.c_str());
          }
        }
        ImGui::NextColumn();

        if (fileExists) {
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.8f, 0.7f));
          if (ImGui::SmallButton(("[LOAD]##load_" + name).c_str())) {
            if (m_blockUI) {
              m_blockUI->LoadProgram(name);
              if (m_debugMode) {
                printf("[MACRO DEBUG] [LOAD] Loaded program '%s' into BlockUI for testing\n", name.c_str());
              }
            }
          }
          ImGui::PopStyleColor();
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Load '%s' into Block Programming for editing", name.c_str());
          }
        }
        else {
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 0.5f));
          ImGui::SmallButton("[X]");
          ImGui::PopStyleColor();
        }
        ImGui::NextColumn();

        ImGui::Separator();
      }
      ImGui::Columns(1);
      ImGui::EndChild();
    }
  }
}

// SIMPLIFIED MAIN RenderUI METHOD
void MacroManager::RenderUI() {
  if (!m_showWindow) return;

  ImGui::Begin("Macro Manager", &m_showWindow, ImGuiWindowFlags_None);

  // Render all sections in order
  RenderDebugSection();
  
  RenderCreateMacroSection();
  RenderAvailableProgramsSection();
  RenderLoadMacroSection();
  
  RenderEditMacrosSection();
  // NEW: Render embedded feedback section
  RenderEmbeddedFeedbackSection();
  RenderExecutionStatus();
  ImGui::End();
}



// Simple text-based feedback rendering


// 4. UPDATE RenderEmbeddedFeedbackSection to use display logs:
void MacroManager::RenderEmbeddedFeedbackSection() {
  // CRITICAL: Process pending logs on main thread before rendering
  ProcessPendingLogs();

  if (ImGui::CollapsingHeader("[FEEDBACK] Execution Progress", ImGuiTreeNodeFlags_DefaultOpen)) {

    ImGui::Checkbox("Show Execution Log", &m_showEmbeddedFeedback);
    ImGui::SameLine();
    ImGui::TextDisabled("(Shows real-time execution progress)");

    if (m_showEmbeddedFeedback) {
      ImGui::SameLine();
      if (ImGui::Button("Clear Log")) {
        ClearExecutionLog();
      }

      ImGui::SameLine();
      if (ImGui::Button("Test Log")) {
        AddExecutionLog("=== TEST MACRO EXECUTION ===");
        AddExecutionLog("Loading program 1/3: Test Program 1");
        AddExecutionLog("Program loaded, starting execution...");
        AddExecutionLog("Program 'Test Program 1' completed: SUCCESS");
        AddExecutionLog("=== TEST COMPLETED ===");
      }

      // Show compact status if executing
      if (m_isExecuting && !m_currentExecutingProgram.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f),
          "EXECUTING: %s (%d/%d)",
          m_currentExecutingProgram.c_str(),
          m_currentProgramIndex + 1,
          static_cast<int>(m_macros[m_currentMacro].programs.size())
        );
      }

      // Render execution log
      ImGui::Separator();
      ImGui::Text("Execution Log:");

      ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));

      if (ImGui::BeginChild("ExecutionLog", ImVec2(-1, 150), true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {

        // USE DISPLAY LOGS INSTEAD OF EXECUTION LOGS
        if (m_displayLogs.empty()) {
          ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No execution activity yet...");
        }
        else {
          for (const auto& logEntry : m_displayLogs) {
            // Color-code log entries
            ImVec4 color = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);  // Light Gray

            if (logEntry.find("SUCCESS") != std::string::npos ||
              logEntry.find("completed successfully") != std::string::npos ||
              logEntry.find("COMPLETED SUCCESSFULLY") != std::string::npos) {
              color = ImVec4(0.1f, 0.9f, 0.1f, 1.0f);  // Bright Green
            }
            else if (logEntry.find("FAILED") != std::string::npos ||
              logEntry.find("ERROR") != std::string::npos ||
              logEntry.find("TIMEOUT") != std::string::npos) {
              color = ImVec4(0.9f, 0.1f, 0.1f, 1.0f);  // Bright Red
            }
            else if (logEntry.find("STARTING") != std::string::npos ||
              logEntry.find("STOPPED") != std::string::npos ||
              logEntry.find("CANCELLED") != std::string::npos) {
              color = ImVec4(0.2f, 0.6f, 1.0f, 1.0f);  // Sky Blue
            }
            else if (logEntry.find("Loading") != std::string::npos ||
              logEntry.find("executing") != std::string::npos ||
              logEntry.find("Waiting") != std::string::npos) {
              color = ImVec4(1.0f, 0.9f, 0.1f, 1.0f);  // Gold
            }

            ImGui::TextColored(color, "%s", logEntry.c_str());
          }

          // Auto-scroll to bottom
          if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
          }
        }
      }
      ImGui::EndChild();
      ImGui::PopStyleColor();
    }

    ImGui::Separator();
  }
}



// Simplified initialization - no complex feedback UI needed
void MacroManager::InitializeFeedbackUI() {
  // Just clear the log - no complex UI initialization needed
  ClearExecutionLog();
  // Set the show flag to true by default so logs are visible
  m_showEmbeddedFeedback = true;
  printf("[MACRO DEBUG] InitializeFeedbackUI called - logs should be visible\n");

  AddExecutionLog("Macro Manager feedback system initialized");
}


void MacroManager::AddExecutionLog(const std::string& message) {
  std::string timestampedMessage = "[" + GetCurrentTimeString() + "] " + message;

  // Thread-safe: Add to pending logs from any thread
  {
    std::lock_guard<std::mutex> lock(m_logMutex);
    m_pendingLogs.push_back(timestampedMessage);
  }

  // Always print to console for debugging
  printf("%s\n", timestampedMessage.c_str());
}

// 3. ADD this new method to process pending logs on main thread:
void MacroManager::ProcessPendingLogs() {
  std::lock_guard<std::mutex> lock(m_logMutex);

  // Move pending logs to display logs
  for (const auto& log : m_pendingLogs) {
    m_displayLogs.push_back(log);
  }
  m_pendingLogs.clear();

  // Keep only the last N entries
  if (m_displayLogs.size() > m_maxLogLines) {
    m_displayLogs.erase(m_displayLogs.begin(),
      m_displayLogs.begin() + (m_displayLogs.size() - m_maxLogLines));
  }
}


std::string MacroManager::GetCurrentTimeString() {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    now.time_since_epoch()) % 1000;

  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
  ss << "." << std::setfill('0') << std::setw(3) << ms.count();
  return ss.str();
}

// 5. UPDATE ClearExecutionLog to clear both logs:
void MacroManager::ClearExecutionLog() {
  std::lock_guard<std::mutex> lock(m_logMutex);
  m_pendingLogs.clear();
  m_displayLogs.clear();
  m_executionLog.clear();  // Keep for backward compatibility
}

