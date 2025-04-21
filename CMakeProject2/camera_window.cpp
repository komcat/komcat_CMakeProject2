#include "camera_window.h"
#include <SDL_opengl.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

CameraWindow::CameraWindow()
    : isInitialized(false)
    , isConnected(false)
    , textureID(0)
    , textureInitialized(false)
    , imageCaptured(false)
    , threadRunning(false)
    , newFrameReady(false)
{
    // Initialize Pylon runtime before using any Pylon methods
    Pylon::PylonInitialize();

    // Set up format converter for display - try RGB format
    formatConverter.OutputPixelFormat = Pylon::PixelType_RGB8packed;
    // Add only the bit alignment which seems available in your SDK
    formatConverter.OutputBitAlignment = Pylon::OutputBitAlignment_MsbAligned;

    // Don't try to use the rotation and flip properties if they're not available
    // Let's rely on image conversion and texture updates instead
}

CameraWindow::~CameraWindow()
{
    // Ensure the thread is stopped
    if (threadRunning.load()) {
        threadRunning.store(false);
        if (grabThread.joinable()) {
            grabThread.join();
        }
    }

    // Clean up resources
    if (isConnected) {
        Disconnect();
    }

    // Clean up texture if it was created
    if (textureInitialized) {
        glDeleteTextures(1, &textureID);
        textureInitialized = false;
    }

    // Terminate Pylon runtime
    Pylon::PylonTerminate();
}

bool CameraWindow::Initialize()
{
    try {
        // Get the transport layer factory
        Pylon::CTlFactory& tlFactory = Pylon::CTlFactory::GetInstance();

        // Get all attached devices
        Pylon::DeviceInfoList_t devices;
        if (tlFactory.EnumerateDevices(devices) == 0) {
            return false; // No camera found
        }

        // Create device from first camera found
        camera.Attach(tlFactory.CreateDevice(devices[0]));

        // Get camera info
        cameraInfo = camera.GetDeviceInfo().GetModelName();
        cameraModel = camera.GetDeviceInfo().GetModelName();

        isInitialized = true;
        return true;
    }
    catch (const Pylon::GenericException& e) {
        // Error handling
        return false;
    }
}

bool CameraWindow::Connect()
{
    if (!isInitialized) {
        return false;
    }

    try {
        // Open the camera
        camera.Open();

        // Set acquisition parameters
        camera.MaxNumBuffer = 5;

        // We'll rely on format conversion instead of trying to set camera format

        // Start grabbing (continuous mode)
        camera.StartGrabbing(Pylon::GrabStrategy_LatestImageOnly);

        isConnected = true;

        // Start the grab thread
        threadRunning.store(true);
        grabThread = std::thread(&CameraWindow::GrabThreadFunction, this);

        return true;
    }
    catch (const Pylon::GenericException& e) {
        printf("Error connecting to camera: %s\n", e.GetDescription());
        return false;
    }
}


void CameraWindow::Disconnect()
{
    // Stop the grab thread first
    if (threadRunning.load()) {
        threadRunning.store(false);
        if (grabThread.joinable()) {
            grabThread.join();
        }
    }

    if (isConnected) {
        // Stop grabbing
        camera.StopGrabbing();

        // Close camera
        camera.Close();

        isConnected = false;
    }
}

