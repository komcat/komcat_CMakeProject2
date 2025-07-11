#include "CircleDetectionUI.h"
#include <chrono>


// Implementation in CircleDetectionUI.cpp:

#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <ctime>


#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <GL/gl.h>
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#else
#include <OpenGL/gl.h>
#endif

CircleDetectionUI::CircleDetectionUI()
  : m_showWindow(true),
  m_imageLoaded(false),
  m_detectionRan(false),
  m_circleFound(false),
  m_centerX(0.0),
  m_centerY(0.0),
  m_radius(0.0),
  m_imageTexture(0),
  m_imageWidth(0),
  m_imageHeight(0),
  m_textureLoaded(false),
  m_lastTextureWidth(0),
  m_lastTextureHeight(0),
  m_parametersChanged(false),
  m_parameterChangeTimer(0.0f),
  m_isDetecting(false),
  m_lastDetectionTime(0.0f),
  m_detectionCount(0)
{
  // Initialize HALCON
  try {
    SetHcppInterfaceStringEncodingIsUtf8(false);
    std::cout << "[CircleDetectionUI] Initialized successfully" << std::endl;
  }
  catch (HException& ex) {
    std::cout << "[CircleDetectionUI] HALCON init error: " << ex.ErrorMessage().TextA() << std::endl;
  }
}

CircleDetectionUI::~CircleDetectionUI()
{
  cleanupTexture();
}

void CircleDetectionUI::RenderUI()
{
  if (!m_showWindow) return;
  // Add button to main UI to open profile manager
// In RenderUI() method, add this button:
  if (ImGui::Button("Profile Manager", ImVec2(120, 30))) {
    ToggleProfileManager();
  }

  // Render the profile manager
  RenderProfileManagerUI();
  // Update auto-detection timer
  updateAutoDetection();

  // Set larger window size to accommodate parameters and image
  ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);

  if (!ImGui::Begin("Circle Detection with Real-time Parameters", &m_showWindow, ImGuiWindowFlags_NoCollapse))
  {
    ImGui::End();
    return;
  }

  // Create main layout with splitter
  ImGui::Columns(3, "main_columns", true);
  ImGui::SetColumnWidth(0, 300);  // Parameters column
  ImGui::SetColumnWidth(1, 400);  // Results column  
  ImGui::SetColumnWidth(2, 500);  // Image column

  // === LEFT COLUMN: PARAMETERS ===
  ImGui::Text("Detection Parameters");
  ImGui::Separator();
  renderParameterControls();

  // === MIDDLE COLUMN: FILE LOADING & RESULTS ===
  ImGui::NextColumn();
  ImGui::Text("Control & Results");
  ImGui::Separator();

  // File selection section
  if (ImGui::Button("Load Image File", ImVec2(150, 30)))
  {
    std::string newPath = openFileDialog();
    if (!newPath.empty()) {
      m_selectedImagePath = newPath;
      m_imageLoaded = true;
      m_detectionRan = false;
      m_circleFound = false;

      // Load image as texture for display
      loadImageAsTexture(newPath);

      // Auto-detect if enabled
      if (m_params.autoDetect) {
        executeDetection();
      }

      std::cout << "[CircleDetectionUI] Image loaded: " << newPath << std::endl;
    }
  }

  // Show loaded file status
  if (m_imageLoaded) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Loaded");

    // Show filename (truncated)
    std::string displayPath = m_selectedImagePath;
    if (displayPath.length() > 40) {
      displayPath = "..." + displayPath.substr(displayPath.length() - 37);
    }
    ImGui::Text("File: %s", displayPath.c_str());
  }

  ImGui::Spacing();

  // Manual detection button
  ImGui::BeginDisabled(!m_imageLoaded || m_isDetecting);
  if (ImGui::Button("Manual Detection", ImVec2(150, 30)))
  {
    executeDetection();
  }
  ImGui::EndDisabled();

  // Detection status
  if (m_isDetecting) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Detecting...");
  }

  // Performance info
  if (m_detectionCount > 0) {
    ImGui::Text("Detection #%d (%.1fms)", m_detectionCount, m_lastDetectionTime * 1000.0f);
  }

  ImGui::Spacing();
  ImGui::Separator();

  // Detection Results
  ImGui::Text("Detection Results:");

  if (m_detectionRan) {
    if (m_circleFound) {
      // Success - show results in green
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Circle Detected");

      // Results table
      if (ImGui::BeginTable("Results", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
      {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Center X");
        ImGui::TableNextColumn();
        ImGui::Text("%.1f px", m_centerX);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Center Y");
        ImGui::TableNextColumn();
        ImGui::Text("%.1f px", m_centerY);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Radius");
        ImGui::TableNextColumn();
        ImGui::Text("%.1f px", m_radius);

        ImGui::EndTable();
      }

      ImGui::Spacing();

      // Action button
      if (ImGui::Button("Send to Robot", ImVec2(120, 25))) {
        std::cout << "[CircleDetectionUI] Sending coordinates to robot: ("
          << m_centerX << ", " << m_centerY << ")" << std::endl;
        // TODO: Add your robot communication code here
      }
    }
    else {
      // Failed - show in red
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "✗ No Circle Found");
      ImGui::Text("Try adjusting parameters.");
    }
  }
  else {
    ImGui::Text("No detection performed yet.");
  }

  // === RIGHT COLUMN: IMAGE DISPLAY ===
  ImGui::NextColumn();
  ImGui::Text("Image Preview:");

  if (m_textureLoaded && m_imageTexture != 0) {
    renderImageWithCrosshair();
  }
  else if (m_imageLoaded) {
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Image Display Not Available");
    if (m_circleFound && m_detectionRan) {
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "🎯 Detection Results:");
      ImGui::Text("Center: (%.1f, %.1f)", m_centerX, m_centerY);
      ImGui::Text("Radius: %.1f px", m_radius);
    }
  }
  else {
    ImGui::Text("No image loaded");
  }

  ImGui::Columns(1); // Reset columns
  ImGui::End();
}

