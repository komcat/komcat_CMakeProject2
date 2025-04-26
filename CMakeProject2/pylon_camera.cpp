#include "pylon_camera.h"
#include <iostream>
#include <chrono>
#include <thread>

// Implementation of PylonDeviceRemovalHandler

PylonDeviceRemovalHandler::PylonDeviceRemovalHandler(PylonCamera* pOwner)
    : m_pOwner(pOwner)
{
}

PylonDeviceRemovalHandler::~PylonDeviceRemovalHandler()
{
    // No specific cleanup needed
}

void PylonDeviceRemovalHandler::OnCameraDeviceRemoved(Pylon::CInstantCamera& /*camera*/)
{
    if (m_pOwner)
    {
        std::cout << "PylonDeviceRemovalHandler::OnCameraDeviceRemoved called." << std::endl;
        // Notify the PylonCamera that the device has been removed
        m_pOwner->HandleDeviceRemoval();
    }
}

// Implementation of PylonCamera

PylonCamera::PylonCamera()
    : m_initialized(false)
    , m_connected(false)
    , m_deviceRemoved(false)
    , m_reconnecting(false)
    , m_threadRunning(false)
    , m_pDeviceRemovalHandler(nullptr)
    , m_deviceRemovalCallback(nullptr)
    , m_newFrameCallback(nullptr)
{
    // Initialize Pylon runtime
    Pylon::PylonInitialize();

    // Create the device removal handler
    m_pDeviceRemovalHandler = new PylonDeviceRemovalHandler(this);
}

