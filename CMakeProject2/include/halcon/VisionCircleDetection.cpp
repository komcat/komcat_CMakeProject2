#include "VisionCircleDetection.h"

// HALCON includes ONLY in the .cpp file
#include "HalconCpp.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <cmath>
#include <algorithm>

using namespace HalconCpp;

// ============================================================================
// PIMPL IMPLEMENTATION CLASS (Hidden from header)
// ============================================================================

class VisionCircleDetection::Implementation {
public:
  Parameters m_params;
  double m_lastProcessingTime = 0.0;
  std::string m_lastError = "";
  bool m_halconInitialized = false;

  Implementation() {
    try {
      // Initialize HALCON
      SetHcppInterfaceStringEncodingIsUtf8(false);
      m_halconInitialized = true;

      // Set default parameters
      m_params = VisionCircleDetection::GetDefaultParameters();

      std::cout << "[VisionCircleDetection] HALCON initialized successfully" << std::endl;
    }
    catch (HException& ex) {
      setError("HALCON initialization failed: " + std::string(ex.ErrorMessage().TextA()));
      m_halconInitialized = false;
      std::cout << "[VisionCircleDetection] " << m_lastError << std::endl;
    }
    catch (const std::exception& ex) {
      setError("Standard exception during HALCON init: " + std::string(ex.what()));
      m_halconInitialized = false;
      std::cout << "[VisionCircleDetection] " << m_lastError << std::endl;
    }
  }

  ~Implementation() {
    // HALCON cleanup is automatic
  }

