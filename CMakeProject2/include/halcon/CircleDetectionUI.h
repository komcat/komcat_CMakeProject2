#pragma once
// Add these to CircleDetectionUI.h header file:

#include <nlohmann/json.hpp>
#include <filesystem>
#include "HalconCpp.h"
#include "imgui.h"
#include <iostream>
#include <string>
#include <functional>

// Simple OpenGL includes for texture handling
#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#else
#include <OpenGL/gl.h>
#endif

using namespace HalconCpp;
using json = nlohmann::json;



// Parameters structure for circle detection
struct CircleDetectionParams {
  // ROI Parameters
  int roiSize = 200;              // ±pixels from center
  int roiOffsetX = 0;             // ROI center offset X
  int roiOffsetY = 0;             // ROI center offset Y

  // Threshold Parameters  
  int thresholdLow = 180;         // Min brightness (0-255)
  int thresholdHigh = 255;        // Max brightness (0-255)
  bool invertImage = true;        // Invert before processing

  // Region Filter Parameters
  int minArea = 500;              // Minimum region area
  int maxArea = 99999;            // Maximum region area
  float minCircularity = 0.7f;    // Minimum circularity (0.0-1.0)
  float maxCircularity = 1.0f;    // Maximum circularity (0.0-1.0)

  // Circle Size Parameters
  float minRadius = 40.0f;        // Minimum circle radius
  float maxRadius = 60.0f;        // Maximum circle radius
  float targetRadius = 50.0f;     // Preferred radius

  // Advanced Shape Parameters
  float minCompactness = 1.0f;    // Shape compactness (1.0-10.0)
  float maxCompactness = 10.0f;
  bool useCompactnessFilter = false; // Enable compactness filtering

  // Noise Reduction Parameters
  bool useNoiseReduction = false; // Apply median filter
  int medianKernelSize = 3;       // Median filter size (3, 5, 7, 9)

  // Fallback Parameters
  bool enableFallback = true;     // Use fallback detection
  float fallbackMinRadius = 30.0f; // Fallback radius range
  float fallbackMaxRadius = 80.0f;

  // Real-time Parameters
  bool autoDetect = true;         // Auto-detect on parameter change
  float detectionDelay = 0.5f;    // Delay before auto-detection (seconds)
};

class CircleDetectionUI {
public:
  CircleDetectionUI();
  ~CircleDetectionUI();

  // Main render function - call this in your main loop
  void RenderUI();

  // Check if window is visible
  bool IsVisible() const { return m_showWindow; }

  // Toggle window visibility
  void ToggleWindow() { m_showWindow = !m_showWindow; }

  // Get detection results
  bool GetLastResults(double& centerX, double& centerY, double& radius) const;

  // Set callback for when circle is detected (optional)
  void SetDetectionCallback(std::function<void(double, double, double)> callback);

  // Get current parameters
  const CircleDetectionParams& GetParams() const { return m_params; }

  // Set parameters (for external configuration)
  void SetParams(const CircleDetectionParams& params) { m_params = params; }

  // Named Profile Management
  bool SaveProfile(const std::string& profileName, const std::string& description = "");
  bool LoadProfile(const std::string& profileName);
  bool DeleteProfile(const std::string& profileName);
  std::vector<std::string> GetAvailableProfiles();

  // Profile information
  struct ProfileInfo {
    std::string name;
    std::string description;
    std::string filename;
    std::string lastModified;
    bool isDefault;
  };

  std::vector<ProfileInfo> GetProfileDetails();
  bool SetDefaultProfile(const std::string& profileName);
  std::string GetDefaultProfile();

  // Quick profile switching for machine operations
  bool LoadProfileForOperation(const std::string& operationType);
  void RegisterOperationProfile(const std::string& operationType, const std::string& profileName);
  void RenderProfileManagerUI();
  void ToggleProfileManager() { m_showProfileManager = !m_showProfileManager; }

private:
  // UI state
  bool m_showWindow;
  std::string m_selectedImagePath;
  bool m_imageLoaded;

  // Detection results
  bool m_detectionRan;
  bool m_circleFound;
  double m_centerX;
  double m_centerY;
  double m_radius;

  // Detection parameters
  CircleDetectionParams m_params;

  // Real-time detection
  bool m_parametersChanged;
  float m_parameterChangeTimer;
  bool m_isDetecting;

  // Detection performance
  float m_lastDetectionTime;
  int m_detectionCount;

  // Image display
  unsigned int m_imageTexture;
  int m_imageWidth;
  int m_imageHeight;
  bool m_textureLoaded;

  // Texture size tracking
  uint32_t m_lastTextureWidth;
  uint32_t m_lastTextureHeight;

  // Optional callback for detection results
  std::function<void(double, double, double)> m_detectionCallback;

  // Helper methods
  std::string openFileDialog();
  bool detectCircleCenter(const std::string& imagePath, double& centerX, double& centerY, double& radius);
  bool loadImageAsTexture(const std::string& imagePath);
  void renderImageWithCrosshair();
  void cleanupTexture();

  // Parameter UI methods
  void renderParameterControls();
  void renderROIControls();
  void renderThresholdControls();
  void renderFilterControls();
  void renderAdvancedControls();
  void onParameterChanged();
  void updateAutoDetection();
  void executeDetection();

  // Camera-style texture handling methods
  void UpdateTextureFromHalconImage(const HObject& halconImage);
  void UpdateTextureFromBuffer(const uint8_t* pImageBuffer, uint32_t width, uint32_t height);

  // Profile management
  std::string m_profilesPath = "circle_detection_profiles/";
  std::string m_defaultProfile = "";
  std::map<std::string, std::string> m_operationProfiles; // operation -> profile mapping

  // Internal helpers
  void parametersToJson(nlohmann::json& j);
  void parametersFromJson(const nlohmann::json& j);
  std::string generateProfileFilename(const std::string& profileName);
  void ensureProfileDirectoryExists();
  void loadOperationMappings();
  void saveOperationMappings();

  // UI state for profile management
  bool m_showProfileManager = false;
  char m_newProfileName[256] = "";
  char m_newProfileDescription[512] = "";
  int m_selectedProfileIndex = -1;
  std::string getCurrentTimestamp();
  bool stringStartsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() &&
      str.substr(0, prefix.size()) == prefix;
  }
  // Add this method declaration:
  void CreateTestPatternTexture();
};