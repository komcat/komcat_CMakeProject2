// include/scanning/grid_volume_scanner_ui.cpp
#include "grid_volume_scanner_ui.h"
#include "include/logger.h"
#include <algorithm>
#include <limits>
#include <sstream>
#include <iomanip>

GridVolumeScannerUI::GridVolumeScannerUI()
  : m_visible(false)
  , m_deviceName("Unknown")
  , m_dataChannel("Unknown") {

  ResetVolumeData();
  Logger::GetInstance()->LogInfo("GridVolumeScannerUI initialized");
}

void GridVolumeScannerUI::SetScanner(std::shared_ptr<GridVolumeScanner> scanner) {
  m_scanner = scanner;
  if (scanner) {
    Logger::GetInstance()->LogInfo("Volume scanner connected");
  }
}

void GridVolumeScannerUI::SetDeviceInfo(const std::string& deviceName, const std::string& dataChannel) {
  m_deviceName = deviceName;
  m_dataChannel = dataChannel;
}

void GridVolumeScannerUI::ResetVolumeData() {
  m_volumeData.data.clear();
  m_volumeData.zPositions.clear();
  m_currentLayerData.clear();
  m_currentLayer = 0;
}

void GridVolumeScannerUI::Render() {
  if (!m_visible) return;

  ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("3D Volume Scanner", &m_visible)) {
    ImGui::End();
    return;
  }

  // Update scan state
  if (m_scanner) {
    bool wasScanning = m_scanInProgress;
    m_scanInProgress = m_scanner->IsVolumeScanActive();

    // Get volume data when scan completes OR when viewing results
    if ((!m_scanInProgress && wasScanning) ||
      (!m_scanInProgress && m_scanner->GetVolumeData().data.size() > 0 && m_volumeData.data.empty())) {
      m_volumeData = m_scanner->GetVolumeData();

      // Auto-select first layer for display
      if (!m_volumeData.data.empty() && m_currentLayer < m_volumeData.data.size()) {
        UpdateLayerDisplay(0);
      }
    }
  }

  // Left panel - Controls
  ImGui::BeginChild("Controls", ImVec2(350, 0), true);
  RenderControls();
  ImGui::EndChild();

  ImGui::SameLine();

  // Right panel - Visualization
  ImGui::BeginChild("Visualization", ImVec2(0, 0), true);
  RenderLayerView();
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  RenderVolumeStats();
  ImGui::EndChild();

  ImGui::End();

  // Render results dialog if needed
  if (m_showResultsDialog) {
    RenderResultsDialog();
  }
}

