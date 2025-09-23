// Include WinSock guard first
#ifdef _WIN32
#include "WinSockGuard.h"
#endif


#include "SystemStatusUI.h"
#include "AppContext.h"
#include "imgui.h"
#include "include/logger.h"
#include <algorithm>
#include <map>
#include <cstdio>  // for sprintf

SystemStatusUI::SystemStatusUI() {
  m_lastRefreshTime = std::chrono::steady_clock::now();
  m_lastReport.lastUpdate = m_lastRefreshTime;
}

void SystemStatusUI::Render() {
  if (!m_visible) return;

  ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("System Status Monitor", &m_visible)) {

    // Header section
    ImGui::Text("System Module Status");
    ImGui::Separator();

    // Control buttons
    if (ImGui::Button("Run System Check", ImVec2(150, 0))) {
      RunSystemCheck();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Auto Refresh", &m_autoRefresh);

    if (m_autoRefresh) {
      ImGui::SameLine();
      ImGui::SetNextItemWidth(100);
      ImGui::SliderFloat("Interval (s)", &m_refreshInterval, 1.0f, 60.0f, "%.1f");

      // Auto refresh logic
      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - m_lastRefreshTime).count();

      if (elapsed >= m_refreshInterval) {
        RunSystemCheck();
        m_lastRefreshTime = now;
      }

      // Show countdown
      float timeUntilRefresh = m_refreshInterval - elapsed;
      ImGui::SameLine();
      ImGui::Text("Next refresh in: %.1fs", timeUntilRefresh);
    }

    // Last update time
    if (m_lastReport.totalCount > 0) {
      auto now = std::chrono::steady_clock::now();
      auto timeSinceUpdate = std::chrono::duration_cast<std::chrono::seconds>(
        now - m_lastReport.lastUpdate).count();

      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
        "(Last update: %lld seconds ago)", timeSinceUpdate);
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Summary section
    RenderSummarySection();

    ImGui::Separator();
    ImGui::Spacing();

    // Module details in scrollable area
    ImGui::BeginChild("ModuleList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()),
      true, ImGuiWindowFlags_HorizontalScrollbar);

    // Group modules by category
    std::map<std::string, std::vector<ModuleStatus>> categorized;
    for (const auto& module : m_lastReport.modules) {
      categorized[module.category].push_back(module);
    }

    // Render each category
    for (const auto& [category, modules] : categorized) {
      RenderCategorySection(category);
    }

    ImGui::EndChild();

    // Footer with actions
    if (ImGui::Button("Print to Log")) {
      AppContext::GetInstance().PrintModuleStatusReport();
    }

    ImGui::SameLine();
    if (ImGui::Button("Refresh Now")) {
      RunSystemCheck();
    }
  }
  ImGui::End();
}

