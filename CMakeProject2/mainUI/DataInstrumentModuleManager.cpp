#include "DataInstrumentModuleManager.h"
#include "ModulePreferencesDB.h"  // ADD THIS INCLUDE
#include "include/data/data_client_manager.h"
#include "include/cld101x_manager.h"
#include "include/cld101x_operations.h"
#include "include/SMU/keithley2400_manager.h"
#include "include/SMU/keithley2400_operations.h"
#include "include/siphog/siphog_client.h"
// Add this include at the top
#include "include/machine_operations.h"  // Include for MachineOperations

#include "include/logger.h"
#include <thread>
#include <chrono>

DataInstrumentModuleManager::DataInstrumentModuleManager()
    : m_logger(Logger::GetInstance()) {

    // Initialize preferences database
    m_preferencesDB = std::make_unique<ModulePreferencesDB>();
    if (m_preferencesDB->Initialize()) {
        m_logger->LogInfo("Module preferences database initialized successfully");
    }
    else {
        m_logger->LogWarning("Failed to initialize preferences database, using defaults");
    }

    InitializeModuleInfo();
    LoadPreferences(); // Load saved preferences after initializing module info
}

DataInstrumentModuleManager::~DataInstrumentModuleManager() {
    // Save current preferences before shutdown
    SavePreferences();
    ShutdownAll();
}

void DataInstrumentModuleManager::InitializeModuleInfo() {
    // TCP Data Manager
    ModuleInfo tcpInfo;
    tcpInfo.enabled = false;
    tcpInfo.status = ModuleStatus::NOT_INITIALIZED;
    tcpInfo.statusMessage = "Ready to initialize";
    tcpInfo.configFile = "DataServerConfig.json";
    m_moduleInfo["TCP_DATA_MANAGER"] = tcpInfo;

    // CLD101x Manager
    ModuleInfo cldInfo;
    cldInfo.enabled = false;
    cldInfo.status = ModuleStatus::NOT_INITIALIZED;
    cldInfo.statusMessage = "Ready to initialize";
    cldInfo.configFile = "cld101x_config.json";
    m_moduleInfo["CLD101X_MANAGER"] = cldInfo;

    // Keithley 2400 Manager
    ModuleInfo keithleyInfo;
    keithleyInfo.enabled = false;
    keithleyInfo.status = ModuleStatus::NOT_INITIALIZED;
    keithleyInfo.statusMessage = "Ready to initialize";
    keithleyInfo.configFile = "smu_config.json";
    m_moduleInfo["KEITHLEY_MANAGER"] = keithleyInfo;

    // SIPHOG Client
    ModuleInfo siphogInfo;
    siphogInfo.enabled = false;
    siphogInfo.status = ModuleStatus::NOT_INITIALIZED;
    siphogInfo.statusMessage = "Ready to initialize";
    siphogInfo.configFile = "";
    m_moduleInfo["SIPHOG_CLIENT"] = siphogInfo;
}

void DataInstrumentModuleManager::SetModuleEnabled(const std::string& moduleName, bool enabled) {
    if (m_moduleInfo.find(moduleName) != m_moduleInfo.end()) {
        m_moduleInfo[moduleName].enabled = enabled;

        // Save preference immediately when changed
        if (m_preferencesDB && m_preferencesDB->IsInitialized()) {
            m_preferencesDB->SaveModulePreference(moduleName, enabled);
        }

        if (!enabled && m_moduleInfo[moduleName].status != ModuleStatus::NOT_INITIALIZED) {
            // Shutdown the module if it was initialized
            if (moduleName == "TCP_DATA_MANAGER") {
                ShutdownTcpDataManager();
            }
            else if (moduleName == "CLD101X_MANAGER") {
                ShutdownCLD101xManager();
            }
            else if (moduleName == "KEITHLEY_MANAGER") {
                ShutdownKeithleyManager();
            }
            else if (moduleName == "SIPHOG_CLIENT") {
                ShutdownSIPHOGClient();
            }
        }
    }
}

bool DataInstrumentModuleManager::IsModuleEnabled(const std::string& moduleName) const {
    auto it = m_moduleInfo.find(moduleName);
    return it != m_moduleInfo.end() ? it->second.enabled : false;
}

ModuleStatus DataInstrumentModuleManager::GetModuleStatus(const std::string& moduleName) const {
    auto it = m_moduleInfo.find(moduleName);
    return it != m_moduleInfo.end() ? it->second.status : ModuleStatus::NOT_INITIALIZED;
}

