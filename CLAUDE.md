# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a Windows C++ industrial automation application for controlling robotic assembly systems (UAA3/SAA3/Aurora variants). It provides motion control, vision inspection, I/O management, and process automation through a dual-UI architecture (ImGui + Raylib).

**Key Technologies**: C++20, CMake, Dear ImGui, Raylib, SDL2, OpenGL, SQLite, nlohmann/json, Basler Pylon SDK, MVTec Halcon, PI GCS DLL, ACS SPiiPlus COM SDK

## Build Commands

### Configure and Build (x64 Debug)
```bash
cmake --preset x64-debug
cmake --build out/build/x64-debug
```

### Configure and Build (x64 Release)
```bash
cmake --preset x64-release
cmake --build out/build/x64-release
```

### Build in Visual Studio
The project uses CMake presets compatible with Visual Studio:
1. Open folder in Visual Studio
2. Select preset: `x64-debug` or `x64-release`
3. Build > Build All

### Build Targets
- `uaa3App` - Main application executable
- `test_power_supply` - Power supply testing utility
- `test_power_supply_e36103b` - Keysight E36103B test utility

### Version Management
Build number auto-increments on every CMake configure. Manual version control:
```bash
cd CMakeProject2
.\update_version.ps1 show     # Display current version
.\update_version.ps1 patch    # Increment patch (bug fixes)
.\update_version.ps1 minor    # Increment minor (new features)
.\update_version.ps1 major    # Increment major (breaking changes)
```

### Branch-Specific Builds
The build system auto-detects the git branch and sets compile definitions:
- **SAA_anello/SAA_ANELLO branch**: Defines `BRANCH_SAA_ANELLO` preprocessor macro
- **All other branches**: Standard UAA3/test_services configuration

## Project Structure

```
CMakeProject2/
├── CMakeProject2/              # Main source directory
│   ├── uaa3App.cpp            # Application entry point
│   ├── mainUI/                # ImGui UI components
│   ├── Processes/             # Process definition and management
│   │   ├── UAA3ProcessBuilders/  # UAA3 machine processes
│   │   ├── SAA3ProcessBuilders/  # SAA3 machine processes
│   │   └── AuroraProcesses/      # Aurora machine processes
│   ├── Programming/           # Visual programming system
│   ├── include/               # Core functionality headers/implementations
│   │   ├── motions/          # Motion control layer
│   │   ├── camera/           # Camera management
│   │   ├── ops/              # Operations abstraction
│   │   ├── data/             # Data acquisition and storage
│   │   ├── vision/           # Vision system integration
│   │   ├── SMU/              # Source measurement units
│   │   ├── PowerSupply/      # Power supply integration
│   │   └── ...
│   ├── external/              # Third-party libraries
│   └── *.json                 # Configuration files
└── out/build/                 # CMake build output
```

## Core Architecture

### Application Initialization and Service Registry

**AppContext** (`include/AppContext.h`) - Central service registry providing global access to all subsystems:
- Motion Control: `PIControllerManager`, `ACSControllerManager`, `MotionControlLayer`
- Hardware: `EziIOManager`, `PneumaticManager`, `CLD101xManager` (lasers)
- Vision: `CameraManager`, `CameraConfigManager`, `VisionExposureManager`
- Data: `DatabaseManager`, `OperationResultsManager`, `GlobalDataStore`
- High-Level Operations: `MachineOperations`, `MotionOps`, `IOOps`, `VisionOps`

**ApplicationInitializer** (`include/ApplicationInitializer.h`) - Modular initialization system that:
- Initializes hardware managers in dependency order
- Handles missing hardware gracefully without crashing
- Reports initialization status to UI via callbacks
- Uses `ModuleConfig.h` to enable/disable subsystems via INI file

### Three-Layer Process System

#### Layer 1: Core Operations (`Processes/CoreSequenceStep.h`)
Reusable atomic operations that compose into processes:
- **CorePickPlace**: Pick-and-place with gripper control, optional camera verification
- **CoreInspect**: Vision inspection with camera positioning
- **Movement**: MoveToNode, SetOutput, WaitForInput, etc.

#### Layer 2: Process Builders (`Processes/UAA3ProcessBuilders/`, etc.)
Factory functions that combine core operations into complete processes. Each builder creates a `SequenceStep` containing ordered operations.

