#pragma once

#include "../UIServiceRegistry.h"
#include "core/ServiceLocator.h"
#include "utils/Logger.h"
#include "utils/Unicode.h"
#include "imgui.h"


class MacroService : public IUIService {
public:
  void RenderUI() override {
    ImGui::Text("📝 Macro Manager: Sequence automation ready");
  }
  std::string GetServiceName() const override { return "macro_manager"; }
  std::string GetDisplayName() const override { return "Macro Manager"; }
  std::string GetCategory() const override { return "Program"; }
  bool IsAvailable() const override { return true; }
};