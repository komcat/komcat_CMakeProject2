// src/data/global_data_store.cpp 
#include "include/data/global_data_store.h"

#include "include/PowerSupply/SPDPowerSupplyManager.h"
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


// Add these methods to the existing file:


bool GlobalDataStore::SubscribeToSPDManager(SPDPowerSupplyManager* spdManager) {
	if (!spdManager) {
		std::cout << "[ERROR GlobalDataStore] SPD manager is null!" << std::endl;
		return false;
	}

	if (m_spdSubscribed) {
		std::cout << "[DEBUG GlobalDataStore] Already subscribed to SPD manager" << std::endl;
		return true;
	}

	std::cout << "[DEBUG GlobalDataStore] Setting up SPD callback..." << std::endl;

	// CRITICAL: Make sure the callback is properly captured
	auto callback = [this](const std::string& deviceName, const std::string& statusString) {
		std::cout << "[DEBUG GlobalDataStore] *** SPD CALLBACK RECEIVED ***" << std::endl;
		std::cout << "[DEBUG GlobalDataStore] Device: " << deviceName << std::endl;
		std::cout << "[DEBUG GlobalDataStore] Status: " << statusString << std::endl;
		this->OnSPDStatusUpdate(deviceName, statusString);
		};

	// Set the callback
	spdManager->SetStatusUpdateCallback(callback);
	m_spdSubscribed = true;

	std::cout << "[DEBUG GlobalDataStore] ✅ SPD callback registered successfully" << std::endl;
	std::cout << "[DEBUG GlobalDataStore] Callback address: " << &callback << std::endl;

	return true;
}



void GlobalDataStore::UnsubscribeFromSPDManager() {
	if (!m_spdSubscribed) {
		if (m_showDebug) {
			std::cout << "[DEBUG GlobalDataStore] Not currently subscribed to SPD manager" << std::endl;
		}
		return;
	}

	// Execute the unsubscribe callback
	if (m_spdUnsubscribeCallback) {
		try {
			m_spdUnsubscribeCallback();
		}
		catch (const std::exception& e) {
			if (m_showDebug) {
				std::cout << "[DEBUG GlobalDataStore] Exception during unsubscribe: " << e.what() << std::endl;
			}
		}
		m_spdUnsubscribeCallback = nullptr;
	}

	m_spdSubscribed = false;

	if (m_showDebug) {
		std::cout << "[DEBUG GlobalDataStore] Unsubscribed from SPD Power Manager" << std::endl;
	}
}


// 2. ADD DEBUG TO OnSPDStatusUpdate TO SEE IF IT'S CALLED
void GlobalDataStore::OnSPDStatusUpdate(const std::string& deviceName, const std::string& statusString) {
	std::cout << "[DEBUG GlobalDataStore] *** OnSPDStatusUpdate called ***" << std::endl;
	std::cout << "[DEBUG GlobalDataStore] Processing: " << deviceName << " -> " << statusString << std::endl;

	float voltage, current;
	bool outputState;

	if (ParseSPDStatus(statusString, voltage, current, outputState)) {
		std::cout << "[DEBUG GlobalDataStore] Parsed: V=" << voltage << ", I=" << current << ", Output=" << outputState << std::endl;

		// Store data with device-specific channel names
		std::string voltageChannel = "SPD-" + deviceName + "-Voltage";
		std::string currentChannel = "SPD-" + deviceName + "-Current";
		std::string outputChannel = "SPD-" + deviceName + "-Output";
		std::string powerChannel = "SPD-" + deviceName + "-Power";

		std::cout << "[DEBUG GlobalDataStore] Creating channels:" << std::endl;
		std::cout << "  " << voltageChannel << std::endl;
		std::cout << "  " << currentChannel << std::endl;
		std::cout << "  " << outputChannel << std::endl;
		std::cout << "  " << powerChannel << std::endl;

		SetValue(voltageChannel, voltage);
		SetValue(currentChannel, current);
		SetValue(outputChannel, outputState ? 1.0f : 0.0f);
		SetValue(powerChannel, voltage * current);

		std::cout << "[DEBUG GlobalDataStore] ✅ SPD channels created successfully!" << std::endl;
	}
	else {
		std::cout << "[ERROR GlobalDataStore] Failed to parse SPD status: " << statusString << std::endl;
	}
}