Directory structure:
- `UAA3ProcessBuilders/Core/` - Core processes (pick, place, inspect, UV curing)
- `UAA3ProcessBuilders/Calibration/` - Calibration sequences
- `UAA3ProcessBuilders/Dispensing/` - Material dispensing processes
- `SAA3ProcessBuilders/` - SAA3 machine variants
- `AuroraProcesses/` - Aurora machine variants

#### Layer 3: Process Registry (`Processes/ProcessRegistry.h`)
Dynamic process registration system:
- Processes register at startup with metadata (category, description)
- Supports both legacy (hardcoded) and parameterized processes
- Recipe system allows configuring processes via JSON files
- UI automatically discovers all registered processes

### Process Parameterization System

**Parameter Schema** (`Processes/ProcessParameterSchema.h`):
- Defines parameter types: `DEVICE_SELECTION`, `NODE_SELECTION`, `DOUBLE`, `BOOLEAN`, `STRING`
- Provides default values and descriptions
- Validates parameter types at runtime

**Recipe Execution Flow**:
1. User creates recipe in `RecipePageUI` with parameter values
2. Recipe saved as JSON with `ProcessInstance` objects containing parameters
3. `RunPageUI` loads recipe and extracts parameters
4. `ProcessRegistry::BuildProcessWithParameters()` builds process via lambda
5. Parameters injected into core operations

### Operations Abstraction

**IOperations** interface defines standard operations API
**BaseOperations** provides common logging, error handling, initialization

**Specialized Operations**:
- **MotionOps** (`include/ops/motion_ops.h`) - Motion-specific operations (movement, positioning, scanning)
- **IOOps** (`include/ops/io_ops.h`) - Digital I/O and pneumatic control
- **VisionOps** (`include/ops/vision_ops.h`) - Camera control, image capture, exposure management

### Motion Control Architecture

**Three-Tier System**:
1. **Hardware Layer** - Direct communication with motor controllers
   - PI Controllers: Physik Instrumente hexapods (6-DOF precision stages)
   - ACS Controllers: ACS Motion SPiiPlus (multi-axis gantry systems)

2. **Manager Layer** - Device discovery, connection management, command translation
   - `PIControllerManager`, `ACSControllerManager`

3. **Abstraction Layer** - Unified interface for all motion devices
   - `MotionControlLayer` - Unified motion interface
   - `GlobalMotionController` - Coordinate transformations
   - Path planning between graph nodes

**Motion Configuration** (`motion_config.json`):
- **Devices**: Named motion devices (hex-left, hex-right, gantry-main)
- **Positions**: Named XYZ/UVW positions for each device
- **Graphs**: Node-based paths defining valid movements
- **Edges**: Allowed transitions between nodes with movement parameters

### Dual-UI Architecture

**ImGui UI** (Main application) - Docking workspace with panels:
- Hardware control panels (PI, ACS, I/O, Camera, SMU)
- Configuration editors (`MotionConfigEditor`, `GraphVisualizer`)
- Process execution pages (`RunPageUI`, `RecipePageUI`)

**Raylib UI** (Secondary window) - Dedicated visualization:
- Live video feeds
- Motion path visualization
- Status overlays
- **IMPORTANT**: Raylib includes isolated to .cpp files only to avoid Windows API conflicts

### Configuration System

**Configuration Files**:
- `motion_config.json` - Motion devices, positions, graphs
- `camera_config.json` - Camera instances and connections
- `camera_exposure_config.json` - Per-node exposure settings
- `io_panel_config.json` - I/O pin mappings
- `smu_config.json` - SMU device configurations
- `power_supply_config.json` - Power supply settings
- `module_config.ini` - Module enable/disable flags

**Hot Reload**: `ConfigFileWatchdog` monitors config files for changes and reloads without restart

**AppSettings**: SQLite-based settings database with category support for runtime preferences

## Key Data Flows

### Process Execution
```
RunPageUI → BuildSelectedProcess() → SequenceStep created
→ ProcessThreadFunc() executes in background
→ Operations call MachineOperations methods
→ MachineOperations delegates to *Ops classes
→ *Ops call hardware managers
→ Results stored in OperationResultsManager
```

