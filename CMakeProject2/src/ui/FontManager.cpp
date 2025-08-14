
// ui/FontManager.cpp
#include "FontManager.h"
#include "../utils/Logger.h"
#include "../utils/Unicode.h"
#include <filesystem>

#ifdef IMGUI_ENABLE_FREETYPE
#include "misc/freetype/imgui_freetype.h"
#endif

FontManager::FontManager()
  : emojiSupported(false)
  , greekSupported(false)
  , mathSupported(false)
  , baseFont(nullptr)
  , emojiFont(nullptr) {
}

FontManager::FontLoadResult FontManager::SetupComprehensiveFonts(ImGuiIO& io) {
  Logger::Info(L"🔧 Starting comprehensive font setup...");

  // Clear any existing fonts to start fresh
  io.Fonts->Clear();

  // STEP 1: Load base font with Latin + Greek + Math support
  FontLoadResult result = LoadBaseFont(io);
  if (!result.success) {
    Logger::Warning(L"Base font loading failed, using ImGui default");
    baseFont = io.Fonts->AddFontDefault();
  }

  // STEP 2: Load emoji font with proper merging
  LoadEmojiFont(io);

  // STEP 3: Build font atlas with comprehensive error checking
  return BuildFontAtlas(io);
}

FontManager::FontLoadResult FontManager::LoadBaseFont(ImGuiIO& io) {
  FontLoadResult result;

  auto baseFontCandidates = GetBaseFontCandidates();
  const ImWchar* comprehensive_base_ranges = GetComprehensiveBaseRanges();

  for (const std::string& fontPath : baseFontCandidates) {
    if (std::filesystem::exists(fontPath)) {
      Logger::Info(L"🔤 Attempting base font: " + UnicodeUtils::StringToWString(fontPath));

      ImFontConfig baseConfig;
      baseConfig.PixelSnapH = true;
      baseConfig.OversampleH = 2;
      baseConfig.OversampleV = 1;

      try {
        baseFont = io.Fonts->AddFontFromFileTTF(
          fontPath.c_str(),
          16.0f,
          &baseConfig,
          comprehensive_base_ranges
        );

        if (baseFont) {
          result.success = true;
          result.fontPath = fontPath;
          Logger::Success(L"Base font loaded: " + UnicodeUtils::StringToWString(fontPath));

          // Test Greek support
          const ImFontGlyph* alphaGlyph = baseFont->FindGlyph(0x03B1); // α
          if (alphaGlyph) {
            greekSupported = true;
            Logger::Success(L"Greek letters supported in base font");
          }

          // Test math support  
          const ImFontGlyph* plusMinusGlyph = baseFont->FindGlyph(0x00B1); // ±
          if (plusMinusGlyph) {
            mathSupported = true;
            Logger::Success(L"Math symbols supported in base font");
          }

          break;
        }
      }
      catch (...) {
        Logger::Error(L"Exception loading base font: " + UnicodeUtils::StringToWString(fontPath));
      }
    }
  }

  return result;
}


