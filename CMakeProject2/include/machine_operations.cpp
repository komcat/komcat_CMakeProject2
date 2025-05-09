// machine_operations.cpp
#include "machine_operations.h"
#include "include/cld101x_operations.h"  // Include it here, not in the header
#include <sstream>


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

bool MachineOperations::MoveToPointName(const std::string& deviceName, const std::string& positionName, bool blocking) {
  m_logger->LogInfo("MachineOperations: Moving device " + deviceName + " to named position " + positionName);

  // Check if the device is connected
  if (!IsDeviceConnected(deviceName)) {
    m_logger->LogError("MachineOperations: Device not connected: " + deviceName);
    return false;
  }

  // Get the named position from the motion layer configuration
  auto posOpt = m_motionLayer.GetConfigManager().GetNamedPosition(deviceName, positionName);
  if (!posOpt.has_value()) {
    m_logger->LogError("MachineOperations: Position " + positionName + " not found for device " + deviceName);
    return false;
  }

  const auto& targetPosition = posOpt.value().get();

  // Log detailed position information
  std::stringstream positionLog;
  positionLog << "MachineOperations: Moving device " << deviceName
    << " to position " << positionName
    << " - Coordinates: "
    << "X:" << targetPosition.x << ", "
    << "Y:" << targetPosition.y << ", "
    << "Z:" << targetPosition.z;

  // Include rotation values if any are non-zero
  if (targetPosition.u != 0.0 || targetPosition.v != 0.0 || targetPosition.w != 0.0) {
    positionLog << ", U:" << targetPosition.u << ", "
      << "V:" << targetPosition.v << ", "
      << "W:" << targetPosition.w;
  }

  m_logger->LogInfo(positionLog.str());

  // Determine which controller to use and move to position
  bool success = false;

  // Use the motion layer to perform the movement instead of direct controller calls
  // This avoids the need to access the controller managers directly
  if (blocking) {
    // For blocking moves, use direct position coordinates
    success = m_motionLayer.MoveToPosition(deviceName, targetPosition, true);
  }
  else {
    // For non-blocking moves, use direct position coordinates
    success = m_motionLayer.MoveToPosition(deviceName, targetPosition, false);
  }

  if (success) {
    m_logger->LogInfo("MachineOperations: Successfully moved device " + deviceName + " to position " + positionName);
  }
  else {
    m_logger->LogError("MachineOperations: Failed to move device " + deviceName + " to position " + positionName);
  }

  return success;
}



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

  // If not a PI controller, check if it's an ACS controller
  auto deviceOpt = m_motionLayer.GetConfigManager().GetDevice(deviceName);
  if (!deviceOpt.has_value()) {
    m_logger->LogWarning("Device " + deviceName + " not found in configuration");
    return false;
  }

  const auto& device = deviceOpt.value().get();
  if (device.Port == 701) { // ACS controller typically uses port 701
    // Access the ACS controller manager through motion layer
    ACSController* acsController = m_motionLayer.GetACSControllerManager().GetController(deviceName);
    if (acsController) {
      return acsController->IsConnected();
    }
  }

  // Check if it's an EziIO device as a fallback
  EziIODevice* eziioDevice = m_ioManager.getDeviceByName(deviceName);
  if (eziioDevice) {
    return eziioDevice->isConnected();
  }

  // Device not found in any controller manager
  m_logger->LogWarning("Device " + deviceName + " not found in any controller manager");
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


