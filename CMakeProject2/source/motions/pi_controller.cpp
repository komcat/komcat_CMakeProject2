// pi_controller.cpp
#include "../include/motions/pi_controller.h"
#include "imgui.h"
#include <iostream>
#include <chrono>
#include <sstream>
#include <algorithm>

// Constructor - initialize with correct axis identifiers
PIController::PIController()
	: m_controllerId(-1),
	m_port(50000) {

	// Initialize atomic variables correctly
	m_isConnected.store(false);
	m_threadRunning.store(false);
	m_terminateThread.store(false);

	m_logger = Logger::GetInstance();
	m_logger->LogInfo("PIController: Initializing controller");

	// Initialize available axes with correct identifiers for C-887
	m_availableAxes = { "X", "Y", "Z", "U", "V", "W" };  // Changed from numeric identifiers

	// Start communication thread
	StartCommunicationThread();
}


PIController::~PIController() {
	m_logger->LogInfo("PIController: Shutting down controller");

	// Stop communication thread
	StopCommunicationThread();

	// Disconnect if still connected
	if (m_isConnected) {
		Disconnect();
	}
}

void PIController::StartCommunicationThread() {
	if (!m_threadRunning) {
		m_threadRunning.store(true);
		m_terminateThread.store(false);
		m_communicationThread = std::thread(&PIController::CommunicationThreadFunc, this);
		m_logger->LogInfo("PIController: Communication thread started");
	}
}

void PIController::StopCommunicationThread() {
	if (m_threadRunning) {
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_terminateThread.store(true);
		}
		m_condVar.notify_all();

		if (m_communicationThread.joinable()) {
			m_communicationThread.join();
		}

		m_threadRunning.store(false);
		m_logger->LogInfo("PIController: Communication thread stopped");
	}
}

void PIController::CommunicationThreadFunc() {
	// Reduce update rate from 10Hz to 5Hz
	const auto updateInterval = std::chrono::milliseconds(200);  // Changed from 100ms to 200ms

	// Implement frame skipping for less important updates
	int frameCounter = 0;
	while (!m_terminateThread) {
		// Only update if connected
		if (m_isConnected) {
			frameCounter++;
			// Always update positions
			std::map<std::string, double> positions;
			if (GetPositions(positions)) {
				std::lock_guard<std::mutex> lock(m_mutex);
				m_axisPositions = positions;
			}
			if (frameCounter % 3 == 0) {

				// Update axis motion status
				for (const auto& axis : m_availableAxes) {
					bool moving = IsMoving(axis);
					std::lock_guard<std::mutex> lock(m_mutex);
					m_axisMoving[axis] = moving;
				}

				// Update servo status
				for (const auto& axis : m_availableAxes) {
					bool enabled;
					if (IsServoEnabled(axis, enabled)) {
						std::lock_guard<std::mutex> lock(m_mutex);
						m_axisServoEnabled[axis] = enabled;
					}
				}
			}
		}

		// Wait for next update or termination
		std::unique_lock<std::mutex> lock(m_mutex);
		m_condVar.wait_for(lock, updateInterval, [this]() { return m_terminateThread.load(); });
	}
}

