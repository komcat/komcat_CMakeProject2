# Version Management - Quick Start Guide

## TL;DR

Your project now has automatic version management! 🎉

- **Window title** shows version automatically
- **Build number** auto-increments on each CMake run
- **Manual control** via PowerShell script or CMake targets

## 5-Minute Setup Verification

### 1. Check Current Version
```powershell
# In CMakeProject2 folder:
.\update_version.ps1 show
```
Output: `Current version: 1.0.0.2`

### 2. Verify Window Title
Build and run your application. Window should show:
```
"Scan Data Visualization v1.0.0 - Debug"
```

### 3. Test Version Update
```powershell
.\update_version.ps1 patch
```
Output: 
```
Current version: 1.0.0.2
Updated to version: 1.0.1.0
Changed: patch incremented
```

### 4. Rebuild and Verify
Rebuild your app. Window title should now show:
```
"Scan Data Visualization v1.0.1 - Debug"
```

## Daily Usage

### For Developers (Recommended Method)
Use PowerShell script directly - most reliable:
```powershell
# Navigate to CMakeProject2 folder
cd CMakeProject2

# Check current version
.\update_version.ps1 show

# Build number auto-increments on CMake runs:
# Day 1: v1.0.0.1 → Day 2: v1.0.0.5 → Day 3: v1.0.0.12
```

### For Releases
```powershell
# In CMakeProject2 folder:

# Bug fix release
.\update_version.ps1 patch    # 1.0.0.x -> 1.0.1.0

# New feature release  
.\update_version.ps1 minor    # 1.0.1.x -> 1.1.0.0

# Major release
.\update_version.ps1 major    # 1.1.0.x -> 2.0.0.0
```

## Visual Studio Integration

**Method 1: Solution Explorer (Best for GUI users)**
Right-click solution → Find these targets in **CMakePredefinedTargets**:
- `version-patch` - Bug fix release
- `version-minor` - Feature release  
- `version-major` - Major release
- `version-show` - Check current version

**Method 2: PowerShell in VS (Most reliable)**
1. **View** → **Terminal** (Ctrl+`)
2. Navigate: `cd CMakeProject2`
3. Run: `.\update_version.ps1 patch`

**Note**: If CMake targets show "could not load cache" error, use Method 2.

## Files You Care About

- `Version.h` - Auto-updated, don't edit manually
- `update_version.ps1` - The magic script
- Window title - Shows current version

## Common Commands

```powershell
# Always run from CMakeProject2 folder first:
cd CMakeProject2

# Show version
.\update_version.ps1 show

# Quick increments
.\update_version.ps1 build     # Build number only
.\update_version.ps1 patch     # Patch version (resets build)
.\update_version.ps1 minor     # Minor version (resets patch & build)
.\update_version.ps1 major     # Major version (resets minor, patch & build)

# Set specific version
.\update_version.ps1 set 2 1 5  # Sets to 2.1.5.0
```

**Example Output:**
```powershell
PS CMakeProject2> .\update_version.ps1 major
Current version: 1.0.0.15
Updated to version: 2.0.0.0
Changed: major incremented
```

## That's It! 

Your version management is now automated. The version will:
- ✅ Show in window title
- ✅ Auto-increment during development  
- ✅ Be controllable for releases
- ✅ Work with your existing build process

Need more details? See [VERSION_MANAGEMENT.md](VERSION_MANAGEMENT.md) for complete documentation.