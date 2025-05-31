# Version Management System

This document describes the automated version management system for the Scan Data Visualization project.

## Overview

The project uses a hybrid approach combining:
- **Automatic build number increment** via CMake
- **Manual version control** via PowerShell scripts
- **Real-time version display** in the application window title

The version follows semantic versioning: `MAJOR.MINOR.PATCH.BUILD`

## Files Involved

- `Version.h` - Contains version definitions and utility class
- `update_version.ps1` - PowerShell script for version management  
- `CMakeLists.txt` - CMake configuration with automation
- `VERSION_MANAGEMENT.md` - This documentation

## Version Format

```
MAJOR.MINOR.PATCH.BUILD

Example: 1.2.3.45
```

- **MAJOR**: Breaking changes, major feature releases
- **MINOR**: New features, backward compatible
- **PATCH**: Bug fixes, minor improvements  
- **BUILD**: Auto-incremented on each CMake configure

## Automatic Behavior

### Build Number Auto-Increment
The build number automatically increments every time CMake runs:

```bash
# First CMake run
Version: 1.0.0.1

# Second CMake run  
Version: 1.0.0.2

# Third CMake run
Version: 1.0.0.3
```

### Window Title Display
The application window automatically shows the current version:
```
"Scan Data Visualization v1.0.0 - Debug"
```

## Manual Version Control

### Using PowerShell Script Directly (Recommended)

This is the most reliable method that works from any directory:

```powershell
# Navigate to CMakeProject2 folder first
cd CMakeProject2

# Show current version
.\update_version.ps1 show

# Increment build number  
.\update_version.ps1 build

# Increment patch version (resets build to 0)
.\update_version.ps1 patch

# Increment minor version (resets patch and build to 0)
.\update_version.ps1 minor

# Increment major version (resets minor, patch, and build to 0)
.\update_version.ps1 major

# Set specific version
.\update_version.ps1 set 2 1 5  # Sets to 2.1.5.0
```

**Example Output:**
```powershell
PS CMakeProject2> .\update_version.ps1 major
Current version: 1.0.0.2
Updated to version: 2.0.0.0
Changed: major incremented
```

### Using CMake Targets

**⚠️ Important**: CMake targets work best from Visual Studio. Command line usage requires correct build directory.

**From Visual Studio (Recommended):**
1. Open your project in Visual Studio
2. In Solution Explorer, expand **CMakePredefinedTargets** folder
3. Right-click desired target: `version-build`, `version-patch`, `version-minor`, `version-major`, `version-show`
4. Select **Build**
5. Check Output window for results

**From Command Line (Advanced):**
```bash
# First, find your build directory (usually one of these):
cd out/build/x64-debug           # Visual Studio default
# OR
cd build                         # Standard CMake

# Then run the target:
cmake --build . --target version-show
cmake --build . --target version-patch
cmake --build . --target version-minor
cmake --build . --target version-major

# OR specify build directory directly:
cmake --build out/build/x64-debug --target version-major
```

**Note**: If CMake targets fail with "could not load cache", use the PowerShell script method instead.

## Version Class API

The `Version` class provides these static methods:

```cpp
#include "Version.h"

// Get version strings
std::string version = Version::getVersionString();        // "1.0.0"
std::string fullVer = Version::getFullVersionString();    // "1.0.0.123"
std::string buildInfo = Version::getVersionWithBuildInfo(); // "1.0.0 (Debug, Dec 15 2024)"

// Get window title
std::string title = Version::getWindowTitle();  // "Scan Data Visualization v1.0.0 - Debug"

// Get detailed info
std::string details = Version::getDetailedVersionInfo();
/* 
Scan Data Visualization
Version: 1.0.0.123
Build: Debug (Dec 15 2024 14:30:15)
Branch: main
Commit: abc1234
*/

// Get individual components
int major = Version::getMajor();    // 1
int minor = Version::getMinor();    // 0  
int patch = Version::getPatch();    // 0
int build = Version::getBuild();    // 123
```

## Usage Examples

### In Application Code

```cpp
// Set window title with version
SDL_Window* window = SDL_CreateWindow(
    Version::getWindowTitle().c_str(),
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    1280, 720, window_flags
);

// Log startup version
Logger* logger = Logger::GetInstance();
logger->Log("Application started: " + Version::getVersionWithBuildInfo());

// Display version in about dialog
ImGui::Text("%s", Version::getDetailedVersionInfo().c_str());
```

### Release Workflow

**Recommended Daily Workflow:**
```powershell
# Navigate to CMakeProject2 folder
cd CMakeProject2

# Development builds (automatic)
# Build number increments automatically: 1.0.0.1 -> 1.0.0.2 -> 1.0.0.3

# Bug fix release
.\update_version.ps1 patch    # 1.0.0.15 -> 1.0.1.0

# Feature release  
.\update_version.ps1 minor    # 1.0.1.0 -> 1.1.0.0

# Major release
.\update_version.ps1 major    # 1.1.0.0 -> 2.0.0.0

# Hotfix to specific version
.\update_version.ps1 set 1 0 2  # Emergency patch to 1.0.2.0

# Always verify after changes
.\update_version.ps1 show
```

