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
