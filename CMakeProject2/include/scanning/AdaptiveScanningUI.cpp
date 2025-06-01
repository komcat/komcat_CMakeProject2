// AdaptiveScanningUI.cpp
#include "AdaptiveScanningUI.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>

AdaptiveScanningUI::AdaptiveScanningUI(PIControllerManager& piControllerManager,
  GlobalDataStore& dataStore)
  : m_piControllerManager(piControllerManager),
  m_dataStore(dataStore)
{
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("AdaptiveScanningUI: Initializing adaptive power scanner interface");

  RefreshAvailableDevices();
  RefreshAvailableDataChannels();

  // Initialize with default configuration
  UpdateScanConfig();
  m_scanner = std::make_unique<AdaptivePowerScanner>(m_scanConfig);

  // Set up hardware interface
  m_scanner->setMeasurementFunction([this](double x, double y, double z) -> double {
    return PerformMeasurement(x, y, z);
  });

  m_scanner->setPositionValidationFunction([this](double x, double y, double z) -> bool {
    return IsPositionValid(x, y, z);
  });
}

AdaptiveScanningUI::~AdaptiveScanningUI() {
  // Ensure scanning is stopped before destruction
  if (m_isScanning.load()) {
    m_logger->LogInfo("AdaptiveScanningUI: Stopping scan during destruction");
    StopScan();

    // Give thread time to finish (max 2 seconds)
    int waitCount = 0;
    while (m_isScanning.load() && waitCount < 20) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      waitCount++;
    }

    if (m_isScanning.load()) {
      m_logger->LogWarning("AdaptiveScanningUI: Scan thread did not stop within timeout");
    }
  }

  m_logger->LogInfo("AdaptiveScanningUI: Shutting down");
}

void AdaptiveScanningUI::RenderUI() {
  if (!m_showWindow) return;

  ImGui::Begin("Adaptive Power Scanner", &m_showWindow);

  RenderDeviceSelection();
  ImGui::Separator();

  RenderConfiguration();
  ImGui::Separator();

  RenderScanControls();
  ImGui::Separator();

  RenderScanStatus();
  ImGui::Separator();

  RenderResults();

  ImGui::End();
}

