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