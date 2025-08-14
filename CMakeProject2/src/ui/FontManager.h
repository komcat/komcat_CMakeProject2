// ui/FontManager.h
#pragma once

#include "imgui.h"
#include <string>
#include <vector>

class FontManager {
public:
  struct FontLoadResult {
    bool success = false;
    std::string fontPath;
    int glyphCount = 0;
    std::string errorMessage;
  };

  FontManager();
  ~FontManager() = default;

  // Main setup function
  FontLoadResult SetupComprehensiveFonts(ImGuiIO& io);

  // Status queries
  bool IsEmojiSupported() const { return emojiSupported; }
  bool IsGreekSupported() const { return greekSupported; }
  bool IsMathSupported() const { return mathSupported; }

  // Glyph testing
  bool TestGlyph(ImWchar codepoint) const;

  // Font access
  ImFont* GetBaseFont() const { return baseFont; }
  ImFont* GetEmojiFont() const { return emojiFont; }

private:
  // Font loading
  FontLoadResult LoadBaseFont(ImGuiIO& io);
  void LoadEmojiFont(ImGuiIO& io);
  FontLoadResult BuildFontAtlas(ImGuiIO& io);

  // Unicode ranges
  static const ImWchar* GetComprehensiveBaseRanges();
  static const ImWchar* GetComprehensiveEmojiRanges();

  // Font paths
  std::vector<std::string> GetBaseFontCandidates() const;
  std::vector<std::string> GetEmojiFontCandidates() const;

  // State
  bool emojiSupported = false;
  bool greekSupported = false;
  bool mathSupported = false;
  ImFont* baseFont = nullptr;
  ImFont* emojiFont = nullptr;
};
