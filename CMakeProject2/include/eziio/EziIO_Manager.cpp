#include "EziIO_Manager.h"
#include <sstream>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

// Define the output pin masks
const uint32_t EziIODevice::OUTPUT_PIN_MASKS_16[16] = {
    0x10000,    // Pin 0
    0x20000,    // Pin 1
    0x40000,    // Pin 2
    0x80000,    // Pin 3
    0x100000,   // Pin 4
    0x200000,   // Pin 5
    0x400000,   // Pin 6
    0x800000,   // Pin 7
    0x1000000,  // Pin 8
    0x2000000,  // Pin 9
    0x4000000,  // Pin 10
    0x8000000,  // Pin 11
    0x10000000, // Pin 12
    0x20000000, // Pin 13
    0x40000000, // Pin 14
    0x80000000  // Pin 15
};

const uint32_t EziIODevice::OUTPUT_PIN_MASKS_8[8] = {
    0x100,     // Pin 0
    0x200,     // Pin 1
    0x400,     // Pin 2
    0x800,     // Pin 3
    0x1000,    // Pin 4
    0x2000,    // Pin 5
    0x4000,    // Pin 6
    0x8000     // Pin 7
};

// EziIODevice Implementation
EziIODevice::EziIODevice(int id, const std::string& name, const std::string& ip,
  int inputCount, int outputCount)
  : m_deviceId(id),
  m_name(name),
  m_ipAddress(ip),
  m_inputCount(inputCount),
  m_outputCount(outputCount),
  m_isConnected(false),
  m_lastInputs(0),
  m_lastLatch(0),
  m_lastOutputs(0),
  m_lastOutputStatus(0),
  m_statusUpdated(false),
  m_changeDetectionEnabled(true),  // Enable by default
  m_previousInputs(0),
  m_previousOutputs(0),
  m_firstRead(true)
{
  // Parse IP address string into bytes
  std::istringstream iss(ip);
  std::string token;
  int i = 0;
  while (std::getline(iss, token, '.') && i < 4) {
    m_ipBytes[i++] = static_cast<uint8_t>(std::stoi(token));
  }

  // Select the appropriate mask array based on output count
  m_currentOutputMasks = (outputCount <= 8) ? OUTPUT_PIN_MASKS_8 : OUTPUT_PIN_MASKS_16;
}

EziIODevice::~EziIODevice() {
  if (m_isConnected) {
    disconnect();
  }
}

uint32_t EziIODevice::getOutputPinMask(int pin) const {
  if (pin < 0 || pin >= m_outputCount) {
    std::cerr << "Pin number out of range: " << pin << std::endl;
    return 0;
  }

  return m_currentOutputMasks[pin];
}

bool EziIODevice::connect() {
  if (m_isConnected) {
    return true;
  }

  std::cout << "Attempting to connect to " << m_name
    << " at " << (int)m_ipBytes[0] << "." << (int)m_ipBytes[1]
    << "." << (int)m_ipBytes[2] << "." << (int)m_ipBytes[3]
    << " (Device ID: " << m_deviceId << ")" << std::endl;

  if (PE::FAS_ConnectTCP(m_ipBytes[0], m_ipBytes[1], m_ipBytes[2], m_ipBytes[3], m_deviceId)) {
    m_isConnected = true;
    m_firstRead = true;  // Reset first read flag on new connection
    std::cout << "✓ Connected successfully!" << std::endl;
    return true;
  }
  else {
    // Get error code if available
    std::cerr << "✗ Connection failed for " << m_name << std::endl;
    return false;
  }
}

bool EziIODevice::disconnect() {
  if (!m_isConnected) {
    return true; // Already disconnected
  }

  PE::FAS_Close(m_deviceId);
  m_isConnected = false;
  std::cout << "Disconnected from device " << m_name << " (ID: " << m_deviceId << ")" << std::endl;
  return true;
}

bool EziIODevice::readInputs(uint32_t& inputs, uint32_t& latch) {
  if (!m_isConnected) {
    std::cerr << "Device " << m_name << " not connected" << std::endl;
    return false;
  }

  // Create temporary unsigned long variables
  unsigned long temp_inputs = 0;
  unsigned long temp_latch = 0;

  // Call FAS_GetInput with the correct parameter types
  int result = PE::FAS_GetInput(m_deviceId, &temp_inputs, &temp_latch);

  // Check return value without namespace
  if (result == FMM_OK) {
    // Copy the values back to the uint32_t references
    inputs = static_cast<uint32_t>(temp_inputs);
    latch = static_cast<uint32_t>(temp_latch);

    // Update last known values
    {
      std::lock_guard<std::mutex> lock(m_statusMutex);
      m_lastInputs = inputs;
      m_lastLatch = latch;
      m_statusUpdated = true;
    }

    // Check for changes and notify observers if enabled
    if (m_changeDetectionEnabled) {
      checkAndNotifyInputChanges(inputs);
    }

    return true;
  }
  else {
    std::cerr << "Failed to read inputs from device " << m_name << ", error code: " << result << std::endl;
    return false;
  }
}