bool GlobalDataStore::ParseSPDStatus(const std::string& statusString, float& voltage, float& current, bool& outputState) {
	// Handle common failure cases first
	if (statusString.find("Status read failed") != std::string::npos ||
		statusString.find("Disconnected") != std::string::npos ||
		statusString.find("Device not initialized") != std::string::npos) {
		return false;
	}

	// Initialize outputs
	voltage = 0.0f;
	current = 0.0f;
	outputState = false;

	// Parse output state: look for "Output: ON" or "Output: OFF"
	size_t outputPos = statusString.find("Output: ");
	if (outputPos != std::string::npos) {
		size_t onPos = statusString.find("ON", outputPos);
		size_t offPos = statusString.find("OFF", outputPos);
		if (onPos != std::string::npos && (offPos == std::string::npos || onPos < offPos)) {
			outputState = true;
		}
		else if (offPos != std::string::npos) {
			outputState = false;
		}
	}

	// Parse voltage: look for pattern "V: X.XXXV"
	size_t vPos = statusString.find("V: ");
	if (vPos != std::string::npos) {
		size_t vEnd = statusString.find("V", vPos + 3);
		if (vEnd != std::string::npos) {
			try {
				std::string voltageStr = statusString.substr(vPos + 3, vEnd - vPos - 3);
				voltage = std::stof(voltageStr);
			}
			catch (const std::exception&) {
				return false;
			}
		}
		else {
			return false;
		}
	}
	else {
		return false;
	}

	// Parse current: look for pattern "I: X.XXXA"
	size_t iPos = statusString.find("I: ");
	if (iPos != std::string::npos) {  // FIXED: was "nulf{" - typo
		size_t iEnd = statusString.find("A", iPos + 3);
		if (iEnd != std::string::npos) {
			try {
				std::string currentStr = statusString.substr(iPos + 3, iEnd - iPos - 3);
				current = std::stof(currentStr);
			}
			catch (const std::exception&) {
				return false;
			}
		}
		else {
			return false;
		}
	}
	else {
		return false;
	}

	return true;
}


// Add these method implementations:

bool GlobalDataStore::SubscribeToProvider(std::shared_ptr<IDataProvider> provider,
	const std::string& channelPrefix,
	bool autoStart,
	int pollingInterval) {
	if (!provider) {
		if (m_showDebug) {
			std::cout << "[GlobalDataStore] Cannot subscribe - provider is null" << std::endl;
		}
		return false;
	}

	std::string providerName = provider->GetProviderName();

	std::lock_guard<std::mutex> lock(m_subscriptionsMutex);

	// Check if already subscribed
	if (m_subscriptions.find(providerName) != m_subscriptions.end()) {
		if (m_showDebug) {
			std::cout << "[GlobalDataStore] Already subscribed to: " << providerName << std::endl;
		}
		return true;
	}

	// Create subscription
	DataSubscription subscription;
	subscription.providerName = providerName;
	subscription.channelPrefix = channelPrefix;
	subscription.provider = provider;
	subscription.autoStarted = autoStart;

	// Set up callback with proper capture
	auto callback = [this, providerName, channelPrefix](const std::string& deviceName, const std::string& statusString) {
		this->OnGenericDataUpdate(providerName, channelPrefix, deviceName, statusString);
		};

	provider->SetDataUpdateCallback(callback);

	// Store unsubscribe callback
	subscription.unsubscribeCallback = [provider]() {
		if (provider) {
			provider->SetDataUpdateCallback(nullptr);
			provider->StopDataCollection();
		}
		};

	subscription.active = true;
	m_subscriptions[providerName] = subscription;

	if (m_showDebug) {
		std::cout << "[GlobalDataStore] Subscribed to provider: " << providerName
			<< " with prefix: '" << channelPrefix << "'" << std::endl;
	}

	// Auto-start if requested
	if (autoStart) {
		provider->StartDataCollection(pollingInterval);
		if (m_showDebug) {
			std::cout << "[GlobalDataStore] Auto-started data collection for: " << providerName
				<< " (" << pollingInterval << "ms interval)" << std::endl;
		}
	}

	return true;
}

