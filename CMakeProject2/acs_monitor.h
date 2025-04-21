// acs_monitor.h
#pragma once

#include "ACSC.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector> // Add vector header
#include <algorithm> // For std::remove_if

class ACSMonitor {
public:
    ACSMonitor();
    ~ACSMonitor();

    void RenderUI();

private:
    // Thread control
    void StartCommunicationThread();
    void StopCommunicationThread();
    void CommunicationThreadFunc();

    // Communication functions
    bool ConnectToController(const char* ip); // Creates a non-const copy internally
    void DisconnectFromController();
    void UpdateMotorPositions();
    void MoveMotor(int axis, double distance);

    // Thread-related members
    std::thread m_communicationThread;
    std::mutex m_mutex;
    std::condition_variable m_condVar;
    std::atomic<bool> m_threadRunning;
    std::atomic<bool> m_terminateThread;

    // Command queue structure
    struct MotorCommand {
        int axis;
        double distance;
        bool executed;
    };

    std::vector<MotorCommand> m_commandQueue;
    std::mutex m_commandMutex;

    // Controller state
    HANDLE hComm;
    bool isConnected;
    char ipAddress[64];
    bool connectionAttempted;
    bool connectionSuccessful;
    std::atomic<bool> motorEnabledX, motorEnabledY, motorEnabledZ;

    // Position data (protected by mutex)
    double xPos, yPos, zPos;
    float jogDistance = 0.1f; // Default jog distance in mm

    // Update timing
    double updateInterval = 0.1; // seconds between updates
    double lastUpdateTime = 0.0;
};