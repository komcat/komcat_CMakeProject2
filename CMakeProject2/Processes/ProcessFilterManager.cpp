#include "ProcessFilterManager.h"
#include "imgui.h"
#include <algorithm>
#include <iostream>

ProcessFilterManager::ProcessFilterManager() {
  // Default: all processes hidden
  m_visibleProcesses.clear();

  // Try to load last used preset
  if (!LoadLastUsedPreset()) {
    std::cout << "No previous preset found. All processes hidden by default." << std::endl;
  }
}

ProcessFilterManager::~ProcessFilterManager() = default;

std::vector<std::string> ProcessFilterManager::GetFilteredProcessList() const {
  std::vector<std::string> filtered;

  for (const auto& process : m_allProcesses) {
    if (IsProcessVisible(process)) {
      filtered.push_back(process);
    }
  }

  return filtered;
}

std::vector<std::string> ProcessFilterManager::GetAllAvailableProcesses() const {
  return m_allProcesses;
}

bool ProcessFilterManager::IsProcessVisible(const std::string& processName) const {
  return m_visibleProcesses.find(processName) != m_visibleProcesses.end();
}

void ProcessFilterManager::SetProcessVisible(const std::string& processName, bool visible) {
  if (visible) {
    m_visibleProcesses.insert(processName);
  }
  else {
    m_visibleProcesses.erase(processName);
  }
  NotifyFilterChanged();
}

void ProcessFilterManager::RenderFilterWindow(bool* showWindow) {
  if (!showWindow || !*showWindow) return;

  ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);

  if (ImGui::Begin("Process Filter Configuration", showWindow)) {

    // Current preset info
    ImGui::Text("Current Preset: %s", m_currentPresetName.empty() ? "None" : m_currentPresetName.c_str());
    ImGui::Separator();

    // Save preset section
    ImGui::Text("Save Custom Preset:");
    static char presetName[256] = "";
    ImGui::SetNextItemWidth(200);
    ImGui::InputText("Preset Name", presetName, sizeof(presetName));

    ImGui::SameLine();
    if (ImGui::Button("Save")) {
      if (strlen(presetName) > 0) {
        SavePresetAs(presetName);
        memset(presetName, 0, sizeof(presetName));
      }
    }

    ImGui::Separator();

    // Load preset section
    ImGui::Text("Load Custom Preset:");
    auto customPresets = GetAvailablePresetFiles();

    if (!customPresets.empty()) {
      static int selectedPresetIndex = 0;
      std::vector<const char*> presetNames;
      for (const auto& preset : customPresets) {
        presetNames.push_back(preset.c_str());
      }

      ImGui::SetNextItemWidth(200);
      ImGui::Combo("##CustomPresets", &selectedPresetIndex, presetNames.data(), presetNames.size());

      ImGui::SameLine();
      if (ImGui::Button("Load")) {
        if (selectedPresetIndex >= 0 && selectedPresetIndex < customPresets.size()) {
          LoadPresetFromFile(customPresets[selectedPresetIndex]);
        }
      }

      ImGui::SameLine();
      if (ImGui::Button("Delete")) {
        if (selectedPresetIndex >= 0 && selectedPresetIndex < customPresets.size()) {
          DeletePresetFile(customPresets[selectedPresetIndex]);
        }
      }
    }
    else {
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No custom presets available");
    }

    ImGui::Separator();

    // Process visibility toggles
    ImGui::Text("Process Visibility (%zu visible):", m_visibleProcesses.size());

    if (ImGui::BeginChild("ProcessToggles", ImVec2(0, 300), true)) {
      for (const auto& process : m_allProcesses) {
        bool visible = IsProcessVisible(process);
        if (ImGui::Checkbox(process.c_str(), &visible)) {
          SetProcessVisible(process, visible);
        }
      }
    }
    ImGui::EndChild();

    ImGui::Separator();

    // Quick actions
    if (ImGui::Button("Show All")) {
      for (const auto& process : m_allProcesses) {
        SetProcessVisible(process, true);
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("Hide All")) {
      m_visibleProcesses.clear();
      NotifyFilterChanged();
    }
  }

  ImGui::End();
}

void ProcessFilterManager::SavePresetAs(const std::string& presetName) {
  try {
    // Create JSON structure
    nlohmann::json presetJson;
    presetJson["file_type"] = "filter_preset";
    presetJson["preset_name"] = presetName;
    presetJson["version"] = "1.0";
    presetJson["created_date"] = GetCurrentTimestamp();

    // Save visible processes as array
    nlohmann::json visibleArray = nlohmann::json::array();
    for (const auto& process : m_visibleProcesses) {
      visibleArray.push_back(process);
    }
    presetJson["visible_processes"] = visibleArray;

    // Create presets directory if it doesn't exist
    std::string presetsDir = "presets/";
    if (!std::filesystem::exists(presetsDir)) {
      std::filesystem::create_directory(presetsDir);
    }

    // Save to JSON file
    std::string filepath = presetsDir + presetName + ".json";
    std::ofstream outFile(filepath);
    if (!outFile.is_open()) {
      std::cerr << "Cannot save preset to: " << filepath << std::endl;
      return;
    }

    outFile << std::setw(2) << presetJson << std::endl;
    outFile.close();

    m_currentPresetName = presetName;
    SaveLastPresetToIni(presetName);

    std::cout << "Saved filter preset: " << presetName << " to " << filepath << std::endl;

  }
  catch (const std::exception& e) {
    std::cerr << "Error saving preset: " << e.what() << std::endl;
  }
}

