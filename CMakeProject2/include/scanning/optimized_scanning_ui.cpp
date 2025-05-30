// optimized_scanning_ui.cpp
#include "optimized_scanning_ui.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>

OptimizedScanningUI::OptimizedScanningUI(PIControllerManager& piControllerManager,
  GlobalDataStore& dataStore)
  : m_piControllerManager(piControllerManager),
  m_dataStore(dataStore),
  m_isScanning(false),
  m_scanProgress(0.0),
  m_currentValue(0.0),
  m_peakValue(0.0)
{
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("OptimizedScanningUI: Initializing optimized scanner interface");

  RefreshAvailableDevices();
  RefreshAvailableDataChannels();

  // Initialize scanner with your adjusted parameters
  m_scanner = std::make_unique<SequentialOptimizedScanner>();

  // Set up hardware interface
  m_scanner->setMeasurementFunction([this](double x, double y, double z) -> double {
    return PerformMeasurement(x, y, z);
  });

  m_scanner->setPositionValidationFunction([this](double x, double y, double z) -> bool {
    return IsPositionValid(x, y, z);
  });
}

OptimizedScanningUI::~OptimizedScanningUI() {
  // Ensure scanning is stopped before destruction
  if (m_isScanning.load()) {
    m_logger->LogInfo("OptimizedScanningUI: Stopping scan during destruction");
    StopScan();

    // Give thread time to finish (max 2 seconds)
    int waitCount = 0;
    while (m_isScanning.load() && waitCount < 20) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      waitCount++;
    }

    if (m_isScanning.load()) {
      m_logger->LogWarning("OptimizedScanningUI: Scan thread did not stop within timeout");
    }
  }

  m_logger->LogInfo("OptimizedScanningUI: Shutting down");
}

void OptimizedScanningUI::RenderUI() {
  if (!m_showWindow) return;

  ImGui::Begin("Optimized Hexapod Scanner", &m_showWindow);

  RenderDeviceSelection();
  ImGui::Separator();

  // Show current parameters (matching your adjusted values)
  ImGui::Text("Z-axis steps: 0.005, 0.001, 0.0002 mm");
  ImGui::Text("XY-axis steps: 0.001, 0.0005, 0.0002 mm");
  ImGui::Text("Smart direction selection: Enabled");

  RenderScanControls();
  ImGui::Separator();

  RenderScanStatus();
  ImGui::Separator();

  RenderResults();

  ImGui::End();
}

void OptimizedScanningUI::RenderDeviceSelection() {
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
        m_logger->LogInfo("OptimizedScanningUI: Selected device: " + m_selectedDevice);
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
    ImGui::TextColored(
      controller->IsConnected() ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
      "Status: %s", controller->IsConnected() ? "Connected" : "Disconnected"
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
      ImGui::Text("Current Value: %g", currentValue);
    }
  }
}

void OptimizedScanningUI::RenderScanControls() {
  ImGui::Text("Scan Controls");

  // Check if we can start a scan
  bool canStartScan = !m_selectedDevice.empty() &&
    !m_selectedDataChannel.empty() &&
    GetSelectedController() != nullptr &&
    GetSelectedController()->IsConnected();

  // Check if controller is moving
  bool isControllerMoving = false;
  if (canStartScan && GetSelectedController() != nullptr) {
    for (const auto& axis : { "X", "Y", "Z", "U", "V", "W" }) {
      if (GetSelectedController()->IsMoving(axis)) {
        isControllerMoving = true;
        break;
      }
    }
  }

  bool isScanningNow = m_isScanning.load();

  // Show status
  if (!canStartScan) {
    if (m_selectedDevice.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Select a hexapod device first");
    }
    else if (GetSelectedController() == nullptr || !GetSelectedController()->IsConnected()) {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Selected controller is not connected");
    }
    else if (m_selectedDataChannel.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Select a data channel first");
    }
  }
  else if (isControllerMoving) {
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "Controller is currently moving");
  }
  else {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Ready");
  }

  ImGui::BeginGroup();

  // Start scan button
  if (!isScanningNow && canStartScan && !isControllerMoving) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.7f, 0.1f, 1.0f));

    if (ImGui::Button("Start Optimized Scan", ImVec2(180, 40))) {
      StartScan();
    }

    ImGui::PopStyleColor(3);
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 0.6f));
    ImGui::Button("Start Optimized Scan", ImVec2(180, 40));
    ImGui::PopStyleColor(2);
  }

  ImGui::SameLine();

  // Stop scan button
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

  ImGui::TextWrapped("This optimized scanner uses smart direction selection and adaptive step sizes for faster convergence.");
}

