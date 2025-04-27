// pi_controller.cpp
#include "../include/motions/pi_controller.h"
#include "imgui.h"
#include <iostream>
#include <chrono>
#include <sstream>
#include <algorithm>

// Modify the constructor to initialize timestamps
PIController::PIController()
	: m_controllerId(-1),
	m_port(50000),
	m_lastStatusUpdate(std::chrono::steady_clock::now()),
	m_lastPositionUpdate(std::chrono::steady_clock::now()) {

	// Initialize atomic variables correctly
	m_isConnected.store(false);
	m_threadRunning.store(false);
	m_terminateThread.store(false);

	m_logger = Logger::GetInstance();
	m_logger->LogInfo("PIController: Initializing controller");

	// Initialize available axes with correct identifiers for C-887
	m_availableAxes = { "X", "Y", "Z", "U", "V", "W" };  // Hexapod axes

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
	const auto updateInterval = std::chrono::milliseconds(200);  // 5Hz update rate

	// Implement frame skipping for less important updates
	int frameCounter = 0;
	while (!m_terminateThread) {
		// Only update if connected
		if (m_isConnected) {
			frameCounter++;

			// Always update positions (but use batch query instead of individual queries)
			std::map<std::string, double> positions;
			if (GetPositions(positions)) {
				std::lock_guard<std::mutex> lock(m_mutex);
				m_axisPositions = positions;
			}

			// Update less frequently for non-critical status information
			if (frameCounter % 3 == 0) {  // Update servo and motion status at ~1.7Hz instead of 5Hz
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
// Modified Connect function to initialize status structures
bool PIController::Connect(const std::string& ipAddress, int port) {
	// Check if already connected
	if (m_isConnected) {
		m_logger->LogWarning("PIController: Already connected to a controller");
		return true;
	}

	m_logger->LogInfo("PIController: Connecting to controller at " + ipAddress + ":" + std::to_string(port));

	// Store connection parameters
	m_ipAddress = ipAddress;
	m_port = port;

	// Attempt to connect to the controller
	m_controllerId = PI_ConnectTCPIP(m_ipAddress.c_str(), m_port);

	if (m_controllerId < 0) {
		int errorCode = PI_GetInitError();
		std::string errorMsg = "PIController: Failed to connect to controller. Error code: " + std::to_string(errorCode);
		m_logger->LogError(errorMsg);
		return false;
	}

	m_isConnected.store(true);
	m_logger->LogInfo("PIController: Successfully connected to controller (ID: " + std::to_string(m_controllerId) + ")");

	// Initialize the position and status maps
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		// Initialize position map with zeros
		for (const auto& axis : m_availableAxes) {
			m_axisPositions[axis] = 0.0;
			m_axisMoving[axis] = false;
			m_axisServoEnabled[axis] = false;
		}

		// Reset timestamps
		m_lastStatusUpdate = std::chrono::steady_clock::now();
		m_lastPositionUpdate = std::chrono::steady_clock::now();
	}

	// Initialize controller for all axes
	bool initResult = PI_INI(m_controllerId, NULL);

	if (!initResult) {
		// Log error and try to get error code
		int error = 0;
		PI_qERR(m_controllerId, &error);
		m_logger->LogError("PIController: Initialization failed with error code: " + std::to_string(error));

		// Try individual axis initialization instead of all at once
		for (const auto& axis : m_availableAxes) {
			bool axisInitResult = PI_INI(m_controllerId, axis.c_str());
			if (axisInitResult) {
				m_logger->LogInfo("PIController: Successfully initialized axis " + axis);
			}
		}
	}

	// Immediately update cached positions and statuses
	std::map<std::string, double> positions;
	if (GetPositions(positions)) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_axisPositions = positions;
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

// MoveToPosition optimized to use cached data for status
bool PIController::MoveToPosition(const std::string& axis, double position, bool blocking) {
	if (!m_isConnected) {
		m_logger->LogError("PIController: Cannot move axis - not connected");
		return false;
	}

	// Only log at debug level to reduce overhead
	if (m_enableDebug) {
		m_logger->LogInfo("PIController: Moving axis " + axis + " to position " + std::to_string(position));
	}

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

	// Update the cache to reflect we're now moving
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_axisMoving[axis] = true;
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

// IsMoving optimized to use less frequent direct API calls
bool PIController::IsMoving(const std::string& axis) {
	if (!m_isConnected) {
		return false;
	}

	// Check if we have a recent cached value (less than 200ms old)
	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		now - m_lastStatusUpdate).count();

	if (elapsed < m_statusUpdateInterval) {
		// Use cached value if it exists and is recent
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_axisMoving.find(axis);
		if (it != m_axisMoving.end()) {
			return it->second;
		}
	}

	// If no recent cached value, do direct query
	const char* axes = axis.c_str();
	BOOL isMoving[1] = { FALSE };

	bool success = PI_IsMoving(m_controllerId, axes, isMoving);

	if (success) {
		// Update the cache
		std::lock_guard<std::mutex> lock(m_mutex);
		m_axisMoving[axis] = (isMoving[0] == TRUE);
		m_lastStatusUpdate = now;
		return (isMoving[0] == TRUE);
	}

	// In case of query error, return safe default
	return false;
}


// Optimize the position queries by implementing batch query
bool PIController::GetPositions(std::map<std::string, double>& positions) {
	if (!m_isConnected || m_availableAxes.empty()) {
		return false;
	}

	// For C-887, we need space-separated axis names for batch query
	std::string allAxes = "X Y Z U V W";  // Query all six hexapod axes at once

	// Allocate array for positions - 6 axes for hexapod
	double posArray[6] = { 0.0 };

	// Query positions in a single API call
	bool success = PI_qPOS(m_controllerId, allAxes.c_str(), posArray);

	if (success) {
		// Fill the map with results
		positions["X"] = posArray[0];
		positions["Y"] = posArray[1];
		positions["Z"] = posArray[2];
		positions["U"] = posArray[3];
		positions["V"] = posArray[4];
		positions["W"] = posArray[5];

		// Log the positions (occasionally to reduce log spam)
		static int callCount = 0;
		if (++callCount % 100 == 0 && enableDebug) {
			m_logger->LogInfo("PIController: Positions - X:" + std::to_string(posArray[0]) +
				" Y:" + std::to_string(posArray[1]) +
				" Z:" + std::to_string(posArray[2]) +
				" U:" + std::to_string(posArray[3]) +
				" V:" + std::to_string(posArray[4]) +
				" W:" + std::to_string(posArray[5]));
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

// IsServoEnabled optimized to use cached values
bool PIController::IsServoEnabled(const std::string& axis, bool& enabled) {
	if (!m_isConnected) {
		return false;
	}

	// Check if we have a recent cached value (less than 200ms old)
	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		now - m_lastStatusUpdate).count();

	if (elapsed < m_statusUpdateInterval) {
		// Use cached value if it exists and is recent
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_axisServoEnabled.find(axis);
		if (it != m_axisServoEnabled.end()) {
			enabled = it->second;
			return true;
		}
	}

	// If no recent cached value, do direct query
	const char* axes = axis.c_str();
	BOOL states[1] = { FALSE };

	bool success = PI_qSVO(m_controllerId, axes, states);

	if (success) {
		enabled = (states[0] == TRUE);

		// Update the cache
		std::lock_guard<std::mutex> lock(m_mutex);
		m_axisServoEnabled[axis] = enabled;
		m_lastStatusUpdate = now;
		return true;
	}

	// In case of query error, return false
	return false;
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
// Modified WaitForMotionCompletion to use atomic variables correctly
bool PIController::WaitForMotionCompletion(const std::string& axis, double timeoutSeconds) {
	if (!m_isConnected) {
		m_logger->LogError("PIController: Cannot wait for motion completion - not connected");
		return false;
	}

	m_logger->LogInfo("PIController: Waiting for motion completion on axis " + axis);

	// Use system clock for timeout
	auto startTime = std::chrono::steady_clock::now();
	int checkCount = 0;

	while (true) {
		checkCount++;

		// First check if we have recent cached motion status
		bool stillMoving = false;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			auto it = m_axisMoving.find(axis);
			if (it != m_axisMoving.end()) {
				stillMoving = it->second;
			}
		}

		// If cached value says we're not moving OR if we're not sure, double-check directly
		if (!stillMoving) {
			// Double-check with a direct query to be sure
			stillMoving = IsMoving(axis);

			// Update the cache
			std::lock_guard<std::mutex> lock(m_mutex);
			m_axisMoving[axis] = stillMoving;
		}

		if (!stillMoving) {
			if (m_enableDebug) {
				m_logger->LogInfo("PIController: Motion completed on axis " + axis + " after " +
					std::to_string(checkCount) + " checks");
			}
			return true;
		}

		// Check for timeout
		auto currentTime = std::chrono::steady_clock::now();
		auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();

		if (elapsedSeconds > timeoutSeconds) {
			std::string timeoutMsg = "PIController: Timeout waiting for motion completion on axis " + axis;
			m_logger->LogWarning(timeoutMsg);
			return false;
		}

		// Log less frequently to reduce overhead
		if (m_enableDebug && checkCount % 20 == 0) {
			m_logger->LogInfo("PIController: Still waiting for axis " + axis +
				" to complete motion, elapsed time: " + std::to_string(elapsedSeconds) + "s");
		}

		// Sleep to avoid CPU spikes but be responsive
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
// Optimize the individual GetPosition by using cached positions when possible
bool PIController::GetPosition(const std::string& axis, double& position) {
	if (!m_isConnected) {
		return false;
	}

	// First check if we have a recent cached value
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_axisPositions.find(axis);
		if (it != m_axisPositions.end()) {
			position = it->second;
			return true;
		}
	}

	// Fall back to individual query if no cached value exists
	const char* axes = axis.c_str();
	double positions[1] = { 0.0 };

	bool result = PI_qPOS(m_controllerId, axes, positions);

	if (result) {
		position = positions[0];

		// Update the cache with this new value
		std::lock_guard<std::mutex> lock(m_mutex);
		m_axisPositions[axis] = position;
	}

	return result;
}

// Optimize the RenderUI method to use cached values instead of direct queries
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

		// Store axis positions in local variables to avoid locking the mutex multiple times
		std::map<std::string, double> positionsCopy;
		std::map<std::string, bool> movingCopy;
		std::map<std::string, bool> servoEnabledCopy;

		{
			std::lock_guard<std::mutex> lock(m_mutex);
			positionsCopy = m_axisPositions;
			movingCopy = m_axisMoving;
			servoEnabledCopy = m_axisServoEnabled;
		}

		// Create the popup
		if (ImGui::BeginPopupModal("Controller Details Popup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Detailed Controller Panel - %s", m_ipAddress.c_str());
			ImGui::Separator();

			// Display controller info (only do this once to avoid redundant calls)
			static char idn[256] = { 0 };
			static bool idnQueried = false;

			if (!idnQueried && m_isConnected) {
				if (PI_qIDN(m_controllerId, idn, sizeof(idn))) {
					idnQueried = true;
				}
			}

			if (idnQueried) {
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

					// Column 2: Position - use cached values instead of querying
					ImGui::TableNextColumn();
					auto posIt = positionsCopy.find(axis);
					if (posIt != positionsCopy.end()) {
						ImGui::Text("%.3f mm", posIt->second);
					}
					else {
						ImGui::Text("N/A");
					}

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

			// Add a motion status indicator - use cached values
			ImGui::Separator();
			bool anyAxisMoving = false;
			for (const auto& [axis, moving] : movingCopy) {
				if (moving) {
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

		// Use the correct axis names directly
		for (const auto& axisPair : axisLabels) {
			const std::string& axis = axisPair.first;
			const std::string& label = axisPair.second;

			ImGui::PushID(axis.c_str());

			// Get axis position from cached values
			auto posIt = positionsCopy.find(axis);
			double position = (posIt != positionsCopy.end()) ? posIt->second : 0.0;

			// Display axis info with proper label
			ImGui::Text("Axis %s: %.3f mm", label.c_str(), position);

			// Jog controls in a single row
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.3f, 0.6f, 1.0f)); // Deep blue for negative direction
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.4f, 0.7f, 1.0f)); // Slightly lighter blue on hover
			if (ImGui::Button(("-##" + axis).c_str(), buttonSize)) {
				MoveRelative(axis, -m_jogDistance, false);
			}
			ImGui::PopStyleColor(2);
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.5f, 0.4f, 1.0f)); // Teal-like blue for positive direction
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.6f, 0.5f, 1.0f)); // Slightly lighter teal on hover
			if (ImGui::Button(("+##" + axis).c_str(), buttonSize)) {
				MoveRelative(axis, m_jogDistance, false);
			}
			ImGui::PopStyleColor(2);

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

		// Stop all axes button with dark blue theme
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.2f, 0.5f, 1.0f)); // Deep dark blue
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.3f, 0.6f, 1.0f)); // Slightly lighter blue on hover
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.1f, 0.4f, 1.0f)); // Darker blue when active
		if (ImGui::Button("STOP ALL AXES", ImVec2(-1, 40))) { // Full width, 40px height
			StopAllAxes();
		}
		ImGui::PopStyleColor(3);
	}

	ImGui::End();
}