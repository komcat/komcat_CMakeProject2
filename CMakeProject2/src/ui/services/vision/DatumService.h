
#pragma once

#include "../UIServiceRegistry.h"
#include "core/ServiceLocator.h"
#include "utils/Logger.h"
#include "utils/Unicode.h"
#include "imgui.h"

class DatumService : public IUIService {
public:
  void RenderUI() override {
    ImGui::Text("📐 Datum Reference: Coordinate system ready");
  }
  std::string GetServiceName() const override { return "datum_reference"; }
  std::string GetDisplayName() const override { return "Datum Reference"; }
  std::string GetCategory() const override { return "Vision"; }
  bool IsAvailable() const override { return true; }
};