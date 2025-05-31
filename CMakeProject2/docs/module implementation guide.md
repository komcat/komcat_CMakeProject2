# Module Configuration System Implementation Guide

## Overview
This system allows you to enable/disable modules in your application using an INI configuration file. This is particularly useful for:
- Troubleshooting hardware issues
- Development with missing hardware
- Performance optimization
- Custom application builds

## Implementation Steps

### 1. Add the ModuleConfig Header
1. Create `ModuleConfig.h` in your project
2. Copy the ModuleConfig class code
3. Add `#include "ModuleConfig.h"` to your main CPP file

### 2. Modify Your Main Function
Replace your existing main function with the modified version that:
- Loads the configuration at startup
- Conditionally initializes modules based on settings
- Handles dependencies between modules
- Provides proper cleanup for enabled modules only

### 3. Configuration File
The system automatically creates `module_config.ini` with all modules enabled by default.

## Usage Examples

### Disable Camera for Testing
```ini
[CAMERAS]
PYLON_CAMERA=0
CAMERA_EXPOSURE_TEST=0
```

### Minimal Motion-Only Setup
```ini
[MOTION_CONTROLLERS]
PI_CONTROLLERS=1
ACS_CONTROLLERS=1
MOTION_CONTROL_LAYER=1

[IO_SYSTEMS]
EZIIO_MANAGER=0
PNEUMATIC_SYSTEM=0
IO_CONTROL_PANEL=0

[CAMERAS]
PYLON_CAMERA=0
CAMERA_EXPOSURE_TEST=0

[DATA_SYSTEMS]
DATA_CLIENT_MANAGER=0
DATA_CHART_MANAGER=0
GLOBAL_DATA_STORE=1

[SCANNING_SYSTEMS]
SCANNING_UI_V1=0
OPTIMIZED_SCANNING_UI=0
```

### Development Setup (No Hardware)
```ini
[MOTION_CONTROLLERS]
PI_CONTROLLERS=0
ACS_CONTROLLERS=0
MOTION_CONTROL_LAYER=0

[IO_SYSTEMS]
EZIIO_MANAGER=0
PNEUMATIC_SYSTEM=0
IO_CONTROL_PANEL=0

[CAMERAS]
PYLON_CAMERA=0
CAMERA_EXPOSURE_TEST=0

[LASER_SYSTEMS]
CLD101X_MANAGER=0
PYTHON_PROCESS_MANAGER=0

[USER_INTERFACE]
VERTICAL_TOOLBAR=1
CONFIG_EDITOR=1
GRAPH_VISUALIZER=1
GLOBAL_JOG_PANEL=0
```

## Runtime Configuration
- Access the configuration menu via the main menu bar: Configuration → Module Settings
- View current module states
- Make changes and save (requires restart)
- Changes are automatically saved to the INI file

## Key Features

### Dependency Management
The system automatically handles dependencies:
- If PI_CONTROLLERS is disabled, dependent modules like SCANNING_UI_V1 are also disabled
- If EZIIO_MANAGER is disabled, PNEUMATIC_SYSTEM is automatically disabled
- MachineOperations only initializes if required dependencies are available

### Safe Initialization
- Modules are created as smart pointers (`std::unique_ptr`)
- Only enabled modules are allocated memory
- Proper null checks prevent crashes when accessing disabled modules
- Clean shutdown only affects enabled modules

### Error Handling
- Missing hardware won't crash the application
- Connection failures are logged but don't prevent startup
- Individual module failures don't affect other modules

## Console Output
At startup, the system prints the current configuration:
```
=== Module Configuration ===
PI_CONTROLLERS = ENABLED
ACS_CONTROLLERS = ENABLED
PYLON_CAMERA = DISABLED
...
==========================
```

## Advanced Usage

### Custom Module Groups
You can create custom configurations for different scenarios:

**Production Config** (`production_config.ini`):
- All modules enabled
- Full hardware integration

**Test Config** (`test_config.ini`):
- Hardware modules disabled
- UI and scripting enabled

**Debug Config** (`debug_config.ini`):
- Minimal modules
- Enhanced logging

### Environment-Based Loading
```cpp
std::string configFile = "module_config.ini";
if (const char* env_config = std::getenv("MODULE_CONFIG")) {
    configFile = env_config;
}
ModuleConfig moduleConfig(configFile);
```

## Troubleshooting

### Common Issues
1. **Application won't start**: Check if critical dependencies are disabled
2. **Module not appearing**: Verify the module is enabled and dependencies are met
3. **Configuration not saving**: Check file permissions

### Debug Tips
- Enable console output to see which modules are loading
- Use the runtime configuration UI to verify current states
- Check the log output for connection failures

## Benefits
- **Faster Development**: Skip problematic hardware during development
- **Easier Debugging**: Isolate issues by disabling modules
- **Flexible Deployment**: Different configurations for different machines
- **Resource Management**: Only load what you need
- **Maintenance Friendly**: Easy to disable modules for servicing