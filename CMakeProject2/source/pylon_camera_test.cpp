#include "include/camera/pylon_camera_test.h"
#include <iostream>
#include <SDL_opengl.h>

PylonCameraTest::PylonCameraTest()
    : m_frameCounter(0)
    , m_lastFrameTimestamp(0)
    , m_hasValidImage(false)
    , m_textureID(0)
    , m_textureInitialized(false)
    , m_newFrameReady(false)
{
    // Initialize format converter for display
    m_formatConverter.OutputPixelFormat = Pylon::PixelType_RGB8packed;
    m_formatConverter.OutputBitAlignment = Pylon::OutputBitAlignment_MsbAligned;

    // Set callbacks
    m_camera.SetDeviceRemovalCallback([this]() {
        std::cout << "Device removal callback called" << std::endl;
        m_deviceRemoved = true;
    });

    m_camera.SetNewFrameCallback([this](const Pylon::CGrabResultPtr& grabResult) {
        // Process the new frame
        if (grabResult->GrabSucceeded()) {
            // Update frame counter
            m_frameCounter++;

            // Get timestamp
            m_lastFrameTimestamp = grabResult->GetTimeStamp();

            // Get image width and height
            m_lastFrameWidth = grabResult->GetWidth();
            m_lastFrameHeight = grabResult->GetHeight();

            try {
                // Lock mutex while updating the image
                std::lock_guard<std::mutex> lock(m_imageMutex);

                // Store the grab result
                m_ptrGrabResult = grabResult;

                // Convert the grabbed buffer to a pylon image
                if (m_pylonImage.IsValid()) {
                    m_pylonImage.Release();
                }
                m_pylonImage.AttachGrabResultBuffer(m_ptrGrabResult);

                // Convert to a format suitable for display
                if (m_formatConverterOutput.IsValid()) {
                    m_formatConverterOutput.Release();
                }
                m_formatConverter.Convert(m_formatConverterOutput, m_pylonImage);

                // Signal that a new frame is ready
                m_newFrameReady = true;
            }
            catch (const std::exception& e) {
                std::cerr << "Error in frame callback: " << e.what() << std::endl;
            }
        }
    });
}

PylonCameraTest::~PylonCameraTest() {
    // Clean up texture if it was created
    if (m_textureInitialized) {
        glDeleteTextures(1, &m_textureID);
        m_textureInitialized = false;
    }

    // Release pylon image resources
    m_ptrGrabResult.Release();
    m_formatConverterOutput.Release();
    m_pylonImage.Release();
}

bool PylonCameraTest::CreateTexture() {
    // This should be called from the main thread (RenderUI)

    // Lock mutex while accessing the image
    std::lock_guard<std::mutex> lock(m_imageMutex);

    if (!m_newFrameReady || !m_formatConverterOutput.IsValid()) {
        return false;
    }

    try {
        // Get image dimensions
        uint32_t width = m_formatConverterOutput.GetWidth();
        uint32_t height = m_formatConverterOutput.GetHeight();

        if (width == 0 || height == 0) {
            return false;
        }

        // Get pointer to image data
        const uint8_t* pImageBuffer = static_cast<uint8_t*>(m_formatConverterOutput.GetBuffer());
        if (!pImageBuffer) {
            return false;
        }

        // Clean up previous texture if it exists
        if (m_textureInitialized) {
            glDeleteTextures(1, &m_textureID);
            m_textureInitialized = false;
        }

        // Create a new texture
        glGenTextures(1, &m_textureID);

        // Check if texture was created successfully
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cerr << "OpenGL error after glGenTextures: " << error << std::endl;
            return false;
        }

        m_textureInitialized = true;

        // Bind the texture
        glBindTexture(GL_TEXTURE_2D, m_textureID);

        error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cerr << "OpenGL error after glBindTexture: " << error << std::endl;
            return false;
        }

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Set proper alignment for pixel data - use 1-byte alignment
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // Upload the image data to GPU
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pImageBuffer);

        // Check for OpenGL errors
        error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cerr << "OpenGL error after glTexImage2D: " << error << std::endl;
            m_hasValidImage = false;
            return false;
        }

        // Set valid image flag if no errors
        m_hasValidImage = true;

        // Reset the new frame flag
        m_newFrameReady = false;

        // Unbind the texture
        glBindTexture(GL_TEXTURE_2D, 0);

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error creating texture: " << e.what() << std::endl;
        return false;
    }
}

