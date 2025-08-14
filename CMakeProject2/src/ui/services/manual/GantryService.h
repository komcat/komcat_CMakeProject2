
#pragma once

#include "../UIServiceRegistry.h"
#include "core/ServiceLocator.h"
#include "utils/Logger.h"
#include "utils/Unicode.h"
#include "imgui.h"

class GantryService : public IUIService {
public:
  void RenderUI() override {
    auto acs = Services.ACS();
    ImGui::Text(acs ? "🦾 Gantry: ACS Ready" : "🦾 Gantry: ACS Not available");
  }
  std::string GetServiceName() const override { return "gantry_control"; }
  std::string GetDisplayName() const override { return "Gantry Control"; }
  std::string GetCategory() const override { return "Manual"; }
  bool IsAvailable() const override { return true; }
};