bool MachineOperations::StartScan(const std::string& deviceName, const std::string& dataChannel,
  const std::vector<double>& stepSizes, int settlingTimeMs,
  const std::vector<std::string>& axesToScan) {
  // Check if a scan is already active for this device
    {
      std::lock_guard<std::mutex> lock(m_scanMutex);
      if (m_activeScans.find(deviceName) != m_activeScans.end()) {
        m_logger->LogWarning("MachineOperations: Scan already in progress for device " + deviceName);
        return false;
      }
    }

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

      // Create and configure the scanning algorithm
      auto scanner = std::make_unique<ScanningAlgorithm>(
        *controller,
        *GlobalDataStore::GetInstance(), // Use global data store
        deviceName,
        dataChannel,
        params
      );

      // Initialize scan info - create it in the map if it doesn't exist yet
      {
        std::lock_guard<std::mutex> lock(m_scanMutex);

        // Create the entry if it doesn't exist
        if (m_scanInfo.find(deviceName) == m_scanInfo.end()) {
          m_scanInfo.emplace(std::piecewise_construct,
            std::forward_as_tuple(deviceName),
            std::forward_as_tuple());
        }

        // Initialize the values directly
        m_scanInfo[deviceName].isActive.store(true);
        m_scanInfo[deviceName].progress.store(0.0);

        // Set the status with proper locking
        {
          std::lock_guard<std::mutex> statusLock(m_scanInfo[deviceName].statusMutex);
          m_scanInfo[deviceName].status = "Starting scan...";
        }
      }

      // Set callbacks to update status
      scanner->SetProgressCallback([this, deviceName](const ScanProgressEventArgs& args) {
        this->m_scanInfo[deviceName].progress.store(args.GetProgress());
        std::lock_guard<std::mutex> lock(this->m_scanInfo[deviceName].statusMutex);
        this->m_scanInfo[deviceName].status = args.GetStatus();
      });

      scanner->SetPeakUpdateCallback([this, deviceName](double value, const PositionStruct& position, const std::string& context) {
        std::lock_guard<std::mutex> lock(this->m_scanInfo[deviceName].peakMutex);
        this->m_scanInfo[deviceName].peakValue = value;
        this->m_scanInfo[deviceName].peakPosition = position;
      });

      scanner->SetCompletionCallback([this, deviceName](const ScanCompletedEventArgs& args) {
        // Update status, but don't remove scanner here
        this->m_scanInfo[deviceName].isActive.store(false);
        this->m_scanInfo[deviceName].progress.store(1.0);
        std::lock_guard<std::mutex> lock(this->m_scanInfo[deviceName].statusMutex);
        this->m_scanInfo[deviceName].status = "Scan completed";

        // Schedule cleanup to happen later from the main thread
        // We can't erase from m_activeScans here since we're running in the scanner's thread
        // The scanner will be cleaned up when another scan is started or the program ends
      });

      scanner->SetErrorCallback([this, deviceName](const ScanErrorEventArgs& args) {
        // Update status, but don't remove scanner here
        this->m_scanInfo[deviceName].isActive.store(false);
        std::lock_guard<std::mutex> lock(this->m_scanInfo[deviceName].statusMutex);
        this->m_scanInfo[deviceName].status = "Error: " + args.GetError();

        // Same as above - don't erase from m_activeScans here
      });

      // Start the scan
      if (!scanner->StartScan()) {
        m_logger->LogError("MachineOperations: Failed to start scan for device " + deviceName);
        return false;
      }

      // Store the scanner
      {
        std::lock_guard<std::mutex> lock(m_scanMutex);
        m_activeScans[deviceName] = std::move(scanner);
      }

      m_logger->LogInfo("MachineOperations: Scan started for device " + deviceName);
      return true;
    }
    catch (const std::exception& e) {
      m_logger->LogError("MachineOperations: Exception during scan setup: " + std::string(e.what()));
      return false;
    }
}

bool MachineOperations::StopScan(const std::string& deviceName) {
  std::unique_ptr<ScanningAlgorithm> scanner;

  // Get the scanner and remove it from active scans
  {
    std::lock_guard<std::mutex> lock(m_scanMutex);
    auto it = m_activeScans.find(deviceName);
    if (it == m_activeScans.end()) {
      m_logger->LogWarning("MachineOperations: No active scan for device " + deviceName);
      return false;
    }

    scanner = std::move(it->second);
    m_activeScans.erase(it);
  }

  // Stop the scan
  if (scanner) {
    scanner->HaltScan();
    m_logger->LogInfo("MachineOperations: Scan stopped for device " + deviceName);

     // Update scan status
    auto it = m_scanInfo.find(deviceName);
    if (it != m_scanInfo.end()) {
        it->second.isActive.store(false);
        std::lock_guard<std::mutex> lock(it->second.statusMutex);
        it->second.status = "Scan stopped by user";
    }
    
    // Use our safe cleanup method
    return SafelyCleanupScanner(deviceName);
  }

  return false;
}

bool MachineOperations::IsScanActive(const std::string& deviceName) const {
  auto it = m_scanInfo.find(deviceName);
  if (it != m_scanInfo.end()) {
    return it->second.isActive;
  }
  return false;
}

double MachineOperations::GetScanProgress(const std::string& deviceName) const {
  auto it = m_scanInfo.find(deviceName);
  if (it != m_scanInfo.end()) {
    return it->second.progress;
  }
  return 0.0;
}

std::string MachineOperations::GetScanStatus(const std::string& deviceName) const {
  auto it = m_scanInfo.find(deviceName);
  if (it != m_scanInfo.end()) {
    std::lock_guard<std::mutex> lock(it->second.statusMutex);
    return it->second.status;
  }
  return "No scan information available";
}

bool MachineOperations::GetScanPeak(const std::string& deviceName, double& value, PositionStruct& position) const {
  auto it = m_scanInfo.find(deviceName);
  if (it != m_scanInfo.end()) {
    std::lock_guard<std::mutex> lock(it->second.peakMutex);
    value = it->second.peakValue;
    position = it->second.peakPosition;
    return (value > 0.0); // Return true if a valid peak was found
  }
  return false;
}

