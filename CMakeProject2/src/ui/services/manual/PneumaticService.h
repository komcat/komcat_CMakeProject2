
#pragma once

#include "../UIServiceRegistry.h"
#include "core/ServiceLocator.h"
#include "utils/Logger.h"
#include "utils/Unicode.h"
#include "imgui.h"

class PneumaticService : public IUIService {
public:
  void RenderUI() override {
    auto pneumatic = Services.Pneumatic();
    ImGui::Text(pneumatic ? "💨 Pneumatic: System ready" : "💨 Pneumatic: Not available");
  }
  std::string GetServiceName() const override { return "pneumatic_control"; }
  std::string GetDisplayName() const override { return "Pneumatic Control"; }
  std::string GetCategory() const override { return "Manual"; }
  bool IsAvailable() const override { return true; }
};