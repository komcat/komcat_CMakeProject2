#pragma once

#include "pylon_camera.h"
#include "CameraExposureManager.h"
#include "imgui.h"
#include "nlohmann/json.hpp"  // Add this
#include "raylibclass.h"
#include <mutex>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>   // Add this - this is the key missing include
#include <iostream>  // Add this for std::cout

// Forward declaration of MachineOperations class to avoid circular inclusion
class MachineOperations;

class PylonCameraTest {
public:
	PylonCameraTest();
	~PylonCameraTest();
	friend class MachineOperations;

	// Render ImGui UI for testing camera
	void RenderUI();

	// New method to render UI with machine operations for pixel-to-mm conversion
	void RenderUIWithMachineOps(MachineOperations* machineOps);

	// Capture image and save to disk
	bool CaptureImage();

	// Grab a single frame (when not in continuous mode)
	bool GrabSingleFrame();

	// Get access to the camera instance
	PylonCamera& GetCamera() { return m_camera; }

	// Get access to the exposure manager
	CameraExposureManager& GetExposureManager() { return m_exposureManager; }

	// Method to apply exposure settings for a specific node
	bool ApplyExposureForNode(const std::string& nodeId);

	// Method to apply default exposure settings
	bool ApplyDefaultExposure();

	bool enableDebug = false;

	bool IsVisible() const { return m_isVisible; }

	// Method to check if texture cleanup is needed - to be called from main loop
	// Just declare these methods in the header
	bool NeedsTextureCleanup() const;
	void CleanupTexture();

	// Modified ToggleWindow to handle texture safely
	void ToggleWindow() {
		m_isVisible = !m_isVisible;

		// When becoming visible, try to initialize the camera automatically
		if (m_isVisible) {
			AutoInitializeAndStartGrabbing();
		}
		else {
			// When hiding, mark texture for cleanup and optionally stop grabbing
			m_needsTextureCleanup = true;

			// Optionally: When hiding, stop grabbing to save resources
			// if (m_camera.IsGrabbing()) {
			//     m_camera.StopGrabbing();
			// }
		}
	}

	// Set the pixel-to-mm calibration factors
	void SetPixelToMMFactors(float xFactor, float yFactor) {
		m_pixelToMMFactorX = xFactor;
		m_pixelToMMFactorY = yFactor;
	}

	// Get the pixel-to-mm calibration factors
	float GetPixelToMMFactorX() const { return m_pixelToMMFactorX; }
	float GetPixelToMMFactorY() const { return m_pixelToMMFactorY; }

	// NEW: Raylib video feed integration (add these to PUBLIC section)
	void SetRaylibWindow(RaylibWindow* raylibWindow) { m_raylibWindow = raylibWindow; }
	bool IsRaylibFeedEnabled() const { return m_enableRaylibFeed; }
	void SetRaylibFeedEnabled(bool enabled) { m_enableRaylibFeed = enabled; }

	// NEW: Method to send frame to raylib window (add to private section)
	void SendFrameToRaylib();

private:
	// Creates a new OpenGL texture from the current frame
	// Must be called from the main thread (where OpenGL context is valid)
	bool CreateTexture();
	bool m_needsTextureCleanup = false;

	// Save the image to disk
	bool SaveImageToDisk(const std::string& filename);

	// Camera instance
	PylonCamera m_camera;
	bool m_deviceRemoved = false;

	// Camera exposure manager
	CameraExposureManager m_exposureManager;

	// Statistics
	uint32_t m_frameCounter;
	uint64_t m_lastFrameTimestamp;
	uint32_t m_lastFrameWidth = 0;
	uint32_t m_lastFrameHeight = 0;

	// Mutex for thread-safe image handling
	std::mutex m_imageMutex;

	// Image display flags
	bool m_hasValidImage;
	std::atomic<bool> m_newFrameReady;

	// Image capture state
	bool m_imageCaptured = false;
	std::string m_lastSavedPath;

	// Pylon image handling
	Pylon::CGrabResultPtr m_ptrGrabResult;
	Pylon::CPylonImage m_pylonImage;
	Pylon::CImageFormatConverter m_formatConverter;
	Pylon::CPylonImage m_formatConverterOutput;

	// OpenGL texture for image display
	unsigned int m_textureID;
	bool m_textureInitialized;
	bool m_isVisible = false;

	// Crosshair related variables
	bool m_showMouseCrosshair = false;  // Flag for mouse crosshair
	ImVec2 m_lastMousePos = ImVec2(0, 0);  // Mouse position on image
	bool m_logPixelOnClick = false;  // Flag to log pixel positions
	float m_clickedImageX = 0.0f;  // X coordinate of last clicked pixel
	float m_clickedImageY = 0.0f;  // Y coordinate of last clicked pixel

	// Pixel-to-mm conversion factors (calibration)
	float m_pixelToMMFactorX = 0.010f;  // Default value: 0.01mm per pixel
	float m_pixelToMMFactorY = 0.010f;  // Default value: 0.01mm per pixel
	bool m_enableClickToMove = false;  // Flag to enable/disable gantry movement on click

	// Helper method to initialize and start grabbing
	void AutoInitializeAndStartGrabbing() {
		// Only proceed if not already grabbing
		if (!m_camera.IsGrabbing()) {
			// If not connected, try to initialize and connect
			if (!m_camera.IsConnected()) {
				if (m_camera.Initialize() && m_camera.Connect()) {
					std::cout << "Camera automatically initialized and connected" << std::endl;

					// Apply default exposure settings when connecting
					ApplyDefaultExposure();
				}
				else {
					std::cout << "Failed to automatically initialize or connect camera" << std::endl;
					return;
				}
			}

			// If connected but not grabbing, start grabbing
			if (m_camera.IsConnected() && !m_camera.IsGrabbing()) {
				// Mark texture for cleanup when starting grabbing
				m_needsTextureCleanup = true;
				m_hasValidImage = false;
				m_frameCounter = 0;
				m_newFrameReady = false;

				if (m_camera.StartGrabbing()) {
					std::cout << "Started grabbing automatically" << std::endl;
				}
				else {
					std::cout << "Failed to start grabbing automatically" << std::endl;
				}
			}
		}
	}

	bool m_controlWindowOpen = true;
	bool m_imageWindowOpen = true;
	// Cached UI calculations
	mutable float m_cachedDisplayWidth = 0.0f;
	mutable float m_cachedDisplayHeight = 0.0f;
	mutable float m_cachedAspectRatio = 1.0f;
	mutable uint32_t m_lastCachedWidth = 0;
	mutable uint32_t m_lastCachedHeight = 0;

	// Texture size tracking
	uint32_t m_lastTextureWidth = 0;
	uint32_t m_lastTextureHeight = 0;


	// Add this to pylon_camera_test.h (in private section)
private:
	std::string m_calibrationFilePath = "camera_calibration.json";

	// Load calibration from JSON file
	bool LoadCalibrationFromFile();

	// Save calibration to JSON file  
	bool SaveCalibrationToFile();




	// NEW: Raylib video feed integration (add these to PRIVATE section)
	RaylibWindow* m_raylibWindow = nullptr;
	bool m_enableRaylibFeed = false;
	std::chrono::steady_clock::time_point m_lastRaylibUpdate;
	static constexpr int RAYLIB_FPS_LIMIT = 60;
};