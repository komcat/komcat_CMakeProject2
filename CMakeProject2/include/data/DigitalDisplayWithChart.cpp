// src/ui/DigitalDisplayWithChart.cpp
#include "include/data/DigitalDisplayWithChart.h"
#include <iostream>
#include <cmath>


DigitalDisplayWithChart::DigitalDisplayWithChart(const std::string& initialDataName)
  : m_selectedDataName(initialDataName), m_showDebug(false) { // Add debug flag here

  // Generate unique window ID with static counter instead of memory address
  static int instanceCounter = 0;
  instanceCounter++;
  m_windowId = "Display_" + initialDataName + "_" + std::to_string(instanceCounter);

  initializeUnitsMap();
  initializeDisplayNameMap();

  // DEBUG: Add logging to see what's happening during initialization
  if (m_showDebug) {
    std::cout << "[DEBUG] DigitalDisplayWithChart initialized with channel: " << initialDataName << std::endl;
  }

  loadChannelsFromConfig();
}

void DigitalDisplayWithChart::Render() {
  // Set window appearance
  ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(m_windowSize, ImGuiCond_FirstUseEver);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.15f, 0.2f, 0.95f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

  // Create resizable window with clean display name
  std::string displayWindowName = getDisplayName(m_selectedDataName) + " Display";
  ImGui::Begin(displayWindowName.c_str(), nullptr,
    ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoCollapse);

  // Update data
  updateData();

  // Handle right-click menu
  handleRightClickMenu();

  // Render digital display section
  if (m_digitalDisplayEnabled) {
    renderDigitalDisplay();
  }

  // Add controls
  renderControls();

  // Render chart section if enabled
  if (m_showChart) {
    ImGui::Separator();
    renderChart();
  }

  ImGui::End();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor();
}

void DigitalDisplayWithChart::initializeUnitsMap() {
  m_unitsMap = {
      {"GPIB-Current", {"A", {
          {1e-12, "pA"}, {1e-9, "nA"}, {1e-6, "uA"}, {1e-3, "mA"}, {1, "A"}
      }}},
      {"SMU1-Current", {"A", {
          {1e-12, "pA"}, {1e-9, "nA"}, {1e-6, "uA"}, {1e-3, "mA"}, {1, "A"}
      }}},
      {"SMU1-Voltage", {"V", {
          {1e-12, "pV"}, {1e-9, "nV"}, {1e-6, "uV"}, {1e-3, "mV"}, {1, "V"}
      }}},
      {"SMU1-Resistance", {"Ω", {
          {1e-3, "mΩ"}, {1, "Ω"}, {1e3, "kΩ"}, {1e6, "MΩ"}
      }}},
      {"SMU1-Power", {"W", {
          {1e-12, "pW"}, {1e-9, "nW"}, {1e-6, "uW"}, {1e-3, "mW"}, {1, "W"}
      }}},
      {"hex-right-A-5", {"V", {
          {1e-12, "pV"}, {1e-9, "nV"}, {1e-6, "uV"}, {1e-3, "mV"}, {1, "V"}
      }}},
      {"hex-left-A-5", {"V", {
          {1e-12, "pV"}, {1e-9, "nV"}, {1e-6, "uV"}, {1e-3, "mV"}, {1, "V"}
      }}},
      {"hex-right-A-6", {"V", {
          {1e-12, "pV"}, {1e-9, "nV"}, {1e-6, "uV"}, {1e-3, "mV"}, {1, "V"}
      }}},
      {"hex-left-A-6", {"V", {
          {1e-12, "pV"}, {1e-9, "nV"}, {1e-6, "uV"}, {1e-3, "mV"}, {1, "V"}
      }}},
      {"SagnacV", {"V", {
          {1e-12, "pV"}, {1e-9, "nV"}, {1e-6, "uV"}, {1e-3, "mV"}, {1, "V"}
      }}},
      {"gantry", {"", {
          {1, ""}  // No unit for gantry
      }}},
  };
}

