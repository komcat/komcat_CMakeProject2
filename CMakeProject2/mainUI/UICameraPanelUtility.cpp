// UICameraPanelUtility.cpp - Camera Utility and Control Panel Implementation
#include "UICameraPanelUtility.h"
#include "include/camera/CameraManager.h"
#include "include/camera/pylon_camera_test.h"
#include "include/camera/pylon_camera.h"
#include "imgui.h"
#include <iostream>

UICameraPanelUtility::UICameraPanelUtility(CameraManager& cameraManager)
	: m_cameraManager(cameraManager) {

	std::cout << "[INFO] UICameraPanelUtility created" << std::endl;
}

UICameraPanelUtility::~UICameraPanelUtility() {
	ClearCamera();
}

void UICameraPanelUtility::RenderPanel(PylonCameraTest* camera, const std::string& cameraId) {
	// Update camera reference if changed
	if (m_currentCamera != camera || m_currentCameraId != cameraId) {
		SetSelectedCamera(camera, cameraId);
	}

	if (!ValidateCamera()) {
		ImGui::Text("No camera selected");
		ImGui::Spacing();
		ImGui::Text("Select a camera from the left panel");
		ImGui::Text("to view controls here");
		return;
	}

	// Render all control sections
	RenderCameraHeader();
	ImGui::Separator();

	RenderConnectionControls();
	ImGui::Separator();

	RenderCameraStatus();
	ImGui::Separator();

	RenderGrabbingControls();
	ImGui::Separator();

	RenderExposureControls();
	ImGui::Separator();

	RenderImageControls();
	ImGui::Separator();

	RenderAdvancedControls();
	ImGui::Separator();

	RenderDebugControls();
}

void UICameraPanelUtility::SetSelectedCamera(PylonCameraTest* camera, const std::string& cameraId) {
	m_currentCamera = camera;
	m_currentCameraId = cameraId;

	if (camera) {
		std::cout << "[INFO] Utility panel set to camera: " << cameraId << std::endl;
		// Update exposure UI with current camera settings
		UpdateExposureUIFromCamera();
	}
}

void UICameraPanelUtility::ClearCamera() {
	m_currentCamera = nullptr;
	m_currentCameraId = "";

	std::cout << "[INFO] Utility panel camera cleared" << std::endl;
}

