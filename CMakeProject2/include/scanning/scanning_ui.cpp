// src/scanning/scanning_ui.cpp
#include "include/scanning/scanning_ui.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

// Simplified constructor without MotionControlLayer
ScanningUI::ScanningUI(PIControllerManager& piControllerManager,
  GlobalDataStore& dataStore)
  : m_piControllerManager(piControllerManager),
  m_dataStore(dataStore),
  m_isScanning(false)
{
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("ScanningUI: Initializing hexapod optimization interface");

  // Refresh available devices and data channels
  RefreshAvailableDevices();
  RefreshAvailableDataChannels();

  // Set default parameters
  m_parameters = ScanningParameters::CreateDefault();

  // Default to Z axis only for hexapods, with X, Y as backup axes
  m_parameters.axesToScan = { "Z", "X", "Y" };

  // Use finer step sizes for precision alignment (0.5, 1, 2 microns)
  m_parameters.stepSizes = { 0.0005, 0.001, 0.002 };

  // Set shorter motion settle time for faster scanning
  m_parameters.motionSettleTimeMs = 300; // 300ms for hexapods

  // Increase consecutive decreases limit for better peak finding
  m_parameters.consecutiveDecreasesLimit = 4;

  // Lower improvement threshold for more sensitivity
  m_parameters.improvementThreshold = 0.005; // 0.5%

  // Set max travel distance to 2mm
  m_parameters.maxTotalDistance = 2.0; // 2mm max travel distance
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
  ImGui::Text("Step Size: 0.5, 1, 2 microns");
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

void ScanningUI::RenderScanControls() {
  ImGui::Text("Step 4: Run Scan");

  // Check if we can start a scan
  bool canStartScan = !m_selectedDevice.empty() &&
    !m_selectedDataChannel.empty() &&
    !m_parameters.axesToScan.empty() &&
    !m_parameters.stepSizes.empty() &&
    GetSelectedController() != nullptr &&
    GetSelectedController()->IsConnected();

  // Also check if the controller is currently moving
  bool isControllerMoving = false;
  if (canStartScan && GetSelectedController() != nullptr) {
    for (const auto& axis : { "X", "Y", "Z", "U", "V", "W" }) {
      if (GetSelectedController()->IsMoving(axis)) {
        isControllerMoving = true;
        break;
      }
    }
  }

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
  if (!m_isScanning && canStartScan && !isControllerMoving) {
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
  if (m_isScanning) {
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

void ScanningUI::RenderScanStatus() {
  // Progress bar with current status - convert double to float explicitly
  float progressBarValue = static_cast<float>(m_scanProgress);
  ImGui::ProgressBar(progressBarValue, ImVec2(-1, 0), m_scanStatus.c_str());

  // Display current measurement and peak measurement if available
  {
    std::lock_guard<std::mutex> lock(m_dataMutex);

    ImGui::Text("Current: %g", m_currentValue);

    if (m_peakValue > 0) {
      ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "Best Value: %g", m_peakValue);
      ImGui::Text("Best Position: %s", FormatPosition(m_peakPosition).c_str());
    }
  }
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

void ScanningUI::StartScan() {
  // Check if already scanning
  if (m_isScanning) {
    m_logger->LogWarning("ScanningUI: Scan already in progress");
    return;
  }

  // Check requirements
  if (m_selectedDevice.empty() || m_selectedDataChannel.empty()) {
    m_logger->LogError("ScanningUI: Cannot start scan - missing device or data channel");
    return;
  }

  // Get the PI controller for the selected device
  PIController* controller = GetSelectedController();
  if (!controller || !controller->IsConnected()) {
    m_logger->LogError("ScanningUI: Cannot start scan - controller not connected");
    return;
  }

  try {
    // Validate parameters
    m_parameters.Validate();

    // Create scanner with PI controller directly
    m_scanner = std::make_unique<ScanningAlgorithm>(
      *controller,  // Use the PIController directly
      m_dataStore,
      m_selectedDevice,
      m_selectedDataChannel,
      m_parameters
    );

    // Set callbacks
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

    // Start scan
    bool started = m_scanner->StartScan();
    if (started) {
      m_isScanning = true;
      m_scanProgress = 0.0;
      m_scanStatus = "Starting scan...";
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
  std::lock_guard<std::mutex> lock(m_dataMutex);
  m_scanProgress = args.GetProgress();
  m_scanStatus = args.GetStatus();
}

void ScanningUI::OnScanCompleted(const ScanCompletedEventArgs& args) {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  m_isScanning = false;
  m_scanProgress = 1.0;
  m_scanStatus = "Scan completed";
  m_hasResults = true;

  // Create a copy of the results
  const ScanResults& srcResults = args.GetResults();
  m_lastResults = std::make_unique<ScanResults>();

  // Copy the simple members
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

void ScanningUI::OnErrorOccurred(const ScanErrorEventArgs& args) {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  m_isScanning = false;
  m_scanStatus = "Error: " + args.GetError();
  m_logger->LogError("ScanningUI: Scan error - " + args.GetError());
}

void ScanningUI::OnDataPointAcquired(double value, const PositionStruct& position) {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  m_currentValue = value;
  m_currentPosition = position;
}

void ScanningUI::OnPeakUpdated(double value, const PositionStruct& position, const std::string& context) {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  m_peakValue = value;
  m_peakPosition = position;
  m_peakContext = context;
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