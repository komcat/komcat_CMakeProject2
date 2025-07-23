# Camera Broadcasting System Documentation

## Overview

The Camera Broadcasting System is a publisher-subscriber pattern implementation that allows multiple UI components to receive camera frames simultaneously without direct camera coupling. This system enables efficient frame distribution from cameras to various display components like debug windows, 3D overlays, and live video panels.

## Architecture

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────────┐
│   Camera        │    │  CameraManager   │    │   Subscribers       │
│   Hardware      │───▶│  Broadcasting    │───▶│  (UI Components)    │
│                 │    │  System          │    │                     │
└─────────────────┘    └──────────────────┘    └─────────────────────┘
```

### Key Components

1. **CameraManager** - Central broadcasting hub
2. **CameraFrameSubscriber** - Interface for receiving frames
3. **Subscriber Implementations** - UI components that display frames
4. **CameraFrameData** - Frame data structure

## Core Interfaces

### CameraFrameSubscriber Interface

All subscribers must implement this interface:

```cpp
class CameraFrameSubscriber {
public:
    virtual void OnNewFrame(const CameraFrameData& frameData) = 0;
    virtual void OnCameraStatusChanged(const std::string& cameraId, bool connected, bool grabbing) = 0;
    virtual std::string GetSubscriberId() const = 0;
    virtual bool WantsFramesFromCamera(const std::string& cameraId) const = 0;
    virtual int GetMinFrameIntervalMs() const = 0;
};
```

### CameraFrameData Structure

```cpp
struct CameraFrameData {
    std::string cameraId;           // Source camera identifier
    uint32_t width, height;         // Frame dimensions
    uint32_t channels;              // Color channels (3=RGB, 1=Grayscale)
    uint64_t timestamp;             // Frame timestamp
    uint64_t frameNumber;           // Sequential frame number
    std::vector<uint8_t> imageData; // Raw image data
    
    bool IsValid() const;           // Validation helper
};
```

## Broadcasting Flow

### 1. Camera Frame Capture

```cpp
// Camera captures frame and triggers callback
void OnCameraFrameReceived(const CameraFrameData& frameData) {
    // Frame captured from hardware
    cameraManager->BroadcastFrame(frameData);
}
```

### 2. Frame Broadcasting

```cpp
// CameraManager distributes frame to all subscribers
void CameraManager::BroadcastFrame(const CameraFrameData& frameData) {
    for (auto& subscriber : subscribers) {
        if (subscriber->WantsFramesFromCamera(frameData.cameraId)) {
            // Check frame rate limiting
            if (ShouldSendFrameToSubscriber(subscriber, frameData)) {
                subscriber->OnNewFrame(frameData);
            }
        }
    }
}
```

### 3. Subscriber Frame Processing

```cpp
// Subscriber receives and processes frame
void LiveVideoSubscriber::OnNewFrame(const CameraFrameData& frameData) {
    // Thread-safe frame storage
    {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        m_latestFrame = frameData;  // Deep copy
    }
    
    // Update atomic flags
    m_hasNewFrame.store(true);
    m_totalFramesReceived.fetch_add(1);
    m_lastFrameTimestamp.store(frameData.timestamp);
}
```

## Subscription Management

### Subscribing to Frames

```cpp
// Create subscriber
auto subscriber = std::make_shared<LiveVideoSubscriber>("main_camera");

// Subscribe to broadcasting system
cameraManager->SubscribeToFrames(subscriber);

// Start broadcasting system
cameraManager->StartBroadcastSystem();
```

### Unsubscribing

```cpp
// Unsubscribe by ID
cameraManager->UnsubscribeFromFrames(subscriber->GetSubscriberId());

// Or clear all subscribers
cameraManager->ClearAllSubscribers();
```

## Subscriber Implementations

### 1. LiveVideoSubscriber

**Purpose**: General-purpose video subscriber for UI displays

**Features**:
- Thread-safe frame buffering
- Frame rate statistics
- Camera status tracking
- Configurable target camera

**Usage**:
```cpp
auto subscriber = std::make_shared<LiveVideoSubscriber>("main_camera");
cameraManager->SubscribeToFrames(subscriber);

// Check for new frames
if (subscriber->HasNewFrame()) {
    auto frame = subscriber->GetLatestFrame();
    subscriber->MarkFrameConsumed();
    // Process frame...
}
```

### 2. CameraFeedDisplay

**Purpose**: OpenGL texture-based display for 3D integration

**Features**:
- OpenGL texture management
- Raylib integration
- Legacy camera support
- Automatic texture updates

**Usage**:
```cpp
auto feedDisplay = std::make_unique<CameraFeedDisplay>();
feedDisplay->SetTargetCamera("main_camera");
cameraManager->SubscribeToFrames(feedDisplay);

// Update texture for rendering
if (feedDisplay->UpdateTexture()) {
    unsigned int textureID = feedDisplay->GetTextureID();
    // Use texture in OpenGL/Raylib...
}
```

### 3. UICameraPanelLiveVideo

**Purpose**: ImGui-based camera control panel

**Features**:
- Camera control buttons
- Live preview display
- Status information
- Broadcasting integration

## Frame Rate Management

### Frame Rate Limiting

Subscribers can specify minimum frame intervals:

```cpp
int GetMinFrameIntervalMs() const override {
    return 33;  // ~30fps limit
}
```

### Adaptive Frame Rates

The system automatically adjusts frame rates based on:
- Subscriber processing capacity
- Network conditions (for remote cameras)
- UI refresh rates

## Threading and Thread Safety

### Thread-Safe Design

- **Frame Data**: Protected by mutexes during copy operations
- **Subscriber Lists**: Thread-safe add/remove operations
- **Atomic Flags**: Used for status indicators
- **Frame Queuing**: Lock-free where possible

### Threading Model

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────────┐
│  Camera Thread  │    │  Broadcasting    │    │  UI Thread          │
│  (Frame Capture)│───▶│  Thread          │───▶│  (Frame Display)    │
│                 │    │  (Distribution)  │    │                     │
└─────────────────┘    └──────────────────┘    └─────────────────────┘
```

