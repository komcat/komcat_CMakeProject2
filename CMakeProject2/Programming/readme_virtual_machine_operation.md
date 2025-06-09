# Virtual Machine Operations Documentation

## Overview

The Virtual Machine Operations system provides a comprehensive testing environment for block programming without requiring physical hardware. This allows developers to test machine logic, validate program sequences, and debug automation workflows in a safe, simulated environment.

## Table of Contents

1. [Architecture](#architecture)
2. [Components](#components)
3. [Installation](#installation)
4. [Configuration](#configuration)
5. [Usage](#usage)
6. [API Reference](#api-reference)
7. [Examples](#examples)
8. [Troubleshooting](#troubleshooting)

## Architecture

The virtual operations system uses a **composition-based adapter pattern** to provide the same interface as real machine operations while delegating all calls to virtual implementations.

```
┌─────────────────────────┐
│   MachineBlockUI        │
│   (Block Programming)   │
└──────────┬──────────────┘
           │
           ▼
┌─────────────────────────┐
│ VirtualMachineOps       │
│ Adapter                 │
└──────────┬──────────────┘
           │
           ▼
┌─────────────────────────┐
│ VirtualMachineOps       │
│ Implementation          │
└─────────────────────────┘
```

## Components

### 1. VirtualMachineOperations (`virtual_machine_operations.h`)

**Purpose**: Core virtual implementation that simulates all machine operations

**Key Features**:
- ✅ Motion control simulation with realistic timing
- ✅ IO operations with state tracking
- ✅ Pneumatic control simulation
- ✅ Camera operations mock
- ✅ Scanning functionality simulation
- ✅ Position tracking and reporting
- ✅ Comprehensive logging for all operations

### 2. VirtualMachineOperationsAdapter (`virtual_machine_operations_adapter.h`)

**Purpose**: Adapter that provides `MachineOperations` interface for seamless integration

**Key Features**:
- ✅ Same method signatures as `MachineOperations`
- ✅ Transparent delegation to virtual operations
- ✅ Detailed logging with "VirtualAdapter" prefix
- ✅ Drop-in replacement for real operations

## Installation

### Step 1: Add Virtual Operations Files

Create these files in your project:

1. **`virtual_machine_operations.h`** - Core virtual implementation
2. **`virtual_machine_operations_adapter.h`** - Adapter interface

### Step 2: Update CMakeProject2.cpp

Add includes at the top:
```cpp
#include "virtual_machine_operations.h"
#include "virtual_machine_operations_adapter.h"
```

### Step 3: Modify Machine Operations Initialization

Replace the machine operations section with:
```cpp
// Machine Operations - Real or Virtual based on configuration
std::unique_ptr<MachineOperations> machineOps;
std::unique_ptr<VirtualMachineOperationsAdapter> virtualOps;

bool useRealHardware = moduleConfig.isEnabled("REAL_HARDWARE");

if (useRealHardware && motionControlLayer && piControllerManager &&
    (ioManager || !moduleConfig.isEnabled("EZIIO_MANAGER")) &&
    (pneumaticManager || !moduleConfig.isEnabled("PNEUMATIC_SYSTEM"))) {
    
    // Use real machine operations
    machineOps = std::make_unique<MachineOperations>(
        *motionControlLayer, *piControllerManager,
        ioManager ? *ioManager : *(EziIOManager*)nullptr,
        pneumaticManager ? *pneumaticManager : *(PneumaticManager*)nullptr,
        laserOps.get(), pylonCameraTest.get()
    );
    logger->LogInfo("Real MachineOperations initialized");
}
else {
    // Use virtual machine operations
    virtualOps = std::make_unique<VirtualMachineOperationsAdapter>();
    logger->LogInfo("Virtual MachineOperations initialized");
}
```

## Configuration

### module_config.ini Settings

```ini
[MACHINE_OPERATIONS]
REAL_HARDWARE=0           ; 0 = Virtual, 1 = Real hardware

[USER_INTERFACE]
MACHINE_BLOCK_UI=1        ; Enable block programming interface

# Disable hardware modules for virtual testing
[MOTION_CONTROLLERS]
PI_CONTROLLERS=0
ACS_CONTROLLERS=0
MOTION_CONTROL_LAYER=0

[IO_SYSTEMS]
EZIIO_MANAGER=0
PNEUMATIC_SYSTEM=0

[CAMERAS]
PYLON_CAMERA=0
```

### Switching Between Modes

| Mode | REAL_HARDWARE Setting | Result |
|------|----------------------|--------|
| **Virtual Testing** | `REAL_HARDWARE=0` | Uses virtual operations, safe simulation |
| **Real Hardware** | `REAL_HARDWARE=1` | Uses actual hardware controllers |
| **Debug Only** | No virtual ops setup | Falls back to basic debug simulation |

## Usage

### Basic Block Programming Test

1. **Create a simple program**:
   ```
   START → MOVE_NODE → WAIT → SET_OUTPUT → END
   ```

2. **Configure the blocks**:
   - **MOVE_NODE**: device_name="gantry-main", node_id="node_4027"
   - **WAIT**: milliseconds=1000
   - **SET_OUTPUT**: device_name="IOBottom", pin=1, state=true

3. **Execute the program**:
   - Click "Execute Program" in MachineBlockUI
   - Watch virtual operations execute with realistic timing

### Expected Output

```
🤖 EXECUTING WITH VIRTUAL MACHINE OPERATIONS:
================================================
1. [START] START (ID: 1)
   🟢 Starting program...
   ✅ Block completed successfully

2. [Move Node] MOVE_NODE (ID: 2)
   🏃 Moving gantry-main to node node_4027...
   [VIRTUAL] Moving gantry-main to node node_4027 in graph Process_Flow
   [VIRTUAL] Movement completed successfully
   ✅ Block completed successfully

3. [Wait] WAIT (ID: 3)
   ⏱️  Waiting 1000 ms...
   [VIRTUAL] Waiting for 1000 ms
   ✅ Block completed successfully

4. [Set Output] SET_OUTPUT (ID: 4)
   🔌 Setting output IOBottom pin 1 to HIGH
   [VIRTUAL] Set output 1 on IOBottom to HIGH
   ✅ Block completed successfully

5. [END] END (ID: 5)
   🔴 Program finished
   ✅ Block completed successfully

🎉 [SUCCESS] Virtual program execution completed!
```

## API Reference

### Motion Control Methods

#### `MoveDeviceToNode(deviceName, graphName, targetNodeId, blocking)`
Simulates moving a device to a specific node in the motion graph.

**Parameters**:
- `deviceName` (string): Name of the device to move
- `graphName` (string): Name of the motion graph
- `targetNodeId` (string): Target node ID
- `blocking` (bool): Whether to wait for completion

**Returns**: `bool` - Success status (95% success rate simulation)

**Example**:
```cpp
virtualOps->MoveDeviceToNode("gantry-main", "Process_Flow", "node_4027", true);
```

#### `MoveToPointName(deviceName, positionName, blocking)`
Moves device to a named position with simulated coordinates.

**Predefined Positions**:
- `"home"`: (0, 0, 0, 0, 0, 0)
- `"scan_start"`: (100, 50, 25, 0, 0, 0)
- `"pickup"`: (150, 75, 10, 0, 0, 90)

#### `MoveRelative(deviceName, axis, distance, blocking)`
Performs relative movement on specified axis.

**Supported Axes**: X, Y, Z, U, V, W

### IO Control Methods

#### `SetOutput(deviceName, outputPin, state)`
Sets digital output pin to specified state.

**Parameters**:
- `deviceName` (string): IO device name
- `outputPin` (int): Output pin number
- `state` (bool): HIGH (true) or LOW (false)

#### `ReadInput(deviceName, inputPin, state)`
Reads digital input pin state (simulated random or predefined).

### Pneumatic Control Methods

#### `ExtendSlide(slideName, waitForCompletion, timeoutMs)`
Simulates pneumatic slide extension.

#### `RetractSlide(slideName, waitForCompletion, timeoutMs)`
Simulates pneumatic slide retraction.

**Slide States**: "extended", "retracted", "moving"

### Camera Methods

#### `InitializeCamera()`, `ConnectCamera()`, `CaptureImageToFile()`
Simulates camera operations with realistic timing delays.

### Utility Methods

#### `Wait(milliseconds)`
Simulates waiting with actual thread sleep.

#### `ReadDataValue(dataId, defaultValue)`
Returns mock sensor data:
- `"temperature"`: 23.5-25.5°C
- `"pressure"`: ~1013 mbar
- `"scan_result"`: 0.0-1.0

## Examples

### Example 1: Basic Motion Sequence

```cpp
// Virtual execution of motion sequence
virtualOps->MoveToPointName("gantry-main", "home", true);
virtualOps->MoveRelative("gantry-main", "X", 50.0, true);
virtualOps->MoveToPointName("scanner", "scan_start", true);
```

**Output**:
```
[VIRTUAL] Moving gantry-main to position home
[VIRTUAL] Moved gantry-main to position (0, 0, 0)
[VIRTUAL] Moving gantry-main relative 50mm on X axis
[VIRTUAL] Moving scanner to position scan_start
[VIRTUAL] Moved scanner to position (100, 50, 25)
```

### Example 2: IO and Pneumatic Operations

```cpp
// Control sequence
virtualOps->SetOutput("IOBottom", 1, true);      // Turn on gripper
virtualOps->ExtendSlide("Pick_Up_Tool", true);   // Extend pickup tool
virtualOps->Wait(500);                           // Wait for stabilization
virtualOps->SetOutput("IOBottom", 10, true);     // Turn on vacuum
```

### Example 3: Complete Pick and Place Sequence

```cpp
// Pick and place simulation
virtualOps->MoveToPointName("hex-left", "pickup", true);
virtualOps->ExtendSlide("Pick_Up_Tool", true);
virtualOps->SetOutput("IOBottom", 0, true);      // L_Gripper ON
virtualOps->Wait(200);
virtualOps->RetractSlide("Pick_Up_Tool", true);
virtualOps->MoveToPointName("hex-left", "dropoff", true);
virtualOps->ExtendSlide("Pick_Up_Tool", true);
virtualOps->SetOutput("IOBottom", 0, false);     // L_Gripper OFF
virtualOps->RetractSlide("Pick_Up_Tool", true);
```

## Block Programming Integration

### Supported Block Types

| Block Type | Virtual Operation | Simulation Features |
|-----------|------------------|-------------------|
| **START** | Program initialization | Sets up virtual environment |
| **MOVE_NODE** | `MoveDeviceToNode()` | Realistic timing, success/failure |
| **MOVE_POINT** | `MoveToPointName()` | Position tracking, coordinate simulation |
| **WAIT** | `Wait()` | Actual thread sleep |
| **SET_OUTPUT** | `SetOutput()` | State tracking, logging |
| **CLEAR_OUTPUT** | `SetOutput(false)` | State reset |
| **EXTEND_SLIDE** | `ExtendSlide()` | Pneumatic state simulation |
| **RETRACT_SLIDE** | `RetractSlide()` | Movement timing simulation |
| **END** | Program cleanup | Virtual environment reset |

### Block Parameter Validation

The virtual system validates all block parameters:
- **Device names**: Accepts any string, logs usage
- **Node IDs**: Simulates graph-based navigation
- **Pin numbers**: Tracks IO state per device/pin
- **Timing values**: Enforces minimum/maximum limits

## Troubleshooting

### Common Issues

#### 1. "DEBUG MODE" Instead of Virtual Execution

**Problem**: Block programming shows debug simulation instead of virtual operations.

**Solution**: 
- Verify `REAL_HARDWARE=0` in module_config.ini
- Ensure virtual operations are connected to MachineBlockUI
- Check that `SetVirtualMachineOperations()` is called

#### 2. Missing Virtual Operations Output

**Problem**: No "[VIRTUAL]" log messages appear.

**Solution**:
- Check logger configuration
- Verify VirtualMachineOperationsAdapter is initialized
- Ensure virtual operations aren't being bypassed

#### 3. Block Execution Fails

**Problem**: Virtual operations return false/fail.

**Investigation**:
- Check block parameters (device names, node IDs)
- Verify timing values are reasonable
- Look for simulation success rate (95% default)

### Debug Tips

1. **Enable verbose logging**:
   ```cpp
   virtualOps->GetVirtualOperations()->RunVirtualTest();
   ```

2. **Check initialization**:
   ```
   Virtual MachineOperations initialized for testing
   === VIRTUAL MACHINE OPERATIONS TEST ===
   ```

3. **Monitor block execution**:
   - Each block should show virtual operation calls
   - Timing delays should be realistic
   - Success/failure rates should be reasonable

### Performance Considerations

- **Virtual timing**: Operations use realistic delays (100-1000ms)
- **Memory usage**: Minimal overhead for position/state tracking
- **CPU impact**: Thread sleeps for timing simulation
- **Logging overhead**: Detailed logs may impact performance in large sequences

## Integration with Real Hardware

### Switching to Real Operations

1. **Update configuration**:
   ```ini
   REAL_HARDWARE=1
   PI_CONTROLLERS=1
   ACS_CONTROLLERS=1
   MOTION_CONTROL_LAYER=1
   ```

2. **Connect hardware controllers**

3. **Restart application**

### Validation Workflow

1. **Develop with virtual operations** - Safe, fast iteration
2. **Test complex sequences** - Virtual environment validation
3. **Verify block logic** - Parameter validation, flow control
4. **Switch to real hardware** - Same block programs, real execution
5. **Production deployment** - Tested and validated sequences

## Future Enhancements

### Planned Features

- **📊 Performance Metrics**: Execution timing analysis
- **🎯 Failure Simulation**: Configurable error injection
- **📈 State Visualization**: Real-time position/status display
- **🔄 Sequence Recording**: Capture and replay operations
- **🧪 Unit Testing**: Automated block program validation
- **📋 Report Generation**: Execution summaries and analytics

### Extension Points

- **Custom Devices**: Add new virtual device types
- **Advanced Simulation**: Physics-based motion simulation
- **Network Simulation**: Multi-device coordination
- **Data Integration**: Connect to virtual sensors/databases

---

## Support and Contributing

For questions, issues, or contributions to the Virtual Machine Operations system, please refer to the project documentation or contact the development team.

**Version**: 1.0  
**Last Updated**: December 2024  
**Compatibility**: CMakeProject2 Block Programming System