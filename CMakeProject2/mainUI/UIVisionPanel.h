// =============================================================================
// UIVisionPanel.h - Updated with Node-Preset Integration
// =============================================================================

#pragma once

#include "include/halcon/VisionCircleDetection.h"
#include "VisionPresetManager.h"
#include "imgui.h"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <chrono>

// Forward declarations
class CameraManager;
class ICameraHardware;
class MotionConfigManager;
class MachineOperations;

class UIVisionPanel {
public:
  UIVisionPanel();
  ~UIVisionPanel();

  void RenderUI();
  void ToggleWindow() { m_showWindow = !m_showWindow; }
  bool IsVisible() const { return m_showWindow; }

  // System integration
  void SetCameraManager(CameraManager* cameraManager);
  void SetMotionConfigManager(MotionConfigManager* configManager);
  void SetMachineOperations(MachineOperations* machineOps);

  // Camera exposure preset structure
  struct CameraExposurePreset {
    float exposureTime = 10000.0f;
    float gain = 1.0f;
    bool autoExposure = false;
    bool autoGain = false;
  };

private:
  // ============================================================================
  // MEMBER VARIABLES
  // ============================================================================

  // UI state
  bool m_showWindow = true;
  bool m_showNodeList = false;

  // Core systems
  std::unique_ptr<VisionCircleDetection> m_circleDetector;
  std::unique_ptr<VisionPresetManager> m_presetManager;
  VisionCircleDetection::Result m_lastResult;
  bool m_hasResult = false;

  // Preset management
  std::vector<VisionPresetManager::PresetInfo> m_availablePresets;
  int m_selectedPresetId = -1;
  std::string m_newPresetName = "";
  std::string m_newPresetDescription = "";
  bool m_showNewPresetDialog = false;
  bool m_showDeleteConfirmDialog = false;
  int m_presetToDelete = -1;

  // Auto-execution
  bool m_autoExecute = false;
  float m_autoExecuteInterval = 200.0f;
  float m_lastAutoExecuteTime = 0.0f;

  // Camera integration
  CameraManager* m_cameraManager = nullptr;
  std::string m_selectedCameraId = "";

  // Camera exposure controls
  float m_exposureTimeUI = 10000.0f;  // microseconds
  float m_gainUI = 1.0f;              // gain value
  bool m_autoExposureUI = false;      // auto exposure enabled
  bool m_autoGainUI = false;          // auto gain enabled

  // Motion system integration
  MotionConfigManager* m_configManager = nullptr;
  MachineOperations* m_machineOperations = nullptr;

  // Parameter management
  std::string m_parameterFilePath = "vision_circle_params.json";

  // Image display
  unsigned int m_imageTextureId = 0;
  int m_imageWidth = 0;
  int m_imageHeight = 0;
  std::vector<uint8_t> m_lastImageData;
  bool m_hasImageData = false;

  // Store original image data
  std::vector<uint8_t> m_originalImageData;
  bool m_hasOriginalData = false;

  bool m_showInvertPreview = false;
  unsigned int m_invertedTextureId = 0;
  std::vector<unsigned char> m_invertedImageData;

  // Node list data
  struct NodeInfo {
    std::string id;
    std::string name;
    std::string graphName;
    std::string deviceName;
    std::string positionName;
    bool isReachable = true;
    bool isCurrentPosition = false;
  };
  std::vector<NodeInfo> m_nodes;
  std::string m_selectedNodeId = "";
  std::string m_filterText = "";

  // Node-Preset Association Layer
  struct NodePresetMapping {
    std::string nodeId;
    int presetId;
    std::string presetName;
    std::string guidanceImagePath;  // Path to reference image
    bool autoLoad = true;          // Auto-load when navigating to node
  };

  std::vector<NodePresetMapping> m_nodePresetMappings;
  std::map<std::string, int> m_nodeToPresetMap;  // Quick lookup
  bool m_showNodePresetDialog = false;
  std::string m_selectedNodeForPreset = "";