void GlobalDataStore::UnsubscribeFromProvider(const std::string& providerName) {
	std::lock_guard<std::mutex> lock(m_subscriptionsMutex);

	auto it = m_subscriptions.find(providerName);
	if (it != m_subscriptions.end()) {
		if (it->second.unsubscribeCallback) {
			it->second.unsubscribeCallback();
		}
		m_subscriptions.erase(it);

		if (m_showDebug) {
			std::cout << "[GlobalDataStore] Unsubscribed from provider: " << providerName << std::endl;
		}
	}
}

bool GlobalDataStore::IsSubscribedTo(const std::string& providerName) const {
	std::lock_guard<std::mutex> lock(m_subscriptionsMutex);

	auto it = m_subscriptions.find(providerName);
	return it != m_subscriptions.end() && it->second.active;
}

bool GlobalDataStore::StartProviderDataCollection(const std::string& providerName, int intervalMs) {
	std::lock_guard<std::mutex> lock(m_subscriptionsMutex);

	auto it = m_subscriptions.find(providerName);
	if (it != m_subscriptions.end() && it->second.provider) {
		return it->second.provider->StartDataCollection(intervalMs);
	}
	return false;
}

void GlobalDataStore::StopProviderDataCollection(const std::string& providerName) {
	std::lock_guard<std::mutex> lock(m_subscriptionsMutex);

	auto it = m_subscriptions.find(providerName);
	if (it != m_subscriptions.end() && it->second.provider) {
		it->second.provider->StopDataCollection();
	}
}

std::vector<std::string> GlobalDataStore::GetActiveSubscriptions() const {
	std::lock_guard<std::mutex> lock(m_subscriptionsMutex);

	std::vector<std::string> providers;
	for (const auto& [name, subscription] : m_subscriptions) {
		if (subscription.active) {
			providers.push_back(name);
		}
	}
	return providers;
}

std::vector<std::string> GlobalDataStore::GetProviderChannels(const std::string& providerName) const {
	std::lock_guard<std::mutex> lock(m_subscriptionsMutex);

	auto it = m_subscriptions.find(providerName);
	if (it == m_subscriptions.end() || !it->second.provider) {
		return {};
	}

	std::vector<std::string> channels;
	auto devices = it->second.provider->GetDeviceNames();
	auto suffixes = it->second.provider->GetChannelSuffixes();

	for (const auto& device : devices) {
		for (const auto& [suffix, description] : suffixes) {
			channels.push_back(it->second.channelPrefix + device + "-" + suffix);
		}
	}

	return channels;
}

void GlobalDataStore::OnGenericDataUpdate(const std::string& providerName,
	const std::string& channelPrefix,
	const std::string& deviceName,
	const std::string& statusString) {

	std::map<std::string, float> parsedValues;
	if (ParseProviderData(providerName, statusString, parsedValues)) {

		// Store values with appropriate channel names
		for (const auto& [key, value] : parsedValues) {
			std::string channelName = channelPrefix + deviceName + "-" + key;
			SetValue(channelName, value);
		}

		if (m_showDebug) {
			static std::map<std::string, int> updateCounters;
			updateCounters[providerName]++;

			if (updateCounters[providerName] % 30 == 1) {
				std::cout << "[GlobalDataStore] Provider '" << providerName
					<< "' updated " << parsedValues.size() << " channels for " << deviceName << std::endl;
			}
		}
	}
}

