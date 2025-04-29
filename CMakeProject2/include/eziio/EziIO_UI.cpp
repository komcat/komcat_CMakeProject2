#include "EziIO_UI.h"
#include <sstream>
#include <iomanip>

EziIO_UI::EziIO_UI(EziIOManager& manager)
  : m_ioManager(manager),
  m_showWindow(true),
  m_autoRefresh(true),
  m_refreshInterval(0.5f),  // Refresh every 500ms by default
  m_refreshTimer(0.0f),
  m_showDebugInfo(false)    // Debug info hidden by default
{
  // Initialize device states
  RefreshDeviceStates();

  // Log initialization
  std::cout << "[EziIO_UI] Initialized with " << m_deviceStates.size() << " devices" << std::endl;
}

EziIO_UI::~EziIO_UI()
{
}

void EziIO_UI::RenderUI()
{
  if (!m_showWindow) return;

  // Determine window size based on number of devices and debug state
  ImVec2 windowSize;
  if (m_showDebugInfo) {
    windowSize = ImVec2(800, 200 * m_deviceStates.size() + 400); // More space for debug info
  }
  else {
    windowSize = ImVec2(800, 200 * m_deviceStates.size() + 100);
  }

  // Create the main window
  ImGui::SetNextWindowSize(windowSize, ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("EziIO Status", &m_showWindow, ImGuiWindowFlags_NoCollapse))
  {
    ImGui::End();
    return;
  }

  // Auto-refresh controls
  ImGui::Checkbox("Auto Refresh", &m_autoRefresh);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(150);
  ImGui::SliderFloat("Refresh Interval (s)", &m_refreshInterval, 0.1f, 2.0f, "%.1f");
  ImGui::SameLine();

  if (ImGui::Button("Refresh Now"))
  {
    RefreshDeviceStates();
    std::cout << "[EziIO_UI] Manual refresh triggered" << std::endl;
  }

  // Debug mode toggle with a clearer label
  ImGui::SameLine();

  // Style the debug checkbox differently
  ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(1.0f, 0.6f, 0.0f, 1.0f)); // Orange checkmark
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));  // Dark background
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
  bool debugChanged = ImGui::Checkbox("Show Debug Info", &m_showDebugInfo);
  ImGui::PopStyleColor(3);

  // Update timer for auto-refresh
  if (m_autoRefresh)
  {
    m_refreshTimer += ImGui::GetIO().DeltaTime;
    if (m_refreshTimer >= m_refreshInterval)
    {
      RefreshDeviceStates();
      m_refreshTimer = 0.0f;
    }
  }

  // Display the refresh countdown
  if (m_autoRefresh)
  {
    float remainingTime = m_refreshInterval - m_refreshTimer;
    ImGui::SameLine();
    ImGui::Text("Next refresh in: %.1fs", remainingTime);
  }

  ImGui::Separator();

  // Render each device panel
  for (auto& device : m_deviceStates)
  {
    RenderDevicePanel(device);
  }

  ImGui::End();
}

void EziIO_UI::RefreshDeviceStates()
{
  // Clear previous states
  m_deviceStates.clear();

  // Get all devices from the manager
  for (const auto& devicePtr : m_ioManager.getDevices())
  {
    DeviceState state;
    state.name = devicePtr->getName();
    state.id = devicePtr->getDeviceId();
    state.inputCount = devicePtr->getInputCount();
    state.outputCount = devicePtr->getOutputCount();
    state.connected = devicePtr->isConnected();

    // Get cached input and output states
    uint32_t inputs = 0, latch = 0;
    uint32_t outputs = 0, outStatus = 0;

    bool inputSuccess = m_ioManager.getLastInputStatus(state.id, inputs, latch);
    bool outputSuccess = m_ioManager.getLastOutputStatus(state.id, outputs, outStatus);

    // Log the refresh operation if debug is enabled
    if (m_showDebugInfo) {
      std::cout << "[EziIO_UI] Refreshing device " << state.name << " (ID: " << state.id << ")" << std::endl;
      std::cout << "  Input status: " << (inputSuccess ? "Success" : "Failed")
        << " [0x" << std::hex << inputs << ", Latch: 0x" << latch << std::dec << "]" << std::endl;
      std::cout << "  Output status: " << (outputSuccess ? "Success" : "Failed")
        << " [0x" << std::hex << outputs << ", Status: 0x" << outStatus << std::dec << "]" << std::endl;
    }

    state.inputs = inputs;
    state.latch = latch;
    state.outputs = outputs;
    state.outputStatus = outStatus;

    // Add to our list
    m_deviceStates.push_back(state);
  }
}

