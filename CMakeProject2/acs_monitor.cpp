// acs_monitor.cpp
#include "acs_monitor.h"
#include "imgui.h"
#include <cstring>

ACSMonitor::ACSMonitor()
    : hComm(ACSC_INVALID), isConnected(false), connectionAttempted(false),
    connectionSuccessful(false), motorEnabledX(false),
    motorEnabledY(false), motorEnabledZ(false),
    xPos(0.0), yPos(0.0), zPos(0.0),
    updateInterval(0.1), lastUpdateTime(0.0), jogDistance(0.1f) {
    std::strcpy(ipAddress, "192.168.0.50");
}

ACSMonitor::~ACSMonitor() {
    if (isConnected) {
        acsc_CloseComm(hComm);
    }
}

void ACSMonitor::RenderUI() {
    ImGui::Begin("ACS Controller");
    double currentTime = ImGui::GetTime();  // seconds since ImGui started

    ImGui::InputText("IP Address", ipAddress, IM_ARRAYSIZE(ipAddress));

    if (!isConnected && ImGui::Button("Connect")) {
        hComm = acsc_OpenCommEthernet(ipAddress, ACSC_SOCKET_STREAM_PORT);
        connectionAttempted = true;
        if (hComm != ACSC_INVALID) {
            isConnected = true;
            connectionSuccessful = true;
        }
        else {
            connectionSuccessful = false;
        }
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
        if (!motorEnabledX) motorEnabledX = acsc_Enable(hComm, ACSC_AXIS_X, nullptr);
        if (!motorEnabledY) motorEnabledY = acsc_Enable(hComm, ACSC_AXIS_Y, nullptr);
        if (!motorEnabledZ) motorEnabledZ = acsc_Enable(hComm, ACSC_AXIS_Z, nullptr);

        // Add slider for jog distance
        ImGui::Separator();
        ImGui::Text("Jog Controls");
        ImGui::SliderFloat("Jog Distance (mm)", &jogDistance, 0.001f, 10.0f, "%.3f");
        ImGui::Separator();

        if (currentTime - lastUpdateTime >= updateInterval) {
            // Update positions every updateInterval seconds
            acsc_GetFPosition(hComm, ACSC_AXIS_X, &xPos, nullptr);
            acsc_GetFPosition(hComm, ACSC_AXIS_Y, &yPos, nullptr);
            acsc_GetFPosition(hComm, ACSC_AXIS_Z, &zPos, nullptr);
            lastUpdateTime = currentTime;
        }

        // X axis position and jog buttons
        ImGui::Text("X Axis:");
        ImGui::SameLine();
        if (ImGui::Button("<- X")) {
            if (motorEnabledX) {
                //acsc_Jog(hComm, ACSC_AXIS_X, ACSC_NEGATIVE_DIRECTION, 0, nullptr);
                //acsc_WaitMotionEnd(hComm, ACSC_AXIS_X, 1000);
                acsc_ToPoint(hComm, ACSC_AMF_RELATIVE, ACSC_AXIS_X, -jogDistance, nullptr);
            }
        }
        ImGui::SameLine();
        ImGui::Text("%.2f", xPos);
        ImGui::SameLine();
        if (ImGui::Button("X ->")) {
            if (motorEnabledX) {
                //acsc_Jog(hComm, ACSC_AXIS_X, ACSC_POSITIVE_DIRECTION, 0, nullptr);
                //acsc_WaitMotionEnd(hComm, ACSC_AXIS_X, 1000);
                acsc_ToPoint(hComm, ACSC_AMF_RELATIVE, ACSC_AXIS_X, jogDistance, nullptr);
            }
        }

        // Y axis position and jog buttons
        ImGui::Text("Y Axis:");
        ImGui::SameLine();
        if (ImGui::Button("<- Y")) {
            if (motorEnabledY) {
                //acsc_Jog(hComm, ACSC_AXIS_Y, ACSC_NEGATIVE_DIRECTION, 0, nullptr);
                //acsc_WaitMotionEnd(hComm, ACSC_AXIS_Y, 1000);
                acsc_ToPoint(hComm, ACSC_AMF_RELATIVE, ACSC_AXIS_Y, -jogDistance, nullptr);
            }
        }
        ImGui::SameLine();
        ImGui::Text("%.2f", yPos);
        ImGui::SameLine();
        if (ImGui::Button("Y ->")) {
            if (motorEnabledY) {
                //acsc_Jog(hComm, ACSC_AXIS_Y, ACSC_POSITIVE_DIRECTION, 0, nullptr);
                //acsc_WaitMotionEnd(hComm, ACSC_AXIS_Y, 1000);
                acsc_ToPoint(hComm, ACSC_AMF_RELATIVE, ACSC_AXIS_Y, jogDistance, nullptr);
            }
        }

        // Z axis position and jog buttons
        ImGui::Text("Z Axis:");
        ImGui::SameLine();
        if (ImGui::Button("<- Z")) {
            if (motorEnabledZ) {
                //acsc_Jog(hComm, ACSC_AXIS_Z, ACSC_NEGATIVE_DIRECTION, 0, nullptr);
                //acsc_WaitMotionEnd(hComm, ACSC_AXIS_Z, 1000);
                acsc_ToPoint(hComm, ACSC_AMF_RELATIVE, ACSC_AXIS_Z, -jogDistance, nullptr);
            }
        }
        ImGui::SameLine();
        ImGui::Text("%.2f", zPos);
        ImGui::SameLine();
        if (ImGui::Button("Z ->")) {
            if (motorEnabledZ) {
                //acsc_Jog(hComm, ACSC_AXIS_Z, ACSC_POSITIVE_DIRECTION, 0, nullptr);
                //acsc_WaitMotionEnd(hComm, ACSC_AXIS_Z, 1000);
                acsc_ToPoint(hComm, ACSC_AMF_RELATIVE, ACSC_AXIS_Z, jogDistance, nullptr);
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Disconnect")) {
            acsc_CloseComm(hComm);
            hComm = ACSC_INVALID;
            isConnected = false;
            connectionAttempted = false;
            connectionSuccessful = false;
            motorEnabledX = motorEnabledY = motorEnabledZ = false;
        }
    }

    ImGui::End();
}