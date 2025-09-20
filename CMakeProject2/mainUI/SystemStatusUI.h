#pragma once

// Include WinSock guard first if on Windows
#ifdef _WIN32
#include "WinSockGuard.h"
#endif

// CRITICAL: Forward declare instead of including AppContext.h directly
class AppContext;

#include "MenuManager_uaa3.h"

#include <string>
#include <vector>
#include <chrono>

class SystemStatusUI : public IImguiUI {
public:
  SystemStatusUI();
  ~SystemStatusUI() override = default;

  // IImguiUI interface
  void Render() override;
  void Show() override { m_visible = true; }
  void Hide() override { m_visible = false; }
  bool IsVisible() const override { return m_visible; }
  const std::string& GetName() const override {
    static std::string name = "System Status Monitor";
    return name;
  }

private:
  struct ModuleStatus {
    std::string category;
    std::string name;
    bool initialized;
    bool required;
  };

  struct StatusReport {
    std::vector<ModuleStatus> modules;
    int totalCount = 0;
    int successCount = 0;
    int requiredCount = 0;
    int requiredSuccess = 0;
    int optionalCount = 0;
    int optionalSuccess = 0;
    float overallHealth = 0.0f;
    std::chrono::steady_clock::time_point lastUpdate;
  };

  bool m_visible = false;
  StatusReport m_lastReport;
  bool m_autoRefresh = false;
  float m_refreshInterval = 5.0f; // seconds
  std::chrono::steady_clock::time_point m_lastRefreshTime;

  void RunSystemCheck();
  void RenderCategorySection(const std::string& category);
  void RenderSummarySection();
  void RenderModuleRow(const ModuleStatus& module);
  ImVec4 GetStatusColor(bool initialized, bool required) const;
  const char* GetStatusIcon(bool initialized) const;
};