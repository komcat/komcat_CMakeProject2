// DataChartManager.cpp
#include "include/data/DataChartManager.h"
#include "include/data/global_data_store.h"
#include "include/logger.h"
#include "imgui.h"
#include "implot/implot.h" // Use the main ImPlot header
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

// DataChartManager default constructor
DataChartManager::DataChartManager()
  : m_initialized(false),
  m_implotInitialized(false),
  m_maxPoints(500),
  m_timeWindow(30.0f),
  m_showWindow(true),
  m_windowTitle("Data Charts") {

  // Initialize color palette
  InitializeColors();

  // Get logger instance
  Logger* logger = Logger::GetInstance();
  logger->LogInfo("DataChartManager: Initializing");

  // Initialize chart data
  Initialize();
}

// Constructor with config file path
DataChartManager::DataChartManager(const std::string& configFilePath)
  : m_initialized(false),
  m_implotInitialized(false),
  m_maxPoints(500),
  m_timeWindow(30.0f),
  m_showWindow(true),
  m_windowTitle("Data Charts"),
  m_configFilePath(configFilePath) {

  // Initialize color palette
  InitializeColors();

  // Get logger instance
  Logger* logger = Logger::GetInstance();
  logger->LogInfo("DataChartManager: Initializing with config file: " + configFilePath);

  // Load config file
  if (!LoadConfig(configFilePath)) {
    logger->LogWarning("DataChartManager: Failed to load config file, using default initialization");
    Initialize();
  }
}

// DataChartManager destructor
DataChartManager::~DataChartManager() {
  // Get logger instance
  Logger* logger = Logger::GetInstance();
  logger->LogInfo("DataChartManager: Shutting down");

  // Clean up
  m_chartBuffers.clear();

  // Shut down ImPlot if we initialized it
  ShutdownImPlot();
}

// Initialize ImPlot
void DataChartManager::InitializeImPlot() {
  if (!m_implotInitialized) {
    // Check if ImPlot is already initialized globally
    if (ImPlot::GetCurrentContext() == nullptr) {
      ImPlot::CreateContext();
      m_implotInitialized = true;
      Logger::GetInstance()->LogInfo("DataChartManager: ImPlot initialized");
    }
    else {
      // ImPlot is already initialized elsewhere
      m_implotInitialized = false;
      Logger::GetInstance()->LogInfo("DataChartManager: Using existing ImPlot context");
    }
  }
}

// Shut down ImPlot
void DataChartManager::ShutdownImPlot() {
  if (m_implotInitialized) {
    ImPlot::DestroyContext();
    m_implotInitialized = false;
    Logger::GetInstance()->LogInfo("DataChartManager: ImPlot shut down");
  }
}

// Initialize color palette
void DataChartManager::InitializeColors() {
  // Define a set of visually distinct colors
  m_colors = {
      ImVec4(0.0f, 0.7f, 1.0f, 1.0f),  // Blue
      ImVec4(1.0f, 0.3f, 0.3f, 1.0f),  // Red
      ImVec4(0.0f, 0.8f, 0.2f, 1.0f),  // Green
      ImVec4(1.0f, 0.7f, 0.0f, 1.0f),  // Orange
      ImVec4(0.5f, 0.2f, 0.7f, 1.0f),  // Purple
      ImVec4(0.7f, 0.7f, 0.0f, 1.0f),  // Yellow
      ImVec4(0.0f, 0.6f, 0.6f, 1.0f),  // Teal
      ImVec4(0.9f, 0.4f, 0.7f, 1.0f)   // Pink
  };
}

