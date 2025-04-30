#include "PneumaticSlide.h"
#include <iostream>

PneumaticSlide::PneumaticSlide(const std::string& name,
  const IOPinConfig& output,
  const IOPinConfig& extendedInput,
  const IOPinConfig& retractedInput,
  int timeoutMs)
  : m_name(name),
  m_outputConfig(output),
  m_extendedInputConfig(extendedInput),
  m_retractedInputConfig(retractedInput),
  m_timeoutMs(timeoutMs),
  m_state(SlideState::UNKNOWN)
{
  // Initialize the movement start time
  m_movementStartTime = std::chrono::steady_clock::now();

  std::cout << "Created pneumatic slide: " << name << std::endl;
}

bool PneumaticSlide::extend() {
  // Log the operation
  std::cout << "Extending pneumatic slide: " << m_name << std::endl;

  // Set the movement start time
  m_movementStartTime = std::chrono::steady_clock::now();

  // Update the state to MOVING
  setState(SlideState::MOVING);

  // Return true to indicate the command was accepted
  // The actual result will be determined by sensor feedback
  return true;
}

bool PneumaticSlide::retract() {
  // Log the operation
  std::cout << "Retracting pneumatic slide: " << m_name << std::endl;

  // Set the movement start time
  m_movementStartTime = std::chrono::steady_clock::now();

  // Update the state to MOVING
  setState(SlideState::MOVING);

  // Return true to indicate the command was accepted
  // The actual result will be determined by sensor feedback
  return true;
}

void PneumaticSlide::updateState(bool extendedSensor, bool retractedSensor) {
  // Determine the new state based on sensor inputs
  SlideState newState = m_state;

  if (extendedSensor && retractedSensor) {
    // Both sensors are active - this is an error
    newState = SlideState::P_ERROR;
  }
  else if (extendedSensor) {
    // Extended sensor active - slide is down
    newState = SlideState::EXTENDED;
  }
  else if (retractedSensor) {
    // Retracted sensor active - slide is up
    newState = SlideState::RETRACTED;
  }
  else if (m_state == SlideState::MOVING) {
    // No sensors active, but we're in a moving state
    // Check for timeout
    if (hasTimedOut()) {
      newState = SlideState::P_ERROR;
    }
    // Otherwise, remain in MOVING state
  }
  else if (m_state == SlideState::UNKNOWN) {
    // No sensors active and we don't know the state
    // Remain in UNKNOWN state
  }
  else {
    // No sensors active but we were in a known state before
    // This could be a sensor failure or we're moving
    newState = SlideState::MOVING;
    // Reset the movement timer since we just detected movement
    m_movementStartTime = std::chrono::steady_clock::now();
  }

  // Update the state if it changed
  if (newState != m_state) {
    setState(newState);
  }
}

void PneumaticSlide::setState(SlideState newState) {
  // Only update if the state has changed
  if (m_state != newState) {
    m_state = newState;

    // Log the state change
    std::cout << "Pneumatic slide " << m_name << " state changed to: "
      << getStateString() << std::endl;

    // Call the state change callback if set
    if (m_stateChangeCallback) {
      m_stateChangeCallback(m_name, m_state);
    }
  }
}

bool PneumaticSlide::hasTimedOut() const {
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_movementStartTime).count();
  return elapsed > m_timeoutMs;
}

std::string PneumaticSlide::getStateString() const {
  switch (m_state) {
  case SlideState::UNKNOWN:
    return "Unknown";
  case SlideState::RETRACTED:
    return "Retracted (Up)";
  case SlideState::EXTENDED:
    return "Extended (Down)";
  case SlideState::MOVING:
    return "Moving";
  case SlideState::P_ERROR:
    return "Error";
  default:
    return "Invalid State";
  }
}

void PneumaticSlide::resetState() {
  setState(SlideState::UNKNOWN);
  m_movementStartTime = std::chrono::steady_clock::now();
}