void GridVolumeScannerUI::RenderControls() {
  ImGui::Text("3D Volume Scanner Control");
  ImGui::Separator();

  // Connection Status Section
  ImGui::Text("Connection Status");
  // Add a test button to the UI (in RenderControls):
  if (ImGui::Button("Test Z Movement", ImVec2(-1, 25))) {
    TestZMovement();
  }
  ImGui::Separator();

  bool motionConnected = m_scanner && m_scanner->IsMotionControllerConnected();
  bool dataConnected = m_scanner && m_scanner->IsDataChannelConnected();

  // Motion controller status
  if (motionConnected) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ Motion: %s", m_deviceName.c_str());
  }
  else {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "✗ Motion: %s (Not Connected)", m_deviceName.c_str());
  }

  // Data channel status
  if (dataConnected) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ Data: %s", m_dataChannel.c_str());
  }
  else {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "✗ Data: %s (No Data)", m_dataChannel.c_str());
  }

  if (!motionConnected || !dataConnected) {
    if (ImGui::Button("Check Connections", ImVec2(-1, 25))) {
      CheckAndReconnectHardware();
    }
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Grid Parameters (XY)");
  ImGui::Separator();

  ImGui::BeginDisabled(m_scanInProgress);

  ImGui::PushItemWidth(100);
  if (ImGui::DragFloat("X Step (µm)", &m_xStep, 1.0f, 1.0f, 1000.0f, "%.1f")) {
    if (m_scanner) {
      m_scanner->SetGridParameters(m_xStep, m_yStep, m_xPoints, m_yPoints);
    }
  }
  ImGui::SameLine();
  if (ImGui::DragInt("X Points", &m_xPoints, 1, 2, 20)) {
    if (m_scanner) {
      m_scanner->SetGridParameters(m_xStep, m_yStep, m_xPoints, m_yPoints);
    }
  }

  if (ImGui::DragFloat("Y Step (µm)", &m_yStep, 1.0f, 1.0f, 1000.0f, "%.1f")) {
    if (m_scanner) {
      m_scanner->SetGridParameters(m_xStep, m_yStep, m_xPoints, m_yPoints);
    }
  }
  ImGui::SameLine();
  if (ImGui::DragInt("Y Points", &m_yPoints, 1, 2, 20)) {
    if (m_scanner) {
      m_scanner->SetGridParameters(m_xStep, m_yStep, m_xPoints, m_yPoints);
    }
  }
  ImGui::PopItemWidth();

  // Settling Time control
  ImGui::PushItemWidth(150);
  if (ImGui::InputInt("Settling Time (ms)", &m_settlingTime, 10, 100)) {
    m_settlingTime = (std::max)(0, (std::min)(m_settlingTime, 5000));  // 0 to 5000 ms
    if (m_scanner) {
      m_scanner->SetSettlingTime(m_settlingTime);
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Time to wait after moving to each point before taking measurement");
  }
  ImGui::PopItemWidth();

  ImGui::Separator();
  ImGui::Text("Z Scan Parameters");
  ImGui::Separator();

  // Z Direction selector
  ImGui::Text("Scan Direction from Current Position:");
  ImGui::RadioButton("Negative (-Z)", &m_zDirection, 0);
  ImGui::SameLine();
  ImGui::RadioButton("Positive (+Z)", &m_zDirection, 1);

  ImGui::PushItemWidth(150);

  // Z Step size in micrometers
  if (ImGui::DragFloat("Z Step Size (µm)", &m_zStepSize, 0.1f, 0.1f, 100.0f, "%.1f")) {
    m_zStepSize = (std::max)(0.1f, m_zStepSize);  // Minimum 0.1 µm
  }

  // Number of Z steps
  if (ImGui::InputInt("Number of Z Steps", &m_zSteps, 1, 5)) {
    m_zSteps = (std::max)(1, (std::min)(m_zSteps, 100));  // 1 to 100 steps
  }

  ImGui::PopItemWidth();

  // Calculate and display the scan range
  float zStart, zEnd;
  CalculateZRange(zStart, zEnd);

  // Volume info
  ImGui::Spacing();
  int totalPoints = m_xPoints * m_yPoints * m_zSteps;
  float xRange = (m_xPoints - 1) * m_xStep;  // µm
  float yRange = (m_yPoints - 1) * m_yStep;  // µm
  float zRange = (m_zSteps - 1) * m_zStepSize; // µm

  ImGui::Text("Scan Info:");
  ImGui::Indent();
  ImGui::Text("Starting from: Current Position (0 µm)");
  ImGui::Text("Scan Range: %.1f to %.1f µm", zStart, zEnd);
  ImGui::Text("Total Z Travel: %.1f µm", zRange);
  ImGui::Text("Scan Volume: %.1f × %.1f × %.1f µm³", xRange, yRange, zRange);
  ImGui::Text("Total Points: %d", totalPoints);
  ImGui::Text("Points per layer: %d", m_xPoints * m_yPoints);
  ImGui::Unindent();

  float estimatedTime = (m_xPoints * m_yPoints * m_zSteps * (m_settlingTime + 50)) / 1000.0f;
  ImGui::Text("Estimated Time: %.1f seconds", estimatedTime);

  ImGui::EndDisabled();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Scan Control");
  ImGui::Separator();

  bool canScan = motionConnected && dataConnected;

  if (m_scanner) {
    if (!m_scanInProgress) {
      ImGui::BeginDisabled(!canScan);

      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0.6f, 0, 1));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0.8f, 0, 1));

      if (ImGui::Button("Start Volume Scan", ImVec2(-1, 35))) {
        ResetVolumeData();

        // Calculate actual Z range based on direction and steps
        float zStart, zEnd;
        CalculateZRange(zStart, zEnd);

        // Set scanner parameters (convert µm to mm)
        if (m_scanner) {
          m_scanner->SetZScanParameters(zStart / 1000.0, zEnd / 1000.0, m_zSteps);
        }

        bool started = m_scanner->StartVolumeScan(
          [this](int layer, int total, double z) {
          m_currentScanLayer = layer;
          m_currentZ = z;  // z is in mm from scanner
          m_scanProgress = static_cast<float>(layer) / total;
        }
        );

        if (started) {
          Logger::GetInstance()->LogInfo("Volume scan started: " +
            std::to_string(m_zSteps) + " steps of " +
            std::to_string(m_zStepSize) + " µm in " +
            (m_zDirection == 0 ? "negative" : "positive") + " direction");
        }
        else {
          Logger::GetInstance()->LogError("Failed to start volume scan");
        }
      }

      ImGui::PopStyleColor(2);
      ImGui::EndDisabled();

      if (!canScan && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Connect hardware before scanning");
      }
    }
    else {
      // Progress bar
      ImGui::ProgressBar(m_scanProgress, ImVec2(-1, 20));
      ImGui::Text("Step %d of %d (Z=%.1f µm)",
        m_currentScanLayer + 1, m_zSteps, m_currentZ * 1000.0);

      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0, 1));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 0.3f, 0, 1));

      if (ImGui::Button("Stop Scan", ImVec2(-1, 35))) {
        m_scanner->StopVolumeScan();
        Logger::GetInstance()->LogInfo("Volume scan stopped by user");
      }

      ImGui::PopStyleColor(2);
    }
  }
  else {
    ImGui::TextWrapped("No scanner connected");
  }

  // Display settings
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Display Settings");
  ImGui::Separator();

  ImGui::Checkbox("Auto Scale", &m_autoScale);

  ImGui::BeginDisabled(m_autoScale);
  ImGui::DragFloat("Min", &m_colorScaleMin, 0.001f);
  ImGui::DragFloat("Max", &m_colorScaleMax, 0.001f);
  ImGui::EndDisabled();

  const char* colormaps[] = { "Viridis", "Plasma", "Hot", "Cool", "Jet" };
  ImGui::Combo("Colormap", &m_colormapIndex, colormaps, IM_ARRAYSIZE(colormaps));
}