void DigitalDisplayWithChart::initializeDisplayNameMap() {
  m_displayNameMap = {
      {"GPIB-Current", "Current"},
      {"SMU1-Current", "SMU1 Current"},
      {"SMU1-Voltage", "SMU1 Voltage"},
      {"SMU1-Resistance", "SMU1 Resistance"},
      {"SMU1-Power", "SMU1 Power"},
      {"hex-right-A-5", "Voltage R5"},
      {"hex-left-A-5", "Voltage L5"},
      {"hex-right-A-6", "Voltage R6"},
      {"hex-left-A-6", "Voltage L6"},
      {"SagnacV", "Sagnac V"},
      {"gantry", "Gantry"}
  };
}


void DigitalDisplayWithChart::loadChannelsFromConfig() {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  m_availableChannels.clear();
  std::set<std::string> configChannelIds; // Only track config channels, not all channels

  if (m_showDebug) {
    std::cout << "[DEBUG] Loading channels from config..." << std::endl;
  }

  // First, load from config file
  try {
    std::ifstream configFile("data_display_config.json");
    if (configFile.is_open()) {
      nlohmann::json config;
      configFile >> config;

      if (config.contains("channels") && config["channels"].is_array()) {
        if (m_showDebug) {
          std::cout << "[DEBUG] Found " << config["channels"].size() << " channels in config file" << std::endl;
        }

        for (const auto& channel : config["channels"]) {
          std::string id = channel.value("id", "");
          std::string displayName = channel.value("displayName", id);
          bool enabled = channel.value("enable", true);

          if (m_showDebug) {
            std::cout << "[DEBUG] Config channel: " << id << " (enabled: " << enabled << ")" << std::endl;
          }

          if (!id.empty() && enabled) {
            m_availableChannels.emplace_back(id, displayName);
            configChannelIds.insert(id); // Track this as a config channel
            if (m_showDebug) {
              std::cout << "[DEBUG] Added config channel: " << id << " -> " << displayName << std::endl;
            }
          }
        }
      }
      configFile.close();
    }
    else {
      if (m_showDebug) {
        std::cout << "[DEBUG] Could not open data_display_config.json" << std::endl;
      }
    }
  }
  catch (const std::exception& e) {
    if (m_showDebug) {
      std::cout << "[DEBUG] Exception loading config: " << e.what() << std::endl;
    }
  }

  // Second, add channels from global data store
  // CHANGED: Now we add ALL channels from GlobalDataStore, even if they're in config
  if (m_showDebug) {
    std::cout << "[DEBUG] Checking GlobalDataStore..." << std::endl;
  }

  GlobalDataStore* globalStore = GlobalDataStore::GetInstance();
  if (globalStore) {
    if (m_showDebug) {
      std::cout << "[DEBUG] GlobalDataStore instance found" << std::endl;
    }

    std::vector<std::string> globalChannels = globalStore->GetAvailableChannels();
    if (m_showDebug) {
      std::cout << "[DEBUG] GlobalDataStore has " << globalChannels.size() << " channels:" << std::endl;
    }

    for (const std::string& channelId : globalChannels) {
      if (m_showDebug) {
        std::cout << "[DEBUG] GlobalStore channel: " << channelId << std::endl;
      }

      // Check if this channel has a value
      float value = globalStore->GetValue(channelId, -999.0f);
      if (m_showDebug) {
        std::cout << "[DEBUG]   Current value: " << value << std::endl;
      }

      // CHANGED: Always add auto-detected channels, regardless of config
      std::string displayName = channelId;

      // Make display names more readable
      std::replace(displayName.begin(), displayName.end(), '_', ' ');
      std::replace(displayName.begin(), displayName.end(), '-', ' ');

      // Capitalize first letter of each word
      bool capitalizeNext = true;
      for (char& c : displayName) {
        if (capitalizeNext && std::isalpha(c)) {
          c = std::toupper(c);
          capitalizeNext = false;
        }
        else if (c == ' ') {
          capitalizeNext = true;
        }
      }

      // Add suffix to indicate if it's also in config
      if (configChannelIds.find(channelId) != configChannelIds.end()) {
        displayName += " (Auto + Config)";
      }
      else {
        displayName += " (Auto)";
      }

      m_availableChannels.emplace_back(channelId, displayName);
      if (m_showDebug) {
        std::cout << "[DEBUG] Added auto-detected channel: " << channelId << " -> " << displayName << std::endl;
      }
    }
  }
  else {
    if (m_showDebug) {
      std::cout << "[DEBUG] GlobalDataStore instance is NULL!" << std::endl;
    }
  }

  // Fallback channels if empty
  if (m_availableChannels.empty()) {
    if (m_showDebug) {
      std::cout << "[DEBUG] No channels found, using fallback channels" << std::endl;
    }
    m_availableChannels = {
        {"GPIB-Current", "Current Reading"},
        {"SMU1-Current", "SMU1 Current"},
        {"SMU1-Voltage", "SMU1 Voltage"}
    };
  }

  if (m_showDebug) {
    std::cout << "[DEBUG] Final channel list (" << m_availableChannels.size() << " channels):" << std::endl;
    for (const auto& [id, displayName] : m_availableChannels) {
      std::cout << "[DEBUG]   " << id << " -> " << displayName << std::endl;
    }
  }

  m_channelsLoaded = true;
}



