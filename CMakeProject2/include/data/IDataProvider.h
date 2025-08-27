
#pragma once
#include <string>
#include <functional>
#include <map>
#include <vector>
#include <memory>  // ADD THIS - Required for std::shared_ptr

/**
 * @brief Generic interface for data providers that can be subscribed to
 */
class IDataProvider {
public:
  virtual ~IDataProvider() = default;

  /**
   * @brief Get provider name/identifier
   */
  virtual std::string GetProviderName() const = 0;

  /**
   * @brief Start data collection/polling
   * @param intervalMs Polling interval in milliseconds
   * @return true if started successfully
   */
  virtual bool StartDataCollection(int intervalMs) = 0;

  /**
   * @brief Stop data collection/polling
   */
  virtual void StopDataCollection() = 0;

  /**
   * @brief Check if data collection is active
   */
  virtual bool IsDataCollectionActive() const = 0;

  /**
   * @brief Set callback for data updates
   * @param callback Function called when data is updated (deviceName, statusString)
   */
  virtual void SetDataUpdateCallback(std::function<void(const std::string&, const std::string&)> callback) = 0;

  /**
   * @brief Get list of device names managed by this provider
   */
  virtual std::vector<std::string> GetDeviceNames() const = 0;

  /**
   * @brief Get provider-specific channel suffix mappings
   * @return Map of suffix name to description (e.g., "Voltage" -> "Output voltage in volts")
   */
  virtual std::map<std::string, std::string> GetChannelSuffixes() const = 0;
};

/**
 * @brief Data subscription information
 */
struct DataSubscription {
  std::string providerName;
  std::string channelPrefix;
  std::shared_ptr<IDataProvider> provider;
  std::function<void()> unsubscribeCallback;
  bool active = false;
  bool autoStarted = false;
};
