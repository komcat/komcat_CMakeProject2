#pragma once

#include "pylon_camera.h"
#include "imgui.h"
#include <mutex>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <sstream>

class PylonCameraTest {
public:
	PylonCameraTest();
	~PylonCameraTest();
	friend class MachineOperations;
	// Render ImGui UI for testing camera
	void RenderUI();

	// Capture image and save to disk
	bool CaptureImage();

	// Grab a single frame (when not in continuous mode)
	bool GrabSingleFrame();

	// Get access to the camera instance
	PylonCamera& GetCamera() { return m_camera; }

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


	// Helper method to initialize and start grabbing
	void AutoInitializeAndStartGrabbing() {
		// Only proceed if not already grabbing
		if (!m_camera.IsGrabbing()) {
			// If not connected, try to initialize and connect
			if (!m_camera.IsConnected()) {
				if (m_camera.Initialize() && m_camera.Connect()) {
					std::cout << "Camera automatically initialized and connected" << std::endl;
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
};