void AdaptiveScanningUI::RenderDeviceSelection() {
  ImGui::Text("Select Hexapod Device");

  if (ImGui::BeginCombo("Hexapod", m_selectedDevice.c_str())) {
    for (const auto& device : m_hexapodDevices) {
      PIController* controller = m_piControllerManager.GetController(device);
      bool deviceAvailable = (controller && controller->IsConnected());

      if (!deviceAvailable) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
      }

      bool isSelected = (device == m_selectedDevice);
      if (ImGui::Selectable(device.c_str(), isSelected) && deviceAvailable) {
        m_selectedDevice = device;
        m_logger->LogInfo("AdaptiveScanningUI: Selected device: " + m_selectedDevice);
        RefreshAvailableDataChannels();
      }

      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }

      if (!deviceAvailable) {
        ImGui::PopStyleColor();
      }
    }
    ImGui::EndCombo();
  }

  // Show connection status
  PIController* controller = GetSelectedController();
  if (controller) {
    bool isConnected = controller->IsConnected();
    ImGui::TextColored(
      isConnected ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
      "Status: %s", isConnected ? "Connected" : "Disconnected"
    );
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No controller selected");
  }

  // Data channel selection
  if (!m_availableDataChannels.empty()) {
    if (ImGui::BeginCombo("Data Channel", m_selectedDataChannel.c_str())) {
      for (const auto& channel : m_availableDataChannels) {
        bool isSelected = (channel == m_selectedDataChannel);
        if (ImGui::Selectable(channel.c_str(), isSelected)) {
          m_selectedDataChannel = channel;
        }
        if (isSelected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }

    // Display current value
    if (m_dataStore.HasValue(m_selectedDataChannel)) {
      double currentValue = m_dataStore.GetValue(m_selectedDataChannel);
      ImGui::Text("Current Value: %.3f μA", currentValue * 1e6);
    }
  }
}

void AdaptiveScanningUI::RenderConfiguration() {
  ImGui::Text("Adaptive Scanning Configuration");

  if (ImGui::CollapsingHeader("Power & Step Size Mapping", ImGuiTreeNodeFlags_DefaultOpen)) {
    // Power range configuration
    ImGui::Text("Power Range:");
    bool configChanged = false;

    configChanged |= ImGui::SliderFloat("Min Power (μA)", &m_uiConfig.minPowerUA, 0.1f, 50.0f, "%.1f");
    configChanged |= ImGui::SliderFloat("Max Power (μA)", &m_uiConfig.maxPowerUA, 50.0f, 1000.0f, "%.0f");

    ImGui::Spacing();

    // Step size range
    ImGui::Text("Step Size Range:");
    configChanged |= ImGui::SliderFloat("Min Step (μm)", &m_uiConfig.minStepMicrons, 0.01f, 1.0f, "%.2f");
    configChanged |= ImGui::SliderFloat("Max Step (μm)", &m_uiConfig.maxStepMicrons, 1.0f, 50.0f, "%.1f");

    // Show adaptive step size examples
    ImGui::Spacing();
    ImGui::Text("Adaptive Step Size Examples:");
    ImGui::Text("  At %.1f μA: ~%.1f μm steps (large, fast movement)",
      m_uiConfig.minPowerUA, m_uiConfig.maxStepMicrons);
    ImGui::Text("  At %.0f μA: ~%.2f μm steps (small, precise)",
      m_uiConfig.maxPowerUA, m_uiConfig.minStepMicrons);

    if (configChanged) {
      UpdateScanConfig();
    }
  }

  if (ImGui::CollapsingHeader("Physics Constraints")) {
    bool configChanged = false;

    configChanged |= ImGui::Checkbox("Enable Physics Constraints", &m_uiConfig.enablePhysicsConstraints);

    if (m_uiConfig.enablePhysicsConstraints) {
      ImGui::Text("Z-axis Direction (for power increase):");
      const char* zDirections[] = { "Both Directions", "Negative Only (Z-)", "Positive Only (Z+)" };
      configChanged |= ImGui::Combo("##ZDirection", &m_uiConfig.zDirection, zDirections, 3);

      ImGui::Text("XY-axis Direction:");
      const char* xyDirections[] = { "Both Directions", "Negative Only", "Positive Only" };
      configChanged |= ImGui::Combo("##XYDirection", &m_uiConfig.xyDirection, xyDirections, 3);

      configChanged |= ImGui::SliderFloat("Max Travel (mm)", &m_uiConfig.maxTravelMM, 1.0f, 20.0f, "%.1f");

      // Show current physics setup
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "Current Setup:");
      const char* zDirText[] = { "Both", "Z- only", "Z+ only" };
      const char* xyDirText[] = { "Both", "Negative only", "Positive only" };
      ImGui::Text("  Z-axis: %s", zDirText[m_uiConfig.zDirection]);
      ImGui::Text("  XY-axes: %s", xyDirText[m_uiConfig.xyDirection]);
    }

    if (configChanged) {
      UpdateScanConfig();
    }
  }

  if (ImGui::CollapsingHeader("Algorithm Parameters")) {
    bool configChanged = false;

    configChanged |= ImGui::Checkbox("Enable Adaptive Step Sizing", &m_uiConfig.enableAdaptiveSteps);
    configChanged |= ImGui::SliderFloat("Improvement Threshold (%)", &m_uiConfig.improvementThreshold, 0.01f, 5.0f, "%.2f");

    ImGui::TextWrapped("Lower threshold = more precise but slower convergence");

    if (configChanged) {
      UpdateScanConfig();
    }
  }

  // Apply configuration button
  if (ImGui::Button("Apply All Configuration Changes")) {
    UpdateScanConfig();

    // Recreate scanner with new configuration
    m_scanner = std::make_unique<AdaptivePowerScanner>(m_scanConfig);

    // Re-setup hardware interface
    m_scanner->setMeasurementFunction([this](double x, double y, double z) -> double {
      return PerformMeasurement(x, y, z);
    });
    m_scanner->setPositionValidationFunction([this](double x, double y, double z) -> bool {
      return IsPositionValid(x, y, z);
    });

    m_logger->LogInfo("AdaptiveScanningUI: Configuration updated and applied");
  }
}