void FontManager::LoadEmojiFont(ImGuiIO& io) {
  auto emojiFontCandidates = GetEmojiFontCandidates();
  const ImWchar* comprehensive_emoji_ranges = GetComprehensiveEmojiRanges();

  Logger::Info(L"🎨 Starting emoji font loading...");

  for (const std::string& emojiPath : emojiFontCandidates) {
    Logger::Info(L"🔍 Checking: " + UnicodeUtils::StringToWString(emojiPath));

    if (std::filesystem::exists(emojiPath)) {
      Logger::Success(L"📁 File found, attempting to load...");

      ImFontConfig emojiConfig;
      emojiConfig.MergeMode = true;  // CRITICAL: Merge with base font
      emojiConfig.PixelSnapH = true;
      emojiConfig.GlyphMinAdvanceX = 16.0f; // Monospace emoji

#ifdef IMGUI_ENABLE_FREETYPE
      // Enable color emoji rendering
      emojiConfig.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
      Logger::Success(L"🎨 FreeType color emoji flags enabled");
#else
      Logger::Warning(L"⚠️ IMGUI_ENABLE_FREETYPE not defined - no color emoji support");
#endif

      try {
        emojiFont = io.Fonts->AddFontFromFileTTF(
          emojiPath.c_str(),
          16.0f,
          &emojiConfig,
          comprehensive_emoji_ranges
        );

        if (emojiFont) {
          emojiSupported = true;
          Logger::Success(L"✅ Emoji font loaded successfully: " + UnicodeUtils::StringToWString(emojiPath));
          Logger::Info(L"   Font size: 16.0f");
          Logger::Info(L"   Merge mode: enabled");
          Logger::Info(L"   Color flags: " + std::to_wstring(emojiConfig.FontBuilderFlags));
          break;
        }
        else {
          Logger::Error(L"❌ AddFontFromFileTTF returned null for: " + UnicodeUtils::StringToWString(emojiPath));
        }
      }
      catch (const std::exception& e) {
        Logger::Error(L"❌ Exception loading emoji font: " + UnicodeUtils::StringToWString(e.what()));
      }
      catch (...) {
        Logger::Error(L"❌ Unknown exception loading emoji font: " + UnicodeUtils::StringToWString(emojiPath));
      }
    }
    else {
      Logger::Warning(L"📁 File not found: " + UnicodeUtils::StringToWString(emojiPath));
    }
  }

  if (!emojiSupported) {
    Logger::Warning(L"⚠️ No emoji font loaded - emojis will show as simple glyphs");
  }
}


FontManager::FontLoadResult FontManager::BuildFontAtlas(ImGuiIO& io) {
  FontLoadResult result;

  Logger::Info(L"🔨 Building font atlas...");

  // CRITICAL: Ensure FreeType is configured before building
#ifdef IMGUI_ENABLE_FREETYPE
  io.Fonts->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType();
  io.Fonts->FontBuilderFlags = ImGuiFreeTypeBuilderFlags_LightHinting;
  Logger::Success(L"FreeType renderer configured");
#endif

  // Build the atlas
  bool buildSuccess = io.Fonts->Build();

  if (!buildSuccess) {
    result.success = false;
    result.errorMessage = "Font atlas build failed";
    Logger::Error(L"Font atlas build FAILED");
    return result;
  }

  // Verify atlas was built
  unsigned char* pixels;
  int width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

  if (!pixels || width <= 0 || height <= 0) {
    result.success = false;
    result.errorMessage = "Font atlas texture data invalid";
    Logger::Error(L"Font atlas texture data is invalid");
    return result;
  }

  result.success = true;
  Logger::Success(L"Font atlas built successfully");
  Logger::Info(L"   Atlas size: " + std::to_wstring(width) + L"x" + std::to_wstring(height));
  Logger::Info(L"   Total fonts: " + std::to_wstring(io.Fonts->Fonts.Size));

  // Debug: Show glyph counts
  for (int i = 0; i < io.Fonts->Fonts.Size; i++) {
    ImFont* font = io.Fonts->Fonts[i];
    Logger::Info(L"   Font " + std::to_wstring(i) + L": " +
      std::to_wstring(font->Glyphs.Size) + L" glyphs, " +
      std::to_wstring((int)font->FontSize) + L"px");
  }

  // Set default font to the base font (with merged emoji)
  if (baseFont) {
    io.FontDefault = baseFont;
    Logger::Success(L"Default font set to base font with merged emoji");
  }

  return result;
}

bool FontManager::TestGlyph(ImWchar codepoint) const {
  ImGuiIO& io = ImGui::GetIO();
  if (io.FontDefault) {
    return io.FontDefault->FindGlyph(codepoint) != nullptr;
  }
  return false;
}

std::vector<std::string> FontManager::GetBaseFontCandidates() const {
  return {
      "C:/Windows/Fonts/arial.ttf",           // Arial - excellent Unicode support
      "C:/Windows/Fonts/calibri.ttf",         // Calibri - good Greek support  
      "C:/Windows/Fonts/segoeui.ttf",         // Segoe UI - Microsoft's Unicode font
      "C:/Windows/Fonts/times.ttf",           // Times - classical Greek support
      "assets/fonts/Roboto-Regular.ttf"       // Project font
  };
}

