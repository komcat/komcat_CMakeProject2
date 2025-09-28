// RealtimeChartPage.cpp
#include "RealtimeChartPage.h"
#include "include/logger.h"
#include "include/data/global_data_store.h"

#include <raylib.h>
#include <algorithm>
#include <cmath>
#include "imgui.h"
#include <iostream>
// Forward declaration approach to avoid header conflicts
// We'll use void* pointers and cast them when needed
// Forward declare MachineOperations class to enable casting
class MachineOperations;

extern "C" {
  bool MachineOperations_PerformScan(void* machineOpsPtr,
    const char* deviceName,
    const char* dataChannel,
    const double* stepSizes,
    int stepSizeCount,
    int settlingTimeMs,
    const char** axes,
    int axesCount,
    const char* callerContext);
}
extern "C" {
  bool MachineOperations_StartScan(void* machineOpsPtr,
    const char* deviceName,
    const char* dataChannel,
    const double* stepSizes,
    int stepSizeCount,
    int settlingTimeMs,
    const char** axes,
    int axesCount,
    const char* callerContext);
}
extern "C" {
  // ADD THIS:
  bool MachineOperations_StopScan(void* machineOpsPtr,
    const char* deviceName,
    const char* callerContext);
}

// Declare the C wrapper function
extern "C" bool MachineOperations_IsScanActive(void* machineOpsPtr, const char* deviceName);

RealtimeChartPage::RealtimeChartPage(Logger* logger)
    : m_logger(logger), m_dataStore(nullptr), m_fontLoaded(false),
    m_machineOperations(nullptr), m_piControllerManager(nullptr),
    m_dataChannel("GPIB-Current"), m_timeWindow(10.0f),  // Keep default but will be changed by selector
    m_currentValue(0.0f), m_scaledValue(0.0f),
    m_selectedChannelIndex(0),
    m_leftCoarseState(ScanState::IDLE), m_leftFineState(ScanState::IDLE),
    m_rightCoarseState(ScanState::IDLE), m_rightFineState(ScanState::IDLE) {

    // Initialize available channels with default
    m_availableChannels = { "GPIB-Current" };

    if (m_logger) {
        m_logger->LogInfo("RealtimeChartPage created");
        m_logger->LogInfo("Initial data channel: " + m_dataChannel);  // Show initial channel
    }

  // Load custom font
  m_customFont = LoadFont("assets/fonts/CascadiaCode-Regular.ttf");
  if (m_customFont.texture.id != 0) {
    m_fontLoaded = true;
    if (m_logger) {
      m_logger->LogInfo("CascadiaCode font loaded successfully");
    }
  }
  else {
    m_fontLoaded = false;
    if (m_logger) {
      m_logger->LogWarning("Failed to load CascadiaCode font, using default");
    }
  }
}

RealtimeChartPage::~RealtimeChartPage() {
  if (m_fontLoaded) {
    UnloadFont(m_customFont);
  }

  if (m_logger) {
    m_logger->LogInfo("RealtimeChartPage destroyed");
  }
}

void RealtimeChartPage::SetDataStore(GlobalDataStore* store) {
  m_dataStore = store;
}

void RealtimeChartPage::SetMachineOperations(void* machineOps) {  // Change method name and parameter
  m_machineOperations = machineOps;
}

void RealtimeChartPage::SetPIControllerManager(void* piManager) {
  m_piControllerManager = piManager;
}

void RealtimeChartPage::Render() {
  // Update data first
  updateData();

  // Update available channels
  updateAvailableChannels();

  // Update button states from scanning system
  updateButtonStatesFromScanning();

  // Handle button clicks
  handleButtonClicks();

  // Page title and navigation info
  DrawText("Realtime Chart (C)", 10, 10, 20, DARKBLUE);
  DrawText("C: Chart | M: Menu | V: Live Video | S: Status | R: Rectangles", 10, 40, 14, GRAY);

  // Get screen dimensions
  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();

  // Calculate layout areas
  int topSectionHeight = (int)(screenHeight * 0.6f);
  int chartSectionY = topSectionHeight;
  int chartSectionHeight = screenHeight - topSectionHeight;

  // Render digital display (top 60%)
  renderDigitalDisplay();

  // Render buttons
  renderButtons();

  // Render chart with integrated channel list (bottom 40%)
  renderChart();

  // REMOVED: renderChannelSelector() - now integrated into renderChart()
}


void RealtimeChartPage::updateData() {
  if (!m_dataStore) {
    if (m_logger) {
      static int noDataStoreCount = 0;
      if (++noDataStoreCount % 120 == 0) { // Log every 2 seconds at 60 FPS
        m_logger->LogWarning("RealtimeChartPage: dataStore is NULL");
      }
    }
    return;
  }

  // Get current time
  auto now = std::chrono::steady_clock::now();
  double currentTime = std::chrono::duration<double>(now.time_since_epoch()).count();

  // Try to get current value from data store
  if (m_dataStore->HasValue(m_dataChannel)) {
    float newValue = m_dataStore->GetValue(m_dataChannel, 0.0f);

    m_currentValue = newValue;

    // Add to buffer
    m_dataBuffer.push_back({ currentTime, newValue });

    // Clean old data
    cleanOldData();

    // Calculate display values
    calculateDisplayValue();
  }
  else {
    if (m_logger) {
      static int noValueCount = 0;
      if (++noValueCount % 300 == 0) { // Log every 5 seconds
        m_logger->LogWarning("RealtimeChart: Channel '" + m_dataChannel + "' not found in dataStore");
      }
    }
  }
}

void RealtimeChartPage::cleanOldData() {
  auto now = std::chrono::steady_clock::now();
  double currentTime = std::chrono::duration<double>(now.time_since_epoch()).count();
  double cutoffTime = currentTime - m_timeWindow;

  // Remove old data points
  while (!m_dataBuffer.empty() && m_dataBuffer.front().timestamp < cutoffTime) {
    m_dataBuffer.pop_front();
  }
}

void RealtimeChartPage::calculateDisplayValue() {
  auto [scaledValue, unit] = getScaledUnit(std::abs(m_currentValue));
  m_scaledValue = (m_currentValue >= 0) ? scaledValue : -scaledValue;
  m_displayUnit = unit;
}

