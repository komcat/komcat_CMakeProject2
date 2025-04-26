#include "include/camera/camera_window.h"
#include <SDL_opengl.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

// Implementation of the event handler's OnCameraDeviceRemoved method
// In camera_window.cpp
void CameraDeviceRemovalHandler::OnCameraDeviceRemoved(Pylon::CInstantCamera& /*camera*/)
{
    std::cout << "\n\nCameraDeviceRemovalHandler::OnCameraDeviceRemoved called." << std::endl;
    if (m_pOwner)
    {
        m_pOwner->HandleDeviceRemoval();
    }
}

CameraWindow::CameraWindow()
    : isInitialized(false)
    , isConnected(false)
    , textureID(0)
    , textureInitialized(false)
    , imageCaptured(false)
    , threadRunning(false)
    , newFrameReady(false)
    , pDeviceRemovalHandler(nullptr)
    , isDeviceRemoved(false)
    , attemptReconnect(true)
    , reconnectionInProgress(false)
{
    // Initialize Pylon runtime before using any Pylon methods
    Pylon::PylonInitialize();

    // Set up format converter for display - try RGB format
    formatConverter.OutputPixelFormat = Pylon::PixelType_RGB8packed;
    // Add only the bit alignment which seems available in your SDK
    formatConverter.OutputBitAlignment = Pylon::OutputBitAlignment_MsbAligned;

    // Create the device removal handler and register it
    pDeviceRemovalHandler = new CameraDeviceRemovalHandler(this);
}

