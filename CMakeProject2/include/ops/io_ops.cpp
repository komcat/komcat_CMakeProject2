// io_ops.cpp
#include "io_ops.h"
#include <algorithm>

IOOps::IOOps(
  EziIOManager& ioManager,
  PneumaticManager& pneumaticManager,
  std::shared_ptr<DatabaseManager> dbManager,
  std::shared_ptr<OperationResultsManager> resultsManager
) : m_ioManager(ioManager),
m_pneumaticManager(pneumaticManager),
m_dbManager(dbManager),
m_resultsManager(resultsManager)
{
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("IOOps: Initialized");
}

IOOps::~IOOps() {
  m_logger->LogInfo("IOOps: Shutting down");
}

bool IOOps::Initialize()
{
	return true; // Initialization logic if needed
}

// Digital I/O methods
bool IOOps::SetOutput(const std::string& deviceName, int outputPin, bool state,
  const std::string& callerContext) {

  // Start operation tracking with caller context
  std::string opId;
  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"output_pin", std::to_string(outputPin)},
        {"target_state", state ? "true" : "false"}
    };

    // Extract sequence name from caller context if possible
    std::string sequenceName = "";
    if (callerContext.find("Initialization") != std::string::npos) {
      sequenceName = "Initialization";
    }
    else if (callerContext.find("ProcessStep") != std::string::npos) {
      sequenceName = "Process";
    }

    opId = m_resultsManager->StartOperation("SetOutput", deviceName,
      callerContext, sequenceName, parameters);
  }

  m_logger->LogInfo("IOOps: Setting output pin " + std::to_string(outputPin) +
    " on device " + deviceName + " to " + (state ? "ON" : "OFF") +
    (callerContext.empty() ? "" : " (called by: " + callerContext + ")") +
    (opId.empty() ? "" : " [" + opId + "]"));

  bool success = false;
  std::string errorMessage = "";

  try {
    // Get device by name
    EziIODevice* device = m_ioManager.getDeviceByName(deviceName);
    if (!device) {
      errorMessage = "Device not found";
      if (m_resultsManager && !opId.empty()) {
        m_resultsManager->EndOperation(opId, "failed", errorMessage);
      }
      m_logger->LogError("IOOps: " + errorMessage);
      return false;
    }

    // Execute the operation
    success = device->setOutput(outputPin, state);

    if (success) {
      if (m_resultsManager && !opId.empty()) {
        m_resultsManager->StoreResult(opId, "final_state", state ? "true" : "false");
        m_resultsManager->EndOperation(opId, "success");
      }
    }
    else {
      errorMessage = "Failed to set output";
      if (m_resultsManager && !opId.empty()) {
        m_resultsManager->EndOperation(opId, "failed", errorMessage);
      }
      m_logger->LogError("IOOps: " + errorMessage);
    }

  }
  catch (const std::exception& e) {
    errorMessage = "Exception: " + std::string(e.what());
    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->EndOperation(opId, "failed", errorMessage);
    }
    m_logger->LogError("IOOps: " + errorMessage);
    success = false;
  }

  return success;
}

bool IOOps::SetOutput(int deviceId, int outputPin, bool state) {
  m_logger->LogInfo("IOOps: Setting output pin " + std::to_string(outputPin) +
    " on device ID " + std::to_string(deviceId) + " to " + (state ? "ON" : "OFF"));

  return m_ioManager.setOutput(deviceId, outputPin, state);
}