// Thread function for continuous frame grabbing
void CameraWindow::GrabThreadFunction()
{
    while (threadRunning.load() && camera.IsGrabbing())
    {
        try {
            // Wait for an image and grab it (timeout 1000ms)
            Pylon::CGrabResultPtr localGrabResult;
            if (camera.RetrieveResult(1000, localGrabResult, Pylon::TimeoutHandling_Return))
            {
                if (localGrabResult->GrabSucceeded())
                {
                    // Lock mutex while updating the image
                    std::lock_guard<std::mutex> lock(imageMutex);

                    // Update the grab result
                    ptrGrabResult = localGrabResult;

                    // Get pixel format to see what we're dealing with
                    Pylon::EPixelType pixelType = ptrGrabResult->GetPixelType();

                    // Convert the grabbed buffer to a pylon image
                    pylonImage.AttachGrabResultBuffer(ptrGrabResult);

                    // Print info about the image
                    //printf("Grabbed image: %d x %d, PixelType: %d\n", pylonImage.GetWidth(), pylonImage.GetHeight(), pixelType);

                    // Convert to a format suitable for display
                    formatConverter.Convert(formatConverterOutput, pylonImage);

                    // Signal that a new frame is ready
                    newFrameReady.store(true);
                }
            }
        }
        catch (const Pylon::GenericException& e) {
            printf("Error in grab thread: %s\n", e.GetDescription());
        }

        // Small sleep to avoid maxing out CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool CameraWindow::GrabFrame()
{
    // This is now just a check if a new frame is available
    // The actual grabbing happens in the thread
    if (!isConnected || !newFrameReady.load()) {
        return false;
    }

    // Lock mutex while accessing the image
    std::lock_guard<std::mutex> lock(imageMutex);

    // Update OpenGL texture with new image data
    UpdateTexture();

    // Reset the new frame flag
    newFrameReady.store(false);

    return true;
}

bool CameraWindow::CaptureImage()
{
    if (!isConnected) {
        return false;
    }

    // Lock mutex to ensure we get a complete frame
    std::lock_guard<std::mutex> lock(imageMutex);

    // Check if we have a valid grab result
    if (!ptrGrabResult || !ptrGrabResult->GrabSucceeded()) {
        return false;
    }

    // Generate a filename based on current time
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "capture_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S") << ".png";
    std::string filename = ss.str();

    // Save the image
    if (SaveImageToDisk(filename)) {
        imageCaptured = true;
        lastSavedPath = filename;
        return true;
    }

    return false;
}

bool CameraWindow::SaveImageToDisk(const std::string& filename)
{
    try {
        // Save the image using Pylon's image writer
        Pylon::CImagePersistence::Save(Pylon::ImageFileFormat_Png, filename.c_str(), pylonImage);
        return true;
    }
    catch (const Pylon::GenericException& e) {
        return false;
    }
}

void CameraWindow::UpdateTexture()
{
    // Get image dimensions
    uint32_t width = formatConverterOutput.GetWidth();
    uint32_t height = formatConverterOutput.GetHeight();

    if (width == 0 || height == 0) {
        return;
    }

    // Get pointer to image data
    const uint8_t* pImageBuffer = static_cast<uint8_t*>(formatConverterOutput.GetBuffer());

    // Debug output - just use dimensions
    //printf("Image dimensions: %d x %d\n", width, height);

    // Create texture if not already created
    if (!textureInitialized) {
        glGenTextures(1, &textureID);
        textureInitialized = true;
    }

    // Bind the texture
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Set proper alignment for pixel data - use 1-byte alignment
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Upload the image data to GPU
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pImageBuffer);

    // Unbind the texture
    glBindTexture(GL_TEXTURE_2D, 0);
}

void CameraWindow::RenderUI()
{
    // Creating ImGui window for the camera
    ImGui::Begin("Basler Camera");

    if (!isInitialized) {
        if (ImGui::Button("Initialize Camera")) {
            Initialize();
        }

        ImGui::Text("Camera not initialized");
    }
    else {
        ImGui::Text("Camera Model: %s", cameraModel.c_str());

        if (!isConnected) {
            if (ImGui::Button("Connect")) {
                Connect();
            }
        }
        else {
            if (ImGui::Button("Disconnect")) {
                Disconnect();
            }

            ImGui::SameLine();

            // Add capture image button
            if (ImGui::Button("Capture Image")) {
                CaptureImage();
            }

            // Display capture status if an image was captured
            if (imageCaptured) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Image saved to: %s", lastSavedPath.c_str());

                // Auto-reset the status after 3 seconds
                static float statusTimer = 0.0f;
                statusTimer += ImGui::GetIO().DeltaTime;
                if (statusTimer > 3.0f) {
                    imageCaptured = false;
                    statusTimer = 0.0f;
                }
            }

            // We don't call GrabFrame() here anymore, we just check if a new frame is ready
            // Render the latest captured frame
            bool hasValidFrame = false;

            // Use a scoped lock to minimize lock time during UI rendering
            {
                std::lock_guard<std::mutex> lock(imageMutex);
                if (ptrGrabResult && ptrGrabResult->GrabSucceeded()) {
                    hasValidFrame = true;
                }
            }

            if (hasValidFrame) {
                // GrabFrame now just updates the texture if a new frame is available
                GrabFrame();

                // Lock mutex for accessing image dimensions
                std::lock_guard<std::mutex> lock(imageMutex);

                // Display image dimensions
                uint32_t width = formatConverterOutput.GetWidth();
                uint32_t height = formatConverterOutput.GetHeight();

                ImGui::Text("Image: %d x %d", width, height);

                // Display the texture with ImGui
                if (textureInitialized) {
                    // Calculate the available width of the window
                    float availWidth = ImGui::GetContentRegionAvail().x;

                    // Calculate aspect ratio to maintain proportions
                    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);

                    // Calculate height based on width to maintain aspect ratio
                    float displayWidth = (std::min)(availWidth, 800.0f); // Limit max width
                    float displayHeight = displayWidth / aspectRatio;

                    // Display the image with the calculated dimensions
                    ImGui::Image((ImTextureID)(intptr_t)textureID,
                        ImVec2(displayWidth, displayHeight),
                        ImVec2(0, 0),        // UV coordinates of the top-left corner
                        ImVec2(1, 1));       // UV coordinates of the bottom-right corner
                }
            }
        }
    }

    ImGui::End();
}

bool CameraWindow::IsDone() const
{
    // This will be called from main to check if window should close
    // For now, we always return false since we handle window closing in main
    return false;
}