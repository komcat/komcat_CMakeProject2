// src/scanning/scanning_ui.cpp
#include "include/scanning/scanning_ui.h"
#include <algorithm>
#include <sstream>
#include <iomanip>


// Constructor - properly initialize atomic variables
// Modify the constructor in scanning_ui.cpp to initialize the presets
ScanningUI::ScanningUI(PIControllerManager& piControllerManager,
  GlobalDataStore& dataStore)
  : m_piControllerManager(piControllerManager),
  m_dataStore(dataStore),
  m_isScanning(false),
  m_scanProgress(0.0),
  m_currentValue(0.0),
  m_peakValue(0.0),
  m_hasResults(false)
{
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("ScanningUI: Initializing hexapod optimization interface");

  // Standard initialization
  RefreshAvailableDevices();
  RefreshAvailableDataChannels();

  // Default parameters for hexapod optimization
  m_parameters = ScanningParameters::CreateDefault();
  m_parameters.axesToScan = { "Z", "X", "Y" };
  m_parameters.motionSettleTimeMs = 400;
  m_parameters.consecutiveDecreasesLimit = 2;
  m_parameters.improvementThreshold = 0.005;
  m_parameters.maxTotalDistance = 2.0;

  // Initialize step size presets
  InitializeStepSizePresets();
}



ScanningUI::~ScanningUI() {
  // Ensure scanning is stopped
  if (m_isScanning) {
    StopScan();
  }

  m_logger->LogInfo("ScanningUI: Shutting down");
}

void ScanningUI::RenderUI() {
  if (!m_showWindow) return;

  ImGui::Begin("Hexapod Optimizer", &m_showWindow);

  // Simplified UI with just dropdown, status, and start/stop
  RenderDeviceSelection();
  ImGui::Separator();

  // Simple status display
  ImGui::Text("  m_parameters.stepSizes = { 0.001, 0.0005, 0.0002 };");
  ImGui::Text("Max Travel: 2.0 mm");

  // Start/Stop buttons
  RenderScanControls();
  ImGui::Separator();

  // Status info
  RenderScanStatus();

  ImGui::End();
}

void ScanningUI::RenderDeviceSelection() {
  ImGui::Text("Select Hexapod Device");

  // Create a combo box for hexapod device selection
  if (ImGui::BeginCombo("Hexapod", m_selectedDevice.c_str())) {
    for (const auto& device : m_hexapodDevices) {
      // Check if the device exists and has a PI controller
      PIController* controller = m_piControllerManager.GetController(device);
      bool deviceAvailable = (controller && controller->IsConnected());

      if (!deviceAvailable) {
        // Gray out unavailable devices
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
      }

      bool isSelected = (device == m_selectedDevice);
      if (ImGui::Selectable(device.c_str(), isSelected) && deviceAvailable) {
        m_selectedDevice = device;
        m_logger->LogInfo("ScanningUI: Selected device: " + m_selectedDevice);
        // Refresh data channels based on the selected device
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

  // Show connection status for the selected device
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

  // Data channel selection - simplified to just a dropdown
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

    // Display current value from the selected channel
    if (m_dataStore.HasValue(m_selectedDataChannel)) {
      double currentValue = m_dataStore.GetValue(m_selectedDataChannel);
      ImGui::Text("Current Value: %g", currentValue);
    }
  }
}

// Optimized RenderScanStatus method
void ScanningUI::RenderScanStatus() {
  // Get current progress - atomic access doesn't need lock
  float progressBarValue = static_cast<float>(m_scanProgress.load());

  // Get status string with minimal lock scope
  std::string statusCopy;
  {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    statusCopy = m_scanStatus;
  }

  // Draw progress bar with the copied status
  ImGui::ProgressBar(progressBarValue, ImVec2(-1, 0), statusCopy.c_str());

  // Display current measurement - atomic access doesn't need lock
  ImGui::Text("Current: %g", m_currentValue.load());

  // Display peak information if available
  double peakVal = m_peakValue.load();
  if (peakVal > 0) {
    ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "Best Value: %g", peakVal);

    // Get peak position with minimal lock scope
    PositionStruct peakPosCopy;
    {
      std::lock_guard<std::mutex> lock(m_dataMutex);
      peakPosCopy = m_peakPosition;
    }

    ImGui::Text("Best Position: %s", FormatPosition(peakPosCopy).c_str());
  }
}
void ScanningUI::RenderScanControls() {
  ImGui::Text("Scan Controls");

  // Check if we can start a scan
  bool canStartScan = !m_selectedDevice.empty() &&
    !m_selectedDataChannel.empty() &&
    !m_parameters.axesToScan.empty() &&
    !m_parameters.stepSizes.empty() &&
    GetSelectedController() != nullptr &&
    GetSelectedController()->IsConnected();

  // Add step size preset dropdown
  ImGui::Text("Step Size Preset:");
  if (ImGui::BeginCombo("##StepSizePreset", m_stepSizePresets[m_selectedPresetIndex].name.c_str())) {
    for (int i = 0; i < m_stepSizePresets.size(); i++) {
      bool isSelected = (m_selectedPresetIndex == i);
      if (ImGui::Selectable(m_stepSizePresets[i].name.c_str(), isSelected)) {
        m_selectedPresetIndex = i;
        m_parameters.stepSizes = m_stepSizePresets[i].stepSizes;
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  // Display current step sizes
  std::string stepsStr;
  for (size_t i = 0; i < m_parameters.stepSizes.size(); ++i) {
    if (i > 0) stepsStr += ", ";
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.5f", m_parameters.stepSizes[i]);
    stepsStr += buffer;
  }
  ImGui::Text("Step Sizes (mm): %s", stepsStr.c_str());

  // Check if the controller is currently moving - this avoids lock contention
  bool isControllerMoving = false;
  if (canStartScan && GetSelectedController() != nullptr) {
    for (const auto& axis : { "X", "Y", "Z", "U", "V", "W" }) {
      if (GetSelectedController()->IsMoving(axis)) {
        isControllerMoving = true;
        break;
      }
    }
  }

  // Get scan status atomically without locking
  bool isScanningNow = m_isScanning.load();

  // Show reason why scan can't start
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

  // Create a row with both buttons
  ImGui::BeginGroup();

  // Start scan button styling and state
  if (!isScanningNow && canStartScan && !isControllerMoving) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.7f, 0.1f, 1.0f));

    if (ImGui::Button("Start Scan##StartScanBtn", ImVec2(150, 40))) {
      StartScan();
    }

    ImGui::PopStyleColor(3);
  }
  else {
    // Gray out the button when it's not usable
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 0.6f));
    ImGui::Button("Start Scan##StartScanBtn", ImVec2(150, 40));
    ImGui::PopStyleColor(2);
  }

  ImGui::SameLine();

  // Stop scan button styling and state
  if (isScanningNow) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));

    if (ImGui::Button("Stop Scan##StopScanBtn", ImVec2(150, 40))) {
      StopScan();
    }

    ImGui::PopStyleColor(3);
  }
  else {
    // Gray out the button when it's not usable
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 0.6f));
    ImGui::Button("Stop Scan##StopScanBtn", ImVec2(150, 40));
    ImGui::PopStyleColor(2);
  }

  ImGui::EndGroup();

  // Add brief instructions
  ImGui::TextWrapped("This tool scans selected axes to find the position that maximizes the selected data channel reading. It's useful for optimizing alignment of optical components.");
}