## Error Handling

### Camera Disconnection

```cpp
void OnCameraStatusChanged(const std::string& cameraId, bool connected, bool grabbing) {
    if (!connected || !grabbing) {
        // Clear cached frames
        m_hasNewFrame.store(false);
        // Update UI status
        UpdateConnectionStatus(connected, grabbing);
    }
}
```

### Frame Validation

```cpp
bool CameraFrameData::IsValid() const {
    return (width > 0 && height > 0 && 
            channels > 0 && 
            !imageData.empty() &&
            imageData.size() == width * height * channels);
}
```

## Performance Considerations

### Memory Management

- **Frame Copying**: Deep copies ensure thread safety but use more memory
- **Texture Caching**: Reuse OpenGL textures where possible
- **Buffer Reuse**: Minimize allocations in hot paths

### Optimization Strategies

1. **Frame Skipping**: Skip frames if subscriber is behind
2. **Resolution Scaling**: Provide different resolutions to different subscribers
3. **Format Conversion**: Convert formats once, distribute to multiple subscribers
4. **Lazy Updates**: Only update textures when UI is visible

## Configuration

### Camera Setup

```cpp
// Add cameras to manager
auto camera1 = CameraInfo::CreateByIP("main_camera", "192.168.0.68", "Top view camera");
cameraManager->AddCamera(camera1);

// Initialize and start broadcasting
cameraManager->InitializeAllCameras();
cameraManager->StartBroadcastSystem();
```

### Subscriber Configuration

```cpp
// Configure subscriber for specific camera
subscriber->SetTargetCamera("main_camera");
subscriber->SetFrameRateLimit(30.0f);  // 30fps limit
```

## Debugging and Monitoring

### Debug Information

```cpp
// Get subscriber statistics
size_t subscriberCount = cameraManager->GetSubscriberCount();
auto subscriberIds = cameraManager->GetSubscriberIds();

// Monitor frame reception
uint64_t totalFrames = subscriber->GetTotalFramesReceived();
float frameRate = subscriber->GetActualFrameRate();
```

### Logging

The system provides comprehensive logging:
- Subscriber creation/destruction
- Frame reception statistics
- Camera status changes
- Broadcasting system events

## Example Implementation

### Complete Setup Example

```cpp
// 1. Create camera manager and add cameras
auto cameraManager = std::make_unique<CameraManager>();
auto camera1 = CameraInfo::CreateByIP("main_camera", "192.168.0.68", "Main Camera");
cameraManager->AddCamera(camera1);
cameraManager->InitializeAllCameras();

// 2. Create and configure subscriber
auto subscriber = std::make_shared<LiveVideoSubscriber>("main_camera");
cameraManager->SubscribeToFrames(subscriber);

// 3. Start camera and broadcasting
cameraManager->ConnectCamera("main_camera");
cameraManager->StartGrabbing("main_camera");
cameraManager->StartBroadcastSystem();

// 4. Main loop - check for frames
while (running) {
    if (subscriber->HasNewFrame()) {
        auto frame = subscriber->GetLatestFrame();
        subscriber->MarkFrameConsumed();
        
        // Process frame (update UI, save to file, etc.)
        ProcessFrame(frame);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps UI
}

// 5. Cleanup
cameraManager->UnsubscribeFromFrames(subscriber->GetSubscriberId());
cameraManager->StopGrabbingAll();
```

## Migration from Legacy System

### Before (Direct Camera Access)

```cpp
// Old way - direct camera coupling
PylonCameraTest* camera = cameraManager->GetCamera("main_camera");
if (camera && camera->HasValidTexture()) {
    unsigned int textureID = camera->GetTextureID();
    // Use texture...
}
```

### After (Broadcasting System)

```cpp
// New way - subscriber pattern
auto feedDisplay = std::make_unique<CameraFeedDisplay>();
feedDisplay->SetTargetCamera("main_camera");
cameraManager->SubscribeToFrames(feedDisplay);

if (feedDisplay->UpdateTexture()) {
    unsigned int textureID = feedDisplay->GetTextureID();
    // Use texture...
}
```

## Benefits

1. **Decoupling**: UI components don't depend on specific camera implementations
2. **Scalability**: Easy to add new display components
3. **Performance**: Efficient frame distribution to multiple consumers
4. **Flexibility**: Different subscribers can receive different frame rates/formats
5. **Maintainability**: Clear separation of concerns between capture and display

## Best Practices

1. **Always check frame validity** before processing
2. **Implement proper cleanup** in destructors
3. **Use consistent subscriber IDs** for debugging
4. **Handle camera disconnection gracefully**
5. **Monitor frame rates** to detect performance issues
6. **Use appropriate frame rate limits** to avoid overwhelming subscribers

## Troubleshooting

### Common Issues

1. **No frames received**: Check if camera is grabbing and broadcasting is started
2. **Memory leaks**: Ensure proper unsubscription in destructors
3. **Performance issues**: Check frame rate limits and processing time
4. **Texture problems**: Verify OpenGL context and texture management

### Debug Steps

1. Check subscriber count: `cameraManager->GetSubscriberCount()`
2. Verify camera status: `cameraManager->GetCameraStatus(cameraId)`
3. Monitor frame reception: `subscriber->GetTotalFramesReceived()`
4. Check broadcasting: `cameraManager->StartBroadcastSystem()`