// Also enhance the Connect function to check connection details
bool PIController::Connect(const std::string& ipAddress, int port) {
	// Check if already connected
	if (m_isConnected) {
		m_logger->LogWarning("PIController: Already connected to a controller");
		return true;
	}

	std::cout << "PIController: Attempting connection to " << ipAddress << ":" << port << std::endl;
	m_logger->LogInfo("PIController: Connecting to controller at " + ipAddress + ":" + std::to_string(port));

	// Store connection parameters
	m_ipAddress = ipAddress;
	m_port = port;

	// Attempt to connect to the controller
	std::cout << "PIController: Calling PI_ConnectTCPIP..." << std::endl;
	m_controllerId = PI_ConnectTCPIP(m_ipAddress.c_str(), m_port);
	std::cout << "PIController: Connect result: ID = " << m_controllerId << std::endl;

	if (m_controllerId < 0) {
		int errorCode = PI_GetInitError();
		std::string errorMsg = "PIController: Failed to connect to controller. Error code: " + std::to_string(errorCode);
		m_logger->LogError(errorMsg);
		std::cout << errorMsg << std::endl;
		return false;
	}

	m_isConnected.store(true);
	std::cout << "PIController: Successfully connected with ID " << m_controllerId << std::endl;
	m_logger->LogInfo("PIController: Successfully connected to controller (ID: " + std::to_string(m_controllerId) + ")");

	// Get controller identification
	char idn[256] = { 0 };
	if (PI_qIDN(m_controllerId, idn, sizeof(idn))) {
		std::string idnStr = "PIController: Controller identification: " + std::string(idn);
		m_logger->LogInfo(idnStr);
		std::cout << idnStr << std::endl;
	}
	else {
		std::cout << "PIController: Failed to get controller identification" << std::endl;
	}

	// Check available axes
	std::cout << "PIController: Available axes: ";
	for (const auto& axis : m_availableAxes) {
		std::cout << axis << " ";
	}
	std::cout << std::endl;

	// Initialize controller for all axes
	std::cout << "PIController: Initializing all axes..." << std::endl;
	bool initResult = PI_INI(m_controllerId, NULL);
	std::cout << "PIController: Initialization result: " << (initResult ? "success" : "failed") << std::endl;
	if (!initResult) {
		// Log error and try to get error code
		int error = 0;
		PI_qERR(m_controllerId, &error);
		m_logger->LogError("PIController: Initialization failed with error code: " + std::to_string(error));

		// You might want to try individual axis initialization instead of all at once
		// Or implement a retry mechanism
		for (const auto& axis : m_availableAxes) {
			bool axisInitResult = PI_INI(m_controllerId, axis.c_str());
			std::cout << "Initializing axis " << axis << ": " << (axisInitResult ? "success" : "failed") << std::endl;
		}

	}
	// Check if any axis is in error state after initialization
	for (const auto& axis : m_availableAxes) {
		bool moving = IsMoving(axis);
		bool enabled = false;
		IsServoEnabled(axis, enabled);
		std::cout << "PIController: Axis " << axis << " - Moving: " << (moving ? "yes" : "no")
			<< ", Servo: " << (enabled ? "enabled" : "disabled") << std::endl;
	}

	return true;
}


void PIController::Disconnect() {
	if (!m_isConnected) {
		return;
	}

	m_logger->LogInfo("PIController: Disconnecting from controller");

	// Ensure all axes are stopped before disconnecting
	StopAllAxes();

	// Close connection
	PI_CloseConnection(m_controllerId);

	m_isConnected.store(false);
	m_controllerId = -1;

	m_logger->LogInfo("PIController: Disconnected from controller");
}

bool PIController::MoveToPosition(const std::string& axis, double position, bool blocking) {
	if (!m_isConnected) {
		m_logger->LogError("PIController: Cannot move axis - not connected");
		return false;
	}

	m_logger->LogInfo("PIController: Moving axis " + axis + " to position " + std::to_string(position));

	// Convert single-axis string to char array for PI GCS2 API
	const char* axes = axis.c_str();
	double positions[1] = { position };

	// Command the move
	if (!PI_MOV(m_controllerId, axes, positions)) {
		int error = 0;
		PI_qERR(m_controllerId, &error);
		m_logger->LogError("PIController: Failed to move axis. Error code: " + std::to_string(error));
		return false;
	}

	// If blocking mode, wait for motion to complete
	if (blocking) {
		return WaitForMotionCompletion(axis);
	}

	return true;
}


// Add these improved logging sections to the MoveRelative function in pi_controller.cpp

