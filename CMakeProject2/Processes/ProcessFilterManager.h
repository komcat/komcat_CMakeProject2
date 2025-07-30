// ProcessFilterManager.h - Add numeric sorting to your existing implementation
#pragma once
#include "nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <set>
#include <unordered_map>  // NEW: For sort numbers
#include <functional>
#include <algorithm>

// Process Filter Manager - Enhanced with text, namespace, and numeric sorting
class ProcessFilterManager {
public:
    ProcessFilterManager();
    ~ProcessFilterManager();

    // Process filtering - simple visible/hidden list
    std::vector<std::string> GetFilteredProcessList() const;
    std::vector<std::string> GetAllAvailableProcesses() const;
    bool IsProcessVisible(const std::string& processName) const;
    void SetProcessVisible(const std::string& processName, bool visible);

    // Text-based filtering (your existing implementation)
    void SetTextFilter(const std::string& filter);
    std::string GetCurrentTextFilter() const { return m_textFilter; }
    std::vector<std::string> GetProcessesMatchingText(const std::string& text) const;
    void ApplyTextFilterToVisible(const std::string& text);
    void ClearTextFilter();

    // Namespace-based filtering (your existing implementation)
    void ShowOnlyNamespace(const std::string& namespacePrefix);
    std::vector<std::string> GetAvailableNamespaces() const;

    // NEW: Numeric button ordering
    std::vector<std::string> GetSortedFilteredProcessList() const;
    void SetProcessSortNumber(const std::string& processName, int sortNumber);
    int GetProcessSortNumber(const std::string& processName) const;
    void RemoveProcessSortNumber(const std::string& processName);
    void ClearAllSortNumbers();
    void AssignSequentialNumbers();  // Assign 1,2,3... to current visible processes
    void AssignSpacedNumbers();      // Assign 10,20,30... for easy insertion

    // UI rendering
    void RenderFilterWindow(bool* showWindow);

    // Custom preset management (enhanced to include sort numbers)
    void SavePresetAs(const std::string& presetName);
    bool LoadPresetFromFile(const std::string& presetName);
    std::vector<std::string> GetAvailablePresetFiles() const;
    bool DeletePresetFile(const std::string& presetName);

    // Last preset persistence
    void SaveLastPresetToIni(const std::string& presetName);
    std::string LoadLastPresetFromIni();
    bool LoadLastUsedPreset();

    // Callbacks for when filter changes
    void SetOnFilterChangedCallback(std::function<void()> callback) {
        m_onFilterChanged = callback;
    }

private:
    // Existing members
    std::set<std::string> m_visibleProcesses;
    std::string m_currentPresetName = "";
    std::string m_textFilter = "";
    std::function<void()> m_onFilterChanged;

    // NEW: Sort number mapping for button ordering
    std::unordered_map<std::string, int> m_processSortNumbers;

    // INI file path for last preset
    const std::string INI_FILE_PATH = "filter_settings.ini";

    // Helper methods
    void NotifyFilterChanged();
    std::string GetCurrentTimestamp() const;

    // Text filtering helper methods (your existing implementation)
    bool ProcessMatchesFilter(const std::string& processName, const std::string& filter) const;
    std::string ExtractNamespace(const std::string& processName) const;
    std::string ToLowerCase(const std::string& str) const;

    // NEW: Sorting helper methods
    bool CompareProcessesForSorting(const std::string& a, const std::string& b) const;
    void SaveSortNumbersToFile();
    void LoadSortNumbersFromFile();
    void RenderProcessConfigLine(const std::string& processName, bool isVisible);
};