bool EziIODevice::getLastInputStatus(uint32_t& inputs, uint32_t& latch) {
  std::lock_guard<std::mutex> lock(m_statusMutex);
  inputs = m_lastInputs;
  latch = m_lastLatch;
  return m_statusUpdated;
}

bool EziIODevice::clearLatch(uint32_t latchMask) {
  if (!m_isConnected) {
    std::cerr << "Device " << m_name << " not connected" << std::endl;
    return false;
  }

  // Convert to unsigned long for the API call
  unsigned long temp_latchMask = static_cast<unsigned long>(latchMask);

  int result = PE::FAS_ClearLatch(m_deviceId, temp_latchMask);
  if (result == FMM_OK) {
    return true;
  }
  else {
    std::cerr << "Failed to clear latch for device " << m_name << ", error code: " << result << std::endl;
    return false;
  }
}

bool EziIODevice::getOutputs(uint32_t& outputs, uint32_t& status) {
  if (!m_isConnected) {
    std::cerr << "Device " << m_name << " not connected" << std::endl;
    return false;
  }

  // Create temporary unsigned long variables
  unsigned long temp_outputs = 0;
  unsigned long temp_status = 0;

  int result = PE::FAS_GetOutput(m_deviceId, &temp_outputs, &temp_status);
  if (result == FMM_OK) {
    // Copy back to the original parameters
    outputs = static_cast<uint32_t>(temp_outputs);
    status = static_cast<uint32_t>(temp_status);

    // Update last known values
    {
      std::lock_guard<std::mutex> lock(m_statusMutex);
      m_lastOutputs = outputs;
      m_lastOutputStatus = status;
    }

    // Check for changes and notify observers if enabled
    if (m_changeDetectionEnabled) {
      checkAndNotifyOutputChanges(outputs);
    }

    return true;
  }
  else {
    std::cerr << "Failed to get outputs from device " << m_name << ", error code: " << result << std::endl;
    return false;
  }
}

bool EziIODevice::getLastOutputStatus(uint32_t& outputs, uint32_t& status) {
  std::lock_guard<std::mutex> lock(m_statusMutex);
  outputs = m_lastOutputs;
  status = m_lastOutputStatus;
  return true;
}

bool EziIODevice::setOutputs(uint32_t setMask, uint32_t clearMask) {
  if (!m_isConnected) {
    std::cerr << "Device " << m_name << " not connected" << std::endl;
    return false;
  }

  // Get current state before change for notification
  uint32_t previousOutputs = 0, previousStatus = 0;
  if (m_changeDetectionEnabled) {
    getLastOutputStatus(previousOutputs, previousStatus);
  }

  // Convert to unsigned long for the API call
  unsigned long temp_setMask = static_cast<unsigned long>(setMask);
  unsigned long temp_clearMask = static_cast<unsigned long>(clearMask);

  std::cout << "Setting outputs for " << m_name << " - Set mask: 0x" << std::hex
    << temp_setMask << ", Clear mask: 0x" << temp_clearMask << std::dec << std::endl;

  int result = PE::FAS_SetOutput(m_deviceId, temp_setMask, temp_clearMask);
  if (result == FMM_OK) {
    // Add a small delay to ensure the operation completes
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Update the output status after setting
    uint32_t outputs = 0, status = 0;
    getOutputs(outputs, status);

    return true;
  }
  else {
    std::cerr << "Failed to set outputs for device " << m_name << ", error code: "
      << result << std::endl;
    return false;
  }
}



