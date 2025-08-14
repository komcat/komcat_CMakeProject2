
#pragma once

#include "../UIServiceRegistry.h"
#include "core/ServiceLocator.h"
#include "utils/Logger.h"
#include "utils/Unicode.h"
#include "imgui.h"

class SMUService : public IUIService {
public:
  void RenderUI() override {
    auto smu = Services.SMU();
    ImGui::Text(smu ? "🔋 SMU: Keithley connected" : "🔋 SMU: No device");
  }
  std::string GetServiceName() const override { return "smu_control"; }
  std::string GetDisplayName() const override { return "SMU Control"; }
  std::string GetCategory() const override { return "Data"; }
  bool IsAvailable() const override { return true; }
};