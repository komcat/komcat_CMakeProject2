// BitmapFont.cpp
#include "BitmapFont.h"
#include <iostream>

BitmapFont::BitmapFont() {
}

BitmapFont::~BitmapFont() {
  Shutdown();
}

bool BitmapFont::Initialize(SDL_Renderer* renderer) {
  this->renderer = renderer;

  CreateFontTexture();

  return fontTexture != nullptr;
}

void BitmapFont::Shutdown() {
  if (fontTexture) {
    SDL_DestroyTexture(fontTexture);
    fontTexture = nullptr;
  }
}

void BitmapFont::CreateFontTexture() {
  // Create a texture for our bitmap font
  fontTexture = SDL_CreateTexture(renderer,
    SDL_PIXELFORMAT_RGBA8888,
    SDL_TEXTUREACCESS_TARGET,
    CHARS_PER_ROW * CHAR_WIDTH,
    FONT_ROWS * CHAR_HEIGHT);

  if (!fontTexture) {
    std::cerr << "Failed to create font texture: " << SDL_GetError() << std::endl;
    return;
  }

  // Set blend mode for transparency
  SDL_SetTextureBlendMode(fontTexture, SDL_BLENDMODE_BLEND);

  // Set render target to our font texture
  SDL_SetRenderTarget(renderer, fontTexture);

  // Clear with transparent background
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
  SDL_RenderClear(renderer);

  // Draw characters in a simple 5x7 pixel font
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

  // Define simple bitmap patterns for common characters
  // We'll create a minimal but readable font

  // Space (ASCII 32) - empty, at position 0,0

  // ! (ASCII 33) - at position 1,0
  DrawCharacterPattern(1, 0, {
      "..##..",
      "..##..",
      "..##..",
      "..##..",
      "......",
      "..##..",
      "......"
    });

  // " (ASCII 34) - at position 2,0
  DrawCharacterPattern(2, 0, {
      ".####.",
      ".####.",
      "......",
      "......",
      "......",
      "......",
      "......"
    });

  // 0 (ASCII 48) - at position 0,1 (48-32=16, 16/16=1, 16%16=0)
  DrawCharacterPattern(0, 1, {
      ".####.",
      "##..##",
      "##.###",
      "###.##",
      "##..##",
      ".####.",
      "......"
    });

  // 1-9 (ASCII 49-57)
  DrawCharacterPattern(1, 1, { // 1
      "..##..",
      ".###..",
      "..##..",
      "..##..",
      "..##..",
      ".####.",
      "......"
    });

  DrawCharacterPattern(2, 1, { // 2
      ".####.",
      "##..##",
      "....##",
      ".####.",
      "##....",
      "######",
      "......"
    });

  DrawCharacterPattern(3, 1, { // 3
      ".####.",
      "##..##",
      "..###.",
      "....##",
      "##..##",
      ".####.",
      "......"
    });

  DrawCharacterPattern(4, 1, { // 4
      "##..##",
      "##..##",
      "######",
      "....##",
      "....##",
      "....##",
      "......"
    });

  DrawCharacterPattern(5, 1, { // 5
      "######",
      "##....",
      "#####.",
      "....##",
      "##..##",
      ".####.",
      "......"
    });

  DrawCharacterPattern(6, 1, { // 6
      ".####.",
      "##....",
      "#####.",
      "##..##",
      "##..##",
      ".####.",
      "......"
    });

  DrawCharacterPattern(7, 1, { // 7
      "######",
      "....##",
      "...##.",
      "..##..",
      ".##...",
      ".##...",
      "......"
    });

  DrawCharacterPattern(8, 1, { // 8
      ".####.",
      "##..##",
      ".####.",
      "##..##",
      "##..##",
      ".####.",
      "......"
    });

  DrawCharacterPattern(9, 1, { // 9
      ".####.",
      "##..##",
      "##..##",
      ".#####",
      "....##",
      ".####.",
      "......"
    });

  // : (ASCII 58)
  DrawCharacterPattern(10, 1, {
      "......",
      "..##..",
      "..##..",
      "......",
      "..##..",
      "..##..",
      "......"
    });

  // A-Z (ASCII 65-90)
  DrawCharacterPattern(1, 2, { // A (65-32=33, 33/16=2, 33%16=1)
      ".####.",
      "##..##",
      "##..##",
      "######",
      "##..##",
      "##..##",
      "......"
    });

  DrawCharacterPattern(2, 2, { // B
      "#####.",
      "##..##",
      "#####.",
      "##..##",
      "##..##",
      "#####.",
      "......"
    });

  DrawCharacterPattern(3, 2, { // C
      ".####.",
      "##..##",
      "##....",
      "##....",
      "##..##",
      ".####.",
      "......"
    });

  DrawCharacterPattern(4, 2, { // D
      "####..",
      "##.##.",
      "##..##",
      "##..##",
      "##.##.",
      "####..",
      "......"
    });

  DrawCharacterPattern(5, 2, { // E
      "######",
      "##....",
      "####..",
      "##....",
      "##....",
      "######",
      "......"
    });

  DrawCharacterPattern(6, 2, { // F
      "######",
      "##....",
      "####..",
      "##....",
      "##....",
      "##....",
      "......"
    });

  DrawCharacterPattern(7, 2, { // G
      ".####.",
      "##..##",
      "##....",
      "##.###",
      "##..##",
      ".####.",
      "......"
    });

  DrawCharacterPattern(8, 2, { // H
      "##..##",
      "##..##",
      "######",
      "##..##",
      "##..##",
      "##..##",
      "......"
    });

  DrawCharacterPattern(9, 2, { // I
      ".####.",
      "..##..",
      "..##..",
      "..##..",
      "..##..",
      ".####.",
      "......"
    });

  DrawCharacterPattern(10, 2, { // J
      "....##",
      "....##",
      "....##",
      "....##",
      "##..##",
      ".####.",
      "......"
    });

  DrawCharacterPattern(11, 2, { // K
      "##..##",
      "##.##.",
      "####..",
      "##.##.",
      "##..##",
      "##..##",
      "......"
    });

  DrawCharacterPattern(12, 2, { // L
      "##....",
      "##....",
      "##....",
      "##....",
      "##....",
      "######",
      "......"
    });

  DrawCharacterPattern(13, 2, { // M
      "##..##",
      "######",
      "######",
      "##..##",
      "##..##",
      "##..##",
      "......"
    });

  DrawCharacterPattern(14, 2, { // N
      "##..##",
      "###.##",
      "######",
      "##.###",
      "##..##",
      "##..##",
      "......"
    });

  DrawCharacterPattern(15, 2, { // O
      ".####.",
      "##..##",
      "##..##",
      "##..##",
      "##..##",
      ".####.",
      "......"
    });

  DrawCharacterPattern(0, 3, { // P
      "#####.",
      "##..##",
      "#####.",
      "##....",
      "##....",
      "##....",
      "......"
    });

  DrawCharacterPattern(1, 3, { // Q
      ".####.",
      "##..##",
      "##..##",
      "##.###",
      "##..##",
      ".#####",
      "......"
    });

  DrawCharacterPattern(2, 3, { // R
      "#####.",
      "##..##",
      "#####.",
      "##.##.",
      "##..##",
      "##..##",
      "......"
    });

  DrawCharacterPattern(3, 3, { // S
      ".####.",
      "##....",
      ".###..",
      "....##",
      "....##",
      "#####.",
      "......"
    });

  DrawCharacterPattern(4, 3, { // T
      "######",
      "..##..",
      "..##..",
      "..##..",
      "..##..",
      "..##..",
      "......"
    });

  DrawCharacterPattern(5, 3, { // U
      "##..##",
      "##..##",
      "##..##",
      "##..##",
      "##..##",
      ".####.",
      "......"
    });

  DrawCharacterPattern(6, 3, { // V
      "##..##",
      "##..##",
      "##..##",
      "##..##",
      ".####.",
      "..##..",
      "......"
    });

  DrawCharacterPattern(7, 3, { // W
      "##..##",
      "##..##",
      "##..##",
      "######",
      "######",
      "##..##",
      "......"
    });

  DrawCharacterPattern(8, 3, { // X
      "##..##",
      ".####.",
      "..##..",
      ".####.",
      "##..##",
      "##..##",
      "......"
    });

  DrawCharacterPattern(9, 3, { // Y
      "##..##",
      "##..##",
      ".####.",
      "..##..",
      "..##..",
      "..##..",
      "......"
    });

  DrawCharacterPattern(10, 3, { // Z
      "######",
      "....##",
      "...##.",
      "..##..",
      ".##...",
      "######",
      "......"
    });

  // % (ASCII 37)
  DrawCharacterPattern(5, 0, {
      "##..##",
      "##.##.",
      "..##..",
      ".##...",
      ".##.##",
      "##..##",
      "......"
    });

  // . (ASCII 46)
  DrawCharacterPattern(14, 0, {
      "......",
      "......",
      "......",
      "......",
      "......",
      "..##..",
      "......"
    });

  // Reset render target back to screen
  SDL_SetRenderTarget(renderer, nullptr);
}

