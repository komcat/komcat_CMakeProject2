// include/scanning/grid_scanner_ui.cpp
#include "include/scanning/grid_scanner_ui.h"
#include "include/scanning/PIScanMotionAdapter.h"  // Add this
#include "include/motions/pi_controller_manager.h"  // Add this
#include "include/logger.h"
#include <algorithm>
#include <limits>
#include <sstream>
#include <iomanip>

GridScannerUI::GridScannerUI()
  : m_visible(false)
  , m_deviceName("Unknown")
  , m_dataChannel("Unknown")
  , m_currentValue(0.0)
  , m_dataUpdated(false)
  , m_xStep(50.0f)
  , m_yStep(10.0f)
  , m_xPoints(2)
  , m_yPoints(5)
  , m_settlingTime(100)
  , m_colorScaleMin(0.0f)
  , m_colorScaleMax(1.0f)
  , m_autoScale(true)
  , m_colormapIndex(0)
  , m_scanInProgress(false)
  , m_scanProgress(0.0f) {

  // Initialize empty heatmap
  ResetHeatmapData();
}

void GridScannerUI::SetScanner(std::shared_ptr<GridScanner> scanner) {
  m_scanner = scanner;
}

void GridScannerUI::SetDeviceInfo(const std::string& deviceName, const std::string& dataChannel) {
  m_deviceName = deviceName;
  m_dataChannel = dataChannel;
}

void GridScannerUI::ResetHeatmapData() {
  m_heatmapData.clear();
  m_heatmapData.resize(m_yPoints, std::vector<double>(m_xPoints,
    std::numeric_limits<double>::quiet_NaN()));
  m_dataUpdated = true;
}

void GridScannerUI::Render() {
  if (!m_visible) return;

  ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Grid Scanner", &m_visible)) {
    ImGui::End();
    return;
  }

  // Update scan state
  if (m_scanner) {
    m_scanInProgress = m_scanner->IsScanActive();
    m_scanProgress = static_cast<float>(m_scanner->GetProgress());
  }

  // Left panel - Controls
  ImGui::BeginChild("Controls", ImVec2(300, 0), true);
  RenderControls();
  ImGui::EndChild();

  ImGui::SameLine();

  // Right panel - Heatmap
  ImGui::BeginChild("HeatmapPanel", ImVec2(0, 0), true);
  RenderHeatmap();
  ImGui::EndChild();

  ImGui::End();

  // Render results dialog if needed
  if (m_showResultsDialog) {
    RenderResultsDialog();
  }
}


// Add new method to GridScannerUI
void GridScannerUI::RenderResultsDialog() {
  ImGui::OpenPopup("Scan Results");

  // Center the dialog
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal("Scan Results", &m_showResultsDialog,
    ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Scan Completed Successfully!");
    ImGui::Separator();

    // Peak value info
    ImGui::Text("Peak Value:");
    ImGui::Indent();
    ImGui::Text("Value: %.6f", m_lastPeakValue);
    ImGui::Text("Grid Position: [%d, %d]", m_lastPeakPosition.col, m_lastPeakPosition.row);
    ImGui::Text("Physical Position:");
    ImGui::Text("  X: %.3f mm", m_lastPeakPosition.x);
    ImGui::Text("  Y: %.3f mm", m_lastPeakPosition.y);
    ImGui::Unindent();

    ImGui::Separator();

    // Relative position from origin
    if (m_scanner) {
      double relX = (m_lastPeakPosition.col * m_xStep);
      double relY = -(m_lastPeakPosition.row * m_yStep);
      ImGui::Text("Relative from scan origin:");
      ImGui::Text("  X: %.1f µm", relX);
      ImGui::Text("  Y: %.1f µm", relY);
    }

    ImGui::Separator();

    // Buttons
    if (ImGui::Button("Move to Peak", ImVec2(120, 0))) {
      if (m_scanner && m_scanner->IsMotionControllerConnected()) {
        // Move to peak position
        if (m_piManager) {
          PIController* controller = m_piManager->GetController(m_deviceName);
          if (controller) {
            controller->MoveToPositionMultiAxis(
              { "X", "Y" },
              { m_lastPeakPosition.x, m_lastPeakPosition.y },
              true);
          }
        }
      }
    }

    ImGui::SameLine();

    if (ImGui::Button("OK", ImVec2(120, 0))) {
      m_showResultsDialog = false;
    }

    ImGui::EndPopup();
  }
}