void AdaptiveScanningUI::RenderScanControls() {
  ImGui::Text("Scan Controls");

  bool canStartScan = !m_selectedDevice.empty() &&
    !m_selectedDataChannel.empty() &&
    GetSelectedController() != nullptr &&
    GetSelectedController()->IsConnected();

  bool isScanningNow = m_isScanning.load();

  // Status indicators
  if (!canStartScan) {
    if (m_selectedDevice.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Select a hexapod device first");
    }
    else if (GetSelectedController() == nullptr || !GetSelectedController()->IsConnected()) {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Controller not connected");
    }
    else if (m_selectedDataChannel.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Select a data channel");
    }
  }
  else {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Ready for adaptive scan");
  }

  // Current power reading and step size preview
  if (m_dataStore.HasValue(m_selectedDataChannel) && m_scanner) {
    double currentPower = m_dataStore.GetValue(m_selectedDataChannel);
    ImGui::Text("Current Power: %.3f μA", currentPower * 1e6);

    // Show what step size would be used at current power level
    if (currentPower >= m_uiConfig.minPowerUA * 1e-6) {
      // Create a temporary scanner to calculate step size without affecting the main one
      AdaptivePowerScanner tempScanner(m_scanConfig);
      double estimatedStepSize = tempScanner.calculateStepSize(currentPower) * 1e6; // Convert to microns
      ImGui::Text("→ Current adaptive step size: %.2f μm", estimatedStepSize);

      // Color-code the step size
      if (estimatedStepSize > 5.0) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "(Fast movement)");
      }
      else if (estimatedStepSize < 1.0) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.7f, 1.0f), "(Precise positioning)");
      }
      else {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "(Moderate movement)");
      }
    }
  }

  ImGui::Spacing();

  // Start/Stop buttons
  ImGui::BeginGroup();

  if (!isScanningNow && canStartScan) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.7f, 0.1f, 1.0f));

    if (ImGui::Button("Start Adaptive Scan", ImVec2(200, 40))) {
      StartScan();
    }

    ImGui::PopStyleColor(3);
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 0.6f));
    ImGui::Button("Start Adaptive Scan", ImVec2(200, 40));
    ImGui::PopStyleColor(2);
  }

  ImGui::SameLine();

  if (isScanningNow) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));

    if (ImGui::Button("Stop Scan", ImVec2(150, 40))) {
      StopScan();
    }

    ImGui::PopStyleColor(3);
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 0.6f));
    ImGui::Button("Stop Scan", ImVec2(150, 40));
    ImGui::PopStyleColor(2);
  }

  ImGui::EndGroup();

  ImGui::Spacing();
  ImGui::TextWrapped("Adaptive scanner automatically adjusts step size based on power reading. Lower power = larger steps for faster movement. Higher power = smaller steps for precise positioning.");
}

void AdaptiveScanningUI::RenderScanStatus() {
  float progressBarValue = static_cast<float>(m_scanProgress.load());

  // Get status string safely
  std::string statusCopy;
  {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    statusCopy = m_scanStatus;
  }

  ImGui::ProgressBar(progressBarValue, ImVec2(-1, 0), statusCopy.c_str());

  // Display current measurement
  ImGui::Text("Current: %.3f μA", m_currentValue.load() * 1e6);

  // Display peak information if available
  double peakVal = m_peakValue.load();
  if (peakVal > 0) {
    ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "Best Value: %.3f μA", peakVal * 1e6);

    // Get peak position safely
    ScanStep peakPosCopy;
    {
      std::lock_guard<std::mutex> lock(m_dataMutex);
      peakPosCopy = m_peakPosition;
    }

    ImGui::Text("Best Position: %s", FormatPosition(peakPosCopy).c_str());

    // Show improvement if we have scan history
    if (!m_scanHistory.empty() && m_scanHistory[0].value > 0) {
      double improvement = (peakVal - m_scanHistory[0].value) / m_scanHistory[0].value * 100.0;
      if (improvement > 0) {
        ImGui::Text("Improvement: +%.2f%%", improvement);
      }
    }
  }
}

