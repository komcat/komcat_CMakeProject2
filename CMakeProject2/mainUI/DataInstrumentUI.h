#pragma once

#include "DataInstrumentModuleManager.h"
#include "imgui.h"  // ADD THIS - needed for ImVec4 type
#include <memory>
#include <string>

// Forward declarations
class SIPHOGClient;  // ADD THIS forward declaration
class TCPDataManagerUI;  // ADD THIS for the existing TCP UI
class CLD101xEquipmentUI;  // ADD THIS for CLD101x UI
class UISMUPanel;  // ADD THIS for SMU UI
class GlobalDataStoreViewerUI;  // ADD THIS for Global Data Store Viewer

class DataInstrumentUI {
public:
    DataInstrumentUI();
    ~DataInstrumentUI();

    void Render();

    // Getters for initialized modules (for MainUIManager)
    DataInstrumentModuleManager* GetModuleManager() const { return m_moduleManager.get(); }

private:
    std::unique_ptr<DataInstrumentModuleManager> m_moduleManager;

    // UI state
    bool m_showModuleCards = true;

    // Navigation flags for detailed UIs
    bool m_showTcpDataManagerUI = false;
    bool m_showCLD101xUI = false;
    bool m_showKeithleyUI = false;
    bool m_showGlobalDataStoreUI = false;  // ADD THIS
    // Note: SIPHOG UI is handled directly by the client

    // Detailed UI instances
    std::unique_ptr<TCPDataManagerUI> m_tcpDataManagerDetailUI;
    std::unique_ptr<CLD101xEquipmentUI> m_cld101xDetailUI;
    std::unique_ptr<UISMUPanel> m_smuDetailUI;
    std::unique_ptr<GlobalDataStoreViewerUI> m_globalDataStoreDetailUI;  // ADD THIS

    // Helper methods
    void RenderModuleCard(const std::string& moduleName, const std::string& displayName, const std::string& description);
    void RenderModuleStatus(ModuleStatus status, const std::string& message);
    void RenderGlobalDataStoreCard();  // ADD THIS special card method
    const char* GetStatusText(ModuleStatus status);
    ImVec4 GetStatusColor(ModuleStatus status);

    // Detailed UI render methods
    void RenderTcpDataManagerUI();
    void RenderCLD101xUI();
    void RenderKeithleyUI();
    void RenderGlobalDataStoreUI();  // ADD THIS

    // Module-specific descriptions
    std::string GetModuleDescription(const std::string& moduleName);

    // Status callback handler
    void OnModuleStatusChanged(const std::string& moduleName, ModuleStatus status, const std::string& message);
};