void GridVolumeScannerUI::RenderLayerView() {
  ImGui::Text("Layer View");
  ImGui::Separator();

  // Layer selector
  if (!m_volumeData.data.empty()) {
    int previousLayer = m_currentLayer;
    ImGui::SliderInt("Layer", &m_currentLayer, 0,
      static_cast<int>(m_volumeData.data.size()) - 1);

    if (m_currentLayer != previousLayer || m_currentLayerData.empty()) {
      UpdateLayerDisplay(m_currentLayer);
    }

    // Display Z position relative to start (in micrometers)
    float relativeZ = m_currentLayer * m_zStepSize;
    if (m_zDirection == 0) relativeZ = -relativeZ;  // Negative direction

    ImGui::Text("Step %d: Z = %.1f µm from start",
      m_currentLayer + 1, relativeZ);
  }

  // Plot current layer heatmap
  if (!m_currentLayerData.empty() && m_currentLayerData.size() == m_yPoints) {
    if (ImPlot::BeginPlot("##LayerHeatmap", ImVec2(-1, 400))) {
      //ImPlot::PushColormap(static_cast<ImPlotColormap>(m_colormapIndex));
      // Use same colormap as grid scanner
      ImPlotColormap colormap = ImPlotColormap_Viridis;
      switch (m_colormapIndex) {
      case 0: colormap = ImPlotColormap_Viridis; break;
      case 1: colormap = ImPlotColormap_Plasma; break;
      case 2: colormap = ImPlotColormap_Hot; break;
      case 3: colormap = ImPlotColormap_Cool; break;
      case 4: colormap = ImPlotColormap_Pink; break;
      case 5: colormap = ImPlotColormap_Jet; break;
      case 6: colormap = ImPlotColormap_Twilight; break;
      case 7: colormap = ImPlotColormap_RdBu; break;
      case 8: colormap = ImPlotColormap_BrBG; break;
      case 9: colormap = ImPlotColormap_PiYG; break;
      }
      ImPlot::PushColormap(colormap);

      ImPlot::SetupAxes("X Position (µm)", "Y Position (µm)");

      // Flatten data for ImPlot - ensure correct dimensions
      std::vector<double> flatData;
      flatData.reserve(m_yPoints * m_xPoints);

      for (int row = m_yPoints - 1; row >= 0; --row) {
        for (int col = 0; col < m_xPoints; ++col) {
          if (row < m_currentLayerData.size() && col < m_currentLayerData[row].size()) {
            flatData.push_back(m_currentLayerData[row][col]);
          }
          else {
            flatData.push_back(0.0);
          }
        }
      }

      // Recalculate color scale if auto-scale is on
      if (m_autoScale && !flatData.empty()) {
        auto [minIt, maxIt] = std::minmax_element(flatData.begin(), flatData.end());
        m_colorScaleMin = static_cast<float>(*minIt);
        m_colorScaleMax = static_cast<float>(*maxIt);

        // Ensure valid range
        if (m_colorScaleMin >= m_colorScaleMax) {
          m_colorScaleMax = m_colorScaleMin + 0.000001f;
        }
      }

      double xExtent = ((m_xPoints - 1) * m_xStep) / 2.0;
      double yExtent = ((m_yPoints - 1) * m_yStep) / 2.0;

      ImPlot::PlotHeatmap("Layer Data",
        flatData.data(),
        m_yPoints,
        m_xPoints,
        m_colorScaleMin,
        m_colorScaleMax,
        "",
        ImPlotPoint(-xExtent, -yExtent),
        ImPlotPoint(xExtent, yExtent));

      ImPlot::PopColormap();
      ImPlot::EndPlot();
    }

    // Show actual data range
    ImGui::Text("Data Range: %.6f to %.6f", m_colorScaleMin, m_colorScaleMax);
  }
  else {
    ImGui::Text("No data available");
    if (!m_volumeData.data.empty()) {
      ImGui::Text("Debug: Volume has %zu layers, current layer %d",
        m_volumeData.data.size(), m_currentLayer);
      if (m_currentLayer < m_volumeData.data.size()) {
        ImGui::Text("Layer data size: %zu rows", m_currentLayerData.size());
      }
    }
  }
}