  VisionCircleDetection::Result detectFromFile(const std::string& imagePath) {
    VisionCircleDetection::Result result;
    clearError();

    if (!m_halconInitialized) {
      setError("HALCON not initialized");
      result.errorMessage = m_lastError;
      return result;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
      HObject image;
      ReadImage(&image, imagePath.c_str());
      result = executeDetection(image);
    }
    catch (HException& ex) {
      setError("Failed to read image: " + std::string(ex.ErrorMessage().TextA()));
      result.errorMessage = m_lastError;
    }
    catch (const std::exception& ex) {
      setError("Standard exception: " + std::string(ex.what()));
      result.errorMessage = m_lastError;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    m_lastProcessingTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    return result;
  }

  VisionCircleDetection::Result detectFromBuffer(const uint8_t* imageBuffer, int width, int height, int channels) {
    VisionCircleDetection::Result result;
    clearError();

    if (!m_halconInitialized) {
      setError("HALCON not initialized");
      result.errorMessage = m_lastError;
      return result;
    }

    if (!imageBuffer || width <= 0 || height <= 0 || (channels != 1 && channels != 3)) {
      setError("Invalid image buffer parameters");
      result.errorMessage = m_lastError;
      return result;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
      HObject image;

      if (channels == 1) {
        // Grayscale image
        GenImage1(&image, "byte", width, height,
          reinterpret_cast<Hlong>(imageBuffer));
      }
      else if (channels == 3) {
        // RGB image - create interleaved image
        GenImageInterleaved(&image,
          reinterpret_cast<Hlong>(imageBuffer),
          "rgb", width, height, -1, "byte",
          width, height, 0, 0, -1, 0);
      }

      result = executeDetection(image);
    }
    catch (HException& ex) {
      setError("Failed to create HALCON image from buffer: " + std::string(ex.ErrorMessage().TextA()));
      result.errorMessage = m_lastError;
    }
    catch (const std::exception& ex) {
      setError("Standard exception: " + std::string(ex.what()));
      result.errorMessage = m_lastError;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    m_lastProcessingTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    return result;
  }

  VisionCircleDetection::Result executeDetection(const HObject& image) {
    VisionCircleDetection::Result result;

    // Validate parameters before processing
    if (!validateParameters()) {
      setError("Invalid detection parameters");
      result.errorMessage = m_lastError;
      return result;
    }

    try {
      // Local iconic variables
      HObject ho_ImageProcessed, ho_NewRectangularDomain;
      HObject ho_ImageWithNewDomain, ho_RegionThreshold, ho_SeparateRegions;
      HObject ho_SelectedRegions, ho_FilteredRegions, ho_LargestRegion;

      // Local control variables
      HTuple hv_imageWidth, hv_imageHeight, hv_circle_col;
      HTuple hv_circle_row, hv_domainColumn1, hv_domainColumn2;
      HTuple hv_domainRow1, hv_domainRow2;

      // Get image dimensions
      GetImageSize(image, &hv_imageWidth, &hv_imageHeight);

      // Apply noise reduction if enabled
      if (m_params.useNoiseReduction) {
        MedianImage(image, &ho_ImageProcessed, "circle", m_params.medianKernelSize, "mirrored");
      }
      else {
        ho_ImageProcessed = image;
      }

      // Apply inversion if enabled
      if (m_params.invertImage) {
        InvertImage(ho_ImageProcessed, &ho_ImageProcessed);
      }

      // Calculate ROI with parameters
      hv_circle_col = (hv_imageWidth / 2) + m_params.roiOffsetX;
      hv_circle_row = (hv_imageHeight / 2) + m_params.roiOffsetY;
      hv_domainColumn1 = hv_circle_col - m_params.roiSize;
      hv_domainColumn2 = hv_circle_col + m_params.roiSize;
      hv_domainRow1 = hv_circle_row - m_params.roiSize;
      hv_domainRow2 = hv_circle_row + m_params.roiSize;

      // Create rectangular domain
      GenRectangle1(&ho_NewRectangularDomain, hv_domainRow1, hv_domainColumn1,
        hv_domainRow2, hv_domainColumn2);

      // Reduce domain
      ReduceDomain(ho_ImageProcessed, ho_NewRectangularDomain, &ho_ImageWithNewDomain);

      // Threshold with parameters
      Threshold(ho_ImageWithNewDomain, &ho_RegionThreshold, m_params.thresholdLow, m_params.thresholdHigh);

      // Connection
      Connection(ho_RegionThreshold, &ho_SeparateRegions);

      // Filter regions by area
      SelectShape(ho_SeparateRegions, &ho_SelectedRegions, "area", "and", m_params.minArea, m_params.maxArea);

      // Filter by circularity
      SelectShape(ho_SelectedRegions, &ho_FilteredRegions, "circularity", "and",
        m_params.minCircularity, m_params.maxCircularity);

      // Apply compactness filter if enabled
      if (m_params.useCompactnessFilter) {
        HObject ho_CompactnessFiltered;
        SelectShape(ho_FilteredRegions, &ho_CompactnessFiltered, "compactness", "and",
          m_params.minCompactness, m_params.maxCompactness);
        ho_FilteredRegions = ho_CompactnessFiltered;
      }

      // Find best circle using parameters
      HTuple hv_Number, hv_BestRadius, hv_BestRow, hv_BestCol;
      HTuple hv_MinRadiusDiff, hv_BestCircularity, hv_BestArea;
      hv_MinRadiusDiff = 999999;
      bool circleFound = false;

      CountObj(ho_FilteredRegions, &hv_Number);
      result.numCandidates = hv_Number[0].I();

      // Check each region for circles with desired radius
      for (int i = 1; i <= hv_Number[0].I(); i++) {
        try {
          HObject ho_CurrentRegion;
          HTuple hv_CurrentRow, hv_CurrentCol, hv_CurrentRadius;
          HTuple hv_CurrentCircularity, hv_CurrentArea;

          SelectObj(ho_FilteredRegions, &ho_CurrentRegion, i);
          SmallestCircle(ho_CurrentRegion, &hv_CurrentRow, &hv_CurrentCol, &hv_CurrentRadius);

          // Get additional metrics using correct HALCON functions
          Circularity(ho_CurrentRegion, &hv_CurrentCircularity);
          AreaCenter(ho_CurrentRegion, &hv_CurrentArea, &hv_CurrentRow, &hv_CurrentCol);

          if (hv_CurrentRadius.Length() > 0) {
            double currentRadius = hv_CurrentRadius[0].D();

            // Check if radius is within desired range
            if (currentRadius >= m_params.minRadius && currentRadius <= m_params.maxRadius) {
              double radiusDiff = std::abs(currentRadius - m_params.targetRadius);

              if (radiusDiff < hv_MinRadiusDiff[0].D()) {
                hv_MinRadiusDiff = radiusDiff;
                hv_BestRadius = currentRadius;
                hv_BestRow = hv_CurrentRow[0].D();
                hv_BestCol = hv_CurrentCol[0].D();
                hv_BestCircularity = hv_CurrentCircularity[0].D();
                hv_BestArea = hv_CurrentArea[0].D();
                circleFound = true;
              }
            }
          }
        }
        catch (HException& ex) {
          // Continue with next region if this one fails
          continue;
        }
      }

      // Fallback approach if enabled and no circle found
      if (!circleFound && m_params.enableFallback) {
        try {
          SelectShapeStd(ho_FilteredRegions, &ho_LargestRegion, "max_area", 70);
          HTuple hv_CircleRow2, hv_CircleCol2, hv_Radius2;
          HTuple hv_FallbackCircularity, hv_FallbackArea;

          SmallestCircle(ho_LargestRegion, &hv_CircleRow2, &hv_CircleCol2, &hv_Radius2);
          Circularity(ho_LargestRegion, &hv_FallbackCircularity);
          AreaCenter(ho_LargestRegion, &hv_FallbackArea, &hv_CircleRow2, &hv_CircleCol2);

          if (hv_CircleRow2.Length() > 0 && hv_CircleCol2.Length() > 0 && hv_Radius2.Length() > 0) {
            double fallbackRadius = hv_Radius2[0].D();

            if (fallbackRadius >= m_params.fallbackMinRadius && fallbackRadius <= m_params.fallbackMaxRadius) {
              hv_BestRadius = fallbackRadius;
              hv_BestRow = hv_CircleRow2[0].D();
              hv_BestCol = hv_CircleCol2[0].D();
              hv_BestCircularity = hv_FallbackCircularity[0].D();
              hv_BestArea = hv_FallbackArea[0].D();
              circleFound = true;
            }
          }
        }
        catch (HException& ex) {
          // Fallback failed, but that's okay
        }
      }

      // Return results if found
      if (circleFound) {
        result.found = true;
        result.centerX = hv_BestCol[0].D();
        result.centerY = hv_BestRow[0].D();
        result.radius = hv_BestRadius[0].D();
        result.circularity = hv_BestCircularity[0].D();
        result.area = hv_BestArea[0].D();

        // Calculate confidence based on how close radius is to target
        double radiusDiff = std::abs(result.radius - m_params.targetRadius);
        double maxDiff = (std::max)(m_params.targetRadius - m_params.minRadius,
          m_params.maxRadius - m_params.targetRadius);
        result.confidence = (std::max)(0.0, 1.0 - (radiusDiff / maxDiff));
      }
      else {
        result.found = false;
        result.confidence = 0.0;
      }

      return result;

    }
    catch (HException& ex) {
      setError("HALCON detection error: " + std::string(ex.ErrorMessage().TextA()));
      result.errorMessage = m_lastError;
      return result;
    }
  }

  bool validateParameters() const {
    if (m_params.roiSize <= 0) return false;
    if (m_params.thresholdLow < 0 || m_params.thresholdHigh > 255) return false;
    if (m_params.thresholdLow > m_params.thresholdHigh) return false;
    if (m_params.minArea <= 0 || m_params.maxArea <= m_params.minArea) return false;
    if (m_params.minCircularity < 0.0f || m_params.maxCircularity > 1.0f) return false;
    if (m_params.minCircularity > m_params.maxCircularity) return false;
    if (m_params.minRadius <= 0.0f || m_params.maxRadius <= m_params.minRadius) return false;
    if (m_params.targetRadius < m_params.minRadius || m_params.targetRadius > m_params.maxRadius) return false;

    return true;
  }

  void clampParameters() {
    // Clamp values to reasonable ranges
    m_params.roiSize = (std::max)(10, (std::min)(1000, m_params.roiSize));
    m_params.thresholdLow = (std::max)(0, (std::min)(255, m_params.thresholdLow));
    m_params.thresholdHigh = (std::max)(0, (std::min)(255, m_params.thresholdHigh));

    // Ensure low <= high
    if (m_params.thresholdLow > m_params.thresholdHigh) {
      std::swap(m_params.thresholdLow, m_params.thresholdHigh);
    }

    m_params.minArea = (std::max)(1, m_params.minArea);
    m_params.maxArea = (std::max)(m_params.minArea, m_params.maxArea);

    m_params.minCircularity = (std::max)(0.0f, (std::min)(1.0f, m_params.minCircularity));
    m_params.maxCircularity = (std::max)(0.0f, (std::min)(1.0f, m_params.maxCircularity));

    // Ensure min <= max
    if (m_params.minCircularity > m_params.maxCircularity) {
      std::swap(m_params.minCircularity, m_params.maxCircularity);
    }

    m_params.minRadius = (std::max)(1.0f, m_params.minRadius);
    m_params.maxRadius = (std::max)(m_params.minRadius, m_params.maxRadius);

    // Clamp target radius to be within min/max
    m_params.targetRadius = (std::max)(m_params.minRadius,
      (std::min)(m_params.maxRadius, m_params.targetRadius));

    // Clamp median kernel size to valid values
    std::vector<int> validKernels = { 3, 5, 7, 9 };
    auto it = std::lower_bound(validKernels.begin(), validKernels.end(), m_params.medianKernelSize);
    if (it == validKernels.end()) {
      m_params.medianKernelSize = 9;
    }
    else {
      m_params.medianKernelSize = *it;
    }
  }

  void setError(const std::string& error) {
    m_lastError = error;
    std::cout << "[VisionCircleDetection] Error: " << error << std::endl;
  }

  void clearError() {
    m_lastError = "";
  }

  void parametersToJson(nlohmann::json& j) const {
    // ROI Parameters
    j["roi"]["size"] = m_params.roiSize;
    j["roi"]["offsetX"] = m_params.roiOffsetX;
    j["roi"]["offsetY"] = m_params.roiOffsetY;

    // Threshold Parameters
    j["threshold"]["low"] = m_params.thresholdLow;
    j["threshold"]["high"] = m_params.thresholdHigh;
    j["threshold"]["invertImage"] = m_params.invertImage;

    // Filter Parameters
    j["filter"]["minArea"] = m_params.minArea;
    j["filter"]["maxArea"] = m_params.maxArea;
    j["filter"]["minCircularity"] = m_params.minCircularity;
    j["filter"]["maxCircularity"] = m_params.maxCircularity;
    j["filter"]["minRadius"] = m_params.minRadius;
    j["filter"]["maxRadius"] = m_params.maxRadius;
    j["filter"]["targetRadius"] = m_params.targetRadius;

    // Advanced Parameters
    j["advanced"]["useCompactnessFilter"] = m_params.useCompactnessFilter;
    j["advanced"]["minCompactness"] = m_params.minCompactness;
    j["advanced"]["maxCompactness"] = m_params.maxCompactness;
    j["advanced"]["useNoiseReduction"] = m_params.useNoiseReduction;
    j["advanced"]["medianKernelSize"] = m_params.medianKernelSize;
    j["advanced"]["enableFallback"] = m_params.enableFallback;
    j["advanced"]["fallbackMinRadius"] = m_params.fallbackMinRadius;
    j["advanced"]["fallbackMaxRadius"] = m_params.fallbackMaxRadius;

    // Metadata
    j["metadata"]["version"] = "1.0";
    j["metadata"]["description"] = "VisionCircleDetection parameters";
  }

  void parametersFromJson(const nlohmann::json& j) {
    try {
      // ROI Parameters
      if (j.contains("roi")) {
        m_params.roiSize = j["roi"].value("size", m_params.roiSize);
        m_params.roiOffsetX = j["roi"].value("offsetX", m_params.roiOffsetX);
        m_params.roiOffsetY = j["roi"].value("offsetY", m_params.roiOffsetY);
      }

      // Threshold Parameters
      if (j.contains("threshold")) {
        m_params.thresholdLow = j["threshold"].value("low", m_params.thresholdLow);
        m_params.thresholdHigh = j["threshold"].value("high", m_params.thresholdHigh);
        m_params.invertImage = j["threshold"].value("invertImage", m_params.invertImage);
      }

      // Filter Parameters
      if (j.contains("filter")) {
        m_params.minArea = j["filter"].value("minArea", m_params.minArea);
        m_params.maxArea = j["filter"].value("maxArea", m_params.maxArea);
        m_params.minCircularity = j["filter"].value("minCircularity", m_params.minCircularity);
        m_params.maxCircularity = j["filter"].value("maxCircularity", m_params.maxCircularity);
        m_params.minRadius = j["filter"].value("minRadius", m_params.minRadius);
        m_params.maxRadius = j["filter"].value("maxRadius", m_params.maxRadius);
        m_params.targetRadius = j["filter"].value("targetRadius", m_params.targetRadius);
      }

      // Advanced Parameters
      if (j.contains("advanced")) {
        m_params.useCompactnessFilter = j["advanced"].value("useCompactnessFilter", m_params.useCompactnessFilter);
        m_params.minCompactness = j["advanced"].value("minCompactness", m_params.minCompactness);
        m_params.maxCompactness = j["advanced"].value("maxCompactness", m_params.maxCompactness);
        m_params.useNoiseReduction = j["advanced"].value("useNoiseReduction", m_params.useNoiseReduction);
        m_params.medianKernelSize = j["advanced"].value("medianKernelSize", m_params.medianKernelSize);
        m_params.enableFallback = j["advanced"].value("enableFallback", m_params.enableFallback);
        m_params.fallbackMinRadius = j["advanced"].value("fallbackMinRadius", m_params.fallbackMinRadius);
        m_params.fallbackMaxRadius = j["advanced"].value("fallbackMaxRadius", m_params.fallbackMaxRadius);
      }
    }
    catch (const std::exception& e) {
      setError("Error parsing JSON parameters: " + std::string(e.what()));
    }
  }

  // NEW: Get parameters as JSON object
  nlohmann::json getParametersAsJson() const {
    nlohmann::json j;
    parametersToJson(j);
    return j;
  }

  // NEW: Load parameters from JSON object
  bool loadParametersFromJson(const nlohmann::json& jsonParams) {
    try {
      parametersFromJson(jsonParams);
      clampParameters(); // Ensure parameters are within valid ranges
      clearError();
      std::cout << "[VisionCircleDetection] Parameters loaded from JSON object" << std::endl;
      return true;
    }
    catch (const std::exception& e) {
      setError("Error loading parameters from JSON: " + std::string(e.what()));
      return false;
    }
  }
};

// ============================================================================
// PUBLIC INTERFACE IMPLEMENTATION (Delegates to PIMPL)
// ============================================================================

VisionCircleDetection::VisionCircleDetection()
  : m_impl(std::make_unique<Implementation>()) {
}

VisionCircleDetection::~VisionCircleDetection() = default;

// Move constructor and assignment
VisionCircleDetection::VisionCircleDetection(VisionCircleDetection&&) noexcept = default;
VisionCircleDetection& VisionCircleDetection::operator=(VisionCircleDetection&&) noexcept = default;

VisionCircleDetection::Result VisionCircleDetection::DetectFromFile(const std::string& imagePath) {
  return m_impl->detectFromFile(imagePath);
}

VisionCircleDetection::Result VisionCircleDetection::DetectFromBuffer(
  const uint8_t* imageBuffer, int width, int height, int channels) {
  return m_impl->detectFromBuffer(imageBuffer, width, height, channels);
}

bool VisionCircleDetection::LoadParameters(const std::string& jsonPath) {
  try {
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
      m_impl->setError("Cannot open parameter file: " + jsonPath);
      return false;
    }

    nlohmann::json j;
    file >> j;
    file.close();

    m_impl->parametersFromJson(j);
    m_impl->clampParameters(); // Ensure parameters are within valid ranges

    m_impl->clearError();
    std::cout << "[VisionCircleDetection] Parameters loaded from: " << jsonPath << std::endl;
    return true;
  }
  catch (const std::exception& e) {
    m_impl->setError("Error loading parameters: " + std::string(e.what()));
    return false;
  }
}

bool VisionCircleDetection::SaveParameters(const std::string& jsonPath) const {
  try {
    nlohmann::json j;
    m_impl->parametersToJson(j);

    std::ofstream file(jsonPath);
    if (!file.is_open()) {
      return false;
    }

    file << j.dump(4);
    file.close();

    std::cout << "[VisionCircleDetection] Parameters saved to: " << jsonPath << std::endl;
    return true;
  }
  catch (const std::exception& e) {
    std::cout << "[VisionCircleDetection] Error saving parameters: " << e.what() << std::endl;
    return false;
  }
}

// NEW: Load parameters from JSON object directly
bool VisionCircleDetection::LoadParametersFromJson(const nlohmann::json& jsonParams) {
  return m_impl->loadParametersFromJson(jsonParams);
}

// NEW: Get current parameters as JSON object
nlohmann::json VisionCircleDetection::GetParametersAsJson() const {
  return m_impl->getParametersAsJson();
}

void VisionCircleDetection::SetParameters(const Parameters& params) {
  m_impl->m_params = params;
  m_impl->clampParameters();
  m_impl->clearError();
}

const VisionCircleDetection::Parameters& VisionCircleDetection::GetParameters() const {
  return m_impl->m_params;
}

bool VisionCircleDetection::CreateDefaultParameterFile(const std::string& jsonPath) {
  try {
    VisionCircleDetection detector;
    detector.SetParameters(GetDefaultParameters());
    return detector.SaveParameters(jsonPath);
  }
  catch (const std::exception& e) {
    std::cout << "[VisionCircleDetection] Error creating default parameter file: " << e.what() << std::endl;
    return false;
  }
}

VisionCircleDetection::Parameters VisionCircleDetection::GetDefaultParameters() {
  Parameters defaults;
  // All defaults are already set in the struct definition
  return defaults;
}

double VisionCircleDetection::GetLastProcessingTime() const {
  return m_impl->m_lastProcessingTime;
}

std::string VisionCircleDetection::GetLastError() const {
  return m_impl->m_lastError;
}

bool VisionCircleDetection::IsHalconAvailable() const {
  return m_impl->m_halconInitialized;
}