bool IOOps::ReadInput(const std::string& deviceName, int inputPin, bool& state,
  const std::string& callerContext) {

  // Start operation tracking
  std::string opId;
  auto startTime = std::chrono::steady_clock::now();

  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"device_name", deviceName},
        {"input_pin", std::to_string(inputPin)}
    };
    opId = m_resultsManager->StartOperation("ReadInput", deviceName, callerContext, "", parameters);
  }

  m_logger->LogInfo("IOOps: Reading input pin " + std::to_string(inputPin) +
    " on device " + deviceName +
    (callerContext.empty() ? "" : " (called by: " + callerContext + ")"));

  // Get device by name
  EziIODevice* device = m_ioManager.getDeviceByName(deviceName);
  if (!device) {
    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
      m_resultsManager->EndOperation(opId, "failed", "Device not found: " + deviceName);
    }

    m_logger->LogError("IOOps: Device not found: " + deviceName);
    return false;
  }

  // Read inputs
  uint32_t inputs = 0, latch = 0;
  if (!device->readInputs(inputs, latch)) {
    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
      m_resultsManager->EndOperation(opId, "failed", "Failed to read inputs from device " + deviceName);
    }

    m_logger->LogError("IOOps: Failed to read inputs from device " + deviceName);
    return false;
  }

  // Check if the pin is within range
  if (inputPin >= device->getInputCount()) {
    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->StoreResult(opId, "input_count", std::to_string(device->getInputCount()));
      m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
      m_resultsManager->EndOperation(opId, "failed", "Invalid input pin " + std::to_string(inputPin));
    }

    m_logger->LogError("IOOps: Invalid input pin " + std::to_string(inputPin) +
      " for device " + deviceName);
    return false;
  }

  // Check the pin state
  state = ConvertPinStateToBoolean(inputs, inputPin);

  // Store results and end tracking
  auto endTime = std::chrono::steady_clock::now();
  auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

  if (m_resultsManager && !opId.empty()) {
    m_resultsManager->StoreResult(opId, "pin_state", state ? "HIGH" : "LOW");
    m_resultsManager->StoreResult(opId, "raw_inputs", "0x" + std::to_string(inputs));
    m_resultsManager->StoreResult(opId, "latch_value", "0x" + std::to_string(latch));
    m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
    m_resultsManager->EndOperation(opId, "success");
  }

  return true;
}

bool IOOps::ReadInput(int deviceId, int inputPin, bool& state) {
  m_logger->LogInfo("IOOps: Reading input pin " + std::to_string(inputPin) +
    " on device ID " + std::to_string(deviceId));

  // Read inputs
  uint32_t inputs = 0, latch = 0;
  if (!m_ioManager.readInputs(deviceId, inputs, latch)) {
    m_logger->LogError("IOOps: Failed to read inputs from device ID " +
      std::to_string(deviceId));
    return false;
  }

  // Check the pin state
  state = ConvertPinStateToBoolean(inputs, inputPin);
  return true;
}

bool IOOps::ClearLatch(const std::string& deviceName, int inputPin,
  const std::string& callerContext) {

  // Start operation tracking
  std::string opId;
  auto startTime = std::chrono::steady_clock::now();

  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"device_name", deviceName},
        {"input_pin", std::to_string(inputPin)}
    };
    opId = m_resultsManager->StartOperation("ClearLatch", deviceName, callerContext, "", parameters);
  }

  m_logger->LogInfo("IOOps: Clearing latch for input pin " +
    std::to_string(inputPin) + " on device " + deviceName +
    (callerContext.empty() ? "" : " (called by: " + callerContext + ")"));

  // Get device by name
  EziIODevice* device = m_ioManager.getDeviceByName(deviceName);
  if (!device) {
    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
      m_resultsManager->EndOperation(opId, "failed", "Device not found: " + deviceName);
    }

    m_logger->LogError("IOOps: Device not found: " + deviceName);
    return false;
  }

  // Create mask for this pin and clear latch
  uint32_t latchMask = 1 << inputPin;
  bool success = device->clearLatch(latchMask);

  // Store results and end tracking
  auto endTime = std::chrono::steady_clock::now();
  auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

  if (m_resultsManager && !opId.empty()) {
    m_resultsManager->StoreResult(opId, "latch_mask", "0x" + std::to_string(latchMask));
    m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));

    if (success) {
      m_resultsManager->EndOperation(opId, "success");
    }
    else {
      m_resultsManager->EndOperation(opId, "failed", "Failed to clear latch for pin " + std::to_string(inputPin));
    }
  }

  return success;
}