void UICameraPanelUtility::RenderCameraHeader() {
	ImGui::SetWindowFontScale(1.2f);
	ImGui::Text("Camera: %s", m_currentCameraId.c_str());
	ImGui::SetWindowFontScale(1.0f);

	if (!ValidateCamera()) {
		ImGui::Text("Device: Not available");
		return;
	}

	auto& pylonCamera = m_currentCamera->GetCamera();

	// Access to exposure manager
	if (ImGui::Button("Open Exposure Manager", ImVec2(200, 30))) {
		m_currentCamera->GetExposureManager().ToggleWindow();
	}

	// Camera calibration controls
	ImGui::Spacing();
	ImGui::Text("Camera Calibration:");

	float pixelToMMX = m_currentCamera->GetPixelToMMFactorX();
	float pixelToMMY = m_currentCamera->GetPixelToMMFactorY();

	if (ImGui::InputFloat("X Factor (mm/pixel)", &pixelToMMX, 0.00001f, 0.0001f, "%.5f")) {
		if (pixelToMMX > 0) {
			m_currentCamera->SetPixelToMMFactors(pixelToMMX, pixelToMMY);
		}
	}

	if (ImGui::InputFloat("Y Factor (mm/pixel)", &pixelToMMY, 0.00001f, 0.0001f, "%.5f")) {
		if (pixelToMMY > 0) {
			m_currentCamera->SetPixelToMMFactors(pixelToMMX, pixelToMMY);
		}
	}

	// Raylib feed control if available
	ImGui::Spacing();
	ImGui::Text("Video Feed Options:");

	bool raylibFeedEnabled = m_currentCamera->IsRaylibFeedEnabled();
	if (ImGui::Checkbox("Send to Raylib Window", &raylibFeedEnabled)) {
		m_currentCamera->SetRaylibFeedEnabled(raylibFeedEnabled);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(Live Video page)");
}

void UICameraPanelUtility::RenderDebugControls() {
	ImGui::Text("Debug & Utility");

	if (!ValidateCamera()) {
		ImGui::Text("Camera not available");
		return;
	}

	auto& pylonCamera = m_currentCamera->GetCamera();

	if (ImGui::Button("Debug Camera Settings", ImVec2(200, 30))) {
		pylonCamera.DebugCameraSettings();
	}

	// Enable debug mode
	ImGui::Checkbox("Enable Debug Mode", &m_currentCamera->enableDebug);

	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Enable verbose debug output to console");
	}

	// Device removal testing (only show if connected)
	if (pylonCamera.IsConnected()) {
		ImGui::Spacing();
		ImGui::Text("Device Status:");

		if (pylonCamera.IsCameraDeviceRemoved()) {
			ImGui::TextColored(ImVec4(1, 0, 0, 1), "Device has been removed!");

			if (ImGui::Button("Try Reconnect", ImVec2(150, 30))) {
				if (pylonCamera.TryReconnect()) {
					std::cout << "Successfully reconnected to camera" << std::endl;
					UpdateExposureUIFromCamera();
				}
				else {
					std::cout << "Failed to reconnect to camera" << std::endl;
				}
			}
		}
		else {
			ImGui::TextColored(ImVec4(0, 1, 0, 1), "Device OK");
		}
	}

	// Memory and performance info
	ImGui::Spacing();
	ImGui::Text("Performance Info:");

	if (m_currentCamera->HasValidTexture()) {
		ImGui::Text("Texture: %dx%d", m_currentCamera->GetImageWidth(), m_currentCamera->GetImageHeight());
		ImGui::Text("Texture ID: %u", m_currentCamera->GetTextureID());
	}
	else {
		ImGui::Text("No active texture");
	}

	ImGui::Text("Frame ready: %s", m_currentCamera->HasNewFrameReady() ? "Yes" : "No");
}

bool UICameraPanelUtility::ValidateCamera() const {
	return (m_currentCamera != nullptr);
}

void UICameraPanelUtility::UpdateExposureUIFromCamera() {
	if (!ValidateCamera()) {
		return;
	}

	auto& pylonCamera = m_currentCamera->GetCamera();

	if (!pylonCamera.IsConnected()) {
		return;
	}

	try {
		auto settings = pylonCamera.GetCurrentExposureSettings();
		m_customExposureTime = static_cast<float>(settings.exposure_time);
		m_customGain = static_cast<float>(settings.gain);
		m_exposureAuto = settings.exposure_auto;
		m_gainAuto = settings.gain_auto;

		std::cout << "[INFO] Updated UI with camera exposure settings" << std::endl;
	}
	catch (const std::exception& e) {
		std::cout << "[ERROR] Failed to read camera exposure settings: " << e.what() << std::endl;
	}
}

void UICameraPanelUtility::ApplyExposureSettingsToCamera() {
	if (!ValidateCamera()) {
		return;
	}

	auto& pylonCamera = m_currentCamera->GetCamera();

	if (!pylonCamera.IsConnected()) {
		std::cout << "[ERROR] Cannot apply settings: Camera not connected" << std::endl;
		return;
	}

	try {
		PylonCamera::ExposureSettings settings;
		settings.exposure_time = m_customExposureTime;
		settings.gain = m_customGain;
		settings.exposure_auto = m_exposureAuto;
		settings.gain_auto = m_gainAuto;

		if (m_cameraManager.ApplyExposureSettings(m_currentCameraId, settings)) {
			std::cout << "[INFO] Applied exposure settings to camera" << std::endl;
		}
		else {
			std::cout << "[ERROR] Failed to apply exposure settings" << std::endl;
		}
	}
	catch (const std::exception& e) {
		std::cout << "[ERROR] Exception applying exposure settings: " << e.what() << std::endl;
	}
}

