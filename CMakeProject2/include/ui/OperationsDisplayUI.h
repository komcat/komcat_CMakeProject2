// OperationsDisplayUI.h
#pragma once

#include "include/ui/ToolbarMenu.h" // For ITogglableUI interface
#include "include/machine_operations.h"
#include "include/data/OperationResultsManager.h"
#include "include/logger.h"
#include "imgui.h"
#include <string>
#include <vector>
#include <chrono>
#include <memory>

// UI for displaying operations with left panel (list) and right panel (details)
class OperationsDisplayUI : public ITogglableUI {
public:
  OperationsDisplayUI(MachineOperations& machineOps);
  ~OperationsDisplayUI() = default;

  // ITogglableUI interface implementation
  bool IsVisible() const override { return m_showWindow; }
  void ToggleWindow() override { m_showWindow = !m_showWindow; }
  const std::string& GetName() const override { return m_windowTitle; }

  // Main render function
  void RenderUI();

private:
  // UI state
  bool m_showWindow = true;
  std::string m_windowTitle = "Operations Monitor";

  // Reference to machine operations
  MachineOperations& m_machineOps;
  std::shared_ptr<OperationResultsManager> m_resultsManager;
  Logger* m_logger;

  // Timer-based refresh with smart refresh
  std::chrono::steady_clock::time_point m_lastRefresh;
  std::chrono::milliseconds m_refreshInterval{ 1000 }; // 1 second

  // Smart refresh tracking
  int m_lastKnownOperationCount = 0;
  std::string m_lastKnownLatestOpId;

  // Data
  std::vector<OperationResult> m_operations;
  int m_selectedOperationIndex = -1;
  OperationResult m_selectedOperation;

  // Increased cache size
  static constexpr int DEFAULT_CACHE_SIZE = 200;  // Increased from 100

  // UI controls
  int m_displayCount = 20; // 10, 20, 50 options
  const int m_displayOptions[3] = { 10, 20, 50 };
  int m_displayOptionIndex = 1; // Default to 20

  // Filters
  char m_methodFilter[128] = "";
  char m_deviceFilter[128] = "";
  char m_statusFilter[128] = "";
  bool m_showRunningOnly = false;
  bool m_showFailedOnly = false;

  // Panel sizing
  float m_leftPanelWidth = 400.0f;
  const float m_minPanelWidth = 200.0f;
  const float m_maxPanelWidth = 800.0f;

  // Methods
  void RefreshOperationsList();
  void CheckForUpdates();  // New: Smart refresh check
  bool NeedsRefresh() const;  // New: Check if refresh needed
  void RenderLeftPanel();
  void RenderRightPanel();
  void RenderFilters();
  void RenderOperationsList();
  void RenderOperationDetails();

  // Helpers
  bool ShouldRefresh() const;
  bool PassesFilters(const OperationResult& op) const;
  std::string FormatDuration(int64_t milliseconds) const;
  std::string FormatTimestamp(const std::chrono::system_clock::time_point& timePoint) const;
  ImVec4 GetStatusColor(const std::string& status) const;
  const char* GetStatusIcon(const std::string& status) const;

  // Selection tracking

  std::string m_selectedOperationId;  // NEW: Track by ID instead of index

};