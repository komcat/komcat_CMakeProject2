// include/data/global_data_store.h
#pragma once
#include <string>
#include <map>
#include <mutex>

class GlobalDataStore {
private:
  // Singleton instance
  static GlobalDataStore* s_instance;
  // Map to store latest values by server ID
  std::map<std::string, float> m_latestValues;
  // Mutex for thread-safe access
  std::mutex m_mutex;

  // Private constructor
  GlobalDataStore() {}

public:
  // Get singleton instance
  static GlobalDataStore* GetInstance();

  // Set the latest value for a server
  void SetValue(const std::string& serverId, float value);

  // Get the latest value for a server
  float GetValue(const std::string& serverId, float defaultValue = 0.0f);

  // Check if a server has a value
  bool HasValue(const std::string& serverId);
};