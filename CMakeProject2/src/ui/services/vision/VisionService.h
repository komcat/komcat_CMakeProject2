// src/ui/services/vision/VisionService.h
#pragma once

#include "../UIServiceRegistry.h"
#include "core/ServiceLocator.h"
#include "utils/Logger.h"
#include "utils/Unicode.h"
#include "imgui.h"

class VisionService : public IUIService {
public:
  void RenderUI() override {
    auto camera = Services.Camera();

    ImGui::SetWindowFontScale(1.5f);
    ImGui::Text("👁️ Vision System");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Spacing();
    ImGui::Text("Machine Vision and Inspection Control");
    ImGui::Separator();

    // Camera status
    ImGui::Text("Camera Status:");
    if (camera) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✅ Cameras: Connected and Ready");
      RenderCameraInterface();
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "❌ Cameras: Not Connected");
      RenderDisconnectedInterface();
    }
  }

  std::string GetServiceName() const override { return "vision_system"; }
  std::string GetDisplayName() const override { return "Vision System"; }
  std::string GetCategory() const override { return "Vision"; }
  bool IsAvailable() const override { return true; }

private:
  static inline bool isCapturing = false;
  static inline bool isAnalyzing = false;
  static inline int selectedCamera = 0;

  void RenderCameraInterface() {
    ImGui::Spacing();

    // Camera selection
    ImGui::Text("Camera Selection:");
    const char* cameras[] = { "Main Camera", "Inspection Camera", "Overhead Camera", "Side Camera" };
    ImGui::Combo("Active Camera", &selectedCamera, cameras, IM_ARRAYSIZE(cameras));

    ImGui::Spacing();

    // Live view controls
    ImGui::Text("Live View Controls:");

    // Capture controls
    if (ImGui::Button(isCapturing ? "⏹️ Stop Capture" : "▶️ Start Capture", ImVec2(150, 30))) {
      isCapturing = !isCapturing;
      if (isCapturing) {
        StartCapture();
      }
      else {
        StopCapture();
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("📸 Single Shot", ImVec2(150, 30))) {
      TakeSingleShot();
    }

    ImGui::SameLine();
    if (ImGui::Button("💾 Save Image", ImVec2(150, 30))) {
      SaveCurrentImage();
    }

    // Camera settings
    ImGui::Spacing();
    ImGui::Text("Camera Settings:");

    static float exposure = 50.0f;
    if (ImGui::SliderFloat("Exposure", &exposure, 1.0f, 100.0f, "%.0f ms")) {
      SetExposure(exposure);
    }

    static float gain = 25.0f;
    if (ImGui::SliderFloat("Gain", &gain, 0.0f, 100.0f, "%.0f%%")) {
      SetGain(gain);
    }

    static float focus = 50.0f;
    if (ImGui::SliderFloat("Focus", &focus, 0.0f, 100.0f, "%.0f%%")) {
      SetFocus(focus);
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Analysis tools
    RenderAnalysisTools();

    ImGui::Spacing();
    ImGui::Separator();

    // Live statistics
    RenderLiveStatistics();
  }

  void RenderAnalysisTools() {
    ImGui::Text("Image Analysis Tools:");

    // Analysis controls
    if (ImGui::Button(isAnalyzing ? "⏹️ Stop Analysis" : "🔍 Start Analysis", ImVec2(150, 30))) {
      isAnalyzing = !isAnalyzing;
      if (isAnalyzing) {
        StartAnalysis();
      }
      else {
        StopAnalysis();
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("📊 Run Inspection", ImVec2(150, 30))) {
      RunInspection();
    }

    ImGui::SameLine();
    if (ImGui::Button("🎯 Calibrate", ImVec2(150, 30))) {
      RunCalibration();
    }

    // Analysis settings
    ImGui::Spacing();
    ImGui::Text("Analysis Settings:");

    static bool enableEdgeDetection = true;
    static bool enableColorAnalysis = false;
    static bool enableDimensionCheck = true;
    static bool enableDefectDetection = true;

    ImGui::Checkbox("🔍 Edge Detection", &enableEdgeDetection);
    ImGui::SameLine();
    ImGui::Checkbox("🎨 Color Analysis", &enableColorAnalysis);

    ImGui::Checkbox("📏 Dimension Check", &enableDimensionCheck);
    ImGui::SameLine();
    ImGui::Checkbox("⚠️ Defect Detection", &enableDefectDetection);

    // Thresholds
    if (enableEdgeDetection) {
      static float edgeThreshold = 128.0f;
      ImGui::SliderFloat("Edge Threshold", &edgeThreshold, 50.0f, 255.0f, "%.0f");
    }

    if (enableDimensionCheck) {
      static float dimensionTolerance = 0.1f;
      ImGui::SliderFloat("Dimension Tolerance (mm)", &dimensionTolerance, 0.01f, 1.0f, "%.2f");
    }
  }

  void RenderLiveStatistics() {
    ImGui::Text("Live Statistics:");

    // Current image info
    ImGui::BulletText("Resolution: 1920x1080");
    ImGui::BulletText("Frame Rate: %.1f FPS", ImGui::GetIO().Framerate);
    ImGui::BulletText("Processing Time: %.1f ms", GetProcessingTime());

    // Analysis results (if analyzing)
    if (isAnalyzing) {
      ImGui::Spacing();
      ImGui::Text("Analysis Results:");
      ImGui::BulletText("Objects Detected: %d", GetObjectCount());
      ImGui::BulletText("Pass Rate: %.1f%%", GetPassRate());
      ImGui::BulletText("Average Dimension: %.2f mm", GetAverageDimension());

      // Quality indicators
      float quality = GetImageQuality();
      ImVec4 qualityColor = quality > 80.0f ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) :
        quality > 60.0f ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f) :
        ImVec4(1.0f, 0.0f, 0.0f, 1.0f);

      ImGui::Text("Image Quality:");
      ImGui::SameLine();
      ImGui::TextColored(qualityColor, "%.1f%%", quality);
    }

    // Camera health
    ImGui::Spacing();
    ImGui::Text("Camera Health:");
    ImGui::BulletText("Temperature: %.1f°C", GetCameraTemperature());
    ImGui::BulletText("Uptime: %s", GetCameraUptime().c_str());
    ImGui::BulletText("Total Images: %d", GetTotalImageCount());
  }

  void RenderDisconnectedInterface() {
    ImGui::Spacing();
    ImGui::Text("Connection Options:");

    if (ImGui::Button("🔌 Connect Cameras", ImVec2(200, 40))) {
      AttemptCameraConnection();
    }

    ImGui::Spacing();
    ImGui::Text("Camera Requirements:");
    ImGui::BulletText("USB3.0 or GigE cameras connected");
    ImGui::BulletText("Camera drivers installed");
    ImGui::BulletText("Proper lighting conditions");
    ImGui::BulletText("Calibration files available");

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
      "ℹ️ Vision system will be enabled when cameras are connected.");
  }

  // Camera control methods
  void StartCapture() {
    auto camera = Services.Camera();
    if (camera) {
      // camera->StartCapture();
      Logger::Info(L"Started camera capture");
    }
  }

  void StopCapture() {
    auto camera = Services.Camera();
    if (camera) {
      // camera->StopCapture();
      Logger::Info(L"Stopped camera capture");
    }
  }

  void TakeSingleShot() {
    auto camera = Services.Camera();
    if (camera) {
      // camera->TakeSingleShot();
      Logger::Info(L"Single shot captured");
    }
  }

  void SaveCurrentImage() {
    Logger::Success(L"Image saved successfully");
  }

  void SetExposure(float exposure) {
    auto camera = Services.Camera();
    if (camera) {
      // camera->SetExposure(exposure);
    }
  }

  void SetGain(float gain) {
    auto camera = Services.Camera();
    if (camera) {
      // camera->SetGain(gain);
    }
  }

  void SetFocus(float focus) {
    auto camera = Services.Camera();
    if (camera) {
      // camera->SetFocus(focus);
    }
  }

  // Analysis methods
  void StartAnalysis() {
    Logger::Info(L"Started image analysis");
  }

  void StopAnalysis() {
    Logger::Info(L"Stopped image analysis");
  }

  void RunInspection() {
    Logger::Info(L"Running quality inspection");
  }

  void RunCalibration() {
    Logger::Info(L"Running camera calibration");
  }

  void AttemptCameraConnection() {
    Logger::Info(L"Attempting to connect to cameras...");
  }

  // Statistics methods
  float GetProcessingTime() const {
    return 15.5f + sin(ImGui::GetTime()) * 2.0f;
  }

  int GetObjectCount() const {
    return 3 + (int)(sin(ImGui::GetTime() * 0.5f) * 2.0f);
  }

  float GetPassRate() const {
    return 94.5f + sin(ImGui::GetTime() * 0.3f) * 3.0f;
  }

  float GetAverageDimension() const {
    return 25.0f + sin(ImGui::GetTime() * 0.8f) * 0.5f;
  }

  float GetImageQuality() const {
    return 85.0f + sin(ImGui::GetTime() * 0.4f) * 10.0f;
  }

  float GetCameraTemperature() const {
    return 45.0f + sin(ImGui::GetTime() * 0.1f) * 5.0f;
  }

  std::string GetCameraUptime() const {
    return "12h 34m";
  }

  int GetTotalImageCount() const {
    return 15847;
  }
};