void AdaptiveScanningUI::RenderResults() {
  if (m_scanHistory.empty()) {
    ImGui::Text("No scan results yet");
    return;
  }

  ImGui::Text("Scan Results:");
  ImGui::Text("Total measurements: %d", static_cast<int>(m_scanHistory.size()));

  // Count measurements per axis
  std::map<std::string, int> axisCounts;
  std::map<std::string, double> avgStepSizes;
  std::map<std::string, std::vector<double>> stepSizesByAxis;

  for (const auto& step : m_scanHistory) {
    if (!step.axis.empty()) {
      axisCounts[step.axis]++;
      if (step.stepSize > 0) {
        stepSizesByAxis[step.axis].push_back(step.stepSize * 1e6); // Convert to microns
      }
    }
  }

  // Calculate average step sizes
  for (const auto& [axis, sizes] : stepSizesByAxis) {
    if (!sizes.empty()) {
      double sum = 0;
      for (double size : sizes) sum += size;
      avgStepSizes[axis] = sum / sizes.size();
    }
  }

  ImGui::Text("Measurements per axis:");
  for (const auto& [axis, count] : axisCounts) {
    double percentage = (100.0 * count) / m_scanHistory.size();
    ImGui::Text("  %s: %d (%.1f%%) - Avg step: %.2f μm",
      axis.c_str(), count, percentage,
      avgStepSizes.count(axis) ? avgStepSizes[axis] : 0.0);
  }

  // Total improvement calculation
  if (m_peakValue.load() > 0 && !m_scanHistory.empty() && m_scanHistory[0].value > 0) {
    double totalImprovement = (m_peakValue.load() - m_scanHistory[0].value) / m_scanHistory[0].value * 100.0;
    ImGui::Text("Total improvement: %.2f%%", totalImprovement);
  }

  // Show step size adaptation effectiveness
  if (!stepSizesByAxis.empty()) {
    ImGui::Spacing();
    ImGui::Text("Step Size Adaptation:");
    for (const auto& [axis, sizes] : stepSizesByAxis) {
      if (sizes.size() >= 2) {
        double minStep = *std::min_element(sizes.begin(), sizes.end());
        double maxStep = *std::max_element(sizes.begin(), sizes.end());
        ImGui::Text("  %s: %.2f - %.2f μm range", axis.c_str(), minStep, maxStep);
      }
    }
  }
}

