#include "ProcessFilterManager.h"
#include "ProcessRegistry.h"  // Include if using dynamic registry
#include "imgui.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>

ProcessFilterManager::ProcessFilterManager() {
    // Initialize with all processes visible by default
    auto allProcesses = GetAllAvailableProcesses();
    for (const auto& process : allProcesses) {
        m_visibleProcesses.insert(process);
    }

    // Try to load last used preset
    LoadLastUsedPreset();
}

ProcessFilterManager::~ProcessFilterManager() {
    // Save current state as last preset if there's a current preset
    if (!m_currentPresetName.empty()) {
        SaveLastPresetToIni(m_currentPresetName);
    }
}

// NEW: Text filtering implementation
void ProcessFilterManager::SetTextFilter(const std::string& filter) {
    m_textFilter = filter;

    // Auto-apply the filter if it's not empty
    if (!filter.empty()) {
        ApplyTextFilterToVisible(filter);
    }
}

void ProcessFilterManager::ApplyTextFilterToVisible(const std::string& text) {
    if (text.empty()) return;

    // Clear current visible processes
    m_visibleProcesses.clear();

    // Add processes that match the text filter
    auto allProcesses = GetAllAvailableProcesses();
    for (const auto& process : allProcesses) {
        if (ProcessMatchesFilter(process, text)) {
            m_visibleProcesses.insert(process);
        }
    }

    NotifyFilterChanged();
}

void ProcessFilterManager::ShowOnlyNamespace(const std::string& namespacePrefix) {
    m_visibleProcesses.clear();

    auto allProcesses = GetAllAvailableProcesses();
    for (const auto& process : allProcesses) {
        std::string processLower = ToLowerCase(process);
        std::string prefixLower = ToLowerCase(namespacePrefix);

        // Check various namespace patterns
        if (processLower.find(prefixLower) == 0 ||
            processLower.find(prefixLower + "_") != std::string::npos ||
            processLower.find("_" + prefixLower) != std::string::npos) {
            m_visibleProcesses.insert(process);
        }
    }

    NotifyFilterChanged();
}

std::vector<std::string> ProcessFilterManager::GetProcessesMatchingText(const std::string& text) const {
    std::vector<std::string> matches;
    auto allProcesses = GetAllAvailableProcesses();

    for (const auto& process : allProcesses) {
        if (ProcessMatchesFilter(process, text)) {
            matches.push_back(process);
        }
    }

    return matches;
}

std::vector<std::string> ProcessFilterManager::GetAvailableNamespaces() const {
    std::set<std::string> namespaces;
    auto allProcesses = GetAllAvailableProcesses();

    for (const auto& process : allProcesses) {
        std::string ns = ExtractNamespace(process);
        if (!ns.empty()) {
            namespaces.insert(ns);
        }
    }

    return std::vector<std::string>(namespaces.begin(), namespaces.end());
}

void ProcessFilterManager::ClearTextFilter() {
    m_textFilter.clear();
    NotifyFilterChanged();
}

