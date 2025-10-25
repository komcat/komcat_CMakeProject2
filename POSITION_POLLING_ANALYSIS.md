# Position Polling Analysis - Motion Controllers

## Executive Summary

After reviewing the real-time position polling implementation for both PI and ACS controllers, I found that **the PI controller polling is excessive at 20Hz**, while the ACS controller is more reasonable at 5Hz. There are optimization opportunities to reduce CPU usage, network traffic, and improve overall system efficiency.

## Current Polling Rates

### PI Controller (`pi_controller.cpp`)
- **Update Interval**: 50ms (20Hz)
- **Position Updates**: Every frame (20Hz)
- **Motion Status**: Every frame (20Hz)
- **Servo Status**: Every 3rd frame (~6.7Hz)
- **Analog Readings**: Every 2nd frame (~10Hz, if enabled)

**Code Location**: `CMakeProject2/include/motions/pi_controller.cpp:77`
```cpp
const auto updateInterval = std::chrono::milliseconds(50);  // 20Hz update rate
```

### ACS Controller (`acs_controller.cpp`)
- **Update Interval**: 200ms (5Hz)
- **Position Updates**: Every frame (5Hz)
- **Motion Status**: Every 3rd frame (~1.67Hz)
- **Servo Status**: Every 3rd frame (~1.67Hz)

**Code Location**: `CMakeProject2/include/motions/acs_controller.cpp:70`
```cpp
const auto updateInterval = std::chrono::milliseconds(200);
```

## Issues Identified

### 1. Excessive PI Controller Polling Rate (Critical)

**Problem**: The PI controller polls positions at 20Hz continuously, which is 4x faster than the ACS controller.

**Impact**:
- Higher CPU usage per controller
- Increased network traffic (6-axis position queries every 50ms)
- If multiple PI controllers are connected, each has its own 20Hz polling thread
- Most industrial motion applications only need 5-10Hz for position display

**Evidence**:
```cpp
// PI Controller - pi_controller.cpp:77-94
const auto updateInterval = std::chrono::milliseconds(50);  // 20Hz!

while (!m_terminateThread) {
    if (m_isConnected) {
        // Always update positions - EVERY 50ms
        std::map<std::string, double> positions;
        if (GetPositions(positions)) {
            // ... cache and notify subscribers
        }
    }
}
```

**Recommendation**: Reduce to 100ms (10Hz) or 200ms (5Hz) to match typical industrial HMI update rates.

### 2. Always-On Polling (No Idle Mode)

**Problem**: Both controllers poll continuously whenever connected, regardless of system state.

**Impact**:
- Wasted resources when system is idle
- No reduction in polling when UI windows are closed
- Background threads consuming resources even when no one is monitoring

**Current Behavior**:
```cpp
// Both controllers continuously poll when m_isConnected == true
if (m_isConnected) {
    // Always polling, no idle mode
    GetPositions(positions);
}
```

**Recommendation**: Implement adaptive polling rates:
- **Fast mode (5-10Hz)**: When axes are moving or UI is visible
- **Slow mode (1-2Hz)**: When system is idle and UI is closed
- **Paused mode**: Optional - completely stop polling when no subscribers

### 3. Inconsistent Polling Rates Between Controllers

**Problem**: PI (20Hz) and ACS (5Hz) have 4x difference in polling rates with no clear reason.

**Impact**:
- Confusing for maintenance
- PI position data updates 4x faster than ACS with no apparent benefit
- Different latency characteristics between controller types

**Recommendation**: Standardize both controllers to the same base rate (5-10Hz).

### 4. Servo Status Over-Polling

**Problem**: Servo enable/disable status rarely changes, yet it's polled at 6.7Hz (PI) and 1.67Hz (ACS).

**Impact**:
- Unnecessary queries to hardware
- Servo state typically only changes during initialization or error conditions

**Current Implementation**:
```cpp
// PI Controller - Updates servo status every 3rd frame (6.7Hz)
if (frameCounter % 3 == 0) {
    for (const auto& axis : m_availableAxes) {
        bool enabled;
        if (IsServoEnabled(axis, enabled)) {
            // Cache servo state
        }
    }
}
```

**Recommendation**:
- Only query servo status after servo commands
- Poll at much slower rate (0.5Hz or less) for monitoring
- Implement event-based servo status updates

## Detailed Analysis

### PI Controller Thread Performance

**Per-Frame Operations (50ms interval)**:
1. `GetPositions()` - Queries all 6 axes (X, Y, Z, U, V, W)
2. `IsMoving()` - Checks motion status for all 6 axes
3. Every 2nd frame: `GetAnalogVoltages()` - Reads analog channels (10Hz)
4. Every 3rd frame: `IsServoEnabled()` - Checks servo status for all 6 axes (6.7Hz)

