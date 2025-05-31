# Fabrinet West AAA - CMakeProject2

A comprehensive machine control and data visualization application for industrial automation.

## Overview

This application provides control and monitoring capabilities for:
- PI Hexapod Controllers
- ACS Motion Controllers  
- Pylon Cameras
- IO Systems (EziIO)
- Pneumatic Controls
- Laser Systems (CLD101x)
- Data Acquisition and Visualization

## Quick Start

### Prerequisites
- Windows 10/11
- Visual Studio 2019/2022
- CMake 3.16+
- SDL2
- ImGui
- Required hardware drivers (PI, ACS, Basler Pylon, etc.)

### Building
```bash
git clone <repository-url>
cd CMakeProject2
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Configuration
1. Copy `module_config.ini.example` to `module_config.ini`
2. Edit configuration to enable/disable modules based on available hardware
3. Configure device settings in `motion_config.json`

### Running
```bash
./CMakeProject2.exe
```

## Module Configuration

The application uses a modular design where components can be enabled/disabled via `module_config.ini`:

```ini
[MOTION_CONTROLLERS]
PI_CONTROLLERS=1        # Enable PI hexapod controllers
ACS_CONTROLLERS=1       # Enable ACS gantry controllers

[CAMERAS]
PYLON_CAMERA=1          # Enable Basler Pylon camera
```

See [Module Configuration Guide](docs/MODULE_CONFIG.md) for details.

## Documentation

- [Module Configuration](docs/MODULE_CONFIG.md) - Enable/disable application modules
- [Hardware Setup](docs/HARDWARE_SETUP.md) - Hardware configuration and drivers
- [API Reference](docs/API.md) - Code documentation
- [Troubleshooting](docs/TROUBLESHOOTING.md) - Common issues and solutions

## Project Structure

```
CMakeProject2/
├── docs/                   # Documentation
├── include/               # Header files
│   ├── motions/          # Motion control
│   ├── ui/               # User interface
│   ├── camera/           # Camera control
│   └── data/             # Data management
├── src/                  # Source files
├── resources/            # Application resources
├── configs/              # Configuration files
├── CMakeLists.txt
└── README.md
```

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is proprietary software owned by Fabrinet.

## Support

For technical support, contact the development team or create an issue in this repository.