std::pair<float, std::string> RealtimeChartPage::getScaledUnit(float absValue) {
  // Auto-scale units for current (assuming GPIB-Current)
  if (absValue < 1e-9f) {
    return { absValue * 1e12f, "pA" };
  }
  else if (absValue < 1e-6f) {
    return { absValue * 1e9f, "nA" };
  }
  else if (absValue < 1e-3f) {
    return { absValue * 1e6f, "uA" };
  }
  else if (absValue < 1.0f) {
    return { absValue * 1e3f, "mA" };
  }
  else {
    return { absValue, "A" };
  }
}

void RealtimeChartPage::renderDigitalDisplay() {
  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();
  int topSectionHeight = (int)(screenHeight * 0.6f);

  // Background for entire top section
  DrawRectangle(0, 70, screenWidth, topSectionHeight - 70, Color{ 30, 30, 40, 255 });

  // Calculate column widths
  int leftColWidth = (int)(screenWidth * 0.15f);
  int middleColWidth = (int)(screenWidth * 0.70f);
  int rightColWidth = (int)(screenWidth * 0.15f);

  int leftColX = 0;
  int middleColX = leftColWidth;
  int rightColX = leftColWidth + middleColWidth;

  // Font setup
  Font font = m_fontLoaded ? m_customFont : GetFontDefault();

  // === LEFT COLUMN (15%) - Left Hex Buttons ===
  Rectangle leftColumn = { (float)leftColX, 70, (float)leftColWidth, (float)(topSectionHeight - 70) };
  DrawRectangleLinesEx(leftColumn, 1, Color{ 50, 50, 60, 255 });

  // === MIDDLE COLUMN (70%) - Value Display ===
  Rectangle middleColumn = { (float)middleColX, 70, (float)middleColWidth, (float)(topSectionHeight - 70) };
  DrawRectangleLinesEx(middleColumn, 1, Color{ 50, 50, 60, 255 });

  // Channel name at top of middle column
  int channelFontSize = 24;
  Vector2 channelTextSize = MeasureTextEx(font, m_dataChannel.c_str(), static_cast<float>(channelFontSize), 2);
  int channelX = middleColX + middleColWidth / 2 - (int)channelTextSize.x / 2;
  DrawTextEx(font, m_dataChannel.c_str(), Vector2{ (float)channelX, 100 }, static_cast<float>(channelFontSize), 2, LIGHTGRAY);

  // Large digital value display
  int valueFontSize = 120;
  int valueY = topSectionHeight / 2 - valueFontSize / 2;

  // Format value
  char signChar = (m_scaledValue >= 0) ? '+' : '-';
  float absScaledValue = std::abs(m_scaledValue);
  int wholePart = (int)std::floor(absScaledValue);
  int fracPart = (int)std::round((absScaledValue - std::floor(absScaledValue)) * 1000);

  char valueText[64];
  snprintf(valueText, sizeof(valueText), "%c%3d.%03d %s",
    signChar, wholePart, fracPart, m_displayUnit.c_str());

  // Center the decimal point in middle column
  char beforeDecimal[8] = "+   ";
  beforeDecimal[0] = signChar;
  Vector2 beforeDecimalSize = MeasureTextEx(font, beforeDecimal, static_cast<float>(valueFontSize), 2);
  int decimalCenterX = middleColX + middleColWidth / 2;
  int valueX = decimalCenterX - (int)beforeDecimalSize.x;

  // Value color based on magnitude
  Color valueColor = GREEN;
  if (std::abs(m_currentValue) < 1e-9f) {
    valueColor = GRAY;
  }
  else if (std::abs(m_currentValue) > 1e-3f) {
    valueColor = ORANGE;
  }

  DrawTextEx(font, valueText, Vector2{ (float)valueX, (float)valueY }, static_cast<float> (valueFontSize), 2, valueColor);

  // === RIGHT COLUMN (15%) - Right Hex Buttons ===
  Rectangle rightColumn = { (float)rightColX, 70, (float)rightColWidth, (float)(topSectionHeight - 70) };
  DrawRectangleLinesEx(rightColumn, 1, Color{ 50, 50, 60, 255 });

  // Data points info at bottom of middle column
  char infoText[32];
  snprintf(infoText, sizeof(infoText), "Points: %zu", m_dataBuffer.size());
  DrawText(infoText, middleColX + 20, topSectionHeight - 30, 16, DARKGRAY);
}


// Add this debug version to RealtimeChartPage.cpp - add at the start of renderButtons method:


void RealtimeChartPage::renderButtons() {
  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();
  int topSectionHeight = (int)(screenHeight * 0.6f);

  // Calculate column dimensions
  int leftColWidth = (int)(screenWidth * 0.15f);
  int middleColWidth = (int)(screenWidth * 0.70f);
  int rightColWidth = (int)(screenWidth * 0.15f);

  int leftColX = 0;
  int middleColX = leftColWidth;
  int rightColX = leftColWidth + middleColWidth;

  // Button dimensions - make them fit nicely in columns
  int buttonWidth = leftColWidth - 40;  // Leave margin on sides
  int buttonHeight = 60;
  int buttonSpacing = 20;

  // Calculate vertical centering for buttons
  int totalButtonHeight = (buttonHeight * 2) + buttonSpacing;
  int buttonStartY = 70 + ((topSectionHeight - 70) / 2) - (totalButtonHeight / 2);

  // === LEFT COLUMN BUTTONS ===
  int leftButtonX = leftColX + 20;  // 20px margin from edge

  Rectangle leftCoarseBtn = {
      (float)leftButtonX,
      (float)buttonStartY,
      (float)buttonWidth,
      (float)buttonHeight
  };

  Rectangle leftFineBtn = {
      (float)leftButtonX,
      (float)(buttonStartY + buttonHeight + buttonSpacing),
      (float)buttonWidth,
      (float)buttonHeight
  };

  // === RIGHT COLUMN BUTTONS ===
  int rightButtonX = rightColX + 20;  // 20px margin from edge

  Rectangle rightCoarseBtn = {
      (float)rightButtonX,
      (float)buttonStartY,
      (float)buttonWidth,
      (float)buttonHeight
  };

  Rectangle rightFineBtn = {
      (float)rightButtonX,
      (float)(buttonStartY + buttonHeight + buttonSpacing),
      (float)buttonWidth,
      (float)buttonHeight
  };

  // === MIDDLE COLUMN - STOP BUTTON ===
  int stopWidth = 140;
  int stopHeight = 50;
  int stopX = middleColX + (middleColWidth / 2) - (stopWidth / 2);
  int stopY = topSectionHeight - 100;  // Position near bottom of middle section

  Rectangle stopBtn = { (float)stopX, (float)stopY, (float)stopWidth, (float)stopHeight };

  // Draw all buttons
  drawButton(leftCoarseBtn, "Left Coarse", m_leftCoarseState);
  drawButton(leftFineBtn, "Left Fine", m_leftFineState);
  drawButton(rightCoarseBtn, "Right Coarse", m_rightCoarseState);
  drawButton(rightFineBtn, "Right Fine", m_rightFineState);

  // Stop button
  bool anyScanning = (m_leftCoarseState == ScanState::SCANNING ||
    m_leftFineState == ScanState::SCANNING ||
    m_rightCoarseState == ScanState::SCANNING ||
    m_rightFineState == ScanState::SCANNING);

  Color stopColor = anyScanning ? RED : DARKGRAY;
  Vector2 mousePos = GetMousePosition();
  bool stopHovered = CheckCollisionPointRec(mousePos, stopBtn);

  if (stopHovered && anyScanning) {
    stopColor = MAROON;
  }

  DrawRectangleRec(stopBtn, stopColor);
  DrawRectangleLinesEx(stopBtn, 2, BLACK);

  // Stop button text
  Font font = m_fontLoaded ? m_customFont : GetFontDefault();
  const char* stopText = "STOP";
  int fontSize = 20;
  Vector2 stopTextSize = MeasureTextEx(font, stopText, static_cast<float>(fontSize), 2);
  Vector2 stopTextPos = {
      stopBtn.x + stopBtn.width / 2 - stopTextSize.x / 2,
      stopBtn.y + stopBtn.height / 2 - stopTextSize.y / 2
  };

  DrawTextEx(font, stopText, stopTextPos, static_cast<float>(fontSize), 2, WHITE);
}


