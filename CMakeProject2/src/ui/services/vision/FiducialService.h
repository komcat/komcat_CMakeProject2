
#pragma once

#include "../UIServiceRegistry.h"
#include "core/ServiceLocator.h"
#include "utils/Logger.h"
#include "utils/Unicode.h"
#include "imgui.h"

class FiducialService : public IUIService {
public:
  void RenderUI() override {
    ImGui::Text("🎯 Fiducial Detection: Pattern matching active");
  }
  std::string GetServiceName() const override { return "fiducial_detection"; }
  std::string GetDisplayName() const override { return "Fiducial Detection"; }
  std::string GetCategory() const override { return "Vision"; }
  bool IsAvailable() const override { return true; }
};