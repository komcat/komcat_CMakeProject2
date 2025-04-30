#pragma once

#include "include/tcp_client.h"
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <mutex>
#include <nlohmann/json.hpp>
#include <cmath>
#include <sstream>
#include <iomanip>

// Structure to hold a single data point with timestamp
struct DataPoint {
  float value;
  std::chrono::system_clock::time_point timestamp;

  DataPoint() : value(0.0f), timestamp(std::chrono::system_clock::now()) {}
  DataPoint(float v) : value(v), timestamp(std::chrono::system_clock::now()) {}
};

// Structure to hold a value with an appropriate SI prefix
struct SIValue {
  float value;
  std::string prefix;
  float scaledValue;

  SIValue(float val, const std::string& unit) : value(val), prefix("") {
    scaledValue = value;

    // Apply SI prefix based on magnitude
    if (std::abs(value) > 0) {  // Avoid processing zero values
      if (std::abs(value) < 0.000000001f) {  // < 1 pA/pV etc.
        scaledValue = value * 1e12f;
        prefix = "p";  // pico
      }
      else if (std::abs(value) < 0.000001f) {  // < 1 nA/nV etc.
        scaledValue = value * 1e9f;
        prefix = "n";  // nano
      }
      else if (std::abs(value) < 0.001f) {  // < 1 uA/uV etc.
        scaledValue = value * 1e6f;
        prefix = "u";  // micro (using u as µ might cause display issues)
      }
      else if (std::abs(value) < 1.0f) {  // < 1 A/V etc.
        scaledValue = value * 1e3f;
        prefix = "m";  // milli
      }
      else if (std::abs(value) >= 1000.0f && std::abs(value) < 1000000.0f) {  // >= 1 kA/kV etc.
        scaledValue = value / 1e3f;
        prefix = "k";  // kilo
      }
      else if (std::abs(value) >= 1000000.0f) {  // >= 1 MA/MV etc.
        scaledValue = value / 1e6f;
        prefix = "M";  // mega
      }
      // If between 1 and 1000, keep as is with no prefix
    }
  }

  // Format the value with SI prefix for display
  std::string ToString(int precision = 6) const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(precision) << scaledValue;
    return ss.str();
  }

  // Get the complete string with value, prefix and unit
  std::string GetDisplayString(const std::string& unit, int precision = 6) const {
    return ToString(precision) + " " + prefix + unit;
  }
};

// Function to calculate appropriate chart scaling for a data buffer
struct ChartScale {
  float min;
  float max;

  ChartScale(float currentMin, float currentMax) {
    // Start with the current min/max
    min = currentMin;
    max = currentMax;

    // Calculate the data range
    float range = max - min;

    // Add margins (20% padding)
    float marginPercent = 0.2f;  // 20%
    float margin = range * marginPercent;

    // For maximum, add the margin
    max += margin;

    // For minimum, handle different cases
    if (min > 0) {
      // For positive values, go down by one order of magnitude
      min = min / 10.0f;
    }
    else if (min == 0) {
      // For zero minimum, keep it at zero
      min = 0;
    }
    else {
      // For negative values, add the margin
      min -= margin;
    }

    // Ensure we don't have a too small range
    if (max - min < 0.001f) {
      if (max > 0) {
        max = min + 0.001f;
      }
      else {
        min = max - 0.001f;
      }
    }
  }
};

// Structure to represent a server from config
struct ServerConfig {
  std::string id;
  std::string name;
  std::string host;
  int port;
  std::string unit;
  bool displayUnitSuffix;
  std::string description;
  bool autoConnect;
  bool logData;
};

// Structure to hold client data information
struct DataClientInfo {
  std::shared_ptr<TcpClient> client;
  ServerConfig config;
  bool connected;
  char statusMessage[128];

  // Circular buffer for data points
  std::vector<DataPoint> dataPoints;
  int dataPointCount;
  int dataPointCursor;

