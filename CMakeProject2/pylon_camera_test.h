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

    // Render ImGui UI for testing camera
    void RenderUI();

    // Capture image and save to disk
    bool CaptureImage();

    // Grab a single frame (when not in continuous mode)
    bool GrabSingleFrame();

    // Get access to the camera instance
    PylonCamera& GetCamera() { return m_camera; }

	bool enableDebug = false;
private:
    // Creates a new OpenGL texture from the current frame
    // Must be called from the main thread (where OpenGL context is valid)
    bool CreateTexture();

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
};