// Alternative fix: Ensure the device reads back its state after setting
bool EziIODevice::setOutput(int outputPin, bool state) {
  if (!m_isConnected || outputPin < 0 || outputPin >= m_outputCount) {
    std::cerr << "Invalid output pin or not connected" << std::endl;
    return false;
  }

  uint32_t mask = getOutputPinMask(outputPin);

  std::cout << "Setting output pin " << outputPin << " to " << (state ? "ON" : "OFF")
    << " with mask 0x" << std::hex << mask << std::dec << std::endl;

  bool result;
  if (state) {
    result = setOutputs(mask, 0);
  }
  else {
    result = setOutputs(0, mask);
  }

  // IMPORTANT: Force an immediate read-back to update cached state
  if (result) {
    uint32_t outputs = 0, status = 0;
    getOutputs(outputs, status);  // This will trigger observer notifications
  }

  return result;
}


// New observer pattern helper methods
void EziIODevice::checkAndNotifyInputChanges(uint32_t newInputs) {
  if (m_firstRead) {
    m_previousInputs = newInputs;
    m_firstRead = false;
    return;
  }

  // Check each input pin for changes
  for (int pin = 0; pin < m_inputCount; ++pin) {
    bool previousState = (m_previousInputs & (1U << pin)) != 0;
    bool newState = (newInputs & (1U << pin)) != 0;

    if (previousState != newState) {
      PinChangeEvent event;
      event.deviceName = m_name;
      event.deviceId = m_deviceId;
      event.pinNumber = pin;
      event.isInput = true;
      event.newState = newState;
      event.previousState = previousState;
      event.timestamp = getCurrentTimestamp();

      notifyPinChange(event);
    }
  }

  m_previousInputs = newInputs;
}

void EziIODevice::checkAndNotifyOutputChanges(uint32_t newOutputs) {
  // Don't check on first read to avoid false notifications
  if (m_firstRead) {
    m_previousOutputs = newOutputs;
    return;
  }

  // Check each output pin for changes
  for (int pin = 0; pin < m_outputCount; ++pin) {
    uint32_t mask = (1U << pin);
    bool previousState = (m_previousOutputs & mask) != 0;
    bool newState = (newOutputs & mask) != 0;

    if (previousState != newState) {
      PinChangeEvent event;
      event.deviceName = m_name;
      event.deviceId = m_deviceId;
      event.pinNumber = pin;
      event.isInput = false;
      event.newState = newState;
      event.previousState = previousState;
      event.timestamp = getCurrentTimestamp();

      notifyPinChange(event);
    }
  }

  m_previousOutputs = newOutputs;
}

uint32_t EziIODevice::getCurrentTimestamp() const {
  auto now = std::chrono::steady_clock::now();
  auto duration = now.time_since_epoch();
  return static_cast<uint32_t>(
    std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
    );
}

// EziIOManager Implementation
EziIOManager::EziIOManager()
  : m_initialized(false),
  m_pollingThread(nullptr),
  m_stopPolling(false),
  m_pollingInterval(100) // Default 100ms polling interval
{
}

EziIOManager::~EziIOManager() {
  try {
    shutdown();
  }
  catch (const std::exception& e) {
    std::cerr << "Exception during EziIOManager destruction: " << e.what() << std::endl;
  }
  catch (...) {
    std::cerr << "Unknown exception during EziIOManager destruction" << std::endl;
  }
}

bool EziIOManager::initialize() {
  if (m_initialized) {
    return true;
  }

  // Enable auto-reconnect for better connection stability
  PE::FAS_SetAutoReconnect(1);

  m_initialized = true;
  return true;
}

void EziIOManager::shutdown() {
  if (!m_initialized) {
    return;
  }

  // Stop the polling thread first
  try {
    stopPolling();
  }
  catch (...) {
    std::cerr << "Error stopping polling thread during shutdown" << std::endl;
  }

  // Disconnect all devices
  try {
    disconnectAll();
  }
  catch (...) {
    std::cerr << "Error disconnecting devices during shutdown" << std::endl;
  }

  // Clear collections
  m_devices.clear();
  m_deviceMap.clear();
  m_deviceNameMap.clear();
  m_initialized = false;
}

bool EziIOManager::addDevice(int id, const std::string& name, const std::string& ip,
  int inputCount, int outputCount) {
  // Check if a device with this ID already exists
  if (m_deviceMap.find(id) != m_deviceMap.end()) {
    std::cerr << "Device with ID " << id << " already exists" << std::endl;
    return false;
  }

  // Create a new device
  auto device = std::make_shared<EziIODevice>(id, name, ip, inputCount, outputCount);

  // Add device to collections
  m_devices.push_back(device);
  m_deviceMap[id] = device;
  m_deviceNameMap[name] = device;

  std::cout << "Added device " << name << " (ID: " << id << ") at IP " << ip << std::endl;
  return true;
}

