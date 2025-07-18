#include "DataInstrumentUI.h"
#include "imgui.h"
#include "include/siphog/siphog_client.h"  // ADD THIS - needed for SIPHOGClient methods
#include "TCPDataManagerUI.h"  // ADD THESE existing UI classes
#include "CLD101xEquipmentUI.h"
#include "UISMUPanel.h"
#include "GlobalDataStoreViewerUI.h"  // ADD THIS
#include <iostream>

DataInstrumentUI::DataInstrumentUI() {
    m_moduleManager = std::make_unique<DataInstrumentModuleManager>();

    // Set up status callback
    m_moduleManager->SetStatusCallback([this](const std::string& moduleName, ModuleStatus status, const std::string& message) {
        OnModuleStatusChanged(moduleName, status, message);
        });
}

DataInstrumentUI::~DataInstrumentUI() = default;

void DataInstrumentUI::Render() {
    // Check if we should show detailed UI instead of module cards
    if (m_showTcpDataManagerUI) {
        RenderTcpDataManagerUI();
        return;
    }
    if (m_showCLD101xUI) {
        RenderCLD101xUI();
        return;
    }
    if (m_showKeithleyUI) {
        RenderKeithleyUI();
        return;
    }
    if (m_showGlobalDataStoreUI) {  // ADD THIS
        RenderGlobalDataStoreUI();
        return;
    }

    // Default: Show module manager cards
    ImGui::SetWindowFontScale(1.5f);
    ImGui::Text("Data & Instrument Module Manager");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Spacing();
    ImGui::Text("Configure and initialize data monitoring and instrument control modules:");
    ImGui::Spacing();

    // Module cards layout
    const float cardWidth = 300.0f;
    const float cardHeight = 200.0f;
    const float spacing = 20.0f;

    ImVec2 contentRegion = ImGui::GetContentRegionAvail();
    int cardsPerRow = (int)((contentRegion.x + spacing) / (cardWidth + spacing));
    cardsPerRow = (std::max)(cardsPerRow, 1);

    // TCP Data Manager Card
    RenderModuleCard("TCP_DATA_MANAGER", "TCP Data Manager", GetModuleDescription("TCP_DATA_MANAGER"));

    if ((1 % cardsPerRow) != 0) {
        ImGui::SameLine();
    }

    // CLD101x Manager Card
    RenderModuleCard("CLD101X_MANAGER", "CLD101x Laser Control", GetModuleDescription("CLD101X_MANAGER"));

    if ((2 % cardsPerRow) != 0) {
        ImGui::SameLine();
    }

    // Keithley Manager Card
    RenderModuleCard("KEITHLEY_MANAGER", "Keithley SMU Manager", GetModuleDescription("KEITHLEY_MANAGER"));

    if ((3 % cardsPerRow) != 0) {
        ImGui::SameLine();
    }

    // SIPHOG Client Card
    RenderModuleCard("SIPHOG_CLIENT", "SIPHOG Controller", GetModuleDescription("SIPHOG_CLIENT"));

    if ((4 % cardsPerRow) != 0) {
        ImGui::SameLine();
    }

    // Global Data Store Viewer Card - ADD THIS
    RenderGlobalDataStoreCard();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Global actions
    ImGui::Text("Global Actions:");

    if (ImGui::Button("Initialize All Enabled", ImVec2(150, 30))) {
        if (m_moduleManager->IsModuleEnabled("TCP_DATA_MANAGER")) {
            m_moduleManager->InitializeTcpDataManager();
        }
        if (m_moduleManager->IsModuleEnabled("CLD101X_MANAGER")) {
            m_moduleManager->InitializeCLD101xManager();
        }
        if (m_moduleManager->IsModuleEnabled("KEITHLEY_MANAGER")) {
            m_moduleManager->InitializeKeithleyManager();
        }
        if (m_moduleManager->IsModuleEnabled("SIPHOG_CLIENT")) {
            m_moduleManager->InitializeSIPHOGClient();
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Shutdown All", ImVec2(100, 30))) {
        m_moduleManager->ShutdownAll();
    }

    ImGui::SameLine();

    // NEW PREFERENCE MANAGEMENT BUTTONS
    if (ImGui::Button("Save Preferences", ImVec2(120, 30))) {
        if (m_moduleManager->SavePreferences()) {
            std::cout << "Module preferences saved successfully" << std::endl;
        }
        else {
            std::cout << "Failed to save module preferences" << std::endl;
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Reset All", ImVec2(80, 30))) {
        // Show confirmation popup
        ImGui::OpenPopup("Reset Confirmation");
    }

    // Reset confirmation popup
    if (ImGui::BeginPopupModal("Reset Confirmation", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to reset all module preferences?");
        ImGui::Text("This will disable all modules and clear saved preferences.");
        ImGui::Separator();

        if (ImGui::Button("Yes, Reset All", ImVec2(120, 0))) {
            if (m_moduleManager->ResetPreferences()) {
                std::cout << "All module preferences have been reset" << std::endl;
            }
            else {
                std::cout << "Failed to reset module preferences" << std::endl;
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void DataInstrumentUI::RenderModuleCard(const std::string& moduleName, const std::string& displayName, const std::string& description) {
    const float cardWidth = 300.0f;
    const float cardHeight = 200.0f;

    ImVec2 cursorPos = ImGui::GetCursorPos();

    // Card background
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.5f, 1.0f));

    std::string cardId = "##" + moduleName + "_card";
    ImGui::BeginChild(cardId.c_str(), ImVec2(cardWidth, cardHeight), true);

    // Card header
    ImGui::SetWindowFontScale(1.2f);
    ImGui::Text("%s", displayName.c_str());
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Separator();
    ImGui::Spacing();

    // Description
    ImGui::TextWrapped("%s", description.c_str());
    ImGui::Spacing();

    // Enable/Disable toggle
    bool enabled = m_moduleManager->IsModuleEnabled(moduleName);
    std::string toggleLabel = "Enable##" + moduleName;

    if (ImGui::Checkbox(toggleLabel.c_str(), &enabled)) {
        m_moduleManager->SetModuleEnabled(moduleName, enabled);
    }

    // Status display
    ModuleStatus status = m_moduleManager->GetModuleStatus(moduleName);
    std::string statusMessage = m_moduleManager->GetModuleStatusMessage(moduleName);

    ImGui::Spacing();
    RenderModuleStatus(status, statusMessage);

    // Initialize button (only shown if enabled and not initialized)
    if (enabled && status == ModuleStatus::NOT_INITIALIZED) {
        ImGui::Spacing();
        std::string initButtonLabel = "Initialize##" + moduleName;

        if (ImGui::Button(initButtonLabel.c_str(), ImVec2(-1, 25))) {
            if (moduleName == "TCP_DATA_MANAGER") {
                m_moduleManager->InitializeTcpDataManager();
            }
            else if (moduleName == "CLD101X_MANAGER") {
                m_moduleManager->InitializeCLD101xManager();
            }
            else if (moduleName == "KEITHLEY_MANAGER") {
                m_moduleManager->InitializeKeithleyManager();
            }
            else if (moduleName == "SIPHOG_CLIENT") {
                m_moduleManager->InitializeSIPHOGClient();
            }
        }
    }

    // Shutdown button (only shown if connected)
    if (status == ModuleStatus::CONNECTED) {
        ImGui::Spacing();
        std::string shutdownButtonLabel = "Shutdown##" + moduleName;

        if (ImGui::Button(shutdownButtonLabel.c_str(), ImVec2(-1, 25))) {
            if (moduleName == "TCP_DATA_MANAGER") {
                m_moduleManager->ShutdownTcpDataManager();
            }
            else if (moduleName == "CLD101X_MANAGER") {
                m_moduleManager->ShutdownCLD101xManager();
            }
            else if (moduleName == "KEITHLEY_MANAGER") {
                m_moduleManager->ShutdownKeithleyManager();
            }
            else if (moduleName == "SIPHOG_CLIENT") {
                m_moduleManager->ShutdownSIPHOGClient();
            }
        }

        // ADD DETAILS/UI BUTTON
        ImGui::Spacing();
        std::string detailsButtonLabel = "Open UI##" + moduleName;

        if (ImGui::Button(detailsButtonLabel.c_str(), ImVec2(-1, 25))) {
            if (moduleName == "TCP_DATA_MANAGER") {
                // Set flag to show TCP Data Manager UI (will be handled in MainUIManager)
                m_showTcpDataManagerUI = true;
            }
            else if (moduleName == "CLD101X_MANAGER") {
                // Set flag to show CLD101x Equipment UI
                m_showCLD101xUI = true;
            }
            else if (moduleName == "KEITHLEY_MANAGER") {
                // Set flag to show SMU Manager UI
                m_showKeithleyUI = true;
            }
            else if (moduleName == "SIPHOG_CLIENT") {
                // Open SIPHOG Client window directly
                SIPHOGClient* siphogClient = m_moduleManager->GetSIPHOGClient();
                if (siphogClient) {
                    siphogClient->SetShowWindow(true);
                }
            }
        }
    }

    ImGui::EndChild();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

void DataInstrumentUI::RenderModuleStatus(ModuleStatus status, const std::string& message) {
    ImVec4 statusColor = GetStatusColor(status);
    const char* statusText = GetStatusText(status);

    ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
    ImGui::Text("Status: %s", statusText);
    ImGui::PopStyleColor();

    if (!message.empty()) {
        ImGui::TextWrapped("%s", message.c_str());
    }
}

const char* DataInstrumentUI::GetStatusText(ModuleStatus status) {
    switch (status) {
    case ModuleStatus::NOT_INITIALIZED: return "Not Initialized";
    case ModuleStatus::INITIALIZING: return "Initializing...";
    case ModuleStatus::CONNECTED: return "Connected";
    case ModuleStatus::FAILED: return "Failed";
    case ModuleStatus::DISABLED: return "Disabled";
    default: return "Unknown";
    }
}

ImVec4 DataInstrumentUI::GetStatusColor(ModuleStatus status) {
    switch (status) {
    case ModuleStatus::NOT_INITIALIZED: return ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Gray
    case ModuleStatus::INITIALIZING: return ImVec4(1.0f, 1.0f, 0.0f, 1.0f);    // Yellow
    case ModuleStatus::CONNECTED: return ImVec4(0.0f, 1.0f, 0.0f, 1.0f);       // Green
    case ModuleStatus::FAILED: return ImVec4(1.0f, 0.0f, 0.0f, 1.0f);          // Red
    case ModuleStatus::DISABLED: return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);        // Dark Gray
    default: return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);                            // White
    }
}

std::string DataInstrumentUI::GetModuleDescription(const std::string& moduleName) {
    if (moduleName == "TCP_DATA_MANAGER") {
        return "Manages TCP connections to data servers for real-time data collection and monitoring.";
    }
    else if (moduleName == "CLD101X_MANAGER") {
        return "Controls CLD101x laser temperature controllers for precise thermal management.";
    }
    else if (moduleName == "KEITHLEY_MANAGER") {
        return "Interfaces with Keithley 2400 Source Meter Units for electrical measurements.";
    }
    else if (moduleName == "SIPHOG_CLIENT") {
        return "Communicates with SIPHOG controllers for system integration and control.";
    }
    else {
        return "Unknown module.";
    }
}

// SPECIAL CARD FOR GLOBAL DATA STORE VIEWER
void DataInstrumentUI::RenderGlobalDataStoreCard() {
    const float cardWidth = 300.0f;
    const float cardHeight = 200.0f;

    ImVec2 cursorPos = ImGui::GetCursorPos();

    // Card background - use a different color for the data viewer
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.25f, 0.2f, 1.0f)); // Slightly green tint
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.6f, 0.4f, 1.0f));

    std::string cardId = "##global_data_store_card";
    ImGui::BeginChild(cardId.c_str(), ImVec2(cardWidth, cardHeight), true);

    // Card header
    ImGui::SetWindowFontScale(1.2f);
    ImGui::Text("Global Data Store Viewer");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Separator();
    ImGui::Spacing();

    // Description
    ImGui::TextWrapped("Real-time monitoring of all data channels from active TCP connections and system sensors.");
    ImGui::Spacing();

    // Status - always available (not a module that needs initialization)
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    ImGui::Text("Status: Ready");
    ImGui::PopStyleColor();
    ImGui::TextWrapped("Data viewer is always available");

    // Open button
    ImGui::Spacing();
    if (ImGui::Button("Open Data Viewer", ImVec2(-1, 25))) {
        m_showGlobalDataStoreUI = true;
    }

    ImGui::EndChild();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

void DataInstrumentUI::OnModuleStatusChanged(const std::string& moduleName, ModuleStatus status, const std::string& message) {
    // This callback can be used for logging or additional UI updates
    std::cout << "Module " << moduleName << " status changed to " << GetStatusText(status)
        << ": " << message << std::endl;
}

// DETAILED UI RENDER METHODS
void DataInstrumentUI::RenderTcpDataManagerUI() {
    // Back button
    if (ImGui::Button("<< Back to Module Manager", ImVec2(200, 30))) {
        m_showTcpDataManagerUI = false;
        return;
    }

    ImGui::Spacing();

    // Get TCP Data Manager and render its UI
    DataClientManager* tcpManager = m_moduleManager->GetTcpDataManager();
    if (tcpManager) {
        // Create the detailed UI if not already created
        if (!m_tcpDataManagerDetailUI) {
            m_tcpDataManagerDetailUI = std::make_unique<TCPDataManagerUI>();
            // Initialize with the existing manager (don't create a new one)
            // We'll manually set the manager instead of initializing
        }

        // UPDATE: Call the existing TCPDataManagerUI Update method
        if (m_tcpDataManagerDetailUI->GetManager()) {
            m_tcpDataManagerDetailUI->Update();
        }

        // Render using the existing comprehensive TCP UI
        // Show header
        ImGui::SetWindowFontScale(1.5f);
        ImGui::Text("TCP Data Manager");
        ImGui::SetWindowFontScale(1.0f);

        ImGui::Separator();
        ImGui::Spacing();

        // Get the DataClientManager and force it to be visible for rendering
        bool wasVisible = tcpManager->IsVisible();
        if (!wasVisible) {
            tcpManager->ToggleWindow(); // Make it visible
        }

        // Render the comprehensive TCP Data Manager UI
        tcpManager->RenderUI();

        // Hide it again if it wasn't visible before
        if (!wasVisible) {
            tcpManager->ToggleWindow();
        }

    }
    else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "TCP Data Manager is not initialized!");
        ImGui::Text("Please initialize the TCP Data Manager first from the module cards.");
    }
}