bool IOOps::ClearLatch(int deviceId, uint32_t latchMask) {
  m_logger->LogInfo("IOOps: Clearing latch with mask 0x" +
    std::to_string(latchMask) + " on device ID " + std::to_string(deviceId));

  // Get device by ID
  EziIODevice* device = m_ioManager.getDevice(deviceId);
  if (!device) {
    m_logger->LogError("IOOps: Device not found with ID: " +
      std::to_string(deviceId));
    return false;
  }

  return device->clearLatch(latchMask);
}

bool IOOps::ClearOutput(const std::string& deviceName, int outputPin,
  const std::string& callerContext) {

  // Start operation tracking with caller context
  std::string opId;
  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"output_pin", std::to_string(outputPin)},
        {"action", "clear"}
    };

    // Extract sequence name from caller context if possible
    std::string sequenceName = "";
    if (callerContext.find("Initialization") != std::string::npos) {
      sequenceName = "Initialization";
    }
    else if (callerContext.find("ProcessStep") != std::string::npos) {
      sequenceName = "Process";
    }
    else if (callerContext.find("Cleanup") != std::string::npos) {
      sequenceName = "Cleanup";
    }

    opId = m_resultsManager->StartOperation("ClearOutput", deviceName,
      callerContext, sequenceName, parameters);
  }

  m_logger->LogInfo("IOOps: Clearing output pin " + std::to_string(outputPin) +
    " on device " + deviceName +
    (callerContext.empty() ? "" : " (called by: " + callerContext + ")") +
    (opId.empty() ? "" : " [" + opId + "]"));

  bool success = false;
  std::string errorMessage = "";

  try {
    // Get device by name
    EziIODevice* device = m_ioManager.getDeviceByName(deviceName);
    if (!device) {
      errorMessage = "Device not found";
      if (m_resultsManager && !opId.empty()) {
        m_resultsManager->EndOperation(opId, "failed", errorMessage);
      }
      m_logger->LogError("IOOps: " + errorMessage);
      return false;
    }

    // Store previous state if possible (optional enhancement)
    bool previousState = false;
    // You could read current state here if your hardware supports it
    // device->getOutput(outputPin, previousState); // If this method exists

    // Execute the clear operation (set to false)
    success = device->setOutput(outputPin, false);

    if (success) {
      if (m_resultsManager && !opId.empty()) {
        m_resultsManager->StoreResult(opId, "previous_state", "unknown"); // or actual previous state
        m_resultsManager->StoreResult(opId, "final_state", "false");
        m_resultsManager->StoreResult(opId, "action_performed", "clear");
        m_resultsManager->EndOperation(opId, "success");
      }
      m_logger->LogInfo("IOOps: Successfully cleared output pin " +
        std::to_string(outputPin) + " on device " + deviceName);
    }
    else {
      errorMessage = "Failed to clear output";
      if (m_resultsManager && !opId.empty()) {
        m_resultsManager->EndOperation(opId, "failed", errorMessage);
      }
      m_logger->LogError("IOOps: " + errorMessage);
    }

  }
  catch (const std::exception& e) {
    errorMessage = "Exception: " + std::string(e.what());
    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->EndOperation(opId, "failed", errorMessage);
    }
    m_logger->LogError("IOOps: " + errorMessage);
    success = false;
  }

  return success;
}