void CircleDetectionUI::renderParameterControls()
{
  // Auto-detection toggle
  if (ImGui::Checkbox("Auto-detect on change", &m_params.autoDetect)) {
    onParameterChanged();
  }

  if (m_params.autoDetect) {
    ImGui::SameLine();
    if (ImGui::SliderFloat("Delay", &m_params.detectionDelay, 0.1f, 2.0f, "%.1fs")) {
      // No need to trigger detection for delay change
    }
  }

  ImGui::Separator();

  // Use collapsing headers for organization
  if (ImGui::CollapsingHeader("ROI Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
    renderROIControls();
  }

  if (ImGui::CollapsingHeader("Threshold Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
    renderThresholdControls();
  }

  if (ImGui::CollapsingHeader("Filter Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
    renderFilterControls();
  }

  if (ImGui::CollapsingHeader("Advanced Settings")) {
    renderAdvancedControls();
  }
}

void CircleDetectionUI::renderROIControls()
{
  if (ImGui::SliderInt("ROI Size", &m_params.roiSize, 50, 500, "%d px")) {
    onParameterChanged();
  }

  if (ImGui::SliderInt("ROI Offset X", &m_params.roiOffsetX, -200, 200, "%d px")) {
    onParameterChanged();
  }

  if (ImGui::SliderInt("ROI Offset Y", &m_params.roiOffsetY, -200, 200, "%d px")) {
    onParameterChanged();
  }
}

void CircleDetectionUI::renderThresholdControls()
{
  if (ImGui::SliderInt("Low Threshold", &m_params.thresholdLow, 0, 255)) {
    // Ensure low <= high
    if (m_params.thresholdLow > m_params.thresholdHigh) {
      m_params.thresholdHigh = m_params.thresholdLow;
    }
    onParameterChanged();
  }

  if (ImGui::SliderInt("High Threshold", &m_params.thresholdHigh, 0, 255)) {
    // Ensure low <= high
    if (m_params.thresholdHigh < m_params.thresholdLow) {
      m_params.thresholdLow = m_params.thresholdHigh;
    }
    onParameterChanged();
  }

  if (ImGui::Checkbox("Invert Image", &m_params.invertImage)) {
    onParameterChanged();
  }
}

void CircleDetectionUI::renderFilterControls()
{
  if (ImGui::SliderInt("Min Area", &m_params.minArea, 10, 10000, "%d px²")) {
    if (m_params.minArea > m_params.maxArea) {
      m_params.maxArea = m_params.minArea;
    }
    onParameterChanged();
  }

  if (ImGui::SliderInt("Max Area", &m_params.maxArea, 100, 100000, "%d px²")) {
    if (m_params.maxArea < m_params.minArea) {
      m_params.minArea = m_params.maxArea;
    }
    onParameterChanged();
  }

  if (ImGui::SliderFloat("Min Circularity", &m_params.minCircularity, 0.0f, 1.0f, "%.2f")) {
    if (m_params.minCircularity > m_params.maxCircularity) {
      m_params.maxCircularity = m_params.minCircularity;
    }
    onParameterChanged();
  }

  if (ImGui::SliderFloat("Max Circularity", &m_params.maxCircularity, 0.0f, 1.0f, "%.2f")) {
    if (m_params.maxCircularity < m_params.minCircularity) {
      m_params.minCircularity = m_params.maxCircularity;
    }
    onParameterChanged();
  }

  if (ImGui::SliderFloat("Min Radius", &m_params.minRadius, 1.0f, 200.0f, "%.1f px")) {
    if (m_params.minRadius > m_params.maxRadius) {
      m_params.maxRadius = m_params.minRadius;
    }
    onParameterChanged();
  }

  if (ImGui::SliderFloat("Max Radius", &m_params.maxRadius, 1.0f, 200.0f, "%.1f px")) {
    if (m_params.maxRadius < m_params.minRadius) {
      m_params.minRadius = m_params.maxRadius;
    }
    onParameterChanged();
  }

  if (ImGui::SliderFloat("Target Radius", &m_params.targetRadius, m_params.minRadius, m_params.maxRadius, "%.1f px")) {
    onParameterChanged();
  }
}

void CircleDetectionUI::renderAdvancedControls()
{
  if (ImGui::Checkbox("Use Compactness Filter", &m_params.useCompactnessFilter)) {
    onParameterChanged();
  }

  if (m_params.useCompactnessFilter) {
    if (ImGui::SliderFloat("Min Compactness", &m_params.minCompactness, 1.0f, 10.0f, "%.1f")) {
      if (m_params.minCompactness > m_params.maxCompactness) {
        m_params.maxCompactness = m_params.minCompactness;
      }
      onParameterChanged();
    }

    if (ImGui::SliderFloat("Max Compactness", &m_params.maxCompactness, 1.0f, 10.0f, "%.1f")) {
      if (m_params.maxCompactness < m_params.minCompactness) {
        m_params.minCompactness = m_params.maxCompactness;
      }
      onParameterChanged();
    }
  }

  if (ImGui::Checkbox("Use Noise Reduction", &m_params.useNoiseReduction)) {
    onParameterChanged();
  }

  if (m_params.useNoiseReduction) {
    const char* kernelSizes[] = { "3x3", "5x5", "7x7", "9x9" };
    int kernelIndex = (m_params.medianKernelSize - 3) / 2;
    if (ImGui::Combo("Kernel Size", &kernelIndex, kernelSizes, 4)) {
      m_params.medianKernelSize = 3 + kernelIndex * 2;
      onParameterChanged();
    }
  }

  if (ImGui::Checkbox("Enable Fallback", &m_params.enableFallback)) {
    onParameterChanged();
  }

  if (m_params.enableFallback) {
    if (ImGui::SliderFloat("Fallback Min Radius", &m_params.fallbackMinRadius, 1.0f, 300.0f, "%.1f px")) {
      if (m_params.fallbackMinRadius > m_params.fallbackMaxRadius) {
        m_params.fallbackMaxRadius = m_params.fallbackMinRadius;
      }
      onParameterChanged();
    }

    if (ImGui::SliderFloat("Fallback Max Radius", &m_params.fallbackMaxRadius, 1.0f, 300.0f, "%.1f px")) {
      if (m_params.fallbackMaxRadius < m_params.fallbackMinRadius) {
        m_params.fallbackMinRadius = m_params.fallbackMaxRadius;
      }
      onParameterChanged();
    }
  }
}

void CircleDetectionUI::onParameterChanged()
{
  m_parametersChanged = true;
  m_parameterChangeTimer = 0.0f;
}

void CircleDetectionUI::updateAutoDetection()
{
  if (m_parametersChanged && m_params.autoDetect && m_imageLoaded && !m_isDetecting) {
    m_parameterChangeTimer += ImGui::GetIO().DeltaTime;

    if (m_parameterChangeTimer >= m_params.detectionDelay) {
      executeDetection();
      m_parametersChanged = false;
    }
  }
}

void CircleDetectionUI::executeDetection()
{
  if (!m_imageLoaded || m_isDetecting) return;

  m_isDetecting = true;
  auto startTime = std::chrono::high_resolution_clock::now();

  // Perform detection
  double centerX, centerY, radius;
  m_circleFound = detectCircleCenter(m_selectedImagePath, centerX, centerY, radius);

  if (m_circleFound) {
    m_centerX = centerX;
    m_centerY = centerY;
    m_radius = radius;

    // Call callback if set
    if (m_detectionCallback) {
      m_detectionCallback(centerX, centerY, radius);
    }
  }

  m_detectionRan = true;
  m_detectionCount++;

  auto endTime = std::chrono::high_resolution_clock::now();
  m_lastDetectionTime = std::chrono::duration<float>(endTime - startTime).count();

  m_isDetecting = false;
}

// Modified detection function using parameters
bool CircleDetectionUI::detectCircleCenter(const std::string& imagePath, double& centerX, double& centerY, double& radius)
{
  try {
    // Local iconic variables
    HObject ho_Image1, ho_ImageProcessed, ho_NewRectangularDomain;
    HObject ho_ImageWithNewDomain, ho_RegionThreshold, ho_SeparateRegions;
    HObject ho_SelectedRegions, ho_FilteredRegions, ho_LargestRegion;

    // Local control variables
    HTuple hv_imageWidth, hv_imageHeight, hv_circle_col;
    HTuple hv_circle_row, hv_domainColumn1, hv_domainColumn2;
    HTuple hv_domainRow1, hv_domainRow2;

    // Read image
    ReadImage(&ho_Image1, imagePath.c_str());
    GetImageSize(ho_Image1, &hv_imageWidth, &hv_imageHeight);

    // Apply noise reduction if enabled
    if (m_params.useNoiseReduction) {
      MedianImage(ho_Image1, &ho_ImageProcessed, "circle", m_params.medianKernelSize, "mirrored");
    }
    else {
      ho_ImageProcessed = ho_Image1;
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
    HTuple hv_MinRadiusDiff;
    hv_MinRadiusDiff = 999999;
    bool circleFound = false;

    CountObj(ho_FilteredRegions, &hv_Number);

    // Check each region for circles with desired radius
    for (int i = 1; i <= hv_Number[0].I(); i++) {
      try {
        HObject ho_CurrentRegion;
        HTuple hv_CurrentRow, hv_CurrentCol, hv_CurrentRadius;

        SelectObj(ho_FilteredRegions, &ho_CurrentRegion, i);
        SmallestCircle(ho_CurrentRegion, &hv_CurrentRow, &hv_CurrentCol, &hv_CurrentRadius);

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
              circleFound = true;
            }
          }
        }
      }
      catch (HException& ex) {
        continue;
      }
    }

    // Fallback approach if enabled and no circle found
    if (!circleFound && m_params.enableFallback) {
      try {
        SelectShapeStd(ho_FilteredRegions, &ho_LargestRegion, "max_area", 70);
        HTuple hv_CircleRow2, hv_CircleCol2, hv_Radius2;
        SmallestCircle(ho_LargestRegion, &hv_CircleRow2, &hv_CircleCol2, &hv_Radius2);

        if (hv_CircleRow2.Length() > 0 && hv_CircleCol2.Length() > 0 && hv_Radius2.Length() > 0) {
          double fallbackRadius = hv_Radius2[0].D();

          if (fallbackRadius >= m_params.fallbackMinRadius && fallbackRadius <= m_params.fallbackMaxRadius) {
            hv_BestRadius = fallbackRadius;
            hv_BestRow = hv_CircleRow2[0].D();
            hv_BestCol = hv_CircleCol2[0].D();
            circleFound = true;
          }
        }
      }
      catch (HException& ex) {
        // Fallback failed
      }
    }

    // Return results if found
    if (circleFound) {
      centerX = hv_BestCol[0].D();
      centerY = hv_BestRow[0].D();
      radius = hv_BestRadius[0].D();
      return true;
    }

    return false;

  }
  catch (HException& ex) {
    std::cout << "[CircleDetectionUI] HALCON Error: " << ex.ErrorMessage().TextA() << std::endl;
    return false;
  }
}

// Keep existing methods unchanged
#ifdef _WIN32
std::string CircleDetectionUI::openFileDialog()
{
  OPENFILENAMEA ofn;
  char szFile[260] = { 0 };

  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof(szFile);
  ofn.lpstrFilter = "Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.tiff\0PNG Files\0*.png\0JPG Files\0*.jpg\0All Files\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrFileTitle = NULL;
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = NULL;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  if (GetOpenFileNameA(&ofn)) {
    return std::string(szFile);
  }
  return "";
}
#else
std::string CircleDetectionUI::openFileDialog()
{
  std::string path;
  std::cout << "Enter image path: ";
  std::getline(std::cin, path);
  return path;
}
#endif

bool CircleDetectionUI::GetLastResults(double& centerX, double& centerY, double& radius) const
{
  if (m_circleFound) {
    centerX = m_centerX;
    centerY = m_centerY;
    radius = m_radius;
    return true;
  }
  return false;
}

void CircleDetectionUI::SetDetectionCallback(std::function<void(double, double, double)> callback)
{
  m_detectionCallback = callback;
}

bool CircleDetectionUI::loadImageAsTexture(const std::string& imagePath)
{
  std::cout << "[CircleDetectionUI] Loading image: " << imagePath << std::endl;

  cleanupTexture();

  try {
    HObject ho_Image;
    ReadImage(&ho_Image, imagePath.c_str());

    HTuple width, height;
    GetImageSize(ho_Image, &width, &height);
    m_imageWidth = width[0].I();
    m_imageHeight = height[0].I();

    std::cout << "[CircleDetectionUI] Image dimensions: " << m_imageWidth << "x" << m_imageHeight << std::endl;

    HObject ho_ImageRGB;
    HTuple channels;
    CountChannels(ho_Image, &channels);

    if (channels[0].I() == 1) {
      Compose3(ho_Image, ho_Image, ho_Image, &ho_ImageRGB);
    }
    else if (channels[0].I() == 3) {
      ho_ImageRGB = ho_Image;
    }
    else {
      Rgb1ToGray(ho_Image, &ho_Image);
      Compose3(ho_Image, ho_Image, ho_Image, &ho_ImageRGB);
    }

    UpdateTextureFromHalconImage(ho_ImageRGB);
    return true;

  }
  catch (HException& ex) {
    std::cout << "[CircleDetectionUI] HALCON Exception: " << ex.ErrorMessage().TextA() << std::endl;
    m_textureLoaded = false;
    return false;
  }
}

void CircleDetectionUI::renderImageWithCrosshair()
{
  if (!m_textureLoaded || m_imageTexture == 0) return;

  // Calculate display size maintaining aspect ratio
  float maxDisplayWidth = 450.0f;
  float maxDisplayHeight = 400.0f;

  float aspectRatio = (float)m_imageWidth / (float)m_imageHeight;
  float displayWidth, displayHeight;

  if (maxDisplayWidth / aspectRatio <= maxDisplayHeight) {
    displayWidth = maxDisplayWidth;
    displayHeight = maxDisplayWidth / aspectRatio;
  }
  else {
    displayWidth = maxDisplayHeight * aspectRatio;
    displayHeight = maxDisplayHeight;
  }

  float scaleX = displayWidth / m_imageWidth;
  float scaleY = displayHeight / m_imageHeight;

  ImVec2 imagePos = ImGui::GetCursorScreenPos();

  // Display the image
  ImGui::Image((ImTextureID)(intptr_t)m_imageTexture,
    ImVec2(displayWidth, displayHeight),
    ImVec2(0, 0), ImVec2(1, 1));

  // Draw ROI rectangle overlay
  ImDrawList* drawList = ImGui::GetWindowDrawList();

  // Calculate ROI bounds in display coordinates
  float roiCenterX = imagePos.x + ((m_imageWidth / 2.0f + m_params.roiOffsetX) * scaleX);
  float roiCenterY = imagePos.y + ((m_imageHeight / 2.0f + m_params.roiOffsetY) * scaleY);
  float roiSize = m_params.roiSize * scaleX;

  ImVec2 roiTopLeft(roiCenterX - roiSize, roiCenterY - roiSize);
  ImVec2 roiBottomRight(roiCenterX + roiSize, roiCenterY + roiSize);

  // Draw ROI rectangle
  drawList->AddRect(roiTopLeft, roiBottomRight, IM_COL32(255, 255, 0, 128), 0.0f, 0, 2.0f);

  // Draw crosshair overlay if circle was detected
  if (m_circleFound && m_detectionRan) {
    float crosshairX = imagePos.x + (m_centerX * scaleX);
    float crosshairY = imagePos.y + (m_centerY * scaleY);
    float crosshairRadius = m_radius * scaleX;

    // Colors
    ImU32 crosshairColor = IM_COL32(0, 255, 0, 255);    // Green crosshair
    ImU32 circleColor = IM_COL32(255, 0, 0, 255);       // Red circle outline
    ImU32 centerColor = IM_COL32(255, 255, 0, 255);     // Yellow center dot

    // Draw circle outline
    drawList->AddCircle(ImVec2(crosshairX, crosshairY), crosshairRadius, circleColor, 32, 1.0f);

    // Draw crosshair lines
    float crossSize = 10.0f;
    drawList->AddLine(
      ImVec2(crosshairX - crossSize, crosshairY),
      ImVec2(crosshairX + crossSize, crosshairY),
      crosshairColor, 1.0f
    );
    drawList->AddLine(
      ImVec2(crosshairX, crosshairY - crossSize),
      ImVec2(crosshairX, crosshairY + crossSize),
      crosshairColor, 1.0f
    );

    // Center dot
    //drawList->AddCircleFilled(ImVec2(crosshairX, crosshairY), 1.0f, centerColor);

    // Coordinate text
    std::string coordText = "(" + std::to_string((int)m_centerX) + ", " + std::to_string((int)m_centerY) + ")";
    ImVec2 textPos(crosshairX + 30, crosshairY - 15);
    ImVec2 textSize = ImGui::CalcTextSize(coordText.c_str());

    drawList->AddRectFilled(
      ImVec2(textPos.x - 2, textPos.y - 2),
      ImVec2(textPos.x + textSize.x + 2, textPos.y + textSize.y + 2),
      IM_COL32(0, 0, 0, 180)
    );
    drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), coordText.c_str());
  }

  // Show image info
  ImGui::Text("Size: %dx%d (%.0fx%.0f)", m_imageWidth, m_imageHeight, displayWidth, displayHeight);

  if (m_circleFound) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Circle marked with crosshair");
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "□ Yellow box shows ROI");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "□ Yellow box shows search ROI");
  }
}

