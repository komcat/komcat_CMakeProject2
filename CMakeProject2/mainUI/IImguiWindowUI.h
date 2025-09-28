// IImguiWindowUI.h
#pragma once

#include <string>

class IImguiWindowUI {
public:
  // Default constructor
  IImguiWindowUI() = default;

  // Virtual destructor
  virtual ~IImguiWindowUI() = default;

  // Core UI functions that all ImGui windows should implement
  virtual void Render() = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual bool IsVisible() const = 0;

  // Optional: Get window name/title
  virtual const std::string& GetName() const {
    static std::string defaultName = "ImGui Window";
    return defaultName;
  }

  // Optional: Toggle visibility (can be overridden if needed)
  virtual void Toggle() {
    if (IsVisible()) {
      Hide();
    }
    else {
      Show();
    }
  }
};