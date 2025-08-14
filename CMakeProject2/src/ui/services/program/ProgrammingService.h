#pragma once

#include "../UIServiceRegistry.h"
#include "core/ServiceLocator.h"
#include "utils/Logger.h"
#include "utils/Unicode.h"
#include "imgui.h"

class ProgrammingService : public IUIService {
public:
  void RenderUI() override {
    auto machineOps = Services.MachineOps();
    ImGui::Text(machineOps ? "⚙️ Programming: Machine ops ready" : "⚙️ Programming: Ops not available");
  }
  std::string GetServiceName() const override { return "programming"; }
  std::string GetDisplayName() const override { return "Block Programming"; }
  std::string GetCategory() const override { return "Program"; }
  bool IsAvailable() const override { return true; }
};