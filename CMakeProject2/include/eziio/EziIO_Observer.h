#pragma once

#include <vector>
#include <functional>
#include <mutex>
#include <memory>
#include <string>
#include <map>

// Forward declarations
class EziIODevice;

// Pin change event data
struct PinChangeEvent {
  std::string deviceName;
  int deviceId;
  int pinNumber;
  bool isInput;      // true for input pin, false for output pin
  bool newState;     // true = ON, false = OFF
  bool previousState;
  uint32_t timestamp; // Optional: milliseconds since epoch
};

// Observer interface
class IEziIOObserver {
public:
  virtual ~IEziIOObserver() = default;
  virtual void onPinStateChanged(const PinChangeEvent& event) = 0;
};

// Publisher class - integrated into EziIODevice or EziIOManager
class EziIOPublisher {
public:
  EziIOPublisher() = default;
  ~EziIOPublisher() = default;

  // Subscribe/unsubscribe observers
  void subscribe(std::shared_ptr<IEziIOObserver> observer);
  void unsubscribe(std::shared_ptr<IEziIOObserver> observer);

  // Subscribe with lambda function (convenience method)
  void subscribeCallback(std::function<void(const PinChangeEvent&)> callback);

  // Clear all observers
  void clearObservers();

  // Get observer count
  size_t getObserverCount() const;

protected:
  // Called by EziIODevice when pin state changes
  void notifyPinChange(const PinChangeEvent& event);

private:
  mutable std::mutex m_observersMutex;
  std::vector<std::shared_ptr<IEziIOObserver>> m_observers;
  std::vector<std::function<void(const PinChangeEvent&)>> m_callbacks;
};

// Concrete observer example for logging
class EziIOLoggerObserver : public IEziIOObserver {
public:
  EziIOLoggerObserver(const std::string& logPrefix = "[PIN_CHANGE]");
  void onPinStateChanged(const PinChangeEvent& event) override;

private:
  std::string m_logPrefix;
};

// Concrete observer for statistics tracking
class EziIOStatsObserver : public IEziIOObserver {
public:
  struct PinStats {
    int changeCount = 0;
    int onCount = 0;
    int offCount = 0;
    uint32_t lastChangeTime = 0;
  };

  void onPinStateChanged(const PinChangeEvent& event) override;
  PinStats getStats(const std::string& deviceName, int pinNumber, bool isInput) const;
  void resetStats();

private:
  mutable std::mutex m_statsMutex;
  // Key format: "deviceName_pinNumber_I/O"
  std::map<std::string, PinStats> m_pinStats;

  std::string makeKey(const std::string& device, int pin, bool isInput) const;
};