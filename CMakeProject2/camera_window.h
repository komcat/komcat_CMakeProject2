#pragma once

#include <pylon/PylonIncludes.h>
#include "imgui.h"
#include <string>
#include <filesystem>
#include <thread>
#include <mutex>
#include <atomic>
#include <SDL.h>  // For SDL_Delay

// Forward declarations
class CameraWindow {
private:
    // Pylon camera object
    Pylon::CInstantCamera camera;
    bool isInitialized;
    bool isConnected;
    Pylon::CGrabResultPtr ptrGrabResult;

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

    // Creates an OpenGL texture from the grabbed image
    void UpdateTexture();

    // Save captured image to disk
    bool SaveImageToDisk(const std::string& filename);

    // Thread function for continuous frame grabbing
    void GrabThreadFunction();

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

    // Grab a frame from the camera (now handled in thread)
    bool GrabFrame();

    // Capture an image (grab and save to disk)
    bool CaptureImage();

    // Render the ImGui window with camera controls and image
    void RenderUI();

    // Check if window should be closed
    bool IsDone() const;
    // Safely terminate Pylon runtime
// Static method to safely terminate Pylon
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

    // In camera_window.h, add these methods to public section:
    bool IsGrabbing() const;
    void StopCapture();
    void LogResourceState() const; // For debugging
};