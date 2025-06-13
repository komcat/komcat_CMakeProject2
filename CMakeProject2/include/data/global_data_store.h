// include/data/global_data_store.h
#pragma once
#include <string>
#include <map>
#include <mutex>
#include <vector>  // Add this line

class GlobalDataStore {
private:
  // Singleton instance
  static GlobalDataStore* s_instance;
  // Map to store latest values by server ID
  std::map<std::string, float> m_latestValues;
  // Mutex for thread-safe access - make it mutable
  mutable std::mutex m_mutex;  // Add 'mutable' keyword here
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
  // Get list of all available data channels
  std::vector<std::string> GetAvailableChannels() const;
};