bool RealtimeChartPage::drawButton(Rectangle rect, const char* text, ScanState state) {
  Vector2 mousePos = GetMousePosition();
  bool isHovered = CheckCollisionPointRec(mousePos, rect);

  // Button color based on state
  Color buttonColor;
  switch (state) {
  case ScanState::IDLE:
    buttonColor = isHovered ? Color{ 0, 200, 0, 255 } : GREEN; // Brighter green on hover
    break;
  case ScanState::SCANNING:
    buttonColor = YELLOW;
    break;
  }

  Color textColor = (state == ScanState::SCANNING) ? BLACK : WHITE;

  // Draw button
  DrawRectangleRec(rect, buttonColor);
  DrawRectangleLinesEx(rect, 2, BLACK);

  // Draw button text with custom font
  Font font = m_fontLoaded ? m_customFont : GetFontDefault();
  int fontSize = 16;
  Vector2 textSize = MeasureTextEx(font, text, static_cast<float>(fontSize), 2);
  Vector2 textPos = {
    rect.x + rect.width / 2 - textSize.x / 2,
    rect.y + rect.height / 2 - textSize.y / 2
  };

  DrawTextEx(font, text, textPos, static_cast<float>(fontSize), 2, textColor);

  // Return if button was clicked
  return isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

// Add this debug version to RealtimeChartPage.cpp - replace the handleButtonClicks method:


void RealtimeChartPage::handleButtonClicks() {
  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();
  int topSectionHeight = (int)(screenHeight * 0.6f);

  // Calculate column dimensions (same as renderButtons)
  int leftColWidth = (int)(screenWidth * 0.15f);
  int middleColWidth = (int)(screenWidth * 0.70f);
  int rightColWidth = (int)(screenWidth * 0.15f);

  int leftColX = 0;
  int middleColX = leftColWidth;
  int rightColX = leftColWidth + middleColWidth;

  // Button dimensions (same as renderButtons)
  int buttonWidth = leftColWidth - 40;
  int buttonHeight = 60;
  int buttonSpacing = 20;

  int totalButtonHeight = (buttonHeight * 2) + buttonSpacing;
  int buttonStartY = 70 + ((topSectionHeight - 70) / 2) - (totalButtonHeight / 2);

  // Left column buttons
  int leftButtonX = leftColX + 20;
  Rectangle leftCoarseBtn = { (float)leftButtonX, (float)buttonStartY, (float)buttonWidth, (float)buttonHeight };
  Rectangle leftFineBtn = { (float)leftButtonX, (float)(buttonStartY + buttonHeight + buttonSpacing), (float)buttonWidth, (float)buttonHeight };

  // Right column buttons
  int rightButtonX = rightColX + 20;
  Rectangle rightCoarseBtn = { (float)rightButtonX, (float)buttonStartY, (float)buttonWidth, (float)buttonHeight };
  Rectangle rightFineBtn = { (float)rightButtonX, (float)(buttonStartY + buttonHeight + buttonSpacing), (float)buttonWidth, (float)buttonHeight };

  // Stop button
  int stopWidth = 140;
  int stopHeight = 50;
  int stopX = middleColX + (middleColWidth / 2) - (stopWidth / 2);
  int stopY = topSectionHeight - 100;
  Rectangle stopBtn = { (float)stopX, (float)stopY, (float)stopWidth, (float)stopHeight };

  Vector2 mousePos = GetMousePosition();

  // Check button clicks
  if (drawButton(leftCoarseBtn, "Left Coarse", m_leftCoarseState)) {
    if (m_leftCoarseState == ScanState::IDLE) {
      startHexLeftCoarseScan();
    }
  }

  if (drawButton(leftFineBtn, "Left Fine", m_leftFineState)) {
    if (m_leftFineState == ScanState::IDLE) {
      startHexLeftFineScan();
    }
  }

  if (drawButton(rightCoarseBtn, "Right Coarse", m_rightCoarseState)) {
    if (m_rightCoarseState == ScanState::IDLE) {
      startHexRightCoarseScan();
    }
  }

  if (drawButton(rightFineBtn, "Right Fine", m_rightFineState)) {
    if (m_rightFineState == ScanState::IDLE) {
      startHexRightFineScan();
    }
  }

  // Stop button click
  bool stopHovered = CheckCollisionPointRec(mousePos, stopBtn);
  if (stopHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    stopAllScanning();
  }
}

void RealtimeChartPage::startHexLeftCoarseScan() {
  if (m_logger) {
    m_logger->LogInfo("Starting Hex-Left Coarse Scan");
  }
  std::cout << "Starting Hex-Left Coarse Scan" << std::endl;
  // Execute RunScanOperation with coarse preset
  executeRunScanOperation("hex-left", { 0.005, 0.001, 0.0005 });
  m_leftCoarseState = ScanState::SCANNING;
}

void RealtimeChartPage::startHexLeftFineScan() {
  if (m_logger) {
    m_logger->LogInfo("Starting Hex-Left Fine Scan");
  }
  std::cout << "Starting Hex-Left Fine Scan" << std::endl;
  // Execute RunScanOperation with fine preset
  executeRunScanOperation("hex-left", { 0.0005, 0.0002 });
  m_leftFineState = ScanState::SCANNING;
}

void RealtimeChartPage::startHexRightCoarseScan() {
  if (m_logger) {
    m_logger->LogInfo("Starting Hex-Right Coarse Scan");

  }
  std::cout << "Starting Hex-Right Coarse Scan" << std::endl;

  // Execute RunScanOperation with coarse preset
  executeRunScanOperation("hex-right", { 0.005, 0.001, 0.0005 });
  m_rightCoarseState = ScanState::SCANNING;
}

void RealtimeChartPage::startHexRightFineScan() {
  if (m_logger) {
    m_logger->LogInfo("Starting Hex-Right Fine Scan");
  }
  std::cout << "Starting Hex-Right Fine Scan" << std::endl;
  // Execute RunScanOperation with fine preset
  executeRunScanOperation("hex-right", { 0.0005, 0.0002 });
  m_rightFineState = ScanState::SCANNING;
}

void RealtimeChartPage::stopAllScanning() {
  if (m_logger) {
    m_logger->LogInfo("Stopping all scanning operations");
  }

  if (m_machineOperations) {
    try {


      std::string callerContext = "RealtimeChartPage_stop_all";

      // Stop scans for both devices
      bool leftStopped = MachineOperations_StopScan(m_machineOperations, "hex-left", callerContext.c_str());
      bool rightStopped = MachineOperations_StopScan(m_machineOperations, "hex-right", callerContext.c_str());

      if (m_logger) {
        if (leftStopped && rightStopped) {
          m_logger->LogInfo("RealtimeChart: Successfully stopped all scans");
        }
        else {
          m_logger->LogWarning("RealtimeChart: Some scans may not have stopped properly");
        }
      }

    }
    catch (const std::exception& e) {
      if (m_logger) {
        m_logger->LogError("RealtimeChart: Exception stopping operations: " + std::string(e.what()));
      }
    }
  }

  // Reset all button states to idle
  m_leftCoarseState = ScanState::IDLE;
  m_leftFineState = ScanState::IDLE;
  m_rightCoarseState = ScanState::IDLE;
  m_rightFineState = ScanState::IDLE;
}



void RealtimeChartPage::updateButtonStatesFromScanning() {
  if (!m_machineOperations) {
    return;
  }

  // Update button states based on actual scanning status
  // Check if hex-left is scanning and update left button states accordingly
  bool hexLeftScanning = isDeviceScanning("hex-left");
  bool hexRightScanning = isDeviceScanning("hex-right");

  // If scanning stopped externally, update button states
  if (!hexLeftScanning) {
    if (m_leftCoarseState == ScanState::SCANNING) {
      m_leftCoarseState = ScanState::IDLE;
    }
    if (m_leftFineState == ScanState::SCANNING) {
      m_leftFineState = ScanState::IDLE;
    }
  }

  if (!hexRightScanning) {
    if (m_rightCoarseState == ScanState::SCANNING) {
      m_rightCoarseState = ScanState::IDLE;
    }
    if (m_rightFineState == ScanState::SCANNING) {
      m_rightFineState = ScanState::IDLE;
    }
  }
}


void RealtimeChartPage::renderChart() {
  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();
  int topSectionHeight = (int)(screenHeight * 0.6f);
  int chartY = topSectionHeight;
  int chartHeight = screenHeight - topSectionHeight;

  // Calculate split areas
  int leftPanelWidth = (int)(screenWidth * 0.25f);
  int rightPanelWidth = screenWidth - leftPanelWidth;

  // LEFT PANEL - Channel List with Scrolling
  Rectangle leftPanel = { 10, (float)chartY + 10, (float)leftPanelWidth - 20, (float)chartHeight - 20 };
  DrawRectangleRec(leftPanel, Color{ 25, 25, 35, 255 });
  DrawRectangleLinesEx(leftPanel, 2, DARKGRAY);

  Font font = m_fontLoaded ? m_customFont : GetFontDefault();

  // Title
  DrawTextEx(font, "Channels", Vector2{ leftPanel.x + 10, leftPanel.y + 10 }, 16, 2, WHITE);
  DrawLineEx(Vector2{ leftPanel.x + 5, leftPanel.y + 35 },
    Vector2{ leftPanel.x + leftPanel.width - 5, leftPanel.y + 35 }, 1, GRAY);

  // Render scrollable channel list
  renderChannelList(leftPanel, font);

  // RIGHT PANEL - Chart
  Rectangle rightPanel = { (float)leftPanelWidth + 10, (float)chartY + 10,
                          (float)rightPanelWidth - 30, (float)chartHeight - 20 };
  renderChartPanel(rightPanel, font);
}



void RealtimeChartPage::executeRunScanOperation(const std::string& device,
    const std::vector<double>& stepSizes) {
    if (!m_machineOperations) {
        if (m_logger) {
            m_logger->LogError("RealtimeChart: MachineOperations not available");
        }
        return;
    }

    try {
        if (m_logger) {
            m_logger->LogInfo("RealtimeChart: Starting scan operation for " + device);
            std::string stepStr;
            for (size_t i = 0; i < stepSizes.size(); ++i) {
                if (i > 0) stepStr += ", ";
                stepStr += std::to_string(stepSizes[i]);
            }
            m_logger->LogInfo("  Step sizes: {" + stepStr + "}");
            // CHANGED: Use the active channel instead of hardcoded "GPIB-Current"
            m_logger->LogInfo("  Data channel: " + m_dataChannel);  // Use m_dataChannel instead of "GPIB-Current"
            m_logger->LogInfo("  Settling time: 300ms");
        }

        // Prepare parameters for C wrapper
        std::vector<std::string> axes = { "Z", "X", "Y" };
        std::vector<const char*> axesCStr;
        for (const auto& axis : axes) {
            axesCStr.push_back(axis.c_str());
        }

        int settlingTimeMs = 300;
        std::string callerContext = "RealtimeChartPage_" + device + "_scan";

        // CHANGED: Use m_dataChannel instead of hardcoded "GPIB-Current"
        bool success = MachineOperations_StartScan(
            m_machineOperations,
            device.c_str(),
            m_dataChannel.c_str(),  // Use the currently selected channel
            stepSizes.data(),
            static_cast<int>(stepSizes.size()),
            settlingTimeMs,
            axesCStr.data(),
            static_cast<int>(axesCStr.size()),
            callerContext.c_str());

        if (m_logger) {
            if (success) {
                m_logger->LogInfo("RealtimeChart: Scan started successfully for " + device + " using channel: " + m_dataChannel);
            }
            else {
                m_logger->LogError("RealtimeChart: Failed to start scan for " + device + " using channel: " + m_dataChannel);
            }
        }

    }
    catch (const std::exception& e) {
        if (m_logger) {
            m_logger->LogError("RealtimeChart: Exception executing scan: " + std::string(e.what()));
        }
    }
}

bool RealtimeChartPage::isDeviceScanning(const std::string& deviceName) {
  if (!m_machineOperations) {
    return false;
  }

  try {


    // Call the wrapper to check if scan is active
    bool isActive = MachineOperations_IsScanActive(m_machineOperations, deviceName.c_str());

    return isActive;
  }
  catch (const std::exception& e) {
    if (m_logger) {
      m_logger->LogError("RealtimeChart: Exception checking scan status: " + std::string(e.what()));
    }
    return false;
  }
}


void RealtimeChartPage::updateAvailableChannels() {
  if (!m_dataStore) return;

  // Get channels from data store
  std::vector<std::string> storeChannels = m_dataStore->GetAvailableChannels();

  // Only update if we have new channels
  if (storeChannels.size() > m_availableChannels.size()) {
    m_availableChannels = storeChannels;

    auto it = std::find(m_availableChannels.begin(), m_availableChannels.end(), m_dataChannel);
    if (it != m_availableChannels.end()) {
      auto distance = std::distance(m_availableChannels.begin(), it);
      if (distance <= INT_MAX) {
        m_selectedChannelIndex = static_cast<int>(distance);
      }
    }
    else {
      m_selectedChannelIndex = 0;
      if (!m_availableChannels.empty()) {
        m_dataChannel = m_availableChannels[0];
      }
    }
  }
}

void RealtimeChartPage::renderChannelSelector() {
  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();
  int topSectionHeight = (int)(screenHeight * 0.6f);

  // Position under the large digital value display
  int selectorX = screenWidth / 2 - 100;  // Center horizontally
  int selectorY = topSectionHeight - 120;  // Above the STOP button area
  int selectorWidth = 200;
  int selectorHeight = 30;

  Rectangle selectorRect = { (float)selectorX, (float)selectorY, (float)selectorWidth, (float)selectorHeight };

  // Draw background with border
  DrawRectangleRec(selectorRect, Color{ 60, 60, 70, 255 });
  DrawRectangleLinesEx(selectorRect, 2, WHITE);

  // Draw current channel name
  Font font = m_fontLoaded ? m_customFont : GetFontDefault();
  DrawTextEx(font, m_dataChannel.c_str(), Vector2{ (float)selectorX + 10, (float)selectorY + 8 }, 16, 2, WHITE);

  // Draw dropdown arrow on the right
  DrawTextEx(font, "▼", Vector2{ (float)(selectorX + selectorWidth - 25), (float)selectorY + 8 }, 16, 2, LIGHTGRAY);

  // Handle click to cycle through channels
  Vector2 mousePos = GetMousePosition();
  bool isHovered = CheckCollisionPointRec(mousePos, selectorRect);

  // Highlight on hover
  if (isHovered) {
    DrawRectangleLinesEx(selectorRect, 2, YELLOW);
  }


  if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      // Cycle to next channel
      if (!m_availableChannels.empty()) {
          std::string oldChannel = m_dataChannel;  // Store old channel

          m_selectedChannelIndex = (m_selectedChannelIndex + 1) % m_availableChannels.size();
          m_dataChannel = m_availableChannels[m_selectedChannelIndex];

          // Clear old data when changing channels
          m_dataBuffer.clear();

          if (m_logger) {
              m_logger->LogInfo("RealtimeChart: Channel changed from '" + oldChannel + "' to '" + m_dataChannel + "'");

              // Check if new channel has data
              if (m_dataStore && m_dataStore->HasValue(m_dataChannel)) {
                  float value = m_dataStore->GetValue(m_dataChannel);
                  m_logger->LogInfo("RealtimeChart: New channel '" + m_dataChannel + "' has data: " + std::to_string(value));
              }
              else {
                  m_logger->LogWarning("RealtimeChart: New channel '" + m_dataChannel + "' has no data available");
              }
          }
      }
  }

  // Show channel info below the selector (smaller text)
  char channelInfo[64];
  snprintf(channelInfo, sizeof(channelInfo), "%d/%d",
    m_selectedChannelIndex + 1, (int)m_availableChannels.size());
  DrawTextEx(font, channelInfo, Vector2{ (float)selectorX + selectorWidth + 10, (float)selectorY + 8 }, 12, 2, LIGHTGRAY);
}