void UICameraPanelUtility::SafeDisconnectCamera() {
	if (!ValidateCamera()) {
		return;
	}

	std::cout << "[INFO] Safely disconnecting camera: " << m_currentCameraId << std::endl;

	// Disconnect through camera manager (handles cleanup)
	m_cameraManager.DisconnectCamera(m_currentCameraId);

	//if (pylonCamera.IsConnected()) {
	//	std::string deviceInfo = pylonCamera.GetDeviceInfo();
	//	ImGui::Text("Device: %s", deviceInfo.c_str());
	//}
	//else {
	//	ImGui::Text("Device: Not connected");
	//}
}

void UICameraPanelUtility::RenderConnectionControls() {
	ImGui::Text("Connection Controls");

	if (!ValidateCamera()) {
		ImGui::Text("Camera not available");
		return;
	}

	auto& pylonCamera = m_currentCamera->GetCamera();

	if (!pylonCamera.IsConnected()) {
		if (ImGui::Button("Connect Camera", ImVec2(150, 30))) {
			m_cameraManager.ConnectCamera(m_currentCameraId);
			// Update exposure UI after connecting
			UpdateExposureUIFromCamera();
		}
	}
	else {
		if (ImGui::Button("Disconnect Camera", ImVec2(150, 30))) {
			SafeDisconnectCamera();
		}

		ImGui::SameLine();
		if (ImGui::Button("Reconnect", ImVec2(150, 30))) {
			pylonCamera.TryReconnect();
			// Update exposure UI after reconnecting
			UpdateExposureUIFromCamera();
		}
	}
}

void UICameraPanelUtility::RenderCameraStatus() {
	ImGui::Text("Camera Status");

	if (!ValidateCamera()) {
		ImGui::Text("Camera not available");
		return;
	}

	auto& pylonCamera = m_currentCamera->GetCamera();

	// Status indicators with colors
	ImGui::Text("Connected: ");
	ImGui::SameLine();
	if (pylonCamera.IsConnected()) {
		ImGui::TextColored(ImVec4(0, 1, 0, 1), "Yes");
	}
	else {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "No");
	}

	ImGui::Text("Grabbing: ");
	ImGui::SameLine();
	if (pylonCamera.IsGrabbing()) {
		ImGui::TextColored(ImVec4(0, 1, 0, 1), "Yes");
	}
	else {
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "No");
	}

	ImGui::Text("Device OK: ");
	ImGui::SameLine();
	if (pylonCamera.IsCameraDeviceRemoved()) {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "Removed");
	}
	else {
		ImGui::TextColored(ImVec4(0, 1, 0, 1), "OK");
	}

	if (pylonCamera.IsConnected()) {
		// Show current exposure settings
		auto settings = pylonCamera.GetCurrentExposureSettings();
		ImGui::Text("Current Exposure: %.0f μs", settings.exposure_time);
		ImGui::Text("Current Gain: %.1f", settings.gain);
		ImGui::Text("Auto Exposure: %s", settings.exposure_auto ? "On" : "Off");
		ImGui::Text("Auto Gain: %s", settings.gain_auto ? "On" : "Off");
	}
}

// In UICameraPanelUtility.cpp, update RenderGrabbingControls method:

void UICameraPanelUtility::RenderGrabbingControls() {
	ImGui::Text("Image Acquisition");

	if (!ValidateCamera()) {
		ImGui::Text("Camera not available");
		return;
	}

	auto& pylonCamera = m_currentCamera->GetCamera();

	if (!pylonCamera.IsConnected()) {
		ImGui::Text("Camera not connected");
		return;
	}

	if (!pylonCamera.IsGrabbing()) {
		if (ImGui::Button("Start Grabbing", ImVec2(150, 30))) {
			m_cameraManager.StartGrabbing(m_currentCameraId);
		}

		ImGui::SameLine();
		if (ImGui::Button("Grab Single Frame", ImVec2(150, 30))) {
			// **FIX: Don't call camera directly, call through the camera manager or warn user**
			std::cout << "[INFO] Use Single Frame tab for single frame capture with display" << std::endl;
			m_currentCamera->GrabSingleFrame(); // This only grabs, doesn't update display
		}
	}
	else {
		if (ImGui::Button("Stop Grabbing", ImVec2(150, 30))) {
			m_cameraManager.StopGrabbing(m_currentCameraId);
		}
	}

	// Add helpful text
	ImGui::Spacing();
	ImGui::TextDisabled("Tip: Use 'Single Frame' tab for capture with display");
}


void UICameraPanelUtility::RenderExposureControls() {
	ImGui::Text("Exposure Controls");

	if (!ValidateCamera()) {
		ImGui::Text("Camera not available");
		return;
	}

	auto& pylonCamera = m_currentCamera->GetCamera();

	if (!pylonCamera.IsConnected()) {
		ImGui::Text("Camera not connected");
		return;
	}

	ImGui::Spacing();
	ImGui::Text("Manual Exposure Settings");

	// Custom exposure controls
	ImGui::SliderFloat("Exposure Time (μs)", &m_customExposureTime, 100.0f, 10000.0f, "%.0f");
	ImGui::SliderFloat("Gain", &m_customGain, 0.0f, 10.0f, "%.1f");

	ImGui::Checkbox("Auto Exposure", &m_exposureAuto);
	ImGui::SameLine();
	ImGui::Checkbox("Auto Gain", &m_gainAuto);

	// Control buttons
	if (ImGui::Button("Apply Settings", ImVec2(150, 30))) {
		ApplyExposureSettingsToCamera();
	}

	ImGui::SameLine();
	if (ImGui::Button("Read Current", ImVec2(150, 30))) {
		UpdateExposureUIFromCamera();
	}

	// Quick preset buttons
	ImGui::Spacing();
	ImGui::Text("Quick Presets:");

	if (ImGui::Button("Bright Scene", ImVec2(100, 25))) {
		m_customExposureTime = 500.0f;
		m_customGain = 0.5f;
		m_exposureAuto = false;
		m_gainAuto = false;
		ApplyExposureSettingsToCamera();
	}

	ImGui::SameLine();
	if (ImGui::Button("Dark Scene", ImVec2(100, 25))) {
		m_customExposureTime = 5000.0f;
		m_customGain = 5.0f;
		m_exposureAuto = false;
		m_gainAuto = false;
		ApplyExposureSettingsToCamera();
	}
}

void UICameraPanelUtility::RenderImageControls() {
	ImGui::Text("Image Controls");

	if (!ValidateCamera()) {
		ImGui::Text("Camera not available");
		return;
	}

	auto& pylonCamera = m_currentCamera->GetCamera();

	if (!pylonCamera.IsConnected()) {
		ImGui::Text("Camera not connected");
		return;
	}

	if (ImGui::Button("Capture Image", ImVec2(150, 30))) {
		m_cameraManager.CaptureImage(m_currentCameraId);
	}

	ImGui::SameLine();
	if (ImGui::Button("Toggle View Window", ImVec2(150, 30))) {
		m_currentCamera->ToggleWindow();
	}

	// Show if camera window is visible
	ImGui::Text("View Window: %s", m_currentCamera->IsVisible() ? "Open" : "Closed");
}

void UICameraPanelUtility::RenderAdvancedControls() {
	ImGui::Text("Advanced Controls");

	if (!ValidateCamera()) {
		ImGui::Text("Camera not available");
		return;
	}
}