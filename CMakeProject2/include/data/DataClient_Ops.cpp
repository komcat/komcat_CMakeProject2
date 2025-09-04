#include "DataClient_Ops.h"
#include "include/logger.h"
#include <iomanip>
#include <sstream>

DataClient_Ops::DataClient_Ops(DataClientManager* manager)
  : m_dataClientManager(manager) {

  if (!m_dataClientManager) {
    throw std::invalid_argument("DataClientManager cannot be null");
  }

  Logger::GetInstance()->LogInfo("DataClient_Ops initialized");
}

DataClient_Ops::~DataClient_Ops() {
  // Stop recording if active
  if (m_recording.isActive) {
    StopRecording();
  }

  // Unsubscribe from all channels
  m_dataClientManager->UnsubscribeCompletely(this);

  Logger::GetInstance()->LogInfo("DataClient_Ops shutdown");
}

// IDataSubscriber callback - called when data is received
void DataClient_Ops::OnDataReceived(const std::string& channelId,
  float value,
  const DataPoint& dataPoint) {
  // If recording is active and this channel is being recorded
  if (m_recording.isActive) {
    auto it = std::find(m_recording.channels.begin(),
      m_recording.channels.end(), channelId);

    if (it != m_recording.channels.end()) {
      std::lock_guard<std::mutex> lock(m_recordingMutex);

      // Add to buffer
      m_recording.buffer.push_back(std::make_tuple(channelId, value, dataPoint.timestamp));

      // Maintain max buffer size
      if (m_recording.buffer.size() > RecordingSession::MAX_BUFFER_SIZE) {
        m_recording.buffer.pop_front();
      }

      // Write to file immediately
      if (m_recording.file.is_open()) {
        // Format timestamp
        auto time_t = std::chrono::system_clock::to_time_t(dataPoint.timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
          dataPoint.timestamp.time_since_epoch()) % 1000;

        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &time_t);
#else
        localtime_r(&time_t, &tm);
#endif

        m_recording.file << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        m_recording.file << "." << std::setfill('0') << std::setw(3) << ms.count();
        m_recording.file << "," << channelId << "," << value << "\n";
        m_recording.file.flush();
      }
    }
  }
}

void DataClient_Ops::OnConnectionChanged(const std::string& channelId, bool connected) {
  Logger::GetInstance()->LogInfo("Channel " + channelId + " connection: " +
    (connected ? "connected" : "disconnected"));
}

// 1. Get latest value from channel
float DataClient_Ops::GetLatestValue(const std::string& channelId) const {
  auto* clientInfo = m_dataClientManager->GetClientInfoById(channelId);
  if (clientInfo && clientInfo->connected) {
    std::lock_guard<std::mutex> lock(*clientInfo->dataMutex);
    return clientInfo->latestValue;
  }
  return 0.0f;
}

// 2. Get buffer (points)
std::vector<DataPoint> DataClient_Ops::GetBuffer(const std::string& channelId, size_t maxPoints) const {
  auto* clientInfo = m_dataClientManager->GetClientInfoById(channelId);
  if (clientInfo) {
    return clientInfo->GetLastDataPoints(maxPoints);
  }
  return std::vector<DataPoint>();
}

// 3. Get buffer since timestamp
std::vector<DataPoint> DataClient_Ops::GetBufferSince(const std::string& channelId,
  const std::chrono::system_clock::time_point& timestamp) const {
  auto* clientInfo = m_dataClientManager->GetClientInfoById(channelId);
  if (clientInfo) {
    return clientInfo->GetDataPointsFromTime(timestamp);
  }
  return std::vector<DataPoint>();
}

// 4. Start recording
bool DataClient_Ops::StartRecording(const std::vector<std::string>& channels, const std::string& filename) {
  std::lock_guard<std::mutex> lock(m_recordingMutex);

  if (m_recording.isActive) {
    Logger::GetInstance()->LogWarning("Recording already in progress");
    return false;
  }

  // Open file
  m_recording.file.open(filename);
  if (!m_recording.file.is_open()) {
    Logger::GetInstance()->LogError("Failed to open recording file: " + filename);
    return false;
  }

  // Write CSV header
  m_recording.file << "timestamp,channel,value\n";

  // Initialize recording session
  m_recording.channels = channels;
  m_recording.startTime = std::chrono::system_clock::now();
  m_recording.buffer.clear();
  m_recording.isActive = true;

  // Subscribe to requested channels
  for (const auto& channel : channels) {
    m_dataClientManager->Subscribe(channel, this);
  }

  Logger::GetInstance()->LogInfo("Started recording " + std::to_string(channels.size()) +
    " channels to: " + filename);
  return true;
}

// Stop recording
bool DataClient_Ops::StopRecording() {
  std::lock_guard<std::mutex> lock(m_recordingMutex);

  if (!m_recording.isActive) {
    Logger::GetInstance()->LogWarning("No recording in progress");
    return false;
  }

  // Close file
  if (m_recording.file.is_open()) {
    m_recording.file.close();
  }

  // Unsubscribe from channels
  for (const auto& channel : m_recording.channels) {
    m_dataClientManager->Unsubscribe(channel, this);
  }

  size_t recordedPoints = m_recording.buffer.size();

  // Clear recording state
  m_recording.isActive = false;
  m_recording.channels.clear();
  m_recording.buffer.clear();

  Logger::GetInstance()->LogInfo("Stopped recording. Total points recorded: " +
    std::to_string(recordedPoints));
  return true;
}