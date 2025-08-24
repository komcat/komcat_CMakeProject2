// BitmapFont.h
#pragma once

#include <SDL.h>
#include <string>
#include <vector>

class BitmapFont {
private:
  SDL_Texture* fontTexture = nullptr;
  SDL_Renderer* renderer = nullptr;

  // Font metrics
  static const int CHAR_WIDTH = 8;
  static const int CHAR_HEIGHT = 12;
  static const int CHARS_PER_ROW = 16;
  static const int FONT_ROWS = 6;

  // Character mapping - ASCII 32-127
  static const int FIRST_CHAR = 32; // Space character
  static const int LAST_CHAR = 126;  // Tilde character

public:
  BitmapFont();
  ~BitmapFont();

  bool Initialize(SDL_Renderer* renderer);
  void Shutdown();

  // Text rendering methods
  void DrawText(const std::string& text, int x, int y, int scale = 1);
  void DrawTextCentered(const std::string& text, int y, int windowWidth, int scale = 1);

  // Utility methods
  int GetTextWidth(const std::string& text, int scale = 1) const;
  int GetTextHeight(int scale = 1) const;

private:
  void CreateFontTexture();
  void DrawCharacterPattern(int col, int row, const std::vector<std::string>& pattern);
  SDL_Rect GetCharRect(char c) const;
};