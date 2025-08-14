// utils/Unicode.h
#pragma once

#include <string>

namespace UnicodeUtils {
  // Initialize console for Unicode output
  void InitializeConsole();

  // Convert string to wstring with proper encoding
  std::wstring StringToWString(const std::string& str);

  // Print Unicode text to console
  void PrintUnicode(const std::wstring& message);
}
