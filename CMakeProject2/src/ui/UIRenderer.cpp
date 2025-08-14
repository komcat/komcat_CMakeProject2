#include "UIRenderer.h"
#include "CleanMainUI.h"
#include "imgui.h"
#include "services/ServiceInitializer.h"  // Updated include

UIRenderer::UIRenderer(const FontManager& fm, const std::string& winType)
  : fontManager(fm), windowType(winType) {

  cleanMainUI = std::make_unique<CleanMainUI>();

  // Register all services on first creation using new ServiceInitializer
  static bool servicesRegistered = false;
  if (!servicesRegistered) {
    ServiceInitializer::RegisterAllServices();  // Updated class name
    servicesRegistered = true;
  }
}

UIRenderer::~UIRenderer() = default;

void UIRenderer::Render(std::atomic<bool>& running) {
  if (windowType.find("Window 1") != std::string::npos) {
    cleanMainUI->RenderUI(running);
  }
  else if (windowType.find("Window 2") != std::string::npos) {
    RenderDebugWindow();
  }
}

void UIRenderer::RenderDebugWindow() {
  ImGui::Begin("Debug Info");
  ImGui::Text("🔧 Project4 Debug");
  ImGui::Separator();
  ImGui::Text("Font Support:");
  ImGui::BulletText("Emoji: %s", fontManager.IsEmojiSupported() ? "✅" : "❌");
  ImGui::BulletText("Greek: %s", fontManager.IsGreekSupported() ? "✅" : "❌");
  ImGui::BulletText("Math: %s", fontManager.IsMathSupported() ? "✅" : "❌");
  ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

  // NEW: Service registry debug info
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Service Registry Debug:");
  auto& registry = UIServiceRegistry::Instance();
  auto categories = registry.GetAllCategories();

  for (const auto& category : categories) {
    auto services = registry.GetServicesByCategory(category);
    int availableCount = 0;
    for (const auto& service : services) {
      if (service.available) availableCount++;
    }
    ImGui::BulletText("%s: %d/%d services", category.c_str(), availableCount, (int)services.size());
  }

  ImGui::End();
}