void CircleDetectionUI::cleanupTexture()
{
  if (m_imageTexture != 0) {
    glDeleteTextures(1, &m_imageTexture);
    m_imageTexture = 0;
    m_textureLoaded = false;
    m_lastTextureWidth = 0;
    m_lastTextureHeight = 0;
  }
}

void CircleDetectionUI::UpdateTextureFromHalconImage(const HObject& halconImage)
{
  try {
    HTuple pointerR, pointerG, pointerB, type, widthTuple, heightTuple;
    GetImagePointer3(halconImage, &pointerR, &pointerG, &pointerB, &type, &widthTuple, &heightTuple);

    if (pointerR.Length() > 0 && pointerG.Length() > 0 && pointerB.Length() > 0) {
      unsigned char* redPtr = (unsigned char*)pointerR[0].L();
      unsigned char* greenPtr = (unsigned char*)pointerG[0].L();
      unsigned char* bluePtr = (unsigned char*)pointerB[0].L();

      if (!redPtr || !greenPtr || !bluePtr) {
        throw HException("Invalid channel pointers");
      }

      uint32_t width = m_imageWidth;
      uint32_t height = m_imageHeight;
      std::vector<uint8_t> rgbBuffer(width * height * 3);

      // Convert from HALCON's planar format to interleaved RGB
      for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
          uint32_t srcIndex = y * width + x;
          uint32_t dstIndex = (y * width + x) * 3;
          rgbBuffer[dstIndex + 0] = redPtr[srcIndex];   // R
          rgbBuffer[dstIndex + 1] = greenPtr[srcIndex]; // G
          rgbBuffer[dstIndex + 2] = bluePtr[srcIndex];  // B
        }
      }

      UpdateTextureFromBuffer(rgbBuffer.data(), width, height);
    }
    else {
      throw HException("Failed to get valid image pointers from HALCON");
    }
  }
  catch (HException& ex) {
    std::cout << "[CircleDetectionUI] Error processing HALCON image: " << ex.ErrorMessage().TextA() << std::endl;

    // Create test pattern for debugging
    std::vector<uint8_t> testPattern(m_imageWidth * m_imageHeight * 3);
    for (int y = 0; y < m_imageHeight; y++) {
      for (int x = 0; x < m_imageWidth; x++) {
        int index = (y * m_imageWidth + x) * 3;
        bool checker = ((x / 50) + (y / 50)) % 2;
        testPattern[index + 0] = checker ? 255 : 0;    // R
        testPattern[index + 1] = checker ? 255 : 0;    // G  
        testPattern[index + 2] = checker ? 255 : 0;    // B
      }
    }
    UpdateTextureFromBuffer(testPattern.data(), m_imageWidth, m_imageHeight);
  }
}

