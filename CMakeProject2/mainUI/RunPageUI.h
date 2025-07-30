// RunPageUI.h - Add button ordering support
#pragma once

#include "ProcessBuilders.h"
#include "uaa3_process_builders.h"
#include "machine_operations.h"
#include "MockUserInteractionManager.h"
#include "Programming/UserPromptUI.h"
#include "ProcessFilterManager.h"  // Enhanced with button ordering
#include "include/ui/OperationsDisplayUI.h"
#include "logger.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <chrono>

class RunPageUI {
public:
    RunPageUI(MachineOperations& machineOps);
    ~RunPageUI();

    void RenderUI();
    void SetUserPromptUI(UserPromptUI* promptUI) { m_promptUI = promptUI; }
    void SetImguiFont(ImFont* font);
    ImFont* GetImguiFont() const { return m_imguiFont; }

private:
    // Existing members...
    MachineOperations& m_machineOps;
    std::unique_ptr<MockUserInteractionManager> m_uiManager;
    Logger* m_logger;
    ImFont* m_imguiFont = nullptr;
    UserPromptUI* m_promptUI = nullptr;
    std::unique_ptr<OperationsDisplayUI> m_operationsDisplayUI;

    // UI state
    std::string m_statusMessage = "Ready";
    std::string m_selectedProcess = "Initialization";
    bool m_processRunning = false;
    bool m_processPaused = false;
    float m_progress = 0.0f;
    int m_column3TabIndex = 0;

    // Process handling
    std::thread m_processThread;
    std::atomic<bool> m_stopRequested;
    std::atomic<bool> m_pauseRequested;
    std::mutex m_mutex;
    std::atomic<bool> m_autoConfirm;

    // Enhanced filter management with button ordering
    std::unique_ptr<ProcessFilterManager> m_filterManager;
    bool m_showFilterWindow = false;

    // Completed steps tracking
    struct CompletedProcess {
        std::string processName;
        std::string dateTime;
        std::string duration;
        std::string idleTime;
        bool isSuccess;
    };
    std::vector<CompletedProcess> m_completedSteps;
    static const size_t MAX_COMPLETED_STEPS = 50;
    std::chrono::steady_clock::time_point m_processStartTime;
    std::chrono::steady_clock::time_point m_lastProcessEndTime;
    bool m_hasLastProcessEndTime = false;

    // Status messages
    std::vector<std::string> m_statusHistory;
    static const size_t MAX_STATUS_HISTORY = 100;

    // Methods
    void ShowFilterConfiguration() { m_showFilterWindow = true; }

    // UPDATED: Process list methods with sorting support
    std::vector<std::string> GetCurrentProcessList() const;        // Regular filtered list
    std::vector<std::string> GetSortedProcessList() const;         // NEW: Sorted filtered list
    void OnFilterChanged();

    // Process control methods
    void StartProcess(const std::string& processName);
    void PauseProcess();
    void ResumeProcess();
    void StopProcess();
    void UpdateStatus(const std::string& message, bool isError = false);

    // Completed steps management
    void AddCompletedStep(const std::string& stepName, const std::string& duration,
        const std::string& idleTime, bool isSuccess);
    void ClearCompletedSteps();

    // UI rendering methods
    void RenderColumn1();
    void RenderColumn2();
    void RenderColumn3();
    void RenderStatusTab();
    void RenderDetailResultsTab();
    void RenderControlButtons();
    void RenderStatusArea();
    void RenderProcessButtons();          // UPDATED: Now uses sorted list
    void RenderRunningStatus();
    void RenderCompletedSteps();

    // Process management
    std::unique_ptr<SequenceStep> BuildSelectedProcess();
    void ProcessThreadFunc(const std::string& processName);
};