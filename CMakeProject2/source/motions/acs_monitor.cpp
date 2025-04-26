// acs_monitor.cpp
#include "include/motions/acs_monitor.h"
#include "imgui.h"
#include <cstring>
#include <chrono>

ACSMonitor::ACSMonitor()
    : hComm(ACSC_INVALID), isConnected(false), connectionAttempted(false),
    connectionSuccessful(false), motorEnabledX(false),
    motorEnabledY(false), motorEnabledZ(false),
    xPos(0.0), yPos(0.0), zPos(0.0),
    updateInterval(0.1), lastUpdateTime(0.0), jogDistance(0.1f),
    m_threadRunning(false), m_terminateThread(false) {
    std::strcpy(ipAddress, "192.168.0.50");
}

ACSMonitor::~ACSMonitor() {
    StopCommunicationThread();
    if (isConnected) {
        DisconnectFromController();
    }
}

void ACSMonitor::StartCommunicationThread() {
    if (!m_threadRunning) {
        m_terminateThread = false;
        m_threadRunning = true;
        m_communicationThread = std::thread(&ACSMonitor::CommunicationThreadFunc, this);
    }
}

void ACSMonitor::StopCommunicationThread() {
    if (m_threadRunning) {
        m_terminateThread = true;
        m_condVar.notify_one();

        if (m_communicationThread.joinable()) {
            m_communicationThread.join();
        }
        m_threadRunning = false;
    }
}

void ACSMonitor::CommunicationThreadFunc() {
    while (!m_terminateThread) {
        // Process any pending motor commands
        {
            std::lock_guard<std::mutex> lock(m_commandMutex);
            for (auto& cmd : m_commandQueue) {
                if (!cmd.executed) {
                    MoveMotor(cmd.axis, cmd.distance);
                    cmd.executed = true;
                }
            }

            // Remove executed commands
            m_commandQueue.erase(
                std::remove_if(m_commandQueue.begin(), m_commandQueue.end(),
                    [](const MotorCommand& cmd) { return cmd.executed; }),
                m_commandQueue.end());
        }

        // Update motor positions at regular intervals
        if (isConnected) {
            UpdateMotorPositions();
        }

        // Wait for notification or timeout
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condVar.wait_for(lock, std::chrono::milliseconds(100), [this] {
            return m_terminateThread || !m_commandQueue.empty();
        });
    }
}

bool ACSMonitor::ConnectToController(const char* ip) {
    // This runs in the communication thread
    // Create a non-const copy of the IP string
    char ipCopy[64];
    strncpy(ipCopy, ip, sizeof(ipCopy) - 1);
    ipCopy[sizeof(ipCopy) - 1] = '\0'; // Ensure null termination

    hComm = acsc_OpenCommEthernet(ipCopy, ACSC_SOCKET_STREAM_PORT);
    if (hComm != ACSC_INVALID) {
        isConnected = true;

        // Enable motors
        motorEnabledX = acsc_Enable(hComm, ACSC_AXIS_X, nullptr);
        motorEnabledY = acsc_Enable(hComm, ACSC_AXIS_Y, nullptr);
        motorEnabledZ = acsc_Enable(hComm, ACSC_AXIS_Z, nullptr);

        return true;
    }
    return false;
}

void ACSMonitor::DisconnectFromController() {
    // This runs in the communication thread
    if (hComm != ACSC_INVALID) {
        acsc_CloseComm(hComm);
        hComm = ACSC_INVALID;
    }
    isConnected = false;
    motorEnabledX = false;
    motorEnabledY = false;
    motorEnabledZ = false;
}

void ACSMonitor::UpdateMotorPositions() {
    // This runs in the communication thread
    if (isConnected) {
        double newX, newY, newZ;
        bool success = true;

        // Read positions
        if (!acsc_GetFPosition(hComm, ACSC_AXIS_X, &newX, nullptr)) success = false;
        if (!acsc_GetFPosition(hComm, ACSC_AXIS_Y, &newY, nullptr)) success = false;
        if (!acsc_GetFPosition(hComm, ACSC_AXIS_Z, &newZ, nullptr)) success = false;

        if (success) {
            // Update positions with mutex protection
            std::lock_guard<std::mutex> lock(m_mutex);
            xPos = newX;
            yPos = newY;
            zPos = newZ;
        }
    }
}

