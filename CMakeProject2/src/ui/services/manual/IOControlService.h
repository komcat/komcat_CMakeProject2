
#pragma once

#include "../UIServiceRegistry.h"
#include "core/ServiceLocator.h"
#include "utils/Logger.h"
#include "utils/Unicode.h"
#include "imgui.h"

class IOControlService : public IUIService {
public:
  void RenderUI() override {
    auto io = Services.IO();
    ImGui::Text(io ? "⚡ IO Control: Connected" : "⚡ IO Control: Not connected");
  }
  std::string GetServiceName() const override { return "io_control"; }
  std::string GetDisplayName() const override { return "IO Control"; }
  std::string GetCategory() const override { return "Manual"; }
  bool IsAvailable() const override { return true; }
};