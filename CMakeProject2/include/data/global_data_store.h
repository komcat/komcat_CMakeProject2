
#pragma once
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

public:
	// Existing methods
	static GlobalDataStore* GetInstance();
	void SetValue(const std::string& serverId, float value);
	float GetValue(const std::string& serverId, float defaultValue = 0.0f);
	bool HasValue(const std::string& serverId);
	std::vector<std::string> GetAvailableChannels() const;

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

	/**
		*@brief Manual SPD status update(for chaining callbacks)
		* @param deviceName Device name
		* @param statusString Status string to parse
		*/
		void UpdateSPDStatus(const std::string & deviceName, const std::string & statusString) {
		OnSPDStatusUpdate(deviceName, statusString);
	}

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
};