void RealtimeChartPage::renderChannelList(Rectangle panel, Font font) {
  // Setup scrollable area
  float listStartY = panel.y + 45;
  float filterButtonsHeight = 35;  // Space for filter buttons
  float listHeight = panel.height - 70 - filterButtonsHeight;  // Adjusted for filter buttons

  // Render filter buttons
  renderFilterButtons(panel, font);

  // Update filtered list based on active filter
  updateFilteredChannels();

  // Use filtered channels for display
  std::vector<std::string>& displayChannels = m_activeFilter.empty() ?
    m_availableChannels : m_filteredChannels;

  int itemHeight = 30;
  int itemSpacing = 2;
  int totalItemsHeight = static_cast<int>(displayChannels.size()) * (itemHeight + itemSpacing);

  // Handle mouse wheel scrolling
  Rectangle scrollArea = { panel.x, listStartY + filterButtonsHeight, panel.width, listHeight };
  if (CheckCollisionPointRec(GetMousePosition(), scrollArea)) {
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
      m_channelListScrollOffset -= wheel * 30;

      // Clamp scroll bounds
      float maxScroll = std::max(0.0f, totalItemsHeight - listHeight);
      m_channelListScrollOffset = std::max(0.0f, std::min(m_channelListScrollOffset, maxScroll));
    }
  }

  // Setup clipping rectangle for list items
  Rectangle listArea = { panel.x + 5, listStartY + filterButtonsHeight,
                        panel.width - 25, listHeight };
  BeginScissorMode((int)listArea.x, (int)listArea.y, (int)listArea.width, (int)listArea.height);

  // Render visible channel items
  float currentY = listArea.y - m_channelListScrollOffset;
  Vector2 mousePos = GetMousePosition();

  for (size_t i = 0; i < displayChannels.size(); ++i) {
    Rectangle itemRect = { listArea.x, currentY, listArea.width, (float)itemHeight };

    // Only render if visible
    if (currentY + itemHeight >= listArea.y && currentY <= listArea.y + listHeight) {
      renderChannelItem(itemRect, displayChannels[i], i, mousePos, font);
    }

    currentY += itemHeight + itemSpacing;
  }

  EndScissorMode();

  // Draw scrollbar if needed
  if (totalItemsHeight > listHeight) {
    renderScrollbar(panel, listArea.y, listHeight, static_cast<float>(totalItemsHeight));
  }

  // Show channel count at bottom
  char channelInfo[64];
  if (m_activeFilter.empty()) {
    snprintf(channelInfo, sizeof(channelInfo), "%d/%zu channels",
      m_selectedChannelIndex + 1, displayChannels.size());
  }
  else {
    snprintf(channelInfo, sizeof(channelInfo), "%zu filtered (%s)",
      displayChannels.size(), m_activeFilter.c_str());
  }
  DrawTextEx(font, channelInfo,
    Vector2{ panel.x + 10, panel.y + panel.height - 25 },
    12, 2, GRAY);
}


