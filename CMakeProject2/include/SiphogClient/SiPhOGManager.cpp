#include "SiPhOGManager.h"
#include <iostream>

SiPhOGManager::SiPhOGManager(const std::string& host, int port)
  : m_host(host)
  , m_port(port)
  , m_channelPrefix("SiPhOG-")
  , m_provider(nullptr)
  , m_dataStore(nullptr)
  , m_initialized(false)
  , m_subscribed(false)
  , m_debugMode(false)
{
}

SiPhOGManager::~SiPhOGManager() {
  Shutdown();
}

bool SiPhOGManager::Initialize() {
  if (m_initialized) {
    return true;
  }

  // Get GlobalDataStore instance
  m_dataStore = GlobalDataStore::GetInstance();
  if (!m_dataStore) {
    if (m_debugMode) {
      std::cout << "[SiPhOGManager] Failed to get GlobalDataStore instance" << std::endl;
    }
    return false;
  }

  // Create data provider
  m_provider = std::make_unique<SiPhOGDataProvider>(m_host, m_port);
  if (!m_provider) {
    if (m_debugMode) {
      std::cout << "[SiPhOGManager] Failed to create SiPhOG data provider" << std::endl;
    }
    return false;
  }

  // Configure provider
  m_provider->SetDebugMode(m_debugMode);

  // Subscribe to GlobalDataStore
  if (!SubscribeToGlobalDataStore()) {
    if (m_debugMode) {
      std::cout << "[SiPhOGManager] Failed to subscribe to GlobalDataStore" << std::endl;
    }
    return false;
  }

  m_initialized = true;

  if (m_debugMode) {
    std::cout << "[SiPhOGManager] Initialized successfully" << std::endl;
    std::cout << "[SiPhOGManager] Host: " << m_host << ", Port: " << m_port << std::endl;
    std::cout << "[SiPhOGManager] Channel prefix: " << m_channelPrefix << std::endl;
  }

  return true;
}

void SiPhOGManager::Shutdown() {
  if (!m_initialized) {
    return;
  }

  // Stop data collection
  StopDataCollection();

  // Disconnect
  Disconnect();

  // Unsubscribe from GlobalDataStore
  UnsubscribeFromGlobalDataStore();

  // Reset provider
  m_provider.reset();

  m_initialized = false;

  if (m_debugMode) {
    std::cout << "[SiPhOGManager] Shutdown complete" << std::endl;
  }
}

bool SiPhOGManager::Connect() {
  if (!m_initialized || !m_provider) {
    if (m_debugMode) {
      std::cout << "[SiPhOGManager] Cannot connect - not initialized" << std::endl;
    }
    return false;
  }

  bool result = m_provider->Connect();

  if (m_debugMode) {
    std::cout << "[SiPhOGManager] Connection attempt: " << (result ? "SUCCESS" : "FAILED") << std::endl;
  }

  return result;
}

void SiPhOGManager::Disconnect() {
  if (!m_provider) {
    return;
  }

  m_provider->Disconnect();

  if (m_debugMode) {
    std::cout << "[SiPhOGManager] Disconnected" << std::endl;
  }
}

bool SiPhOGManager::IsConnected() const {
  return m_provider && m_provider->IsConnected();
}

bool SiPhOGManager::StartDataCollection(int intervalMs) {
  if (!m_initialized || !m_provider) {
    if (m_debugMode) {
      std::cout << "[SiPhOGManager] Cannot start data collection - not initialized" << std::endl;
    }
    return false;
  }

  // Use intervalMs parameter (0 means use provider default)
  bool result = m_provider->StartDataCollection(intervalMs);

  if (m_debugMode) {
    std::cout << "[SiPhOGManager] Data collection start: " << (result ? "SUCCESS" : "FAILED") << std::endl;
    if (result) {
      std::cout << "[SiPhOGManager] Data will be published to GlobalDataStore with prefix: " << m_channelPrefix << std::endl;
    }
  }

  return result;
}

void SiPhOGManager::StopDataCollection() {
  if (!m_provider) {
    return;
  }

  m_provider->StopDataCollection();

  if (m_debugMode) {
    std::cout << "[SiPhOGManager] Data collection stopped" << std::endl;
  }
}

bool SiPhOGManager::IsDataCollectionActive() const {
  return m_provider && m_provider->IsDataCollectionActive();
}

void SiPhOGManager::SetDebugMode(bool enable) {
  m_debugMode = enable;

  if (m_provider) {
    m_provider->SetDebugMode(enable);
  }

  if (m_debugMode) {
    std::cout << "[SiPhOGManager] Debug mode " << (enable ? "enabled" : "disabled") << std::endl;
  }
}

SiPhOGDataProvider::Stats SiPhOGManager::GetStats() const {
  if (m_provider) {
    return m_provider->GetStats();
  }
  return SiPhOGDataProvider::Stats();
}

std::string SiPhOGManager::GetConnectionStatus() const {
  if (!m_initialized) {
    return "Not Initialized";
  }

  if (!IsConnected()) {
    return "Disconnected";
  }

  if (IsDataCollectionActive()) {
    return "Connected & Collecting";
  }

  return "Connected (Idle)";
}

bool SiPhOGManager::SubscribeToGlobalDataStore() {
  if (!m_dataStore || !m_provider) {
    return false;
  }

  // Create shared pointer for the provider
  std::shared_ptr<IDataProvider> providerPtr(m_provider.get(), [](IDataProvider*) {
    // Custom deleter that does nothing - we manage the lifetime in SiPhOGManager
    });

  // Subscribe to GlobalDataStore with our provider
  bool result = m_dataStore->SubscribeToProvider(
    providerPtr,
    m_channelPrefix,  // Channel prefix (e.g., "SiPhOG-")
    false,            // Don't auto-start (we'll start manually)
    1000              // Default polling interval (not used for SiPhOG)
  );

  if (result) {
    m_subscribed = true;
    if (m_debugMode) {
      std::cout << "[SiPhOGManager] Successfully subscribed to GlobalDataStore" << std::endl;
      std::cout << "[SiPhOGManager] Expected channels will be prefixed with: " << m_channelPrefix << std::endl;

      // Log expected channel names
      auto devices = m_provider->GetDeviceNames();
      auto suffixes = m_provider->GetChannelSuffixes();
      std::cout << "[SiPhOGManager] Expected channels:" << std::endl;
      for (const auto& device : devices) {
        for (const auto& [suffix, description] : suffixes) {
          std::cout << "[SiPhOGManager]   " << m_channelPrefix << device << "-" << suffix
            << " (" << description << ")" << std::endl;
        }
      }
    }
  }
  else {
    if (m_debugMode) {
      std::cout << "[SiPhOGManager] Failed to subscribe to GlobalDataStore" << std::endl;
    }
  }

  return result;
}

void SiPhOGManager::UnsubscribeFromGlobalDataStore() {
  if (!m_subscribed || !m_dataStore) {
    return;
  }

  m_dataStore->UnsubscribeFromProvider(m_provider->GetProviderName());
  m_subscribed = false;

  if (m_debugMode) {
    std::cout << "[SiPhOGManager] Unsubscribed from GlobalDataStore" << std::endl;
  }
}