void CircleDetectionUI::UpdateTextureFromBuffer(const uint8_t* pImageBuffer, uint32_t width, uint32_t height)
{
  if (!pImageBuffer || width == 0 || height == 0) return;

  if (m_imageTexture == 0) {
    glGenTextures(1, &m_imageTexture);
  }

  glBindTexture(GL_TEXTURE_2D, m_imageTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pImageBuffer);

  m_lastTextureWidth = width;
  m_lastTextureHeight = height;
  m_textureLoaded = true;

  glBindTexture(GL_TEXTURE_2D, 0);
}



bool CircleDetectionUI::SaveProfile(const std::string& profileName, const std::string& description)
{
  if (profileName.empty()) {
    std::cout << "[CircleDetectionUI] Profile name cannot be empty" << std::endl;
    return false;
  }

  try {
    ensureProfileDirectoryExists();

    nlohmann::json j;

    // Add metadata
    j["metadata"]["name"] = profileName;
    j["metadata"]["description"] = description;
    j["metadata"]["version"] = "1.0";
    j["metadata"]["created"] = getCurrentTimestamp();
    j["metadata"]["application"] = "CircleDetectionUI";

    // Add parameters
    parametersToJson(j["parameters"]);

    std::string filename = generateProfileFilename(profileName);
    std::ofstream file(filename);
    if (!file.is_open()) {
      std::cout << "[CircleDetectionUI] Failed to create profile file: " << filename << std::endl;
      return false;
    }

    file << j.dump(4);
    file.close();

    std::cout << "[CircleDetectionUI] Profile '" << profileName << "' saved successfully" << std::endl;
    return true;
  }
  catch (const std::exception& e) {
    std::cout << "[CircleDetectionUI] Error saving profile: " << e.what() << std::endl;
    return false;
  }
}