void ScanningUI::RefreshAvailableDevices() {
  // Just use the predefined hexapod devices in m_hexapodDevices
  bool hexLeftAvailable = (m_piControllerManager.GetController("hex-left") != nullptr);
  bool hexRightAvailable = (m_piControllerManager.GetController("hex-right") != nullptr);

  // If nothing selected yet but devices available, select first available one
  if (m_selectedDevice.empty()) {
    if (hexLeftAvailable) {
      m_selectedDevice = "hex-left";
    }
    else if (hexRightAvailable) {
      m_selectedDevice = "hex-right";
    }
  }
}

void ScanningUI::RefreshAvailableDataChannels() {
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

  // If no channel selected yet but channels available, select first one
  if (m_selectedDataChannel.empty() && !m_availableDataChannels.empty()) {
    m_selectedDataChannel = m_availableDataChannels[0];
  }
  else if (std::find(m_availableDataChannels.begin(), m_availableDataChannels.end(),
    m_selectedDataChannel) == m_availableDataChannels.end() &&
    !m_availableDataChannels.empty()) {
    m_selectedDataChannel = m_availableDataChannels[0];
  }
}

// StartScan method - set up callbacks with optimized handlers
void ScanningUI::StartScan() {
  // Check if already scanning
  if (m_isScanning) {
    m_logger->LogWarning("ScanningUI: Scan already in progress");
    return;
  }

  // Standard validation code...
  if (m_selectedDevice.empty() || m_selectedDataChannel.empty()) {
    m_logger->LogError("ScanningUI: Cannot start scan - missing device or data channel");
    return;
  }

  PIController* controller = GetSelectedController();
  if (!controller || !controller->IsConnected()) {
    m_logger->LogError("ScanningUI: Cannot start scan - controller not connected");
    return;
  }

  try {
    // Validate parameters
    m_parameters.Validate();

    // Create scanner
    m_scanner = std::make_unique<ScanningAlgorithm>(
      *controller,
      m_dataStore,
      m_selectedDevice,
      m_selectedDataChannel,
      m_parameters
    );

    // Set optimized callbacks
    m_scanner->SetProgressCallback([this](const ScanProgressEventArgs& args) {
      OnProgressUpdated(args);
    });

    m_scanner->SetCompletionCallback([this](const ScanCompletedEventArgs& args) {
      OnScanCompleted(args);
    });

    m_scanner->SetErrorCallback([this](const ScanErrorEventArgs& args) {
      OnErrorOccurred(args);
    });

    m_scanner->SetDataPointCallback([this](double value, const PositionStruct& position) {
      OnDataPointAcquired(value, position);
    });

    m_scanner->SetPeakUpdateCallback([this](double value, const PositionStruct& position, const std::string& context) {
      OnPeakUpdated(value, position, context);
    });

    // Start the scan
    bool started = m_scanner->StartScan();
    if (started) {
      m_isScanning = true;
      m_scanProgress = 0.0;
      {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_scanStatus = "Starting scan...";
      }
      m_logger->LogInfo("ScanningUI: Scan started for device " + m_selectedDevice);
    }
    else {
      m_logger->LogError("ScanningUI: Failed to start scan");
    }
  }
  catch (const std::exception& e) {
    m_logger->LogError("ScanningUI: Exception while starting scan - " + std::string(e.what()));
  }
}