// Update CameraWindow::~CameraWindow()
CameraWindow::~CameraWindow()
{
    try {
        // First, stop the grabbing thread if it's running
        if (threadRunning.exchange(false) && grabThread.joinable()) {
            grabThread.join();
        }

        // Stop grabbing if active
        if (isConnected && camera.IsGrabbing()) {
            try {
                camera.StopGrabbing();
            }
            catch (...) {
                // Ignore errors during stop grabbing
            }
        }

        // Release Pylon resources in the correct order
        ptrGrabResult.Release();
        formatConverterOutput.Release();
        pylonImage.Release();

        // Close and detach camera
        if (isConnected) {
            try {
                camera.Close();
                isConnected = false;
            }
            catch (...) {
                // Ignore errors during close
            }
        }

        // Destroy device and handle removal
        if (camera.IsPylonDeviceAttached()) {
            camera.DestroyDevice();
        }

        // Clean up OpenGL texture if created
        if (textureInitialized) {
            glDeleteTextures(1, &textureID);
            textureInitialized = false;
        }

        // Delete the device removal handler
        delete pDeviceRemovalHandler;
        pDeviceRemovalHandler = nullptr;
    }
    catch (...) {
        // Catch any exceptions to ensure destruction completes
    }
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

        // Store device info for reconnection if needed
        lastDeviceSerialNumber = camera.GetDeviceInfo().GetSerialNumber();
        lastDeviceClass = camera.GetDeviceInfo().GetDeviceClass();

        // Register the device removal handler with the camera
        camera.RegisterConfiguration(pDeviceRemovalHandler, Pylon::RegistrationMode_Append, Pylon::Cleanup_None);

        isInitialized = true;
        isDeviceRemoved = false;
        return true;
    }
    catch (const Pylon::GenericException& e) {
        // Error handling
        std::cerr << "Error initializing camera: " << e.GetDescription() << std::endl;
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

        // For GigE cameras, set a reasonable heartbeat timeout for faster device removal detection
        try {
            Pylon::CIntegerParameter heartbeat(camera.GetTLNodeMap(), "HeartbeatTimeout");
            heartbeat.TrySetValue(1000, Pylon::IntegerValueCorrection_Nearest);  // 1000ms
        }
        catch (...) {
            // Ignore if not available for this camera
        }

        // Start grabbing (continuous mode)
        camera.StartGrabbing(Pylon::GrabStrategy_LatestImageOnly);

        isConnected = true;
        isDeviceRemoved = false;

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


// Update the CameraWindow::Disconnect() method in camera_window.cpp
void CameraWindow::Disconnect()
{
    LogResourceState(); // Log state before disconnection

    if (isConnected) {
        try {
            // First, if we have a grab thread running, stop it properly
            if (threadRunning.load()) {
                std::cout << "Stopping grab thread..." << std::endl;
                threadRunning.store(false);

                // Wait for thread to complete with a timeout
                if (grabThread.joinable()) {
                    grabThread.join();
                    std::cout << "Grab thread joined successfully" << std::endl;
                }
            }

            // Then stop camera grabbing
            if (camera.IsGrabbing()) {
                std::cout << "Stopping camera grabbing..." << std::endl;
                camera.StopGrabbing();
                SDL_Delay(100); // Give it time to stop
            }

            // Release grab result to ensure no references are held
            if (ptrGrabResult) {
                std::cout << "Releasing grab result..." << std::endl;
                ptrGrabResult.Release();
            }

            // Release pylon image attachments
            if (pylonImage.IsValid()) {
                std::cout << "Releasing pylon image..." << std::endl;
                pylonImage.Release();
            }

            if (formatConverterOutput.IsValid()) {
                std::cout << "Releasing converter output image..." << std::endl;
                formatConverterOutput.Release();
            }

            // Add a delay to ensure resources are released
            SDL_Delay(150);

            std::cout << "Closing camera connection..." << std::endl;
            // Close camera
            camera.Close();

            isConnected = false;
            std::cout << "Camera disconnected successfully" << std::endl;
        }
        catch (const Pylon::GenericException& e) {
            std::cerr << "Error disconnecting camera: " << e.GetDescription() << std::endl;
        }
    }

    LogResourceState(); // Log state after disconnection
}

void CameraWindow::HandleDeviceRemoval()
{
    std::cout << "Camera device removal detected!" << std::endl;

    // Set the device removed flag
    isDeviceRemoved = true;

    // Stop the grab thread safely
    if (threadRunning.load()) {
        threadRunning.store(false);
        if (grabThread.joinable()) {
            grabThread.join();
        }
    }

    // Update the connection state
    isConnected = false;

    // Destroy the device as recommended by Pylon
    try {
        camera.DestroyDevice();
        std::cout << "Camera device destroyed successfully" << std::endl;
    }
    catch (const Pylon::GenericException& e) {
        std::cerr << "Error destroying device: " << e.GetDescription() << std::endl;
    }
}



bool CameraWindow::IsCameraDeviceRemoved() const
{
    return isDeviceRemoved || (isConnected && camera.IsCameraDeviceRemoved());
}

bool CameraWindow::TryReconnectCamera()
{
    if (reconnectionInProgress.exchange(true)) {
        return false; // Already trying to reconnect
    }

    bool result = false;

    try {
        std::cout << "Attempting to reconnect to camera..." << std::endl;

        // Create device info object for the camera we want to reconnect to
        Pylon::CDeviceInfo info;
        info.SetDeviceClass(lastDeviceClass);
        info.SetSerialNumber(lastDeviceSerialNumber);

        // Create a filter with the device info
        Pylon::DeviceInfoList_t filter;
        filter.push_back(info);

        // Destroy the current device since it's no longer valid
        camera.DestroyDevice();

        // Try to find the camera
        Pylon::CTlFactory& tlFactory = Pylon::CTlFactory::GetInstance();
        Pylon::DeviceInfoList_t devices;

        if (tlFactory.EnumerateDevices(devices, filter) > 0) {
            // The camera has been found, create and attach it
            camera.Attach(tlFactory.CreateDevice(devices[0]));

            // Register the device removal handler again
            camera.RegisterConfiguration(pDeviceRemovalHandler, Pylon::RegistrationMode_Append, Pylon::Cleanup_None);

            // Reset flags
            isDeviceRemoved = false;
            isInitialized = true;

            // Try to connect to the camera
            result = Connect();

            if (result) {
                std::cout << "Successfully reconnected to camera " << camera.GetDeviceInfo().GetModelName() << std::endl;
            }
        }
        else {
            std::cout << "Camera not found for reconnection" << std::endl;
        }
    }
    catch (const Pylon::GenericException& e) {
        std::cerr << "Error during reconnection attempt: " << e.GetDescription() << std::endl;
    }

    reconnectionInProgress.store(false);
    return result;
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

            // Check if the camera has been removed
            if (camera.IsCameraDeviceRemoved()) {
                // Let the main class know about the removal
                HandleDeviceRemoval();
                break; // Exit the thread loop
            }
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

bool CameraWindow::IsGrabbing() const
{
    return isConnected && camera.IsGrabbing();
}

void CameraWindow::StopCapture()
{
    if (isConnected && camera.IsGrabbing()) {
        try {
            // Log the state before stopping
            std::cout << "Stopping camera grabbing..." << std::endl;

            // First make sure the thread knows it should stop
            threadRunning.store(false);

            // Small delay to allow thread to see the flag
            SDL_Delay(50);

            // Then join the thread if it's joinable
            if (grabThread.joinable()) {
                grabThread.join();
                std::cout << "Grab thread joined successfully" << std::endl;
            }

            // Now safe to stop the camera grabbing
            camera.StopGrabbing();

            // Add a small delay
            SDL_Delay(100);

            std::cout << "Camera grabbing stopped successfully" << std::endl;
        }
        catch (const Pylon::GenericException& e) {
            std::cerr << "Error stopping camera grabbing: " << e.GetDescription() << std::endl;
        }
    }
}
void CameraWindow::LogResourceState() const
{
    std::cout << "Camera resource state:" << std::endl;
    std::cout << "  Initialized: " << (isInitialized ? "Yes" : "No") << std::endl;
    std::cout << "  Connected: " << (isConnected ? "Yes" : "No") << std::endl;
    std::cout << "  Is grabbing: " << (isConnected && camera.IsGrabbing() ? "Yes" : "No") << std::endl;
    std::cout << "  Texture initialized: " << (textureInitialized ? "Yes" : "No") << std::endl;
    std::cout << "  Device removed: " << (isDeviceRemoved ? "Yes" : "No") << std::endl;
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

    // Check if the device was removed and try to reconnect if requested
    if (isDeviceRemoved && attemptReconnect) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Camera disconnected!");

        if (ImGui::Button("Try Reconnect")) {
            if (TryReconnectCamera()) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Camera reconnected successfully!");
            }
            else {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Failed to reconnect. Make sure camera is connected.");
            }
        }

        ImGui::End();
        return;
    }

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
            // Check if the camera is still connected
            if (camera.IsCameraDeviceRemoved()) {
                HandleDeviceRemoval();
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Camera disconnected!");

                if (ImGui::Button("Try Reconnect")) {
                    if (TryReconnectCamera()) {
                        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Camera reconnected successfully!");
                    }
                    else {
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Failed to reconnect. Make sure camera is connected.");
                    }
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
    }

    ImGui::End();
}

bool CameraWindow::IsDone() const
{
    // This will be called from main to check if window should close
    // For now, we always return false since we handle window closing in main
    return false;
}