void RealtimeChartPage::renderFilterButtons(Rectangle panel, Font font) {
  float buttonY = panel.y + 45;
  float buttonHeight = 28;
  float buttonSpacing = 5;
  float buttonWidth = (panel.width - 20 - buttonSpacing * 3) / 4;  // 4 buttons: All, Gantry, Hex, Table

  std::vector<std::string> filters = { "All", "Gantry", "Hex", "Table" };
  Vector2 mousePos = GetMousePosition();

  for (size_t i = 0; i < filters.size(); ++i) {
    float buttonX = panel.x + 5 + (buttonWidth + buttonSpacing) * i;
    Rectangle buttonRect = { buttonX, buttonY, buttonWidth, buttonHeight };

    // Check if this filter is active
    bool isActive = (i == 0 && m_activeFilter.empty()) ||
      (i > 0 && m_activeFilter == filters[i]);
    bool isHovered = CheckCollisionPointRec(mousePos, buttonRect);

    // Button colors
    Color bgColor = isActive ? Color{ 80, 120, 160, 255 } :
      (isHovered ? Color{ 60, 60, 70, 255 } : Color{ 45, 45, 55, 255 });
    Color borderColor = isActive ? Color{ 100, 150, 200, 255 } : DARKGRAY;
    Color textColor = isActive ? WHITE : LIGHTGRAY;

    // Draw button
    DrawRectangleRec(buttonRect, bgColor);
    DrawRectangleLinesEx(buttonRect, static_cast<float>(isActive ? 2 : 1), borderColor);

    // Draw text (smaller font for filter buttons)
    const char* filterText = filters[i].c_str();
    Vector2 textSize = MeasureTextEx(font, filterText, 12, 1);
    Vector2 textPos = {
        buttonRect.x + buttonRect.width / 2 - textSize.x / 2,
        buttonRect.y + buttonRect.height / 2 - textSize.y / 2
    };
    DrawTextEx(font, filterText, textPos, 12, 1, textColor);

    // Handle click
    if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      if (i == 0) {
        m_activeFilter = "";  // Show all
      }
      else {
        m_activeFilter = filters[i];
      }
      m_channelListScrollOffset = 0;  // Reset scroll when changing filter

      if (m_logger) {
        m_logger->LogInfo("Channel filter changed to: " +
          (m_activeFilter.empty() ? "All" : m_activeFilter));
      }
    }
  }
}


void RealtimeChartPage::updateFilteredChannels() {
  if (m_activeFilter.empty()) {
    return;  // No filtering needed
  }

  m_filteredChannels.clear();
  std::string filterLower = m_activeFilter;
  std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);

  for (const auto& channel : m_availableChannels) {
    std::string channelLower = channel;
    std::transform(channelLower.begin(), channelLower.end(), channelLower.begin(), ::tolower);

    // Check if channel contains the filter keyword
    if (channelLower.find(filterLower) != std::string::npos) {
      m_filteredChannels.push_back(channel);
    }
  }
}



