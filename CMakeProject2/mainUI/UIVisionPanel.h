#pragma once

#include "include/halcon/VisionCircleDetection.h"
#include "imgui.h"
#include <memory>
#include <string>
#include <vector>

// Forward declarations
class CameraManager;
class ICameraHardware;

class UIVisionPanel {
public:
  UIVisionPanel();
  ~UIVisionPanel();

  void RenderUI();
  void ToggleWindow() { m_showWindow = !m_showWindow; }
  bool IsVisible() const { return m_showWindow; }

  // Camera integration
  void SetCameraManager(CameraManager* cameraManager);

private:
  // UI state
  bool m_showWindow = true;

  // Circle Detection
  std::unique_ptr<VisionCircleDetection> m_circleDetector;
  VisionCircleDetection::Result m_lastResult;
  bool m_hasResult = false;

  // Camera integration
  CameraManager* m_cameraManager = nullptr;
  std::string m_selectedCameraId = "";

  // Parameter file path
  std::string m_parameterFilePath = "vision_circle_params.json";

  // UI state
  bool m_showParameters = false;

  // Image display
  unsigned int m_imageTextureId = 0;
  int m_imageWidth = 0;
  int m_imageHeight = 0;
  std::vector<uint8_t> m_lastImageData;
  bool m_hasImageData = false;

  // UI Rendering Methods
  void RenderLeftPanel();    // Algorithm selection and controls
  void RenderRightPanel();   // Results and parameters
  void RenderImageDisplay(); // Image with detection overlay

  // Circle Detection UI
  void RenderCircleDetectionControls();
  void RenderCircleDetectionResults();
  void RenderCircleParameterControls();
  void RenderCameraSelection();

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