bool CircleDetectionUI::LoadProfile(const std::string& profileName)
{
  try {
    std::string filename = generateProfileFilename(profileName);

    if (!std::filesystem::exists(filename)) {
      std::cout << "[CircleDetectionUI] Profile not found: " << profileName << std::endl;
      return false;
    }

    std::ifstream file(filename);
    if (!file.is_open()) {
      std::cout << "[CircleDetectionUI] Failed to open profile: " << filename << std::endl;
      return false;
    }

    nlohmann::json j;
    file >> j;
    file.close();

    // Load parameters
    if (j.contains("parameters")) {
      parametersFromJson(j["parameters"]);
    }
    else {
      // Legacy format support
      parametersFromJson(j);
    }

    std::cout << "[CircleDetectionUI] Profile '" << profileName << "' loaded successfully" << std::endl;

    // Trigger auto-detection if enabled
    if (m_params.autoDetect && m_imageLoaded) {
      onParameterChanged();
    }

    return true;
  }
  catch (const std::exception& e) {
    std::cout << "[CircleDetectionUI] Error loading profile: " << e.what() << std::endl;
    return false;
  }
}

std::vector<std::string> CircleDetectionUI::GetAvailableProfiles()
{
  std::vector<std::string> profiles;

  if (!std::filesystem::exists(m_profilesPath)) {
    return profiles;
  }

  try {
    for (const auto& entry : std::filesystem::directory_iterator(m_profilesPath)) {
      if (entry.is_regular_file() && entry.path().extension() == ".json") {
        std::string filename = entry.path().stem().string();
        // Remove "circle_detection_params_" prefix
        if (stringStartsWith(filename, "circle_detection_params_")) {
          std::string profileName = filename.substr(24);
          profiles.push_back(profileName);
        }
      }
    }
  }
  catch (const std::exception& e) {
    std::cout << "[CircleDetectionUI] Error scanning profiles: " << e.what() << std::endl;
  }

  std::sort(profiles.begin(), profiles.end());
  return profiles;
}