void DigitalDisplayWithChart::updateData() {
  std::lock_guard<std::mutex> lock(m_dataMutex);

  // DEBUG: Check if GlobalDataStore is accessible
  GlobalDataStore* globalStore = GlobalDataStore::GetInstance();
  if (!globalStore) {
    if (m_showDebug) {
      std::cout << "[DEBUG] GlobalDataStore is NULL in updateData!" << std::endl;
    }
    return;
  }

  // Get current value from global data store
  float currentValue = globalStore->GetValue(m_selectedDataName);

  // DEBUG: Log the value retrieval (only occasionally to avoid spam)
  if (m_showDebug) {
    static int debugCounter = 0;
    debugCounter++;
    if (debugCounter % 60 == 0) { // Log every 60 frames (about once per second at 60fps)
      std::cout << "[DEBUG] Channel '" << m_selectedDataName << "' value: " << currentValue << std::endl;

      // Also check if the channel exists in the global store
      bool hasValue = globalStore->HasValue(m_selectedDataName);
      std::cout << "[DEBUG] Channel '" << m_selectedDataName << "' exists in store: " << hasValue << std::endl;

      // List all available channels
      auto allChannels = globalStore->GetAvailableChannels();
      std::cout << "[DEBUG] All channels in GlobalStore: ";
      for (const auto& ch : allChannels) {
        std::cout << ch << " ";
      }
      std::cout << std::endl;
    }
  }

  // Use high-resolution timestamp for better timing accuracy
  auto now = std::chrono::high_resolution_clock::now();
  auto duration = now.time_since_epoch();
  double currentTime = std::chrono::duration<double>(duration).count();

  // Add to buffer
  if (m_dataBuffers.find(m_selectedDataName) == m_dataBuffers.end()) {
    // Initialize new buffer
    DataBuffer newBuffer;
    newBuffer.displayName = getDisplayName(m_selectedDataName);
    newBuffer.unit = getBaseUnit(m_selectedDataName);
    newBuffer.color = generateColor(m_selectedDataName);
    m_dataBuffers[m_selectedDataName] = newBuffer;

    if (m_showDebug) {
      std::cout << "[DEBUG] Created new data buffer for: " << m_selectedDataName << std::endl;
    }
  }

  auto& buffer = m_dataBuffers[m_selectedDataName];

  // Only add new data if enough time has passed or if this is the first point
  static double lastUpdateTime = 0.0;
  const double minUpdateInterval = 0.01; // 100Hz max update rate to avoid oversampling

  if (buffer.timestamps.empty() || (currentTime - lastUpdateTime) >= minUpdateInterval) {
    buffer.addValue(currentValue, currentTime);
    buffer.clearOldData(currentTime, m_timeWindow * 1.2f); // Keep slightly more data than window
    lastUpdateTime = currentTime;
  }
}


