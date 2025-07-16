// UISMUPanel.h - UI panel for managing Keithley 2400 SMU devices
#pragma once

#include <memory>
#include <string>

// Forward declarations
class Keithley2400Manager;
class Keithley2400Client;

class UISMUPanel {
public:
  UISMUPanel(Keithley2400Manager& smuManager);
  ~UISMUPanel();

  // Disable copy/move to avoid issues with references
  UISMUPanel(const UISMUPanel&) = delete;
  UISMUPanel& operator=(const UISMUPanel&) = delete;
  UISMUPanel(UISMUPanel&&) = delete;
  UISMUPanel& operator=(UISMUPanel&&) = delete;

  // UI rendering
  void RenderUI();
  void ToggleWindow();
  bool IsVisible() const { return m_showWindow; }
  void SetVisible(bool visible) { m_showWindow = visible; }

private:
  // Reference to SMU manager
  Keithley2400Manager& m_smuManager;

  // UI state
  bool m_showWindow = true;
  std::string m_selectedDeviceName;

  // Panel rendering methods
  void RenderLeftPanel();    // Device list and global controls
  void RenderMiddlePanel();  // Main device interface
  void RenderRightPanel();   // Advanced controls

  // Helper methods
  void RenderDeviceList();
  void RenderSelectedDeviceUI();
  void RenderNoSelectionMessage();
  void RenderGlobalControls();

  // Embedded UI rendering methods
  void RenderDeviceHeader(Keithley2400Client* device);
  void RenderConnectionControls(Keithley2400Client* device);
  void RenderDeviceStatus(Keithley2400Client* device);
  void RenderCurrentReadings(Keithley2400Client* device);
  void RenderOutputControls(Keithley2400Client* device);
  void RenderEnhancedSourceControls(Keithley2400Client* device);
  void RenderVoltageSweepControls(Keithley2400Client* device);
  void RenderMeasurementPlots(Keithley2400Client* device);
  void RenderSCPIInterface(Keithley2400Client* device);
  void RenderUtilityControls(Keithley2400Client* device);

  // Legacy method for basic source controls
  void RenderSourceControls(Keithley2400Client* device);
  void RenderMeasurementDisplay(Keithley2400Client* device);

  // UI state for source controls
  struct SourceSettings {
    float voltageSetpoint = 0.0f;
    float currentSetpoint = 0.001f;
    float compliance = 0.1f;
    int sourceMode = 0; // 0 = voltage source, 1 = current source

    // Voltage sweep parameters
    float sweepStart = 0.0f;
    float sweepStop = 5.0f;
    int sweepSteps = 11;
    float sweepCompliance = 0.01f;
    float sweepDelay = 0.1f;

    // Polling controls
    int pollingInterval = 100;  // Changed to 100ms for faster updates
  } m_sourceSettings;

  // UI state for SCPI commands
  struct SCPIState {
    char command[256] = "";
    std::string lastResponse = "";
  } m_scpiState;

  // UI state for global operations
  struct GlobalSettings {
    float globalVoltage = 0.0f;
    float globalCurrent = 0.001f;
    float globalCompliance = 0.1f;
    int globalSourceMode = 0;
  } m_globalSettings;
};