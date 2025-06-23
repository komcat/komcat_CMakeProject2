# How to Add New Pages to Raylib Window

## Overview
This guide shows how to add new pages to the Raylib window system without Windows API conflicts. The approach uses **separate files** with raylib includes only in `.cpp` files.

## Current Working Pages
- **Live Video Page** (V key) - Canvas for picture rendering
- **Menu Page** (M key) - 5 step buttons
- **Status Page** (S key) - System status information

## Step-by-Step Guide to Add a New Page

### 1. Create Header File (`NewPage.h`)

```cpp
// NewPage.h - Clean header with NO raylib includes
#pragma once

// Forward declaration only - no raylib includes in header
class Logger;

class NewPage {
public:
    NewPage(Logger* logger);
    ~NewPage();

    // Simple interface
    void Render();

private:
    Logger* m_logger;
    // Add any non-raylib member variables here
};
```

### 2. Create Implementation File (`NewPage.cpp`)

```cpp
// NewPage.cpp - Implementation with raylib includes ONLY here
#include "NewPage.h"
#include "include/logger.h"

// ONLY include raylib in the .cpp file - NEVER in .h files
#include <raylib.h>

NewPage::NewPage(Logger* logger) : m_logger(logger) {
    if (m_logger) {
        m_logger->LogInfo("NewPage created");
    }
}

NewPage::~NewPage() {
    if (m_logger) {
        m_logger->LogInfo("NewPage destroyed");
    }
}

void NewPage::Render() {
    // Your page content here
    DrawText("New Page Title", 10, 10, 20, DARKBLUE);
    DrawText("Navigation: V: Live | M: Menu | S: Status | N: New Page | ESC: Close", 10, 40, 14, GRAY);
    
    // Add your custom content
    DrawText("This is my new page!", 10, 80, 16, BLACK);
    
    // Example: Add a simple rectangle
    DrawRectangle(100, 120, 200, 100, LIGHTBLUE);
    DrawText("Custom Content", 120, 160, 16, WHITE);
}
```

### 3. Update `raylibclass.cpp`

Add these changes to the main Raylib thread function:

```cpp
// At the top of raylibclass.cpp, add the include:
#include "NewPage.h"  // Add your new page include

// In RaylibThreadFunction(), after creating other pages:
// Create NewPage instance
NewPage newPage(logger);

// Update the enum to include your new page:
enum PageType { LIVE_VIDEO_PAGE, MENU_PAGE, STATUS_PAGE, NEW_PAGE };

// Add key handling in the input section:
if (IsKeyPressed(KEY_N)) {  // Use any available key
    currentPage = NEW_PAGE;
}

// Add rendering in the render section:
else if (currentPage == NEW_PAGE) {
    newPage.Render();  // Use your new page class
}
```

### 4. Update Navigation Text (Optional)

Update the navigation text on other pages to include your new page:

```cpp
// In RenderLiveVideoPage():
DrawText("M: Menu | S: Status | N: New Page | ESC: Close", 10, 40, 14, GRAY);

// In RenderMenuPage():
DrawText("V: Live | S: Status | N: New Page | ESC: Close", 10, 40, 14, GRAY);

// In StatusPage.cpp:
DrawText("V: Live | M: Menu | N: New Page | ESC: Close", 10, 40, 14, GRAY);
```

## Key Rules to Avoid Conflicts

### ✅ DO:
- **Keep raylib includes ONLY in `.cpp` files**
- **Use forward declarations in header files**
- **Create separate files for each page class**
- **Include your new page header in `raylibclass.cpp`**
- **Test each new page individually**

### ❌ DON'T:
- **NEVER include `raylib.h` in any `.h` file**
- **Don't include Windows headers in raylib files**
- **Don't mix raylib types in header files**
- **Don't create complex inheritance with raylib types**

## Example: Adding a Settings Page

### `SettingsPage.h`
```cpp
#pragma once
class Logger;

class SettingsPage {
public:
    SettingsPage(Logger* logger);
    void Render();
private:
    Logger* m_logger;
    bool m_setting1;
    int m_setting2;
};
```

### `SettingsPage.cpp`
```cpp
#include "SettingsPage.h"
#include "include/logger.h"
#include <raylib.h>

SettingsPage::SettingsPage(Logger* logger) 
    : m_logger(logger), m_setting1(false), m_setting2(0) {}

void SettingsPage::Render() {
    DrawText("Settings Page", 10, 10, 20, DARKBLUE);
    
    // Interactive settings
    if (IsKeyPressed(KEY_ONE)) m_setting1 = !m_setting1;
    if (IsKeyPressed(KEY_TWO)) m_setting2++;
    
    DrawText(TextFormat("Setting 1: %s (Press 1 to toggle)", 
             m_setting1 ? "ON" : "OFF"), 10, 80, 16, BLACK);
    DrawText(TextFormat("Setting 2: %d (Press 2 to increment)", 
             m_setting2), 10, 100, 16, BLACK);
}
```

## File Structure
```
project/
├── raylibclass.h          # Main raylib window (no raylib includes)
├── raylibclass.cpp        # Main implementation (raylib includes here)
├── StatusPage.h           # Status page header (clean)
├── StatusPage.cpp         # Status page implementation (raylib here)
├── NewPage.h              # Your new page header (clean)
├── NewPage.cpp            # Your new page implementation (raylib here)
└── include/
    └── logger.h           # Logger interface
```

## Benefits of This Approach

- ✅ **Zero Windows API conflicts**
- ✅ **Clean separation of concerns**
- ✅ **Easy to add new pages**
- ✅ **Maintainable code structure**
- ✅ **No complex inheritance needed**
- ✅ **Each page can be developed independently**

## Testing Your New Page

1. **Compile** - Should build without errors
2. **Run** - Check that your key switches to the new page
3. **Verify** - Make sure navigation back to other pages works
4. **Log Check** - Confirm logger messages appear for page creation

## Troubleshooting

**If you get compilation errors:**
- Check that you didn't include `raylib.h` in any `.h` file
- Verify forward declarations are used in headers
- Make sure the new page is included in `raylibclass.cpp`

**If page doesn't appear:**
- Check the key press handler is added
- Verify the enum includes your new page type
- Confirm the render call is in the correct if/else block

## Summary

This approach allows unlimited page expansion while maintaining clean architecture and avoiding Windows API conflicts. Each page is self-contained and can be developed independently.