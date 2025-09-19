// RunPageUI.h - Add Live View camera support
#pragma once

#include "ProcessBuilders.h"
#include "uaa3_process_builders.h"
#include "machine_operations.h"
#include "MockUserInteractionManager.h"
#include "Programming/UserPromptUI.h"
#include "ProcessFilterManager.h"
#include "include/ui/OperationsDisplayUI.h"
#include "include/camera/ICameraHardware.h"  // NEW: Add for camera hardware interface
#include "include/data/LiveDataPlot.h"
#include "logger.h"
#include "CameraViewport.h"
#include "LiveVideoSubscriber.h"  // NEW: Add camera support
// Add to includes section
#include "ProcessConfiguration.h"
#include "ProcessConfigBuilders.h"
#include "ProcessConfigUI.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <chrono>
#include <SDL_opengl.h>  // NEW: For OpenGL texture management
#include "EmbeddedJogControl.h"

using namespace UAA3ProcessBuilders;

// Forward declarations
class CameraManager;

class RunPageUI {
public:
  RunPageUI(MachineOperations& machineOps);
  ~RunPageUI();

  void RenderUI();
  void SetUserPromptUI(UserPromptUI* promptUI) { m_promptUI = promptUI; }
  void SetImguiFont(ImFont* font);
  ImFont* GetImguiFont() const { return m_imguiFont; }

  // NEW: Camera management
  void SetCameraManager(CameraManager* cameraManager);
  void SetSelectedCamera(const std::string& cameraId);
  void InitializeCameraFeed();
  void ClearCameraFeed();
  bool HasUserPromptUI() const {
    return m_promptUI != nullptr;
  }

private:
  // Existing members...
  MachineOperations& m_machineOps;
  std::unique_ptr<MockUserInteractionManager> m_uiManager;
  Logger* m_logger;
  ImFont* m_imguiFont = nullptr;
  UserPromptUI* m_promptUI = nullptr;
  std::unique_ptr<OperationsDisplayUI> m_operationsDisplayUI;
  std::unique_ptr<EmbeddedJogControl> m_jogControl;
  // Process configuration
  std::unique_ptr<ProcessConfigUI> m_processConfigUI;
  ProcessConfiguration m_currentProcessConfig;
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

  // NEW: Camera system for Live View tab
  CameraManager* m_cameraManager = nullptr;
  std::shared_ptr<LiveVideoSubscriber> m_cameraSubscriber;
  std::string m_selectedCameraId;
  bool m_cameraSystemInitialized = false;

  // OpenGL texture management for camera feed
  unsigned int m_cameraTextureID = 0;
  bool m_textureInitialized = false;
  uint32_t m_textureWidth = 0;
  uint32_t m_textureHeight = 0;

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

  // Process list methods with sorting support
  std::vector<std::string> GetCurrentProcessList() const;
  std::vector<std::string> GetSortedProcessList() const;
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
  void RenderLiveViewTab();  // NEW: Live View tab
  void RenderControlButtons();
  void RenderStatusArea();
  void RenderProcessButtons();
  void RenderRunningStatus();
  void RenderCompletedSteps();

  // NEW: Camera rendering methods
  void RenderCameraFeedFromSubscriber(const ImVec2& canvasSize);
  void RenderCameraPlaceholder(const ImVec2& canvasSize, const std::string& message);
  void RenderCameraControls();
  void UpdateCameraTexture();
  void CleanupCameraTexture();

  // Process management
  std::unique_ptr<SequenceStep> BuildSelectedProcess();
  void ProcessThreadFunc(const std::string& processName);

  // NEW: Crosshair overlay control
  bool m_showCrosshair = false;



  // NEW: Add this method declaration with other camera rendering methods
  void RenderCrosshairOverlay(const ImVec2& canvasPos, const ImVec2& canvasSize);

  // NEW: Data overlay members
  std::string m_selectedDataChannel;
  bool m_showDataOverlay = false;
  std::map<std::string, float> m_lastDataValues; // For change detection


  // NEW: Spec value members
  std::map<std::string, float> m_specValues;        // Parsed spec values per channel
  std::map<std::string, std::string> m_specValueStrings; // String input per channel
  std::map<std::string, bool> m_hasValidSpec;      // Whether spec is valid per channel