std::vector<CircleDetectionUI::ProfileInfo> CircleDetectionUI::GetProfileDetails()
{
  std::vector<ProfileInfo> details;
  auto profiles = GetAvailableProfiles();

  for (const auto& profileName : profiles) {
    ProfileInfo info;
    info.name = profileName;
    info.filename = generateProfileFilename(profileName);
    info.isDefault = (profileName == m_defaultProfile);

    try {
      // Get file modification time
      auto ftime = std::filesystem::last_write_time(info.filename);
      auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
      auto cftime = std::chrono::system_clock::to_time_t(sctp);

      std::stringstream ss;
      ss << std::put_time(std::localtime(&cftime), "%Y-%m-%d %H:%M");
      info.lastModified = ss.str();

      // Try to load description from file
      std::ifstream file(info.filename);
      if (file.is_open()) {
        nlohmann::json j;
        file >> j;
        if (j.contains("metadata") && j["metadata"].contains("description")) {
          info.description = j["metadata"]["description"];
        }
        file.close();
      }
    }
    catch (const std::exception& e) {
      info.lastModified = "Unknown";
      info.description = "Error reading file";
    }

    details.push_back(info);
  }

  return details;
}

bool CircleDetectionUI::LoadProfileForOperation(const std::string& operationType)
{
  auto it = m_operationProfiles.find(operationType);
  if (it != m_operationProfiles.end()) {
    std::cout << "[CircleDetectionUI] Loading profile '" << it->second
      << "' for operation: " << operationType << std::endl;
    return LoadProfile(it->second);
  }

  std::cout << "[CircleDetectionUI] No profile registered for operation: " << operationType << std::endl;
  return false;
}

