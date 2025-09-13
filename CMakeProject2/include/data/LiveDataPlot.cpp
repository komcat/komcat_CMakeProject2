#include "LiveDataPlot.h"
#include "global_data_store.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <map>
#include "implot/implot.h"

// Constructor
LiveDataPlot::LiveDataPlot(const std::string& id)
  : m_id(id)
  , m_currentValue(0.0f)
  , m_autoMinY(-1.0f)
  , m_autoMaxY(1.0f)
  , m_needsAutoScale(true)
  , m_initialized(false)
  , m_plotContext(nullptr)
  , m_showChannelSelector(false)
  , m_selectedChannelIndex(-1) {

  m_startTime = std::chrono::steady_clock::now();
  m_lastUpdateTime = m_startTime;
}

// Destructor
LiveDataPlot::~LiveDataPlot() {
  // Cleanup if needed
}

// Initialize
void LiveDataPlot::Initialize(const Config& config) {
  std::lock_guard<std::mutex> lock(m_dataMutex);

  m_config = config;
  m_detectedUnit = DetectUnit();

  // Reserve space for history
  m_dataHistory.clear();

  // Initialize available channels
  m_availableChannels = GetAvailableChannels();

  m_initialized = true;
  m_needsAutoScale = true;
}

// Render the plot
void LiveDataPlot::Render(const ImVec2& size) {
  if (!m_initialized) {
    ImGui::Text("Plot not initialized");
    return;
  }

  // Update data from GlobalDataStore
  UpdateData();

  // Create unique plot ID
  std::string plotId = "##LivePlot_" + m_id;

  // Start plot
  if (ImPlot::BeginPlot(plotId.c_str(), size)) {
    // Setup axes
    std::string yLabel = m_config.yAxisLabel.empty() ? m_detectedUnit : m_config.yAxisLabel;
    ImPlot::SetupAxes("Time (s)", yLabel.c_str());

    // Set axis limits
    double currentTime = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - m_startTime).count();
    double xMin = std::max(0.0, currentTime - m_config.timeWindow);
    double xMax = currentTime;

    ImPlot::SetupAxisLimits(ImAxis_X1, xMin, xMax, ImPlotCond_Always);

    if (m_config.autoScale) {
      CalculateAutoScale();
      ImPlot::SetupAxisLimits(ImAxis_Y1, m_autoMinY, m_autoMaxY, ImPlotCond_Always);
    }
    else {
      ImPlot::SetupAxisLimits(ImAxis_Y1, m_config.minValue, m_config.maxValue, ImPlotCond_Always);
    }

    // MOVE LEGEND SETUP HERE - INSIDE THE PLOT CONTEXT
    if (m_config.showLegend) {
      ImPlot::SetupLegend(ImPlotLocation_NorthEast);
    }

    // Show grid - REMOVE THIS DUPLICATE SETUP
    // ImPlot::SetupAxes("Time (s)", yLabel.c_str(),
    //   m_config.showGrid ? ImPlotAxisFlags_None : ImPlotAxisFlags_NoGridLines,
    //   m_config.showGrid ? ImPlotAxisFlags_None : ImPlotAxisFlags_NoGridLines);

    // Prepare data for plotting
    std::vector<double> xData;
    std::vector<double> yData;

    {
      std::lock_guard<std::mutex> lock(m_dataMutex);

      for (const auto& point : m_dataHistory) {
        if (point.timestamp >= xMin) {
          xData.push_back(point.timestamp);
          yData.push_back(point.value);
        }
      }
    }

    // Plot the data line
    if (!xData.empty()) {
      ImPlot::PushStyleColor(ImPlotCol_Line, m_config.lineColor);
      ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, m_config.lineThickness);

      ImPlot::PlotLine(m_config.channelName.c_str(),
        xData.data(), yData.data(), (int)xData.size());

      ImPlot::PopStyleVar();
      ImPlot::PopStyleColor();
    }

    // Draw spec line if enabled
    if (m_config.enableSpec) {
      ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1.0f, 0.0f, 0.0f, 0.8f));
      ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.5f);

      double specX[] = { xMin, xMax };
      double specY[] = { m_config.specValue, m_config.specValue };
      ImPlot::PlotLine("Spec", specX, specY, 2);

      ImPlot::PopStyleVar();
      ImPlot::PopStyleColor();
    }

    ImPlot::EndPlot();
  }

  // Show current value overlay with clickable functionality
  if (m_config.showCurrentValue) {
    RenderCurrentValueDisplay();
  }

  // Render channel selector popup if needed
  if (m_showChannelSelector) {
    RenderChannelSelector();
  }
}