// Initialize chart data with default values
void DataChartManager::Initialize() {
  // Lock for thread safety
  std::lock_guard<std::mutex> lock(m_dataMutex);

  // Skip if already initialized
  if (!m_chartBuffers.empty()) {
    return;
  }

  // Get logger instance
  Logger* logger = Logger::GetInstance();

  // List of channels to monitor - updated to include 'enable' field
  std::vector<ChannelInfo> channels = {
    { "GPIB-Current", "Current Reading", "A", true, true, m_colors[0] },
    { "Virtual_1", "Virtual Channel 1", "unit", false, true, m_colors[1] },
    { "Virtual_2", "Virtual Channel 2", "unit", false, true, m_colors[2] }
  };

  // Create chart buffers for each channel
  for (size_t i = 0; i < channels.size(); i++) {
    const auto& info = channels[i];

    // Only add enabled channels
    if (info.enable) {
      // Create a chart buffer for this channel
      m_chartBuffers.insert(std::make_pair(
        info.id,
        ChartDataBuffer(
          info.id,
          info.displayName,
          info.unit,
          info.displayUnitSuffix,
          info.color,
          info.enable // Pass the enabled state
        )
      ));

      logger->LogInfo("DataChartManager: Created chart for " + info.displayName);
    }
  }

  m_initialized = true;
  logger->LogInfo("DataChartManager: Initialization complete with " +
    std::to_string(m_chartBuffers.size()) + " channels");
}

// Update chart data from global data store
void DataChartManager::Update() {
  // Lock for thread safety
  std::lock_guard<std::mutex> lock(m_dataMutex);

  // Skip if not initialized
  if (!m_initialized) {
    return;
  }

  double currentTime = ImGui::GetTime();

  // Process each chart buffer
  for (auto& [id, buffer] : m_chartBuffers) {
    // Skip disabled channels
    if (!buffer.enabled) {
      continue;
    }

    // Get current value from GlobalDataStore
    float currentValue = GlobalDataStore::GetInstance()->GetValue(id);

    // Only add new data point if the value has changed or sufficient time has passed
    bool shouldAddPoint = false;

    // If buffer is empty, always add the first point
    if (buffer.values.empty()) {
      shouldAddPoint = true;
    }
    // Check if value has changed significantly (adjust threshold as needed)
    else if (std::abs(currentValue - buffer.values.back()) > 0.0001f) {
      shouldAddPoint = true;
    }
    // Or if enough time has passed since last update (e.g., minimum 100ms)
    else if (buffer.timestamps.empty() ||
      (currentTime - buffer.timestamps.back()) > 0.1) { // 100ms
      shouldAddPoint = true;
    }

    // Add new point if needed
    if (shouldAddPoint) {
      buffer.values.push_back(currentValue);
      buffer.timestamps.push_back(currentTime);

      // Keep the buffer at maximum size
      while (buffer.values.size() > m_maxPoints) {
        buffer.values.pop_front();
        buffer.timestamps.pop_front();
      }
    }
  }
}

// Format value using SI prefixes
std::string DataChartManager::FormatWithSIPrefix(float value, const std::string& unit, bool displayUnitSuffix) const {
  std::string prefix = "";
  float scaledValue = value;

  // Apply SI prefix based on magnitude
  if (std::abs(value) > 0) {  // Avoid processing zero values
    if (std::abs(value) < 0.000000001f) {  // < 1 pA/pV etc.
      scaledValue = value * 1e12f;
      prefix = "p";  // pico
    }
    else if (std::abs(value) < 0.000001f) {  // < 1 nA/nV etc.
      scaledValue = value * 1e9f;
      prefix = "n";  // nano
    }
    else if (std::abs(value) < 0.001f) {  // < 1 uA/uV etc.
      scaledValue = value * 1e6f;
      prefix = "u";  // micro (using u as µ might cause display issues)
    }
    else if (std::abs(value) < 1.0f) {  // < 1 A/V etc.
      scaledValue = value * 1e3f;
      prefix = "m";  // milli
    }
    else if (std::abs(value) >= 1000.0f && std::abs(value) < 1000000.0f) {  // >= 1 kA/kV etc.
      scaledValue = value / 1e3f;
      prefix = "k";  // kilo
    }
    else if (std::abs(value) >= 1000000.0f) {  // >= 1 MA/MV etc.
      scaledValue = value / 1e6f;
      prefix = "M";  // mega
    }
  }

  std::stringstream ss;
  ss << std::fixed << std::setprecision(4) << scaledValue;
  std::string result = ss.str();

  if (displayUnitSuffix && !unit.empty()) {
    result += " " + prefix + unit;
  }

  return result;
}