// Pneumatic control methods
bool IOOps::ExtendSlide(const std::string& slideName, bool waitForCompletion,
  int timeoutMs, const std::string& callerContext) {

  // Start operation tracking
  std::string opId;
  auto startTime = std::chrono::steady_clock::now();

  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"slideName", slideName},
        {"waitForCompletion", waitForCompletion ? "true" : "false"},
        {"timeoutMs", std::to_string(timeoutMs)}
    };
    opId = m_resultsManager->StartOperation("ExtendSlide", slideName, callerContext, "", parameters);
  }

  m_logger->LogInfo("IOOps: Extending slide " + slideName);

  // Store initial state
  SlideState initialState = m_pneumaticManager.getSlideState(slideName);

  // Execute extend operation
  bool success = m_pneumaticManager.extendSlide(slideName);
  if (!success) {
    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->StoreResult(opId, "initial_state", std::to_string(static_cast<int>(initialState)));
      m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
      m_resultsManager->EndOperation(opId, "failed", "Failed to extend slide " + slideName);
    }

    m_logger->LogError("IOOps: Failed to extend slide " + slideName);
    return false;
  }

  // Wait for completion if requested
  bool finalSuccess = true;
  if (waitForCompletion) {
    finalSuccess = WaitForSlideState(slideName, SlideState::EXTENDED, timeoutMs, callerContext);
  }

  // Store results and end tracking
  auto endTime = std::chrono::steady_clock::now();
  auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

  if (m_resultsManager && !opId.empty()) {
    SlideState finalState = m_pneumaticManager.getSlideState(slideName);

    m_resultsManager->StoreResult(opId, "initial_state", std::to_string(static_cast<int>(initialState)));
    m_resultsManager->StoreResult(opId, "final_state", std::to_string(static_cast<int>(finalState)));
    m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
    m_resultsManager->StoreResult(opId, "wait_for_completion", waitForCompletion ? "true" : "false");

    if (finalSuccess) {
      m_resultsManager->EndOperation(opId, "success");
    }
    else {
      m_resultsManager->EndOperation(opId, "failed", "Slide extend operation failed or timed out");
    }
  }

  return finalSuccess;
}

bool IOOps::RetractSlide(const std::string& slideName, bool waitForCompletion,
  int timeoutMs, const std::string& callerContext) {

  // Start operation tracking
  std::string opId;
  auto startTime = std::chrono::steady_clock::now();

  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"slideName", slideName},
        {"waitForCompletion", waitForCompletion ? "true" : "false"},
        {"timeoutMs", std::to_string(timeoutMs)}
    };
    opId = m_resultsManager->StartOperation("RetractSlide", slideName, callerContext, "", parameters);
  }

  m_logger->LogInfo("IOOps: Retracting slide " + slideName);

  // Store initial state
  SlideState initialState = m_pneumaticManager.getSlideState(slideName);

  // Execute retract operation
  bool success = m_pneumaticManager.retractSlide(slideName);
  if (!success) {
    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    if (m_resultsManager && !opId.empty()) {
      m_resultsManager->StoreResult(opId, "initial_state", std::to_string(static_cast<int>(initialState)));
      m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
      m_resultsManager->EndOperation(opId, "failed", "Failed to retract slide " + slideName);
    }

    m_logger->LogError("IOOps: Failed to retract slide " + slideName);
    return false;
  }

  // Wait for completion if requested
  bool finalSuccess = true;
  if (waitForCompletion) {
    finalSuccess = WaitForSlideState(slideName, SlideState::RETRACTED, timeoutMs, callerContext);
  }

  // Store results and end tracking
  auto endTime = std::chrono::steady_clock::now();
  auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

  if (m_resultsManager && !opId.empty()) {
    SlideState finalState = m_pneumaticManager.getSlideState(slideName);

    m_resultsManager->StoreResult(opId, "initial_state", std::to_string(static_cast<int>(initialState)));
    m_resultsManager->StoreResult(opId, "final_state", std::to_string(static_cast<int>(finalState)));
    m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));
    m_resultsManager->StoreResult(opId, "wait_for_completion", waitForCompletion ? "true" : "false");

    if (finalSuccess) {
      m_resultsManager->EndOperation(opId, "success");
    }
    else {
      m_resultsManager->EndOperation(opId, "failed", "Slide retract operation failed or timed out");
    }
  }

  return finalSuccess;
}

SlideState IOOps::GetSlideState(const std::string& slideName) {
  return m_pneumaticManager.getSlideState(slideName);
}