void AdaptiveScanningUI::StartScan() {
  if (m_isScanning.load()) {
    m_logger->LogWarning("AdaptiveScanningUI: Scan already in progress");
    return;
  }

  if (m_selectedDevice.empty() || m_selectedDataChannel.empty()) {
    m_logger->LogError("AdaptiveScanningUI: Cannot start scan - missing device or data channel");
    return;
  }

  PIController* controller = GetSelectedController();
  if (!controller || !controller->IsConnected()) {
    m_logger->LogError("AdaptiveScanningUI: Cannot start scan - controller not connected");
    return;
  }

  try {
    // Set scanning state atomically BEFORE starting thread
    m_isScanning.store(true);
    m_scanProgress.store(0.0);

    // Clear previous results
    {
      std::lock_guard<std::mutex> lock(m_dataMutex);
      m_scanHistory.clear();
      m_peakPosition = ScanStep();
    }

    // Update status
    {
      std::lock_guard<std::mutex> lock(m_statusMutex);
      m_scanStatus = "Preparing adaptive scan...";
    }

    // Get starting position
    ScanStep startPosition;
    try {
      double x, y, z;
      bool gotX = controller->GetPosition("X", x);
      bool gotY = controller->GetPosition("Y", y);
      bool gotZ = controller->GetPosition("Z", z);

      if (gotX && gotY && gotZ) {
        startPosition.x = x;
        startPosition.y = y;
        startPosition.z = z;
        m_logger->LogInfo("AdaptiveScanningUI: Starting from current position (" +
          std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")");
      }
      else {
        startPosition.x = 0.0;
        startPosition.y = 0.0;
        startPosition.z = 0.0;
        m_logger->LogWarning("AdaptiveScanningUI: Could not read current position, using (0,0,0)");
      }
    }
    catch (const std::exception& e) {
      startPosition.x = 0.0;
      startPosition.y = 0.0;
      startPosition.z = 0.0;
      m_logger->LogWarning("AdaptiveScanningUI: Exception reading position, using (0,0,0) - " + std::string(e.what()));
    }

    // Get initial measurement
    {
      std::lock_guard<std::mutex> lock(m_statusMutex);
      m_scanStatus = "Taking initial measurement...";
    }

    startPosition.value = PerformMeasurement(startPosition.x, startPosition.y, startPosition.z);

    if (startPosition.value <= 0.0) {
      m_logger->LogError("AdaptiveScanningUI: Could not get valid initial measurement");
      m_isScanning.store(false);
      {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_scanStatus = "Failed to get initial measurement";
      }
      return;
    }

    m_logger->LogInfo("AdaptiveScanningUI: Initial measurement: " + std::to_string(startPosition.value));

    // Start the scan thread
    std::thread scanThread([this, startPosition]() {
      try {
        {
          std::lock_guard<std::mutex> lock(m_statusMutex);
          m_scanStatus = "Running adaptive power scan...";
        }

        // Run the actual adaptive scan
        ScanStep finalPosition = m_scanner->adaptivePowerScan(startPosition);

        // Check if scan was cancelled during execution
        if (!m_isScanning.load()) {
          {
            std::lock_guard<std::mutex> lock(m_statusMutex);
            m_scanStatus = "Scan cancelled";
          }
          return;
        }

        // Update results
        {
          std::lock_guard<std::mutex> lock(m_dataMutex);
          m_scanHistory = m_scanner->getScanHistory();
          m_peakPosition = finalPosition;
        }

        // Update peak value and progress
        m_peakValue.store(finalPosition.value);
        m_scanProgress.store(1.0);

        // Update final status
        {
          std::lock_guard<std::mutex> lock(m_statusMutex);
          m_scanStatus = "Adaptive scan completed successfully";
        }

        m_logger->LogInfo("AdaptiveScanningUI: Adaptive scan completed. Total measurements: " +
          std::to_string(m_scanner->getTotalMeasurements()) +
          ", Final value: " + std::to_string(finalPosition.value));

      }
      catch (const std::exception& e) {
        // Handle scan errors
        {
          std::lock_guard<std::mutex> lock(m_statusMutex);
          m_scanStatus = "Error: " + std::string(e.what());
        }

        m_logger->LogError("AdaptiveScanningUI: Scan error - " + std::string(e.what()));
      }

      // Always set scanning to false when thread completes
      m_isScanning.store(false);
    });

    // Detach thread so it runs independently
    scanThread.detach();

  }
  catch (const std::exception& e) {
    // Handle startup errors
    m_isScanning.store(false);
    {
      std::lock_guard<std::mutex> lock(m_statusMutex);
      m_scanStatus = "Startup error: " + std::string(e.what());
    }
    m_logger->LogError("AdaptiveScanningUI: Exception while starting scan - " + std::string(e.what()));
  }
}

void AdaptiveScanningUI::StopScan() {
  if (!m_isScanning.load()) {
    return;
  }

  m_logger->LogInfo("AdaptiveScanningUI: Stopping adaptive scan");

  // Set atomic flag to stop scan
  m_isScanning.store(false);

  // Update status
  {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_scanStatus = "Stopping scan...";
  }

  // Stop all controller movement immediately
  PIController* controller = GetSelectedController();
  if (controller && controller->IsConnected()) {
    try {
      controller->StopAllAxes();
      m_logger->LogInfo("AdaptiveScanningUI: Stopped all axes");
    }
    catch (const std::exception& e) {
      m_logger->LogError("AdaptiveScanningUI: Error stopping axes - " + std::string(e.what()));
    }
  }

  // Update final status
  {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_scanStatus = "Scan stopped by user";
  }
}