  // ============================================================================
  // INITIALIZATION METHODS
  // ============================================================================
  void InitializeCircleDetection();
  void InitializePresetManager();

  // ============================================================================
  // UI RENDERING METHODS
  // ============================================================================
  void RenderLeftPanel();
  void RenderRightPanel();
  void RenderImageDisplay();
  void RenderNodeListPanel();

  // ============================================================================
  // CAMERA METHODS
  // ============================================================================
  void RenderCameraSelection();
  std::vector<std::string> GetAvailableCameras();
  bool CaptureImageFromCamera(std::vector<uint8_t>& imageBuffer, int& width, int& height, int& channels);

  // Camera exposure controls
  void RenderExposureControls();
  void ApplyExposureSettings();
  void UpdateExposureUIFromCamera();

  // Camera exposure preset methods
  CameraExposurePreset GetCurrentExposureSettings() const;
  void ApplyExposurePreset(const CameraExposurePreset& preset);
  nlohmann::json ExposurePresetToJson(const CameraExposurePreset& preset) const;
  CameraExposurePreset ExposurePresetFromJson(const nlohmann::json& j) const;

  // ============================================================================
  // DETECTION METHODS
  // ============================================================================
  void RenderCircleDetectionControls();
  void RenderCircleDetectionResults();
  void ExecuteCircleDetection();

  // ============================================================================
  // PARAMETER METHODS
  // ============================================================================
  void RenderCircleParameterControls();
  void LoadParameters();
  void SaveParameters();
  void ResetToDefaults();

  // ============================================================================
  // PRESET METHODS
  // ============================================================================
  void RenderPresetControls();
  void RenderNewPresetDialog();
  void RenderDeleteConfirmDialog();
  void RefreshPresetList();
  bool LoadPreset(int presetId);
  bool SaveCurrentAsPreset(const std::string& name, const std::string& description = "");
  bool LoadPresetByName(const std::string& name);

  // ============================================================================
  // AUTO-EXECUTION METHODS
  // ============================================================================
  void UpdateAutoExecution();
  void RenderAutoExecutionControls();

  // ============================================================================
  // NODE NAVIGATION METHODS
  // ============================================================================
  void RenderNodeListControls();
  void RenderNodeListTable();
  void LoadNodesFromConfig();
  bool NavigateToNode(const std::string& nodeId);

  // ============================================================================
  // NODE-PRESET INTEGRATION METHODS
  // ============================================================================
  void RenderNodePresetControls();
  void RenderNodePresetDialog();
  void LoadNodePresetMappings();
  void SaveNodePresetMappings();
  bool AssignPresetToNode(const std::string& nodeId, int presetId);
  bool DeleteNodePresetMapping(const std::string& nodeId);  // ADD THIS LINE
  void CreateNodePresetTable();

  // ============================================================================
  // IMAGE DISPLAY METHODS
  // ============================================================================
  void UpdateImageTexture(const std::vector<uint8_t>& imageData, int width, int height, int channels);
  void CleanupImageTexture();
  void RenderImageWithOverlay();
  void ProcessAndUpdateImageTexture(const std::vector<uint8_t>& originalImageData, int width, int height, int channels);
  std::vector<uint8_t> ApplyImageProcessing(const std::vector<uint8_t>& imageData, int width, int height, int channels);
  void RenderInvertPreviewDialog();
  void CreateInvertedTexture();
  void CleanupInvertedTexture();

  // ============================================================================
  // IMAGE SAVING METHODS
  // ============================================================================
  bool SaveGuidanceImageForNode(const std::string& nodeId);
  bool SaveGuidanceImageForNodeAdvanced(const std::string& nodeId, const std::string& format = "jpg");
  bool SaveVisionResultWithOverlay(const std::string& nodeId, const std::string& suffix = "");
  bool UpdateGuidanceImagePath(const std::string& nodeId, const std::string& imagePath);
  std::vector<std::string> GetSupportedImageFormats() const;
  bool IsImageReadyForSaving() const;
};