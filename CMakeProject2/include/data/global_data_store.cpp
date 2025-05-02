// src/data/global_data_store.cpp 
#include "include/data/global_data_store.h"

// Initialize static instance
GlobalDataStore* GlobalDataStore::s_instance = nullptr;

GlobalDataStore* GlobalDataStore::GetInstance() {
  if (s_instance == nullptr) {
    s_instance = new GlobalDataStore();
  }
  return s_instance;
}

void GlobalDataStore::SetValue(const std::string& serverId, float value) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_latestValues[serverId] = value;
}

float GlobalDataStore::GetValue(const std::string& serverId, float defaultValue) {
  std::lock_guard<std::mutex> lock(m_mutex);
  auto it = m_latestValues.find(serverId);
  if (it != m_latestValues.end()) {
    return it->second;
  }
  return defaultValue;
}

bool GlobalDataStore::HasValue(const std::string& serverId) {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_latestValues.find(serverId) != m_latestValues.end();
}