// Enhanced UI rendering with text filter
void ProcessFilterManager::RenderFilterWindow(bool* showWindow) {
    if (!showWindow || !*showWindow) return;

    ImGui::SetNextWindowSize(ImVec2(600, 700), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Process Filter Configuration", showWindow)) {
        // Current preset info
        ImGui::Text("Current Preset: %s", m_currentPresetName.empty() ? "None" : m_currentPresetName.c_str());
        ImGui::Separator();

        // NEW: Text filter section
        ImGui::Text("Text Filter:");
        static char textFilterBuffer[256] = "";

        // Initialize buffer with current filter
        if (strlen(textFilterBuffer) == 0 && !m_textFilter.empty()) {
            strncpy(textFilterBuffer, m_textFilter.c_str(), sizeof(textFilterBuffer) - 1);
        }

        ImGui::SetNextItemWidth(200);
        if (ImGui::InputText("Filter Text", textFilterBuffer, sizeof(textFilterBuffer))) {
            SetTextFilter(std::string(textFilterBuffer));
        }

        ImGui::SameLine();
        if (ImGui::Button("Apply Filter")) {
            ApplyTextFilterToVisible(std::string(textFilterBuffer));
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear Filter")) {
            memset(textFilterBuffer, 0, sizeof(textFilterBuffer));
            ClearTextFilter();
        }

        // NEW: Quick namespace buttons
        ImGui::Text("Quick Namespace Filters:");

        if (ImGui::Button("UAA")) {
            ShowOnlyNamespace("UAA");
        }
        ImGui::SameLine();
        if (ImGui::Button("SAA")) {
            ShowOnlyNamespace("SAA");
        }
        ImGui::SameLine();
        if (ImGui::Button("Pick")) {
            ShowOnlyNamespace("Pick");
        }
        ImGui::SameLine();
        if (ImGui::Button("Dispense")) {
            ShowOnlyNamespace("Dispense");
        }
        ImGui::SameLine();
        if (ImGui::Button("Maintenance")) {
            ShowOnlyNamespace("Maintenance");
        }

        // Show detected namespaces
        auto namespaces = GetAvailableNamespaces();
        if (!namespaces.empty()) {
            ImGui::Text("Available Namespaces:");
            ImGui::SameLine();
            for (size_t i = 0; i < namespaces.size() && i < 5; ++i) {
                if (i > 0) ImGui::SameLine();
                if (ImGui::SmallButton(namespaces[i].c_str())) {
                    ShowOnlyNamespace(namespaces[i]);
                }
            }
        }

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

        // Process visibility toggles with search highlighting
        auto allProcesses = GetAllAvailableProcesses();
        ImGui::Text("Process Visibility (%zu of %zu visible):",
            m_visibleProcesses.size(), allProcesses.size());

        // Show match count if filter is active
        if (!m_textFilter.empty()) {
            auto matches = GetProcessesMatchingText(m_textFilter);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
                "(%zu matches for '%s')", matches.size(), m_textFilter.c_str());
        }

        // Control buttons
        if (ImGui::Button("Show All")) {
            for (const auto& process : allProcesses) {
                SetProcessVisible(process, true);
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Hide All")) {
            m_visibleProcesses.clear();
            NotifyFilterChanged();
        }

        ImGui::SameLine();
        if (ImGui::Button("Show Core Only")) {
            m_visibleProcesses.clear();

            // Try to use registry if available
            try {
                auto& registry = ProcessRegistry::GetInstance();
                auto coreProcesses = registry.GetProcessesByCategory("Core");
                if (!coreProcesses.empty()) {
                    for (const auto& process : coreProcesses) {
                        SetProcessVisible(process, true);
                    }
                }
                else {
                    // Fallback: show processes containing "core" or "initial"
                    for (const auto& process : allProcesses) {
                        std::string lower = ToLowerCase(process);
                        if (lower.find("core") != std::string::npos ||
                            lower.find("initial") != std::string::npos) {
                            SetProcessVisible(process, true);
                        }
                    }
                }
            }
            catch (...) {
                // Registry not available, use fallback
                for (const auto& process : allProcesses) {
                    std::string lower = ToLowerCase(process);
                    if (lower.find("core") != std::string::npos ||
                        lower.find("initial") != std::string::npos) {
                        SetProcessVisible(process, true);
                    }
                }
            }
        }

        // Process list with highlighting
        if (ImGui::BeginChild("ProcessToggles", ImVec2(0, 300), true)) {
            for (const auto& process : allProcesses) {
                bool visible = IsProcessVisible(process);

                // Highlight if matches current filter
                bool matchesFilter = m_textFilter.empty() || ProcessMatchesFilter(process, m_textFilter);
                if (!m_textFilter.empty() && matchesFilter) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                }

                if (ImGui::Checkbox(process.c_str(), &visible)) {
                    SetProcessVisible(process, visible);
                }

                if (!m_textFilter.empty() && matchesFilter) {
                    ImGui::PopStyleColor();
                }
            }
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

// Existing methods (keep your current implementations)
std::vector<std::string> ProcessFilterManager::GetFilteredProcessList() const {
    std::vector<std::string> filtered;
    auto allProcesses = GetAllAvailableProcesses();

    for (const auto& process : allProcesses) {
        if (IsProcessVisible(process)) {
            filtered.push_back(process);
        }
    }

    return filtered;
}

std::vector<std::string> ProcessFilterManager::GetAllAvailableProcesses() const {
    // Use ProcessRegistry if available, otherwise fallback to hardcoded list
    try {
        auto& registry = ProcessRegistry::GetInstance();
        return registry.GetAllProcessNames();
    }
    catch (...) {
        // Fallback to hardcoded list if registry not available
        return {
            "UAA3_Initialization",
            "UAA3_Probing",
            "UAA3_PickPlaceLeftLens",
            "UAA3_PickPlaceRightLens",
            "UAA3_UVCuring",
            "UAA3_RejectLeftLens",
            "UAA3_RejectRightLens",
            "UAA3_NeedleCalibration",
            "UAA3_DispenseCalibration1",
            "UAA3_DispenseCalibration2",
            "UAA3_DispenseEpoxy1",
            "UAA3_DispenseEpoxy2",
            "SAAS_MaintenanceRoutine",
            "SAAS_Core",
            "SAAS_PickPlaceLeftLens",
            "SAAS_Initial",
            "SAAS_Dispensing",
            "SAAS_DispenseEpoxyLens",
            "SAAS_Curing",
            "SAAS_UVCuring",
            "SAAS_Utility",
            "SAAS_RejectLeftLens"
        };
    }
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

        // NEW: Save text filter if active
        if (!m_textFilter.empty()) {
            presetJson["text_filter"] = m_textFilter;
        }

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
            std::cerr << "Invalid preset file format: " << filepath << std::endl;
            return false;
        }

        // Clear current state
        m_visibleProcesses.clear();
        m_textFilter.clear();

        // Load visible processes
        if (presetJson.contains("visible_processes") && presetJson["visible_processes"].is_array()) {
            for (const auto& process : presetJson["visible_processes"]) {
                if (process.is_string()) {
                    m_visibleProcesses.insert(process.get<std::string>());
                }
            }
        }

        // NEW: Load text filter if present
        if (presetJson.contains("text_filter") && presetJson["text_filter"].is_string()) {
            m_textFilter = presetJson["text_filter"].get<std::string>();
        }

        m_currentPresetName = presetName;
        SaveLastPresetToIni(presetName);
        NotifyFilterChanged();

        std::cout << "Loaded filter preset: " << presetName << " from " << filepath << std::endl;
        return true;

    }
    catch (const std::exception& e) {
        std::cerr << "Error loading preset: " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::string> ProcessFilterManager::GetAvailablePresetFiles() const {
    std::vector<std::string> presets;
    std::string presetsDir = "presets/";

    if (!std::filesystem::exists(presetsDir)) {
        return presets;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(presetsDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                std::string filename = entry.path().stem().string();
                presets.push_back(filename);
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error reading presets directory: " << e.what() << std::endl;
    }

    return presets;
}

bool ProcessFilterManager::DeletePresetFile(const std::string& presetName) {
    try {
        std::string filepath = "presets/" + presetName + ".json";
        if (std::filesystem::exists(filepath)) {
            std::filesystem::remove(filepath);

            // Clear current preset if it was the one we just deleted
            if (m_currentPresetName == presetName) {
                m_currentPresetName.clear();
            }

            std::cout << "Deleted preset: " << presetName << std::endl;
            return true;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error deleting preset: " << e.what() << std::endl;
    }

    return false;
}

void ProcessFilterManager::SaveLastPresetToIni(const std::string& presetName) {
    try {
        std::ofstream iniFile(INI_FILE_PATH);
        if (iniFile.is_open()) {
            iniFile << "[Filter]" << std::endl;
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
                    return line.substr(11); // Remove "LastPreset=" prefix
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

// NEW: Helper method implementations
bool ProcessFilterManager::ProcessMatchesFilter(const std::string& processName, const std::string& filter) const {
    if (filter.empty()) return true;

    std::string processLower = ToLowerCase(processName);
    std::string filterLower = ToLowerCase(filter);

    return processLower.find(filterLower) != std::string::npos;
}

std::string ProcessFilterManager::ExtractNamespace(const std::string& processName) const {
    size_t underscorePos = processName.find('_');
    if (underscorePos != std::string::npos) {
        return processName.substr(0, underscorePos);
    }
    return "";
}

std::string ProcessFilterManager::ToLowerCase(const std::string& str) const {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}