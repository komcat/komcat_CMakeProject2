// ============================================
// src/utils/Logger.cpp - CREATE THIS FILE
// ============================================
#include "Logger.h"
#include "Unicode.h"
#include <iostream>

std::mutex Logger::log_mutex;

void Logger::Info(const std::wstring& message) {
  Log(Level::INFO, message);
}

void Logger::Warning(const std::wstring& message) {
  Log(Level::WARNING, message);
}

void Logger::Error(const std::wstring& message) {
  Log(Level::EERROR, message);
}

void Logger::Success(const std::wstring& message) {
  Log(Level::SUCCESS, message);
}

void Logger::Log(Level level, const std::wstring& message) {
  std::lock_guard<std::mutex> lock(log_mutex);

  std::wstring fullMessage = GetLevelPrefix(level) + message + L"\n";
  UnicodeUtils::PrintUnicode(fullMessage);
}

std::wstring Logger::GetLevelPrefix(Level level) {
  switch (level) {
  case Level::INFO:    return L"";
  case Level::WARNING: return L"⚠️ ";
  case Level::EERROR:   return L"❌ ";
  case Level::SUCCESS: return L"✅ ";
  default:             return L"";
  }
}