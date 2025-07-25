#pragma once
#include "nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <set>
#include <functional>

// Process Filter Manager - Simple custom preset management
class ProcessFilterManager {
public:
  ProcessFilterManager();
  ~ProcessFilterManager();

  // Process filtering - simple visible/hidden list
  std::vector<std::string> GetFilteredProcessList() const;
  std::vector<std::string> GetAllAvailableProcesses() const;
  bool IsProcessVisible(const std::string& processName) const;
  void SetProcessVisible(const std::string& processName, bool visible);

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
  std::function<void()> m_onFilterChanged;

  // Master list of all available processes
  std::vector<std::string> m_allProcesses = {
      "Initialization",
      "InitializationParallel",
      "Probing",
      "UAA3_ModernProbing",
      "UAA3_EnhancedProbing",
      "UAA3_QuickProbing",
      "PickPlaceLeftLens",
      "PickPlaceRightLens",
      "UVCuring",
      "RejectLeftLens",
      "RejectRightLens",
      "NeedleCalibration",
      "UAA3_ModernNeedleCalib",
      "DispenseCalibration1",
      "DispenseCalibration2",
      "DispenseEpoxy1",
      "DispenseEpoxy2"
  };

  // INI file path for last preset
  const std::string INI_FILE_PATH = "filter_settings.ini";

  // Helper methods
  void NotifyFilterChanged();
  std::string GetCurrentTimestamp() const;
};