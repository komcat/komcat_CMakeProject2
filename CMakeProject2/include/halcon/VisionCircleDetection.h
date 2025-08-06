#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

/**
 * @brief Simple circle detection class without UI components
 *
 * This class provides core circle detection functionality using HALCON
 * with configurable parameters loaded from JSON files.
 *
 * Uses PIMPL pattern to hide HALCON dependencies from header.
 */
class VisionCircleDetection {
public:
  // Detection parameters structure
  struct Parameters {
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
  };

  // Detection result structure
  struct Result {
    bool found = false;             // Whether circle was detected
    double centerX = 0.0;           // X coordinate of center
    double centerY = 0.0;           // Y coordinate of center  
    double radius = 0.0;            // Radius of detected circle
    double confidence = 0.0;        // Detection confidence (0.0-1.0)
    std::string errorMessage = "";  // Error message if detection failed

    // Additional metrics
    double circularity = 0.0;       // Shape circularity
    double area = 0.0;              // Region area
    int numCandidates = 0;          // Number of candidate regions found
  };

public:
  VisionCircleDetection();
  ~VisionCircleDetection();

  // Disable copy constructor and assignment operator
  VisionCircleDetection(const VisionCircleDetection&) = delete;
  VisionCircleDetection& operator=(const VisionCircleDetection&) = delete;

  // Move constructor and assignment are allowed
  VisionCircleDetection(VisionCircleDetection&&) noexcept;
  VisionCircleDetection& operator=(VisionCircleDetection&&) noexcept;

  /**
   * @brief Detect circle in image file
   * @param imagePath Path to image file
   * @return Detection result with coordinates and metadata
   */
  Result DetectFromFile(const std::string& imagePath);

  /**
   * @brief Detect circle from raw image buffer
   * @param imageBuffer Raw image data (RGB or grayscale)
   * @param width Image width
   * @param height Image height
   * @param channels Number of channels (1 or 3)
   * @return Detection result with coordinates and metadata
   */
  Result DetectFromBuffer(const uint8_t* imageBuffer, int width, int height, int channels = 3);

  /**
   * @brief Load parameters from JSON file
   * @param jsonPath Path to JSON parameter file
   * @return True if loaded successfully
   */
  bool LoadParameters(const std::string& jsonPath);

  /**
   * @brief Save parameters to JSON file
   * @param jsonPath Path to JSON parameter file
   * @return True if saved successfully
   */
  bool SaveParameters(const std::string& jsonPath) const;

  /**
   * @brief Set detection parameters directly
   * @param params Parameter structure
   */
  void SetParameters(const Parameters& params);

  /**
   * @brief Get current detection parameters
   * @return Current parameter structure
   */
  const Parameters& GetParameters() const;

  /**
   * @brief Create default parameter JSON file
   * @param jsonPath Path where to save default parameters
   * @return True if created successfully
   */
  static bool CreateDefaultParameterFile(const std::string& jsonPath);

  /**
   * @brief Get default parameters
   * @return Default parameter structure
   */
  static Parameters GetDefaultParameters();

  /**
   * @brief Get last detection processing time in milliseconds
   * @return Processing time in ms
   */
  double GetLastProcessingTime() const;

  /**
   * @brief Get last error message
   * @return Error message string
   */
  std::string GetLastError() const;

  /**
   * @brief Check if HALCON is properly initialized
   * @return True if HALCON is available and working
   */
  bool IsHalconAvailable() const;

private:
  // PIMPL - Hide all HALCON implementation details
  class Implementation;
  std::unique_ptr<Implementation> m_impl;
};