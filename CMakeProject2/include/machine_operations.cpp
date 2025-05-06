// machine_operations.cpp
#include "machine_operations.h"
#include "include/cld101x_operations.h"  // Include it here, not in the header



MachineOperations::MachineOperations(
  MotionControlLayer& motionLayer,
  PIControllerManager& piControllerManager,
  EziIOManager& ioManager,
  PneumaticManager& pneumaticManager,
  CLD101xOperations* laserOps  // New parameter
) : m_motionLayer(motionLayer),
m_piControllerManager(piControllerManager),
m_ioManager(ioManager),
m_pneumaticManager(pneumaticManager),
m_laserOps(laserOps)
{
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("MachineOperations: Initialized");
}


MachineOperations::~MachineOperations() {
  m_logger->LogInfo("MachineOperations: Shutting down");
}

// Move a device to a specific node in the graph
bool MachineOperations::MoveDeviceToNode(const std::string& deviceName, const std::string& graphName,
  const std::string& targetNodeId, bool blocking) {
  m_logger->LogInfo("MachineOperations: Moving device " + deviceName +
    " to node " + targetNodeId + " in graph " + graphName);

  // Get the current node for the device
  std::string currentNodeId;
  if (!m_motionLayer.GetDeviceCurrentNode(graphName, deviceName, currentNodeId)) {
    m_logger->LogError("MachineOperations: Failed to get current node for device " + deviceName);
    return false;
  }

  // Already at the target node
  if (currentNodeId == targetNodeId) {
    m_logger->LogInfo("MachineOperations: Device " + deviceName +
      " is already at node " + targetNodeId);
    return true;
  }

  // Plan and execute path
  return MovePathFromTo(deviceName, graphName, currentNodeId, targetNodeId, blocking);
}

// Move a device along a path from start to end node
bool MachineOperations::MovePathFromTo(const std::string& deviceName, const std::string& graphName,
  const std::string& startNodeId, const std::string& endNodeId,
  bool blocking) {
  m_logger->LogInfo("MachineOperations: Planning path for device " + deviceName +
    " from " + startNodeId + " to " + endNodeId + " in graph " + graphName);

  // Plan the path
  if (!m_motionLayer.PlanPath(graphName, startNodeId, endNodeId)) {
    m_logger->LogError("MachineOperations: Failed to plan path from " +
      startNodeId + " to " + endNodeId);
    return false;
  }

  // Execute the path
  m_logger->LogInfo("MachineOperations: Executing path");
  bool success = m_motionLayer.ExecutePath(blocking);

  if (success) {
    m_logger->LogInfo("MachineOperations: Path execution " +
      std::string(blocking ? "completed" : "started"));
  }
  else {
    m_logger->LogError("MachineOperations: Path execution failed");
  }

  return success;
}

// Set an output state by device name
bool MachineOperations::SetOutput(const std::string& deviceName, int outputPin, bool state) {
  m_logger->LogInfo("MachineOperations: Setting output pin " + std::to_string(outputPin) +
    " on device " + deviceName + " to " + (state ? "ON" : "OFF"));

  // Get device by name
  EziIODevice* device = m_ioManager.getDeviceByName(deviceName);
  if (!device) {
    m_logger->LogError("MachineOperations: Device not found: " + deviceName);
    return false;
  }

  return device->setOutput(outputPin, state);
}

// Set an output state by device ID
bool MachineOperations::SetOutput(int deviceId, int outputPin, bool state) {
  m_logger->LogInfo("MachineOperations: Setting output pin " + std::to_string(outputPin) +
    " on device ID " + std::to_string(deviceId) + " to " + (state ? "ON" : "OFF"));

  return m_ioManager.setOutput(deviceId, outputPin, state);
}

