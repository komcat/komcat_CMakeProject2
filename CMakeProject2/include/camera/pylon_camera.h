#pragma once

#include <pylon/PylonIncludes.h>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>

// Forward declarations
class PylonCamera;

// Configuration event handler for device removal handling
class PylonDeviceRemovalHandler : public Pylon::CConfigurationEventHandler
{
private:
    PylonCamera* m_pOwner;

public:
    PylonDeviceRemovalHandler(PylonCamera* pOwner);
    virtual ~PylonDeviceRemovalHandler();

    // This method is called from a different thread when camera device removal has been detected
    virtual void OnCameraDeviceRemoved(Pylon::CInstantCamera& camera) override;
};

// PylonCamera class that handles camera connection, image acquisition, and device removal
class PylonCamera {
public:
    // Callback type for device removal notification
    using DeviceRemovalCallback = std::function<void()>;

    // Callback type for new frame notification
    using NewFrameCallback = std::function<void(const Pylon::CGrabResultPtr&)>;

public:
    // Constructor
    PylonCamera();

    // Destructor - ensures proper cleanup
    ~PylonCamera();

    // Initialize the camera system - call once before using any other methods
    bool Initialize();

    // Connect to the first available camera
    bool Connect();

    // Connect to a specific camera by serial number
    bool ConnectToSerial(const std::string& serialNumber);

    // Disconnect from the camera
    void Disconnect();

    // Start continuous image acquisition
    bool StartGrabbing();

    // Stop image acquisition
    void StopGrabbing();

    // Get camera information
    std::string GetDeviceInfo() const;

    // Check if camera is connected
    bool IsConnected() const;

    // Check if camera is grabbing
    bool IsGrabbing() const;

    // Check if the camera device has been removed
    bool IsCameraDeviceRemoved() const;

    // Handle device removal - called by the event handler
    void HandleDeviceRemoval();

    // Try to reconnect to a removed camera
    bool TryReconnect();

    // Set a callback for device removal notification
    void SetDeviceRemovalCallback(DeviceRemovalCallback callback);

    // Set a callback for new frames
    void SetNewFrameCallback(NewFrameCallback callback);

    // Get access to the internal camera object (for advanced operations)
    Pylon::CInstantCamera& GetInternalCamera() { return m_camera; }
    const Pylon::CInstantCamera& GetInternalCamera() const { return m_camera; }

    // Debug method to print current camera settings
    void DebugCameraSettings();

private:
    // Grab thread function for continuous image acquisition
    void GrabThreadFunction();

private:
    // Pylon camera object
    Pylon::CInstantCamera m_camera;

    // State flags
    bool m_initialized;
    bool m_connected;
    bool m_deviceRemoved;
    std::atomic<bool> m_reconnecting;

    // Device info for reconnection
    Pylon::String_t m_lastDeviceSerialNumber;
    Pylon::String_t m_lastDeviceClass;

    // Grabbing thread
    std::thread m_grabThread;
    std::atomic<bool> m_threadRunning;
    std::mutex m_cameraMutex;

    // Frame acquisition
    Pylon::CGrabResultPtr m_ptrGrabResult;
    std::atomic<bool> m_newFrameReady;

    // Device removal handler
    PylonDeviceRemovalHandler* m_pDeviceRemovalHandler;

    // Callbacks
    DeviceRemovalCallback m_deviceRemovalCallback;
    NewFrameCallback m_newFrameCallback;

    // Frame rate control
    int m_targetFPS;

    // Friend the handler so it can access private members
    friend class PylonDeviceRemovalHandler;
};