// In grid_scanner_ui.cpp - Complete RenderControls() method
void GridScannerUI::RenderControls() {
  ImGui::Text("Grid Scanner Control");
  ImGui::Separator();

  // NEW: Device Selection Section
  if (!m_availableDevices.empty()) {
    ImGui::Text("Device Selection");
    ImGui::Separator();

    // Convert vector to array of C strings for ImGui
    std::vector<const char*> deviceNames;
    for (const auto& device : m_availableDevices) {
      deviceNames.push_back(device.c_str());
    }

    // Device dropdown
    int previousIndex = m_selectedDeviceIndex;
    if (ImGui::Combo("Motion Device", &m_selectedDeviceIndex,
      deviceNames.data(), static_cast<int>(deviceNames.size()))) {
      // Device changed
      if (m_selectedDeviceIndex != previousIndex &&
        m_selectedDeviceIndex >= 0 &&
        m_selectedDeviceIndex < static_cast<int>(m_availableDevices.size())) {

        std::string newDevice = m_availableDevices[m_selectedDeviceIndex];
        Logger::GetInstance()->LogInfo("GridScannerUI: Device changed to " + newDevice);

        // Update device name
        m_deviceName = newDevice;

        // Notify manager of change
        if (m_deviceChangeCallback) {
          m_deviceChangeCallback(newDevice);
        }
      }
    }

    ImGui::Spacing();
  }

  // Connection Status Section
  ImGui::Text("Connection Status");
  ImGui::Separator();

  // Check hardware status
  bool motionConnected = m_scanner && m_scanner->IsMotionControllerConnected();
  bool dataConnected = m_scanner && m_scanner->IsDataChannelConnected();

  // Motion controller status
  if (motionConnected) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ Motion: %s", m_deviceName.c_str());
  }
  else {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "✗ Motion: %s (Not Connected)", m_deviceName.c_str());
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Waiting for %s controller to connect", m_deviceName.c_str());
    }
  }

  // Data channel status
  if (dataConnected) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ Data: %s", m_dataChannel.c_str());
  }
  else {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "✗ Data: %s (No Data)", m_dataChannel.c_str());
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Waiting for data on %s channel", m_dataChannel.c_str());
    }
  }

  // Add reconnect button if not connected
  if (!motionConnected || !dataConnected) {
    if (ImGui::Button("Check Connections", ImVec2(-1, 25))) {
      CheckAndReconnectHardware();
    }
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Grid Parameters");
  ImGui::Separator();

  // Grid configuration (disabled during scan)
  ImGui::BeginDisabled(m_scanInProgress);

  if (ImGui::DragFloat("X Step (µm)", &m_xStep, 1.0f, 1.0f, 1000.0f, "%.1f")) {
    if (!m_scanInProgress && m_scanner) {
      m_scanner->SetGridParameters(m_xStep, m_yStep, m_xPoints, m_yPoints);
    }
  }

  if (ImGui::DragFloat("Y Step (µm)", &m_yStep, 1.0f, 1.0f, 1000.0f, "%.1f")) {
    if (!m_scanInProgress && m_scanner) {
      m_scanner->SetGridParameters(m_xStep, m_yStep, m_xPoints, m_yPoints);
    }
  }

  if (ImGui::DragInt("X Points", &m_xPoints, 1, 2, 20)) {
    if (!m_scanInProgress) {
      ResetHeatmapData();
      if (m_scanner) {
        m_scanner->SetGridParameters(m_xStep, m_yStep, m_xPoints, m_yPoints);
      }
    }
  }

  if (ImGui::DragInt("Y Points", &m_yPoints, 1, 2, 20)) {
    if (!m_scanInProgress) {
      ResetHeatmapData();
      if (m_scanner) {
        m_scanner->SetGridParameters(m_xStep, m_yStep, m_xPoints, m_yPoints);
      }
    }
  }

  if (ImGui::DragInt("Settling Time (ms)", &m_settlingTime, 10, 0, 5000)) {
    if (m_scanner) {
      m_scanner->SetSettlingTime(m_settlingTime);
    }
  }

  ImGui::EndDisabled();

  // Total scan info
  ImGui::Spacing();
  int totalPoints = m_xPoints * m_yPoints;
  float totalDistanceX = (m_xPoints - 1) * m_xStep;
  float totalDistanceY = (m_yPoints - 1) * m_yStep;

  ImGui::Text("Total Points: %d", totalPoints);
  ImGui::Text("Scan Area: %.1f × %.1f µm", totalDistanceX, totalDistanceY);

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Scan Control");
  ImGui::Separator();

  // Scan control buttons
  bool canScan = motionConnected && dataConnected;

  if (m_scanner) {
    if (!m_scanInProgress) {
      ImGui::BeginDisabled(!canScan);

      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0.6f, 0, 1));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0.8f, 0, 1));

      // In grid_scanner_ui.cpp - Update the Start Scan button handler
      if (ImGui::Button("Start Scan", ImVec2(-1, 35))) {
        // Reset heatmap
        ResetHeatmapData();

        // Make sure any previous scan is stopped
        if (m_scanner->IsScanActive()) {
          m_scanner->StopScan();
          std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        // Start scan with update callback
        bool started = m_scanner->StartScan([this](const GridPoint& point, double value) {
          UpdateHeatmap(point, value);
        });

        if (started) {
          Logger::GetInstance()->LogInfo("GridScannerUI: Scan started");
        }
        else {
          Logger::GetInstance()->LogError("GridScannerUI: Failed to start scan");
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

      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0, 1));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 0.3f, 0, 1));

      if (ImGui::Button("Stop Scan", ImVec2(-1, 35))) {
        m_scanner->StopScan();
        Logger::GetInstance()->LogInfo("GridScannerUI: Scan stopped by user");
      }

      ImGui::PopStyleColor(2);
    }
  }
  else {
    ImGui::TextWrapped("No scanner connected");
  }

  // Status display
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Status");
  ImGui::Separator();

  if (m_scanInProgress) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "● Scanning");

    if (m_scanner) {
      GridPoint pos = m_scanner->GetCurrentPosition();
      ImGui::Text("Grid Position: [%d, %d]", pos.col, pos.row);
      ImGui::Text("Physical Pos: (%.1f, %.1f) µm",
        pos.col * m_xStep, -pos.row * m_yStep);
      ImGui::Text("Current Value: %.6f", m_scanner->GetLastValue());
    }
  }
  else {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "● Idle");
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

  // Colormap selection
  const char* colormaps[] = { "Viridis", "Plasma", "Hot", "Cool", "Pink", "Jet", "Twilight", "RdBu", "BrBG", "PiYG" };
  ImGui::Combo("Colormap", &m_colormapIndex, colormaps, IM_ARRAYSIZE(colormaps));
}