void DigitalDisplayWithChart::handleRightClickMenu() {
  if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    m_showChannelPopup = true;
    if (m_showDebug) {
      std::cout << "[DEBUG] Right-click detected, reloading channels..." << std::endl;
    }
    loadChannelsFromConfig(); // Refresh channels
    ImGui::OpenPopup("SelectDataSource");
  }

  if (ImGui::BeginPopup("SelectDataSource")) {
    ImGui::Text("Select Data Source:");
    ImGui::Separator();

    // DEBUG: Show debug info in the popup - but only if debug is enabled
    if (m_showDebug && ImGui::TreeNode("Debug Info")) {
      ImGui::Text("Currently selected: %s", m_selectedDataName.c_str());
      ImGui::Text("Available channels: %zu", m_availableChannels.size());

      // Show GlobalDataStore status
      GlobalDataStore* globalStore = GlobalDataStore::GetInstance();
      if (globalStore) {
        auto allChannels = globalStore->GetAvailableChannels();
        ImGui::Text("GlobalStore channels: %zu", allChannels.size());

        if (ImGui::TreeNode("All GlobalStore Channels")) {
          for (const auto& ch : allChannels) {
            float val = globalStore->GetValue(ch);
            ImGui::Text("%s: %.6f", ch.c_str(), val);
          }
          ImGui::TreePop();
        }
      }
      else {
        ImGui::Text("GlobalStore: NULL");
      }
      ImGui::TreePop();
    }

    ImGui::Separator();

    // Group channels by type
    bool hasConfigChannels = false;
    bool hasAutoOnlyChannels = false;
    bool hasAutoConfigChannels = false;

    for (const auto& [id, displayName] : m_availableChannels) {
      if (displayName.find("(Auto + Config)") != std::string::npos) {
        hasAutoConfigChannels = true;
      }
      else if (displayName.find("(Auto)") != std::string::npos) {
        hasAutoOnlyChannels = true;
      }
      else {
        hasConfigChannels = true;
      }
    }

    // Show config-only channels first
    if (hasConfigChannels) {
      ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "Configured Channels:");
      for (const auto& [id, displayName] : m_availableChannels) {
        if (displayName.find("(Auto") == std::string::npos) {
          bool isSelected = (id == m_selectedDataName);
          if (ImGui::Selectable((displayName + "##config_" + id).c_str(), isSelected)) {
            SetSelectedChannel(id);
            m_showChannelPopup = false;
            if (m_showDebug) {
              std::cout << "[DEBUG] Selected config channel: " << id << std::endl;
            }
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("ID: %s", id.c_str());
          }
        }
      }
    }

    // Show channels that are both auto-detected AND in config
    if (hasAutoConfigChannels) {
      if (hasConfigChannels) ImGui::Separator();
      ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.8f, 1.0f), "Live Channels (Config + Auto):");
      for (const auto& [id, displayName] : m_availableChannels) {
        if (displayName.find("(Auto + Config)") != std::string::npos) {
          bool isSelected = (id == m_selectedDataName);
          if (ImGui::Selectable((displayName + "##live_" + id).c_str(), isSelected)) {
            SetSelectedChannel(id);
            m_showChannelPopup = false;
            if (m_showDebug) {
              std::cout << "[DEBUG] Selected live channel: " << id << std::endl;
            }
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("ID: %s\nThis channel is both configured and has live data", id.c_str());
          }
        }
      }
    }

    // Show auto-detected only channels
    if (hasAutoOnlyChannels) {
      if (hasConfigChannels || hasAutoConfigChannels) ImGui::Separator();
      ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.0f, 1.0f), "Auto-detected Channels:");
      for (const auto& [id, displayName] : m_availableChannels) {
        if (displayName.find("(Auto)") != std::string::npos) {
          bool isSelected = (id == m_selectedDataName);
          if (ImGui::Selectable((displayName + "##auto_" + id).c_str(), isSelected)) {
            SetSelectedChannel(id);
            m_showChannelPopup = false;
            if (m_showDebug) {
              std::cout << "[DEBUG] Selected auto channel: " << id << std::endl;
            }
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("ID: %s\nAuto-detected from live data", id.c_str());
          }
        }
      }
    }

    ImGui::Separator();
    if (ImGui::Selectable("Refresh All Channels")) {
      m_channelsLoaded = false;
      if (m_showDebug) {
        std::cout << "[DEBUG] Manual refresh requested" << std::endl;
      }
      loadChannelsFromConfig();
    }

    // Add debug button - but only show if debug is enabled
    if (m_showDebug && ImGui::Selectable("Force Debug Log")) {
      std::cout << "[DEBUG] === FORCE DEBUG LOG ===" << std::endl;

      // Check DataClientManager state if possible
      GlobalDataStore* store = GlobalDataStore::GetInstance();
      if (store) {
        auto channels = store->GetAvailableChannels();
        std::cout << "[DEBUG] Current GlobalStore channels (" << channels.size() << "):" << std::endl;
        for (const auto& ch : channels) {
          float val = store->GetValue(ch);
          bool hasVal = store->HasValue(ch);
          std::cout << "[DEBUG]   " << ch << ": value=" << val << ", exists=" << hasVal << std::endl;
        }
      }
    }

    ImGui::EndPopup();
  }
}

