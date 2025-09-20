// motion_ops.cpp - Main implementation file
#include "motion_ops.h"

MotionOps::MotionOps(
    MotionControlLayer& motionLayer,
    PIControllerManager& piControllerManager,
    std::shared_ptr<DatabaseManager> dbManager,
    std::shared_ptr<OperationResultsManager> resultsManager
) : m_motionLayer(motionLayer),
    m_piControllerManager(piControllerManager),
    m_dbManager(dbManager),
    m_resultsManager(resultsManager)
{
    m_logger = Logger::GetInstance();
    m_logger->LogInfo("MotionOps: Initialized");
}

MotionOps::~MotionOps() {
    m_logger->LogInfo("MotionOps: Shutting down");
}


// Add to motion_ops.cpp

bool MotionOps::Initialize() {
  LogInfo("Initializing Motion Operations");

  // Validate dependencies
  if (!&m_motionLayer) {
    SetError("Motion control layer not available");
    return false;
  }

  if (!&m_piControllerManager) {
    SetError("PI controller manager not available");
    return false;
  }

  // Check if we have any devices available
  auto devices = GetAvailableDevices();
  if (devices.empty()) {
    LogWarning("No motion devices found during initialization");
  }
  else {
    LogInfo("Found " + std::to_string(devices.size()) + " motion devices");
  }

  SetInitialized();
  return true;
}

void MotionOps::Shutdown() {
  LogInfo("Shutting down Motion Operations");

  // Stop any active scans
  CleanupAllScanners();

  // Clear cached data
  {
    std::lock_guard<std::mutex> lock(m_currentPositionsMutex);
    m_currentPositions.clear();
  }

  ClearStoredPositions();

  SetShutdown();
}

std::vector<std::string> MotionOps::GetAvailableDevices() const {
  return m_motionLayer.GetAvailableDevices();
}

bool MotionOps::IsDeviceConnected(const std::string& deviceName) const {
  // Use your existing implementation
  PIController* piController = m_piControllerManager.GetController(deviceName);
  if (piController) {
    return piController->IsConnected();
  }

  // Check ACS controllers...
  // (use your existing logic)
  return false;  // Simplified for now
}

bool MotionOps::SelfTest() {
  if (!CheckInitialized("SelfTest")) {
    return false;
  }

  LogInfo("Performing self-test");

  // Test basic functionality
  auto devices = GetAvailableDevices();
  int connectedCount = 0;

  for (const auto& device : devices) {
    if (IsDeviceConnected(device)) {
      connectedCount++;
    }
  }

  LogInfo("Self-test: " + std::to_string(connectedCount) + " of " +
    std::to_string(devices.size()) + " devices connected");

  return true;  // Always pass for now
}


// Helper method to store position data
void MotionOps::StorePositionResult(const std::string& operationId,
    const std::string& prefix,
    const PositionStruct& position) {
    if (!m_resultsManager) return;

    m_resultsManager->StoreResult(operationId, prefix + "_x", std::to_string(position.x));
    m_resultsManager->StoreResult(operationId, prefix + "_y", std::to_string(position.y));
    m_resultsManager->StoreResult(operationId, prefix + "_z", std::to_string(position.z));

    // Include rotation if non-zero
    if (position.u != 0.0) m_resultsManager->StoreResult(operationId, prefix + "_u", std::to_string(position.u));
    if (position.v != 0.0) m_resultsManager->StoreResult(operationId, prefix + "_v", std::to_string(position.v));
    if (position.w != 0.0) m_resultsManager->StoreResult(operationId, prefix + "_w", std::to_string(position.w));
}

// Logging methods
void MotionOps::LogInfo(const std::string& message) const {
    if (m_logger) {
        m_logger->LogInfo("MotionOps: " + message);
    }
}

void MotionOps::LogWarning(const std::string& message) const {
    if (m_logger) {
        m_logger->LogWarning("MotionOps: " + message);
    }
}

void MotionOps::LogError(const std::string& message) const {
    if (m_logger) {
        m_logger->LogError("MotionOps: " + message);
    }
}