void GridScannerUI::RenderHeatmap() {
  ImGui::Text("Real-time Scan Heatmap");
  ImGui::Separator();

  // Convert colormap index to ImPlot colormap
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

  // Update heatmap data from scanner if needed
  if (m_scanner && m_dataUpdated) {
    m_heatmapData = m_scanner->GetDataGrid();
    m_dataUpdated = false;
  }

  // IMPORTANT: Validate we have data before trying to render
  if (m_heatmapData.empty() || m_xPoints <= 0 || m_yPoints <= 0) {
    ImGui::Text("No data to display");
    return;
  }

  // Calculate data range for color scaling
  double dataMin = (std::numeric_limits<double>::max)();
  double dataMax = std::numeric_limits<double>::lowest();
  bool hasValidData = false;

  for (const auto& row : m_heatmapData) {
    for (double val : row) {
      if (!std::isnan(val) && !std::isinf(val)) {
        dataMin = (std::min)(dataMin, val);
        dataMax = (std::max)(dataMax, val);
        hasValidData = true;
      }
    }
  }

  // If no valid data, show message
  if (!hasValidData) {
    ImGui::Text("Waiting for scan data...");
    return;
  }

  // Auto scale if enabled
  if (m_autoScale && dataMin < dataMax) {
    m_colorScaleMin = static_cast<float>(dataMin);
    m_colorScaleMax = static_cast<float>(dataMax);
  }

  // Ensure scale range is valid
  if (m_colorScaleMin >= m_colorScaleMax) {
    m_colorScaleMax = m_colorScaleMin + 1.0f;
  }

  // Create flat array for ImPlot - FIXED ORDER
  std::vector<double> flatData;
  flatData.reserve(m_yPoints * m_xPoints);

  // ImPlot expects data from bottom-left, but our grid starts from top
  // So we need to reverse the row order
  for (int row = m_yPoints - 1; row >= 0; --row) {  // Start from bottom row
    for (int col = 0; col < m_xPoints; ++col) {
      double val = m_heatmapData[row][col];
      if (std::isnan(val) || std::isinf(val)) {
        flatData.push_back(m_colorScaleMin);
      }
      else {
        flatData.push_back(val);
      }
    }
  }

  // Validate flat data size
  if (flatData.size() != static_cast<size_t>(m_yPoints * m_xPoints)) {
    ImGui::Text("Data size mismatch: expected %d, got %zu",
      m_yPoints * m_xPoints, flatData.size());
    return;
  }

  // Setup plot
  if (ImPlot::BeginPlot("##Heatmap", ImVec2(-1, -1))) {
    ImPlot::PushColormap(colormap);

    // Setup axes - keep display in µm for clarity
    ImPlot::SetupAxes("X Position (µm)", "Y Position (µm)");

    // Set axis limits centered around origin
    double xExtent = ((m_xPoints - 1) * m_xStep) / 2.0;
    double yExtent = ((m_yPoints - 1) * m_yStep) / 2.0;

    ImPlot::SetupAxisLimits(ImAxis_X1, -xExtent - m_xStep / 2, xExtent + m_xStep / 2, ImPlotCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, -yExtent - m_yStep / 2, yExtent + m_yStep / 2, ImPlotCond_Always);

    // Plot heatmap with corrected bounds for centered grid
    ImPlot::PlotHeatmap("Scan Data",
      flatData.data(),
      m_yPoints,  // rows
      m_xPoints,  // cols
      m_colorScaleMin,
      m_colorScaleMax,
      "",  // no label format
      ImPlotPoint(-xExtent, -yExtent),  // bounds min (bottom-left of centered grid)
      ImPlotPoint(xExtent, yExtent));    // bounds max (top-right of centered grid)

    // Draw current position marker if scanning
    if (m_scanInProgress && m_scanner) {
      GridPoint pos = m_scanner->GetCurrentPosition();

      // Validate position is within bounds
      if (pos.col >= 0 && pos.col < m_xPoints &&
        pos.row >= 0 && pos.row < m_yPoints) {

        // Calculate marker position for centered grid
        // Map grid indices to centered coordinates
        double markerX = -xExtent + (pos.col * m_xStep);
        double markerY = yExtent - (pos.row * m_yStep);  // Flip Y for display

        // Draw crosshair at current position
        ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1, 0, 0, 1));
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.0f);

        double xCross[] = { markerX - m_xStep / 4, markerX + m_xStep / 4 };
        double yCross[] = { markerY, markerY };
        ImPlot::PlotLine("##CrossX", xCross, yCross, 2);

        double xCross2[] = { markerX, markerX };
        double yCross2[] = { markerY - m_yStep / 4, markerY + m_yStep / 4 };
        ImPlot::PlotLine("##CrossY", xCross2, yCross2, 2);

        ImPlot::PopStyleVar();
        ImPlot::PopStyleColor();
      }
    }

    ImPlot::PopColormap();
    ImPlot::EndPlot();
  }

  // Show colorbar scale values
  ImGui::Text("Scale: %.3e to %.3e", m_colorScaleMin, m_colorScaleMax);
}
// In grid_scanner_ui.cpp - Modify UpdateHeatmap to detect scan completion
void GridScannerUI::UpdateHeatmap(const GridPoint& point, double value) {
  // Update heatmap data
  if (point.row >= 0 && point.row < m_yPoints &&
    point.col >= 0 && point.col < m_xPoints) {
    m_heatmapData[point.row][point.col] = value;
    m_dataUpdated = true;
  }

  // Update current position and value
  m_currentPosition = point;
  m_currentValue = value;

  // Check if scan completed (last point)
  if (m_scanner) {
    double progress = m_scanner->GetProgress();
    if (progress >= 1.0 && !m_showResultsDialog) {
      // Get peak data
      if (m_scanner->HasValidPeak()) {
        m_lastPeakValue = m_scanner->GetPeakValue();
        m_lastPeakPosition = m_scanner->GetPeakPosition();
        m_showResultsDialog = true;

        Logger::GetInstance()->LogInfo("Scan complete. Peak value: " +
          std::to_string(m_lastPeakValue));
      }
    }
  }
}