bool ProcessFilterManager::LoadPresetFromFile(const std::string& presetName) {
  try {
    std::string filepath = "presets/" + presetName + ".json";
    std::ifstream inFile(filepath);
    if (!inFile.is_open()) {
      std::cerr << "Cannot load preset from: " << filepath << std::endl;
      return false;
    }

    nlohmann::json presetJson;
    inFile >> presetJson;
    inFile.close();

    // Validate file type
    if (!presetJson.contains("file_type") || presetJson["file_type"] != "filter_preset") {
      std::cerr << "Invalid preset file format" << std::endl;
      return false;
    }

    // Load visible processes
    if (presetJson.contains("visible_processes") && presetJson["visible_processes"].is_array()) {
      m_visibleProcesses.clear();

      for (const auto& processName : presetJson["visible_processes"]) {
        if (processName.is_string()) {
          m_visibleProcesses.insert(processName.get<std::string>());
        }
      }

      m_currentPresetName = presetName;
      SaveLastPresetToIni(presetName);
      NotifyFilterChanged();

      std::cout << "Loaded filter preset: " << presetName << " (" << m_visibleProcesses.size() << " processes visible)" << std::endl;
      return true;
    }

    return false;

  }
  catch (const std::exception& e) {
    std::cerr << "Error loading preset: " << e.what() << std::endl;
    return false;
  }
}

std::vector<std::string> ProcessFilterManager::GetAvailablePresetFiles() const {
  std::vector<std::string> presetFiles;
  std::string presetsDir = "presets/";

  try {
    if (std::filesystem::exists(presetsDir)) {
      for (const auto& entry : std::filesystem::directory_iterator(presetsDir)) {
        if (entry.path().extension() == ".json") {
          std::string filename = entry.path().stem().string();
          presetFiles.push_back(filename);
        }
      }

      std::sort(presetFiles.begin(), presetFiles.end());
    }
  }
  catch (const std::exception& e) {
    std::cerr << "Error scanning presets directory: " << e.what() << std::endl;
  }

  return presetFiles;
}

bool ProcessFilterManager::DeletePresetFile(const std::string& presetName) {
  try {
    std::string filepath = "presets/" + presetName + ".json";
    if (std::filesystem::exists(filepath)) {
      std::filesystem::remove(filepath);

      // If we deleted the current preset, clear it
      if (m_currentPresetName == presetName) {
        m_currentPresetName = "";
        SaveLastPresetToIni("");
      }

      std::cout << "Deleted preset file: " << filepath << std::endl;
      return true;
    }
    return false;
  }
  catch (const std::exception& e) {
    std::cerr << "Error deleting preset file: " << e.what() << std::endl;
    return false;
  }
}

void ProcessFilterManager::SaveLastPresetToIni(const std::string& presetName) {
  try {
    std::ofstream iniFile(INI_FILE_PATH);
    if (iniFile.is_open()) {
      iniFile << "[FilterSettings]" << std::endl;
      iniFile << "LastPreset=" << presetName << std::endl;
      iniFile.close();
    }
  }
  catch (const std::exception& e) {
    std::cerr << "Error saving INI file: " << e.what() << std::endl;
  }
}

std::string ProcessFilterManager::LoadLastPresetFromIni() {
  try {
    std::ifstream iniFile(INI_FILE_PATH);
    if (iniFile.is_open()) {
      std::string line;
      while (std::getline(iniFile, line)) {
        if (line.find("LastPreset=") == 0) {
          std::string presetName = line.substr(11); // Remove "LastPreset="
          iniFile.close();
          return presetName;
        }
      }
      iniFile.close();
    }
  }
  catch (const std::exception& e) {
    std::cerr << "Error loading INI file: " << e.what() << std::endl;
  }

  return "";
}

bool ProcessFilterManager::LoadLastUsedPreset() {
  std::string lastPreset = LoadLastPresetFromIni();
  if (!lastPreset.empty()) {
    return LoadPresetFromFile(lastPreset);
  }
  return false;
}

void ProcessFilterManager::NotifyFilterChanged() {
  if (m_onFilterChanged) {
    m_onFilterChanged();
  }
}

std::string ProcessFilterManager::GetCurrentTimestamp() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  return ss.str();
}