void SystemStatusUI::RunSystemCheck() {
  auto* logger = Logger::GetInstance();
  if (logger) {
    logger->LogInfo("Running system status check...");
  }

  AppContext& context = AppContext::GetInstance();
  m_lastReport.modules.clear();

  // Define required modules
  std::map<std::string, bool> requiredModules = {
      {"Logger", true},
      {"MotionConfig", true},
      {"MotionControlLayer", true},
      {"PIController", true},
      {"ACSController", true},
      {"IOManager", true},
      {"IOConfig", true},
      {"Pneumatic", true},
      {"MachineOps", true},
      {"MotionOps", true},
      {"IOOps", true}
  };

  // Check Core Services
  auto checkModule = [&](const std::string& category, const std::string& name, void* ptr) {
    ModuleStatus status;
    status.category = category;
    status.name = name;
    status.initialized = (ptr != nullptr);
    status.required = requiredModules.find(name) != requiredModules.end() &&
      requiredModules[name];
    m_lastReport.modules.push_back(status);

    if (status.initialized) m_lastReport.successCount++;
    if (status.required) {
      m_lastReport.requiredCount++;
      if (status.initialized) m_lastReport.requiredSuccess++;
    }
    else {
      m_lastReport.optionalCount++;
      if (status.initialized) m_lastReport.optionalSuccess++;
    }
    m_lastReport.totalCount++;
  };

  // Reset counters
  m_lastReport.totalCount = 0;
  m_lastReport.successCount = 0;
  m_lastReport.requiredCount = 0;
  m_lastReport.requiredSuccess = 0;
  m_lastReport.optionalCount = 0;
  m_lastReport.optionalSuccess = 0;

  // Check all modules
  checkModule("Core Services", "Logger", context.GetLogger());
  checkModule("Core Services", "MotionConfig", context.GetMotionConfig());
  checkModule("Core Services", "MotionControlLayer", context.GetMotionControlLayer());
  checkModule("Core Services", "ConfigWatchdog", context.GetConfigWatchdog());

  checkModule("Motion Hardware", "PIController", context.GetPIController());
  checkModule("Motion Hardware", "ACSController", context.GetACSController());

  checkModule("IO Systems", "IOManager", context.GetIOManager());
  checkModule("IO Systems", "IOConfig", context.GetIOConfig());
  checkModule("IO Systems", "Pneumatic", context.GetPneumaticManager());

  checkModule("Vision", "CameraManager", context.GetCameraManager());
  checkModule("Vision", "CameraConfig", context.GetCameraConfig());
  checkModule("Vision", "VisionExposureManager", context.GetVisionExposureManager());

  checkModule("Instruments", "CLD101x Laser", context.GetCLD101x());
  checkModule("Instruments", "Keithley 2400", context.GetKeithley());
  checkModule("Instruments", "Keithley 6482", context.GetKeithley6482());
  //checkModule("Instruments", "SPD Power Supply", context.GetSPDPowerSupply());

  checkModule("Data Services", "DataClient", context.GetDataClient());
  checkModule("Data Services", "Database", context.GetDatabaseManager());
  checkModule("Data Services", "ResultsManager", context.GetResultsManager());
  checkModule("Data Services", "DUT Recorder", context.GetDUTDataRecorder());

  checkModule("Operations", "MachineOps", context.GetMachineOperations());
  checkModule("Operations", "MotionOps", context.GetMotionOps());
  checkModule("Operations", "IOOps", context.GetIOOps());
  checkModule("Operations", "VisionOps", context.GetVisionOps());

  // Calculate overall health
  if (m_lastReport.requiredCount > 0) {
    float requiredHealth = (float)m_lastReport.requiredSuccess / m_lastReport.requiredCount;
    float optionalHealth = m_lastReport.optionalCount > 0 ?
      (float)m_lastReport.optionalSuccess / m_lastReport.optionalCount : 0.0f;
    m_lastReport.overallHealth = requiredHealth * 0.8f + optionalHealth * 0.2f;
  }

  m_lastReport.lastUpdate = std::chrono::steady_clock::now();

  if (logger) {
    logger->LogInfo("System check complete: " +
      std::to_string(m_lastReport.successCount) + "/" +
      std::to_string(m_lastReport.totalCount) + " modules initialized");
  }
}

