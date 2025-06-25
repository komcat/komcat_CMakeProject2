// RealtimeChartPage.cpp
#include "RealtimeChartPage.h"
#include "include/logger.h"
#include "include/data/global_data_store.h"

#include <raylib.h>
#include <algorithm>
#include <cmath>

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
  m_machineOperations(nullptr), m_piControllerManager(nullptr),  // Update variable names
  m_dataChannel("GPIB-Current"), m_timeWindow(10.0f),
  m_currentValue(0.0f), m_scaledValue(0.0f),
  m_leftCoarseState(ScanState::IDLE), m_leftFineState(ScanState::IDLE),
  m_rightCoarseState(ScanState::IDLE), m_rightFineState(ScanState::IDLE) {

  if (m_logger) {
    m_logger->LogInfo("RealtimeChartPage created");
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

  // Render chart (bottom 40%)
  renderChart();
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
    return { absValue * 1e6f, "μA" };
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

  // Background for digital display area
  DrawRectangle(0, 70, screenWidth, topSectionHeight - 70, Color{ 30, 30, 40, 255 });
  DrawRectangleLines(0, 70, screenWidth, topSectionHeight - 70, DARKGRAY);

  // Channel name
  int channelFontSize = 24;
  Font font = m_fontLoaded ? m_customFont : GetFontDefault();

  Vector2 channelTextSize = MeasureTextEx(font, m_dataChannel.c_str(), channelFontSize, 2);
  int channelX = screenWidth / 2 - (int)channelTextSize.x / 2;
  DrawTextEx(font, m_dataChannel.c_str(), Vector2{ (float)channelX, 100 }, channelFontSize, 2, LIGHTGRAY);

  // Digital value display
  char valueText[64];
  snprintf(valueText, sizeof(valueText), "%.3f %s", m_scaledValue, m_displayUnit.c_str());

  int valueFontSize = 48;
  Vector2 valueTextSize = MeasureTextEx(font, valueText, valueFontSize, 2);
  int valueX = screenWidth / 2 - (int)valueTextSize.x / 2;
  int valueY = topSectionHeight / 2 - valueFontSize / 2;

  // Value color based on magnitude
  Color valueColor = GREEN;
  if (std::abs(m_currentValue) < 1e-9f) {
    valueColor = GRAY;
  }
  else if (std::abs(m_currentValue) > 1e-3f) {
    valueColor = ORANGE;
  }

  DrawTextEx(font, valueText, Vector2{ (float)valueX, (float)valueY }, valueFontSize, 2, valueColor);

  // Data points info
  char infoText[32];
  snprintf(infoText, sizeof(infoText), "Points: %zu", m_dataBuffer.size());
  DrawText(infoText, 20, topSectionHeight - 30, 16, DARKGRAY);
}

void RealtimeChartPage::renderButtons() {
  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();
  int topSectionHeight = (int)(screenHeight * 0.6f);

  // Button dimensions
  int buttonWidth = 120;
  int buttonHeight = 50;
  int buttonSpacing = 20;

  // Left side buttons
  int leftX = 30;
  int leftCoarseY = 150;
  int leftFineY = leftCoarseY + buttonHeight + buttonSpacing;

  Rectangle leftCoarseBtn = { (float)leftX, (float)leftCoarseY, (float)buttonWidth, (float)buttonHeight };
  Rectangle leftFineBtn = { (float)leftX, (float)leftFineY, (float)buttonWidth, (float)buttonHeight };

  // Right side buttons  
  int rightX = screenWidth - buttonWidth - 30;
  int rightCoarseY = 150;
  int rightFineY = rightCoarseY + buttonHeight + buttonSpacing;

  Rectangle rightCoarseBtn = { (float)rightX, (float)rightCoarseY, (float)buttonWidth, (float)buttonHeight };
  Rectangle rightFineBtn = { (float)rightX, (float)rightFineY, (float)buttonWidth, (float)buttonHeight };

  // Stop button (center, below value display)
  int stopWidth = 100;
  int stopHeight = 40;
  int stopX = screenWidth / 2 - stopWidth / 2;
  int stopY = topSectionHeight - 80;

  Rectangle stopBtn = { (float)stopX, (float)stopY, (float)stopWidth, (float)stopHeight };

  // Draw buttons
  drawButton(leftCoarseBtn, "Left Coarse", m_leftCoarseState);
  drawButton(leftFineBtn, "Left Fine", m_leftFineState);
  drawButton(rightCoarseBtn, "Right Coarse", m_rightCoarseState);
  drawButton(rightFineBtn, "Right Fine", m_rightFineState);

  // Stop button (always red when any scanning is active)
  bool anyScanning = (m_leftCoarseState == ScanState::SCANNING ||
    m_leftFineState == ScanState::SCANNING ||
    m_rightCoarseState == ScanState::SCANNING ||
    m_rightFineState == ScanState::SCANNING);

  Color stopColor = anyScanning ? RED : DARKGRAY;
  Vector2 mousePos = GetMousePosition();
  bool stopHovered = CheckCollisionPointRec(mousePos, stopBtn);

  if (stopHovered && anyScanning) {
    stopColor = MAROON; // Darker red when hovered
  }

  DrawRectangleRec(stopBtn, stopColor);
  DrawRectangleLinesEx(stopBtn, 2, BLACK);

  // Draw stop button text with custom font
  Font font = m_fontLoaded ? m_customFont : GetFontDefault();
  const char* stopText = "STOP";
  int fontSize = 16;
  Vector2 stopTextSize = MeasureTextEx(font, stopText, fontSize, 2);
  Vector2 stopTextPos = {
    stopBtn.x + stopBtn.width / 2 - stopTextSize.x / 2,
    stopBtn.y + stopBtn.height / 2 - stopTextSize.y / 2
  };

  DrawTextEx(font, stopText, stopTextPos, fontSize, 2, WHITE);
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
  Vector2 textSize = MeasureTextEx(font, text, fontSize, 2);
  Vector2 textPos = {
    rect.x + rect.width / 2 - textSize.x / 2,
    rect.y + rect.height / 2 - textSize.y / 2
  };

  DrawTextEx(font, text, textPos, fontSize, 2, textColor);

  // Return if button was clicked
  return isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void RealtimeChartPage::handleButtonClicks() {
  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();
  int topSectionHeight = (int)(screenHeight * 0.6f);

  // Button dimensions (same as in renderButtons)
  int buttonWidth = 120;
  int buttonHeight = 50;
  int buttonSpacing = 20;

  // Left side buttons
  int leftX = 30;
  int leftCoarseY = 150;
  int leftFineY = leftCoarseY + buttonHeight + buttonSpacing;

  Rectangle leftCoarseBtn = { (float)leftX, (float)leftCoarseY, (float)buttonWidth, (float)buttonHeight };
  Rectangle leftFineBtn = { (float)leftX, (float)leftFineY, (float)buttonWidth, (float)buttonHeight };

  // Right side buttons  
  int rightX = screenWidth - buttonWidth - 30;
  int rightCoarseY = 150;
  int rightFineY = rightCoarseY + buttonHeight + buttonSpacing;

  Rectangle rightCoarseBtn = { (float)rightX, (float)rightCoarseY, (float)buttonWidth, (float)buttonHeight };
  Rectangle rightFineBtn = { (float)rightX, (float)rightFineY, (float)buttonWidth, (float)buttonHeight };

  // Stop button
  int stopWidth = 100;
  int stopHeight = 40;
  int stopX = screenWidth / 2 - stopWidth / 2;
  int stopY = topSectionHeight - 80;

  Rectangle stopBtn = { (float)stopX, (float)stopY, (float)stopWidth, (float)stopHeight };

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
  Vector2 mousePos = GetMousePosition();
  bool stopHovered = CheckCollisionPointRec(mousePos, stopBtn);
  if (stopHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    stopAllScanning();
  }
}



void RealtimeChartPage::startHexLeftCoarseScan() {
  if (m_logger) {
    m_logger->LogInfo("Starting Hex-Left Coarse Scan");
  }

  // Execute RunScanOperation with coarse preset
  executeRunScanOperation("hex-left", { 0.005, 0.001, 0.0005 });
  m_leftCoarseState = ScanState::SCANNING;
}

void RealtimeChartPage::startHexLeftFineScan() {
  if (m_logger) {
    m_logger->LogInfo("Starting Hex-Left Fine Scan");
  }

  // Execute RunScanOperation with fine preset
  executeRunScanOperation("hex-left", { 0.0005, 0.0002 });
  m_leftFineState = ScanState::SCANNING;
}

void RealtimeChartPage::startHexRightCoarseScan() {
  if (m_logger) {
    m_logger->LogInfo("Starting Hex-Right Coarse Scan");
  }

  // Execute RunScanOperation with coarse preset
  executeRunScanOperation("hex-right", { 0.005, 0.001, 0.0005 });
  m_rightCoarseState = ScanState::SCANNING;
}

void RealtimeChartPage::startHexRightFineScan() {
  if (m_logger) {
    m_logger->LogInfo("Starting Hex-Right Fine Scan");
  }

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
  if (m_dataBuffer.empty()) return;

  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();
  int topSectionHeight = (int)(screenHeight * 0.6f);
  int chartY = topSectionHeight;
  int chartHeight = screenHeight - topSectionHeight;

  // Chart area background
  Rectangle chartArea = { 20, (float)chartY + 20, (float)screenWidth - 40, (float)chartHeight - 40 };
  DrawRectangleRec(chartArea, Color{ 20, 20, 30, 255 });
  DrawRectangleLinesEx(chartArea, 2, DARKGRAY);

  // Chart title
  DrawText("10 Second History", 30, chartY + 5, 16, WHITE);

  if (m_dataBuffer.size() < 2) return;

  // Find data range
  auto minMaxValue = std::minmax_element(m_dataBuffer.begin(), m_dataBuffer.end(),
    [](const DataPoint& a, const DataPoint& b) { return a.value < b.value; });

  float minValue = minMaxValue.first->value;
  float maxValue = minMaxValue.second->value;

  // Add some padding to the range
  float range = maxValue - minValue;
  if (range < 1e-12f) range = 1e-12f; // Prevent division by zero
  minValue -= range * 0.1f;
  maxValue += range * 0.1f;

  // Get time range
  double minTime = m_dataBuffer.front().timestamp;
  double maxTime = m_dataBuffer.back().timestamp;
  double timeRange = maxTime - minTime;
  if (timeRange < 0.1) timeRange = 0.1; // Minimum time range

  // Draw chart lines
  for (size_t i = 1; i < m_dataBuffer.size(); ++i) {
    const auto& prev = m_dataBuffer[i - 1];
    const auto& curr = m_dataBuffer[i];

    // Map to screen coordinates
    float x1 = chartArea.x + ((prev.timestamp - minTime) / timeRange) * chartArea.width;
    float y1 = chartArea.y + chartArea.height - ((prev.value - minValue) / (maxValue - minValue)) * chartArea.height;

    float x2 = chartArea.x + ((curr.timestamp - minTime) / timeRange) * chartArea.width;
    float y2 = chartArea.y + chartArea.height - ((curr.value - minValue) / (maxValue - minValue)) * chartArea.height;

    DrawLineEx(Vector2{ x1, y1 }, Vector2{ x2, y2 }, 2.0f, LIME);
  }

  // Draw current value point
  if (!m_dataBuffer.empty()) {
    const auto& last = m_dataBuffer.back();
    float x = chartArea.x + ((last.timestamp - minTime) / timeRange) * chartArea.width;
    float y = chartArea.y + chartArea.height - ((last.value - minValue) / (maxValue - minValue)) * chartArea.height;
    DrawCircle((int)x, (int)y, 4, RED);
  }

  // Y-axis labels
  auto [scaledMin, unitMin] = getScaledUnit(std::abs(minValue));
  auto [scaledMax, unitMax] = getScaledUnit(std::abs(maxValue));

  char minLabel[32], maxLabel[32];
  snprintf(minLabel, sizeof(minLabel), "%.2f%s", (minValue >= 0) ? scaledMin : -scaledMin, unitMin.c_str());
  snprintf(maxLabel, sizeof(maxLabel), "%.2f%s", (maxValue >= 0) ? scaledMax : -scaledMax, unitMax.c_str());

  DrawText(maxLabel, 25, (int)chartArea.y + 5, 12, LIGHTGRAY);
  DrawText(minLabel, 25, (int)(chartArea.y + chartArea.height - 15), 12, LIGHTGRAY);
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
      m_logger->LogInfo("  Data channel: GPIB-Current");
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

    // Call the C wrapper function for StartScan (non-blocking)
    bool success = MachineOperations_StartScan(
      m_machineOperations,
      device.c_str(),
      "GPIB-Current",
      stepSizes.data(),
      static_cast<int>(stepSizes.size()),
      settlingTimeMs,
      axesCStr.data(),
      static_cast<int>(axesCStr.size()),
      callerContext.c_str());

    if (m_logger) {
      if (success) {
        m_logger->LogInfo("RealtimeChart: Scan started successfully for " + device);
      }
      else {
        m_logger->LogError("RealtimeChart: Failed to start scan for " + device);
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