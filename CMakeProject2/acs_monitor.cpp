// acs_monitor.cpp
#include "acs_monitor.h"
#include "imgui.h"
#include <cstring>

ACSMonitor::ACSMonitor()
    : hComm(ACSC_INVALID), isConnected(false), connectionAttempted(false),
      connectionSuccessful(false), motorEnabledX(false),
      motorEnabledY(false), motorEnabledZ(false),
      xPos(0.0), yPos(0.0), zPos(0.0),
      updateInterval(0.1), lastUpdateTime(0.0) {
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

		if (currentTime - lastUpdateTime >= updateInterval) {
			// Update positions every updateInterval seconds
			if (acsc_GetFPosition(hComm, ACSC_AXIS_X, &xPos, nullptr)) {
				lastUpdateTime = currentTime;
			}
			else {
				ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to read X position");
			}
			if (acsc_GetFPosition(hComm, ACSC_AXIS_Y, &yPos, nullptr)) {
				lastUpdateTime = currentTime;
			}
			else {
				ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to read Y position");
			}
			if (acsc_GetFPosition(hComm, ACSC_AXIS_Z, &zPos, nullptr)) {
				lastUpdateTime = currentTime;
			}
			else {
				ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to read Z position");
			}
		}


        // Display position for each axis
        if (acsc_GetFPosition(hComm, ACSC_AXIS_X, &xPos, nullptr)) {
            ImGui::Text("X Position: %.2f", xPos);
        }
        else {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to read X position");
        }

        if (acsc_GetFPosition(hComm, ACSC_AXIS_Y, &yPos, nullptr)) {
            ImGui::Text("Y Position: %.2f", yPos);
        }
        else {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to read Y position");
        }

        if (acsc_GetFPosition(hComm, ACSC_AXIS_Z, &zPos, nullptr)) {
            ImGui::Text("Z Position: %.2f", zPos);
        }
        else {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to read Z position");
        }


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