void CircleDetectionUI::RegisterOperationProfile(const std::string& operationType, const std::string& profileName)
{
  m_operationProfiles[operationType] = profileName;
  saveOperationMappings();
  std::cout << "[CircleDetectionUI] Registered profile '" << profileName
    << "' for operation: " << operationType << std::endl;
}

void CircleDetectionUI::RenderProfileManagerUI()
{
  if (!m_showProfileManager) return;

  ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);

  if (!ImGui::Begin("Profile Manager", &m_showProfileManager)) {
    ImGui::End();
    return;
  }

  // Create new profile section
  ImGui::Text("Create New Profile");
  ImGui::Separator();

  ImGui::InputText("Profile Name", m_newProfileName, sizeof(m_newProfileName));
  ImGui::InputTextMultiline("Description", m_newProfileDescription, sizeof(m_newProfileDescription), ImVec2(-1, 60));

  ImGui::BeginDisabled(strlen(m_newProfileName) == 0);
  if (ImGui::Button("Save Current Settings as Profile", ImVec2(200, 30))) {
    if (SaveProfile(m_newProfileName, m_newProfileDescription)) {
      memset(m_newProfileName, 0, sizeof(m_newProfileName));
      memset(m_newProfileDescription, 0, sizeof(m_newProfileDescription));
    }
  }
  ImGui::EndDisabled();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Existing Profiles");

  // Profile list with details
  auto profileDetails = GetProfileDetails();

  if (profileDetails.empty()) {
    ImGui::Text("No profiles found. Create your first profile above.");
  }
  else {
    // Table headers
    if (ImGui::BeginTable("ProfileTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
      ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 150);
      ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 120);
      ImGui::TableSetupColumn("Default", ImGuiTableColumnFlags_WidthFixed, 60);
      ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 120);
      ImGui::TableHeadersRow();

      for (size_t i = 0; i < profileDetails.size(); i++) {
        const auto& profile = profileDetails[i];
        ImGui::TableNextRow();

        // Name
        ImGui::TableNextColumn();
        if (profile.isDefault) {
          ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", profile.name.c_str());
        }
        else {
          ImGui::Text("%s", profile.name.c_str());
        }

        // Description
        ImGui::TableNextColumn();
        ImGui::Text("%s", profile.description.c_str());

        // Modified
        ImGui::TableNextColumn();
        ImGui::Text("%s", profile.lastModified.c_str());

        // Default checkbox
        ImGui::TableNextColumn();
        bool isDefault = profile.isDefault;
        if (ImGui::Checkbox(("##default" + std::to_string(i)).c_str(), &isDefault)) {
          if (isDefault) {
            SetDefaultProfile(profile.name);
          }
        }

        // Actions
        ImGui::TableNextColumn();
        if (ImGui::Button(("Load##" + std::to_string(i)).c_str(), ImVec2(50, 20))) {
          LoadProfile(profile.name);
        }
        ImGui::SameLine();
        if (ImGui::Button(("Del##" + std::to_string(i)).c_str(), ImVec2(40, 20))) {
          DeleteProfile(profile.name);
        }
      }
      ImGui::EndTable();
    }
  }

  ImGui::Spacing();
  ImGui::Separator();

  // Operation mappings section
  ImGui::Text("Operation Profile Mappings");
  ImGui::Text("Link profiles to specific machine operations:");

  // Common operation types
  const char* operations[] = {
      "WAFER_ALIGNMENT", "CHIP_DETECTION", "FIDUCIAL_SEARCH",
      "QUALITY_CHECK", "DEFECT_INSPECTION", "CALIBRATION",
      "PICK_AND_PLACE", "WIRE_BONDING", "FINAL_INSPECTION"
  };

  for (const char* op : operations) {
    ImGui::Text("%s:", op);
    ImGui::SameLine(200);

    std::string currentProfile = "None";
    auto it = m_operationProfiles.find(op);
    if (it != m_operationProfiles.end()) {
      currentProfile = it->second;
    }

    if (ImGui::BeginCombo(("##" + std::string(op)).c_str(), currentProfile.c_str())) {
      if (ImGui::Selectable("None")) {
        m_operationProfiles.erase(op);
        saveOperationMappings();
      }

      for (const auto& profile : GetAvailableProfiles()) {
        bool isSelected = (currentProfile == profile);
        if (ImGui::Selectable(profile.c_str(), isSelected)) {
          RegisterOperationProfile(op, profile);
        }
      }
      ImGui::EndCombo();
    }
  }

  ImGui::End();
}

