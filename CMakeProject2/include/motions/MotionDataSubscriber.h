// include/motions/MotionDataSubscriber.h
#pragma once
#include "IPositionSubscriber.h"
#include <string>
#include <map>
#include <mutex>

// Forward declaration only - no include
class GlobalDataStore;

class MotionDataSubscriber : public IPositionSubscriber {
protected:
  GlobalDataStore* m_dataStore;
  std::string m_subscriberType;  // "PI", "ACS", etc.
  bool m_enabled;
  mutable std::mutex m_mutex;

public:
  // Just declare, don't implement
  MotionDataSubscriber(const std::string& subscriberType);
  virtual ~MotionDataSubscriber();

  void SetEnabled(bool enabled);
  bool IsEnabled() const;

protected:
  void StoreValue(const std::string& key, float value);
};