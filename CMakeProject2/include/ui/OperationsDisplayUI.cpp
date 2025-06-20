// OperationsDisplayUI.cpp
#include "OperationsDisplayUI.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

OperationsDisplayUI::OperationsDisplayUI(MachineOperations& machineOps)
  : m_machineOps(machineOps), m_lastRefresh(std::chrono::steady_clock::now()) {

  m_logger = Logger::GetInstance();
  m_resultsManager = m_machineOps.GetResultsManager();

  if (!m_resultsManager) {
    m_logger->LogWarning("OperationsDisplayUI: No results manager available");
  }
  else {
    m_logger->LogInfo("OperationsDisplayUI: Initialized successfully");
    RefreshOperationsList(); // Initial load
  }
}

void OperationsDisplayUI::RenderUI() {
  if (!m_showWindow) return;

  // Check if we need to refresh data (smart refresh)
  if (ShouldRefresh()) {
    CheckForUpdates();
    m_lastRefresh = std::chrono::steady_clock::now();
  }

  // Set window size and position
  ImGuiIO& io = ImGui::GetIO();
  ImVec2 displaySize = io.DisplaySize;
  ImGui::SetNextWindowSize(ImVec2(displaySize.x * 0.8f, displaySize.y * 0.7f), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImVec2(displaySize.x * 0.1f, displaySize.y * 0.15f), ImGuiCond_FirstUseEver);

  if (ImGui::Begin(m_windowTitle.c_str(), &m_showWindow, ImGuiWindowFlags_MenuBar)) {

    // Menu bar
    if (ImGui::BeginMenuBar()) {
      if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Refresh Now")) {
          RefreshOperationsList();
          m_logger->LogInfo("OperationsDisplayUI: Manual refresh triggered");
        }
        ImGui::Separator();

        // Refresh interval options
        if (ImGui::SliderInt("Refresh Interval (ms)",
          reinterpret_cast<int*>(&m_refreshInterval), 500, 5000)) {
          // Interval changed
        }

        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Filter")) {
        ImGui::Checkbox("Show Running Only", &m_showRunningOnly);
        ImGui::Checkbox("Show Failed Only", &m_showFailedOnly);
        if (ImGui::Button("Clear All Filters")) {
          memset(m_methodFilter, 0, sizeof(m_methodFilter));
          memset(m_deviceFilter, 0, sizeof(m_deviceFilter));
          memset(m_statusFilter, 0, sizeof(m_statusFilter));
          m_showRunningOnly = false;
          m_showFailedOnly = false;
        }
        ImGui::EndMenu();
      }

      ImGui::EndMenuBar();
    }

    // Main content area with splitter
    float availableWidth = ImGui::GetContentRegionAvail().x;

    // Left panel
    ImGui::BeginChild("LeftPanel", ImVec2(m_leftPanelWidth, 0), true);
    RenderLeftPanel();
    ImGui::EndChild();

    // Splitter
    ImGui::SameLine();
    ImGui::Button("##splitter", ImVec2(8.0f, -1));
    if (ImGui::IsItemActive()) {
      float delta = ImGui::GetIO().MouseDelta.x;
      m_leftPanelWidth += delta;
      m_leftPanelWidth = (std::clamp)(m_leftPanelWidth, m_minPanelWidth,
        (std::min)(m_maxPanelWidth, availableWidth - 100.0f));
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }

    // Right panel
    ImGui::SameLine();
    float rightPanelWidth = availableWidth - m_leftPanelWidth - 16.0f; // Account for splitter
    ImGui::BeginChild("RightPanel", ImVec2(rightPanelWidth, 0), true);
    RenderRightPanel();
    ImGui::EndChild();
  }
  ImGui::End();
}

void OperationsDisplayUI::RenderLeftPanel() {
  ImGui::Text("Operations List");
  ImGui::Separator();

  // Filters and controls
  RenderFilters();

  ImGui::Separator();

  // Operations list
  RenderOperationsList();
}