void PylonCameraTest::RenderUI() {
    // Main control window
    ImGui::Begin("Pylon Camera Test");

    // Camera connection/initialization
    if (!m_camera.IsConnected()) {
        if (ImGui::Button("Initialize & Connect")) {
            if (m_camera.Initialize() && m_camera.Connect()) {
                std::cout << "Camera initialized and connected" << std::endl;
            }
            else {
                std::cout << "Failed to initialize or connect camera" << std::endl;
            }
        }
    }
    else {
        // Display camera info
        ImGui::Text("%s", m_camera.GetDeviceInfo().c_str());

        // Grabbing control
        if (!m_camera.IsGrabbing()) {
            // Add "Start Grabbing" button for continuous acquisition
            if (ImGui::Button("Start Grabbing")) {
                // Reset texture when starting grabbing
                if (m_textureInitialized) {
                    glDeleteTextures(1, &m_textureID);
                    m_textureInitialized = false;
                }
                m_hasValidImage = false;
                m_frameCounter = 0;
                m_newFrameReady = false;

                if (m_camera.StartGrabbing()) {
                    std::cout << "Started grabbing" << std::endl;
                }
                else {
                    std::cout << "Failed to start grabbing" << std::endl;
                }
            }

            // Add "Grab One Image" button for single frame acquisition
            ImGui::SameLine();
            if (ImGui::Button("Grab One Image")) {
                if (GrabSingleFrame()) {
                    // If frame was grabbed successfully, update texture
                    if (m_newFrameReady) {
                        CreateTexture();
                    }
                }
            }
        }
        else {
            // Button to stop continuous acquisition
            if (ImGui::Button("Stop Grabbing")) {
                m_camera.StopGrabbing();
                std::cout << "Stopped grabbing" << std::endl;
            }

            // Add capture button
            ImGui::SameLine();
            if (ImGui::Button("Capture Image")) {
                CaptureImage();
            }

            // Display capture status if an image was captured
            if (m_imageCaptured) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Saved to: %s", m_lastSavedPath.c_str());

                // Auto-reset the status after 3 seconds
                static float statusTimer = 0.0f;
                statusTimer += ImGui::GetIO().DeltaTime;
                if (statusTimer > 3.0f) {
                    m_imageCaptured = false;
                    statusTimer = 0.0f;
                }
            }

            // Show grabbing stats
            ImGui::Text("Frames received: %d", m_frameCounter);
            ImGui::Text("Last frame size: %dx%d", m_lastFrameWidth, m_lastFrameHeight);
            ImGui::Text("Last timestamp: %llu", m_lastFrameTimestamp);

            // Check if we need to create a new texture from the latest frame
            if (m_newFrameReady) {
                if (CreateTexture()) {

					if (enableDebug)
					{
						// Debug output for texture creation						
                        std::cout << "successfully created texture with ID: " << m_textureID << std::endl;
					}
                    
                }
            }
        }

        // Disconnect button
        if (ImGui::Button("Disconnect")) {
            m_camera.Disconnect();

            // Clean up texture when disconnecting
            if (m_textureInitialized) {
                glDeleteTextures(1, &m_textureID);
                m_textureInitialized = false;
            }
            m_hasValidImage = false;

            std::cout << "Camera disconnected" << std::endl;
            m_frameCounter = 0;
        }
    }

    // Device removal handling
    if (m_camera.IsCameraDeviceRemoved() || m_deviceRemoved) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Camera has been removed!");

        if (ImGui::Button("Try Reconnect")) {
            if (m_camera.TryReconnect()) {
                std::cout << "Successfully reconnected to camera" << std::endl;
                m_deviceRemoved = false;
            }
            else {
                std::cout << "Failed to reconnect to camera" << std::endl;
            }
        }
    }

    ImGui::End();

    // Image display window - always show when we have a valid image
    if (m_camera.IsGrabbing() || m_hasValidImage) {
        ImGui::Begin("Camera Image");

        if (m_textureInitialized && m_hasValidImage) {
            // Calculate the available width of the window
            float availWidth = ImGui::GetContentRegionAvail().x;

            // Calculate aspect ratio to maintain proportions
            float aspectRatio = static_cast<float>(m_lastFrameWidth) / static_cast<float>(m_lastFrameHeight);
            if (aspectRatio <= 0.0f) aspectRatio = 1.0f; // Safeguard

            // Calculate height based on width to maintain aspect ratio
            float displayWidth = (std::min)(availWidth, 800.0f); // Limit max width
            float displayHeight = displayWidth / aspectRatio;

            // Display the image with the calculated dimensions
            ImGui::Image((ImTextureID)(intptr_t)m_textureID,
                ImVec2(displayWidth, displayHeight),
                ImVec2(0, 0),        // UV coordinates of the top-left corner
                ImVec2(1, 1));       // UV coordinates of the bottom-right corner
        }
        else {
            ImGui::Text("Waiting for valid image from camera...");
            if (!m_textureInitialized) {
                ImGui::Text("Texture not initialized");
            }
            if (!m_hasValidImage) {
                ImGui::Text("No valid image data");
            }
        }

        ImGui::End();
    }
}