bool GlobalDataStore::ParseProviderData(const std::string& providerName,
	const std::string& statusString,
	std::map<std::string, float>& values) {

	// Handle common error cases
	if (statusString.find("Status read failed") != std::string::npos ||
		statusString.find("Disconnected") != std::string::npos ||
		statusString.find("Device not initialized") != std::string::npos) {
		return false;
	}

	values.clear();

	// Provider-specific parsing
	if (providerName.find("SPD") != std::string::npos || providerName.find("PowerSupply") != std::string::npos) {
		// Parse SPD format: "Connected | Output: ON | V: 3.300V | I: 0.002A"

		// Parse voltage
		size_t vPos = statusString.find("V: ");
		if (vPos != std::string::npos) {
			size_t vEnd = statusString.find("V", vPos + 3);
			if (vEnd != std::string::npos) {
				try {
					float voltage = std::stof(statusString.substr(vPos + 3, vEnd - vPos - 3));
					values["Voltage"] = voltage;
				}
				catch (...) { return false; }
			}
		}

		// Parse current
		size_t iPos = statusString.find("I: ");
		if (iPos != std::string::npos) {
			size_t iEnd = statusString.find("A", iPos + 3);
			if (iEnd != std::string::npos) {
				try {
					float current = std::stof(statusString.substr(iPos + 3, iEnd - iPos - 3));
					values["Current"] = current;

					// Calculate power if we have voltage
					if (values.find("Voltage") != values.end()) {
						values["Power"] = values["Voltage"] * current;
					}
				}
				catch (...) { return false; }
			}
		}

		// Parse output state
		bool outputState = statusString.find("Output: ON") != std::string::npos;
		values["Output"] = outputState ? 1.0f : 0.0f;

		return !values.empty();
	}

	// Add more provider types here...
	// else if (providerName.find("SMU") != std::string::npos) { ... }
	// else if (providerName.find("CLD") != std::string::npos) { ... }

	return false;
}

// global_data_store.cpp

PIMotionSubscriber* GlobalDataStore::GetPISubscriber() {
	if (!m_piSubscriber) {
		m_piSubscriber = std::make_unique<PIMotionSubscriber>();
		if (m_showDebug) {
			std::cout << "[DEBUG GlobalDataStore] Created PI motion subscriber" << std::endl;
		}
	}
	return m_piSubscriber.get();
}

ACSMotionSubscriber* GlobalDataStore::GetACSSubscriber() {
	if (!m_acsSubscriber) {
		m_acsSubscriber = std::make_unique<ACSMotionSubscriber>();
		if (m_showDebug) {
			std::cout << "[DEBUG GlobalDataStore] Created ACS motion subscriber" << std::endl;
		}
	}
	return m_acsSubscriber.get();
}

void GlobalDataStore::ConfigureMotionSubscribers(bool enablePI, bool enableACS) {
	if (enablePI && !m_piSubscriber) {
		m_piSubscriber = std::make_unique<PIMotionSubscriber>();
	}
	if (enableACS && !m_acsSubscriber) {
		m_acsSubscriber = std::make_unique<ACSMotionSubscriber>();
	}

	if (m_showDebug) {
		std::cout << "[DEBUG GlobalDataStore] Motion subscribers configured - "
			<< "PI: " << (enablePI ? "enabled" : "disabled") << ", "
			<< "ACS: " << (enableACS ? "enabled" : "disabled") << std::endl;
	}
}

// In global_data_store.cpp, add this method:
bool GlobalDataStore::TryGetValue(const std::string& serverId, float& value) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_latestValues.find(serverId);
	if (it != m_latestValues.end()) {
		value = it->second;

		// Debug log for successful retrieval
		if (m_showDebug) {
			static int tryGetCounter = 0;
			tryGetCounter++;
			if (tryGetCounter % 100 == 0 && serverId == "GPIB-Current") {
				std::cout << "[DEBUG GlobalDataStore] TryGetValue: " << serverId
					<< " = " << value << " (SUCCESS)" << std::endl;
			}
		}

		return true;
	}

	// Debug log for missing channel
	if (m_showDebug) {
		static std::set<std::string> loggedMissing;
		if (loggedMissing.find(serverId) == loggedMissing.end()) {
			std::cout << "[DEBUG GlobalDataStore] TryGetValue: Channel '" << serverId
				<< "' not found" << std::endl;
			loggedMissing.insert(serverId);
		}
	}

	return false;
}