// Safe cleanup method to call before destructing the scanner
bool MachineOperations::SafelyCleanupScanner(const std::string& deviceName) {
  std::unique_ptr<ScanningAlgorithm> scanner;

  // Get the scanner and remove it from active scans
  {
    std::lock_guard<std::mutex> lock(m_scanMutex);
    auto it = m_activeScans.find(deviceName);
    if (it == m_activeScans.end()) {
      return false;  // Nothing to clean up
    }

    scanner = std::move(it->second);
    m_activeScans.erase(it);
  }

  // Now safely stop the scan if it's running
  if (scanner) {
    if (scanner->IsScanningActive()) {
      scanner->HaltScan();

      // Wait a brief period to allow thread to exit
      for (int i = 0; i < 50; i++) {  // Wait up to 5 seconds
        if (!scanner->IsScanningActive()) {
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }

    // The scanner will be destructed when this method returns
    return true;
  }

  return false;
}


bool MachineOperations::IsDevicePIController(const std::string& deviceName) const {
  // Get the device info from configuration (using motion layer, which has access to config)
  auto deviceOpt = m_motionLayer.GetConfigManager().GetDevice(deviceName);
  if (!deviceOpt.has_value()) {
    m_logger->LogError("MachineOperations: Device " + deviceName + " not found in configuration");
    return false;
  }

  const auto& device = deviceOpt.value().get();

  // PI controllers use port 50000, ACS uses different ports (like 701)
  return (device.Port == 50000);
}

bool MachineOperations::IsDeviceMoving(const std::string& deviceName) {
  // Determine which controller manager to use
  if (IsDevicePIController(deviceName)) {
    PIController* controller = m_piControllerManager.GetController(deviceName);
    if (!controller || !controller->IsConnected()) {
      m_logger->LogError("MachineOperations: No connected PI controller for device " + deviceName);
      return false;
    }

    // Check all axes for motion using the existing IsMoving method
    for (const auto& axis : { "X", "Y", "Z", "U", "V", "W" }) {
      if (controller->IsMoving(axis)) {
        return true;
      }
    }

    return false;
  }
  else {
    // For ACS controllers or other devices
    // Fall back to position change detection if we can't check directly

    PositionStruct currentPos;
    if (!m_motionLayer.GetCurrentPosition(deviceName, currentPos)) {
      // Can't determine position
      return false;
    }

    // Store last positions and check times
    static std::map<std::string, PositionStruct> lastPositions;
    static std::map<std::string, std::chrono::steady_clock::time_point> lastCheckTimes;

    auto now = std::chrono::steady_clock::now();

    // First time checking this device
    if (lastPositions.find(deviceName) == lastPositions.end()) {
      lastPositions[deviceName] = currentPos;
      lastCheckTimes[deviceName] = now;
      return false; // Assume not moving on first check
    }

    // Get time since last check
    auto lastTime = lastCheckTimes[deviceName];
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime).count();

    // Only check if enough time has passed
    if (elapsed < 100) { // Less than 100ms since last check
      return false;
    }

    // Compare positions
    const auto& lastPos = lastPositions[deviceName];
    double tolerance = 0.0001; // 0.1 micron position change detection threshold

    bool posChanged =
      std::abs(currentPos.x - lastPos.x) > tolerance ||
      std::abs(currentPos.y - lastPos.y) > tolerance ||
      std::abs(currentPos.z - lastPos.z) > tolerance ||
      std::abs(currentPos.u - lastPos.u) > tolerance ||
      std::abs(currentPos.v - lastPos.v) > tolerance ||
      std::abs(currentPos.w - lastPos.w) > tolerance;

    // Update stored values
    lastPositions[deviceName] = currentPos;
    lastCheckTimes[deviceName] = now;

    return posChanged;
  }
}


bool MachineOperations::WaitForDeviceMotionCompletion(const std::string& deviceName, int timeoutMs) {
  m_logger->LogInfo("MachineOperations: Waiting for device " + deviceName + " motion to complete");

  auto startTime = std::chrono::steady_clock::now();
  auto endTime = startTime + std::chrono::milliseconds(timeoutMs);

  // First, wait a small amount of time to ensure motion has actually started
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Keep track of position stability
  bool wasMoving = false;
  int stableCount = 0;

  while (true) {
    bool isMoving = IsDeviceMoving(deviceName);

    if (isMoving) {
      wasMoving = true;
      stableCount = 0; // Reset stability counter
    }
    else if (wasMoving) {
      // Device has stopped moving, increment stability counter
      stableCount++;

      // If stable for 5 consecutive checks (about 250ms with 50ms sleep)
      if (stableCount >= 5) {
        m_logger->LogInfo("MachineOperations: Motion completed for device " + deviceName);
        return true;
      }
    }
    else {
      // Never detected any movement
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();

      // If we've waited reasonably long and never saw movement, assume it was quick or unnecessary
      if (elapsed > 1000) {
        m_logger->LogInfo("MachineOperations: No motion detected for device " + deviceName);
        return true;
      }
    }

    // Check for timeout
    if (std::chrono::steady_clock::now() > endTime) {
      m_logger->LogError("MachineOperations: Timeout waiting for motion completion of device " + deviceName);
      return false;
    }

    // Sleep to avoid CPU spinning
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}