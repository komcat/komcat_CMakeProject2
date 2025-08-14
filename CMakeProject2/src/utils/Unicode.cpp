
// ============================================
// src/utils/Unicode.cpp - CREATE THIS FILE  
// ============================================
#include "Unicode.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

namespace UnicodeUtils {

  void InitializeConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);

    // Enable virtual terminal processing for color support
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif
  }

  std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return std::wstring();

#ifdef _WIN32
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
#else
    // Simple fallback for non-Windows
    return std::wstring(str.begin(), str.end());
#endif
  }

  void PrintUnicode(const std::wstring& message) {
#ifdef _WIN32
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD n_written;
    WriteConsoleW(handle, message.c_str(), (DWORD)message.length(), &n_written, NULL);
#else
    std::wcout << message;
#endif
  }

}