**Network Traffic per Second** (if using TCP/IP):
- Position queries: 20 requests/sec
- Motion status: 20 requests/sec
- Servo status: 6.7 requests/sec
- Analog readings: 10 requests/sec (if enabled)
- **Total**: ~57 queries/second per PI controller

### ACS Controller Thread Performance

**Per-Frame Operations (200ms interval)**:
1. `GetPositions()` - Queries 3 axes (X, Y, Z)
2. Every 3rd frame: `IsMoving()` - Checks motion status for 3 axes (1.67Hz)
3. Every 3rd frame: `IsServoEnabled()` - Checks servo status for 3 axes (1.67Hz)

**Network Traffic per Second**:
- Position queries: 5 requests/sec
- Motion status: 1.67 requests/sec
- Servo status: 1.67 requests/sec
- **Total**: ~8.3 queries/second per ACS controller

**Comparison**: PI controller generates 6.8x more network traffic than ACS controller!

## Positive Findings

### What's Working Well:

1. **Batch Position Queries** ✓
   - Both controllers query all axes in a single call
   - Efficient use of `GetPositions()` instead of per-axis queries

2. **Separate Communication Thread** ✓
   - Non-blocking background updates
   - UI remains responsive

3. **Mutex Protection** ✓
   - Proper thread synchronization for cached values
   - No race conditions observed

4. **Subscriber Pattern** ✓
   - Clean notification system for position updates
   - Multiple subscribers can receive updates efficiently

5. **Staggered Status Updates** ✓
   - Servo and motion status update less frequently than positions
   - Good initial optimization strategy

## Recommended Solutions

### Solution 1: Reduce PI Controller Base Rate (Quick Win)

**Change**: Reduce PI controller polling from 50ms to 100-200ms

**Implementation**:
```cpp
// In pi_controller.cpp:77
const auto updateInterval = std::chrono::milliseconds(100);  // 10Hz (or 200 for 5Hz)
```

**Benefits**:
- 50-75% reduction in network traffic
- Lower CPU usage
- Matches typical industrial HMI refresh rates
- Minimal code change

**Risk**: Low - 10Hz is still very responsive for position display

### Solution 2: Implement Adaptive Polling (Recommended)

**Change**: Add dynamic polling rate based on motion state

**Implementation**:
```cpp
// In pi_controller.h - add member variables
enum class PollingMode { FAST, NORMAL, IDLE };
std::atomic<PollingMode> m_pollingMode{PollingMode::NORMAL};

// In pi_controller.cpp - CommunicationThreadFunc
void PIController::CommunicationThreadFunc() {
    while (!m_terminateThread) {
        // Determine update interval based on mode
        auto updateInterval = std::chrono::milliseconds(200);  // Default: 5Hz

        switch (m_pollingMode.load()) {
            case PollingMode::FAST:
                updateInterval = std::chrono::milliseconds(50);   // 20Hz when moving
                break;
            case PollingMode::NORMAL:
                updateInterval = std::chrono::milliseconds(100);  // 10Hz normal
                break;
            case PollingMode::IDLE:
                updateInterval = std::chrono::milliseconds(500);  // 2Hz when idle
                break;
        }

        if (m_isConnected) {
            // Update positions...

            // Auto-switch to FAST mode if any axis is moving
            bool anyAxisMoving = false;
            for (const auto& [axis, isMoving] : m_axisMoving) {
                if (isMoving) {
                    anyAxisMoving = true;
                    break;
                }
            }

            if (anyAxisMoving) {
                m_pollingMode = PollingMode::FAST;
            } else if (!m_showWindow && m_positionSubscribers.empty()) {
                m_pollingMode = PollingMode::IDLE;  // No UI, no subscribers
            } else {
                m_pollingMode = PollingMode::NORMAL;
            }
        }

        // Wait for next update...
        m_condVar.wait_for(lock, updateInterval, [this]() { return m_terminateThread.load(); });
    }
}
```

**Benefits**:
- Responsive during motion (20Hz)
- Efficient when idle (2Hz)
- Automatically adapts to system state
- No manual configuration needed

### Solution 3: Event-Based Servo Status Updates

**Change**: Only query servo status on-demand or at very low frequency

**Implementation**:
```cpp
// Remove servo status from main loop
// Add method to explicitly update servo status
void PIController::UpdateServoStatus() {
    for (const auto& axis : m_availableAxes) {
        bool enabled;
        if (IsServoEnabled(axis, enabled)) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_axisServoEnabled[axis] = enabled;
        }
    }
}

// Call only when needed:
// - After servo commands (EnableServo)
// - Once every 10 seconds in background thread
// - When UI requests status update
```