bool EziIOManager::connectAll() {
  bool allConnected = true;
  int connectedCount = 0;  // Add counter

  for (auto& device : m_devices) {
    if (device->connect()) {
      connectedCount++;  // Increment when connected
    }
    else {
      allConnected = false;
    }
  }

  m_connectedDeviceCount = connectedCount;  // Update atomic counter

  std::cout << "Connected " << connectedCount << "/"
    << m_devices.size() << " EziIO devices" << std::endl;

  return allConnected;
}

bool EziIOManager::disconnectAll() {
  bool allDisconnected = true;

  for (auto& device : m_devices) {
    if (!device->disconnect()) {
      allDisconnected = false;
    }
  }

  m_connectedDeviceCount = 0;  // Reset counter

  return allDisconnected;
}

bool EziIOManager::connectDevice(int deviceId) {
  auto device = getDevice(deviceId);
  if (!device) {
    return false;
  }

  bool wasConnected = device->isConnected();
  bool result = device->connect();

  if (result && !wasConnected) {
    m_connectedDeviceCount++;  // Increment when newly connected
  }

  return result;
}

bool EziIOManager::disconnectDevice(int deviceId) {
  auto device = getDevice(deviceId);
  if (!device) {
    return false;
  }

  bool wasConnected = device->isConnected();
  bool result = device->disconnect();

  if (result && wasConnected) {
    m_connectedDeviceCount--;  // Decrement when disconnected
  }

  return result;
}

// Helper to get error message string
std::string EziIOManager::getErrorString(EziIOError error) {
  switch (error) {
  case EziIOError::SUCCESS:
    return "Success";
  case EziIOError::DEVICE_NOT_FOUND:
    return "Device not found";
  case EziIOError::DEVICE_DISCONNECTED:
    return "Device disconnected";
  case EziIOError::OPERATION_FAILED:
    return "Operation failed";
  default:
    return "Unknown error";
  }
}

// Updated validation helper to return EziIOError
EziIOError EziIOManager::validateDevice(int deviceId, EziIODevice** device, const std::string& operation) {
  *device = getDevice(deviceId);

  if (!*device) {
    if (m_enableLogging) {
      std::cerr << "[EziIOManager] " << operation << " failed: Device ID "
        << deviceId << " not found" << std::endl;
    }
    return EziIOError::DEVICE_NOT_FOUND;
  }

  if (!(*device)->isConnected()) {
    if (m_enableLogging) {
      std::cerr << "[EziIOManager] " << operation << " failed: Device '"
        << (*device)->getName() << "' (ID: " << deviceId
        << ") is disconnected" << std::endl;
    }
    return EziIOError::DEVICE_DISCONNECTED;
  }

  return EziIOError::SUCCESS;
}

// Updated I/O functions to return EziIOError
EziIOError EziIOManager::readInputs(int deviceId, uint32_t& inputs, uint32_t& latch) {
  EziIODevice* device = nullptr;
  EziIOError validationResult = validateDevice(deviceId, &device, "readInputs");
  if (validationResult != EziIOError::SUCCESS) {
    return validationResult;
  }

  bool result = device->readInputs(inputs, latch);
  return result ? EziIOError::SUCCESS : EziIOError::OPERATION_FAILED;
}

EziIOError EziIOManager::getLastInputStatus(int deviceId, uint32_t& inputs, uint32_t& latch) {
  EziIODevice* device = getDevice(deviceId);
  if (!device) {
    return EziIOError::DEVICE_NOT_FOUND;
  }
  // Return cached values even if disconnected
  bool result = device->getLastInputStatus(inputs, latch);
  return result ? EziIOError::SUCCESS : EziIOError::OPERATION_FAILED;
}

EziIOError EziIOManager::getOutputs(int deviceId, uint32_t& outputs, uint32_t& status) {
  EziIODevice* device = nullptr;
  EziIOError validationResult = validateDevice(deviceId, &device, "getOutputs");
  if (validationResult != EziIOError::SUCCESS) {
    return validationResult;
  }

  bool result = device->getOutputs(outputs, status);
  return result ? EziIOError::SUCCESS : EziIOError::OPERATION_FAILED;
}

EziIOError EziIOManager::getLastOutputStatus(int deviceId, uint32_t& outputs, uint32_t& status) {
  EziIODevice* device = getDevice(deviceId);
  if (!device) {
    return EziIOError::DEVICE_NOT_FOUND;
  }
  // Return cached values even if disconnected
  bool result = device->getLastOutputStatus(outputs, status);
  return result ? EziIOError::SUCCESS : EziIOError::OPERATION_FAILED;
}