void EziIO_UI::RenderDevicePanel(DeviceState& device)
{
  // Create a collapsing header for each device
  std::string headerName = device.name + " (ID: " + std::to_string(device.id) + ")";
  if (device.connected)
  {
    headerName += " - Connected";
  }
  else
  {
    headerName += " - Disconnected";
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
  }

  if (ImGui::CollapsingHeader(headerName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (!device.connected)
    {
      ImGui::PopStyleColor();
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Device is not connected!");

      if (ImGui::Button("Connect"))
      {
        if (m_ioManager.connectDevice(device.id))
        {
          device.connected = true;
          std::cout << "[EziIO_UI] Connected to device " << device.name << std::endl;
        }
      }
    }
    else
    {
      // Show raw debug values if debug mode is enabled
      if (m_showDebugInfo)
      {
        ImGui::Text("Debug Information:");
        ImGui::Text("Raw Inputs: 0x%08X  Latch: 0x%08X", device.inputs, device.latch);
        ImGui::Text("Raw Outputs: 0x%08X  Status: 0x%08X", device.outputs, device.outputStatus);

        // Add binary representation for more detailed debugging
        std::stringstream ssInputs, ssOutputs;
        ssInputs << "Inputs (Binary): ";
        ssOutputs << "Outputs (Binary): ";

        for (int bit = 31; bit >= 0; bit--) {
          ssInputs << ((device.inputs & (1U << bit)) ? "1" : "0");
          ssOutputs << ((device.outputs & (1U << bit)) ? "1" : "0");

          if (bit % 8 == 0) {
            ssInputs << " ";
            ssOutputs << " ";
          }
        }

        ImGui::Text("%s", ssInputs.str().c_str());
        ImGui::Text("%s", ssOutputs.str().c_str());

        // Add output pin mask debugging
        if (device.outputCount > 0) {
          ImGui::Text("Output Pin Masks (Expected bit patterns):");
          for (int i = 0; i < device.outputCount; i++) {
            uint32_t mask = GetOutputPinMask(device.name, i);
            ImGui::Text("Pin %d: 0x%08X", i, mask);
          }
        }

        ImGui::Separator();
      }

      // Create a two-column layout for inputs and outputs
      ImGui::Columns(2, "io_columns", true);

      // Render input pins if any
      if (device.inputCount > 0)
      {
        RenderInputPins(device);
      }
      else
      {
        ImGui::Text("No input pins available.");
      }

      ImGui::NextColumn();

      // Render output pins if any
      if (device.outputCount > 0)
      {
        RenderOutputPins(device);
      }
      else
      {
        ImGui::Text("No output pins available.");
      }

      ImGui::Columns(1);

      // Add a force refresh button for this device
      if (ImGui::Button(("Force Refresh Device##" + device.name).c_str()))
      {
        // Force a direct hardware read instead of using cached values
        EziIODevice* dev = m_ioManager.getDevice(device.id);
        if (dev) {
          uint32_t outputs = 0, status = 0;
          uint32_t inputs = 0, latch = 0;

          bool inputSuccess = dev->readInputs(inputs, latch);
          bool outputSuccess = dev->getOutputs(outputs, status);

          if (outputSuccess) {
            device.outputs = outputs;
            device.outputStatus = status;

            if (m_showDebugInfo) {
              std::cout << "[EziIO_UI] Forced refresh of " << device.name
                << " outputs: 0x" << std::hex << outputs
                << " status: 0x" << status << std::dec << std::endl;
            }
          }

          if (inputSuccess) {
            device.inputs = inputs;
            device.latch = latch;
          }
        }
      }
    }
  }
  else if (!device.connected)
  {
    ImGui::PopStyleColor();
  }

  ImGui::Separator();
}

void EziIO_UI::RenderInputPins(DeviceState& device)
{
  ImGui::Text("Input Pins:");

  // Create a table for input pins
  if (ImGui::BeginTable("Inputs", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
  {
    ImGui::TableSetupColumn("Pin", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableSetupColumn("Latch", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableSetupColumn("Visual", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Clear Latch", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableHeadersRow();

    // Render each input pin row
    for (int i = 0; i < device.inputCount; i++)
    {
      ImGui::TableNextRow();

      // Pin number
      ImGui::TableNextColumn();
      ImGui::Text("%d", i);

      // State (ON/OFF)
      ImGui::TableNextColumn();
      bool inputState = IsPinOn(device.inputs, i);
      ImGui::Text("%s", inputState ? "ON" : "OFF");

      // Latch status
      ImGui::TableNextColumn();
      bool latchState = IsPinOn(device.latch, i);
      ImGui::Text("%s", latchState ? "YES" : "NO");

      // Visual indicator
      ImGui::TableNextColumn();

      // Create a unique ID for the color button based on device ID and pin
      std::string colorButtonId = "##input_indicator_" + device.name + "_" + std::to_string(i);

      ImVec4 color = inputState ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
      ImGui::ColorButton(colorButtonId.c_str(), color, ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));

      // Clear latch button
      ImGui::TableNextColumn();
      std::string buttonId = "Clear##input_" + device.name + "_" + std::to_string(i);
      if (ImGui::Button(buttonId.c_str()))
      {
        uint32_t mask = 1 << i;
        EziIODevice* dev = m_ioManager.getDevice(device.id);
        if (dev) {
          bool success = dev->clearLatch(mask);
          if (m_showDebugInfo) {
            std::cout << "[EziIO_UI] Clear latch for " << device.name
              << " pin " << i << ": " << (success ? "Success" : "Failed")
              << " (mask: 0x" << std::hex << mask << std::dec << ")" << std::endl;
          }
        }
      }
    }

    ImGui::EndTable();
  }
}

void EziIO_UI::RenderOutputPins(DeviceState& device)
{
  ImGui::Text("Output Pins:");

  // Create a table for output pins
  if (ImGui::BeginTable("Outputs", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
  {
    ImGui::TableSetupColumn("Pin", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Visual", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableHeadersRow();

    // Render each output pin row
    for (int i = 0; i < device.outputCount; i++)
    {
      ImGui::TableNextRow();

      // Pin number
      ImGui::TableNextColumn();
      ImGui::Text("%d", i);

      // State (ON/OFF)
      ImGui::TableNextColumn();

      // Use the correct mask for the device type
      uint32_t mask = GetOutputPinMask(device.name, i);
      bool outputState = (device.outputs & mask) != 0;

      if (m_showDebugInfo) {
        // When debug mode is on, show the hex mask value
        std::string stateInfo = std::string(outputState ? "ON" : "OFF") +
          " [0x" +
          (std::stringstream() << std::hex << mask).str() +
          "]";
        ImGui::Text("%s", stateInfo.c_str());
      }
      else {
        // Simple ON/OFF display when debug is off
        ImGui::Text("%s", outputState ? "ON" : "OFF");
      }

      // Visual indicator
      ImGui::TableNextColumn();

      // Create a unique ID for the color button based on device ID and pin
      std::string colorButtonId = "##output_indicator_" + device.name + "_" + std::to_string(i);

      ImVec4 color = outputState ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
      ImGui::ColorButton(colorButtonId.c_str(), color, ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));

      // Control buttons
      ImGui::TableNextColumn();

      // Generate unique IDs for each button based on device name and pin
      std::string onButtonId = "ON##out_" + device.name + "_" + std::to_string(i);
      std::string offButtonId = "OFF##out_" + device.name + "_" + std::to_string(i);

      // ON button
      if (ImGui::Button(onButtonId.c_str(), ImVec2(50, 20)))
      {
        bool success = m_ioManager.setOutput(device.id, i, true);

        if (m_showDebugInfo) {
          std::cout << "[EziIO_UI] Set output " << device.name
            << " pin " << i << " to ON: " << (success ? "Success" : "Failed") << std::endl;
        }

        // Force an immediate update of the output status
        EziIODevice* dev = m_ioManager.getDevice(device.id);
        if (dev) {
          // Read the current outputs directly from the device
          uint32_t outputs = 0, status = 0;
          if (dev->getOutputs(outputs, status)) {
            // Update our cached state for the UI
            device.outputs = outputs;
            device.outputStatus = status;

            if (m_showDebugInfo) {
              std::cout << "  Updated outputs: 0x" << std::hex << outputs
                << " status: 0x" << status << std::dec << std::endl;

              // Check if the expected bit is now set
              bool newState = (outputs & mask) != 0;
              std::cout << "  New pin state: " << (newState ? "ON" : "OFF")
                << " (expected ON)" << std::endl;
            }
          }
        }

        // Call output change callback if set
        if (m_outputChangeCallback)
        {
          m_outputChangeCallback(device.name, i, true);
        }
      }

      ImGui::SameLine();

      // OFF button
      if (ImGui::Button(offButtonId.c_str(), ImVec2(50, 20)))
      {
        bool success = m_ioManager.setOutput(device.id, i, false);

        if (m_showDebugInfo) {
          std::cout << "[EziIO_UI] Set output " << device.name
            << " pin " << i << " to OFF: " << (success ? "Success" : "Failed") << std::endl;
        }

        // Force an immediate update of the output status
        EziIODevice* dev = m_ioManager.getDevice(device.id);
        if (dev) {
          // Read the current outputs directly from the device
          uint32_t outputs = 0, status = 0;
          if (dev->getOutputs(outputs, status)) {
            // Update our cached state for the UI
            device.outputs = outputs;
            device.outputStatus = status;

            if (m_showDebugInfo) {
              std::cout << "  Updated outputs: 0x" << std::hex << outputs
                << " status: 0x" << status << std::dec << std::endl;

              // Check if the expected bit is now clear
              bool newState = (outputs & mask) != 0;
              std::cout << "  New pin state: " << (newState ? "ON" : "OFF")
                << " (expected OFF)" << std::endl;
            }
          }
        }

        // Call output change callback if set
        if (m_outputChangeCallback)
        {
          m_outputChangeCallback(device.name, i, false);
        }
      }
    }

    ImGui::EndTable();
  }
}

uint32_t EziIO_UI::GetOutputPinMask(const std::string& deviceName, int pin) const
{
  if (deviceName == "IOBottom" && pin < 16) {
    // 16-output module uses specific pin masks (starting from bit 16)
    return 0x10000 << pin;
  }
  else if (deviceName == "IOTop" && pin < 8) {
    // 8-output module uses different masks (starting from bit 8)
    return 0x100 << pin;
  }
  else {
    // Fallback to standard bit pattern
    return 1U << pin;
  }
}

bool EziIO_UI::IsPinOn(uint32_t value, int pin) const
{
  // For inputs, each bit represents a pin state
  if (pin < 32)
  {
    return (value & (1U << pin)) != 0;
  }
  return false;
}

void EziIO_UI::SetInputChangeCallback(std::function<void(const std::string&, int, bool)> callback)
{
  m_inputChangeCallback = callback;
}

void EziIO_UI::SetOutputChangeCallback(std::function<void(const std::string&, int, bool)> callback)
{
  m_outputChangeCallback = callback;
}