void ACSMonitor::MoveMotor(int axis, double distance) {
    // This runs in the communication thread
    if (isConnected) {
        // Check if motor is enabled
        bool motorEnabled = false;

        switch (axis) {
        case ACSC_AXIS_X: motorEnabled = motorEnabledX; break;
        case ACSC_AXIS_Y: motorEnabled = motorEnabledY; break;
        case ACSC_AXIS_Z: motorEnabled = motorEnabledZ; break;
        }

        if (motorEnabled) {
            acsc_ToPoint(hComm, ACSC_AMF_RELATIVE, axis, distance, nullptr);
        }
    }
}

void ACSMonitor::RenderUI() {
    ImGui::Begin("ACS Controller");

    ImGui::InputText("IP Address", ipAddress, IM_ARRAYSIZE(ipAddress));

    if (!isConnected && ImGui::Button("Connect")) {
        connectionAttempted = true;

        // Start communication thread if not running
        StartCommunicationThread();

        // Queue connection request
        std::thread([this, ip = std::string(ipAddress)]() {
            connectionSuccessful = ConnectToController(ip.c_str());
        }).detach();
    }

    if (connectionAttempted) {
        if (connectionSuccessful) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "✅ Connected to %s", ipAddress);
        }
        else {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "❌ Failed to connect.");
        }
    }

    if (isConnected) {
        // Add slider for jog distance
        ImGui::Separator();
        ImGui::Text("Jog Controls");
        ImGui::SliderFloat("Jog Distance (mm)", &jogDistance, 0.001f, 10.0f, "%.3f");
        ImGui::Separator();

        // Thread-safe access to position data
        double currentX, currentY, currentZ;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            currentX = xPos;
            currentY = yPos;
            currentZ = zPos;
        }

        // X axis position and jog buttons
        ImGui::Text("X Axis:");
        ImGui::SameLine();
        if (ImGui::Button("<- X")) {
            if (motorEnabledX) {
                // Queue the motor command
                std::lock_guard<std::mutex> lock(m_commandMutex);
                m_commandQueue.push_back({ ACSC_AXIS_X, -jogDistance, false });
                m_condVar.notify_one();
            }
        }
        ImGui::SameLine();
        ImGui::Text("%.2f", currentX);
        ImGui::SameLine();
        if (ImGui::Button("X ->")) {
            if (motorEnabledX) {
                // Queue the motor command
                std::lock_guard<std::mutex> lock(m_commandMutex);
                m_commandQueue.push_back({ ACSC_AXIS_X, jogDistance, false });
                m_condVar.notify_one();
            }
        }

        // Y axis position and jog buttons
        ImGui::Text("Y Axis:");
        ImGui::SameLine();
        if (ImGui::Button("<- Y")) {
            if (motorEnabledY) {
                // Queue the motor command
                std::lock_guard<std::mutex> lock(m_commandMutex);
                m_commandQueue.push_back({ ACSC_AXIS_Y, -jogDistance, false });
                m_condVar.notify_one();
            }
        }
        ImGui::SameLine();
        ImGui::Text("%.2f", currentY);
        ImGui::SameLine();
        if (ImGui::Button("Y ->")) {
            if (motorEnabledY) {
                // Queue the motor command
                std::lock_guard<std::mutex> lock(m_commandMutex);
                m_commandQueue.push_back({ ACSC_AXIS_Y, jogDistance, false });
                m_condVar.notify_one();
            }
        }

        // Z axis position and jog buttons
        ImGui::Text("Z Axis:");
        ImGui::SameLine();
        if (ImGui::Button("<- Z")) {
            if (motorEnabledZ) {
                // Queue the motor command
                std::lock_guard<std::mutex> lock(m_commandMutex);
                m_commandQueue.push_back({ ACSC_AXIS_Z, -jogDistance, false });
                m_condVar.notify_one();
            }
        }
        ImGui::SameLine();
        ImGui::Text("%.2f", currentZ);
        ImGui::SameLine();
        if (ImGui::Button("Z ->")) {
            if (motorEnabledZ) {
                // Queue the motor command
                std::lock_guard<std::mutex> lock(m_commandMutex);
                m_commandQueue.push_back({ ACSC_AXIS_Z, jogDistance, false });
                m_condVar.notify_one();
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Disconnect")) {
            // Queue disconnection
            std::thread([this]() {
                DisconnectFromController();
                connectionAttempted = false;
                connectionSuccessful = false;
            }).detach();

            // We'll stop the thread after disconnection completes
            StopCommunicationThread();
        }
    }

    ImGui::End();
}