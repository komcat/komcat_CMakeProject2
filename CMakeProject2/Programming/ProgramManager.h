
// ProgramManager.h
#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>

struct ProgramInfo {
  std::string filename;
  std::string name;
  std::string description;
  std::string author;
  int blockCount;
  int connectionCount;
  std::string lastModified;
};

class ProgramManager {
private:
  std::string m_programsDirectory = "programs/";
  std::vector<ProgramInfo> m_programList;
  std::string m_currentProgram;

public:
  ProgramManager();

  // Directory management
  void CreateProgramsDirectory();
  void RefreshProgramList();

  // Program operations
  bool SaveProgram(const std::string& filename, const nlohmann::json& programData);
  bool LoadProgram(const std::string& filename, nlohmann::json& programData);
  bool DeleteProgram(const std::string& filename);
  bool DuplicateProgram(const std::string& sourceFile, const std::string& newName);

  // Getters
  const std::vector<ProgramInfo>& GetProgramList() const { return m_programList; }
  const std::string& GetCurrentProgram() const { return m_currentProgram; }

  // UI helpers
  void RenderProgramBrowser();
  void RenderSaveAsDialog();


  // Add callback support for UI integration
  void SetLoadCallback(std::function<void(const std::string&)> callback) {
    m_loadCallback = callback;
  }

  void SetSaveCallback(std::function<void(const std::string&)> callback) {
    m_saveCallback = callback;
  }

  // Setter for current program
 // Setter for current program
  void SetCurrentProgram(const std::string& programName) {
    m_currentProgram = programName;
    printf("[INFO] Current program set to: '%s'\n", programName.c_str());
  }

  // Clear current program (convenience method)
  void ClearCurrentProgram() {
    m_currentProgram = "";
    printf("[INFO] Current program cleared\n");
  }

  // Alternative with validation
  bool SetCurrentProgramWithValidation(const std::string& programName) {
    // Check if program file exists
    std::string filepath = m_programsDirectory + programName + ".json";
    if (std::filesystem::exists(filepath)) {
      m_currentProgram = programName;
      printf("[INFO] Current program set to: %s\n", programName.c_str());
      return true;
    }
    else {
      printf("[WARNING] Program file not found: %s\n", programName.c_str());
      return false;
    }
  }


  std::string GenerateUniqueFilename(const std::string& baseName);

private:
  ProgramInfo ExtractProgramInfo(const std::string& filepath);

  std::function<void(const std::string&)> m_loadCallback;
  std::function<void(const std::string&)> m_saveCallback;
};