PylonCamera::~PylonCamera()
{
    std::cout << "PylonCamera destructor called" << std::endl;

    try
    {
        // First, stop the grabbing thread if it's running
        bool wasRunning = m_threadRunning.exchange(false);
        if (wasRunning && m_grabThread.joinable())
        {
            std::cout << "Joining grab thread..." << std::endl;
            m_grabThread.join();
            std::cout << "Grab thread joined" << std::endl;
        }

        // Disconnect the camera if it's connected
        if (m_connected)
        {
            Disconnect();
        }

        // Detach camera to release device if still attached
        try
        {
            if (m_camera.IsPylonDeviceAttached())
            {
                m_camera.DetachDevice();
                std::cout << "Camera device detached" << std::endl;
            }

            m_camera.DestroyDevice();
        }
        catch (...)
        {
            std::cout << "Ignoring error during DetachDevice" << std::endl;
        }

        // Delete the device removal handler
        delete m_pDeviceRemovalHandler;
        m_pDeviceRemovalHandler = nullptr;

        // Terminate Pylon runtime
        try
        {
            // Add a small delay to ensure all resources are released
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            Pylon::PylonTerminate();
            std::cout << "Pylon terminated successfully" << std::endl;
        }
        catch (...)
        {
            std::cout << "Ignoring Pylon termination error and continuing..." << std::endl;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error in PylonCamera destructor: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Unknown error in PylonCamera destructor" << std::endl;
    }

    std::cout << "PylonCamera destructor completed" << std::endl;
}

bool PylonCamera::Initialize()
{
    if (m_initialized)
    {
        return true; // Already initialized
    }

    try
    {
        // Reset state
        m_deviceRemoved = false;
        m_initialized = true;

        return true;
    }
    catch (const Pylon::GenericException& e)
    {
        std::cerr << "Error initializing camera: " << e.GetDescription() << std::endl;
        return false;
    }
}

bool PylonCamera::Connect()
{
    if (!m_initialized)
    {
        if (!Initialize())
        {
            return false;
        }
    }

    // Already connected
    if (m_connected)
    {
        return true;
    }

    try
    {
        std::lock_guard<std::mutex> lock(m_cameraMutex);

        // Get the transport layer factory
        Pylon::CTlFactory& tlFactory = Pylon::CTlFactory::GetInstance();

        // Get all attached devices
        Pylon::DeviceInfoList_t devices;
        if (tlFactory.EnumerateDevices(devices) == 0)
        {
            std::cerr << "No camera found" << std::endl;
            return false; // No camera found
        }

        // Create device from first camera found
        m_camera.Attach(tlFactory.CreateDevice(devices[0]));

        // Store device info for reconnection if needed
        m_lastDeviceSerialNumber = m_camera.GetDeviceInfo().GetSerialNumber();
        m_lastDeviceClass = m_camera.GetDeviceInfo().GetDeviceClass();

        // Register the device removal handler with the camera
        m_camera.RegisterConfiguration(m_pDeviceRemovalHandler, Pylon::RegistrationMode_Append, Pylon::Cleanup_None);

        // Open the camera
        m_camera.Open();

        // For GigE cameras, set a reasonable heartbeat timeout for faster device removal detection
        try
        {
            Pylon::CIntegerParameter heartbeat(m_camera.GetTLNodeMap(), "HeartbeatTimeout");
            heartbeat.TrySetValue(1000, Pylon::IntegerValueCorrection_Nearest);  // set to 1000 ms timeout if writable
        }
        catch (...)
        {
            // Ignore if not available for this camera
        }

        m_connected = true;
        m_deviceRemoved = false;

        std::cout << "Connected to camera: " << m_camera.GetDeviceInfo().GetModelName() << std::endl;
        std::cout << "Serial Number: " << m_lastDeviceSerialNumber << std::endl;

        return true;
    }
    catch (const Pylon::GenericException& e)
    {
        std::cerr << "Error connecting to camera: " << e.GetDescription() << std::endl;
        return false;
    }
}

bool PylonCamera::ConnectToSerial(const std::string& serialNumber)
{
    if (!m_initialized)
    {
        if (!Initialize())
        {
            return false;
        }
    }

    // Already connected
    if (m_connected)
    {
        // Check if we're already connected to the requested camera
        // Convert Pylon::String_t to std::string for proper comparison
        std::string currentSerialNumber(m_lastDeviceSerialNumber.c_str());
        if (currentSerialNumber == serialNumber)
        {
            return true;
        }

        // Disconnect from the current camera
        Disconnect();
    }

    try
    {
        std::lock_guard<std::mutex> lock(m_cameraMutex);

        // Get the transport layer factory
        Pylon::CTlFactory& tlFactory = Pylon::CTlFactory::GetInstance();

        // Create device info object for the camera we want to connect to
        Pylon::CDeviceInfo info;
        info.SetSerialNumber(serialNumber.c_str());

        // Create a filter with the device info
        Pylon::DeviceInfoList_t filter;
        filter.push_back(info);

        // Try to find the camera
        Pylon::DeviceInfoList_t devices;
        if (tlFactory.EnumerateDevices(devices, filter) == 0)
        {
            std::cerr << "Camera with serial " << serialNumber << " not found" << std::endl;
            return false; // Camera not found
        }

        // Create device from the found camera
        m_camera.Attach(tlFactory.CreateDevice(devices[0]));

        // Store device info for reconnection if needed
        m_lastDeviceSerialNumber = m_camera.GetDeviceInfo().GetSerialNumber();
        m_lastDeviceClass = m_camera.GetDeviceInfo().GetDeviceClass();

        // Register the device removal handler with the camera
        m_camera.RegisterConfiguration(m_pDeviceRemovalHandler, Pylon::RegistrationMode_Append, Pylon::Cleanup_None);

        // Open the camera
        m_camera.Open();

        // For GigE cameras, set a reasonable heartbeat timeout for faster device removal detection
        try
        {
            Pylon::CIntegerParameter heartbeat(m_camera.GetTLNodeMap(), "HeartbeatTimeout");
            heartbeat.TrySetValue(1000, Pylon::IntegerValueCorrection_Nearest);  // set to 1000 ms timeout if writable
        }
        catch (...)
        {
            // Ignore if not available for this camera
        }

        m_connected = true;
        m_deviceRemoved = false;

        std::cout << "Connected to camera: " << m_camera.GetDeviceInfo().GetModelName() << std::endl;
        std::cout << "Serial Number: " << m_lastDeviceSerialNumber << std::endl;

        return true;
    }
    catch (const Pylon::GenericException& e)
    {
        std::cerr << "Error connecting to camera: " << e.GetDescription() << std::endl;
        return false;
    }
}


void PylonCamera::Disconnect()
{
    std::lock_guard<std::mutex> lock(m_cameraMutex);

    if (!m_connected)
    {
        return; // Not connected
    }

    try
    {
        // First, stop grabbing if we're grabbing
        if (m_threadRunning.load())
        {
            StopGrabbing();
        }

        // Close camera
        if (m_camera.IsOpen())
        {
            m_camera.Close();
        }

        m_connected = false;
        std::cout << "Camera disconnected successfully" << std::endl;
    }
    catch (const Pylon::GenericException& e)
    {
        std::cerr << "Error disconnecting camera: " << e.GetDescription() << std::endl;
    }
}

bool PylonCamera::StartGrabbing()
{
    // Use a lock to ensure thread safety during camera configuration
    std::lock_guard<std::mutex> lock(m_cameraMutex);

    if (!m_connected || m_deviceRemoved)
    {
        return false;
    }

    // If already grabbing, stop first
    if (m_camera.IsGrabbing())
    {
        try {
            m_camera.StopGrabbing();

            // Wait for thread to terminate if it exists
            if (m_threadRunning.load())
            {
                m_threadRunning.store(false);
                if (m_grabThread.joinable())
                {
                    m_grabThread.join();
                }
            }
        }
        catch (const Pylon::GenericException& e) {
            std::cerr << "Error stopping existing grab session: " << e.GetDescription() << std::endl;
        }
    }

    try
    {
        // Set acquisition parameters
        m_camera.MaxNumBuffer = 10;  // Increased buffer count

        // Set acquisition mode to continuous
        try {
            // First check if the camera supports setting acquisition mode
            if (m_camera.GetNodeMap().GetNode("AcquisitionMode")) {
                Pylon::CEnumParameter(m_camera.GetNodeMap(), "AcquisitionMode").SetValue("Continuous");
                std::cout << "Set acquisition mode to Continuous" << std::endl;
            }
        }
        catch (const Pylon::GenericException& e) {
            std::cerr << "Warning: Could not set acquisition mode: " << e.GetDescription() << std::endl;
        }

        // Disable trigger mode if camera supports it
        try {
            if (m_camera.GetNodeMap().GetNode("TriggerMode")) {
                Pylon::CEnumParameter(m_camera.GetNodeMap(), "TriggerMode").SetValue("Off");
                std::cout << "Disabled trigger mode" << std::endl;
            }
        }
        catch (const Pylon::GenericException& e) {
            std::cerr << "Warning: Could not disable trigger mode: " << e.GetDescription() << std::endl;
        }

        // For GigE cameras, try to optimize the packet size
        try {
            // Check if this is a GigE camera
            if (m_camera.GetTLNodeMap().GetNode("GevSCPSPacketSize")) {
                Pylon::CIntegerParameter packetSize(m_camera.GetTLNodeMap(), "GevSCPSPacketSize");
                // Get the maximum allowed packet size
                int64_t maxPacketSize = packetSize.GetMax();
                // Set to maximum value
                packetSize.SetValue(maxPacketSize);
                std::cout << "Set GigE packet size to maximum: " << packetSize.GetValue() << std::endl;
            }
        }
        catch (const Pylon::GenericException& e) {
            std::cerr << "Warning: Could not optimize packet size: " << e.GetDescription() << std::endl;
        }

        // Configure auto exposure and gain
        try {
            if (m_camera.GetNodeMap().GetNode("ExposureAuto")) {
                Pylon::CEnumParameter(m_camera.GetNodeMap(), "ExposureAuto").SetValue("Continuous");
                std::cout << "Set exposure to auto continuous" << std::endl;
            }
        }
        catch (const Pylon::GenericException& e) {
            std::cerr << "Warning: Could not set auto exposure: " << e.GetDescription() << std::endl;
        }

        try {
            if (m_camera.GetNodeMap().GetNode("GainAuto")) {
                Pylon::CEnumParameter(m_camera.GetNodeMap(), "GainAuto").SetValue("Continuous");
                std::cout << "Set gain to auto continuous" << std::endl;
            }
        }
        catch (const Pylon::GenericException& e) {
            std::cerr << "Warning: Could not set auto gain: " << e.GetDescription() << std::endl;
        }

        // Debug camera settings before starting grabbing
        DebugCameraSettings();

        // Make sure the thread is not running
        if (m_threadRunning.load() && m_grabThread.joinable())
        {
            m_threadRunning.store(false);
            m_grabThread.join();
        }

        // Start grabbing (continuous mode)
        // Use NewestOnly strategy to ensure we always get the latest image
// In pylon_camera.cpp, update the StartGrabbing method
        m_camera.StartGrabbing(Pylon::GrabStrategy_LatestImageOnly, Pylon::GrabLoop_ProvidedByUser);
        std::cout << "Started continuous grabbing" << std::endl;

        // Start the grab thread
        m_threadRunning.store(true);
        m_grabThread = std::thread(&PylonCamera::GrabThreadFunction, this);

        return true;
    }
    catch (const Pylon::GenericException& e)
    {
        std::cerr << "Error starting grabbing: " << e.GetDescription() << std::endl;
        return false;
    }
}


void PylonCamera::StopGrabbing()
{
    try
    {
        // First, if we have a grab thread running, stop it properly
        if (m_threadRunning.load())
        {
            std::cout << "Stopping grab thread..." << std::endl;
            m_threadRunning.store(false);

            // Wait for thread to complete
            if (m_grabThread.joinable())
            {
                m_grabThread.join();
                std::cout << "Grab thread joined successfully" << std::endl;
            }
        }

        // Then stop camera grabbing
        std::lock_guard<std::mutex> lock(m_cameraMutex);
        if (m_camera.IsGrabbing())
        {
            std::cout << "Stopping camera grabbing..." << std::endl;
            m_camera.StopGrabbing();
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Give it time to stop
        }
    }
    catch (const Pylon::GenericException& e)
    {
        std::cerr << "Error stopping grabbing: " << e.GetDescription() << std::endl;
    }
}

std::string PylonCamera::GetDeviceInfo() const
{
    if (!m_connected || !m_camera.IsPylonDeviceAttached())
    {
        return "No camera connected";
    }

    try
    {
        std::string info = "Camera: " + std::string(m_camera.GetDeviceInfo().GetModelName());
        info += ", S/N: " + std::string(m_camera.GetDeviceInfo().GetSerialNumber());
        return info;
    }
    catch (const Pylon::GenericException& e)
    {
        return "Error getting device info: " + std::string(e.GetDescription());
    }
}

bool PylonCamera::IsConnected() const
{
    return m_connected && !m_deviceRemoved && m_camera.IsPylonDeviceAttached();
}

bool PylonCamera::IsGrabbing() const
{
    return m_connected && !m_deviceRemoved && m_camera.IsGrabbing();
}

bool PylonCamera::IsCameraDeviceRemoved() const
{
    return m_deviceRemoved || (m_connected && m_camera.IsCameraDeviceRemoved());
}

void PylonCamera::HandleDeviceRemoval()
{
    std::cout << "Camera device removal detected!" << std::endl;

    // Set the device removed flag
    m_deviceRemoved = true;

    // Stop the grab thread
    if (m_threadRunning.load())
    {
        m_threadRunning.store(false);
        if (m_grabThread.joinable())
        {
            m_grabThread.join();
        }
    }

    // Update the connection state
    m_connected = false;

    // Call the device removal callback if registered
    if (m_deviceRemovalCallback)
    {
        m_deviceRemovalCallback();
    }
}

bool PylonCamera::TryReconnect()
{
    if (m_reconnecting.exchange(true))
    {
        return false; // Already trying to reconnect
    }

    bool result = false;

    try
    {
        std::cout << "Attempting to reconnect to camera..." << std::endl;

        // Create device info object for the camera we want to reconnect to
        Pylon::CDeviceInfo info;
        info.SetDeviceClass(m_lastDeviceClass);
        info.SetSerialNumber(m_lastDeviceSerialNumber);

        // Create a filter with the device info
        Pylon::DeviceInfoList_t filter;
        filter.push_back(info);

        // Try to find the camera
        Pylon::CTlFactory& tlFactory = Pylon::CTlFactory::GetInstance();
        Pylon::DeviceInfoList_t devices;

        // Destroy the current device since it's no longer valid
        if (m_camera.IsPylonDeviceAttached())
        {
            m_camera.DetachDevice();
        }
        m_camera.DestroyDevice();

        if (tlFactory.EnumerateDevices(devices, filter) > 0)
        {
            // The camera has been found, create and attach it
            m_camera.Attach(tlFactory.CreateDevice(devices[0]));

            // Register the device removal handler again
            m_camera.RegisterConfiguration(m_pDeviceRemovalHandler, Pylon::RegistrationMode_Append, Pylon::Cleanup_None);

            // Reset flags
            m_deviceRemoved = false;
            m_initialized = true;

            // Try to connect to the camera
            m_camera.Open();

            // For GigE cameras, set a reasonable heartbeat timeout for faster device removal detection
            try
            {
                Pylon::CIntegerParameter heartbeat(m_camera.GetTLNodeMap(), "HeartbeatTimeout");
                heartbeat.TrySetValue(1000, Pylon::IntegerValueCorrection_Nearest);  // set to 1000 ms timeout if writable
            }
            catch (...)
            {
                // Ignore if not available for this camera
            }

            m_connected = true;

            // Start grabbing again if we were grabbing before
            if (m_threadRunning.load())
            {
                m_camera.StartGrabbing(Pylon::GrabStrategy_LatestImageOnly);
            }

            std::cout << "Successfully reconnected to camera " << m_camera.GetDeviceInfo().GetModelName() << std::endl;
            result = true;
        }
        else
        {
            std::cout << "Camera not found for reconnection" << std::endl;
        }
    }
    catch (const Pylon::GenericException& e)
    {
        std::cerr << "Error during reconnection attempt: " << e.GetDescription() << std::endl;
    }

    m_reconnecting.store(false);
    return result;
}

void PylonCamera::SetDeviceRemovalCallback(DeviceRemovalCallback callback)
{
    m_deviceRemovalCallback = callback;
}

void PylonCamera::SetNewFrameCallback(NewFrameCallback callback)
{
    m_newFrameCallback = callback;
}

void PylonCamera::GrabThreadFunction()
{
    std::cout << "Grab thread started" << std::endl;
    int frameCounter = 0;

    while (m_threadRunning.load() && m_camera.IsGrabbing())
    {
        try
        {
            // Wait for an image and grab it (timeout 1000ms)
            Pylon::CGrabResultPtr grabResult;

            // Use WaitForFrameTriggerReady instead of RetrieveResult to avoid contention
            if (m_camera.WaitForFrameTriggerReady(1000, Pylon::TimeoutHandling_Return))
            {
                // Now trigger the frame
                m_camera.ExecuteSoftwareTrigger();

                // Retrieve the result
                if (m_camera.RetrieveResult(5000, grabResult, Pylon::TimeoutHandling_Return) &&
                    grabResult && grabResult->GrabSucceeded())
                {
                    // Increment frame counter
                    frameCounter++;

                    // Call the new frame callback if registered
                    if (m_newFrameCallback)
                    {
                        m_newFrameCallback(grabResult);
                    }
                }
            }
        }
        catch (const Pylon::GenericException& e)
        {
            std::cerr << "Error in grab thread: " << e.GetDescription() << std::endl;

            // Check if the camera has been removed
            if (m_camera.IsCameraDeviceRemoved())
            {
                HandleDeviceRemoval();
                break;
            }

            // Add a delay to prevent rapid error messages
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Small sleep to avoid tight loop
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "Grab thread exiting after grabbing " << frameCounter << " frames" << std::endl;
}

void PylonCamera::DebugCameraSettings()
{
    if (!m_connected || !m_camera.IsOpen()) {
        std::cerr << "Cannot debug settings - camera not connected or not open" << std::endl;
        return;
    }

    try {
        std::cout << "\n--- Camera Settings Debug ---" << std::endl;

        // Get camera model info
        std::cout << "Camera Model: " << m_camera.GetDeviceInfo().GetModelName() << std::endl;
        std::cout << "Camera S/N: " << m_camera.GetDeviceInfo().GetSerialNumber() << std::endl;

        // Check acquisition mode
        try {
            if (m_camera.GetNodeMap().GetNode("AcquisitionMode")) {
                Pylon::CEnumParameter acqMode(m_camera.GetNodeMap(), "AcquisitionMode");
                std::cout << "Acquisition Mode: " << acqMode.GetValue() << std::endl;
            }
        }
        catch (...) {
            std::cout << "Acquisition Mode: [Not available]" << std::endl;
        }

        // Check trigger mode
        try {
            if (m_camera.GetNodeMap().GetNode("TriggerMode")) {
                Pylon::CEnumParameter trigMode(m_camera.GetNodeMap(), "TriggerMode");
                std::cout << "Trigger Mode: " << trigMode.GetValue() << std::endl;

                // If trigger is enabled, check trigger source
                if (std::string(trigMode.GetValue()) == "On") {
                    try {
                        Pylon::CEnumParameter trigSrc(m_camera.GetNodeMap(), "TriggerSource");
                        std::cout << "Trigger Source: " << trigSrc.GetValue() << std::endl;
                    }
                    catch (...) {
                        std::cout << "Trigger Source: [Not available]" << std::endl;
                    }
                }
            }
        }
        catch (...) {
            std::cout << "Trigger Mode: [Not available]" << std::endl;
        }

        // Check exposure settings
        try {
            if (m_camera.GetNodeMap().GetNode("ExposureAuto")) {
                Pylon::CEnumParameter expAuto(m_camera.GetNodeMap(), "ExposureAuto");
                std::cout << "Exposure Auto: " << expAuto.GetValue() << std::endl;
            }
        }
        catch (...) {
            std::cout << "Exposure Auto: [Not available]" << std::endl;
        }

        try {
            if (m_camera.GetNodeMap().GetNode("ExposureTime")) {
                Pylon::CFloatParameter expTime(m_camera.GetNodeMap(), "ExposureTime");
                std::cout << "Exposure Time: " << expTime.GetValue() << " μs" << std::endl;
            }
        }
        catch (...) {
            std::cout << "Exposure Time: [Not available]" << std::endl;
        }

        // Check gain settings
        try {
            if (m_camera.GetNodeMap().GetNode("GainAuto")) {
                Pylon::CEnumParameter gainAuto(m_camera.GetNodeMap(), "GainAuto");
                std::cout << "Gain Auto: " << gainAuto.GetValue() << std::endl;
            }
        }
        catch (...) {
            std::cout << "Gain Auto: [Not available]" << std::endl;
        }

        try {
            if (m_camera.GetNodeMap().GetNode("Gain")) {
                Pylon::CFloatParameter gain(m_camera.GetNodeMap(), "Gain");
                std::cout << "Gain: " << gain.GetValue() << " dB" << std::endl;
            }
        }
        catch (...) {
            std::cout << "Gain: [Not available]" << std::endl;
        }

        // Check packet size for GigE cameras
        try {
            if (m_camera.GetTLNodeMap().GetNode("GevSCPSPacketSize")) {
                Pylon::CIntegerParameter packetSize(m_camera.GetTLNodeMap(), "GevSCPSPacketSize");
                std::cout << "Packet Size: " << packetSize.GetValue() << " bytes" << std::endl;
            }
        }
        catch (...) {
            std::cout << "Packet Size: [Not available]" << std::endl;
        }

        // Check max buffer count
        std::cout << "Max Buffer Count: " << m_camera.MaxNumBuffer.GetValue() << std::endl;

        std::cout << "--- End Camera Settings ---\n" << std::endl;
    }
    catch (const Pylon::GenericException& e) {
        std::cerr << "Error getting camera settings: " << e.GetDescription() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error getting camera settings: " << e.what() << std::endl;
    }
}