// Read input state by device name
bool MachineOperations::ReadInput(const std::string& deviceName, int inputPin, bool& state) {
  m_logger->LogInfo("MachineOperations: Reading input pin " + std::to_string(inputPin) +
    " on device " + deviceName);

  // Get device by name
  EziIODevice* device = m_ioManager.getDeviceByName(deviceName);
  if (!device) {
    m_logger->LogError("MachineOperations: Device not found: " + deviceName);
    return false;
  }

  // Read inputs
  uint32_t inputs = 0, latch = 0;
  if (!device->readInputs(inputs, latch)) {
    m_logger->LogError("MachineOperations: Failed to read inputs from device " + deviceName);
    return false;
  }

  // Check if the pin is within range
  if (inputPin >= device->getInputCount()) {
    m_logger->LogError("MachineOperations: Invalid input pin " + std::to_string(inputPin) +
      " for device " + deviceName);
    return false;
  }

  // Check the pin state
  state = ConvertPinStateToBoolean(inputs, inputPin);
  return true;
}

// Read input state by device ID
bool MachineOperations::ReadInput(int deviceId, int inputPin, bool& state) {
  m_logger->LogInfo("MachineOperations: Reading input pin " + std::to_string(inputPin) +
    " on device ID " + std::to_string(deviceId));

  // Read inputs
  uint32_t inputs = 0, latch = 0;
  if (!m_ioManager.readInputs(deviceId, inputs, latch)) {
    m_logger->LogError("MachineOperations: Failed to read inputs from device ID " +
      std::to_string(deviceId));
    return false;
  }

  // Check the pin state
  state = ConvertPinStateToBoolean(inputs, inputPin);
  return true;
}

// Clear latch by device name and pin
bool MachineOperations::ClearLatch(const std::string& deviceName, int inputPin) {
  m_logger->LogInfo("MachineOperations: Clearing latch for input pin " +
    std::to_string(inputPin) + " on device " + deviceName);

  // Get device by name
  EziIODevice* device = m_ioManager.getDeviceByName(deviceName);
  if (!device) {
    m_logger->LogError("MachineOperations: Device not found: " + deviceName);
    return false;
  }

  // Create mask for this pin
  uint32_t latchMask = 1 << inputPin;
  return device->clearLatch(latchMask);
}

// Clear latch by device ID and mask
bool MachineOperations::ClearLatch(int deviceId, uint32_t latchMask) {
  m_logger->LogInfo("MachineOperations: Clearing latch with mask 0x" +
    std::to_string(latchMask) + " on device ID " + std::to_string(deviceId));

  // Get device by ID
  EziIODevice* device = m_ioManager.getDevice(deviceId);
  if (!device) {
    m_logger->LogError("MachineOperations: Device not found with ID: " +
      std::to_string(deviceId));
    return false;
  }

  return device->clearLatch(latchMask);
}

// Extend a pneumatic slide
bool MachineOperations::ExtendSlide(const std::string& slideName, bool waitForCompletion, int timeoutMs) {
  m_logger->LogInfo("MachineOperations: Extending slide " + slideName);

  bool success = m_pneumaticManager.extendSlide(slideName);
  if (!success) {
    m_logger->LogError("MachineOperations: Failed to extend slide " + slideName);
    return false;
  }

  // If waiting for completion is requested
  if (waitForCompletion) {
    return WaitForSlideState(slideName, SlideState::EXTENDED, timeoutMs);
  }

  return true;
}

// Retract a pneumatic slide
bool MachineOperations::RetractSlide(const std::string& slideName, bool waitForCompletion, int timeoutMs) {
  m_logger->LogInfo("MachineOperations: Retracting slide " + slideName);

  bool success = m_pneumaticManager.retractSlide(slideName);
  if (!success) {
    m_logger->LogError("MachineOperations: Failed to retract slide " + slideName);
    return false;
  }

  // If waiting for completion is requested
  if (waitForCompletion) {
    return WaitForSlideState(slideName, SlideState::RETRACTED, timeoutMs);
  }

  return true;
}

// Get the current state of a pneumatic slide
SlideState MachineOperations::GetSlideState(const std::string& slideName) {
  return m_pneumaticManager.getSlideState(slideName);
}