void DigitalDisplayWithChart::SetSelectedChannel(const std::string& channelName) {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  if (m_showDebug) {
    std::cout << "[DEBUG] Changing channel from '" << m_selectedDataName << "' to '" << channelName << "'" << std::endl;
  }
  m_selectedDataName = channelName;
  // Update window ID to reflect new channel
  m_windowId = "Display_" + channelName + "_" + std::to_string(reinterpret_cast<uintptr_t>(this));
}

void DigitalDisplayWithChart::renderDigitalDisplay() {
  float currentValue = GlobalDataStore::GetInstance()->GetValue(m_selectedDataName);
  bool isNegative = (currentValue < 0);
  float absValue = std::abs(currentValue);

  // Get scaled value and unit
  auto [scaledValue, unitDisplay] = getScaledValueAndUnit(absValue);
  std::string displayName = getDisplayName(m_selectedDataName);

  // Display name on left
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.7f, 1.0f, 1.0f));
  ImGui::SetWindowFontScale(2.0f);
  ImGui::Text("%s", displayName.c_str());
  ImGui::PopStyleColor();

  // Display unit on right
  if (!unitDisplay.empty()) {
    float windowWidth = ImGui::GetWindowSize().x;
    ImGui::SameLine(windowWidth - ImGui::CalcTextSize(unitDisplay.c_str()).x - 20);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.7f, 1.0f, 1.0f));
    ImGui::SetWindowFontScale(2.0f);
    ImGui::Text("%s", unitDisplay.c_str());
    ImGui::PopStyleColor();
  }

  ImGui::Separator();

  // Format value
  char valueStr[32];
  snprintf(valueStr, sizeof(valueStr), "%.2f", scaledValue);

  // Large digital display
  ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
  ImGui::SetWindowFontScale(7.0f);

  float windowWidth = ImGui::GetWindowSize().x;
  float valueWidth = ImGui::CalcTextSize(valueStr).x;
  float signWidth = ImGui::CalcTextSize("-").x;

  if (isNegative) {
    ImGui::SetCursorPosX((windowWidth - valueWidth - signWidth) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
    ImGui::Text("-");
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 0);
  }
  else {
    ImGui::SetCursorPosX((windowWidth - valueWidth) * 0.5f + (signWidth * 0.5f));
  }

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
  ImGui::Text("%s", valueStr);
  ImGui::PopStyleColor();

  ImGui::SetWindowFontScale(1.0f);
  ImGui::PopFont();
}

