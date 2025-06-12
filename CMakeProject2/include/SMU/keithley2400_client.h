#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <deque>
#include <chrono>
#include <vector>

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

// Data structures for measurements
struct Keithley2400Reading {
  double voltage;
  double current;
  double resistance;
  double power;
  std::chrono::steady_clock::time_point timestamp;
};

struct VoltageSweepResult {
  double setVoltage;
  double measuredVoltage;
  double measuredCurrent;
  std::chrono::steady_clock::time_point timestamp;
};

class Keithley2400Client {
public:
  Keithley2400Client();
  ~Keithley2400Client();

  // Connect to Keithley 2400 server
  bool Connect(const std::string& ip, int port);

  // Disconnect from server
  void Disconnect();

  // Check if connected
  bool IsConnected() const;

  // Basic control methods
  bool ResetInstrument();
  bool SetOutput(bool enable);
  bool GetStatus(std::string& instrumentId, std::string& outputState, std::string& sourceFunction);

  // Source configuration
  bool SetupVoltageSource(double voltage, double compliance = 0.1, const std::string& range = "AUTO");
  bool SetupCurrentSource(double current, double compliance = 10.0, const std::string& range = "AUTO");

  // Raw SCPI commands
  bool SendWriteCommand(const std::string& command);
  bool SendQueryCommand(const std::string& command, std::string& response);

  // Measurement methods
  bool ReadMeasurement(Keithley2400Reading& reading);
  bool VoltageSweep(double start, double stop, int steps, double compliance, double delay,
    std::vector<VoltageSweepResult>& results);

  // Get latest readings
  Keithley2400Reading GetLatestReading() const;
  double GetVoltage() const;
  double GetCurrent() const;
  double GetResistance() const;
  double GetPower() const;

  // UI rendering
  void RenderUI();
  void ToggleWindow();
  bool IsVisible() const;
  const std::string& GetName() const;
  void SetName(const std::string& name);

  // Start/stop background polling
  void StartPolling(int intervalMs = 1000);
  void StopPolling();

  // Get last error message
  const std::string& GetLastError() const;

private:
  // Send JSON command to server
  bool SendJsonCommand(const std::string& type, const std::string& data = "", std::string* response = nullptr);

  // Background polling thread
  void PollingThread();

  // Parse measurement response
  bool ParseMeasurement(const std::string& jsonResponse, Keithley2400Reading& reading);

  // Socket variables
  SOCKET m_socket;
  std::string m_serverIp;
  int m_serverPort;
  std::atomic<bool> m_isConnected;

  // Latest readings
  mutable std::mutex m_dataMutex;
  Keithley2400Reading m_latestReading;
  std::string m_lastError;

  // Polling thread
  std::thread m_pollingThread;
  std::atomic<bool> m_isPolling;
  int m_pollingIntervalMs;

  // UI state
  bool m_showWindow;
  std::string m_name;

  // History for plotting
  std::deque<Keithley2400Reading> m_readingHistory;
  static constexpr size_t MAX_HISTORY_SIZE = 300; // ~5 minutes at 1Hz polling

  // UI control variables
  struct UIState {
    bool outputEnabled = false;
    double voltageSetpoint = 0.0;
    double currentSetpoint = 0.001; // 1mA default
    double compliance = 0.1; // 100mA default
    int sourceMode = 0; // 0 = voltage, 1 = current

    // Sweep parameters
    double sweepStart = 0.0;
    double sweepStop = 5.0;
    int sweepSteps = 11;
    double sweepCompliance = 0.01;
    double sweepDelay = 0.1;

    // Status
    std::string instrumentStatus = "Unknown";
    std::string outputStatus = "OFF";
    std::string sourceFunction = "VOLT";
  } m_uiState;
};