// Render UI for charts
void DataChartManager::RenderUI() {
  // Skip if window is not visible
  if (!m_showWindow) {
    return;
  }

  try {
    // Verify ImPlot context exists
    if (ImPlot::GetCurrentContext() == nullptr) {
      InitializeImPlot();
      if (ImPlot::GetCurrentContext() == nullptr) {
        // Still null, can't continue
        ImGui::Begin(m_windowTitle.c_str(), &m_showWindow);
        ImGui::Text("Error: ImPlot context not available");
        ImGui::End();
        return;
      }
    }

    // Update data before rendering
    Update();

    // Start ImGui window
    ImGui::Begin(m_windowTitle.c_str(), &m_showWindow);

    // Check if we have any data to display
    bool hasData = false;
    for (const auto& [id, buffer] : m_chartBuffers) {
      if (!buffer.values.empty() && buffer.enabled) {
        hasData = true;
        break;
      }
    }

    // Display a message if no data available
    if (!hasData) {
      ImGui::Text("No data available for plotting. Check your data sources.");
      ImGui::Text("Number of registered data sources: %zu", m_chartBuffers.size());
      for (const auto& [id, buffer] : m_chartBuffers) {
        ImGui::Text("  - %s: %s (Enabled: %s)",
          id.c_str(),
          buffer.values.empty() ? "No data" : "Has data",
          buffer.enabled ? "Yes" : "No");
      }
      ImGui::End();
      return;
    }

    // Controls
    ImGui::SliderFloat("Time Window (s)", &m_timeWindow, 5.0f, 120.0f, "%.1f");

    // Add a control to pause/resume updates
    static bool pauseUpdates = false;
    ImGui::SameLine();
    if (ImGui::Button(pauseUpdates ? "Resume" : "Pause")) {
      pauseUpdates = !pauseUpdates;
    }

    // Add channel visibility controls with multi-column layout
    ImGui::Separator();
    ImGui::Text("Channel Visibility:");

    // Define number of columns (adjust as needed)
    const int numColumns = 3;

    // Count channels with data (that are also enabled)
    std::vector<std::string> channelsWithData;
    for (const auto& [id, buffer] : m_chartBuffers) {
      if (!buffer.values.empty() && buffer.enabled) {
        channelsWithData.push_back(id);
      }
    }

    // Only create table if we have channels with data
    if (!channelsWithData.empty()) {
      // Create a table for the checkboxes
      if (ImGui::BeginTable("##ChannelVisibilityTable", numColumns)) {
        int col = 0;

        // Lock for thread safety while accessing chart data
        std::lock_guard<std::mutex> lock(m_dataMutex);

        for (const auto& id : channelsWithData) {
          auto& buffer = m_chartBuffers[id];

          // Start a new row every 'numColumns' columns
          if (col == 0) {
            ImGui::TableNextRow();
          }

          ImGui::TableNextColumn();

          // Checkbox with colored text for the channel
          ImGui::PushStyleColor(ImGuiCol_Text, buffer.color);
          if (ImGui::Checkbox(buffer.displayName.c_str(), &buffer.visible)) {
            // Checkbox was clicked
          }
          ImGui::PopStyleColor();

          // Move to the next column
          col = (col + 1) % numColumns;
        }

        ImGui::EndTable();
      }
    }

    ImGui::Separator();

    // Display current values for each visible source
    ImGui::Text("Current Values:");

    // Store charts that have data to display and are visible
    std::vector<std::string> activeCharts;

    // Lock for thread safety while accessing chart data
    std::lock_guard<std::mutex> lock(m_dataMutex);

    // Display current values for each visible and enabled source
    if (ImGui::BeginTable("##CurrentValuesTable", numColumns)) {
      int col = 0;

      for (const auto& [id, buffer] : m_chartBuffers) {
        if (!buffer.values.empty() && buffer.visible && buffer.enabled) {
          // Start a new row every 'numColumns' columns
          if (col == 0) {
            ImGui::TableNextRow();
          }

          ImGui::TableNextColumn();

          // Get the latest value
          float currentValue = buffer.values.back();

          // Format with SI prefix
          std::string valueStr = FormatWithSIPrefix(
            currentValue, buffer.unit, buffer.displayUnitSuffix);

          // Display name and value with color
          ImGui::TextColored(
            buffer.color,
            "%s: %s",
            buffer.displayName.c_str(),
            valueStr.c_str()
          );

          // Add to active charts list for plotting
          activeCharts.push_back(id);

          // Move to the next column
          col = (col + 1) % numColumns;
        }
      }

      ImGui::EndTable();
    }

    ImGui::Separator();

    // Only try to create the plot if we have active and visible charts
    if (!activeCharts.empty()) {
      // Get the available content area size for the chart
      ImVec2 contentSize = ImGui::GetContentRegionAvail();

      // Set a minimum height (adjust as needed)
      float chartHeight = std::max(contentSize.y, 200.0f);

      // Create the combined plot with dynamic size
      if (ImPlot::BeginPlot("##DataCharts", ImVec2(contentSize.x, chartHeight))) {
        // Setup axes
        ImPlot::SetupAxes("Time (s)", "Value",
          ImPlotAxisFlags_AutoFit,
          ImPlotAxisFlags_AutoFit);

        // Find the most recent timestamp across all data sets
        double latestTime = ImGui::GetTime();
        double earliestTime = latestTime - m_timeWindow;

        // Set x-axis to scroll with newest data
        ImPlot::SetupAxisLimits(ImAxis_X1,
          earliestTime,
          latestTime,
          ImGuiCond_Always);

        // Plot each visible and enabled data source
        for (const auto& id : activeCharts) {
          auto& buffer = m_chartBuffers[id];

          // Only plot if the buffer is visible and enabled
          if (buffer.visible && buffer.enabled && buffer.values.size() > 1) {
            try {
              // Convert deques to raw arrays for ImPlot
              size_t dataSize = buffer.values.size();

              // Use try-catch to handle potential memory allocation issues
              float* xValues = new float[dataSize];
              float* yValues = new float[dataSize];

              // Copy data from deques to arrays with boundary checking
              for (size_t i = 0; i < dataSize && i < buffer.timestamps.size(); i++) {
                xValues[i] = static_cast<float>(buffer.timestamps[i]);
                yValues[i] = buffer.values[i];
              }

              // Apply styling
              ImPlot::PushStyleColor(ImPlotCol_Line, buffer.color);
              ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.0f);

              // Create an informative label
              std::string label = buffer.displayName;
              if (buffer.displayUnitSuffix && !buffer.unit.empty()) {
                label += " (" + buffer.unit + ")";
              }

              // Use PlotLine with raw arrays
              ImPlot::PlotLine(
                label.c_str(),
                xValues,
                yValues,
                (int)dataSize
              );

              ImPlot::PopStyleVar();
              ImPlot::PopStyleColor();

              // Clean up the temporary arrays
              delete[] xValues;
              delete[] yValues;
            }
            catch (const std::exception& e) {
              Logger::GetInstance()->LogError("Exception plotting data: " + std::string(e.what()));
            }
          }
        }

        ImPlot::EndPlot();
      }
    }

    ImGui::End();
  }
  catch (const std::exception& e) {
    // Log and handle any exceptions
    Logger* logger = Logger::GetInstance();
    logger->LogError("DataChartManager::RenderUI exception: " + std::string(e.what()));

    // Try to end the ImGui window if it was started
    if (ImGui::GetCurrentContext()) {
      ImGui::End();
    }
  }
}