void ScanningUI::StopScan() {
  if (!m_isScanning || !m_scanner) {
    return;
  }

  m_logger->LogInfo("ScanningUI: Stopping scan");
  m_scanner->HaltScan();
  m_isScanning = false;
  m_scanStatus = "Scan stopped by user";
}

void ScanningUI::OnProgressUpdated(const ScanProgressEventArgs& args) {
  // Use atomic directly - no lock needed
  m_scanProgress = args.GetProgress();

  // Use mutex only for the string update
  {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_scanStatus = args.GetStatus();
  }
}

void ScanningUI::OnScanCompleted(const ScanCompletedEventArgs& args) {
  // Update atomic states without locking
  m_isScanning = false;
  m_scanProgress = 1.0;
  m_hasResults = true;

  // Update status string with protection
  {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_scanStatus = "Scan completed";
  }

  // Create a deep copy of the results with a single lock
  {
    std::lock_guard<std::mutex> lock(m_dataMutex);

    // Copy the results
    const ScanResults& srcResults = args.GetResults();
    m_lastResults = std::make_unique<ScanResults>();

    // Copy simple members
    m_lastResults->deviceId = srcResults.deviceId;
    m_lastResults->scanId = srcResults.scanId;
    m_lastResults->startTime = srcResults.startTime;
    m_lastResults->endTime = srcResults.endTime;
    m_lastResults->totalMeasurements = srcResults.totalMeasurements;

    // Deep copy the unique pointers
    if (srcResults.baseline) {
      m_lastResults->baseline = std::make_unique<ScanBaseline>(*srcResults.baseline);
    }

    if (srcResults.peak) {
      m_lastResults->peak = std::make_unique<ScanPeak>(*srcResults.peak);
    }

    if (srcResults.statistics) {
      m_lastResults->statistics = std::make_unique<ScanStatistics>(*srcResults.statistics);
    }
  }
}
// Process batched measurements if needed - can be called periodically
void ScanningUI::ProcessMeasurementBatch() {
  // This method could process the batch of measurements with a single lock
  // For example, calculating averages, trends, or other statistics
  std::lock_guard<std::mutex> lock(m_dataMutex);

  if (m_recentMeasurements.empty()) {
    return;
  }

  // Example: Compute statistics on the batch
  // (implementation depends on specific needs)

  // Clear processed measurements
  m_recentMeasurements.clear();
}

void ScanningUI::OnErrorOccurred(const ScanErrorEventArgs& args) {
  // Update atomic state without locking
  m_isScanning = false;

  // Update status with protection
  {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_scanStatus = "Error: " + args.GetError();
  }

  m_logger->LogError("ScanningUI: Scan error - " + args.GetError());
}


void ScanningUI::OnDataPointAcquired(double value, const PositionStruct& position) {
  // Atomic update doesn't need mutex
  m_currentValue = value;

  // Add to measurement batch with minimal lock scope
  {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_currentPosition = position;

    // Add to batch queue for potential batch processing
    m_recentMeasurements.push_front(std::make_pair(value, position));
    if (m_recentMeasurements.size() > MAX_BATCH_SIZE) {
      m_recentMeasurements.pop_back();
    }
  }
}

void ScanningUI::OnPeakUpdated(double value, const PositionStruct& position, const std::string& context) {
  // Atomic update doesn't need mutex
  m_peakValue = value;

  // Update position and context with protection
  {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_peakPosition = position;
    m_peakContext = context;
  }
}






std::string ScanningUI::FormatPosition(const PositionStruct& position) const {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(6);
  ss << "X:" << position.x << " Y:" << position.y << " Z:" << position.z;
  return ss.str();
}

PIController* ScanningUI::GetSelectedController() const {
  if (m_selectedDevice.empty()) {
    return nullptr;
  }
  return m_piControllerManager.GetController(m_selectedDevice);
}
// Add this implementation to scanning_ui.cpp
void ScanningUI::InitializeStepSizePresets() {
  // Define presets
  m_stepSizePresets = {
    {"Normal (0.002, 0.001, 0.0005, 0.0002 mm)", {0.002, 0.001, 0.0005, 0.0002}},
    {"Fine (0.005, 0.0002 mm)", {0.001, 0.0002}},
    {"Ultra (0.0002, 0.0001 mm)", {0.0003, 0.0001}}
  };

  // Initialize parameters with the first preset
  if (!m_stepSizePresets.empty()) {
    m_parameters.stepSizes = m_stepSizePresets[0].stepSizes;
  }
}