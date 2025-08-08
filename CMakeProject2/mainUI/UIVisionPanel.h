#pragma once

#include "include/halcon/VisionCircleDetection.h"
#include "VisionPresetManager.h"
#include "imgui.h"
#include <memory>
#include <string>
#include <vector>
#include <map>

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

  // NEW: Camera exposure preset structure
  struct CameraExposurePreset {
    float exposureTime = 10000.0f;
    float gain = 1.0f;
    bool autoExposure = false;
    bool autoGain = false;
  };

private:
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

  // NEW: Camera exposure controls
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

  // === INITIALIZATION ===
  void InitializeCircleDetection();
  void InitializePresetManager();

  // === UI RENDERING ===
  void RenderLeftPanel();
  void RenderRightPanel();
  void RenderImageDisplay();
  void RenderNodeListPanel();

  // === CAMERA UI ===
  void RenderCameraSelection();
  std::vector<std::string> GetAvailableCameras();
  bool CaptureImageFromCamera(std::vector<uint8_t>& imageBuffer, int& width, int& height, int& channels);

  // === DETECTION UI ===
  void RenderCircleDetectionControls();
  void RenderCircleDetectionResults();
  void ExecuteCircleDetection();

  // === PARAMETER UI ===
  void RenderCircleParameterControls();
  void LoadParameters();
  void SaveParameters();
  void ResetToDefaults();

  // === PRESET UI ===
  void RenderPresetControls();
  void RenderNewPresetDialog();
  void RenderDeleteConfirmDialog();
  void RefreshPresetList();
  bool LoadPreset(int presetId);
  bool SaveCurrentAsPreset(const std::string& name, const std::string& description = "");
  bool LoadPresetByName(const std::string& name);

  // === AUTO-EXECUTION ===
  void UpdateAutoExecution();
  void RenderAutoExecutionControls();

  // NEW: Camera exposure controls
  void RenderExposureControls();
  void ApplyExposureSettings();
  void UpdateExposureUIFromCamera();

  // NEW: Camera exposure preset methods
  CameraExposurePreset GetCurrentExposureSettings() const;
  void ApplyExposurePreset(const CameraExposurePreset& preset);
  nlohmann::json ExposurePresetToJson(const CameraExposurePreset& preset) const;
  CameraExposurePreset ExposurePresetFromJson(const nlohmann::json& j) const;

  // === NODE NAVIGATION ===
  void RenderNodeListControls();
  void RenderNodeListTable();
  void LoadNodesFromConfig();
  bool NavigateToNode(const std::string& nodeId);

  // === IMAGE DISPLAY ===
  void UpdateImageTexture(const std::vector<uint8_t>& imageData, int width, int height, int channels);
  void CleanupImageTexture();
  void RenderImageWithOverlay();

  // New method for processing image with current parameters
  void ProcessAndUpdateImageTexture(const std::vector<uint8_t>& originalImageData, int width, int height, int channels);

  // Helper to apply image processing pipeline
  std::vector<uint8_t> ApplyImageProcessing(const std::vector<uint8_t>& imageData, int width, int height, int channels);

  // Store original image data
  std::vector<uint8_t> m_originalImageData;
  bool m_hasOriginalData = false;

  bool m_showInvertPreview = false;
  unsigned int m_invertedTextureId = 0;
  std::vector<unsigned char> m_invertedImageData;

  void RenderInvertPreviewDialog();
  void CreateInvertedTexture();
  void CleanupInvertedTexture();
};