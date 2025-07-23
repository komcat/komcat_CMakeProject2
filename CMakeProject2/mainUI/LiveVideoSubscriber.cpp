// LiveVideoSubscriber.cpp
// =======================

#include "LiveVideoSubscriber.h"
#include <iostream>
#include <sstream>

LiveVideoSubscriber::LiveVideoSubscriber(const std::string& cameraId)
  : m_targetCameraId(cameraId) {

  UpdateSubscriberId();
  ResetState();

  std::cout << "[INFO] LiveVideoSubscriber created for camera: " << cameraId
    << " (ID: " << m_subscriberId << ")" << std::endl;
}

LiveVideoSubscriber::~LiveVideoSubscriber() {
  std::cout << "[INFO] LiveVideoSubscriber destroyed for camera: " << m_targetCameraId
    << " (Total frames received: " << m_totalFramesReceived.load() << ")" << std::endl;
}

void LiveVideoSubscriber::OnNewFrame(const CameraFrameData& frameData) {
  if (frameData.cameraId != m_targetCameraId) {
    return; // Not interested in this camera
  }

  // Thread-safe frame storage
  {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    m_latestFrame = frameData; // Deep copy of frame data
  }

  // Update atomic flags and statistics
  m_hasNewFrame.store(true);
  m_totalFramesReceived.fetch_add(1);
  m_lastFrameTimestamp.store(frameData.timestamp);

  // Optional: Debug logging (enable only when needed)
#ifdef DEBUG_FRAME_RECEPTION
  std::cout << "[DEBUG] Frame received for " << frameData.cameraId
    << ": " << frameData.width << "x" << frameData.height
    << " (Frame #" << m_totalFramesReceived.load() << ")" << std::endl;
#endif
}

void LiveVideoSubscriber::OnCameraStatusChanged(const std::string& cameraId, bool connected, bool grabbing) {
  if (cameraId != m_targetCameraId) {
    return; // Not our camera
  }

  // Update status atomically
  bool wasConnected = m_cameraConnected.exchange(connected);
  bool wasGrabbing = m_cameraGrabbing.exchange(grabbing);

  // Log status changes
  if (wasConnected != connected || wasGrabbing != grabbing) {
    std::cout << "[INFO] Camera " << cameraId << " status changed: "
      << "connected=" << (connected ? "Yes" : "No")
      << ", grabbing=" << (grabbing ? "Yes" : "No") << std::endl;
  }

  // Reset frame flag if camera stopped grabbing
  if (wasGrabbing && !grabbing) {
    m_hasNewFrame.store(false);
  }
}

std::string LiveVideoSubscriber::GetSubscriberId() const {
  return m_subscriberId;
}

bool LiveVideoSubscriber::WantsFramesFromCamera(const std::string& cameraId) const {
  return cameraId == m_targetCameraId;
}

void LiveVideoSubscriber::SetTargetCamera(const std::string& cameraId) {
  if (m_targetCameraId == cameraId) {
    return; // No change needed
  }

  std::cout << "[INFO] LiveVideoSubscriber switching from '" << m_targetCameraId
    << "' to '" << cameraId << "'" << std::endl;

  // Update target camera
  m_targetCameraId = cameraId;
  UpdateSubscriberId();

  // Reset state for new camera
  ResetState();
}

std::string LiveVideoSubscriber::GetTargetCamera() const {
  return m_targetCameraId;
}

CameraFrameData LiveVideoSubscriber::GetLatestFrame() const {
  std::lock_guard<std::mutex> lock(m_frameMutex);
  return m_latestFrame; // Return copy
}

uint64_t LiveVideoSubscriber::GetLastFrameTimestamp() const {
  return m_lastFrameTimestamp.load();
}

void LiveVideoSubscriber::ResetState() {
  // Clear frame data
  {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    m_latestFrame = CameraFrameData(); // Reset to empty frame
  }

  // Reset atomic flags
  m_hasNewFrame.store(false);
  m_cameraConnected.store(false);
  m_cameraGrabbing.store(false);

  // Reset statistics
  m_totalFramesReceived.store(0);
  m_lastFrameTimestamp.store(0);
}

void LiveVideoSubscriber::UpdateSubscriberId() {
  std::ostringstream oss;
  oss << "UI_LiveVideo_" << m_targetCameraId << "_" << this; // Add pointer for uniqueness
  m_subscriberId = oss.str();
}