void OptimizedScanningUI::RenderScanStatus() {
  float progressBarValue = static_cast<float>(m_scanProgress.load());

  std::string statusCopy;
  {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    statusCopy = m_scanStatus;
  }

  ImGui::ProgressBar(progressBarValue, ImVec2(-1, 0), statusCopy.c_str());

  ImGui::Text("Current: %g", m_currentValue.load());

  double peakVal = m_peakValue.load();
  if (peakVal > 0) {
    ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "Best Value: %g", peakVal);

    ScanStep peakPosCopy;
    {
      std::lock_guard<std::mutex> lock(m_dataMutex);
      peakPosCopy = m_peakPosition;
    }

    ImGui::Text("Best Position: %s", FormatPosition(peakPosCopy).c_str());
  }
}

void OptimizedScanningUI::RenderResults() {
  if (m_scanHistory.empty()) {
    ImGui::Text("No scan results yet");
    return;
  }

  ImGui::Text("Scan Results:");
  ImGui::Text("Total measurements: %d", static_cast<int>(m_scanHistory.size()));

  // Count measurements per axis
  std::map<std::string, int> axisCounts;
  for (const auto& step : m_scanHistory) {
    if (!step.axis.empty()) {
      axisCounts[step.axis]++;
    }
  }

  for (const auto& [axis, count] : axisCounts) {
    double percentage = (100.0 * count) / m_scanHistory.size();
    ImGui::Text("  %s: %d (%.1f%%)", axis.c_str(), count, percentage);
  }

  if (m_peakValue.load() > 0) {
    double improvement = 0.0;
    if (!m_scanHistory.empty() && m_scanHistory[0].value > 0) {
      improvement = (m_peakValue.load() - m_scanHistory[0].value) / m_scanHistory[0].value * 100.0;
    }
    ImGui::Text("Total improvement: %.2f%%", improvement);
  }
}

void OptimizedScanningUI::StartScan() {
  if (m_isScanning.load()) {
    m_logger->LogWarning("OptimizedScanningUI: Scan already in progress");
    return;
  }

  if (m_selectedDevice.empty() || m_selectedDataChannel.empty()) {
    m_logger->LogError("OptimizedScanningUI: Cannot start scan - missing device or data channel");
    return;
  }

  PIController* controller = GetSelectedController();
  if (!controller || !controller->IsConnected()) {
    m_logger->LogError("OptimizedScanningUI: Cannot start scan - controller not connected");
    return;
  }

  try {
    // Set scanning state atomically BEFORE starting thread
    m_isScanning.store(true);
    m_scanProgress.store(0.0);

    // Clear previous results with mutex protection
    {
      std::lock_guard<std::mutex> lock(m_dataMutex);
      m_scanHistory.clear();
      m_currentPosition = ScanStep();
      m_peakPosition = ScanStep();
    }

    // Update status safely
    {
      std::lock_guard<std::mutex> lock(m_statusMutex);
      m_scanStatus = "Preparing scan...";
    }

    // Get starting position (this could fail, so do it before thread)
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
        m_logger->LogInfo("OptimizedScanningUI: Starting from current position (" +
          std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")");
      }
      else {
        startPosition.x = 0.0;
        startPosition.y = 0.0;
        startPosition.z = 0.0;
        m_logger->LogWarning("OptimizedScanningUI: Could not read current position, using (0,0,0)");
      }
    }
    catch (const std::exception& e) {
      startPosition.x = 0.0;
      startPosition.y = 0.0;
      startPosition.z = 0.0;
      m_logger->LogWarning("OptimizedScanningUI: Exception reading position, using (0,0,0) - " + std::string(e.what()));
    }

    // Get initial measurement (could be slow, so do before thread)
    {
      std::lock_guard<std::mutex> lock(m_statusMutex);
      m_scanStatus = "Taking initial measurement...";
    }

    startPosition.value = PerformMeasurement(startPosition.x, startPosition.y, startPosition.z);

    if (startPosition.value <= 0.0) {
      m_logger->LogError("OptimizedScanningUI: Could not get valid initial measurement");
      m_isScanning.store(false);
      {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_scanStatus = "Failed to get initial measurement";
      }
      return;
    }

    m_logger->LogInfo("OptimizedScanningUI: Initial measurement: " + std::to_string(startPosition.value));

    // Now start the scan thread with all preparation done
    std::thread scanThread([this, startPosition]() {
      try {
        {
          std::lock_guard<std::mutex> lock(m_statusMutex);
          m_scanStatus = "Running optimized scan...";
        }

        // Run the actual scan
        ScanStep finalPosition = m_scanner->optimizedSequentialScan(startPosition);

        // Check if scan was cancelled during execution
        if (!m_isScanning.load()) {
          {
            std::lock_guard<std::mutex> lock(m_statusMutex);
            m_scanStatus = "Scan cancelled";
          }
          return;
        }

        // Update results atomically
        {
          std::lock_guard<std::mutex> lock(m_dataMutex);
          m_scanHistory = m_scanner->getScanHistory();
          m_peakPosition = finalPosition;
        }

        // Update peak value atomically
        m_peakValue.store(finalPosition.value);
        m_scanProgress.store(1.0);

        // Update final status
        {
          std::lock_guard<std::mutex> lock(m_statusMutex);
          m_scanStatus = "Scan completed successfully";
        }

        m_logger->LogInfo("OptimizedScanningUI: Scan completed. Total measurements: " +
          std::to_string(m_scanner->getTotalMeasurements()) +
          ", Final value: " + std::to_string(finalPosition.value));

      }
      catch (const std::exception& e) {
        // Handle any scan errors
        {
          std::lock_guard<std::mutex> lock(m_statusMutex);
          m_scanStatus = "Error: " + std::string(e.what());
        }

        m_logger->LogError("OptimizedScanningUI: Scan error - " + std::string(e.what()));
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
    m_logger->LogError("OptimizedScanningUI: Exception while starting scan - " + std::string(e.what()));
  }
}

void OptimizedScanningUI::StopScan() {
  if (!m_isScanning.load()) {
    return;
  }

  m_logger->LogInfo("OptimizedScanningUI: Stopping scan");

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
      m_logger->LogInfo("OptimizedScanningUI: Stopped all axes");
    }
    catch (const std::exception& e) {
      m_logger->LogError("OptimizedScanningUI: Error stopping axes - " + std::string(e.what()));
    }
  }

  // Update final status
  {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_scanStatus = "Scan stopped by user";
  }
}