// Update MoveRelative function to use correct identifiers
bool PIController::MoveRelative(const std::string& axis, double distance, bool blocking) {
	if (!m_isConnected) {
		m_logger->LogError("PIController: Cannot move axis - not connected");
		return false;
	}

	m_logger->LogInfo("PIController: START Moving axis " + axis + " relative distance " + std::to_string(distance));
	std::cout << "PIController: START Moving axis " + axis + " relative distance " + std::to_string(distance) << std::endl;

	// Log controller ID and connection status
	std::cout << "PIController: Controller ID = " << m_controllerId << ", IsConnected = " << (m_isConnected ? "true" : "false") << std::endl;

	// Use the correct axis identifier directly
	const char* axes = axis.c_str();  // No conversion needed if already using "X", "Y", etc.
	double distances[1] = { distance };

	// Log pre-move position
	double currentPos = 0.0;
	if (GetPosition(axis, currentPos)) {
		std::cout << "PIController: Pre-move position of axis " << axis << " = " << currentPos << std::endl;
	}
	else {
		std::cout << "PIController: Failed to get pre-move position of axis " << axis << std::endl;
	}

	// Command the relative move
	std::cout << "PIController: Sending MVR command with axis=" << axis << ", distance=" << distance << std::endl;
	bool moveResult = PI_MVR(m_controllerId, axes, distances);

	if (!moveResult) {
		int error = 0;
		PI_qERR(m_controllerId, &error);
		std::string errorMsg = "PIController: Failed to move axis relatively. Error code: " + std::to_string(error);
		m_logger->LogError(errorMsg);
		std::cout << errorMsg << std::endl;

		// Add detailed error information
		char errorText[256] = { 0 };
		if (PI_TranslateError(error, errorText, sizeof(errorText))) {
			std::cout << "PIController: Error translation: " << errorText << std::endl;
		}

		return false;
	}

	std::cout << "PIController: MVR command sent successfully" << std::endl;

	// If blocking mode, wait for motion to complete
	if (blocking) {
		std::cout << "PIController: Waiting for motion to complete..." << std::endl;
		bool waitResult = WaitForMotionCompletion(axis);
		std::cout << "PIController: Motion completion wait result: " << (waitResult ? "success" : "failed") << std::endl;
		return waitResult;
	}

	// Get post-move position
	if (GetPosition(axis, currentPos)) {
		std::cout << "PIController: Post-move position of axis " << axis << " = " << currentPos << std::endl;
	}

	m_logger->LogInfo("PIController: FINISHED Moving axis " + axis + " relative distance " + std::to_string(distance));
	std::cout << "PIController: FINISHED Moving axis " + axis + " relative distance " + std::to_string(distance) << std::endl;

	return true;
}

bool PIController::GetPosition(const std::string& axis, double& position) {
	if (!m_isConnected) {
		return false;
	}

	// For C-887, use the axis identifier directly (X, Y, Z, U, V, W)
	const char* axes = axis.c_str();
	double positions[1] = { 0.0 };

	bool result = PI_qPOS(m_controllerId, axes, positions);

	if (result) {
		position = positions[0];
		// Only log occasionally to avoid console flooding
		static int callCount = 0;
		if (++callCount % 100 == 0) {
			//std::cout << "PIController: Position of axis " << axis << " = " << position << std::endl;
		}
	}
	else {
		// Log error if we failed to get the position
		int error = 0;
		PI_qERR(m_controllerId, &error);
		if (error) {
			std::cout << "PIController: Error getting position for axis " << axis << ": " << error << std::endl;
		}
	}

	return result;
}

bool PIController::HomeAxis(const std::string& axis) {
	if (!m_isConnected) {
		m_logger->LogError("PIController: Cannot home axis - not connected");
		return false;
	}

	m_logger->LogInfo("PIController: Homing axis " + axis);

	// Convert single-axis string to char array for PI GCS2 API
	const char* axes = axis.c_str();

	// Command the homing operation
	if (!PI_FRF(m_controllerId, axes)) {
		int error = 0;
		PI_qERR(m_controllerId, &error);
		m_logger->LogError("PIController: Failed to home axis. Error code: " + std::to_string(error));
		return false;
	}

	// Wait for homing to complete
	return WaitForMotionCompletion(axis);
}

bool PIController::StopAxis(const std::string& axis) {
	if (!m_isConnected) {
		m_logger->LogError("PIController: Cannot stop axis - not connected");
		return false;
	}

	m_logger->LogInfo("PIController: Stopping axis " + axis);

	// Convert single-axis string to char array for PI GCS2 API
	const char* axes = axis.c_str();

	// Command the stop
	if (!PI_HLT(m_controllerId, axes)) {
		int error = 0;
		PI_qERR(m_controllerId, &error);
		m_logger->LogError("PIController: Failed to stop axis. Error code: " + std::to_string(error));
		return false;
	}

	return true;
}

bool PIController::StopAllAxes() {
	if (!m_isConnected) {
		m_logger->LogError("PIController: Cannot stop all axes - not connected");
		return false;
	}

	m_logger->LogInfo("PIController: Stopping all axes");

	// Command the stop for all axes
	if (!PI_STP(m_controllerId)) {
		int error = 0;
		PI_qERR(m_controllerId, &error);
		m_logger->LogError("PIController: Failed to stop all axes. Error code: " + std::to_string(error));
		return false;
	}

	return true;
}