void DigitalDisplayWithChart::renderControls() {
  ImGui::Separator();

  // Toggle buttons
  if (ImGui::Button(m_showChart ? "Hide Chart" : "Show Chart")) {
    m_showChart = !m_showChart;
    // Adjust window size based on chart visibility
    if (m_showChart) {
      m_windowSize = ImVec2(500, 400);
    }
    else {
      m_windowSize = ImVec2(280, 120);
    }
    ImGui::SetWindowSize(m_windowSize);
  }

  if (m_showChart) {
    ImGui::SameLine();
    if (ImGui::Button(m_digitalDisplayEnabled ? "Hide Digital" : "Show Digital")) {
      m_digitalDisplayEnabled = !m_digitalDisplayEnabled;
    }

    // Chart controls
    ImGui::PushItemWidth(100);
    ImGui::SliderFloat("Time Window", &m_timeWindow, 5.0f, 120.0f, "%.1fs");
    ImGui::PopItemWidth();

    ImGui::SameLine();
    ImGui::Checkbox("Auto Y Scale", &m_autoScaleY);

    if (!m_autoScaleY) {
      ImGui::PushItemWidth(80);
      ImGui::DragFloat("Y Min", &m_yMin, 0.01f);
      ImGui::SameLine();
      ImGui::DragFloat("Y Max", &m_yMax, 0.01f);
      ImGui::PopItemWidth();
    }
  }
}

void DigitalDisplayWithChart::renderChart() {
  if (m_dataBuffers.find(m_selectedDataName) == m_dataBuffers.end() ||
    m_dataBuffers[m_selectedDataName].values.empty()) {
    ImGui::Text("No data available for plotting");
    return;
  }

  auto& buffer = m_dataBuffers[m_selectedDataName];

  // Get available space for chart
  ImVec2 contentSize = ImGui::GetContentRegionAvail();
  float chartHeight = std::max(contentSize.y - 20.0f, 150.0f);

  if (ImPlot::BeginPlot("##DataChart", ImVec2(contentSize.x, chartHeight))) {
    // Setup axes - CRITICAL: Use proper timing like DataChartManager
    ImPlot::SetupAxes("Time (s)", "Value",
      ImPlotAxisFlags_AutoFit,
      ImPlotAxisFlags_AutoFit);

    // FIXED: Calculate time window based on oldest available data
    double latestTime = 0.0;
    double earliestTime = 0.0;

    if (!buffer.timestamps.empty()) {
      latestTime = buffer.timestamps.back();

      // Use either the time window OR the span of available data, whichever is smaller
      double dataSpan = latestTime - buffer.timestamps.front();
      double timeWindow = std::min((double)m_timeWindow, dataSpan);

      earliestTime = latestTime - timeWindow;
    }
    else {
      // Fallback if no data
      latestTime = ImGui::GetTime();
      earliestTime = latestTime - m_timeWindow;
    }

    // Set x-axis to scroll with newest data - ALWAYS update
    ImPlot::SetupAxisLimits(ImAxis_X1,
      earliestTime,
      latestTime,
      ImGuiCond_Always);

    if (!m_autoScaleY) {
      ImPlot::SetupAxisLimits(ImAxis_Y1, m_yMin, m_yMax, ImGuiCond_Always);
    }

    // Plot ALL available data (don't filter by time window in plotting)
    if (buffer.values.size() > 1) {
      std::vector<float> xValues, yValues;

      // Plot all data points - let ImPlot handle the clipping
      for (size_t i = 0; i < buffer.timestamps.size(); i++) {
        xValues.push_back(static_cast<float>(buffer.timestamps[i]));
        yValues.push_back(buffer.values[i]);
      }

      // Plot all data
      if (!xValues.empty()) {
        ImPlot::PushStyleColor(ImPlotCol_Line, buffer.color);
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.0f);

        std::string label = buffer.displayName;
        if (!buffer.unit.empty()) {
          label += " (" + buffer.unit + ")";
        }

        ImPlot::PlotLine(label.c_str(), xValues.data(), yValues.data(), (int)xValues.size());

        ImPlot::PopStyleVar();
        ImPlot::PopStyleColor();
      }
    }

    ImPlot::EndPlot();
  }
}



