#pragma once
#include "nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <set>
#include <functional>
#include <algorithm>

// Process Filter Manager - Enhanced with text and namespace filtering
class ProcessFilterManager {
public:
    ProcessFilterManager();
    ~ProcessFilterManager();

    // Process filtering - simple visible/hidden list
    std::vector<std::string> GetFilteredProcessList() const;
    std::vector<std::string> GetAllAvailableProcesses() const;
    bool IsProcessVisible(const std::string& processName) const;
    void SetProcessVisible(const std::string& processName, bool visible);

    // NEW: Text-based filtering
    void SetTextFilter(const std::string& filter);
    std::string GetCurrentTextFilter() const { return m_textFilter; }
    std::vector<std::string> GetProcessesMatchingText(const std::string& text) const;
    void ApplyTextFilterToVisible(const std::string& text);
    void ClearTextFilter();

    // NEW: Namespace-based filtering
    void ShowOnlyNamespace(const std::string& namespacePrefix);
    std::vector<std::string> GetAvailableNamespaces() const;

    // UI rendering
    void RenderFilterWindow(bool* showWindow);

    // Custom preset management
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
    // Simple set of visible processes (all others are hidden)
    std::set<std::string> m_visibleProcesses;
    std::string m_currentPresetName = "";
    std::string m_textFilter = "";  // NEW: Current text filter
    std::function<void()> m_onFilterChanged;

    // INI file path for last preset
    const std::string INI_FILE_PATH = "filter_settings.ini";

    // Helper methods
    void NotifyFilterChanged();
    std::string GetCurrentTimestamp() const;

    // NEW: Text filtering helper methods
    bool ProcessMatchesFilter(const std::string& processName, const std::string& filter) const;
    std::string ExtractNamespace(const std::string& processName) const;
    std::string ToLowerCase(const std::string& str) const;
};