// Render current value display with clickable functionality
void LiveDataPlot::RenderCurrentValueDisplay() {
  ImGui::SameLine();
  ImGui::BeginGroup();

  // Channel name (clickable if selector is enabled)
  if (m_config.enableChannelSelector) {
    // Make channel name clickable
    std::string channelButtonId = "Channel: " + m_config.channelName + "##" + m_id;
    ImVec4 channelColor = IsChannelAvailable() ?
      ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, channelColor);
    if (ImGui::Selectable(channelButtonId.c_str(), false, ImGuiSelectableFlags_DontClosePopups)) {
      m_showChannelSelector = true;
      m_availableChannels = GetAvailableChannels(); // Refresh channels
      m_channelFilter[0] = '\0'; // Clear filter buffer - FIXED

      // Find current channel index
      m_selectedChannelIndex = -1;
      for (size_t i = 0; i < m_availableChannels.size(); ++i) {
        if (m_availableChannels[i] == m_config.channelName) {
          m_selectedChannelIndex = (int)i;
          break;
        }
      }
    }
    ImGui::PopStyleColor();

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Click to select different channel");
    }
  }
  else {
    // Non-clickable channel name
    ImGui::Text("Channel: %s", m_config.channelName.c_str());
  }

  // Current value (also clickable if selector is enabled)
  std::string currentValueText = "Current: " + FormatValue(m_currentValue);
  if (m_config.enableChannelSelector) {
    std::string valueButtonId = currentValueText + "##value_" + m_id;

    // Highlight if value is being clicked
    ImVec4 valueColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    if (ImGui::Selectable(valueButtonId.c_str(), false, ImGuiSelectableFlags_DontClosePopups)) {
      m_showChannelSelector = true;
      m_availableChannels = GetAvailableChannels();
      m_channelFilter[0] = '\0'; // Clear filter buffer

      // Find current channel index
      m_selectedChannelIndex = -1;
      for (size_t i = 0; i < m_availableChannels.size(); ++i) {
        if (m_availableChannels[i] == m_config.channelName) {
          m_selectedChannelIndex = (int)i;
          break;
        }
      }
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Click to select different channel");
    }
  }
  else {
    ImGui::Text("%s", currentValueText.c_str());
  }

  // Spec comparison if enabled
  if (m_config.enableSpec) {
    float diff = m_currentValue - m_config.specValue;
    float percentDiff = (m_config.specValue != 0) ?
      (diff / m_config.specValue * 100.0f) : 0.0f;

    ImGui::Text("Spec: %s", FormatValue(m_config.specValue).c_str());
    ImGui::Text("Diff: %s (%.1f%%)", FormatValue(diff).c_str(), percentDiff);

    // Color indicator
    ImVec4 color = (std::abs(diff) < std::abs(m_config.specValue * 0.1f)) ?
      ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    ImGui::TextColored(color, std::abs(diff) < std::abs(m_config.specValue * 0.1f) ?
      "PASS" : "FAIL");
  }

  ImGui::EndGroup();
}

// Render channel selector popup
void LiveDataPlot::RenderChannelSelector() {
  std::string popupId = "Channel Selector##" + m_id;

  if (m_showChannelSelector) {
    ImGui::OpenPopup(popupId.c_str());
  }

  // Set popup size and position
  ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);

  if (ImGui::BeginPopupModal(popupId.c_str(), &m_showChannelSelector,
    ImGuiWindowFlags_NoResize)) {

    ImGui::Text("Select Data Channel for %s", m_id.c_str());
    ImGui::Separator();

    // Search filter
    ImGui::Text("Filter:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    bool filterChanged = ImGui::InputText(("##filter_" + m_id).c_str(),
      m_channelFilter, sizeof(m_channelFilter));

    // Channel list
    ImGui::Text("Available Channels:");

    // Create filtered list
    std::vector<std::string> filteredChannels;
    std::string lowerFilter = m_channelFilter;
    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);

    for (const auto& channel : m_availableChannels) {
      if (lowerFilter.empty()) {
        filteredChannels.push_back(channel);
      }
      else {
        std::string lowerChannel = channel;
        std::transform(lowerChannel.begin(), lowerChannel.end(), lowerChannel.begin(), ::tolower);
        if (lowerChannel.find(lowerFilter) != std::string::npos) {
          filteredChannels.push_back(channel);
        }
      }
    }

    // Show filtered channels in a list box
    if (ImGui::BeginListBox(("##channels_" + m_id).c_str(), ImVec2(-1, 180))) {
      for (size_t i = 0; i < filteredChannels.size(); ++i) {
        const std::string& channel = filteredChannels[i];

        bool isSelected = (channel == m_config.channelName);

        // Color code based on availability
        ImVec4 textColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Default white

        ImGui::PushStyleColor(ImGuiCol_Text, textColor);

        if (ImGui::Selectable(channel.c_str(), isSelected)) {
          // Channel selected - update configuration
          SetChannel(channel);
          m_showChannelSelector = false;
        }

        ImGui::PopStyleColor();

        // Double-click to select and close
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
          SetChannel(channel);
          m_showChannelSelector = false;
        }
      }
      ImGui::EndListBox();
    }

    ImGui::Separator();

    // Status info
    ImGui::Text("Total channels: %zu", m_availableChannels.size());
    if (!lowerFilter.empty()) {
      ImGui::SameLine();
      ImGui::Text("| Filtered: %zu", filteredChannels.size());
    }

    // Buttons
    if (ImGui::Button("Refresh")) {
      m_availableChannels = GetAvailableChannels();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      m_showChannelSelector = false;
    }

    ImGui::EndPopup();
  }
}