std::string DataInstrumentModuleManager::GetModuleStatusMessage(const std::string& moduleName) const {
    auto it = m_moduleInfo.find(moduleName);
    return it != m_moduleInfo.end() ? it->second.statusMessage : "Unknown module";
}

void DataInstrumentModuleManager::UpdateModuleStatus(const std::string& moduleName, ModuleStatus status, const std::string& message) {
    if (m_moduleInfo.find(moduleName) != m_moduleInfo.end()) {
        m_moduleInfo[moduleName].status = status;
        m_moduleInfo[moduleName].statusMessage = message;

        if (m_statusCallback) {
            m_statusCallback(moduleName, status, message);
        }
    }
}

bool DataInstrumentModuleManager::InitializeTcpDataManager() {
    if (!IsModuleEnabled("TCP_DATA_MANAGER")) {
        return false;
    }

    UpdateModuleStatus("TCP_DATA_MANAGER", ModuleStatus::INITIALIZING, "Initializing TCP Data Manager...");

    try {
        m_tcpDataManager = std::make_unique<DataClientManager>("DataServerConfig.json");

        // Connect to auto-connect servers
        m_tcpDataManager->ConnectAutoClients();

        UpdateModuleStatus("TCP_DATA_MANAGER", ModuleStatus::CONNECTED, "TCP Data Manager initialized successfully");
        m_logger->LogInfo("TCP Data Manager initialized successfully");
        return true;
    }
    catch (const std::exception& e) {
        std::string errorMsg = "Failed to initialize: " + std::string(e.what());
        UpdateModuleStatus("TCP_DATA_MANAGER", ModuleStatus::FAILED, errorMsg);
        m_logger->LogError("TCP Data Manager initialization failed: " + std::string(e.what()));
        return false;
    }
}

// DataInstrumentModuleManager.cpp - Update these methods to ensure operations are constructed


// UPDATE: Modify existing InitializeCLD101xManager method
bool DataInstrumentModuleManager::InitializeCLD101xManager() {
    if (!IsModuleEnabled("CLD101X_MANAGER")) {
        return false;
    }

    UpdateModuleStatus("CLD101X_MANAGER", ModuleStatus::INITIALIZING, "Initializing CLD101x Manager...");

    try {
        m_cld101xManager = std::make_unique<CLD101xManager>();
        m_cld101xManager->Initialize();

        if (m_cld101xManager->ConnectAll()) {
            // ENSURE: Create CLD101xOperations when manager is connected
            m_cld101xOperations = std::make_unique<CLD101xOperations>(*m_cld101xManager);

            UpdateModuleStatus("CLD101X_MANAGER", ModuleStatus::CONNECTED, "CLD101x Manager connected successfully");
            m_logger->LogInfo("CLD101x Manager initialized and connected successfully");
            m_logger->LogInfo("CLD101x Operations created successfully");

            // AUTO-UPDATE: Notify MachineOperations if registered
            if (m_machineOperationsCallback) {
                m_machineOperationsCallback->SetLaserOperations(m_cld101xOperations.get());
                m_logger->LogInfo("DataInstrumentModuleManager: Auto-updated MachineOperations with laser operations");
            }

            return true;
        }
        else {
            UpdateModuleStatus("CLD101X_MANAGER", ModuleStatus::FAILED, "Failed to connect to CLD101x devices");
            m_logger->LogWarning("CLD101x Manager initialized but failed to connect to devices");
            return false;
        }
    }
    catch (const std::exception& e) {
        std::string errorMsg = "Failed to initialize: " + std::string(e.what());
        UpdateModuleStatus("CLD101X_MANAGER", ModuleStatus::FAILED, errorMsg);
        m_logger->LogError("CLD101x Manager initialization failed: " + std::string(e.what()));
        return false;
    }
}