**Alternative: Visual Studio Method**
1. Open project in Visual Studio
2. Solution Explorer → CMakePredefinedTargets
3. Right-click version target → Build
4. Check Output window for confirmation

## Configuration Options

### CMakeLists.txt Settings

```cmake
# Current behavior: Auto-increment on CMake configure
auto_increment_build()

# Alternative: Auto-increment on every build
# Uncomment this section in CMakeLists.txt for pre-build increment:
# add_custom_command(
#     TARGET CMakeProject2 PRE_BUILD
#     COMMAND ${POWERSHELL_EXE} -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/update_version.ps1" build
#     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#     COMMENT "Auto-incrementing build number before build"
# )
```

### Version.h Customization

To change version display format, edit the `Version` class methods:

```cpp
// Custom window title format
static std::string getWindowTitle() {
    return "MyApp v" + getVersionString() + " Build " + intToString(VERSION_BUILD);
}

// Custom version format
static std::string getVersionString() {
    return intToString(VERSION_MAJOR) + "." + intToString(VERSION_MINOR);  // Just major.minor
}
```

## Troubleshooting

### PowerShell Execution Policy
If you get execution policy errors:
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### CMake Targets Not Working
If CMake command line fails with "could not load cache":

**Problem**: CMake can't find build files in current directory
```powershell
PS CMakeProject2> cmake --build . --target version-major
Error: could not load cache
```

**Solutions**:
1. **Use PowerShell script instead** (recommended):
   ```powershell
   .\update_version.ps1 major
   ```

2. **Find correct build directory**:
   ```powershell
   # Visual Studio usually uses:
   cd out/build/x64-debug
   cmake --build . --target version-major
   
   # Or specify build directory:
   cmake --build out/build/x64-debug --target version-major
   ```

3. **Use Visual Studio interface**:
   - Right-click version targets in Solution Explorer
   - Select "Build"

### CMake Not Finding PowerShell
Ensure PowerShell is in your PATH or update the CMake script:
```cmake
find_program(POWERSHELL_EXE powershell HINTS 
    "C:/Windows/System32/WindowsPowerShell/v1.0/"
    "C:/Program Files/PowerShell/7/"
)
```

### Version Not Updating
1. Check that `update_version.ps1` is in the CMakeProject2 folder
2. Verify `Version.h` exists and is writable  
3. Check CMake output for error messages
4. Run the script manually to test: `.\update_version.ps1 show`

### Build Errors
If you get compilation errors:
1. Ensure `#include "Version.h"` is added to your main file
2. Check that `Version.h` has valid integer values (not `@VERSION_MAJOR@`)
3. Clean and rebuild your project

### Visual Studio Targets Missing
If version targets don't appear in Solution Explorer:
1. **Delete** build cache: Right-click solution → **Delete Cache and Reconfigure**
2. **Check CMakeLists.txt** has the version target definitions
3. **Use PowerShell method** as fallback

## Git Integration (Future Enhancement)

The system is ready for Git integration. To enable:

1. Add Git tag parsing to CMake
2. Use Git commit hash and branch in version display
3. Automatic version bumping based on Git tags

Example Git workflow:
```bash
git tag v1.2.0        # Creates version 1.2.0
git commit -m "fix"   # Auto-increments to 1.2.0.1
```

## Best Practices

### Development
- Let build numbers auto-increment during development
- Use `.\update_version.ps1 show` to check current version before releases
- Don't manually edit version numbers in `Version.h`
- **Prefer PowerShell script** over CMake targets for reliability

### Releases  
- Use `.\update_version.ps1 patch` for bug fixes
- Use `.\update_version.ps1 minor` for new features
- Use `.\update_version.ps1 major` for breaking changes
- Tag Git commits with version numbers
- Always rebuild after version changes

### CI/CD Integration
```yaml
# Example GitHub Actions step
- name: Increment Version
  run: |
    cd CMakeProject2
    ./update_version.ps1 patch
    git add Version.h
    git commit -m "Bump version to $(./update_version.ps1 show)"
```

### Method Preference
1. **PowerShell Script** - Most reliable, works anywhere
2. **Visual Studio** - Good for GUI users, easy to access
3. **CMake Targets** - Advanced users, requires correct build directory

## Version History Tracking

To track version changes, consider:
1. Git tags for releases: `git tag v1.2.3`
2. Changelog file with version entries
3. Release notes with version details

Example changelog entry:
```markdown
## Version 1.2.3 (2024-12-15)
- Fixed camera initialization bug
- Added new scanning algorithm
- Improved UI responsiveness
```