void OperationsDisplayUI::RenderRightPanel() {
  ImGui::Text("Operation Details");
  ImGui::Separator();

  // Debug info (can remove later)
  ImGui::Text("Selected ID: %s", m_selectedOperationId.c_str());
  ImGui::Text("Selected Index: %d / %d", m_selectedOperationIndex, (int)m_operations.size());

  // FIXED: Check if we have a valid selection by ID
  if (!m_selectedOperationId.empty()) {
    // Find the operation by ID in current list
    bool found = false;
    for (int i = 0; i < static_cast<int>(m_operations.size()); i++) {
      if (m_operations[i].operationId == m_selectedOperationId) {
        m_selectedOperation = m_operations[i];  // Refresh from current data
        m_selectedOperationIndex = i;           // Update index
        found = true;
        break;
      }
    }

    if (found) {
      RenderOperationDetails();
    }
    else {
      // Selected operation no longer exists in current list
      ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
        "Selected operation no longer in current view");
      ImGui::Text("(Operation ID: %s)", m_selectedOperationId.c_str());

      // Clear invalid selection
      m_selectedOperationId.clear();
      m_selectedOperationIndex = -1;
    }
  }
  else {
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
      "Select an operation from the list to view details");
  }
}

void OperationsDisplayUI::RenderFilters() {
  // Display count selector
  ImGui::Text("Show:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80);
  if (ImGui::Combo("##count", &m_displayOptionIndex, "10\00020\00050\000")) {
    m_displayCount = m_displayOptions[m_displayOptionIndex];
    m_logger->LogInfo("OperationsDisplayUI: Display count changed to " + std::to_string(m_displayCount));
    // No need to refresh - just changes display filtering
  }

  ImGui::SameLine();
  ImGui::Text("operations");

  // Filters
  ImGui::Text("Filters:");

  ImGui::SetNextItemWidth(-1);
  if (ImGui::InputText("##method", m_methodFilter, sizeof(m_methodFilter))) {
    // Filter changed - will apply on next render
  }
  ImGui::SameLine(0, 0);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Filter by method name");
  }

  ImGui::SetNextItemWidth(-1);
  if (ImGui::InputText("##device", m_deviceFilter, sizeof(m_deviceFilter))) {
    // Filter changed
  }
  ImGui::SameLine(0, 0);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Filter by device name");
  }

  ImGui::SetNextItemWidth(-1);
  if (ImGui::InputText("##status", m_statusFilter, sizeof(m_statusFilter))) {
    // Filter changed
  }
  ImGui::SameLine(0, 0);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Filter by status");
  }
}

