#pragma once
#include <string>
#include <atomic>
#include <memory>
#include "FontManager.h"

class CleanMainUI;

class UIRenderer {
public:
  UIRenderer(const FontManager& fontManager, const std::string& windowType);
  ~UIRenderer();
  void Render(std::atomic<bool>& running);

private:
  const FontManager& fontManager;
  std::string windowType;
  std::unique_ptr<CleanMainUI> cleanMainUI;
  void RenderDebugWindow();
};