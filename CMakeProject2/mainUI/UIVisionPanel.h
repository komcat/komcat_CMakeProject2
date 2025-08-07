#pragma once

#include "include/halcon/VisionCircleDetection.h"
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

  // Camera integration
  void SetCameraManager(CameraManager* cameraManager);

  // NEW: Motion system integration
  void SetMotionConfigManager(MotionConfigManager* configManager);
  void SetMachineOperations(MachineOperations* machineOps);

private:
  // UI state
  bool m_showWindow = true;
  bool m_showNodeList = false;  // NEW: Toggle for node list panel

  // Circle Detection
  std::unique_ptr<VisionCircleDetection> m_circleDetector;
  VisionCircleDetection::Result m_lastResult;
  bool m_hasResult = false;

  // NEW: Automatic periodic execution
  bool m_autoExecute = false;
  float m_autoExecuteInterval = 200.0f;  // milliseconds
  float m_lastAutoExecuteTime = 0.0f;

  // Camera integration
  CameraManager* m_cameraManager = nullptr;
  std::string m_selectedCameraId = "";

  // NEW: Motion system integration
  MotionConfigManager* m_configManager = nullptr;
  MachineOperations* m_machineOperations = nullptr;

  // Parameter file path
  std::string m_parameterFilePath = "vision_circle_params.json";

  // Image display
  unsigned int m_imageTextureId = 0;
  int m_imageWidth = 0;
  int m_imageHeight = 0;
  std::vector<uint8_t> m_lastImageData;
  bool m_hasImageData = false;

  // NEW: Node list data
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

  // UI Rendering Methods
  void RenderLeftPanel();    // Algorithm selection and controls
  void RenderRightPanel();   // Results and parameters
  void RenderImageDisplay(); // Image with detection overlay
  void RenderNodeListPanel(); // NEW: Node list panel

  // Circle Detection UI
  void RenderCircleDetectionControls();
  void RenderCircleDetectionResults();
  void RenderCircleParameterControls();
  void RenderCameraSelection();

  // NEW: Auto-execution methods
  void UpdateAutoExecution();
  void RenderAutoExecutionControls();

  // NEW: Node list UI methods
  void RenderNodeListControls();
  void RenderNodeListTable();
  void LoadNodesFromConfig();
  bool NavigateToNode(const std::string& nodeId);

  // Execution
  void ExecuteCircleDetection();

  // Parameter Management  
  void LoadParameters();
  void SaveParameters();
  void ResetToDefaults();

  // Camera Methods
  std::vector<std::string> GetAvailableCameras();
  bool CaptureImageFromCamera(std::vector<uint8_t>& imageBuffer, int& width, int& height, int& channels);

  // Initialization
  void InitializeCircleDetection();

  // Image texture management
  void UpdateImageTexture(const std::vector<uint8_t>& imageData, int width, int height, int channels);
  void CleanupImageTexture();
  void RenderImageWithOverlay();
};