void AdaptiveScanningUI::UpdateScanConfig() {
  // Update power mapping
  m_scanConfig.powerMapping.minPower = m_uiConfig.minPowerUA * 1e-6;  // Convert to Amps
  m_scanConfig.powerMapping.maxPower = m_uiConfig.maxPowerUA * 1e-6;
  m_scanConfig.powerMapping.minStepSize = m_uiConfig.minStepMicrons * 1e-6;  // Convert to mm
  m_scanConfig.powerMapping.maxStepSize = m_uiConfig.maxStepMicrons * 1e-6;

  // Update direction constraints
  std::vector<std::string> directionOptions = { "Both", "Negative", "Positive" };

  m_scanConfig.directionConstraints.forcedDirection["Z"] = directionOptions[m_uiConfig.zDirection];
  m_scanConfig.directionConstraints.forcedDirection["X"] = directionOptions[m_uiConfig.xyDirection];
  m_scanConfig.directionConstraints.forcedDirection["Y"] = directionOptions[m_uiConfig.xyDirection];

  // Update travel limits
  m_scanConfig.directionConstraints.maxTravel["X"] = m_uiConfig.maxTravelMM * 1e-3;  // Convert to mm
  m_scanConfig.directionConstraints.maxTravel["Y"] = m_uiConfig.maxTravelMM * 1e-3;
  m_scanConfig.directionConstraints.maxTravel["Z"] = m_uiConfig.maxTravelMM * 1e-3;

  // Update algorithm parameters
  m_scanConfig.improvementThreshold = m_uiConfig.improvementThreshold / 100.0;  // Convert percentage
  m_scanConfig.usePhysicsConstraints = m_uiConfig.enablePhysicsConstraints;
  m_scanConfig.usePowerAdaptiveSteps = m_uiConfig.enableAdaptiveSteps;
}

double AdaptiveScanningUI::PerformMeasurement(double x, double y, double z) {
  // Check if scan was cancelled before expensive operations
  if (!m_isScanning.load()) {
    return 0.0;
  }

  // Move to position first (this is the slow part)
  if (!MoveToPosition(x, y, z)) {
    m_logger->LogWarning("AdaptiveScanningUI: Failed to move to position");
    return 0.0;
  }

  // Check again after movement
  if (!m_isScanning.load()) {
    return 0.0;
  }

  // Wait for settle time with cancellation checks
  const int settleTimeMs = 300; // Shorter settle time for adaptive scanning
  const int checkIntervalMs = 50;
  int waitedMs = 0;

  while (waitedMs < settleTimeMs) {
    if (!m_isScanning.load()) {
      return 0.0; // Exit early if cancelled
    }

    int sleepTime = (std::min)(checkIntervalMs, settleTimeMs - waitedMs);
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
    waitedMs += sleepTime;
  }

  // Get measurement from data store (should be fast)
  double value = 0.0;
  if (m_dataStore.HasValue(m_selectedDataChannel)) {
    value = m_dataStore.GetValue(m_selectedDataChannel);

    // Update current value for UI (atomic)
    m_currentValue.store(value);
  }

  return value;
}

bool AdaptiveScanningUI::IsPositionValid(double x, double y, double z) {
  // Check bounds based on travel limits
  double maxTravel = m_uiConfig.maxTravelMM * 1e-3; // Convert to mm
  return (std::abs(x) < maxTravel &&
    std::abs(y) < maxTravel &&
    std::abs(z) < maxTravel);
}

