# Simplified CMakeLists.txt for CMakeProject2 with SDL2 and ImGui
cmake_minimum_required(VERSION 3.12)
project(CMakeProject2)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define paths to SDL2 and ImGui - adjust these if needed
set(SDL2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/sdl2")
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
# Define path to ACSC files
set(ACSC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
# Set SDL2 paths directly - using simple direct references
set(SDL2_INCLUDE_DIR "${SDL2_DIR}/include")

# Handle different platforms for library files
if(WIN32)
    # For Windows, directly set library paths
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        # 64-bit build
        set(SDL2_LIBRARY "${SDL2_DIR}/lib/x64/SDL2.lib")
        set(SDL2MAIN_LIBRARY "${SDL2_DIR}/lib/x64/SDL2main.lib")
        set(SDL2_DLL "${SDL2_DIR}/lib/x64/SDL2.dll")
        set(ACSC_LIBRARY "${ACSC_DIR}/ACSCL_x64.LIB")
    else()
        # 32-bit build
        set(SDL2_LIBRARY "${SDL2_DIR}/lib/x86/SDL2.lib")
        set(SDL2MAIN_LIBRARY "${SDL2_DIR}/lib/x86/SDL2main.lib")
        set(SDL2_DLL "${SDL2_DIR}/lib/x86/SDL2.dll")
        set(ACSC_LIBRARY "${ACSC_DIR}/ACSCL_x86.LIB")
    endif()
else()
    # For Linux/Mac - adjust if needed
    set(SDL2_LIBRARY "${SDL2_DIR}/lib/libSDL2.so")
    set(SDL2MAIN_LIBRARY "")
endif()

# ImGui source files - directly referenced
set(IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

# OpenGL - use basic approach rather than find_package
if(WIN32)
    # Windows OpenGL library is typically named opengl32
    set(OPENGL_LIBRARIES "opengl32.lib")
else()
    # Linux/Mac - typical OpenGL library
    set(OPENGL_LIBRARIES "-lGL")
endif()

# Add source to this project's executable
add_executable(CMakeProject2 
    "CMakeProject2.cpp" 
    "CMakeProject2.h" 
    "random.cpp" 
    "random.h"
    "randomwindow.cpp"
    "randomwindow.h"
    ${IMGUI_SOURCES}
    "tcp_client.h"
    "tcp_client.cpp"
    "client_manager.h"
    "client_manager.cpp"
 "acs_monitor.h" "acs_monitor.cpp")

# Include directories - directly include both SDL2 and ImGui
target_include_directories(CMakeProject2 PRIVATE
    ${SDL2_INCLUDE_DIR}
    ${IMGUI_DIR}
     ${ACSC_DIR}  # Added to include the directory with ACSC.h
)

# Link libraries - directly reference libraries
target_link_libraries(CMakeProject2 PRIVATE
    ${SDL2MAIN_LIBRARY}
    ${SDL2_LIBRARY}
    ${OPENGL_LIBRARIES}
    ${ACSC_LIBRARY}  # Added ACSC library
)

# For Windows, we need to copy the SDL2.dll
if(WIN32)
    # Simple command to copy SDL2.dll
    add_custom_command(TARGET CMakeProject2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SDL2_DLL}
        $<TARGET_FILE_DIR:CMakeProject2>
    )
endif()

# For Windows, set subsystem
if(WIN32)
    if(MSVC)
        set_target_properties(CMakeProject2 PROPERTIES
            LINK_FLAGS "/SUBSYSTEM:CONSOLE"
        )
    endif()
endif()