EziIOError EziIOManager::setOutputs(int deviceId, uint32_t setMask, uint32_t clearMask) {
  EziIODevice* device = nullptr;
  EziIOError validationResult = validateDevice(deviceId, &device, "setOutputs");
  if (validationResult != EziIOError::SUCCESS) {
    return validationResult;
  }

  bool result = device->setOutputs(setMask, clearMask);
  return result ? EziIOError::SUCCESS : EziIOError::OPERATION_FAILED;
}

EziIOError EziIOManager::setOutput(int deviceId, int outputPin, bool state) {
  EziIODevice* device = nullptr;
  EziIOError validationResult = validateDevice(deviceId, &device, "setOutput");
  if (validationResult != EziIOError::SUCCESS) {
    return validationResult;
  }

  bool result = device->setOutput(outputPin, state);
  return result ? EziIOError::SUCCESS : EziIOError::OPERATION_FAILED;
}

EziIODevice* EziIOManager::getDevice(int deviceId) {
  auto it = m_deviceMap.find(deviceId);
  if (it == m_deviceMap.end()) {
    return nullptr;
  }

  return it->second.get();
}

EziIODevice* EziIOManager::getDeviceByName(const std::string& name) {
  auto it = m_deviceNameMap.find(name);
  if (it == m_deviceNameMap.end()) {
    return nullptr;
  }

  return it->second.get();
}

// Thread functions for polling
void EziIOManager::startPolling(unsigned int intervalMs) {
  // Don't start if already running
  if (m_pollingThread) {
    return;
  }

  // Set the polling interval
  m_pollingInterval = intervalMs;

  // Reset the stop flag
  m_stopPolling = false;

  // Start the polling thread
  m_pollingThread = new std::thread(&EziIOManager::pollingThreadFunc, this);
}

void EziIOManager::stopPolling() {
  // Signal the thread to stop
  m_stopPolling = true;

  // Wait for the thread to exit
  if (m_pollingThread) {
    if (m_pollingThread->joinable()) {
      m_pollingThread->join();
    }
    delete m_pollingThread;
    m_pollingThread = nullptr;
  }
}

void EziIOManager::setPollingInterval(unsigned int intervalMs) {
  m_pollingInterval = intervalMs;
}

void EziIOManager::pollingThreadFunc() {
  std::cout << "Polling thread started with interval " << m_pollingInterval << "ms" << std::endl;

  while (!m_stopPolling) {
    int connectedCount = 0;

    // Poll status of all connected devices
    for (auto& device : m_devices) {
      if (device->isConnected()) {
        connectedCount++;

        // Poll for inputs and outputs
        uint32_t inputs = 0, latch = 0;
        uint32_t outputs = 0, status = 0;

        // These calls will automatically trigger notifications if changes detected
        device->readInputs(inputs, latch);
        device->getOutputs(outputs, status);
      }
    }

    // Adjust sleep based on connection status
    if (connectedCount == 0) {
      // No devices connected - sleep longer (1 second)
      //std::cout << "No devices connected, sleeping for 1000ms" << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    else {
      // Normal polling interval when devices are connected
      // Could also log: std::cout << connectedCount << " devices connected" << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(m_pollingInterval));
    }
  }

  std::cout << "Polling thread stopped" << std::endl;
}

bool EziIOManager::isPolling() const {
  return m_pollingThread != nullptr && !m_stopPolling;
}

// Observer pattern methods for manager
void EziIOManager::subscribeToDevice(int deviceId, std::shared_ptr<IEziIOObserver> observer) {
  auto device = getDevice(deviceId);
  if (device) {
    device->subscribe(observer);
  }
}

void EziIOManager::subscribeCallbackToDevice(int deviceId, std::function<void(const PinChangeEvent&)> callback) {
  auto device = getDevice(deviceId);
  if (device) {
    device->subscribeCallback(callback);
  }
}

void EziIOManager::subscribeToAllDevices(std::shared_ptr<IEziIOObserver> observer) {
  for (auto& device : m_devices) {
    device->subscribe(observer);
  }
}

void EziIOManager::subscribeCallbackToAllDevices(std::function<void(const PinChangeEvent&)> callback) {
  for (auto& device : m_devices) {
    device->subscribeCallback(callback);
  }
}

void EziIOManager::setChangeDetectionEnabled(bool enabled) {
  for (auto& device : m_devices) {
    device->setChangeDetectionEnabled(enabled);
  }
}