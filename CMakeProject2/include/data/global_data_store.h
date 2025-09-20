
#pragma once
#include "IDataProvider.h"
#include "include/motions/PIMotionSubscriber.h"
#include "include/motions/ACSMotionSubscriber.h"
#include <memory>
#include <string>
#include <map>
#include <mutex>
#include <vector>
#include <functional>  // Add this include

// Forward declaration
class SPDPowerSupplyManager;

class GlobalDataStore {
private:
	// Existing members
	static GlobalDataStore* s_instance;
	std::map<std::string, float> m_latestValues;
	mutable std::mutex m_mutex;
	GlobalDataStore() {}
	bool m_showDebug = false;

	// NEW SPD subscription members
	std::function<void()> m_spdUnsubscribeCallback;
	bool m_spdSubscribed = false;
	// NEW: Generic subscription management
	std::map<std::string, DataSubscription> m_subscriptions;
	mutable std::mutex m_subscriptionsMutex;

	// Motion subscribers
	std::unique_ptr<PIMotionSubscriber> m_piSubscriber;
	std::unique_ptr<ACSMotionSubscriber> m_acsSubscriber;
public:
	// Existing methods
	static GlobalDataStore* GetInstance();
	void SetValue(const std::string& serverId, float value);
	float GetValue(const std::string& serverId, float defaultValue = 0.0f);
	bool HasValue(const std::string& serverId);
	std::vector<std::string> GetAvailableChannels() const;




	// Get motion subscribers (creates them if needed)
	PIMotionSubscriber* GetPISubscriber();
	ACSMotionSubscriber* GetACSSubscriber();

	// Configure motion subscribers
	void ConfigureMotionSubscribers(bool enablePI = true, bool enableACS = true);

	// NEW SPD subscription methods
	/**
	 * @brief Subscribe to SPD Power Manager updates
	 * @param spdManager Pointer to SPD manager instance
	 * @return true if subscription successful
	 */
	bool SubscribeToSPDManager(SPDPowerSupplyManager* spdManager);

	/**
	 * @brief Unsubscribe from SPD Power Manager
	 */
	void UnsubscribeFromSPDManager();

	/**
	 * @brief Check if subscribed to SPD manager
	 * @return true if currently subscribed
	 */
	bool IsSPDSubscribed() const { return m_spdSubscribed; }

	/**
	 * @brief Enable/disable debug output
	 * @param enable Debug mode on/off
	 */
	void SetDebugMode(bool enable) { m_showDebug = enable; }

	// In global_data_store.h, add after GetValue declaration:
	bool TryGetValue(const std::string& serverId, float& value);

	/**
		*@brief Manual SPD status update(for chaining callbacks)
		* @param deviceName Device name
		* @param statusString Status string to parse
		*/
	void UpdateSPDStatus(const std::string& deviceName, const std::string& statusString) {
		OnSPDStatusUpdate(deviceName, statusString);
	}




	// NEW: Generic subscription methods

	/**
	 * @brief Subscribe to a generic data provider
	 * @param provider Shared pointer to data provider
	 * @param channelPrefix Prefix for channel names (e.g., "SPD-", "SMU-")
	 * @param autoStart Whether to start data collection immediately
	 * @param pollingInterval Polling interval if autoStart is true
	 * @return true if subscription successful
	 */
	bool SubscribeToProvider(std::shared_ptr<IDataProvider> provider,
		const std::string& channelPrefix = "",
		bool autoStart = false,
		int pollingInterval = 1000);

	/**
	 * @brief Unsubscribe from a data provider
	 * @param providerName Name of provider to unsubscribe from
	 */
	void UnsubscribeFromProvider(const std::string& providerName);

	/**
	 * @brief Get list of active subscriptions
	 */
	std::vector<std::string> GetActiveSubscriptions() const;

	/**
	 * @brief Check if subscribed to a specific provider
	 */
	bool IsSubscribedTo(const std::string& providerName) const;

	/**
	 * @brief Start data collection for a subscribed provider
	 */
	bool StartProviderDataCollection(const std::string& providerName, int intervalMs = 1000);

	/**
	 * @brief Stop data collection for a subscribed provider
	 */
	void StopProviderDataCollection(const std::string& providerName);

	/**
	 * @brief Get expected channel names for a provider
	 */
	std::vector<std::string> GetProviderChannels(const std::string& providerName) const;





private:
	/**
	 * @brief Callback function for SPD status updates
	 * @param deviceName Name of the SPD device
	 * @param statusString Status string from SPD manager
	 */
	void OnSPDStatusUpdate(const std::string& deviceName, const std::string& statusString);

	/**
	 * @brief Parse SPD status string and extract voltage/current values
	 * @param statusString Raw status from SPD manager
	 * @param voltage Output parameter for voltage
	 * @param current Output parameter for current
	 * @param outputState Output parameter for output on/off state
	 * @return true if parsing successful
	 */
	bool ParseSPDStatus(const std::string& statusString, float& voltage, float& current, bool& outputState);

	/**
		 * @brief Generic callback handler for data updates
		 */
	void OnGenericDataUpdate(const std::string& providerName,
		const std::string& channelPrefix,
		const std::string& deviceName,
		const std::string& statusString);

	/**
	 * @brief Parse provider-specific data format
	 */
	bool ParseProviderData(const std::string& providerName,
		const std::string& statusString,
		std::map<std::string, float>& values);


};