double OptimizedScanningUI::PerformMeasurement(double x, double y, double z) {
  // Check if scan was cancelled before expensive operations
  if (!m_isScanning.load()) {
    return 0.0;
  }

  // Move to position first (this is the slow part)
  if (!MoveToPosition(x, y, z)) {
    m_logger->LogWarning("OptimizedScanningUI: Failed to move to position");
    return 0.0;
  }

  // Check again after movement
  if (!m_isScanning.load()) {
    return 0.0;
  }

  // Wait for settle time with cancellation checks
  const int settleTimeMs = 400;
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

    // Update current position and value for UI (atomic for value, mutex for position)
    m_currentValue.store(value);
    {
      std::lock_guard<std::mutex> lock(m_dataMutex);
      m_currentPosition.x = x;
      m_currentPosition.y = y;
      m_currentPosition.z = z;
      m_currentPosition.value = value;
    }
  }

  return value;
}

bool OptimizedScanningUI::IsPositionValid(double x, double y, double z) {
  // Simple bounds checking (adjust for your hardware)
  const double MAX_TRAVEL = 0.01; // 10mm travel range
  return (std::abs(x) < MAX_TRAVEL &&
    std::abs(y) < MAX_TRAVEL &&
    std::abs(z) < MAX_TRAVEL);
}

bool OptimizedScanningUI::MoveToPosition(double x, double y, double z) {
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
      m_logger->LogError("OptimizedScanningUI: Failed to initiate movement");
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
          m_logger->LogError("OptimizedScanningUI: Error stopping axes during cancellation - " + std::string(e.what()));
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
        m_logger->LogError("OptimizedScanningUI: Error checking movement status - " + std::string(e.what()));
        return false;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(checkIntervalMs));
      waitedMs += checkIntervalMs;
    }

    m_logger->LogWarning("OptimizedScanningUI: Movement timeout");

    // Try to stop axes on timeout
    try {
      controller->StopAllAxes();
    }
    catch (const std::exception& e) {
      m_logger->LogError("OptimizedScanningUI: Error stopping axes after timeout - " + std::string(e.what()));
    }

    return false;

  }
  catch (const std::exception& e) {
    m_logger->LogError("OptimizedScanningUI: Movement error - " + std::string(e.what()));
    return false;
  }
}

void OptimizedScanningUI::RefreshAvailableDevices() {
  if (m_selectedDevice.empty()) {
    if (m_piControllerManager.GetController("hex-left") != nullptr) {
      m_selectedDevice = "hex-left";
    }
    else if (m_piControllerManager.GetController("hex-right") != nullptr) {
      m_selectedDevice = "hex-right";
    }
  }
}

void OptimizedScanningUI::RefreshAvailableDataChannels() {
  m_availableDataChannels.clear();

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
    m_availableDataChannels.push_back("GPIB-Current");
  }

  if (m_selectedDataChannel.empty() && !m_availableDataChannels.empty()) {
    m_selectedDataChannel = m_availableDataChannels[0];
  }
  else if (std::find(m_availableDataChannels.begin(), m_availableDataChannels.end(),
    m_selectedDataChannel) == m_availableDataChannels.end() &&
    !m_availableDataChannels.empty()) {
    m_selectedDataChannel = m_availableDataChannels[0];
  }
}

std::string OptimizedScanningUI::FormatPosition(const ScanStep& step) const {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(6);
  ss << "X:" << step.x << " Y:" << step.y << " Z:" << step.z;
  return ss.str();
}

PIController* OptimizedScanningUI::GetSelectedController() const {
  if (m_selectedDevice.empty()) {
    return nullptr;
  }
  return m_piControllerManager.GetController(m_selectedDevice);
}