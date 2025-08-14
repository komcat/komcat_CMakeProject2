
#pragma once

#include "../UIServiceRegistry.h"
#include "core/ServiceLocator.h"
#include "utils/Logger.h"
#include "utils/Unicode.h"
#include "imgui.h"


class CLD101xService : public IUIService {
public:
  void RenderUI() override {
    auto cld = Services.CLD101x();
    ImGui::Text(cld ? "🔥 CLD101x: Laser online" : "🔥 CLD101x: Not connected");
  }
  std::string GetServiceName() const override { return "cld101x_control"; }
  std::string GetDisplayName() const override { return "CLD101x Equipment"; }
  std::string GetCategory() const override { return "Data"; }
  bool IsAvailable() const override { return true; }
};