void BitmapFont::DrawCharacterPattern(int col, int row, const std::vector<std::string>& pattern) {
  int startX = col * CHAR_WIDTH;
  int startY = row * CHAR_HEIGHT;

  for (int y = 0; y < pattern.size() && y < CHAR_HEIGHT; y++) {
    const std::string& line = pattern[y];
    for (int x = 0; x < line.length() && x < CHAR_WIDTH; x++) {
      if (line[x] == '#') {
        SDL_RenderDrawPoint(renderer, startX + x, startY + y);
      }
    }
  }
}


void BitmapFont::DrawText(const std::string& text, int x, int y, int scale) {
  if (!fontTexture) return;

  int currentX = x;

  for (char c : text) {
    if (c >= FIRST_CHAR && c <= LAST_CHAR) {
      SDL_Rect srcRect = GetCharRect(c);
      SDL_Rect dstRect = {
          currentX,
          y,
          CHAR_WIDTH * scale,
          CHAR_HEIGHT * scale
      };

      SDL_RenderCopy(renderer, fontTexture, &srcRect, &dstRect);
    }

    currentX += CHAR_WIDTH * scale;
  }
}

void BitmapFont::DrawTextCentered(const std::string& text, int y, int windowWidth, int scale) {
  int textWidth = GetTextWidth(text, scale);
  int x = (windowWidth - textWidth) / 2;
  DrawText(text, x, y, scale);
}

int BitmapFont::GetTextWidth(const std::string& text, int scale) const {
  return static_cast<int>(text.length()) * CHAR_WIDTH * scale;
}

int BitmapFont::GetTextHeight(int scale) const {
  return CHAR_HEIGHT * scale;
}

SDL_Rect BitmapFont::GetCharRect(char c) const {
  int index = c - FIRST_CHAR;
  int col = index % CHARS_PER_ROW;
  int row = index / CHARS_PER_ROW;

  return {
      col * CHAR_WIDTH,
      row * CHAR_HEIGHT,
      CHAR_WIDTH,
      CHAR_HEIGHT
  };
}