// Add to GridVolumeScannerUI::RenderVolumeStats() method

void GridVolumeScannerUI::RenderVolumeStats() {
  ImGui::Text("Volume Statistics");
  ImGui::Separator();

  if (m_scanner && !m_volumeData.data.empty()) {
    const auto& volData = m_scanner->GetVolumeData();

    ImGui::Text("Scan ID: %s", volData.scanId.c_str());
    ImGui::Text("Total Layers: %zu", volData.data.size());
    ImGui::Text("Points per Layer: %d", m_xPoints * m_yPoints);
    ImGui::Text("Total Points: %zu", volData.data.size() * m_xPoints * m_yPoints);

    // NEW: Show Z movement confirmation
    ImGui::Separator();
    ImGui::Text("Z Movement Status:");
    ImGui::Indent();

    // Show Z positions for each layer
    for (size_t i = 0; i < volData.zPositions.size() && i < 5; ++i) { // Show first 5 layers
      ImGui::Text("Layer %zu: Z = %.6f mm", i + 1, volData.zPositions[i]);
    }

    if (volData.zPositions.size() > 5) {
      ImGui::Text("... (%zu more layers)", volData.zPositions.size() - 5);
    }

    // Calculate Z travel
    if (volData.zPositions.size() > 1) {
      double zTravel = volData.zPositions.back() - volData.zPositions.front();
      ImGui::Text("Total Z Travel: %.6f mm", zTravel);

      // Check if Z actually moved
      if (std::abs(zTravel) < 0.000001) { // Less than 1 µm
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "⚠ Warning: Z axis did not move!");
        ImGui::TextWrapped("All layers scanned at same Z position. Check Z-axis connection.");
      }
      else {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ Z movement confirmed");
      }
    }

    ImGui::Unindent();

    ImGui::Separator();
    ImGui::Text("Global Peak:");
    ImGui::Indent();
    ImGui::Text("Value: %.6f", volData.peakValue);
    ImGui::Text("Position (mm): (%.3f, %.3f, %.3f)",
      volData.peakPosition.x,
      volData.peakPosition.y,
      volData.peakPosition.z);
    ImGui::Text("Position (µm): (%.1f, %.1f, %.1f)",
      volData.peakPosition.x * 1000.0,
      volData.peakPosition.y * 1000.0,
      volData.peakPosition.z * 1000.0);
    ImGui::Text("Grid Index: [%d, %d, %d]",
      volData.peakPosition.col,
      volData.peakPosition.row,
      volData.peakPosition.layer);
    ImGui::Unindent();

    if (ImGui::Button("Move to Peak XY", ImVec2(120, 25))) {
      if (m_piManager) {
        PIController* controller = m_piManager->GetController(m_deviceName);
        if (controller) {
          controller->MoveToPositionMultiAxis(
            { "X", "Y" },
            { volData.peakPosition.x, volData.peakPosition.y },
            true);
        }
      }
    }

    ImGui::SameLine();

    if (ImGui::Button("Move to Peak Z", ImVec2(120, 25))) {
      if (m_piManager) {
        PIController* controller = m_piManager->GetController(m_deviceName);
        if (controller) {
          // Move to peak Z position (absolute move)
          controller->MoveToPosition("Z", volData.peakPosition.z, true);
        }
      }
    }
  }
  else {
    ImGui::Text("No volume data available");
  }
}




