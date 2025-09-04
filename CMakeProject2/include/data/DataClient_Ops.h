#pragma once

#include "data_subscriber_interface.h"
#include "data_client_manager.h"
#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <mutex>
#include <chrono>
#include <deque>

class DataClient_Ops : public IDataSubscriber {
private:
  DataClientManager* m_dataClientManager;

  // Recording state
  struct RecordingSession {
    bool isActive = false;
    std::ofstream file;
    std::vector<std::string> channels;
    std::chrono::system_clock::time_point startTime;
    std::deque<std::tuple<std::string, float, std::chrono::system_clock::time_point>> buffer;
    static const size_t MAX_BUFFER_SIZE = 10000;
  };
  RecordingSession m_recording;
  mutable std::mutex m_recordingMutex;

public:
  DataClient_Ops(DataClientManager* manager);
  ~DataClient_Ops();

  // IDataSubscriber interface
  void OnDataReceived(const std::string& channelId, float value, const DataPoint& dataPoint) override;
  void OnConnectionChanged(const std::string& channelId, bool connected) override;
  void OnDataError(const std::string& channelId, const std::string& errorMessage) override {}
  std::string GetSubscriberName() const override { return "DataClient_Ops"; }

  // Basic 4 methods

  // 1. Get latest value from channel
  float GetLatestValue(const std::string& channelId) const;

  // 2. Get buffer (all points for channel)
  std::vector<DataPoint> GetBuffer(const std::string& channelId, size_t maxPoints = 100) const;

  // 3. Get buffer since timestamp
  std::vector<DataPoint> GetBufferSince(const std::string& channelId,
    const std::chrono::system_clock::time_point& timestamp) const;

  // 4. Recording to CSV
  bool StartRecording(const std::vector<std::string>& channels, const std::string& filename);
  bool StopRecording();
  bool IsRecording() const { return m_recording.isActive; }
};