// src/ui/CleanMainUI.cpp - FIXED VERSION
#include "CleanMainUI.h"
#include "imgui.h"
#include <iostream>
#include <ctime>

void CleanMainUI::RenderUI(std::atomic<bool>& running) {
  // Handle escape key for navigation
  HandleKeyboardInput();

  // Full screen window
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  ImGui::Begin("CleanMainApplication", nullptr, window_flags);

  // Top navigation
  if (currentCategory == "Home") {
    RenderTopMenuBar();
  }
  else {
    RenderBackButton();
  }

  RenderDateTime();
  RenderBreadcrumbs();
  ImGui::Separator();
  RenderMainContent(running);

  ImGui::End();
}

void CleanMainUI::RenderTopMenuBar() {
  ImGui::Dummy(ImVec2(0.0f, 12.0f));

  auto& registry = UIServiceRegistry::Instance();
  auto categories = registry.GetAllCategories();

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 10));
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.3f, 0.8f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.4f, 0.9f, 1.0f));

  // Dynamically create buttons for each category
  for (const auto& category : categories) {
    std::string displayName = GetCategoryDisplayName(category);
    std::string icon = GetCategoryIcon(category);

    // FIXED: Use proper UTF-8 string concatenation for emojis
    std::string buttonText = icon + " " + displayName;

    if (ImGui::Button(buttonText.c_str(), ImVec2(150, 40))) {
      NavigateToCategory(category);
    }
    ImGui::SameLine();
  }

  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar();
}

void CleanMainUI::RenderBackButton() {
  ImGui::Dummy(ImVec2(0.0f, 15.0f));

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(15, 8));
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));

  if (ImGui::Button("<< BACK")) {
    NavigateBack();
  }

  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar();
}

void CleanMainUI::RenderDateTime() {
  time_t rawtime;
  struct tm timeinfo;
  char buffer[80];

  time(&rawtime);
#ifdef _WIN32
  localtime_s(&timeinfo, &rawtime);
#else
  timeinfo = *localtime(&rawtime);
#endif

  strftime(buffer, sizeof(buffer), "%d %b %Y\n%H:%M:%S", &timeinfo);

  ImVec2 textSize = ImGui::CalcTextSize(buffer);
  ImGui::SameLine(ImGui::GetWindowWidth() - textSize.x - 20);
  ImGui::SetCursorPosY(10);
  ImGui::Text("%s", buffer);
}

void CleanMainUI::RenderBreadcrumbs() {
  std::string breadcrumb = "Home";

  if (currentCategory != "Home") {
    breadcrumb += " > " + GetCategoryDisplayName(currentCategory);

    if (!currentService.empty()) {
      auto service = UIServiceRegistry::Instance().GetService(currentService);
      if (service) {
        breadcrumb += " > " + service->GetDisplayName();
      }
    }
  }

  ImGui::Text("%s", breadcrumb.c_str());

  // Add keyboard hints
  if (currentCategory != "Home" || !currentService.empty()) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), " (Press ESC to go back)");
  }

  if (currentCategory == "Home" && currentService.empty()) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), " (Press 1-6 for quick navigation)");
  }
}

void CleanMainUI::RenderMainContent(std::atomic<bool>& running) {
  ImGui::SetCursorPosY(100);

  if (currentCategory == "Home") {
    RenderHomePage();
  }
  else if (currentService.empty()) {
    RenderCategoryPage(currentCategory);
  }
  else {
    RenderServicePage(currentService, running);
  }
}