### Motion Command Flow
```
MachineOperations::MoveToNode()
→ MotionControlLayer looks up node in MotionConfigManager
→ Determines device type (PI vs ACS)
→ Calls appropriate manager
→ Manager sends commands to hardware controller
→ Real-time position updates broadcast to subscribers
```

### Vision Inspection Flow
```
Process moves gantry to inspection node
→ VisionCameraExposureManager applies node-specific exposure
→ CameraManager captures image(s)
→ Optional: Vision processing (Halcon)
→ Results stored in GlobalDataStore
→ Live video subscribers receive frames for display
```

## Threading Model

- **Main Thread**: UI rendering (ImGui + Raylib)
- **Process Thread**: Sequential process execution (`RunPageUI::ProcessThreadFunc`)
- **Position Update Thread**: Real-time motion position polling (`MotionControlLayer`)
- **Camera Grab Threads**: Continuous frame acquisition (per-camera)
- **Data Client Threads**: TCP/IP communication with instruments
- **Watchdog Thread**: Configuration file monitoring

**Synchronization**: Mutexes protect shared resources, atomics for status flags, condition variables for blocking operations

## Extension Patterns

### Adding New Processes
1. Define core operations in `Processes/CoreSequenceStep.h`
2. Create builder function in appropriate `*ProcessBuilders` directory
3. Register in `Processes/ProcessInitializer.cpp` with parameter schema
4. Process automatically appears in UI

### Adding New Hardware
1. Implement manager class (e.g., `NewDeviceManager`)
2. Register in `AppContext` (`include/AppContext.h`)
3. Initialize in `ApplicationInitializer` (`include/ApplicationInitializer.cpp`)
4. Add configuration JSON file
5. Create UI adapter for control panel in `mainUI/`

### Adding UI Pages
- **ImGui pages**: Create new UI class in `mainUI/`, add to `MainUIManager`
- **Raylib pages**: Create page class (header clean, raylib includes only in .cpp), add to `raylibclass.cpp`

## Important Coding Patterns

### Error Handling
- Hardware operations return `bool` (success/failure)
- Error messages stored in manager classes via `GetLastError()`
- Operations log errors via `Logger` singleton
- UI displays errors in status area

### Logging
Use `Logger` singleton with caller context:
```cpp
Logger::GetInstance()->LogInfo("Operation started", "ProcessName");
Logger::GetInstance()->LogError("Operation failed: " + error, "ProcessName");
```

### Configuration Validation
- JSON schema validation on config load
- Range checking for numeric parameters
- Device name verification against available hardware
- Graceful fallback to defaults on errors

### Raylib Integration
**CRITICAL**: Never include raylib headers in .h files to avoid Windows API conflicts
- Include raylib.h only in .cpp files
- Forward declare types or use void pointers in headers
- Isolate raylib usage to dedicated window classes

## Key Files for Understanding

1. **Architecture**: `include/AppContext.h`, `include/ApplicationInitializer.cpp`
2. **Process System**: `Processes/ProcessRegistry.h`, `Processes/CoreSequenceStep.h`
3. **Motion**: `include/motions/motion_control_layer.h`, `include/machine_operations.h`
4. **Configuration**: `include/motions/MotionConfigManager.h`, `include/AppSettings.h`
5. **UI**: `mainUI/RunPageUI.h`, `mainUI/MainUIManager.h`
6. **Entry Point**: `CMakeProject2/uaa3App.cpp`

## Common Development Tasks

### Modify Motion Graph
Edit `motion_config.json` → Nodes/Edges section → ConfigFileWatchdog auto-reloads

### Add New Process
1. Add builder function in `Processes/UAA3ProcessBuilders/Core/` (or appropriate directory)
2. Register in `Processes/ProcessInitializer.cpp` using `ProcessRegistry::RegisterProcess()`
3. Define parameters using `ProcessParameterSchema::Define()`
4. Process appears automatically in RunPageUI

### Debug Process Execution
- Enable debug logging in `Logger` settings
- Check `OperationResultsManager` for operation results
- Use `ProcessControlPanel` for step-by-step execution
- Monitor real-time data in `GlobalDataStore`

### Modify Camera Exposure for Node
Edit `camera_exposure_config.json` → Add/modify node-specific settings → Hot-reload active
