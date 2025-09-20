#include "ProcessFilterManager.h"
#include "ProcessRegistry.h"  // Include if using dynamic registry
#include "imgui.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>

ProcessFilterManager::ProcessFilterManager() {
    // Keep your existing initialization
    auto allProcesses = GetAllAvailableProcesses();
    for (const auto& process : allProcesses) {
        m_visibleProcesses.insert(process);
    }

    // NEW: Load sort numbers
    LoadSortNumbersFromFile();

    // Try to load last used preset
    LoadLastUsedPreset();
}

// NEW: Destructor modification - save sort numbers
ProcessFilterManager::~ProcessFilterManager() {
    // NEW: Save sort numbers
    SaveSortNumbersToFile();

    // Keep your existing preset saving
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

		std::tm timeinfo;
		localtime_s(&timeinfo, &time_t);

    std::stringstream ss;
    ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
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


// NEW: Get sorted and filtered process list
std::vector<std::string> ProcessFilterManager::GetSortedFilteredProcessList() const {
    std::vector<std::string> filtered;
    auto allProcesses = GetAllAvailableProcesses();

    // Get only visible processes
    for (const auto& process : allProcesses) {
        if (IsProcessVisible(process)) {
            filtered.push_back(process);
        }
    }

    // Sort: numbered processes first (by number), then alphabetical
    std::sort(filtered.begin(), filtered.end(), [this](const std::string& a, const std::string& b) {
        return CompareProcessesForSorting(a, b);
        });

    return filtered;
}

// NEW: Sorting comparison logic
bool ProcessFilterManager::CompareProcessesForSorting(const std::string& a, const std::string& b) const {
    auto aSortIt = m_processSortNumbers.find(a);
    auto bSortIt = m_processSortNumbers.find(b);

    bool aHasNumber = (aSortIt != m_processSortNumbers.end());
    bool bHasNumber = (bSortIt != m_processSortNumbers.end());

    if (aHasNumber && bHasNumber) {
        // Both have sort numbers - sort by number
        return aSortIt->second < bSortIt->second;
    }
    else if (aHasNumber && !bHasNumber) {
        // Only 'a' has number - it comes first
        return true;
    }
    else if (!aHasNumber && bHasNumber) {
        // Only 'b' has number - it comes first
        return false;
    }
    else {
        // Neither has number - sort alphabetically
        return a < b;
    }
}

// NEW: Set process sort number
void ProcessFilterManager::SetProcessSortNumber(const std::string& processName, int sortNumber) {
    auto allProcesses = GetAllAvailableProcesses();
    auto it = std::find(allProcesses.begin(), allProcesses.end(), processName);

    if (it != allProcesses.end()) {
        if (sortNumber > 0) {
            m_processSortNumbers[processName] = sortNumber;
        }
        else {
            m_processSortNumbers.erase(processName); // Remove if number <= 0
        }
        SaveSortNumbersToFile();
        NotifyFilterChanged();
    }
}

// NEW: Get process sort number
int ProcessFilterManager::GetProcessSortNumber(const std::string& processName) const {
    auto it = m_processSortNumbers.find(processName);
    return (it != m_processSortNumbers.end()) ? it->second : 0; // 0 means no number assigned
}

// NEW: Remove process sort number
void ProcessFilterManager::RemoveProcessSortNumber(const std::string& processName) {
    m_processSortNumbers.erase(processName);
    SaveSortNumbersToFile();
    NotifyFilterChanged();
}

// NEW: Clear all sort numbers
void ProcessFilterManager::ClearAllSortNumbers() {
    m_processSortNumbers.clear();
    SaveSortNumbersToFile();
    NotifyFilterChanged();
}

// NEW: Assign sequential numbers (1, 2, 3...)
void ProcessFilterManager::AssignSequentialNumbers() {
    auto visibleProcesses = GetSortedFilteredProcessList();

    for (size_t i = 0; i < visibleProcesses.size(); ++i) {
        m_processSortNumbers[visibleProcesses[i]] = static_cast<int>(i + 1);
    }

    SaveSortNumbersToFile();
    NotifyFilterChanged();
}

// NEW: Assign spaced numbers (10, 20, 30...)
void ProcessFilterManager::AssignSpacedNumbers() {
    auto visibleProcesses = GetSortedFilteredProcessList();

    for (size_t i = 0; i < visibleProcesses.size(); ++i) {
        m_processSortNumbers[visibleProcesses[i]] = static_cast<int>((i + 1) * 10);
    }

    SaveSortNumbersToFile();
    NotifyFilterChanged();
}

// NEW: Save sort numbers to file
void ProcessFilterManager::SaveSortNumbersToFile() {
    std::ofstream file("process_button_order.config");
    if (file.is_open()) {
        for (const auto& [processName, sortNumber] : m_processSortNumbers) {
            file << processName << "=" << sortNumber << std::endl;
        }
        file.close();
    }
}

// NEW: Load sort numbers from file
void ProcessFilterManager::LoadSortNumbersFromFile() {
    std::ifstream file("process_button_order.config");
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            size_t equalPos = line.find('=');
            if (equalPos != std::string::npos) {
                std::string processName = line.substr(0, equalPos);
                try {
                    int sortNumber = std::stoi(line.substr(equalPos + 1));
                    m_processSortNumbers[processName] = sortNumber;
                }
                catch (const std::exception&) {
                    // Skip invalid lines
                }
            }
        }
        file.close();
    }
}