bool PIController::IsMoving(const std::string& axis) {
	if (!m_isConnected) {
		return false;
	}

	const char* axes = axis.c_str();
	BOOL isMoving[1] = { FALSE };

	if (!PI_IsMoving(m_controllerId, axes, isMoving)) {
		// Error checking omitted for brevity in status check
		return false;
	}

	return (isMoving[0] == TRUE);
}


bool PIController::GetPositions(std::map<std::string, double>& positions) {
	if (!m_isConnected || m_availableAxes.empty()) {
		return false;
	}

	// For C-887, we need space-separated axis names
	std::string allAxes = "X Y Z U V W";

	// Allocate array for positions - 6 axes for hexapod
	double posArray[6] = { 0.0 };

	// Query positions
	bool success = PI_qPOS(m_controllerId, allAxes.c_str(), posArray);

	if (success) {
		// Fill the map with results
		positions["X"] = posArray[0];
		positions["Y"] = posArray[1];
		positions["Z"] = posArray[2];
		positions["U"] = posArray[3];
		positions["V"] = posArray[4];
		positions["W"] = posArray[5];

		// Log the positions (occasionally)
		static int callCount = 0;
		if (++callCount % 100 == 0) {
			std::cout << "PIController: Positions - X:" << posArray[0] << " Y:" << posArray[1]
				<< " Z:" << posArray[2] << " U:" << posArray[3] << " V:" << posArray[4]
				<< " W:" << posArray[5] << std::endl;
		}
	}

	return success;
}


bool PIController::EnableServo(const std::string& axis, bool enable) {
	if (!m_isConnected) {
		m_logger->LogError("PIController: Cannot change servo state - not connected");
		return false;
	}

	m_logger->LogInfo("PIController: Setting servo state for axis " + axis + " to " +
		(enable ? "enabled" : "disabled"));

	const char* axes = axis.c_str();
	BOOL states[1] = { enable ? TRUE : FALSE };

	if (!PI_SVO(m_controllerId, axes, states)) {
		int error = 0;
		PI_qERR(m_controllerId, &error);
		m_logger->LogError("PIController: Failed to set servo state. Error code: " + std::to_string(error));
		return false;
	}

	return true;
}

bool PIController::IsServoEnabled(const std::string& axis, bool& enabled) {
	if (!m_isConnected) {
		return false;
	}

	const char* axes = axis.c_str();
	BOOL states[1] = { FALSE };

	if (!PI_qSVO(m_controllerId, axes, states)) {
		// Error checking omitted for brevity in status check
		return false;
	}

	enabled = (states[0] == TRUE);
	return true;
}

bool PIController::SetVelocity(const std::string& axis, double velocity) {
	if (!m_isConnected) {
		m_logger->LogError("PIController: Cannot set velocity - not connected");
		return false;
	}

	m_logger->LogInfo("PIController: Setting velocity for axis " + axis + " to " + std::to_string(velocity));

	const char* axes = axis.c_str();
	double velocities[1] = { velocity };

	if (!PI_VEL(m_controllerId, axes, velocities)) {
		int error = 0;
		PI_qERR(m_controllerId, &error);
		m_logger->LogError("PIController: Failed to set velocity. Error code: " + std::to_string(error));
		return false;
	}

	return true;
}

bool PIController::GetVelocity(const std::string& axis, double& velocity) {
	if (!m_isConnected) {
		return false;
	}

	const char* axes = axis.c_str();
	double velocities[1] = { 0.0 };

	if (!PI_qVEL(m_controllerId, axes, velocities)) {
		// Error checking omitted for brevity in status check
		return false;
	}

	velocity = velocities[0];
	return true;
}