bool AdaptiveScanningUI::MoveToPosition(double x, double y, double z) {
  PIController* controller = GetSelectedController();
  if (!controller || !controller->IsConnected()) {
    return false;
  }

  try {
    // Check if scan was cancelled before starting movement
    if (!m_isScanning.load()) {
      return false;
    }

    // Use the PI controller's MoveToPosition method for each axis
    bool xOk = controller->MoveToPosition("X", x, false); // non-blocking
    bool yOk = controller->MoveToPosition("Y", y, false);
    bool zOk = controller->MoveToPosition("Z", z, false);

    if (!xOk || !yOk || !zOk) {
      m_logger->LogError("AdaptiveScanningUI: Failed to initiate movement");
      return false;
    }

    // Wait for all axes to complete movement with cancellation checks
    const int maxWaitMs = 5000; // 5 second timeout
    const int checkIntervalMs = 100;
    int waitedMs = 0;

    while (waitedMs < maxWaitMs) {
      // Check if scan was cancelled
      if (!m_isScanning.load()) {
        // Stop all axes if scan was cancelled
        try {
          controller->StopAllAxes();
        }
        catch (const std::exception& e) {
          m_logger->LogError("AdaptiveScanningUI: Error stopping axes during cancellation - " + std::string(e.what()));
        }
        return false;
      }

      // Check if movement is complete
      try {
        if (!controller->IsMoving("X") &&
          !controller->IsMoving("Y") &&
          !controller->IsMoving("Z")) {
          return true;
        }
      }
      catch (const std::exception& e) {
        m_logger->LogError("AdaptiveScanningUI: Error checking movement status - " + std::string(e.what()));
        return false;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(checkIntervalMs));
      waitedMs += checkIntervalMs;
    }

    m_logger->LogWarning("AdaptiveScanningUI: Movement timeout");

    // Try to stop axes on timeout
    try {
      controller->StopAllAxes();
    }
    catch (const std::exception& e) {
      m_logger->LogError("AdaptiveScanningUI: Error stopping axes after timeout - " + std::string(e.what()));
    }

    return false;

  }
  catch (const std::exception& e) {
    m_logger->LogError("AdaptiveScanningUI: Movement error - " + std::string(e.what()));
    return false;
  }
}

void AdaptiveScanningUI::RefreshAvailableDevices() {
  // Select first available device if none selected
  if (m_selectedDevice.empty()) {
    if (m_piControllerManager.GetController("hex-left") != nullptr) {
      m_selectedDevice = "hex-left";
    }
    else if (m_piControllerManager.GetController("hex-right") != nullptr) {
      m_selectedDevice = "hex-right";
    }
  }
}

void AdaptiveScanningUI::RefreshAvailableDataChannels() {
  m_availableDataChannels.clear();

  // Based on the selected hexapod, add appropriate channels
  if (m_selectedDevice == "hex-left") {
    m_availableDataChannels.push_back("hex-left-Analog-Ch5");
    m_availableDataChannels.push_back("hex-left-Analog-Ch6");
    m_availableDataChannels.push_back("GPIB-Current");
  }
  else if (m_selectedDevice == "hex-right") {
    m_availableDataChannels.push_back("hex-right-Analog-Ch5");
    m_availableDataChannels.push_back("hex-right-Analog-Ch6");
    m_availableDataChannels.push_back("GPIB-Current");
  }
  else {
    // Default channels if no device selected
    m_availableDataChannels.push_back("GPIB-Current");
  }

  // Auto-select first channel if none selected or current selection is invalid
  if (m_selectedDataChannel.empty() && !m_availableDataChannels.empty()) {
    m_selectedDataChannel = m_availableDataChannels[0];
  }
  else if (std::find(m_availableDataChannels.begin(), m_availableDataChannels.end(),
    m_selectedDataChannel) == m_availableDataChannels.end() &&
    !m_availableDataChannels.empty()) {
    m_selectedDataChannel = m_availableDataChannels[0];
  }
}

std::string AdaptiveScanningUI::FormatPosition(const ScanStep& step) const {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(6);
  ss << "X:" << step.x << " Y:" << step.y << " Z:" << step.z << " mm";
  return ss.str();
}

PIController* AdaptiveScanningUI::GetSelectedController() const {
  if (m_selectedDevice.empty()) {
    return nullptr;
  }
  return m_piControllerManager.GetController(m_selectedDevice);
}