#pragma once

#include <memory>
#include <string>
#include <functional>
#include <map>

// Forward declarations
class DataClientManager;
class CLD101xManager;
class CLD101xOperations;
class Keithley2400Manager;
class Keithley2400Operations;
class SIPHOGClient;
class Logger;
class ModulePreferencesDB;  // ADD THIS
class MachineOperations;

enum class ModuleStatus {
    NOT_INITIALIZED,
    INITIALIZING,
    CONNECTED,
    FAILED,
    DISABLED
};

struct ModuleInfo {
    bool enabled = false;
    ModuleStatus status = ModuleStatus::NOT_INITIALIZED;
    std::string statusMessage = "";
    std::string configFile = "";
};

class DataInstrumentModuleManager {
public:
    DataInstrumentModuleManager();
    ~DataInstrumentModuleManager();

    // Module enable/disable
    void SetModuleEnabled(const std::string& moduleName, bool enabled);
    bool IsModuleEnabled(const std::string& moduleName) const;

    // Module status
    ModuleStatus GetModuleStatus(const std::string& moduleName) const;
    std::string GetModuleStatusMessage(const std::string& moduleName) const;

    // Module initialization
    bool InitializeTcpDataManager();
    bool InitializeCLD101xManager();
    bool InitializeKeithleyManager();
    bool InitializeSIPHOGClient();

    // Module cleanup
    void ShutdownTcpDataManager();
    void ShutdownCLD101xManager();
    void ShutdownKeithleyManager();
    void ShutdownSIPHOGClient();
    void ShutdownAll();

    // Preferences management - NEW METHODS
    bool SavePreferences();
    bool LoadPreferences();
    bool ResetPreferences();

    // Getters for initialized modules
    DataClientManager* GetTcpDataManager() const { return m_tcpDataManager.get(); }
    CLD101xManager* GetCLD101xManager() const { return m_cld101xManager.get(); }
    CLD101xOperations* GetCLD101xOperations() const { return m_cld101xOperations.get(); }
    Keithley2400Manager* GetKeithleyManager() const { return m_keithleyManager.get(); }
    Keithley2400Operations* GetKeithleyOperations() const { return m_keithleyOperations.get(); }
    SIPHOGClient* GetSIPHOGClient() const { return m_siphogClient.get(); }

    // Status callback
    void SetStatusCallback(std::function<void(const std::string&, ModuleStatus, const std::string&)> callback);

    // NEW: Add method to register MachineOperations for automatic updates
    void SetMachineOperationsCallback(MachineOperations* machineOps);

    // NEW: Get registered MachineOperations (for verification)
    MachineOperations* GetMachineOperationsCallback() const { return m_machineOperationsCallback; }

private:
    // Module instances
    std::unique_ptr<DataClientManager> m_tcpDataManager;
    std::unique_ptr<CLD101xManager> m_cld101xManager;
    std::unique_ptr<CLD101xOperations> m_cld101xOperations;
    std::unique_ptr<Keithley2400Manager> m_keithleyManager;
    std::unique_ptr<Keithley2400Operations> m_keithleyOperations;
    std::unique_ptr<SIPHOGClient> m_siphogClient;

    // Module information
    std::map<std::string, ModuleInfo> m_moduleInfo;

    // Status callback
    std::function<void(const std::string&, ModuleStatus, const std::string&)> m_statusCallback;

    // Logger
    Logger* m_logger;

    // Preferences database - NEW MEMBER
    std::unique_ptr<ModulePreferencesDB> m_preferencesDB;

    // Helper methods
    void UpdateModuleStatus(const std::string& moduleName, ModuleStatus status, const std::string& message = "");
    void InitializeModuleInfo();

    // NEW: Add callback for automatic MachineOperations updates
    MachineOperations* m_machineOperationsCallback = nullptr;
};