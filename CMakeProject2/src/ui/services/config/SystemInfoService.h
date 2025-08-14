
#pragma once

#include "../UIServiceRegistry.h"
#include "core/ServiceLocator.h"
#include "utils/Logger.h"
#include "utils/Unicode.h"
#include "imgui.h"

class SystemInfoService : public IUIService {
public:
  void RenderUI() override {
    ImGui::Text("ℹ️ System Info: Project4 v1.0.0 - Clean Architecture");
  }
  std::string GetServiceName() const override { return "system_info"; }
  std::string GetDisplayName() const override { return "System Information"; }
  std::string GetCategory() const override { return "Config"; }
  bool IsAvailable() const override { return true; }
};