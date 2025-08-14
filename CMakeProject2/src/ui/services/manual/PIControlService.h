// src/ui/services/manual/PIControlService.h
#pragma once

#include "../UIServiceRegistry.h"
#include "core/ServiceLocator.h"
#include "utils/Logger.h"
#include "utils/Unicode.h"
#include "imgui.h"

class PIControlService : public IUIService {
public:
  void RenderUI() override {
    auto pi = Services.PI();

    ImGui::SetWindowFontScale(1.5f);
    ImGui::Text("🤖 PI Controllers");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Spacing();
    ImGui::Text("Precision Motion Control System");
    ImGui::Separator();

    // Connection status
    ImGui::Text("Connection Status:");
    if (pi) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✅ PI Controllers: Connected and Ready");
      RenderConnectedInterface();
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "❌ PI Controllers: Not Connected");
      RenderDisconnectedInterface();
    }
  }

  std::string GetServiceName() const override { return "pi_control"; }
  std::string GetDisplayName() const override { return "PI Controllers"; }
  std::string GetCategory() const override { return "Manual"; }
  bool IsAvailable() const override { return true; }

private:
  void RenderConnectedInterface() {
    ImGui::Spacing();

    // Axis control
    ImGui::Text("Axis Control:");

    // X-Axis
    ImGui::Text("X-Axis:");
    ImGui::SameLine(100);
    static float xPosition = 0.0f;
    if (ImGui::SliderFloat("##x_pos", &xPosition, -50.0f, 50.0f, "%.2f mm")) {
      MoveAxis(0, xPosition);
    }
    ImGui::SameLine();
    if (ImGui::Button("Home X")) {
      HomeAxis(0);
    }

    // Y-Axis  
    ImGui::Text("Y-Axis:");
    ImGui::SameLine(100);
    static float yPosition = 0.0f;
    if (ImGui::SliderFloat("##y_pos", &yPosition, -50.0f, 50.0f, "%.2f mm")) {
      MoveAxis(1, yPosition);
    }
    ImGui::SameLine();
    if (ImGui::Button("Home Y")) {
      HomeAxis(1);
    }

    // Z-Axis
    ImGui::Text("Z-Axis:");
    ImGui::SameLine(100);
    static float zPosition = 0.0f;
    if (ImGui::SliderFloat("##z_pos", &zPosition, -25.0f, 25.0f, "%.2f mm")) {
      MoveAxis(2, zPosition);
    }
    ImGui::SameLine();
    if (ImGui::Button("Home Z")) {
      HomeAxis(2);
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Quick actions
    ImGui::Text("Quick Actions:");
    if (ImGui::Button("🏠 Home All Axes", ImVec2(150, 30))) {
      HomeAllAxes();
    }

    ImGui::SameLine();
    if (ImGui::Button("⏹️ Stop All", ImVec2(150, 30))) {
      StopAllAxes();
    }

    ImGui::SameLine();
    if (ImGui::Button("📍 Set Origin", ImVec2(150, 30))) {
      SetOrigin();
    }

    // Speed control
    ImGui::Spacing();
    ImGui::Text("Speed Control:");
    static float speed = 50.0f;
    ImGui::SliderFloat("Speed (%)", &speed, 1.0f, 100.0f, "%.0f");

    // Status display
    ImGui::Spacing();
    ImGui::Text("Status:");
    ImGui::BulletText("X: %.3f mm %s", xPosition, IsAxisMoving(0) ? "(Moving)" : "(Stopped)");
    ImGui::BulletText("Y: %.3f mm %s", yPosition, IsAxisMoving(1) ? "(Moving)" : "(Stopped)");
    ImGui::BulletText("Z: %.3f mm %s", zPosition, IsAxisMoving(2) ? "(Moving)" : "(Stopped)");
  }

  void RenderDisconnectedInterface() {
    ImGui::Spacing();
    ImGui::Text("Connection Options:");

    if (ImGui::Button("🔌 Connect PI Controllers", ImVec2(200, 40))) {
      AttemptConnection();
    }

    ImGui::Spacing();
    ImGui::Text("Connection Requirements:");
    ImGui::BulletText("PI controller hardware connected via USB/Ethernet");
    ImGui::BulletText("PI software drivers installed");
    ImGui::BulletText("Proper configuration file loaded");

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
      "ℹ️ This interface will be enabled when PI controllers are connected.");
  }

  // Control methods
  void MoveAxis(int axis, float position) {
    auto pi = Services.PI();
    if (pi) {
      // pi->MoveAxis(axis, position);
      Logger::Info(L"Moving axis " + std::to_wstring(axis) + L" to position " + std::to_wstring(position));
    }
  }

  void HomeAxis(int axis) {
    auto pi = Services.PI();
    if (pi) {
      // pi->HomeAxis(axis);
      Logger::Info(L"Homing axis " + std::to_wstring(axis));
    }
  }

  void HomeAllAxes() {
    auto pi = Services.PI();
    if (pi) {
      // pi->HomeAllAxes();
      Logger::Info(L"Homing all axes");
    }
  }

  void StopAllAxes() {
    auto pi = Services.PI();
    if (pi) {
      // pi->StopAllAxes();
      Logger::Info(L"Stopping all axes");
    }
  }

  void SetOrigin() {
    auto pi = Services.PI();
    if (pi) {
      // pi->SetOrigin();
      Logger::Info(L"Setting current position as origin");
    }
  }

  void AttemptConnection() {
    Logger::Info(L"Attempting to connect to PI controllers...");
    // Connection logic would go here
  }

  bool IsAxisMoving(int axis) {
    // Would check actual axis status
    return false;
  }
};