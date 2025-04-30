#pragma once

#include <string>
#include <chrono>
#include <functional>

// Possible states of a pneumatic slide
enum class SlideState {
  UNKNOWN,    // Initial state or when sensors give contradictory readings
  RETRACTED,  // Slide is fully retracted (up position)
  EXTENDED,   // Slide is fully extended (down position)
  MOVING,     // Slide is in the process of moving
  P_ERROR       // Error state (e.g., timeout or conflicting sensors)
};

// Structure to hold IO pin configuration
struct IOPinConfig {
  std::string deviceName;
  std::string pinName;
  int deviceId = -1;
  int pinNumber = -1;
};

class PneumaticSlide {
public:
  PneumaticSlide(const std::string& name,
    const IOPinConfig& output,
    const IOPinConfig& extendedInput,
    const IOPinConfig& retractedInput,
    int timeoutMs);

  ~PneumaticSlide() = default;

  // Get the current state of the slide
  SlideState getState() const { return m_state; }

  // Get the slide name
  const std::string& getName() const { return m_name; }

  // Extend the slide (move down)
  bool extend();

  // Retract the slide (move up)
  bool retract();

  // Update the state based on sensor inputs
  void updateState(bool extendedSensor, bool retractedSensor);

  // Set callback for state changes
  void setStateChangeCallback(std::function<void(const std::string&, SlideState)> callback) {
    m_stateChangeCallback = callback;
  }

  // Get a string representation of the current state
  std::string getStateString() const;

  // Get pin configurations
  const IOPinConfig& getOutputConfig() const { return m_outputConfig; }
  const IOPinConfig& getExtendedInputConfig() const { return m_extendedInputConfig; }
  const IOPinConfig& getRetractedInputConfig() const { return m_retractedInputConfig; }

  // Get timeout value
  int getTimeoutMs() const { return m_timeoutMs; }

  // Reset the state to UNKNOWN (e.g., after an error)
  void resetState();

private:
  std::string m_name;                // Name of the pneumatic slide
  IOPinConfig m_outputConfig;        // Control output pin configuration
  IOPinConfig m_extendedInputConfig; // Extended sensor input pin configuration
  IOPinConfig m_retractedInputConfig; // Retracted sensor input pin configuration
  int m_timeoutMs;                   // Timeout in milliseconds for movement

  SlideState m_state;                // Current state of the slide
  std::chrono::steady_clock::time_point m_movementStartTime; // When movement began

  // Callback for state changes
  std::function<void(const std::string&, SlideState)> m_stateChangeCallback;

  // Helper to update state with notification
  void setState(SlideState newState);

  // Check if movement has timed out
  bool hasTimedOut() const;
};