// Helper methods implementation
void CircleDetectionUI::parametersToJson(nlohmann::json& j)
{
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

  // Real-time Parameters
  j["realtime"]["autoDetect"] = m_params.autoDetect;
  j["realtime"]["detectionDelay"] = m_params.detectionDelay;
}

void CircleDetectionUI::parametersFromJson(const nlohmann::json& j)
{
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

    // Real-time Parameters
    if (j.contains("realtime")) {
      m_params.autoDetect = j["realtime"].value("autoDetect", m_params.autoDetect);
      m_params.detectionDelay = j["realtime"].value("detectionDelay", m_params.detectionDelay);
    }
  }
  catch (const std::exception& e) {
    std::cout << "[CircleDetectionUI] Error parsing parameters: " << e.what() << std::endl;
  }
}

std::string CircleDetectionUI::generateProfileFilename(const std::string& profileName)
{
  return m_profilesPath + "circle_detection_params_" + profileName + ".json";
}

void CircleDetectionUI::ensureProfileDirectoryExists()
{
  if (!std::filesystem::exists(m_profilesPath)) {
    std::filesystem::create_directories(m_profilesPath);
  }
}

std::string CircleDetectionUI::getCurrentTimestamp()
{
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

bool CircleDetectionUI::DeleteProfile(const std::string& profileName)
{
  try {
    std::string filename = generateProfileFilename(profileName);

    if (!std::filesystem::exists(filename)) {
      std::cout << "[CircleDetectionUI] Profile not found: " << profileName << std::endl;
      return false;
    }

    std::filesystem::remove(filename);

    // If this was the default profile, clear the default
    if (m_defaultProfile == profileName) {
      m_defaultProfile = "";
    }

    // Remove from operation mappings
    for (auto it = m_operationProfiles.begin(); it != m_operationProfiles.end();) {
      if (it->second == profileName) {
        it = m_operationProfiles.erase(it);
      }
      else {
        ++it;
      }
    }
    saveOperationMappings();

    std::cout << "[CircleDetectionUI] Profile '" << profileName << "' deleted successfully" << std::endl;
    return true;
  }
  catch (const std::exception& e) {
    std::cout << "[CircleDetectionUI] Error deleting profile: " << e.what() << std::endl;
    return false;
  }
}


bool CircleDetectionUI::SetDefaultProfile(const std::string& profileName)
{
  // Verify profile exists
  std::string filename = generateProfileFilename(profileName);
  if (!std::filesystem::exists(filename)) {
    std::cout << "[CircleDetectionUI] Cannot set default - profile not found: " << profileName << std::endl;
    return false;
  }

  m_defaultProfile = profileName;

  // Save default profile setting to a config file
  try {
    nlohmann::json config;
    config["defaultProfile"] = m_defaultProfile;

    std::ofstream file(m_profilesPath + "default_profile.json");
    if (file.is_open()) {
      file << config.dump(4);
      file.close();
    }
  }
  catch (const std::exception& e) {
    std::cout << "[CircleDetectionUI] Error saving default profile setting: " << e.what() << std::endl;
  }

  std::cout << "[CircleDetectionUI] Default profile set to: " << profileName << std::endl;
  return true;
}

std::string CircleDetectionUI::GetDefaultProfile()
{
  if (!m_defaultProfile.empty()) {
    return m_defaultProfile;
  }

  // Try to load from config file
  try {
    std::string configFile = m_profilesPath + "default_profile.json";
    if (std::filesystem::exists(configFile)) {
      std::ifstream file(configFile);
      if (file.is_open()) {
        nlohmann::json config;
        file >> config;
        file.close();

        if (config.contains("defaultProfile")) {
          m_defaultProfile = config["defaultProfile"];
        }
      }
    }
  }
  catch (const std::exception& e) {
    std::cout << "[CircleDetectionUI] Error loading default profile setting: " << e.what() << std::endl;
  }

  return m_defaultProfile;
}


// Add these methods to your CircleDetectionUI.cpp file:

void CircleDetectionUI::loadOperationMappings()
{
  try {
    std::string mappingFile = m_profilesPath + "operation_mappings.json";
    if (std::filesystem::exists(mappingFile)) {
      std::ifstream file(mappingFile);
      if (file.is_open()) {
        nlohmann::json mappings;
        file >> mappings;
        file.close();

        if (mappings.contains("mappings")) {
          for (auto& [operation, profile] : mappings["mappings"].items()) {
            m_operationProfiles[operation] = profile;
          }
        }
      }
    }
  }
  catch (const std::exception& e) {
    std::cout << "[CircleDetectionUI] Error loading operation mappings: " << e.what() << std::endl;
  }
}

void CircleDetectionUI::saveOperationMappings()
{
  try {
    ensureProfileDirectoryExists();

    nlohmann::json mappings;
    mappings["mappings"] = m_operationProfiles;
    mappings["lastUpdated"] = getCurrentTimestamp();

    std::string mappingFile = m_profilesPath + "operation_mappings.json";
    std::ofstream file(mappingFile);
    if (file.is_open()) {
      file << mappings.dump(4);
      file.close();
    }
  }
  catch (const std::exception& e) {
    std::cout << "[CircleDetectionUI] Error saving operation mappings: " << e.what() << std::endl;
  }
}