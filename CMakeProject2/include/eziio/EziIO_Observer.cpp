#include "EziIO_Observer.h"
#include <algorithm>
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>

// ============= EziIOPublisher Implementation =============

void EziIOPublisher::subscribe(std::shared_ptr<IEziIOObserver> observer) {
  if (!observer) return;

  std::lock_guard<std::mutex> lock(m_observersMutex);

  // Check if already subscribed
  auto it = std::find(m_observers.begin(), m_observers.end(), observer);
  if (it == m_observers.end()) {
    m_observers.push_back(observer);
  }
}

void EziIOPublisher::unsubscribe(std::shared_ptr<IEziIOObserver> observer) {
  if (!observer) return;

  std::lock_guard<std::mutex> lock(m_observersMutex);
  m_observers.erase(
    std::remove(m_observers.begin(), m_observers.end(), observer),
    m_observers.end()
  );
}

void EziIOPublisher::subscribeCallback(std::function<void(const PinChangeEvent&)> callback) {
  if (!callback) return;

  std::lock_guard<std::mutex> lock(m_observersMutex);
  m_callbacks.push_back(callback);
}

void EziIOPublisher::clearObservers() {
  std::lock_guard<std::mutex> lock(m_observersMutex);
  m_observers.clear();
  m_callbacks.clear();
}

size_t EziIOPublisher::getObserverCount() const {
  std::lock_guard<std::mutex> lock(m_observersMutex);
  return m_observers.size() + m_callbacks.size();
}

void EziIOPublisher::notifyPinChange(const PinChangeEvent& event) {
  std::lock_guard<std::mutex> lock(m_observersMutex);

  // Notify all observers
  for (auto& observer : m_observers) {
    if (observer) {
      observer->onPinStateChanged(event);
    }
  }

  // Notify all callback functions
  for (auto& callback : m_callbacks) {
    if (callback) {
      callback(event);
    }
  }
}

// ============= EziIOLoggerObserver Implementation =============

EziIOLoggerObserver::EziIOLoggerObserver(const std::string& logPrefix)
  : m_logPrefix(logPrefix) {
}

void EziIOLoggerObserver::onPinStateChanged(const PinChangeEvent& event) {
  std::stringstream ss;
  ss << m_logPrefix << " "
    << "Device: " << event.deviceName << " (ID:" << event.deviceId << ") "
    << (event.isInput ? "INPUT" : "OUTPUT") << " "
    << "Pin " << event.pinNumber << " "
    << "changed from " << (event.previousState ? "ON" : "OFF")
    << " to " << (event.newState ? "ON" : "OFF");

  if (event.timestamp > 0) {
    ss << " @ " << event.timestamp << "ms";
  }

  std::cout << ss.str() << std::endl;
}

// ============= EziIOStatsObserver Implementation =============

void EziIOStatsObserver::onPinStateChanged(const PinChangeEvent& event) {
  std::lock_guard<std::mutex> lock(m_statsMutex);

  std::string key = makeKey(event.deviceName, event.pinNumber, event.isInput);

  auto& stats = m_pinStats[key];
  stats.changeCount++;

  if (event.newState) {
    stats.onCount++;
  }
  else {
    stats.offCount++;
  }

  stats.lastChangeTime = event.timestamp;
}

EziIOStatsObserver::PinStats EziIOStatsObserver::getStats(
  const std::string& deviceName, int pinNumber, bool isInput) const {

  std::lock_guard<std::mutex> lock(m_statsMutex);

  std::string key = makeKey(deviceName, pinNumber, isInput);
  auto it = m_pinStats.find(key);

  if (it != m_pinStats.end()) {
    return it->second;
  }

  return PinStats{};
}

void EziIOStatsObserver::resetStats() {
  std::lock_guard<std::mutex> lock(m_statsMutex);
  m_pinStats.clear();
}

std::string EziIOStatsObserver::makeKey(const std::string& device, int pin, bool isInput) const {
  std::stringstream ss;
  ss << device << "_" << pin << "_" << (isInput ? "I" : "O");
  return ss.str();
}