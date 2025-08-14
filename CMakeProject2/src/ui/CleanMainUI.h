// src/ui/CleanMainUI.h
#pragma once
#include "services/UIServiceRegistry.h"
#include <string>
#include <atomic>

class CleanMainUI {
public:
  CleanMainUI() = default;
  ~CleanMainUI() = default;

  void RenderUI(std::atomic<bool>& running);

private:
  // Simple state - no complex enums needed
  std::string currentCategory = "Home";
  std::string currentService = "";

  // UI rendering methods
  void RenderTopMenuBar();
  void RenderBreadcrumbs();
  void RenderMainContent(std::atomic<bool>& running);
  void RenderBackButton();
  void RenderDateTime();

  // Content rendering
  void RenderHomePage();
  void RenderCategoryPage(const std::string& category);
  void RenderServicePage(const std::string& serviceName, std::atomic<bool>& running);

  // Helpers
  std::string GetCategoryDisplayName(const std::string& category);
  std::string GetCategoryIcon(const std::string& category);
  void NavigateToCategory(const std::string& category);
  void NavigateToService(const std::string& serviceName);
  void NavigateBack();
  void HandleKeyboardInput();
};