bool IOOps::WaitForSlideState(const std::string& slideName, SlideState targetState,
  int timeoutMs, const std::string& callerContext) {

  // Start operation tracking
  std::string opId;
  auto startTime = std::chrono::steady_clock::now();

  if (m_resultsManager) {
    std::map<std::string, std::string> parameters = {
        {"slideName", slideName},
        {"targetState", std::to_string(static_cast<int>(targetState))},
        {"timeoutMs", std::to_string(timeoutMs)}
    };
    opId = m_resultsManager->StartOperation("WaitForSlideState", slideName, callerContext, "", parameters);
  }

  m_logger->LogInfo("IOOps: Waiting for slide " + slideName +
    " to reach state: " + std::to_string(static_cast<int>(targetState)));

  // Store initial state
  SlideState initialState = m_pneumaticManager.getSlideState(slideName);

  auto endTime = startTime + std::chrono::milliseconds(timeoutMs);
  bool success = false;
  SlideState finalState = initialState;

  // Wait loop
  while (std::chrono::steady_clock::now() < endTime) {
    SlideState currentState = m_pneumaticManager.getSlideState(slideName);
    finalState = currentState;

    if (currentState == targetState) {
      success = true;
      m_logger->LogInfo("IOOps: Slide " + slideName + " reached target state");
      break;
    }

    // Check for error state
    if (currentState == SlideState::P_ERROR) {
      m_logger->LogError("IOOps: Slide " + slideName + " is in ERROR state");
      break;
    }

    // Sleep to avoid CPU spinning
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  // Handle timeout
  if (!success && finalState != SlideState::P_ERROR) {
    m_logger->LogError("IOOps: Timeout waiting for slide " + slideName +
      " to reach target state");
  }

  // Store results and end tracking
  auto actualEndTime = std::chrono::steady_clock::now();
  auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(actualEndTime - startTime).count();

  if (m_resultsManager && !opId.empty()) {
    m_resultsManager->StoreResult(opId, "initial_state", std::to_string(static_cast<int>(initialState)));
    m_resultsManager->StoreResult(opId, "final_state", std::to_string(static_cast<int>(finalState)));
    m_resultsManager->StoreResult(opId, "target_state", std::to_string(static_cast<int>(targetState)));
    m_resultsManager->StoreResult(opId, "elapsed_time_ms", std::to_string(elapsedMs));

    if (success) {
      m_resultsManager->EndOperation(opId, "success");
    }
    else if (finalState == SlideState::P_ERROR) {
      m_resultsManager->EndOperation(opId, "failed", "Slide entered ERROR state");
    }
    else {
      m_resultsManager->EndOperation(opId, "failed", "Timeout waiting for target state");
    }
  }

  return success;
}

// Device status methods
bool IOOps::IsSlideExtended(const std::string& slideName) {
  SlideState state = m_pneumaticManager.getSlideState(slideName);
  return state == SlideState::EXTENDED;
}

bool IOOps::IsSlideRetracted(const std::string& slideName) {
  SlideState state = m_pneumaticManager.getSlideState(slideName);
  return state == SlideState::RETRACTED;
}

bool IOOps::IsSlideMoving(const std::string& slideName) {
  SlideState state = m_pneumaticManager.getSlideState(slideName);
  return state == SlideState::MOVING;
}

bool IOOps::IsSlideInError(const std::string& slideName) {
  SlideState state = m_pneumaticManager.getSlideState(slideName);
  return state == SlideState::P_ERROR;
}

int IOOps::GetDeviceId(const std::string& deviceName) {
  EziIODevice* device = m_ioManager.getDeviceByName(deviceName);
  if (!device) {
    m_logger->LogError("IOOps: Device not found: " + deviceName);
    return -1;
  }
  return device->getDeviceId();
}

// Helper method to convert pin state to boolean
bool IOOps::ConvertPinStateToBoolean(uint32_t inputs, int pin) {
  // Check if the specific pin is high
  return (inputs & (1 << pin)) != 0;
}

// Logging methods
void IOOps::LogInfo(const std::string& message) const {
  if (m_logger) {
    m_logger->LogInfo("IOOps: " + message);
  }
}

void IOOps::LogWarning(const std::string& message) const {
  if (m_logger) {
    m_logger->LogWarning("IOOps: " + message);
  }
}

void IOOps::LogError(const std::string& message) const {
  if (m_logger) {
    m_logger->LogError("IOOps: " + message);
  }
}