bool PylonCameraTest::CaptureImage()
{
    if (!m_camera.IsConnected() || !m_camera.IsGrabbing()) {
        std::cerr << "Cannot capture image: Camera not connected or not grabbing" << std::endl;
        return false;
    }

    try {
        // Lock mutex to ensure we get a complete frame
        std::lock_guard<std::mutex> lock(m_imageMutex);

        // Check if we have a valid grab result
        if (!m_ptrGrabResult || !m_ptrGrabResult->GrabSucceeded() || !m_pylonImage.IsValid()) {
            std::cerr << "No valid frame available to capture" << std::endl;
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
            m_imageCaptured = true;
            m_lastSavedPath = filename;
            std::cout << "Image captured and saved as: " << filename << std::endl;
            return true;
        }
        else {
            std::cerr << "Failed to save image" << std::endl;
            return false;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error during image capture: " << e.what() << std::endl;
        return false;
    }
}

bool PylonCameraTest::SaveImageToDisk(const std::string& filename)
{
    try {
        // Save the image using Pylon's image writer
        Pylon::CImagePersistence::Save(Pylon::ImageFileFormat_Png, filename.c_str(), m_pylonImage);
        return true;
    }
    catch (const Pylon::GenericException& e) {
        std::cerr << "Error saving image: " << e.GetDescription() << std::endl;
        return false;
    }
    catch (const std::exception& e) {
        std::cerr << "Error saving image: " << e.what() << std::endl;
        return false;
    }
}
bool PylonCameraTest::GrabSingleFrame()
{
    if (!m_camera.IsConnected()) {
        std::cerr << "Cannot grab frame: Camera not connected" << std::endl;
        return false;
    }

    // If already grabbing continuously, don't do anything
    if (m_camera.IsGrabbing()) {
        std::cerr << "Already grabbing continuously" << std::endl;
        return false;
    }

    try {
        // Start single frame acquisition
        std::cout << "Grabbing single frame..." << std::endl;

        // Create a temporary grab result
        Pylon::CGrabResultPtr grabResult;

        // Open camera if not already open
        if (!m_camera.GetInternalCamera().IsOpen()) {
            m_camera.GetInternalCamera().Open();
        }

        // Set the camera to SingleFrame acquisition mode if possible
        try {
            Pylon::CEnumParameter(m_camera.GetInternalCamera().GetNodeMap(), "AcquisitionMode").SetValue("SingleFrame");
        }
        catch (...) {
            // Some cameras might not support this mode, ignore the error
        }

        // Execute software trigger if camera supports it
        try {
            m_camera.GetInternalCamera().ExecuteSoftwareTrigger();
        }
        catch (...) {
            // If software trigger is not supported, we'll just grab
        }

        // Start grabbing with a timeout of 5 seconds
        m_camera.GetInternalCamera().GrabOne(5000, grabResult);

        if (grabResult && grabResult->GrabSucceeded()) {
            std::cout << "Single frame grabbed successfully" << std::endl;

            // Lock mutex while updating the image
            std::lock_guard<std::mutex> lock(m_imageMutex);

            // Store the grab result
            m_ptrGrabResult = grabResult;

            // Update frame counter and dimensions
            m_frameCounter++;
            m_lastFrameWidth = grabResult->GetWidth();
            m_lastFrameHeight = grabResult->GetHeight();
            m_lastFrameTimestamp = grabResult->GetTimeStamp();

            // Convert the grabbed buffer to a pylon image
            if (m_pylonImage.IsValid()) {
                m_pylonImage.Release();
            }
            m_pylonImage.AttachGrabResultBuffer(m_ptrGrabResult);

            // Convert to a format suitable for display
            if (m_formatConverterOutput.IsValid()) {
                m_formatConverterOutput.Release();
            }
            m_formatConverter.Convert(m_formatConverterOutput, m_pylonImage);

            // Signal that a new frame is ready
            m_newFrameReady = true;

            return true;
        }
        else {
            std::cerr << "Failed to grab single frame" << std::endl;
            return false;
        }
    }
    catch (const Pylon::GenericException& e) {
        std::cerr << "Pylon exception during single frame grab: " << e.GetDescription() << std::endl;
        return false;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception during single frame grab: " << e.what() << std::endl;
        return false;
    }
}