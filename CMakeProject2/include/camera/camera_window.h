#pragma once

#include <pylon/PylonIncludes.h>
#include "imgui.h"
#include <string>
#include <filesystem>
#include <thread>
#include <mutex>
#include <atomic>
#include <SDL.h>  // For SDL_Delay
#include <memory>
#include <vector>

// Structure for double buffering
struct ImageBuffer {
    std::vector<uint8_t> data;
    uint32_t width;
    uint32_t height;
    bool isValid;

    // Resize the buffer if needed
    void Resize(uint32_t newWidth, uint32_t newHeight) {
        if (newWidth != width || newHeight != height) {
            data.resize(newWidth * newHeight * 3);
            width = newWidth;
            height = newHeight;
        }
    }

    // Clear the buffer
    void Clear() {
        data.clear();
        width = 0;
        height = 0;
        isValid = false;
    }
};

// Configuration event handler for device removal handling
class CameraDeviceRemovalHandler : public Pylon::CConfigurationEventHandler
{
private:
    class CameraWindow* m_pOwner;

public:
    CameraDeviceRemovalHandler(CameraWindow* pOwner)
        : m_pOwner(pOwner)
    {
    }

    // This method is called from a different thread when camera device removal has been detected
    virtual void OnCameraDeviceRemoved(Pylon::CInstantCamera& camera);
};

// Forward declarations
class CameraWindow {
private:
    // Pylon camera object
    Pylon::CInstantCamera camera;
    bool isInitialized;
    bool isConnected;
    Pylon::CGrabResultPtr ptrGrabResult;

    // Device removal handling
    CameraDeviceRemovalHandler* pDeviceRemovalHandler;
    bool isDeviceRemoved;
    Pylon::String_t lastDeviceSerialNumber;
    Pylon::String_t lastDeviceClass;
    bool attemptReconnect;
    std::atomic<bool> reconnectionInProgress;

    // Image format converter
    Pylon::CImageFormatConverter formatConverter;

    // Latest image data for display
    Pylon::CPylonImage pylonImage;
    Pylon::CPylonImage formatConverterOutput;

    // Camera info
    Pylon::String_t cameraInfo;
    Pylon::String_t cameraModel;

    // OpenGL texture ID for displaying the image
    unsigned int textureID;
    bool textureInitialized;

    // Image capture state
    bool imageCaptured;
    std::string lastSavedPath;

    // Threading related members
    std::thread grabThread;
    std::mutex imageMutex;
    std::atomic<bool> threadRunning;
    std::atomic<bool> newFrameReady;

    // Frame rate control
    int m_targetFPS;

    // Double buffering
    std::unique_ptr<ImageBuffer> m_frontBuffer;
    std::unique_ptr<ImageBuffer> m_backBuffer;
    std::mutex m_bufferMutex;
    bool m_useDoubleBuffering;

    // Creates an OpenGL texture from the grabbed image
    void UpdateTexture();

    // Creates an OpenGL texture from buffer data
    void UpdateTextureFromBuffer(const uint8_t* pImageBuffer, uint32_t width, uint32_t height);

    // Save captured image to disk
    bool SaveImageToDisk(const std::string& filename);

    // Thread function for continuous frame grabbing
    void GrabThreadFunction();

    // Try to reconnect to the previously connected camera
    bool TryReconnectCamera();

    // Double buffering methods
    void InitializeDoubleBuffering();
    void SwapBuffers();
    void UpdateBackBuffer(const uint8_t* imageData, uint32_t width, uint32_t height);

public:
    // Constructor
    CameraWindow();

    // Destructor
    ~CameraWindow();

    // Initialize camera resources
    bool Initialize();

    // Connect to camera
    bool Connect();

    // Disconnect from camera
    void Disconnect();

    // Handle device removal (called by the event handler)
    void HandleDeviceRemoval();

    // Grab a frame from the camera (now handled in thread)
    bool GrabFrame();

    // Capture an image (grab and save to disk)
    bool CaptureImage();

    // Render the ImGui window with camera controls and image
    void RenderUI();

    // Check if window should be closed
    bool IsDone() const;

    // Safely terminate Pylon runtime
    static void SafeTerminatePylon() {
        try {
            // Add a small delay to ensure all resources are released
            SDL_Delay(100);  // 100ms should be enough
            Pylon::PylonTerminate();
            std::cout << "Pylon terminated successfully" << std::endl;
        }
        catch (...) {
            // Silently ignore any errors during termination
            std::cout << "Ignoring Pylon termination error and continuing..." << std::endl;
        }
    }

    // Public methods
    bool IsGrabbing() const;
    void StopCapture();
    void LogResourceState() const; // For debugging

    // Set target FPS for the grab thread
    void SetTargetFPS(int fps) { m_targetFPS = fps; }
    int GetTargetFPS() const { return m_targetFPS; }

    // Check if the camera device has been removed
    bool IsCameraDeviceRemoved() const;

    // Friend the event handler so it can access our private members
    friend class CameraDeviceRemovalHandler;
};