void OperationsDisplayUI::RenderOperationsList() {
  // Table headers
  ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
    ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg;

  if (ImGui::BeginTable("OperationsTable", 6, flags)) {
    ImGui::TableSetupColumn("Select", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableSetupColumn("Method", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Device", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    // Filter and display operations
    int displayedCount = 0;
    for (int i = 0; i < static_cast<int>(m_operations.size()) && displayedCount < m_displayCount; i++) {
      const auto& op = m_operations[i];

      if (!PassesFilters(op)) continue;

      ImGui::TableNextRow();

      // FIXED: Simple checkbox selection 
      bool isSelected = (m_selectedOperationId == op.operationId);

      // Select column with checkbox
      ImGui::TableNextColumn();
      if (ImGui::Checkbox(("##select_" + op.operationId).c_str(), &isSelected)) {
        if (isSelected) {
          // This operation was just selected
          m_selectedOperationId = op.operationId;
          m_selectedOperation = op;
          m_selectedOperationIndex = i;
          m_logger->LogInfo("OperationsDisplayUI: Selected operation " + op.operationId +
            " (array index: " + std::to_string(i) + ")");
        }
        else {
          // This operation was deselected
          m_selectedOperationId.clear();
          m_selectedOperationIndex = -1;
        }
      }

      // Status column with icon and color  
      ImGui::TableNextColumn();
      ImVec4 statusColor = GetStatusColor(op.status);
      ImGui::TextColored(statusColor, "%s %s", GetStatusIcon(op.status), op.status.c_str());

      // Method column - just display text now
      ImGui::TableNextColumn();
      ImGui::Text("%s", op.methodName.c_str());

      // Device column
      ImGui::TableNextColumn();
      ImGui::Text("%s", op.deviceName.c_str());

      // Duration column
      ImGui::TableNextColumn();
      if (op.elapsedTimeMs > 0) {
        ImGui::Text("%s", FormatDuration(op.elapsedTimeMs).c_str());
      }
      else if (op.status == "running") {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Running...");
      }
      else {
        ImGui::Text("-");
      }

      // Time column
      ImGui::TableNextColumn();
      ImGui::Text("%s", FormatTimestamp(op.timestamp).c_str());

      displayedCount++;
    }

    ImGui::EndTable();
  }

  // Status info
  ImGui::Text("Showing %d of %d operations",
    (std::min)(m_displayCount, static_cast<int>(m_operations.size())),
    static_cast<int>(m_operations.size()));
}



void OperationsDisplayUI::RenderOperationDetails() {
  const auto& op = m_selectedOperation;

  // Operation metadata
  ImGui::Text("Operation ID: %s", op.operationId.c_str());
  ImGui::Text("Method: %s", op.methodName.c_str());
  ImGui::Text("Device: %s", op.deviceName.c_str());
  ImGui::Text("Status: ");
  ImGui::SameLine();
  ImVec4 statusColor = GetStatusColor(op.status);
  ImGui::TextColored(statusColor, "%s %s", GetStatusIcon(op.status), op.status.c_str());

  if (!op.callerContext.empty()) {
    ImGui::Text("Caller: %s", op.callerContext.c_str());
  }

  if (!op.sequenceName.empty()) {
    ImGui::Text("Sequence: %s", op.sequenceName.c_str());
  }

  // Timing info
  ImGui::Separator();
  ImGui::Text("Timing Information:");

  // Use the timestamp field (which should be populated from start_time)
  ImGui::Text("Operation Time: %s", FormatTimestamp(op.timestamp).c_str());

  if (op.elapsedTimeMs > 0) {
    ImGui::Text("Duration: %s", FormatDuration(op.elapsedTimeMs).c_str());
  }

  // Error message if failed (check data map for error info)
  if (op.status == "failed" || op.status == "error") {
    auto errorIt = op.data.find("error_message");
    if (errorIt != op.data.end() && !errorIt->second.empty()) {
      ImGui::Separator();
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Error Message:");
      ImGui::TextWrapped("%s", errorIt->second.c_str());
    }
  }

  // Results/Parameters
  if (!op.data.empty()) {
    ImGui::Separator();
    ImGui::Text("Parameters & Results:");

    if (ImGui::BeginTable("ResultsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 150.0f);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      for (const auto& [key, value] : op.data) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        // Color-code parameter vs result keys
        if (key.find("param_") == 0) {
          ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "%s", key.c_str());
        }
        else {
          ImGui::Text("%s", key.c_str());
        }

        ImGui::TableNextColumn();
        ImGui::Text("%s", value.c_str());
      }

      ImGui::EndTable();
    }
  }
}

void OperationsDisplayUI::CheckForUpdates() {
  if (!m_resultsManager) {
    return;
  }

  try {
    // First, do a lightweight check - just get the count and latest operation
    auto latestOp = m_resultsManager->GetLatestOperation();
    int currentCount = m_resultsManager->GetOperationCount();

    // Check if we need to refresh
    bool needsRefresh = false;

    if (currentCount != m_lastKnownOperationCount) {
      needsRefresh = true;
      m_logger->LogInfo("OperationsDisplayUI: Operation count changed (" +
        std::to_string(m_lastKnownOperationCount) + " -> " +
        std::to_string(currentCount) + ")");
    }
    else if (!latestOp.operationId.empty() && latestOp.operationId != m_lastKnownLatestOpId) {
      needsRefresh = true;
      m_logger->LogInfo("OperationsDisplayUI: New latest operation detected: " + latestOp.operationId);
    }

    // Only do full refresh if something actually changed
    if (needsRefresh) {
      RefreshOperationsList();
      m_lastKnownOperationCount = currentCount;
      m_lastKnownLatestOpId = latestOp.operationId;
    }

  }
  catch (const std::exception& e) {
    m_logger->LogError("OperationsDisplayUI: Error checking for updates: " + std::string(e.what()));
    // Fall back to full refresh on error
    RefreshOperationsList();
  }
}

bool OperationsDisplayUI::NeedsRefresh() const {
  if (!m_resultsManager) return false;

  try {
    int currentCount = m_resultsManager->GetOperationCount();
    return (currentCount != m_lastKnownOperationCount);
  }
  catch (const std::exception&) {
    return true; // Refresh on error
  }
}

void OperationsDisplayUI::RefreshOperationsList() {
  if (!m_resultsManager) {
    m_operations.clear();
    m_selectedOperationIndex = -1;
    m_selectedOperationId.clear();  // FIXED: Clear ID selection too
    return;
  }

  try {
    // Store currently selected operation ID for restoration
    std::string selectedOpId = m_selectedOperationId;

    // Get recent operations with increased cache size
    m_operations = m_resultsManager->GetOperationHistory(DEFAULT_CACHE_SIZE);

    // Update smart refresh tracking
    if (!m_operations.empty()) {
      m_lastKnownLatestOpId = m_operations[0].operationId; // First is most recent
    }
    m_lastKnownOperationCount = m_resultsManager->GetOperationCount();

    // FIXED: Try to restore selection by operation ID
    bool selectionRestored = false;
    if (!selectedOpId.empty()) {
      for (int i = 0; i < static_cast<int>(m_operations.size()); i++) {
        if (m_operations[i].operationId == selectedOpId) {
          m_selectedOperationId = selectedOpId;
          m_selectedOperationIndex = i;
          m_selectedOperation = m_operations[i];
          selectionRestored = true;
          break;
        }
      }
    }

    // If we couldn't restore selection, reset it
    if (!selectionRestored) {
      m_selectedOperationIndex = -1;
      m_selectedOperationId.clear();
    }

    m_logger->LogInfo("OperationsDisplayUI: Refreshed operations list (" +
      std::to_string(m_operations.size()) + " operations loaded)");

  }
  catch (const std::exception& e) {
    m_logger->LogError("OperationsDisplayUI: Error refreshing operations: " + std::string(e.what()));
    m_operations.clear();
    m_selectedOperationIndex = -1;
    m_selectedOperationId.clear();  // FIXED: Clear ID selection too
  }
}

bool OperationsDisplayUI::ShouldRefresh() const {
  auto now = std::chrono::steady_clock::now();
  return (now - m_lastRefresh) >= m_refreshInterval;
}

bool OperationsDisplayUI::PassesFilters(const OperationResult& op) const {
  // Method filter
  if (strlen(m_methodFilter) > 0) {
    if (op.methodName.find(m_methodFilter) == std::string::npos) {
      return false;
    }
  }

  // Device filter
  if (strlen(m_deviceFilter) > 0) {
    if (op.deviceName.find(m_deviceFilter) == std::string::npos) {
      return false;
    }
  }

  // Status filter
  if (strlen(m_statusFilter) > 0) {
    if (op.status.find(m_statusFilter) == std::string::npos) {
      return false;
    }
  }

  // Running only filter
  if (m_showRunningOnly && op.status != "running") {
    return false;
  }

  // Failed only filter
  if (m_showFailedOnly && op.status != "failed" && op.status != "error") {
    return false;
  }

  return true;
}

std::string OperationsDisplayUI::FormatDuration(int64_t milliseconds) const {
  if (milliseconds < 1000) {
    return std::to_string(milliseconds) + "ms";
  }
  else if (milliseconds < 60000) {
    return std::to_string(milliseconds / 1000) + "s";
  }
  else {
    int minutes = static_cast<int>(milliseconds / 60000);
    int seconds = static_cast<int>((milliseconds % 60000) / 1000);
    return std::to_string(minutes) + "m " + std::to_string(seconds) + "s";
  }
}

std::string OperationsDisplayUI::FormatTimestamp(const std::chrono::system_clock::time_point& timePoint) const {
  auto time_t = std::chrono::system_clock::to_time_t(timePoint);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
  return ss.str();
}

ImVec4 OperationsDisplayUI::GetStatusColor(const std::string& status) const {
  if (status == "success" || status == "completed") {
    return ImVec4(0.0f, 0.8f, 0.0f, 1.0f); // Green
  }
  else if (status == "failed" || status == "error") {
    return ImVec4(1.0f, 0.3f, 0.3f, 1.0f); // Red
  }
  else if (status == "running") {
    return ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
  }
  else {
    return ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Gray
  }
}

const char* OperationsDisplayUI::GetStatusIcon(const std::string& status) const {
  if (status == "success" || status == "completed") {
    return "✓";
  }
  else if (status == "failed" || status == "error") {
    return "✗";
  }
  else if (status == "running") {
    return "⟳";
  }
  else {
    return "?";
  }
}