// Add logging to WaitForMotionCompletion to track any issues there
bool PIController::WaitForMotionCompletion(const std::string& axis, double timeoutSeconds) {
	if (!m_isConnected) {
		std::cout << "PIController: Cannot wait for motion completion - not connected" << std::endl;
		return false;
	}

	std::cout << "PIController: Waiting for motion completion on axis " << axis << " with timeout " << timeoutSeconds << "s" << std::endl;
	m_logger->LogInfo("PIController: Waiting for motion completion on axis " + axis);

	// Use system clock for timeout
	auto startTime = std::chrono::steady_clock::now();
	int checkCount = 0;

	while (true) {
		checkCount++;
		bool stillMoving = IsMoving(axis);

		if (!stillMoving) {
			std::cout << "PIController: Motion completed on axis " << axis << " after " << checkCount << " checks" << std::endl;
			m_logger->LogInfo("PIController: Motion completed on axis " + axis);
			return true;
		}

		// Check for timeout
		auto currentTime = std::chrono::steady_clock::now();
		auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();

		if (elapsedSeconds > timeoutSeconds) {
			std::string timeoutMsg = "PIController: Timeout waiting for motion completion on axis " + axis;
			std::cout << timeoutMsg << std::endl;
			m_logger->LogWarning(timeoutMsg);
			return false;
		}

		if (checkCount % 20 == 0) {  // Log every ~1 second (20 * 50ms)
			std::cout << "PIController: Still waiting for axis " << axis << " to complete motion, elapsed time: "
				<< elapsedSeconds << "s" << std::endl;
		}

		// Sleep to avoid CPU spikes
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
}


bool PIController::ConfigureFromDevice(const MotionDevice& device) {
	if (m_isConnected) {
		m_logger->LogWarning("PIController: Cannot configure from device while connected");
		return false;
	}

	m_logger->LogInfo("PIController: Configuring from device: " + device.Name);

	// Store the IP address and port from the device configuration
	m_ipAddress = device.IpAddress;
	m_port = device.Port;

	// Define available axes based on device configuration
	// For a 6-axis hexapod, typical axes are 1-6
	m_availableAxes.clear();
	for (int i = 1; i <= 6; i++) {
		m_availableAxes.push_back(std::to_string(i));
	}

	return true;
}

bool PIController::MoveToNamedPosition(const std::string& deviceName, const std::string& positionName) {
	// This is a placeholder implementation - you'll need to flesh this out
	// based on how you want to handle named positions
	m_logger->LogInfo("PIController: Moving to named position " + positionName + " for device " + deviceName);

	// In a real implementation, you would:
	// 1. Look up the position coordinates for the named position
	// 2. Move each axis to the specified position

	return true;
}

// Update UI rendering to match the new axis identifiers
void PIController::RenderUI() {
	if (!m_showWindow) {
		return;
	}

	// Use the unique window title
	if (!ImGui::Begin(m_windowTitle.c_str(), &m_showWindow, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::End();
		return;
	}

	// Connection status and controls
	ImGui::Text("Connection Status: %s", m_isConnected ? "Connected" : "Disconnected");

	if (!m_isConnected) {
		// Show connection controls
		static char ipBuffer[64] = "192.168.0.10"; // Default IP
		ImGui::InputText("IP Address", ipBuffer, sizeof(ipBuffer));
		static int port = 50000; // Default port
		ImGui::InputInt("Port", &port);

		if (ImGui::Button("Connect")) {
			Connect(ipBuffer, port);
		}
	}
	else {
		// Show disconnection control
		if (ImGui::Button("Disconnect")) {
			Disconnect();
		}

		ImGui::Separator();

		// Motion controls
		ImGui::Text("Motion Controls");

		// Jog distance control
		float jogDistanceFloat = static_cast<float>(m_jogDistance);
		if (ImGui::SliderFloat("Jog Distance (mm)", &jogDistanceFloat, 0.01f, 10.0f, "%.3f")) {
			m_jogDistance = static_cast<double>(jogDistanceFloat);
		}

		// Add a button to open the detailed panel
		if (ImGui::Button("Open Detailed Panel")) {
			ImGui::OpenPopup("Controller Details Popup");
		}

		// Create the popup
		if (ImGui::BeginPopupModal("Controller Details Popup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Detailed Controller Panel - %s", m_ipAddress.c_str());
			ImGui::Separator();

			// Display controller info
			char idn[256] = { 0 };
			if (PI_qIDN(m_controllerId, idn, sizeof(idn))) {
				ImGui::Text("Controller Identification: %s", idn);
			}

			ImGui::Separator();
			ImGui::Text("Axis Status and Controls");

			// Use the correct axis names directly
			std::vector<std::pair<std::string, std::string>> axisLabels = {
					{"X", "X"},
					{"Y", "Y"},
					{"Z", "Z"},
					{"U", "U (Roll)"},
					{"V", "V (Pitch)"},
					{"W", "W (Yaw)"}
			};

			// Display a table for better organization
			if (ImGui::BeginTable("AxisControlTable", 5, ImGuiTableFlags_Borders)) {
				ImGui::TableSetupColumn("Axis");
				ImGui::TableSetupColumn("Position");
				ImGui::TableSetupColumn("Jog");
				ImGui::TableSetupColumn("Home");
				ImGui::TableSetupColumn("Stop");
				ImGui::TableHeadersRow();

				for (const auto& axisPair : axisLabels) {
					const std::string& axis = axisPair.first;
					const std::string& label = axisPair.second;

					ImGui::PushID(axis.c_str());
					ImGui::TableNextRow();

					// Column 1: Axis name
					ImGui::TableNextColumn();
					ImGui::Text("%s", label.c_str());

					// Column 2: Position
					ImGui::TableNextColumn();
					double position = 0.0;
					GetPosition(axis, position);
					ImGui::Text("%.3f mm", position);

					// Column 3: Jog controls
					ImGui::TableNextColumn();
					ImVec2 buttonSize(30, 25);
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
					if (ImGui::Button(("-##" + axis).c_str(), buttonSize)) {
						MoveRelative(axis, -m_jogDistance, false);
					}
					ImGui::PopStyleColor();

					ImGui::SameLine();

					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
					if (ImGui::Button(("+##" + axis).c_str(), buttonSize)) {
						MoveRelative(axis, m_jogDistance, false);
					}
					ImGui::PopStyleColor();

					// Column 4: Home button
					ImGui::TableNextColumn();
					if (ImGui::Button(("Home##" + axis).c_str(), ImVec2(60, 25))) {
						HomeAxis(axis);
					}

					// Column 5: Stop button
					ImGui::TableNextColumn();
					if (ImGui::Button(("Stop##" + axis).c_str(), ImVec2(60, 25))) {
						StopAxis(axis);
					}

					ImGui::PopID();
				}
				ImGui::EndTable();
			}

			// Add a motion status indicator
			ImGui::Separator();
			bool anyAxisMoving = false;
			for (const auto& axisPair : axisLabels) {
				if (IsMoving(axisPair.first)) {
					anyAxisMoving = true;
					break;
				}
			}

			ImGui::Text("Motion Status: %s", anyAxisMoving ? "Moving" : "Idle");

			ImGui::Separator();

			// Stop all axes button with larger size and warning color
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.1f, 0.1f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
			if (ImGui::Button("STOP ALL AXES", ImVec2(-1, 40))) {
				StopAllAxes();
			}
			ImGui::PopStyleColor(3);

			// Close button
			ImGui::Separator();
			if (ImGui::Button("Close", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		// Display simplified axis controls in the main window
		ImGui::Text("Quick Controls");

		// Axis status and controls with improved layout
		ImVec2 buttonSize(30, 25); // Standard size for all jog buttons

		// Use the correct axis names directly
		std::vector<std::pair<std::string, std::string>> axisLabels = {
				{"X", "X"},
				{"Y", "Y"},
				{"Z", "Z"},
				{"U", "U (Roll)"},
				{"V", "V (Pitch)"},
				{"W", "W (Yaw)"}
		};

		for (const auto& axisPair : axisLabels) {
			const std::string& axis = axisPair.first;
			const std::string& label = axisPair.second;

			ImGui::PushID(axis.c_str());

			// Get axis position
			double position = 0.0;
			GetPosition(axis, position);

			// Display axis info with proper label
			ImGui::Text("Axis %s: %.3f mm", label.c_str(), position);

			// Jog controls in a single row
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); // Red for negative
			if (ImGui::Button(("-##" + axis).c_str(), buttonSize)) {
				MoveRelative(axis, -m_jogDistance, false);
			}
			ImGui::PopStyleColor();

			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f)); // Green for positive
			if (ImGui::Button(("+##" + axis).c_str(), buttonSize)) {
				MoveRelative(axis, m_jogDistance, false);
			}
			ImGui::PopStyleColor();

			ImGui::SameLine();

			if (ImGui::Button(("Home##" + axis).c_str(), ImVec2(60, 25))) {
				HomeAxis(axis);
			}

			ImGui::SameLine();

			if (ImGui::Button(("Stop##" + axis).c_str(), ImVec2(60, 25))) {
				StopAxis(axis);
			}

			ImGui::PopID();
		}

		ImGui::Separator();

		// Stop all axes button with larger size and warning color
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.1f, 0.1f, 1.0f)); // Bright red
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
		if (ImGui::Button("STOP ALL AXES", ImVec2(-1, 40))) { // Full width, 40px height
			StopAllAxes();
		}
		ImGui::PopStyleColor(3);
	}

	ImGui::End();
}