// Wait for a pneumatic slide to reach a specific state
bool MachineOperations::WaitForSlideState(const std::string& slideName, SlideState targetState, int timeoutMs) {
  m_logger->LogInfo("MachineOperations: Waiting for slide " + slideName +
    " to reach state: " + std::to_string(static_cast<int>(targetState)));

  auto startTime = std::chrono::steady_clock::now();
  auto endTime = startTime + std::chrono::milliseconds(timeoutMs);

  while (std::chrono::steady_clock::now() < endTime) {
    SlideState currentState = m_pneumaticManager.getSlideState(slideName);

    if (currentState == targetState) {
      m_logger->LogInfo("MachineOperations: Slide " + slideName + " reached target state");
      return true;
    }

    // Check for error state
    if (currentState == SlideState::P_ERROR) {
      m_logger->LogError("MachineOperations: Slide " + slideName + " is in ERROR state");
      return false;
    }

    // Sleep to avoid CPU spinning
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  m_logger->LogError("MachineOperations: Timeout waiting for slide " + slideName +
    " to reach target state");
  return false;
}

// Wait for a specified time
void MachineOperations::Wait(int milliseconds) {
  if (milliseconds <= 0) return;

  m_logger->LogInfo("MachineOperations: Waiting for " + std::to_string(milliseconds) + " ms");
  std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

// Read a value from the global data store
float MachineOperations::ReadDataValue(const std::string& dataId, float defaultValue) {
  float value = GlobalDataStore::GetInstance()->GetValue(dataId, defaultValue);
  m_logger->LogInfo("MachineOperations: Read value from " + dataId + ": " + std::to_string(value));
  return value;
}

// Check if a data value exists
bool MachineOperations::HasDataValue(const std::string& dataId) {
  bool hasValue = GlobalDataStore::GetInstance()->HasValue(dataId);
  m_logger->LogInfo("MachineOperations: Checked if data exists for " + dataId + ": " +
    (hasValue ? "yes" : "no"));
  return hasValue;
}

// Perform a scan operation
bool MachineOperations::PerformScan(const std::string& deviceName, const std::string& dataChannel,
  const std::vector<double>& stepSizes, int settlingTimeMs,
  const std::vector<std::string>& axesToScan) {
  m_logger->LogInfo("MachineOperations: Starting scan for device " + deviceName +
    " using data channel " + dataChannel);

  // Get the PI controller for the device
  PIController* controller = m_piControllerManager.GetController(deviceName);
  if (!controller || !controller->IsConnected()) {
    m_logger->LogError("MachineOperations: No connected PI controller for device " + deviceName);
    return false;
  }

  // Setup scanning parameters
  ScanningParameters params = ScanningParameters::CreateDefault();
  params.axesToScan = axesToScan;
  params.stepSizes = stepSizes;
  params.motionSettleTimeMs = settlingTimeMs;

  try {
    // Validate parameters
    params.Validate();

    // Create scanning algorithm
    GlobalDataStore* dataStore = GlobalDataStore::GetInstance();
    ScanningAlgorithm scanner(*controller, *dataStore, deviceName, dataChannel, params);

    // Start the scan (blocking)
    m_logger->LogInfo("MachineOperations: Executing scan");
    bool success = scanner.StartScan();

    if (success) {
      m_logger->LogInfo("MachineOperations: Scan started for device " + deviceName);

      // Wait for scan to complete
      while (scanner.IsScanningActive()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      m_logger->LogInfo("MachineOperations: Scan completed for device " + deviceName);
      return true;
    }
    else {
      m_logger->LogError("MachineOperations: Failed to start scan for device " + deviceName);
      return false;
    }
  }
  catch (const std::exception& e) {
    m_logger->LogError("MachineOperations: Exception during scan: " + std::string(e.what()));
    return false;
  }
}

// Check if device is connected
bool MachineOperations::IsDeviceConnected(const std::string& deviceName) {
  // Check if it's a PI controller
  PIController* piController = m_piControllerManager.GetController(deviceName);
  if (piController) {
    return piController->IsConnected();
  }

  // Check if it's an EziIO device
  EziIODevice* eziioDevice = m_ioManager.getDeviceByName(deviceName);
  if (eziioDevice) {
    return eziioDevice->isConnected();
  }

  // Device not found
  m_logger->LogWarning("MachineOperations: Device not found: " + deviceName);
  return false;
}

// Check if slide is extended
bool MachineOperations::IsSlideExtended(const std::string& slideName) {
  SlideState state = m_pneumaticManager.getSlideState(slideName);
  return state == SlideState::EXTENDED;
}

// Check if slide is retracted
bool MachineOperations::IsSlideRetracted(const std::string& slideName) {
  SlideState state = m_pneumaticManager.getSlideState(slideName);
  return state == SlideState::RETRACTED;
}

// Check if slide is moving
bool MachineOperations::IsSlideMoving(const std::string& slideName) {
  SlideState state = m_pneumaticManager.getSlideState(slideName);
  return state == SlideState::MOVING;
}

// Check if slide is in error state
bool MachineOperations::IsSlideInError(const std::string& slideName) {
  SlideState state = m_pneumaticManager.getSlideState(slideName);
  return state == SlideState::P_ERROR;
}

// Get EziIO device ID from name
int MachineOperations::GetDeviceId(const std::string& deviceName) {
  EziIODevice* device = m_ioManager.getDeviceByName(deviceName);
  if (!device) {
    m_logger->LogError("MachineOperations: Device not found: " + deviceName);
    return -1;
  }
  return device->getDeviceId();
}

// Helper to convert pin state to boolean
bool MachineOperations::ConvertPinStateToBoolean(uint32_t inputs, int pin) {
  // Check if the specific pin is high
  return (inputs & (1 << pin)) != 0;
}

// Add implementations for the new methods:
bool MachineOperations::LaserOn(const std::string& laserName) {
  if (!m_laserOps) {
    m_logger->LogError("MachineOperations: No laser operations module available");
    return false;
  }
  return m_laserOps->LaserOn(laserName);
}

bool MachineOperations::LaserOff(const std::string& laserName) {
  if (!m_laserOps) {
    m_logger->LogError("MachineOperations: No laser operations module available");
    return false;
  }
  return m_laserOps->LaserOff(laserName);
}

bool MachineOperations::TECOn(const std::string& laserName) {
  if (!m_laserOps) {
    m_logger->LogError("MachineOperations: No laser operations module available");
    return false;
  }
  return m_laserOps->TECOn(laserName);
}

bool MachineOperations::TECOff(const std::string& laserName) {
  if (!m_laserOps) {
    m_logger->LogError("MachineOperations: No laser operations module available");
    return false;
  }
  return m_laserOps->TECOff(laserName);
}

bool MachineOperations::SetLaserCurrent(float current, const std::string& laserName) {
  if (!m_laserOps) {
    m_logger->LogError("MachineOperations: No laser operations module available");
    return false;
  }
  return m_laserOps->SetLaserCurrent(current, laserName);
}

bool MachineOperations::SetTECTemperature(float temperature, const std::string& laserName) {
  if (!m_laserOps) {
    m_logger->LogError("MachineOperations: No laser operations module available");
    return false;
  }
  return m_laserOps->SetTECTemperature(temperature, laserName);
}

float MachineOperations::GetLaserTemperature(const std::string& laserName) {
  if (!m_laserOps) {
    m_logger->LogError("MachineOperations: No laser operations module available");
    return 0.0f;
  }
  return m_laserOps->GetTemperature(laserName);
}

float MachineOperations::GetLaserCurrent(const std::string& laserName) {
  if (!m_laserOps) {
    m_logger->LogError("MachineOperations: No laser operations module available");
    return 0.0f;
  }
  return m_laserOps->GetLaserCurrent(laserName);
}

bool MachineOperations::WaitForLaserTemperature(float targetTemp, float tolerance,
  int timeoutMs, const std::string& laserName) {
  if (!m_laserOps) {
    m_logger->LogError("MachineOperations: No laser operations module available");
    return false;
  }
  return m_laserOps->WaitForTemperatureStabilization(targetTemp, tolerance, timeoutMs, laserName);
}