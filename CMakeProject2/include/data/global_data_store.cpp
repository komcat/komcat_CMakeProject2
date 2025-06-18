// src/data/global_data_store.cpp 
#include "include/data/global_data_store.h"
#include <vector>
#include <mutex>
#include <iostream>
#include <set>

// Initialize static instance
GlobalDataStore* GlobalDataStore::s_instance = nullptr;

GlobalDataStore* GlobalDataStore::GetInstance() {
  if (s_instance == nullptr) {
    s_instance = new GlobalDataStore();
    // Debug message when creating instance
    if (s_instance->m_showDebug) {
      std::cout << "[DEBUG GlobalDataStore] Created singleton instance" << std::endl;
    }
  }
  return s_instance;
}

void GlobalDataStore::SetValue(const std::string& serverId, float value) {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Debug: Track when GPIB-Current is being set
  static int setCounter = 0;
  setCounter++;

  bool wasNew = (m_latestValues.find(serverId) == m_latestValues.end());

  m_latestValues[serverId] = value;

  // Log periodically or for new channels
  if (m_showDebug && (wasNew || (setCounter % 60 == 0 && serverId == "GPIB-Current"))) {
    std::cout << "[DEBUG GlobalDataStore] SetValue: " << serverId << " = " << value;
    if (wasNew) {
      std::cout << " (NEW CHANNEL, total channels: " << m_latestValues.size() << ")";
    }
    std::cout << std::endl;
  }
}

float GlobalDataStore::GetValue(const std::string& serverId, float defaultValue) {
  std::lock_guard<std::mutex> lock(m_mutex);
  auto it = m_latestValues.find(serverId);
  if (it != m_latestValues.end()) {
    return it->second;
  }

  // Debug: Log when a requested channel is not found
  if (m_showDebug) {
    static std::set<std::string> loggedMissing;
    if (loggedMissing.find(serverId) == loggedMissing.end()) {
      std::cout << "[DEBUG GlobalDataStore] GetValue: Channel '" << serverId
        << "' not found, returning default: " << defaultValue << std::endl;
      std::cout << "[DEBUG GlobalDataStore] Available channels: ";
      for (const auto& [key, value] : m_latestValues) {
        std::cout << key << " ";
      }
      std::cout << std::endl;
      loggedMissing.insert(serverId);
    }
  }

  return defaultValue;
}

bool GlobalDataStore::HasValue(const std::string& serverId) {
  std::lock_guard<std::mutex> lock(m_mutex);
  bool exists = m_latestValues.find(serverId) != m_latestValues.end();

  // Debug log for specific checks
  if (m_showDebug) {
    static int hasValueCounter = 0;
    hasValueCounter++;
    if (hasValueCounter % 60 == 0 && serverId == "GPIB-Current") {
      std::cout << "[DEBUG GlobalDataStore] HasValue: " << serverId << " = " << exists << std::endl;
    }
  }

  return exists;
}

std::vector<std::string> GlobalDataStore::GetAvailableChannels() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  std::vector<std::string> channels;

  for (const auto& [key, value] : m_latestValues) {
    channels.push_back(key);
  }

  // Debug: Log the channels being returned
  if (m_showDebug) {
    static int getChannelsCounter = 0;
    getChannelsCounter++;
    if (getChannelsCounter % 60 == 0) {
      std::cout << "[DEBUG GlobalDataStore] GetAvailableChannels returning "
        << channels.size() << " channels: ";
      for (const auto& ch : channels) {
        std::cout << ch << " ";
      }
      std::cout << std::endl;
    }
  }

  return channels;
}