// Get available channels from GlobalDataStore
std::vector<std::string> LiveDataPlot::GetAvailableChannels() const {
  GlobalDataStore* store = GlobalDataStore::GetInstance();
  if (!store) {
    return {};
  }

  return store->GetAvailableChannels();
}

// Update configuration
void LiveDataPlot::UpdateConfig(const Config& config) {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  m_config = config;
  m_detectedUnit = DetectUnit();
  m_needsAutoScale = true;
}

// Set channel
void LiveDataPlot::SetChannel(const std::string& channelName) {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  m_config.channelName = channelName;
  m_detectedUnit = DetectUnit();
  Clear();
}

// Set spec
void LiveDataPlot::SetSpec(float value, bool enabled) {
  m_config.specValue = value;
  m_config.enableSpec = enabled;
}

// Set channel selector enabled
void LiveDataPlot::SetChannelSelectorEnabled(bool enable) {
  m_config.enableChannelSelector = enable;
}

// Clear data
void LiveDataPlot::Clear() {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  m_dataHistory.clear();
  m_currentValue = 0.0f;
  m_needsAutoScale = true;
  m_startTime = std::chrono::steady_clock::now();
}

// Check if channel is available
bool LiveDataPlot::IsChannelAvailable() const {
  GlobalDataStore* store = GlobalDataStore::GetInstance();
  if (!store) return false;

  auto channels = store->GetAvailableChannels();
  return std::find(channels.begin(), channels.end(), m_config.channelName) != channels.end();
}

// Set Y range
void LiveDataPlot::SetYRange(float min, float max) {
  m_config.minValue = min;
  m_config.maxValue = max;
  m_config.autoScale = false;
}

// Set auto scale
void LiveDataPlot::SetAutoScale(bool enable) {
  m_config.autoScale = enable;
  if (enable) {
    m_needsAutoScale = true;
  }
}

// Update data from GlobalDataStore
void LiveDataPlot::UpdateData() {
  GlobalDataStore* store = GlobalDataStore::GetInstance();
  if (!store || m_config.channelName.empty()) return;

  // Check if enough time has passed for update (limit to 60 Hz)
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastUpdateTime);
  if (elapsed.count() < 16) return; // ~60 FPS

  m_lastUpdateTime = now;

  // Get current value
  float value = store->GetValue(m_config.channelName);

  // Calculate timestamp
  double timestamp = std::chrono::duration<double>(now - m_startTime).count();

  // Add to history
  {
    std::lock_guard<std::mutex> lock(m_dataMutex);

    m_currentValue = value;
    m_dataHistory.push_back(DataPoint(value, timestamp));

    // Limit history size
    while (m_dataHistory.size() > (size_t)m_config.historySize) {
      m_dataHistory.pop_front();
    }

    // Remove old data outside time window
    double cutoffTime = timestamp - m_config.timeWindow * 1.5; // Keep a bit extra
    while (!m_dataHistory.empty() && m_dataHistory.front().timestamp < cutoffTime) {
      m_dataHistory.pop_front();
    }
  }
}

