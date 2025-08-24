// UIVisionPanel_Parameters.cpp - Complete parameter controls covering ALL circle detection parameters
#include "UIVisionPanel.h"
#include "include/halcon/VisionCircleDetection.h"
#include <iostream>


// OpenGL headers for texture management
#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#else
#include <OpenGL/gl.h>
#endif


void UIVisionPanel::RenderCircleParameterControls() {
  if (!m_circleDetector) return;

  auto params = m_circleDetector->GetParameters();
  bool paramsChanged = false;

  // === ROI PARAMETERS ===
  if (ImGui::CollapsingHeader("ROI (Region of Interest)", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("Define the search area:");

    if (ImGui::SliderInt("ROI Size", &params.roiSize, 50, 1000, "%d px")) {
      paramsChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("?##roi_size")) {
      ImGui::SetTooltip("Search area size (±pixels from center)");
    }

    if (ImGui::SliderInt("Offset X", &params.roiOffsetX, -500, 500, "%d px")) {
      paramsChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("?##roi_x")) {
      ImGui::SetTooltip("Horizontal offset from image center");
    }

    if (ImGui::SliderInt("Offset Y", &params.roiOffsetY, -500, 500, "%d px")) {
      paramsChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("?##roi_y")) {
      ImGui::SetTooltip("Vertical offset from image center");
    }
  }

  ImGui::Spacing();

  // === THRESHOLD PARAMETERS ===
  if (ImGui::CollapsingHeader("Threshold", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("Image brightness filtering:");

    if (ImGui::SliderInt("Low", &params.thresholdLow, 0, 255)) {
      paramsChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("?##thresh_low")) {
      ImGui::SetTooltip("Minimum brightness value (0-255)");
    }

    if (ImGui::SliderInt("High", &params.thresholdHigh, 0, 255)) {
      paramsChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("?##thresh_high")) {
      ImGui::SetTooltip("Maximum brightness value (0-255)");
    }

    if (ImGui::Checkbox("Invert Image", &params.invertImage)) {
      paramsChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Preview##invert_preview")) {
      if (m_hasImageData) {
        m_showInvertPreview = true;
        CreateInvertedTexture();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("?##invert")) {
      ImGui::SetTooltip("Invert image before processing (for dark circles on light background)");
    }
  }

  ImGui::Spacing();

  // === FILTER PARAMETERS ===
  if (ImGui::CollapsingHeader("Shape Filtering", ImGuiTreeNodeFlags_DefaultOpen)) {

    // Area constraints
    ImGui::Text("Area Constraints:");
    if (ImGui::SliderInt("Min Area", &params.minArea, 10, 10000, "%d px²")) {
      paramsChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("?##min_area")) {
      ImGui::SetTooltip("Minimum region area in pixels²");
    }

    if (ImGui::SliderInt("Max Area", &params.maxArea, 100, 100000, "%d px²")) {
      paramsChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("?##max_area")) {
      ImGui::SetTooltip("Maximum region area in pixels²");
    }

    ImGui::Spacing();

    // Circularity constraints
    ImGui::Text("Circularity Constraints:");
    if (ImGui::SliderFloat("Min Circularity", &params.minCircularity, 0.0f, 1.0f, "%.3f")) {
      paramsChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("?##min_circ")) {
      ImGui::SetTooltip("Minimum circularity (0.0 = any shape, 1.0 = perfect circle)");
    }

    if (ImGui::SliderFloat("Max Circularity", &params.maxCircularity, 0.0f, 1.0f, "%.3f")) {
      paramsChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("?##max_circ")) {
      ImGui::SetTooltip("Maximum circularity (1.0 = perfect circle only)");
    }
  }

  ImGui::Spacing();

  // === CIRCLE SIZE PARAMETERS ===
  if (ImGui::CollapsingHeader("Circle Size", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("Radius Constraints:");

    if (ImGui::SliderFloat("Target", &params.targetRadius, 10.0f, 200.0f, "%.0f px")) {
      paramsChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("?##target_radius")) {
      ImGui::SetTooltip("Preferred circle radius - detection prioritizes circles closest to this size");
    }

    if (ImGui::SliderFloat("Min", &params.minRadius, 1.0f, 200.0f, "%.0f px")) {
      paramsChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("?##min_radius")) {
      ImGui::SetTooltip("Minimum acceptable circle radius");
    }

    if (ImGui::SliderFloat("Max", &params.maxRadius, 1.0f, 200.0f, "%.0f px")) {
      paramsChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("?##max_radius")) {
      ImGui::SetTooltip("Maximum acceptable circle radius");
    }
  }

  ImGui::Spacing();

  // === ADVANCED PARAMETERS ===
  if (ImGui::CollapsingHeader("Advanced Options")) {

    // Compactness filtering
    ImGui::Text("Shape Quality Filtering:");
    if (ImGui::Checkbox("Use Compactness Filter", &params.useCompactnessFilter)) {
      paramsChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("?##use_compact")) {
      ImGui::SetTooltip("Enable shape compactness filtering for better circle detection");
    }

    if (params.useCompactnessFilter) {
      ImGui::Indent();
      if (ImGui::SliderFloat("Min Compactness", &params.minCompactness, 1.0f, 10.0f, "%.2f")) {
        paramsChanged = true;
      }
      ImGui::SameLine();
      if (ImGui::Button("?##min_compact")) {
        ImGui::SetTooltip("Minimum shape compactness (1.0 = most compact/circular)");
      }

      if (ImGui::SliderFloat("Max Compactness", &params.maxCompactness, 1.0f, 10.0f, "%.2f")) {
        paramsChanged = true;
      }
      ImGui::SameLine();
      if (ImGui::Button("?##max_compact")) {
        ImGui::SetTooltip("Maximum shape compactness");
      }
      ImGui::Unindent();
    }

    ImGui::Spacing();

    // Noise reduction
    ImGui::Text("Noise Reduction:");
    if (ImGui::Checkbox("Use Noise Reduction", &params.useNoiseReduction)) {
      paramsChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("?##use_noise")) {
      ImGui::SetTooltip("Apply median filter to reduce image noise before detection");
    }

    if (params.useNoiseReduction) {
      ImGui::Indent();
      const char* kernelSizes[] = { "3x3", "5x5", "7x7", "9x9" };
      int currentKernel = (params.medianKernelSize - 3) / 2;
      if (ImGui::Combo("Median Kernel", &currentKernel, kernelSizes, 4)) {
        params.medianKernelSize = 3 + (currentKernel * 2);
        paramsChanged = true;
      }
      ImGui::SameLine();
      if (ImGui::Button("?##kernel")) {
        ImGui::SetTooltip("Noise reduction filter size (larger = more smoothing)");
      }
      ImGui::Unindent();
    }

    ImGui::Spacing();

    // Fallback detection
    ImGui::Text("Fallback Detection:");
    if (ImGui::Checkbox("Enable Fallback Detection", &params.enableFallback)) {
      paramsChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("?##enable_fallback")) {
      ImGui::SetTooltip("Use alternative detection method if primary detection fails");
    }

    if (params.enableFallback) {
      ImGui::Indent();
      if (ImGui::SliderFloat("Fallback Min Radius", &params.fallbackMinRadius, 1.0f, 200.0f, "%.1f px")) {
        paramsChanged = true;
      }
      ImGui::SameLine();
      if (ImGui::Button("?##fallback_min")) {
        ImGui::SetTooltip("Minimum radius for fallback detection method");
      }

      if (ImGui::SliderFloat("Fallback Max Radius", &params.fallbackMaxRadius, 1.0f, 200.0f, "%.1f px")) {
        paramsChanged = true;
      }
      ImGui::SameLine();
      if (ImGui::Button("?##fallback_max")) {
        ImGui::SetTooltip("Maximum radius for fallback detection method");
      }
      ImGui::Unindent();
    }
  }

  ImGui::Spacing();

  // === PARAMETER FILE OPERATIONS ===
  if (ImGui::CollapsingHeader("Parameter File Operations")) {
    if (ImGui::Button("Load from File", ImVec2(-1, 25))) {
      LoadParameters();
    }

    if (ImGui::Button("Save to File", ImVec2(-1, 25))) {
      SaveParameters();
    }

    if (ImGui::Button("Reset to Defaults", ImVec2(-1, 25))) {
      ResetToDefaults();
    }

    ImGui::Text("Parameter file: %s", m_parameterFilePath.c_str());
  }

  // === PARAMETER VALIDATION AND APPLICATION ===
  if (paramsChanged) {
    // Validate and fix parameter constraints

    // ROI constraints (already reasonable)

    // Threshold constraints
    if (params.thresholdLow > params.thresholdHigh) {
      params.thresholdHigh = params.thresholdLow;
    }

    // Area constraints
    if (params.minArea > params.maxArea) {
      params.maxArea = params.minArea;
    }

    // Circularity constraints
    if (params.minCircularity > params.maxCircularity) {
      params.maxCircularity = params.minCircularity;
    }

    // Radius constraints
    if (params.minRadius > params.maxRadius) {
      params.maxRadius = params.minRadius;
    }
    if (params.targetRadius < params.minRadius) {
      params.targetRadius = params.minRadius;
    }
    if (params.targetRadius > params.maxRadius) {
      params.targetRadius = params.maxRadius;
    }

    // Compactness constraints
    if (params.minCompactness > params.maxCompactness) {
      params.maxCompactness = params.minCompactness;
    }

    // Fallback radius constraints
    if (params.fallbackMinRadius > params.fallbackMaxRadius) {
      params.fallbackMaxRadius = params.fallbackMinRadius;
    }

    // Apply the validated parameters
    m_circleDetector->SetParameters(params);

    // Optional: Show parameter change feedback
    // std::cout << "[UIVisionPanel] Parameters updated" << std::endl;
  }
}

void UIVisionPanel::LoadParameters() {
  if (m_circleDetector && m_circleDetector->LoadParameters(m_parameterFilePath)) {
    std::cout << "[UIVisionPanel] Parameters loaded from: " << m_parameterFilePath << std::endl;
  }
}

void UIVisionPanel::SaveParameters() {
  if (m_circleDetector && m_circleDetector->SaveParameters(m_parameterFilePath)) {
    std::cout << "[UIVisionPanel] Parameters saved to: " << m_parameterFilePath << std::endl;
  }
}

void UIVisionPanel::ResetToDefaults() {
  if (m_circleDetector) {
    auto defaults = VisionCircleDetection::GetDefaultParameters();
    m_circleDetector->SetParameters(defaults);
    std::cout << "[UIVisionPanel] Parameters reset to defaults" << std::endl;
  }
}




// Add this method to UIVisionPanel.cpp
void UIVisionPanel::RenderInvertPreviewDialog() {
  if (!m_showInvertPreview) return;

  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

  if (ImGui::Begin("Image Inversion Preview", &m_showInvertPreview, ImGuiWindowFlags_None)) {
    if (!m_showInvertPreview) {
      CleanupInvertedTexture();
      ImGui::End();
      return;
    }

    // Get current parameters
    auto params = m_circleDetector->GetParameters();

    ImGui::Text("Compare original and processed images:");
    ImGui::Separator();

    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    float windowWidth = availableSize.x;
    float imageWidth = (windowWidth - 30) / 2; // Split in half with some padding

    // Calculate display size maintaining aspect ratio
    float imageAspect = static_cast<float>(m_imageWidth) / static_cast<float>(m_imageHeight);
    ImVec2 displaySize;
    displaySize.x = imageWidth;
    displaySize.y = imageWidth / imageAspect;

    // Ensure height fits
    if (displaySize.y > availableSize.y - 80) {
      displaySize.y = availableSize.y - 80;
      displaySize.x = displaySize.y * imageAspect;
    }

    // Original image column
    ImGui::BeginGroup();
    ImGui::Text("Original Image");
    ImGui::Text("(Current thresholds: %d-%d)", params.thresholdLow, params.thresholdHigh);
    if (m_imageTextureId != 0) {
      ImGui::Image((ImTextureID)(intptr_t)m_imageTextureId, displaySize);
    }
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(15, 0)); // Spacing between images
    ImGui::SameLine();

    // Inverted image column
    ImGui::BeginGroup();
    ImGui::Text("Processed Image");
    ImGui::Text("(Inverted + Thresholded)");
    if (m_invertedTextureId != 0) {
      ImGui::Image((ImTextureID)(intptr_t)m_invertedTextureId, displaySize);
    }
    ImGui::EndGroup();

    ImGui::Separator();

    // Add refresh button for real-time updates
    if (ImGui::Button("Refresh Preview")) {
      CreateInvertedTexture(); // Update with current threshold settings
    }
    ImGui::SameLine();

    // Controls at bottom
    if (ImGui::Button("Apply Inversion")) {
      auto params = m_circleDetector->GetParameters();
      params.invertImage = true;
      m_circleDetector->SetParameters(params);
      m_showInvertPreview = false;
      CleanupInvertedTexture();
    }
    ImGui::SameLine();

    if (ImGui::Button("Keep Original")) {
      auto params = m_circleDetector->GetParameters();
      params.invertImage = false;
      m_circleDetector->SetParameters(params);
      m_showInvertPreview = false;
      CleanupInvertedTexture();
    }
    ImGui::SameLine();

    if (ImGui::Button("Close")) {
      m_showInvertPreview = false;
      CleanupInvertedTexture();
    }
  }
  ImGui::End();
}



void UIVisionPanel::CreateInvertedTexture() {
  if (!m_hasImageData || m_lastImageData.empty()) return;

  // Get current threshold parameters
  auto params = m_circleDetector->GetParameters();

  // Create inverted image data
  m_invertedImageData = m_lastImageData; // Copy original

  // Apply inversion and threshold in real-time
  for (size_t i = 0; i < m_invertedImageData.size(); i++) {
    unsigned char originalPixel = m_lastImageData[i];

    // Invert if enabled
    unsigned char processedPixel = params.invertImage ? (255 - originalPixel) : originalPixel;

    // Apply threshold
    if (processedPixel < params.thresholdLow || processedPixel > params.thresholdHigh) {
      processedPixel = 0; // Black for out-of-range pixels
    }

    m_invertedImageData[i] = processedPixel;
  }

  // Create OpenGL texture for inverted image
  CleanupInvertedTexture(); // Clean up any existing texture

  glGenTextures(1, &m_invertedTextureId);
  glBindTexture(GL_TEXTURE_2D, m_invertedTextureId);

  // Set texture parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Determine format based on image channels
  GLenum format = GL_RGB;
  int channels = static_cast<int>(m_invertedImageData.size()) / (m_imageWidth * m_imageHeight);
  if (channels == 1) format = GL_LUMINANCE;
  else if (channels == 3) format = GL_RGB;
  else if (channels == 4) format = GL_RGBA;

  // Upload texture data
  glTexImage2D(GL_TEXTURE_2D, 0, format, m_imageWidth, m_imageHeight, 0,
    format, GL_UNSIGNED_BYTE, m_invertedImageData.data());

  glBindTexture(GL_TEXTURE_2D, 0);
}

void UIVisionPanel::CleanupInvertedTexture() {
  if (m_invertedTextureId != 0) {
    glDeleteTextures(1, &m_invertedTextureId);
    m_invertedTextureId = 0;
  }
  m_invertedImageData.clear();
}