void DataInstrumentUI::RenderCLD101xUI() {
    // Back button
    if (ImGui::Button("<< Back to Module Manager", ImVec2(200, 30))) {
        m_showCLD101xUI = false;
        return;
    }

    ImGui::Spacing();

    // Get CLD101x Manager and render its UI
    CLD101xManager* cldManager = m_moduleManager->GetCLD101xManager();
    if (cldManager) {
        // Create the detailed UI if not already created
        if (!m_cld101xDetailUI) {
            m_cld101xDetailUI = std::make_unique<CLD101xEquipmentUI>();
            m_cld101xDetailUI->SetCLD101xManager(cldManager);
        }

        // Render the comprehensive CLD101x Equipment UI
        m_cld101xDetailUI->Render();

    }
    else {
        ImGui::SetWindowFontScale(1.5f);
        ImGui::Text("CLD101x Laser Equipment");
        ImGui::SetWindowFontScale(1.0f);

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "CLD101x Manager is not initialized!");
        ImGui::Text("Please initialize the CLD101x Manager first from the module cards.");
    }
}

void DataInstrumentUI::RenderKeithleyUI() {
    // Back button
    if (ImGui::Button("<< Back to Module Manager", ImVec2(200, 30))) {
        m_showKeithleyUI = false;
        return;
    }

    ImGui::Spacing();

    // Get Keithley Manager and render its UI
    Keithley2400Manager* keithleyManager = m_moduleManager->GetKeithleyManager();
    if (keithleyManager) {
        // Create the detailed UI if not already created
        if (!m_smuDetailUI) {
            m_smuDetailUI = std::make_unique<UISMUPanel>(*keithleyManager);
        }

        // Render the comprehensive SMU UI
        m_smuDetailUI->RenderUI();

    }
    else {
        ImGui::SetWindowFontScale(1.5f);
        ImGui::Text("Keithley SMU Manager");
        ImGui::SetWindowFontScale(1.0f);

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Keithley Manager is not initialized!");
        ImGui::Text("Please initialize the Keithley Manager first from the module cards.");
    }
}

void DataInstrumentUI::RenderGlobalDataStoreUI() {
    // Back button
    if (ImGui::Button("<< Back to Module Manager", ImVec2(200, 30))) {
        m_showGlobalDataStoreUI = false;
        return;
    }

    ImGui::Spacing();

    // Create the Global Data Store Viewer UI if not already created
    if (!m_globalDataStoreDetailUI) {
        m_globalDataStoreDetailUI = std::make_unique<GlobalDataStoreViewerUI>();

        // Connect it to any available TCP Data Manager for server info
        DataClientManager* tcpManager = m_moduleManager->GetTcpDataManager();
        if (tcpManager) {
            m_globalDataStoreDetailUI->SetDataClientManager(tcpManager);
        }
    }

    // Render the comprehensive Global Data Store Viewer UI
    m_globalDataStoreDetailUI->Render();
}