std::vector<std::string> FontManager::GetEmojiFontCandidates() const {
  std::vector<std::string> candidates = {
      //"assets/fonts/NotoColorEmoji.ttf",       // Your font is here
      //"assets/fonts/AppleColorEmoji.ttc",
      "C:/Windows/Fonts/seguiemj.ttf",
      "C:/Windows/Fonts/segoeui.ttf"
  };

  // ADD DEBUG OUTPUT
  Logger::Info(L"🔍 Checking emoji font candidates:");
  for (const auto& path : candidates) {
    bool exists = std::filesystem::exists(path);
    Logger::Info(L"   " + UnicodeUtils::StringToWString(path) + L": " +
      (exists ? L"✅ EXISTS" : L"❌ NOT FOUND"));
  }

  return candidates;
}

const ImWchar* FontManager::GetComprehensiveBaseRanges() {
  static const ImWchar comprehensive_base_ranges[] = {
      0x0020, 0x00FF, // Basic Latin + Latin Supplement
      0x0100, 0x017F, // Latin Extended-A
      0x0180, 0x024F, // Latin Extended-B
      0x0370, 0x03FF, // Greek and Coptic
      0x1F00, 0x1FFF, // Greek Extended
      0x2000, 0x206F, // General Punctuation
      0x2070, 0x209F, // Superscripts and Subscripts
      0x20A0, 0x20CF, // Currency Symbols
      0x2100, 0x214F, // Letterlike Symbols
      0x2150, 0x218F, // Number Forms
      0x2190, 0x21FF, // Arrows
      0x2200, 0x22FF, // Mathematical Operators
      0x2300, 0x23FF, // Miscellaneous Technical
      0x2460, 0x24FF, // Enclosed Alphanumerics
      0x25A0, 0x25FF, // Geometric Shapes
      0x2600, 0x26FF, // Miscellaneous Symbols
      0, // Terminator
  };
  return comprehensive_base_ranges;
}

const ImWchar* FontManager::GetComprehensiveEmojiRanges() {
  static const ImWchar comprehensive_emoji_ranges[] = {
    // Core Emoji Blocks
    0x1F600, 0x1F64F, // Emoticons
    0x1F300, 0x1F5FF, // Misc Symbols and Pictographs
    0x1F680, 0x1F6FF, // Transport and Map Symbols
    0x1F700, 0x1F77F, // Alchemical Symbols
    0x1F780, 0x1F7FF, // Geometric Shapes Extended
    0x1F800, 0x1F8FF, // Supplemental Arrows-C
    0x1F900, 0x1F9FF, // Supplemental Symbols and Pictographs
    0x1FA00, 0x1FA6F, // Chess Symbols
    0x1FA70, 0x1FAFF, // Symbols and Pictographs Extended-A
    0x1FB00, 0x1FBFF, // Symbols and Pictographs Extended-B

    // Additional Symbol Blocks
    0x2600, 0x26FF,   // Miscellaneous Symbols
    0x2700, 0x27BF,   // Dingbats
    0x2B00, 0x2BFF,   // Miscellaneous Symbols and Arrows
    0x1F100, 0x1F1FF, // Enclosed Alphanumeric Supplement
    0x1F200, 0x1F2FF, // Enclosed Ideographic Supplement

    // Technical and UI Symbols
    0x2190, 0x21FF,   // Arrows
    0x2300, 0x23FF,   // Miscellaneous Technical
    0x25A0, 0x25FF,   // Geometric Shapes
    0x2460, 0x24FF,   // Enclosed Alphanumerics

    // Special Characters
    0x2010, 0x201F,   // Punctuation
    0x2020, 0x206F,   // General Punctuation
    0x20A0, 0x20CF,   // Currency Symbols
    0x2100, 0x214F,   // Letterlike Symbols

    // Variation Selectors (CRITICAL for proper emoji display)
    0xFE00, 0xFE0F,   // Variation Selectors
    0xE0100, 0xE01EF, // Variation Selectors Supplement

    // Combining Characters
    0x200D, 0x200D,   // Zero Width Joiner (for emoji sequences)
    0x20E3, 0x20E3,   // Combining Enclosing Keycap

    0, // Terminator
  };
  return comprehensive_emoji_ranges;
}