// Create a toggleable UI adapter for the chart manager
std::shared_ptr<ITogglableUI> CreateDataChartManagerUI(DataChartManager& manager) {
  return std::make_shared<TogglableUIAdapter<DataChartManager>>(manager, manager.GetName());
}

// Add a new channel to monitor - updated to use the enabled property
void DataChartManager::AddChannel(const std::string& id, const std::string& displayName,
  const std::string& unit, bool displayUnitSuffix) {
  // Lock for thread safety
  std::lock_guard<std::mutex> lock(m_dataMutex);

  // Skip if channel already exists
  if (m_chartBuffers.find(id) != m_chartBuffers.end()) {
    Logger::GetInstance()->LogWarning("DataChartManager: Channel " + id + " already exists");
    return;
  }

  // Select a color for the new channel
  ImVec4 color = m_colors[m_chartBuffers.size() % m_colors.size()];

  // Add the new channel (enabled by default)
  m_chartBuffers.insert(std::make_pair(
    id,
    ChartDataBuffer(
      id,
      displayName,
      unit,
      displayUnitSuffix,
      color,
      true // Enabled by default
    )
  ));

  Logger::GetInstance()->LogInfo("DataChartManager: Added new channel " + id);
}

// Load config from JSON file
bool DataChartManager::LoadConfig(const std::string& configFilePath) {
  try {
    // Get logger instance
    Logger* logger = Logger::GetInstance();
    logger->LogInfo("DataChartManager: Loading config from: " + configFilePath);

    // Open the config file
    std::ifstream configFile(configFilePath);
    if (!configFile.is_open()) {
      logger->LogError("DataChartManager: Could not open config file: " + configFilePath);
      return false;
    }

    // Parse JSON
    nlohmann::json config;
    try {
      configFile >> config;
    }
    catch (const std::exception& e) {
      logger->LogError("DataChartManager: JSON parsing error: " + std::string(e.what()));
      return false;
    }

    // Lock for thread safety
    std::lock_guard<std::mutex> lock(m_dataMutex);

    // Load settings if present
    if (config.contains("settings")) {
      auto& settings = config["settings"];

      if (settings.contains("maxPoints")) {
        m_maxPoints = settings["maxPoints"].get<int>();
        logger->LogInfo("DataChartManager: Set maxPoints to " + std::to_string(m_maxPoints));
      }

      if (settings.contains("timeWindow")) {
        m_timeWindow = settings["timeWindow"].get<float>();
        logger->LogInfo("DataChartManager: Set timeWindow to " + std::to_string(m_timeWindow));
      }
    }

    // Load channels
    if (config.contains("channels") && config["channels"].is_array()) {
      int channelCount = 0;
      int enabledCount = 0;

      for (auto& channelConfig : config["channels"]) {
        try {
          // Extract channel info
          std::string id = channelConfig["id"].get<std::string>();
          std::string displayName = channelConfig["displayName"].get<std::string>();
          std::string unit = channelConfig["unit"].get<std::string>();
          bool displayUnitSuffix = channelConfig["displayUnitSuffix"].get<bool>();

          // Get enabled state - default to true if not specified
          bool enabled = true;
          if (channelConfig.contains("enable")) {
            enabled = channelConfig["enable"].get<bool>();
          }

          // Always add the channel to the list, but mark it as enabled or disabled
          // This allows channels to be toggled in the UI without reloading config
          ImVec4 color = m_colors[channelCount % m_colors.size()];

          m_chartBuffers.insert(std::make_pair(
            id,
            ChartDataBuffer(
              id,
              displayName,
              unit,
              displayUnitSuffix,
              color,
              enabled
            )
          ));

          if (enabled) {
            logger->LogInfo("DataChartManager: Added enabled channel from config: " + id);
            enabledCount++;
          }
          else {
            logger->LogInfo("DataChartManager: Added disabled channel from config: " + id);
          }

          channelCount++;
        }
        catch (const std::exception& e) {
          logger->LogWarning("DataChartManager: Error adding channel from config: " + std::string(e.what()));
          // Continue with next channel
        }
      }

      logger->LogInfo("DataChartManager: Loaded " + std::to_string(channelCount) +
        " channels from config (" + std::to_string(enabledCount) + " enabled)");
    }
    else {
      logger->LogWarning("DataChartManager: No channels found in config file");
    }

    m_initialized = true;
    return true;
  }
  catch (const std::exception& e) {
    Logger::GetInstance()->LogError("DataChartManager: Error loading config: " + std::string(e.what()));
    return false;
  }
}