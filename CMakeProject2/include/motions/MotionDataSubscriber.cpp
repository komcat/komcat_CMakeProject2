// src/motions/MotionDataSubscriber.cpp
#include "../../include/motions/MotionDataSubscriber.h"
#include "../../include/data/global_data_store.h"

MotionDataSubscriber::MotionDataSubscriber(const std::string& subscriberType)
  : m_subscriberType(subscriberType)
  , m_enabled(true) {
  m_dataStore = GlobalDataStore::GetInstance();
}

MotionDataSubscriber::~MotionDataSubscriber() {
}

void MotionDataSubscriber::SetEnabled(bool enabled) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_enabled = enabled;
}

bool MotionDataSubscriber::IsEnabled() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_enabled;
}

void MotionDataSubscriber::StoreValue(const std::string& key, float value) {
  if (m_dataStore && m_enabled) {
    m_dataStore->SetValue(key, value);
  }
}