// UPDATE: Modify existing InitializeKeithleyManager method
bool DataInstrumentModuleManager::InitializeKeithleyManager() {
    if (!IsModuleEnabled("KEITHLEY_MANAGER")) {
        return false;
    }

    UpdateModuleStatus("KEITHLEY_MANAGER", ModuleStatus::INITIALIZING, "Initializing Keithley Manager...");

    try {
        m_keithleyManager = std::make_unique<Keithley2400Manager>();

        if (m_keithleyManager->Initialize("smu_config.json")) {
            // ENSURE: Create Keithley2400Operations when manager is initialized
            m_keithleyOperations = std::make_unique<Keithley2400Operations>(*m_keithleyManager);

            if (m_keithleyManager->ConnectAll()) {
                UpdateModuleStatus("KEITHLEY_MANAGER", ModuleStatus::CONNECTED, "Keithley Manager connected successfully");
                m_logger->LogInfo("Keithley Manager initialized and connected successfully");
                m_logger->LogInfo("Keithley Operations created successfully");

                // AUTO-UPDATE: Notify MachineOperations if registered  
                if (m_machineOperationsCallback) {
                    m_machineOperationsCallback->SetSMUOperations(m_keithleyOperations.get());
                    m_logger->LogInfo("DataInstrumentModuleManager: Auto-updated MachineOperations with SMU operations");
                }

                return true;
            }
            else {
                UpdateModuleStatus("KEITHLEY_MANAGER", ModuleStatus::FAILED, "Failed to connect to Keithley devices");
                m_logger->LogWarning("Keithley Manager initialized but failed to connect to devices");
                // Keep operations even if connection failed - they might connect later

                // Still notify MachineOperations even if connection failed
                if (m_machineOperationsCallback) {
                    m_machineOperationsCallback->SetSMUOperations(m_keithleyOperations.get());
                    m_logger->LogInfo("DataInstrumentModuleManager: Auto-updated MachineOperations with SMU operations (connection pending)");
                }

                return false;
            }
        }
        else {
            UpdateModuleStatus("KEITHLEY_MANAGER", ModuleStatus::FAILED, "Failed to load configuration");
            m_logger->LogError("Failed to initialize Keithley Manager - config load failed");
            return false;
        }
    }
    catch (const std::exception& e) {
        std::string errorMsg = "Failed to initialize: " + std::string(e.what());
        UpdateModuleStatus("KEITHLEY_MANAGER", ModuleStatus::FAILED, errorMsg);
        m_logger->LogError("Keithley Manager initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool DataInstrumentModuleManager::InitializeSIPHOGClient() {
    if (!IsModuleEnabled("SIPHOG_CLIENT")) {
        return false;
    }

    UpdateModuleStatus("SIPHOG_CLIENT", ModuleStatus::INITIALIZING, "Initializing SIPHOG Client...");

    try {
        m_siphogClient = std::make_unique<SIPHOGClient>();

        // SIPHOG Client doesn't have explicit connection method in the original code
        // So we assume it's ready after construction
        UpdateModuleStatus("SIPHOG_CLIENT", ModuleStatus::CONNECTED, "SIPHOG Client initialized successfully");
        m_logger->LogInfo("SIPHOG Client initialized successfully");
        return true;
    }
    catch (const std::exception& e) {
        std::string errorMsg = "Failed to initialize: " + std::string(e.what());
        UpdateModuleStatus("SIPHOG_CLIENT", ModuleStatus::FAILED, errorMsg);
        m_logger->LogError("SIPHOG Client initialization failed: " + std::string(e.what()));
        return false;
    }
}

void DataInstrumentModuleManager::ShutdownTcpDataManager() {
    if (m_tcpDataManager) {
        m_logger->LogInfo("Shutting down TCP Data Manager...");
        m_tcpDataManager.reset();
        UpdateModuleStatus("TCP_DATA_MANAGER", ModuleStatus::NOT_INITIALIZED, "TCP Data Manager shutdown");
    }
}


// UPDATE: Modify existing shutdown methods to clear operations
void DataInstrumentModuleManager::ShutdownCLD101xManager() {
    if (m_cld101xManager) {
        m_logger->LogInfo("Shutting down CLD101x Manager...");

        // AUTO-UPDATE: Clear from MachineOperations before shutdown
        if (m_machineOperationsCallback) {
            m_machineOperationsCallback->SetLaserOperations(nullptr);
            m_logger->LogInfo("DataInstrumentModuleManager: Auto-cleared laser operations from MachineOperations");
        }

        m_cld101xOperations.reset();
        m_cld101xManager->DisconnectAll();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        m_cld101xManager.reset();
        UpdateModuleStatus("CLD101X_MANAGER", ModuleStatus::NOT_INITIALIZED, "CLD101x Manager shutdown");
    }
}

void DataInstrumentModuleManager::ShutdownKeithleyManager() {
    if (m_keithleyManager) {
        m_logger->LogInfo("Shutting down Keithley Manager...");

        // AUTO-UPDATE: Clear from MachineOperations before shutdown
        if (m_machineOperationsCallback) {
            m_machineOperationsCallback->SetSMUOperations(nullptr);
            m_logger->LogInfo("DataInstrumentModuleManager: Auto-cleared SMU operations from MachineOperations");
        }

        m_keithleyOperations.reset();
        m_keithleyManager->DisconnectAll();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        m_keithleyManager.reset();
        UpdateModuleStatus("KEITHLEY_MANAGER", ModuleStatus::NOT_INITIALIZED, "Keithley Manager shutdown");
    }
}


void DataInstrumentModuleManager::ShutdownSIPHOGClient() {
    if (m_siphogClient) {
        m_logger->LogInfo("Shutting down SIPHOG Client...");
        m_siphogClient.reset();
        UpdateModuleStatus("SIPHOG_CLIENT", ModuleStatus::NOT_INITIALIZED, "SIPHOG Client shutdown");
    }
}

void DataInstrumentModuleManager::ShutdownAll() {
    ShutdownTcpDataManager();
    ShutdownCLD101xManager();
    ShutdownKeithleyManager();
    ShutdownSIPHOGClient();
}

void DataInstrumentModuleManager::SetStatusCallback(std::function<void(const std::string&, ModuleStatus, const std::string&)> callback) {
    m_statusCallback = callback;
}

// NEW PREFERENCE MANAGEMENT METHODS
bool DataInstrumentModuleManager::SavePreferences() {
    if (!m_preferencesDB || !m_preferencesDB->IsInitialized()) {
        m_logger->LogWarning("Preferences database not available, cannot save preferences");
        return false;
    }

    std::map<std::string, bool> preferences;
    for (const auto& [moduleName, moduleInfo] : m_moduleInfo) {
        preferences[moduleName] = moduleInfo.enabled;
    }

    bool success = m_preferencesDB->SaveAllPreferences(preferences);
    if (success) {
        m_logger->LogInfo("Successfully saved all module preferences");
    }
    else {
        m_logger->LogError("Failed to save module preferences");
    }

    return success;
}

bool DataInstrumentModuleManager::LoadPreferences() {
    if (!m_preferencesDB || !m_preferencesDB->IsInitialized()) {
        m_logger->LogWarning("Preferences database not available, using default preferences");
        return false;
    }

    std::map<std::string, bool> savedPreferences = m_preferencesDB->LoadAllPreferences();

    // Apply saved preferences to module info
    for (auto& [moduleName, moduleInfo] : m_moduleInfo) {
        auto it = savedPreferences.find(moduleName);
        if (it != savedPreferences.end()) {
            moduleInfo.enabled = it->second;
            m_logger->LogInfo("Loaded preference: " + moduleName + " = " + (it->second ? "enabled" : "disabled"));
        }
        else {
            // Module not found in saved preferences, keep default (false)
            m_logger->LogInfo("No saved preference for " + moduleName + ", using default (disabled)");
        }
    }

    if (!savedPreferences.empty()) {
        m_logger->LogInfo("Successfully loaded " + std::to_string(savedPreferences.size()) + " module preferences");
        return true;
    }

    return false;
}

bool DataInstrumentModuleManager::ResetPreferences() {
    if (!m_preferencesDB || !m_preferencesDB->IsInitialized()) {
        m_logger->LogWarning("Preferences database not available, cannot reset preferences");
        return false;
    }

    // Reset database
    bool success = m_preferencesDB->ResetAllPreferences();

    if (success) {
        // Reset all modules to disabled
        for (auto& [moduleName, moduleInfo] : m_moduleInfo) {
            // Shutdown if currently running
            if (moduleInfo.enabled) {
                SetModuleEnabled(moduleName, false);
            }
            moduleInfo.enabled = false;
        }

        m_logger->LogInfo("Successfully reset all module preferences");
    }
    else {
        m_logger->LogError("Failed to reset module preferences");
    }

    return success;
}

// NEW: Implementation of callback setter
void DataInstrumentModuleManager::SetMachineOperationsCallback(MachineOperations* machineOps) {
    m_machineOperationsCallback = machineOps;

    if (machineOps) {
        m_logger->LogInfo("DataInstrumentModuleManager: MachineOperations callback registered for auto-updates");

        // If modules are already initialized, update immediately
        if (m_cld101xOperations) {
            machineOps->SetLaserOperations(m_cld101xOperations.get());
            m_logger->LogInfo("DataInstrumentModuleManager: Auto-updated existing laser operations");
        }

        if (m_keithleyOperations) {
            machineOps->SetSMUOperations(m_keithleyOperations.get());
            m_logger->LogInfo("DataInstrumentModuleManager: Auto-updated existing SMU operations");
        }
    }
    else {
        m_logger->LogInfo("DataInstrumentModuleManager: MachineOperations callback cleared");
    }
}