// Format value with units
std::string LiveDataPlot::FormatValue(float value) const {
  std::stringstream ss;
  float absValue = std::abs(value);

  std::string unit = m_detectedUnit;

  // Format based on detected unit type
  if (unit.find("A") != std::string::npos) {
    // Current formatting
    if (absValue == 0.0f) {
      ss << "0.00 A";
    }
    else if (absValue < 1e-9f) {
      ss << std::fixed << std::setprecision(2) << (value * 1e12f) << " pA";
    }
    else if (absValue < 1e-6f) {
      ss << std::fixed << std::setprecision(2) << (value * 1e9f) << " nA";
    }
    else if (absValue < 1e-3f) {
      ss << std::fixed << std::setprecision(2) << (value * 1e6f) << " µA";
    }
    else if (absValue < 1.0f) {
      ss << std::fixed << std::setprecision(3) << (value * 1e3f) << " mA";
    }
    else {
      ss << std::fixed << std::setprecision(3) << value << " A";
    }
  }
  else if (unit.find("V") != std::string::npos) {
    // Voltage formatting
    if (absValue == 0.0f) {
      ss << "0.00 V";
    }
    else if (absValue < 1e-6f) {
      ss << std::fixed << std::setprecision(2) << (value * 1e6f) << " µV";
    }
    else if (absValue < 1e-3f) {
      ss << std::fixed << std::setprecision(2) << (value * 1e3f) << " mV";
    }
    else {
      ss << std::fixed << std::setprecision(3) << value << " V";
    }
  }
  else if (unit.find("°C") != std::string::npos) {
    // Temperature
    ss << std::fixed << std::setprecision(1) << value << " °C";
  }
  else {
    // Generic formatting
    if (absValue == 0.0f) {
      ss << "0.00";
    }
    else if (absValue < 1e-6f) {
      ss << std::scientific << std::setprecision(3) << value;
    }
    else if (absValue < 0.001f) {
      ss << std::fixed << std::setprecision(6) << value;
    }
    else {
      ss << std::fixed << std::setprecision(3) << value;
    }
    if (!unit.empty()) {
      ss << " " << unit;
    }
  }

  return ss.str();
}

// Auto-detect units from channel name
std::string LiveDataPlot::DetectUnit() const {
  std::string lowerChannel = m_config.channelName;
  std::transform(lowerChannel.begin(), lowerChannel.end(), lowerChannel.begin(), ::tolower);

  if (lowerChannel.find("current") != std::string::npos ||
    lowerChannel.find("amp") != std::string::npos ||
    lowerChannel.back() == 'a') {
    return "A";
  }
  else if (lowerChannel.find("voltage") != std::string::npos ||
    lowerChannel.find("volt") != std::string::npos ||
    lowerChannel.back() == 'v') {
    return "V";
  }
  else if (lowerChannel.find("temp") != std::string::npos) {
    return "°C";
  }
  else if (lowerChannel.find("pressure") != std::string::npos) {
    return "Pa";
  }
  else if (lowerChannel.find("power") != std::string::npos) {
    return "W";
  }
  else if (lowerChannel.find("resistance") != std::string::npos) {
    return "Ω";
  }

  return "";
}

// Calculate auto scale
void LiveDataPlot::CalculateAutoScale() {
  if (m_dataHistory.empty()) {
    m_autoMinY = -1.0f;
    m_autoMaxY = 1.0f;
    return;
  }

  std::lock_guard<std::mutex> lock(m_dataMutex);

  float minVal = m_dataHistory[0].value;
  float maxVal = m_dataHistory[0].value;

  for (const auto& point : m_dataHistory) {
    minVal = std::min(minVal, point.value);
    maxVal = std::max(maxVal, point.value);
  }

  // Add padding
  float range = maxVal - minVal;
  if (range < 1e-6f) {
    // Very small range, add fixed padding
    m_autoMinY = minVal - 0.1f;
    m_autoMaxY = maxVal + 0.1f;
  }
  else {
    // Add 20% padding
    float padding = range * 0.2f;
    m_autoMinY = minVal - padding;
    m_autoMaxY = maxVal + padding;
  }

  // Include spec value if enabled
  if (m_config.enableSpec) {
    m_autoMinY = std::min(m_autoMinY, m_config.specValue * 0.9f);
    m_autoMaxY = std::max(m_autoMaxY, m_config.specValue * 1.1f);
  }
}

// LiveDataPlotManager implementation
LiveDataPlotManager* LiveDataPlotManager::GetInstance() {
  static LiveDataPlotManager instance;
  return &instance;
}

LiveDataPlot* LiveDataPlotManager::GetPlot(const std::string& id) {
  std::lock_guard<std::mutex> lock(m_managerMutex);

  auto it = m_plots.find(id);
  if (it == m_plots.end()) {
    m_plots[id] = std::make_unique<LiveDataPlot>(id);
  }

  return m_plots[id].get();
}

void LiveDataPlotManager::RemovePlot(const std::string& id) {
  std::lock_guard<std::mutex> lock(m_managerMutex);
  m_plots.erase(id);
}

void LiveDataPlotManager::ClearAll() {
  std::lock_guard<std::mutex> lock(m_managerMutex);
  m_plots.clear();
}