void GridVolumeScannerUI::RenderResultsDialog() {
  ImGui::OpenPopup("Volume Scan Results");

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal("Volume Scan Results", &m_showResultsDialog,
    ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Volume Scan Completed!");
    ImGui::Separator();

    ImGui::Text("Peak Value: %.6f", m_lastPeakValue);
    ImGui::Text("Peak Position:");
    ImGui::Indent();
    ImGui::Text("X: %.3f mm (%.1f µm)", m_lastPeakPosition.x, m_lastPeakPosition.x * 1000.0);
    ImGui::Text("Y: %.3f mm (%.1f µm)", m_lastPeakPosition.y, m_lastPeakPosition.y * 1000.0);
    ImGui::Text("Z: %.3f mm (%.1f µm)", m_lastPeakPosition.z, m_lastPeakPosition.z * 1000.0);
    ImGui::Unindent();
    ImGui::Text("Scan Duration: %.1f seconds", m_lastScanDuration);

    ImGui::Separator();

    if (ImGui::Button("OK", ImVec2(120, 0))) {
      m_showResultsDialog = false;
    }

    ImGui::EndPopup();
  }
}

void GridVolumeScannerUI::CheckAndReconnectHardware() {
  if (!m_scanner) return;

  Logger::GetInstance()->LogInfo("GridVolumeScannerUI: Checking hardware connections...");

  // Check motion controller
  if (!m_scanner->IsMotionControllerConnected() && m_piManager) {
    PIController* controller = m_piManager->GetController(m_deviceName);
    if (controller && controller->IsConnected()) {
      auto motionAdapter = std::make_shared<PIScanMotionAdapter>(
        controller, m_deviceName);
      m_scanner->SetMotionController(motionAdapter);
      Logger::GetInstance()->LogInfo("GridVolumeScannerUI: Reconnected to " + m_deviceName);
    }
  }

  // Check data channel
  GlobalDataStore* dataStore = GlobalDataStore::GetInstance();
  if (dataStore) {
    auto channels = dataStore->GetAvailableChannels();
    for (const auto& channel : channels) {
      if (channel == m_dataChannel ||
        channel == "(" + m_dataChannel + ")") {

        if (!m_scanner->IsDataChannelConnected() && m_dataManager) {
          m_scanner->SetDataChannel(channel);
          Logger::GetInstance()->LogInfo("GridVolumeScannerUI: Connected to " + channel);
        }
        break;
      }
    }
  }
}