  // Latest value
  float latestValue;

  // Mutex for thread-safe operations - using shared_ptr to make the struct copyable
  std::shared_ptr<std::mutex> dataMutex;

  // Constructor with defaults
  DataClientInfo(const ServerConfig& serverConfig) :
    client(std::make_shared<TcpClient>()),
    config(serverConfig),
    connected(false),
    dataPointCount(0),
    dataPointCursor(0),
    latestValue(0.0f),
    dataMutex(std::make_shared<std::mutex>()) {  // Initialize with a shared_ptr
    strcpy(statusMessage, "Not connected");

    // Initialize the data points vector with 100 elements
    dataPoints.resize(100);
  }

  // Copy constructor
  DataClientInfo(const DataClientInfo& other) :
    client(other.client),
    config(other.config),
    connected(other.connected),
    dataPoints(other.dataPoints),
    dataPointCount(other.dataPointCount),
    dataPointCursor(other.dataPointCursor),
    latestValue(other.latestValue),
    dataMutex(other.dataMutex) {
    strcpy(statusMessage, other.statusMessage);
  }

  // Move constructor
  DataClientInfo(DataClientInfo&& other) noexcept :
    client(std::move(other.client)),
    config(std::move(other.config)),
    connected(other.connected),
    dataPoints(std::move(other.dataPoints)),
    dataPointCount(other.dataPointCount),
    dataPointCursor(other.dataPointCursor),
    latestValue(other.latestValue),
    dataMutex(std::move(other.dataMutex)) {
    strcpy(statusMessage, other.statusMessage);
  }

  // Assignment operator
  DataClientInfo& operator=(const DataClientInfo& other) {
    if (this != &other) {
      client = other.client;
      config = other.config;
      connected = other.connected;
      dataPoints = other.dataPoints;
      dataPointCount = other.dataPointCount;
      dataPointCursor = other.dataPointCursor;
      latestValue = other.latestValue;
      dataMutex = other.dataMutex;
      strcpy(statusMessage, other.statusMessage);
    }
    return *this;
  }

  // Move assignment operator
  DataClientInfo& operator=(DataClientInfo&& other) noexcept {
    if (this != &other) {
      client = std::move(other.client);
      config = std::move(other.config);
      connected = other.connected;
      dataPoints = std::move(other.dataPoints);
      dataPointCount = other.dataPointCount;
      dataPointCursor = other.dataPointCursor;
      latestValue = other.latestValue;
      dataMutex = std::move(other.dataMutex);
      strcpy(statusMessage, other.statusMessage);
    }
    return *this;
  }
};

// Class to manage data clients
class DataClientManager {
private:
  std::vector<DataClientInfo> m_clients;
  nlohmann::json m_config;
  std::string m_configFilePath;

  // Settings from config
  std::string m_defaultServerId;
  int m_maxLogEntries;
  std::string m_logDirectory;
  bool m_autoSaveData;
  int m_dataSaveInterval;

public:
  // Constructor takes the path to the config file
  DataClientManager(const std::string& configFilePath);
  ~DataClientManager();

  // Load configuration from file
  bool LoadConfig();

  // Save configuration to file
  bool SaveConfig();

  // Get the number of clients
  size_t GetClientCount() const;

  // Get a specific client info reference
  DataClientInfo& GetClientInfo(int index);

  // Get client info by server ID
  DataClientInfo* GetClientInfoById(const std::string& serverId);

  // Connect a specific client
  bool ConnectClient(int index);
  bool ConnectClientById(const std::string& serverId);

  // Disconnect a specific client
  void DisconnectClient(int index);
  void DisconnectClientById(const std::string& serverId);

  // Connect all clients with autoConnect set to true
  void ConnectAutoClients();

  // Update all clients (call this every frame)
  void UpdateClients();

  // Render the UI for all clients
  void RenderUI();

  // Get raw configuration
  const nlohmann::json& GetConfig() const { return m_config; }
};