// NEW: Enhanced SavePresetAs to include sort numbers
void ProcessFilterManager::SavePresetAs(const std::string& presetName) {
    try {
        // Create JSON structure (keep your existing structure)
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

        // Save text filter if active (your existing code)
        if (!m_textFilter.empty()) {
            presetJson["text_filter"] = m_textFilter;
        }

        // NEW: Save sort numbers
        if (!m_processSortNumbers.empty()) {
            nlohmann::json sortNumbersJson;
            for (const auto& [processName, sortNumber] : m_processSortNumbers) {
                sortNumbersJson[processName] = sortNumber;
            }
            presetJson["sort_numbers"] = sortNumbersJson;
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

// NEW: Enhanced LoadPresetFromFile to include sort numbers
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
        m_processSortNumbers.clear();  // NEW: Clear sort numbers

        // Load visible processes (your existing code)
        if (presetJson.contains("visible_processes") && presetJson["visible_processes"].is_array()) {
            for (const auto& process : presetJson["visible_processes"]) {
                if (process.is_string()) {
                    m_visibleProcesses.insert(process.get<std::string>());
                }
            }
        }

        // Load text filter if present (your existing code)
        if (presetJson.contains("text_filter") && presetJson["text_filter"].is_string()) {
            m_textFilter = presetJson["text_filter"].get<std::string>();
        }

        // NEW: Load sort numbers if present
        if (presetJson.contains("sort_numbers") && presetJson["sort_numbers"].is_object()) {
            for (const auto& [processName, sortNumber] : presetJson["sort_numbers"].items()) {
                if (sortNumber.is_number_integer()) {
                    m_processSortNumbers[processName] = sortNumber.get<int>();
                }
            }
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

// NEW: Enhanced RenderFilterWindow with button ordering section
void ProcessFilterManager::RenderFilterWindow(bool* showWindow) {
    if (!showWindow || !*showWindow) return;

    ImGui::SetNextWindowSize(ImVec2(700, 800), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Process Filter & Button Order Configuration", showWindow)) {
        // Current preset info (your existing code)
        ImGui::Text("Current Preset: %s", m_currentPresetName.empty() ? "None" : m_currentPresetName.c_str());
        ImGui::Separator();

        // NEW: Button ordering section
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "Button Order Control");
        ImGui::Text("Assign numbers to control button display order in Column 1");

        if (ImGui::Button("Assign 1,2,3...")) {
            AssignSequentialNumbers();
        }
        ImGui::SameLine();
        if (ImGui::Button("Assign 10,20,30...")) {
            AssignSpacedNumbers();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear All Numbers")) {
            ClearAllSortNumbers();
        }

        ImGui::Separator();

        // Text filter section (your existing code)
        ImGui::Text("Text Filter:");
        static char textFilterBuffer[256] = "";

        if (strlen(textFilterBuffer) == 0 && !m_textFilter.empty()) {
          strncpy_s(textFilterBuffer, sizeof(textFilterBuffer), m_textFilter.c_str(), _TRUNCATE);
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

        // Quick namespace buttons (your existing code)
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

        ImGui::Separator();

        // Process list with sort number editing
        auto allProcesses = GetAllAvailableProcesses();
        ImGui::Text("Process Visibility & Button Order (%zu of %zu visible):",
            m_visibleProcesses.size(), allProcesses.size());

        // Show match count if filter is active (your existing code)
        if (!m_textFilter.empty()) {
            auto matches = GetProcessesMatchingText(m_textFilter);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
                "(%zu matches for '%s')", matches.size(), m_textFilter.c_str());
        }

        // Control buttons (your existing code)
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

        // Process list with enhanced display
        if (ImGui::BeginChild("ProcessConfiguration", ImVec2(0, 350), true)) {

            // Show numbered visible processes first
            auto sortedVisible = GetSortedFilteredProcessList();
            if (!sortedVisible.empty()) {
                bool hasNumberedVisible = false;
                for (const auto& process : sortedVisible) {
                    int sortNum = GetProcessSortNumber(process);
                    if (sortNum > 0) {
                        if (!hasNumberedVisible) {
                            ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "Numbered Buttons (Visible):");
                            hasNumberedVisible = true;
                        }
                        RenderProcessConfigLine(process, true);
                    }
                }

                if (hasNumberedVisible) {
                    ImGui::Separator();
                }

                // Show unnumbered visible processes
                bool hasUnnumberedVisible = false;
                for (const auto& process : sortedVisible) {
                    int sortNum = GetProcessSortNumber(process);
                    if (sortNum <= 0) {
                        if (!hasUnnumberedVisible) {
                            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.8f, 1.0f), "Unnumbered Buttons (Visible):");
                            hasUnnumberedVisible = true;
                        }
                        RenderProcessConfigLine(process, true);
                    }
                }
            }

            ImGui::Separator();

            // Show hidden processes (collapsed)
            if (ImGui::CollapsingHeader("Hidden Processes")) {
                for (const auto& process : allProcesses) {
                    if (!IsProcessVisible(process)) {
                        RenderProcessConfigLine(process, false);
                    }
                }
            }
        }
        ImGui::EndChild();

        ImGui::Separator();

        // Preset management section (your existing code with minor additions)
        ImGui::Text("Save Custom Preset:");
        static char presetName[256] = "";
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("Preset Name", presetName, sizeof(presetName));

        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            if (strlen(presetName) > 0) {
                SavePresetAs(presetName);  // Now saves sort numbers too
                memset(presetName, 0, sizeof(presetName));
            }
        }

        // Load preset section (your existing code)
        ImGui::Text("Load Custom Preset:");
        auto customPresets = GetAvailablePresetFiles();

        if (!customPresets.empty()) {
            static int selectedPresetIndex = 0;
            std::vector<const char*> presetNames;
            for (const auto& preset : customPresets) {
                presetNames.push_back(preset.c_str());
            }

            ImGui::SetNextItemWidth(200);
            ImGui::Combo("##CustomPresets", &selectedPresetIndex, presetNames.data(), static_cast<int>(presetNames.size()));

            ImGui::SameLine();
            if (ImGui::Button("Load")) {
                if (selectedPresetIndex >= 0 && selectedPresetIndex < customPresets.size()) {
                    LoadPresetFromFile(customPresets[selectedPresetIndex]);  // Now loads sort numbers too
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
    }

    ImGui::End();
}

// NEW: Render individual process configuration line
void ProcessFilterManager::RenderProcessConfigLine(const std::string& processName, bool isVisible) {
    ImGui::PushID(processName.c_str());

    // Visibility checkbox
    bool visible = isVisible;
    if (ImGui::Checkbox("", &visible)) {
        SetProcessVisible(processName, visible);
    }

    ImGui::SameLine();

    // Sort number input (width: 50px)
    int sortNum = GetProcessSortNumber(processName);
    ImGui::SetNextItemWidth(50);
    if (ImGui::InputInt("##sort", &sortNum, 0, 0)) {
        SetProcessSortNumber(processName, sortNum);
    }

    ImGui::SameLine();

    // Process name with sort number display and text filter highlighting
    bool matchesFilter = m_textFilter.empty() || ProcessMatchesFilter(processName, m_textFilter);
    if (!m_textFilter.empty() && matchesFilter) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    }

    if (sortNum > 0) {
        ImGui::Text("[%d] %s", sortNum, processName.c_str());
    }
    else {
        ImGui::Text("[ ] %s", processName.c_str());
    }

    if (!m_textFilter.empty() && matchesFilter) {
        ImGui::PopStyleColor();
    }

    ImGui::PopID();
}