void GridVolumeScannerUI::UpdateLayerDisplay(int layer) {
  if (layer >= 0 && layer < m_volumeData.data.size()) {
    m_currentLayer = layer;
    m_currentLayerData = m_volumeData.data[layer];

    // Auto-scale if enabled
    if (m_autoScale) {
      double minVal = (std::numeric_limits<double>::max)();
      double maxVal = std::numeric_limits<double>::lowest();

      for (const auto& row : m_currentLayerData) {
        for (double val : row) {
          if (!std::isnan(val) && !std::isinf(val)) {
            minVal = (std::min)(minVal, val);
            maxVal = (std::max)(maxVal, val);
          }
        }
      }

      m_colorScaleMin = static_cast<float>(minVal);
      m_colorScaleMax = static_cast<float>(maxVal);
    }
  }
}

void GridVolumeScannerUI::CalculateZRange(float& zStart, float& zEnd) const {
  // Always start from current position (0)
  if (m_zDirection == 0) {  // Negative direction
    zStart = 0.0f;
    zEnd = -(m_zSteps - 1) * m_zStepSize;
  }
  else {  // Positive direction
    zStart = 0.0f;
    zEnd = (m_zSteps - 1) * m_zStepSize;
  }
}



void GridVolumeScannerUI::TestZMovement() {
  if (!m_scanner || !m_piManager) return;

  PIController* controller = m_piManager->GetController(m_deviceName);
  if (!controller) return;

  Logger::GetInstance()->LogInfo("=== Z MOVEMENT TEST ===");

  // Get current Z position
  std::map<std::string, double> startPos;
  if (controller->GetPositions(startPos)) {
    auto zIt = startPos.find("Z");
    if (zIt != startPos.end()) {
      double startZ = zIt->second;
      Logger::GetInstance()->LogInfo("Starting Z position: " + std::to_string(startZ) + " mm");

      // Test 1: Move +0.010 mm (should be +10 µm)
      Logger::GetInstance()->LogInfo("Test 1: Moving +0.010 mm (+10 µm)");
      bool success1 = controller->MoveRelative("Z", 0.010, true);
      Logger::GetInstance()->LogInfo("Move result: " + std::string(success1 ? "SUCCESS" : "FAILED"));

      // Check new position
      std::map<std::string, double> newPos1;
      if (controller->GetPositions(newPos1)) {
        auto zIt1 = newPos1.find("Z");
        if (zIt1 != newPos1.end()) {
          double newZ1 = zIt1->second;
          double actualMove1 = newZ1 - startZ;
          Logger::GetInstance()->LogInfo("New Z position: " + std::to_string(newZ1) + " mm");
          Logger::GetInstance()->LogInfo("Actual movement: " + std::to_string(actualMove1) + " mm (" +
            std::to_string(actualMove1 * 1000.0) + " µm)");
        }
      }

      // Test 2: Move back -0.010 mm 
      Logger::GetInstance()->LogInfo("Test 2: Moving -0.010 mm (-10 µm)");
      bool success2 = controller->MoveRelative("Z", -0.010, true);
      Logger::GetInstance()->LogInfo("Move result: " + std::string(success2 ? "SUCCESS" : "FAILED"));

      // Check final position
      std::map<std::string, double> finalPos;
      if (controller->GetPositions(finalPos)) {
        auto zItF = finalPos.find("Z");
        if (zItF != finalPos.end()) {
          double finalZ = zItF->second;
          double totalMove = finalZ - startZ;
          Logger::GetInstance()->LogInfo("Final Z position: " + std::to_string(finalZ) + " mm");
          Logger::GetInstance()->LogInfo("Total movement from start: " + std::to_string(totalMove) + " mm (" +
            std::to_string(totalMove * 1000.0) + " µm)");
        }
      }
    }
  }

  Logger::GetInstance()->LogInfo("=== Z MOVEMENT TEST COMPLETE ===");
}