void RealtimeChartPage::renderChannelItem(Rectangle rect, const std::string& channelName,
  size_t index, Vector2 mousePos, Font font) {
  bool isSelected = (m_dataChannel == channelName);
  bool isHovered = CheckCollisionPointRec(mousePos, rect);

  // Background color
  Color bgColor = isSelected ? Color{ 60, 90, 120, 255 } :
    (isHovered ? Color{ 50, 50, 60, 255 } : Color{ 35, 35, 45, 255 });

  DrawRectangleRec(rect, bgColor);
  if (isSelected) {
    DrawRectangleLinesEx(rect, 2, Color{ 100, 150, 200, 255 });
  }

  // Truncate long channel names
  std::string displayName = channelName;
  if (displayName.length() > 22) {
    displayName = displayName.substr(0, 19) + "...";
  }

  // Highlight the filter keyword if active
  if (!m_activeFilter.empty() && m_activeFilter != "All") {
    // Draw channel name with highlighted keyword
    std::string lowerChannel = channelName;
    std::string lowerFilter = m_activeFilter;
    std::transform(lowerChannel.begin(), lowerChannel.end(), lowerChannel.begin(), ::tolower);
    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);

    size_t pos = lowerChannel.find(lowerFilter);
    if (pos != std::string::npos) {
      // Draw prefix
      if (pos > 0) {
        std::string prefix = displayName.substr(0, pos);
        DrawTextEx(font, prefix.c_str(),
          Vector2{ rect.x + 10, rect.y + 8 },
          14, 2, isSelected ? WHITE : LIGHTGRAY);

        Vector2 prefixSize = MeasureTextEx(font, prefix.c_str(), 14, 2);

        // Draw highlighted part
        std::string highlighted = displayName.substr(pos, m_activeFilter.length());
        DrawTextEx(font, highlighted.c_str(),
          Vector2{ rect.x + 10 + prefixSize.x, rect.y + 8 },
          14, 2, isSelected ? YELLOW : Color{ 255, 200, 100, 255 });

        Vector2 highlightSize = MeasureTextEx(font, highlighted.c_str(), 14, 2);

        // Draw suffix
        if (pos + m_activeFilter.length() < displayName.length()) {
          std::string suffix = displayName.substr(pos + m_activeFilter.length());
          DrawTextEx(font, suffix.c_str(),
            Vector2{ rect.x + 10 + prefixSize.x + highlightSize.x, rect.y + 8 },
            14, 2, isSelected ? WHITE : LIGHTGRAY);
        }
      }
    }
    else {
      // Normal draw if keyword not found
      DrawTextEx(font, displayName.c_str(),
        Vector2{ rect.x + 10, rect.y + 8 },
        14, 2, isSelected ? WHITE : LIGHTGRAY);
    }
  }
  else {
    // Normal draw without highlighting
    DrawTextEx(font, displayName.c_str(),
      Vector2{ rect.x + 10, rect.y + 8 },
      14, 2, isSelected ? WHITE : LIGHTGRAY);
  }

  // Handle click
  if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    m_dataChannel = channelName;

    // Find the actual index in the full list
    auto it = std::find(m_availableChannels.begin(), m_availableChannels.end(), channelName);
    if (it != m_availableChannels.end()) {
      m_selectedChannelIndex = std::distance(m_availableChannels.begin(), it);
    }

    m_dataBuffer.clear();

    if (m_logger) {
      m_logger->LogInfo("Channel selected: " + m_dataChannel);
    }
  }
}



void RealtimeChartPage::renderScrollbar(Rectangle panel, float listStartY,
  float listHeight, float totalHeight) {
  // Calculate scrollbar dimensions
  float scrollbarWidth = 12;
  float scrollbarX = panel.x + panel.width - scrollbarWidth - 5;
  float scrollRatio = listHeight / totalHeight;
  float scrollbarHeight = std::max(20.0f, listHeight * scrollRatio);
  float scrollProgress = m_channelListScrollOffset / (totalHeight - listHeight);
  float scrollbarY = listStartY + scrollProgress * (listHeight - scrollbarHeight);

  // Draw scrollbar track
  DrawRectangle(static_cast<int>(scrollbarX), static_cast<int>(listStartY), static_cast<int>(scrollbarWidth), static_cast<int>(listHeight),
    Color{ 40, 40, 45, 255 });

  // Draw scrollbar thumb
  Rectangle scrollThumb = { scrollbarX, scrollbarY, scrollbarWidth, scrollbarHeight };
  DrawRectangleRec(scrollThumb, Color{ 100, 100, 120, 200 });

  // Draw scroll indicators
  if (m_channelListScrollOffset > 0) {
    DrawTriangle(
      Vector2{ scrollbarX + scrollbarWidth / 2, listStartY - 8 },
      Vector2{ scrollbarX + 2, listStartY - 2 },
      Vector2{ scrollbarX + scrollbarWidth - 2, listStartY - 2 },
      GRAY
    );
  }

  if (m_channelListScrollOffset < totalHeight - listHeight - 1) {
    DrawTriangle(
      Vector2{ scrollbarX + 2, listStartY + listHeight + 2 },
      Vector2{ scrollbarX + scrollbarWidth - 2, listStartY + listHeight + 2 },
      Vector2{ scrollbarX + scrollbarWidth / 2, listStartY + listHeight + 8 },
      GRAY
    );
  }
}