std::string DigitalDisplayWithChart::getDisplayName(const std::string& dataName) {
  auto it = m_displayNameMap.find(dataName);
  if (it != m_displayNameMap.end()) {
    return it->second;
  }
  return dataName;
}

std::string DigitalDisplayWithChart::getBaseUnit(const std::string& dataName) {
  auto it = m_unitsMap.find(dataName);
  if (it != m_unitsMap.end()) {
    return it->second.baseUnit;
  }

  // Try to guess unit from name
  std::string lowerName = dataName;
  std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

  if (lowerName.find("current") != std::string::npos) return "A";
  if (lowerName.find("voltage") != std::string::npos) return "V";
  if (lowerName.find("resistance") != std::string::npos) return "Ω";
  if (lowerName.find("power") != std::string::npos) return "W";

  return "";
}

std::pair<float, std::string> DigitalDisplayWithChart::getScaledValueAndUnit(float absValue) {
  auto unitIt = m_unitsMap.find(m_selectedDataName);
  if (unitIt != m_unitsMap.end()) {
    auto& prefixes = unitIt->second.prefixes;

    for (size_t i = 0; i < prefixes.size(); i++) {
      const auto& prefix = prefixes[i];
      float threshold = 2000.0f;

      if (i < prefixes.size() - 1) {
        if (absValue < prefix.first * threshold) {
          return { absValue / prefix.first, prefix.second };
        }
      }
      else {
        return { absValue / prefix.first, prefix.second };
      }
    }
  }

  // Fallback: guess unit and scaling
  std::string baseUnit = getBaseUnit(m_selectedDataName);
  if (baseUnit == "A") {
    if (absValue < 1e-9) return { absValue * 1e12, "pA" };
    if (absValue < 1e-6) return { absValue * 1e9, "nA" };
    if (absValue < 1e-3) return { absValue * 1e6, "uA" };
    if (absValue < 1) return { absValue * 1e3, "mA" };
    return { absValue, "A" };
  }
  else if (baseUnit == "V") {
    if (absValue < 1e-3) return { absValue * 1e6, "uV" };
    if (absValue < 1) return { absValue * 1e3, "mV" };
    return { absValue, "V" };
  }

  return { absValue, baseUnit };
}

ImVec4 DigitalDisplayWithChart::generateColor(const std::string& channelName) {
  // Generate a consistent color based on channel name hash
  std::hash<std::string> hasher;
  size_t hash = hasher(channelName);

  float hue = (hash % 360) / 360.0f;
  float saturation = 0.7f;
  float value = 0.9f;

  // Convert HSV to RGB
  float c = value * saturation;
  float x = c * (1 - std::abs(std::fmod(hue * 6, 2) - 1));
  float m = value - c;

  float r, g, b;
  if (hue < 1.0f / 6) { r = c; g = x; b = 0; }
  else if (hue < 2.0f / 6) { r = x; g = c; b = 0; }
  else if (hue < 3.0f / 6) { r = 0; g = c; b = x; }
  else if (hue < 4.0f / 6) { r = 0; g = x; b = c; }
  else if (hue < 5.0f / 6) { r = x; g = 0; b = c; }
  else { r = c; g = 0; b = x; }

  return ImVec4(r + m, g + m, b + m, 1.0f);
}