void CleanMainUI::RenderHomePage() {
  ImGui::SetWindowFontScale(2.0f);
  ImGui::Text("Welcome to Project4 UI");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::Text("Select a category from the top menu to begin:");

  // Emoji test
  ImGui::Spacing();
  ImGui::Text("Emoji Test:");
  ImGui::TextUnformatted(reinterpret_cast<const char*>(u8"🎯 🤖 🦾 ⚡ 📷 👁️ 📊 🔧"));

  // Test if individual emojis work
  ImGui::Text("Individual tests:");
  ImGui::SameLine(); ImGui::TextUnformatted(reinterpret_cast<const char*>(u8"🎯"));
  ImGui::SameLine(); ImGui::TextUnformatted(reinterpret_cast<const char*>(u8"🤖"));
  ImGui::SameLine(); ImGui::TextUnformatted(reinterpret_cast<const char*>(u8"⚡"));

  auto& registry = UIServiceRegistry::Instance();
  auto categories = registry.GetAllCategories();

  for (const auto& category : categories) {
    auto services = registry.GetServicesByCategory(category);
    int availableCount = 0;
    for (const auto& service : services) {
      if (service.available) availableCount++;
    }

    ImGui::BulletText("%s %s - %d services available (%d total)",
      GetCategoryIcon(category).c_str(),
      GetCategoryDisplayName(category).c_str(),
      availableCount,
      (int)services.size());
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::SetWindowFontScale(1.3f);
  ImGui::Text("System Status");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ UI Service Registry: Ready");
  ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Clean Architecture: Active");

  int totalServices = 0;
  int availableServices = 0;
  for (const auto& category : categories) {
    auto services = registry.GetServicesByCategory(category);
    totalServices += services.size();
    for (const auto& service : services) {
      if (service.available) availableServices++;
    }
  }

  ImGui::Text("✓ Services: %d available of %d registered", availableServices, totalServices);

  // Font system debug info
  ImGui::Spacing();
  ImGui::Text("Font System Debug:");
  ImGui::BulletText("Font Atlas Built: %s", ImGui::GetIO().Fonts->IsBuilt() ? "Yes" : "No");
  ImGui::BulletText("Font Count: %d", ImGui::GetIO().Fonts->Fonts.Size);
  if (ImGui::GetIO().FontDefault) {
    ImGui::BulletText("Default Font Glyphs: %d", ImGui::GetIO().FontDefault->Glyphs.Size);

    // Test specific emoji glyphs
    ImFont* font = ImGui::GetIO().FontDefault;
    ImWchar testChars[] = { 0x1F3AF, 0x1F916, 0x26A1, 0x1F4F7, 0 };  // 🎯🤖⚡📷

    for (int i = 0; testChars[i] != 0; i++) {
      const ImFontGlyph* glyph = font->FindGlyph(testChars[i]);
      ImGui::BulletText("U+%04X: %s", testChars[i], glyph ? "Found" : "Missing");
    }
  }
  else {
    ImGui::BulletText("Default Font: NULL");
  }
}

void CleanMainUI::RenderCategoryPage(const std::string& category) {
  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("%s %s", GetCategoryIcon(category).c_str(), GetCategoryDisplayName(category).c_str());
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::Text("Select a service:");
  ImGui::Spacing();

  auto& registry = UIServiceRegistry::Instance();
  auto services = registry.GetServicesByCategory(category);

  for (size_t i = 0; i < services.size(); ++i) {
    const auto& service = services[i];

    std::string buttonText = service.icon + " " + service.displayName;

    // Color code based on availability
    if (service.available) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
    }
    else {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    }

    if (ImGui::Button(buttonText.c_str(), ImVec2(250, 50))) {
      if (service.available) {
        NavigateToService(service.serviceName);
      }
    }

    ImGui::PopStyleColor(2);

    if (!service.available) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(Not Available)");
    }
  }
}

void CleanMainUI::RenderServicePage(const std::string& serviceName, std::atomic<bool>& running) {
  auto& registry = UIServiceRegistry::Instance();
  auto service = registry.GetService(serviceName);

  if (service && service->IsAvailable()) {
    service->RenderUI();
  }
  else {
    ImGui::SetWindowFontScale(1.5f);
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Service Not Available");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Spacing();
    if (service) {
      ImGui::Text("Service '%s' is registered but not available.", service->GetDisplayName().c_str());
      ImGui::Text("This typically means required dependencies are not initialized.");
    }
    else {
      ImGui::Text("Service '%s' is not registered.", serviceName.c_str());
    }
  }
}

// FIXED: Updated helper methods with proper UTF-8 emoji strings
std::string CleanMainUI::GetCategoryDisplayName(const std::string& category) {
  if (category == "Manual") return "Manual Control";
  if (category == "Data") return "Data & Instrument";
  if (category == "Program") return "Programming";
  if (category == "Config") return "Configuration";
  if (category == "Vision") return "Vision System";
  return category;
}

std::string CleanMainUI::GetCategoryIcon(const std::string& category) {
  // FIXED: Use proper UTF-8 literals for emojis
  if (category == "Manual") return reinterpret_cast<const char*>(u8"🕹️");
  if (category == "Data") return reinterpret_cast<const char*>(u8"📊");
  if (category == "Program") return reinterpret_cast<const char*>(u8"⚙️");
  if (category == "Config") return reinterpret_cast<const char*>(u8"🔧");
  if (category == "Vision") return reinterpret_cast<const char*>(u8"👁️");
  if (category == "Run") return reinterpret_cast<const char*>(u8"🚀");
  return reinterpret_cast<const char*>(u8"📋");
}

void CleanMainUI::NavigateToCategory(const std::string& category) {
  currentCategory = category;
  currentService = "";
}

void CleanMainUI::NavigateToService(const std::string& serviceName) {
  currentService = serviceName;
}

void CleanMainUI::NavigateBack() {
  if (!currentService.empty()) {
    currentService = "";
  }
  else {
    currentCategory = "Home";
  }
}

// NEW: Handle keyboard input for navigation
void CleanMainUI::HandleKeyboardInput() {
  // Check if escape key was pressed
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    // Only navigate back if we're not already at home
    if (currentCategory != "Home" || !currentService.empty()) {
      NavigateBack();
    }
  }

  // Optional: Add more keyboard shortcuts
  if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
    // F1 = Go to home
    currentCategory = "Home";
    currentService = "";
  }

  // Optional: Number keys for quick category navigation (when at home)
  if (currentCategory == "Home" && currentService.empty()) {
    auto& registry = UIServiceRegistry::Instance();
    auto categories = registry.GetAllCategories();

    // Sort categories for consistent numbering
    std::sort(categories.begin(), categories.end());

    for (size_t i = 0; i < categories.size() && i < 9; i++) {
      ImGuiKey key = (ImGuiKey)(ImGuiKey_1 + i);
      if (ImGui::IsKeyPressed(key)) {
        NavigateToCategory(categories[i]);
        break;
      }
    }
  }
}