void RealtimeChartPage::renderChartPanel(Rectangle panel, Font font) {
  DrawRectangleRec(panel, Color{ 20, 20, 30, 255 });
  DrawRectangleLinesEx(panel, 2, DARKGRAY);

  // Chart title and info
  DrawTextEx(font, "10 Second History", Vector2{ panel.x + 10, panel.y + 10 }, 16, 2, WHITE);

  char pointsText[32];
  snprintf(pointsText, sizeof(pointsText), "Points: %zu", m_dataBuffer.size());
  DrawTextEx(font, pointsText, Vector2{ panel.x + panel.width - 100, panel.y + 10 }, 12, 2, GRAY);

  // Chart area with margins
  Rectangle chartArea = { panel.x + 50, panel.y + 40,
                         panel.width - 70, panel.height - 60 };

  // Draw grid
  drawChartGrid(chartArea);

  // Draw axes
  drawChartAxes(chartArea);

  // Check if we have data
  if (m_dataBuffer.empty()) {
    const char* noDataText = "No data available";
    Vector2 textSize = MeasureTextEx(font, noDataText, 20, 2);
    DrawTextEx(font, noDataText,
      Vector2{ chartArea.x + chartArea.width / 2 - textSize.x / 2,
              chartArea.y + chartArea.height / 2 - textSize.y / 2 },
      20, 2, GRAY);
    return;
  }

  if (m_dataBuffer.size() < 2) return;

  // Calculate data ranges
  auto [minValue, maxValue] = calculateDataRange();
  auto [minTime, maxTime] = calculateTimeRange();

  // Draw data line
  drawDataLine(chartArea, minValue, maxValue, minTime, maxTime);

  // Draw axis labels
  drawAxisLabels(chartArea, minValue, maxValue, font);
}

void RealtimeChartPage::drawChartGrid(Rectangle chartArea) {
  int gridLinesX = 10;
  int gridLinesY = 5;

  for (int i = 0; i <= gridLinesX; i++) {
    float x = chartArea.x + (chartArea.width / gridLinesX) * i;
    DrawLineEx(Vector2{ x, chartArea.y },
      Vector2{ x, chartArea.y + chartArea.height },
      1, Color{ 50, 50, 60, 100 });
  }

  for (int i = 0; i <= gridLinesY; i++) {
    float y = chartArea.y + (chartArea.height / gridLinesY) * i;
    DrawLineEx(Vector2{ chartArea.x, y },
      Vector2{ chartArea.x + chartArea.width, y },
      1, Color{ 50, 50, 60, 100 });
  }
}

void RealtimeChartPage::drawChartAxes(Rectangle chartArea) {
  // X-axis
  DrawLineEx(Vector2{ chartArea.x, chartArea.y + chartArea.height },
    Vector2{ chartArea.x + chartArea.width, chartArea.y + chartArea.height },
    2, WHITE);
  // Y-axis
  DrawLineEx(Vector2{ chartArea.x, chartArea.y },
    Vector2{ chartArea.x, chartArea.y + chartArea.height },
    2, WHITE);
}

std::pair<float, float> RealtimeChartPage::calculateDataRange() {
  auto minMaxValue = std::minmax_element(m_dataBuffer.begin(), m_dataBuffer.end(),
    [](const DataPoint& a, const DataPoint& b) { return a.value < b.value; });

  float minValue = minMaxValue.first->value;
  float maxValue = minMaxValue.second->value;

  float range = maxValue - minValue;
  if (range < 1e-12f) range = 1e-12f;

  minValue -= range * 0.1f;
  maxValue += range * 0.1f;

  return { minValue, maxValue };
}

std::pair<double, double> RealtimeChartPage::calculateTimeRange() {
  double minTime = m_dataBuffer.front().timestamp;
  double maxTime = m_dataBuffer.back().timestamp;
  double timeRange = maxTime - minTime;

  if (timeRange < 0.1) timeRange = 0.1;

  return { minTime, maxTime };
}

void RealtimeChartPage::drawDataLine(Rectangle chartArea, float minValue, float maxValue,
  double minTime, double maxTime) {
  double timeRange = maxTime - minTime;
  float valueRange = maxValue - minValue;

  for (size_t i = 1; i < m_dataBuffer.size(); ++i) {
    const auto& prev = m_dataBuffer[i - 1];
    const auto& curr = m_dataBuffer[i];

    float x1 = static_cast<double>(chartArea.x + ((prev.timestamp - minTime) / timeRange) * chartArea.width);
    float y1 = static_cast<double>(chartArea.y + chartArea.height - ((prev.value - minValue) / valueRange) * chartArea.height);
    float x2 = static_cast<double>(chartArea.x + ((curr.timestamp - minTime) / timeRange) * chartArea.width);
    float y2 = static_cast<double>(chartArea.y + chartArea.height - ((curr.value - minValue) / valueRange) * chartArea.height);

    DrawLineEx(Vector2{ x1, y1 }, Vector2{ x2, y2 }, 2.0f, LIME);
  }

  // Draw current value point
  if (!m_dataBuffer.empty()) {
    const auto& last = m_dataBuffer.back();
    float x = chartArea.x + ((last.timestamp - minTime) / timeRange) * chartArea.width;
    float y = chartArea.y + chartArea.height - ((last.value - minValue) / valueRange) * chartArea.height;
    DrawCircle((int)x, (int)y, 4, RED);
  }
}

void RealtimeChartPage::drawAxisLabels(Rectangle chartArea, float minValue, float maxValue, Font font) {
  // Y-axis labels
  auto [scaledMin, unitMin] = getScaledUnit(std::abs(minValue));
  auto [scaledMax, unitMax] = getScaledUnit(std::abs(maxValue));

  char minLabel[32], maxLabel[32];
  snprintf(minLabel, sizeof(minLabel), "%.2f%s",
    (minValue >= 0) ? scaledMin : -scaledMin, unitMin.c_str());
  snprintf(maxLabel, sizeof(maxLabel), "%.2f%s",
    (maxValue >= 0) ? scaledMax : -scaledMax, unitMax.c_str());

  DrawTextEx(font, maxLabel, Vector2{ chartArea.x - 45, chartArea.y }, 12, 2, LIGHTGRAY);
  DrawTextEx(font, minLabel, Vector2{ chartArea.x - 45, chartArea.y + chartArea.height - 15 }, 12, 2, LIGHTGRAY);

  // X-axis labels
  DrawTextEx(font, "0s", Vector2{ chartArea.x, chartArea.y + chartArea.height + 5 }, 12, 2, LIGHTGRAY);
  DrawTextEx(font, "10s", Vector2{ chartArea.x + chartArea.width - 20, chartArea.y + chartArea.height + 5 }, 12, 2, LIGHTGRAY);
}