**Benefits**:
- Reduces queries from 6.7Hz to ~0.1Hz
- Servo status available when needed
- Less network traffic

### Solution 4: Standardize Polling Rates

**Change**: Make both PI and ACS controllers use the same configurable rate

**Implementation**:
```cpp
// In MotionConfigManager or AppSettings
struct PollingConfig {
    int normalRateMs = 100;      // 10Hz default
    int fastRateMs = 50;         // 20Hz when moving
    int idleRateMs = 500;        // 2Hz when idle
};

// Apply to both PI and ACS controllers
// Read from config file or settings database
```

**Benefits**:
- Consistent behavior across controller types
- Configurable without recompilation
- Easy to tune for different applications

## Performance Impact Estimates

### Current System (with 2 PI + 1 ACS controller):
- **Total queries/second**: (2 × 57) + 8.3 = **122.3 queries/sec**
- **CPU usage**: Moderate (3 threads polling)
- **Network bandwidth**: ~12-25 KB/sec (depending on protocol overhead)

### After Optimization (10Hz for all controllers):
- **Total queries/second**: (2 × 14) + 8.3 = **36.3 queries/sec**
- **Reduction**: 70% fewer queries
- **CPU usage**: Significantly lower
- **Network bandwidth**: ~4-8 KB/sec

### With Adaptive Polling (idle state):
- **Total queries/second**: (2 × 3) + 2 = **8 queries/sec**
- **Reduction**: 93% fewer queries when idle
- **Wake-up latency**: <50ms when motion starts

## Recommendations Priority

### High Priority (Implement Soon)
1. **Reduce PI controller base rate** from 50ms to 100ms (or 200ms)
   - File: `CMakeProject2/include/motions/pi_controller.cpp:77`
   - Change: `std::chrono::milliseconds(50)` → `std::chrono::milliseconds(100)`
   - Impact: Immediate 50% reduction in PI controller load

### Medium Priority (Next Sprint)
2. **Implement adaptive polling** for both controllers
   - Fast when moving, slow when idle
   - Reduces load by 90%+ during idle periods

3. **Reduce servo status polling** to event-based updates
   - Only query after servo commands
   - Background check at 0.1-0.5Hz

### Low Priority (Nice to Have)
4. **Pause polling when no subscribers**
   - Detect when UI windows are closed
   - Stop polling if no data subscribers active

5. **Make polling rates configurable**
   - Add to settings database
   - Allow runtime tuning without recompilation

## Testing Recommendations

When implementing changes, test:

1. **Position Display Accuracy**
   - Verify UI updates smoothly at lower polling rates
   - Check for any "stuttering" or delayed position display

2. **Motion Completion Detection**
   - Ensure motion complete callbacks still fire promptly
   - Test with blocking and non-blocking moves

3. **Multiple Controllers**
   - Test with all controllers connected simultaneously
   - Verify no performance degradation

4. **Subscriber Notifications**
   - Confirm all subscribers receive updates correctly
   - Test with raylib window, ImGui panels, data loggers

5. **Mode Transitions**
   - Test adaptive polling mode switching
   - Verify smooth transition between idle/normal/fast modes

## Conclusion

The **PI controller is currently polling excessively at 20Hz**, while the ACS controller's 5Hz rate is more reasonable. The quickest win is to reduce the PI controller's base rate to 100-200ms (5-10Hz), which will:

- Reduce network traffic by 50-75%
- Lower CPU usage
- Align with typical industrial automation refresh rates
- Have minimal impact on user experience

For a more sophisticated solution, implementing adaptive polling will provide the best balance of responsiveness and efficiency, automatically adjusting to system state.

**No evidence of bugs or functional issues** - the polling works correctly, it's just more frequent than necessary for the PI controllers.

---

## Code Change Reference

### Quick Fix (5 minutes)

**File**: `CMakeProject2/include/motions/pi_controller.cpp`

**Line 77**: Change from:
```cpp
const auto updateInterval = std::chrono::milliseconds(50);  // 20Hz update rate
```

To:
```cpp
const auto updateInterval = std::chrono::milliseconds(100);  // 10Hz update rate (matches industrial HMI standards)
```

Or for even more efficiency:
```cpp
const auto updateInterval = std::chrono::milliseconds(200);  // 5Hz update rate (matches ACS controller)
```

This single-line change will reduce PI controller load by 50-75% with no other code modifications needed.
