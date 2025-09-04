#pragma once
#include <string>
#include <chrono>

// Forward declaration
struct DataPoint;

// Interface for classes that want to subscribe to data channels
class IDataSubscriber {
public:
  virtual ~IDataSubscriber() = default;

  // Called when new data is received on a subscribed channel
  virtual void OnDataReceived(const std::string& channelId,
    float value,
    const DataPoint& dataPoint) = 0;

  // Called when connection status changes
  virtual void OnConnectionChanged(const std::string& channelId,
    bool connected) = 0;

  // Optional: Called when data error occurs
  virtual void OnDataError(const std::string& channelId,
    const std::string& errorMessage) {
  }

  // Optional: Get subscriber name for debugging
  virtual std::string GetSubscriberName() const { return "Unknown"; }
};