// In grid_scanner_ui.cpp - Update CheckAndReconnectHardware
void GridScannerUI::CheckAndReconnectHardware() {
  if (!m_scanner) return;

  Logger::GetInstance()->LogInfo("GridScannerUI: Checking hardware connections...");

  // Check motion controller
  if (!m_scanner->IsMotionControllerConnected() && m_piManager) {
    PIController* controller = m_piManager->GetController(m_deviceName);
    if (controller && controller->IsConnected()) {
      auto motionAdapter = std::make_shared<PIScanMotionAdapter>(
        controller, m_deviceName);
      m_scanner->SetMotionController(motionAdapter);
      Logger::GetInstance()->LogInfo("GridScannerUI: Reconnected to " + m_deviceName);
    }
    else {
      Logger::GetInstance()->LogWarning("GridScannerUI: " + m_deviceName + " not available");
    }
  }

  // Check data channel in GlobalDataStore
  GlobalDataStore* dataStore = GlobalDataStore::GetInstance();
  if (dataStore) {
    auto channels = dataStore->GetAvailableChannels();
    bool channelFound = false;
    std::string foundChannelName;

    // Look for GPIB-Current with various possible formats
    for (const auto& channel : channels) {
      // Check various possible formats
      if (channel == m_dataChannel ||                    // Exact match "GPIB-Current"
        channel == "(" + m_dataChannel + ")" ||        // With parentheses "(GPIB-Current)"
        channel.find("GPIB") != std::string::npos ||   // Contains "GPIB"
        channel.find("Current") != std::string::npos)  // Contains "Current"
      {
        channelFound = true;
        foundChannelName = channel;
        Logger::GetInstance()->LogInfo("GridScannerUI: Found matching channel: " + channel);

        // Update the scanner to use the actual channel name
        if (!m_scanner->IsDataChannelConnected() && m_dataManager) {
          m_scanner->SetDataChannel(channel);  // Use the actual channel name found
          Logger::GetInstance()->LogInfo("GridScannerUI: Subscribed to channel: " + channel);
        }
        break;
      }
    }

    if (!channelFound) {
      Logger::GetInstance()->LogWarning("GridScannerUI: Channel " + m_dataChannel + " not found in GlobalDataStore");
      Logger::GetInstance()->LogInfo("Available channels (" + std::to_string(channels.size()) + " total):");

      // Log ALL channels for debugging
      int count = 0;
      for (const auto& ch : channels) {
        Logger::GetInstance()->LogInfo("  [" + std::to_string(count++) + "] " + ch);
        if (count >= 20) {  // Limit output to first 20 channels
          Logger::GetInstance()->LogInfo("  ... and " + std::to_string(channels.size() - 20) + " more");
          break;
        }
      }
    }
  }
}

void GridScannerUI::SetAvailableDevices(const std::vector<std::string>& devices) {
  m_availableDevices = devices;

  // Find current device in list
  m_selectedDeviceIndex = 0;
  for (size_t i = 0; i < devices.size(); ++i) {
    if (devices[i] == m_deviceName) {
      m_selectedDeviceIndex = static_cast<int>(i);
      break;
    }
  }
}