  // NEW: Add these method declarations with other rendering methods
  void RenderDataValueOverlay(const ImVec2& canvasPos, const ImVec2& canvasSize);
  void FormatDataValueWithUnit(const std::string& channel, float value, char* buffer, size_t bufferSize);
  bool HasDataValueChanged(const std::string& channel, float currentValue);


  // New members for jog control
  int m_selectedJogAxis = 0;  // 0=X, 1=Y, 2=Z, 3=R
  int m_jogStepSize = 1;      // 0=0.01mm, 1=0.1mm, 2=1mm, 3=10mm

  // New methods
  void RenderStatusTabCol2();
  void RenderProcessConfigTab();
  void RenderJogControlTab();
  void RenderProcessTreeView();
  void RenderProgressBar();

  bool m_autoStartOnSelect = false;  // Checkbox state for automatic start
  std::string m_selectedTreeNode = "";  // Currently selected node in tree



  // Store the operations of the selected process
  std::vector<std::string> m_selectedProcessOperations;
  // Track current operation during execution
  size_t m_currentOperationIndex = 0;
  size_t m_lastCompletedIndex = 0;  // ADD THIS
  bool m_operationInProgress = false;
  std::mutex m_operationMutex;  // For thread-safe operation tracking

  // Add method declarations
  void ExtractSelectedProcessOperations();
  void RenderSequenceBreakdownTab();



  // Camera viewport for embedded display
  std::unique_ptr<CameraViewport> m_cameraViewport;
  void InitializeCameraViewport();

  // Add to private members in RunPageUI.h:
  std::shared_ptr<LiveVideoSubscriber> m_embeddedCameraSubscriber;

  // Add to private methods in RunPageUI.h:
  void InitializeEmbeddedCameraFeed();
  void ClearEmbeddedCameraFeed();
  void RenderEmbeddedCameraFeed(const ImVec2& canvasSize);

  void DebugCameraFrameFlow();

  // In the private section
  LiveDataPlot* m_liveDataPlot = nullptr;
  bool m_plotInitialized = false;
  // Method to check if plot is initialized
  bool IsPlotInitialized() const { return m_liveDataPlot != nullptr; }

  // Panel expansion state
  enum class PanelState {
    Normal,     // All 3 panels visible
    Panel1,     // Only Panel 1 expanded
    Panel2,     // Only Panel 2 expanded  
    Panel3      // Only Panel 3 expanded
  };
  PanelState m_panelState = PanelState::Normal;

  // Method to handle panel clicks
  void HandlePanelClick(int panelNumber);
  void RenderPanelContent(int panelNumber, const ImVec2& size);
  // Add this with the other render methods
  void RenderConfigurableTab();

  // === ADD THESE NEW MEMBERS FOR SPEC CONTROL ===
// Spec threshold management
  float m_specThreshold = 800e-6f;      // Default 800uA in Amps (scientific notation)
  char m_specInputBuffer[32] = "800";   // Input buffer for ImGui text field
  std::string m_specUnit = "uA";        // Current unit selection (uA default)
  bool m_specEnabled = true;            // Enable/disable spec comparison

  // Comparison thresholds structure
  struct SpecThresholds {
    float excellent = 110.0f;  // >110% threshold
    float pass = 100.0f;       // >100% threshold  
    float needWork = 95.0f;    // >95% threshold
  } m_thresholds;


  // === ADD THESE NEW METHOD DECLARATIONS ===
 // Main rendering methods for split view
  void RenderSpecControl();           // Renders left column spec control panel
  void RenderLivePlot();              // Renders right column live plot

  // Helper methods for spec control
  void UpdateSpecThreshold();                                    // Updates threshold from input
  float GetUnitMultiplier(const std::string& unit);             // Converts unit to multiplier
  int GetUnitIndex(const std::string& unit);                    // Gets combo box index for unit
  void FormatCurrentValue(float value, char* buffer, size_t bufferSize);  // Formats value with units

  // Visual feedback helpers
  ImVec4 GetPercentageColor(float percentage);                  // Returns color based on percentage
  const char* GetStatusText(float percentage);                  // Returns status text
  void RenderComparisonBar(float percentage);                   // Renders visual prog



  std::string m_plotChannelName = "GPIB-Current";  // Current channel for plot
  std::vector<std::string> m_availableChannels;    // Cache of available channels
  int m_selectedChannelIndex = 0;

  void ChangeChannel(const std::string& channelName);
};