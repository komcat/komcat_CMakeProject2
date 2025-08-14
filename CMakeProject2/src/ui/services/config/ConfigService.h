
#pragma once

#include "../UIServiceRegistry.h"
#include "core/ServiceLocator.h"
#include "utils/Logger.h"
#include "utils/Unicode.h"
#include "imgui.h"

class ConfigService : public IUIService {
public:
  void RenderUI() override {
    auto config = Services.MotionConfig();
    ImGui::Text(config ? "🔧 Configuration: Config loaded" : "🔧 Configuration: No config");
  }
  std::string GetServiceName() const override { return "system_config"; }
  std::string GetDisplayName() const override { return "System Configuration"; }
  std::string GetCategory() const override { return "Config"; }
  bool IsAvailable() const override { return true; }
};