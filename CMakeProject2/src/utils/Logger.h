// utils/Logger.h
#pragma once

#include <string>
#include <mutex>

class Logger {
public:
  enum class Level {
    INFO,
    WARNING,
    EERROR,
    SUCCESS
  };

  static void Info(const std::wstring& message);
  static void Warning(const std::wstring& message);
  static void Error(const std::wstring& message);
  static void Success(const std::wstring& message);
  static void Log(Level level, const std::wstring& message);

private:
  static std::mutex log_mutex;
  static std::wstring GetLevelPrefix(Level level);
};