void SystemStatusUI::RenderSummarySection() {
  if (m_lastReport.totalCount == 0) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
      "No status data available. Click 'Run System Check' to scan modules.");
    return;
  }

  // Overall system status
  float requiredRate = m_lastReport.requiredCount > 0 ?
    (float)m_lastReport.requiredSuccess / m_lastReport.requiredCount * 100.0f : 0.0f;

  ImVec4 statusColor;
  const char* statusText;

  if (requiredRate == 100.0f) {
    statusColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
    statusText = "✓ SYSTEM FULLY OPERATIONAL";
  }
  else if (requiredRate >= 80.0f) {
    statusColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
    statusText = "⚠ SYSTEM PARTIALLY OPERATIONAL";
  }
  else {
    statusColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    statusText = "✗ SYSTEM NOT READY";
  }

  ImGui::TextColored(statusColor, "%s", statusText);
  ImGui::Spacing();

  // Statistics table
  if (ImGui::BeginTable("StatusSummary", 3, ImGuiTableFlags_Borders)) {
    ImGui::TableSetupColumn("Module Type", ImGuiTableColumnFlags_WidthFixed, 150);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 100);
    ImGui::TableSetupColumn("Success Rate", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    // Total row
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Total Modules");
    ImGui::TableNextColumn();
    ImGui::Text("%d / %d", m_lastReport.successCount, m_lastReport.totalCount);
    ImGui::TableNextColumn();
    float totalRate = m_lastReport.totalCount > 0 ?
      (float)m_lastReport.successCount / m_lastReport.totalCount : 0.0f;
    // FIXED: Correct ProgressBar usage
    char totalBuf[32];
    sprintf_s(totalBuf, sizeof(totalBuf), "%.0f%%", totalRate * 100.0f);
    ImGui::ProgressBar(totalRate, ImVec2(-1, 0), totalBuf);

    // Required row
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Required");
    ImGui::TableNextColumn();
    ImGui::Text("%d / %d", m_lastReport.requiredSuccess, m_lastReport.requiredCount);
    ImGui::TableNextColumn();
    float reqRate = m_lastReport.requiredCount > 0 ?
      (float)m_lastReport.requiredSuccess / m_lastReport.requiredCount : 0.0f;
    // FIXED: Correct ProgressBar usage
    char reqBuf[32];

    //example sprintf_s(totalBuf, sizeof(totalBuf), "%.0f%%", totalRate * 100.0f);
    sprintf_s(reqBuf, sizeof(reqBuf), "%.0f%%", reqRate * 100.0f);
    ImGui::ProgressBar(reqRate, ImVec2(-1, 0), reqBuf);

    // Optional row
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1.0f), "Optional");
    ImGui::TableNextColumn();
    ImGui::Text("%d / %d", m_lastReport.optionalSuccess, m_lastReport.optionalCount);
    ImGui::TableNextColumn();
    float optRate = m_lastReport.optionalCount > 0 ?
      (float)m_lastReport.optionalSuccess / m_lastReport.optionalCount : 0.0f;
    // FIXED: Correct ProgressBar usage
    char optBuf[32];
    sprintf_s(optBuf,sizeof(optBuf), "%.0f%%", optRate * 100.0f);
    ImGui::ProgressBar(optRate, ImVec2(-1, 0), optBuf);

    ImGui::EndTable();
  }
}


void SystemStatusUI::RenderCategorySection(const std::string& category) {
  // Filter modules for this category
  std::vector<ModuleStatus> categoryModules;
  for (const auto& module : m_lastReport.modules) {
    if (module.category == category) {
      categoryModules.push_back(module);
    }
  }

  if (categoryModules.empty()) return;

  // Category header
  if (ImGui::CollapsingHeader(category.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::BeginTable(("Table_" + category).c_str(), 3,
      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Module", ImGuiTableColumnFlags_WidthFixed, 200);
      ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 150);
      ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100);
      ImGui::TableHeadersRow();

      for (const auto& module : categoryModules) {
        RenderModuleRow(module);
      }

      ImGui::EndTable();
    }
  }
}

void SystemStatusUI::RenderModuleRow(const ModuleStatus& module) {
  ImGui::TableNextRow();

  ImGui::TableNextColumn();
  ImGui::Text("%s %s", GetStatusIcon(module.initialized), module.name.c_str());

  ImGui::TableNextColumn();
  ImGui::TextColored(GetStatusColor(module.initialized, module.required),
    "%s", module.initialized ? "INITIALIZED" : "NOT INITIALIZED");

  ImGui::TableNextColumn();
  if (module.required) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "REQUIRED");
  }
  else {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1.0f), "OPTIONAL");
  }
}

ImVec4 SystemStatusUI::GetStatusColor(bool initialized, bool required) const {
  if (initialized) {
    return ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
  }
  else if (required) {
    return ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
  }
  else {
    return ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
  }
}

const char* SystemStatusUI::GetStatusIcon(bool initialized) const {
  return initialized ? "✓" : "✗";
}