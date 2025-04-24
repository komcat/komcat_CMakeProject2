# Simplified CMakeLists.txt for CMakeProject2 with SDL2, ImGui, and Pylon SDK
cmake_minimum_required(VERSION 3.12)
project(CMakeProject2)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

add_subdirectory(nlohmann_json)



# Define paths to SDL2 and ImGui - adjust these if needed
set(SDL2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/sdl2")
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
# Define path to ACSC files
set(ACSC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Set Pylon SDK path - adjust this if your installation is different
set(PYLON_ROOT "C:/Program Files/Basler/pylon 8")
set(PYLON_INCLUDE_DIR "${PYLON_ROOT}/Development/include")
set(PYLON_LIB_DIR "${PYLON_ROOT}/Development/lib/x64")

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
    
    # For Windows, specify the Pylon library paths manually
    set(PYLON_LIBRARIES
        "${PYLON_LIB_DIR}/PylonBase_v9.lib"
        "${PYLON_LIB_DIR}/PylonUtility_v9.lib"
        "${PYLON_LIB_DIR}/GCBase_MD_VC141_v3_1_Basler_pylon.lib"
        "${PYLON_LIB_DIR}/GenApi_MD_VC141_v3_1_Basler_pylon.lib"
    )
    
    # Add PylonGUI for Windows
    set(PYLON_GUI_LIB "${PYLON_LIB_DIR}/PylonGUI_v9.lib")
    list(APPEND PYLON_LIBRARIES ${PYLON_GUI_LIB})
    
    # Define the PYLON_WIN_BUILD macro
    add_definitions(-DPYLON_WIN_BUILD)
else()
    # For Linux/Mac - adjust if needed
    set(SDL2_LIBRARY "${SDL2_DIR}/lib/libSDL2.so")
    set(SDL2MAIN_LIBRARY "")
    
    # For Linux/Mac, Pylon libraries would need to be configured here
    # This is a placeholder - adjust as needed for your Linux/Mac environment
    # set(PYLON_LIBRARIES "")
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
    "acs_monitor.h" 
    "acs_monitor.cpp"
    "camera_window.h"
    "camera_window.cpp"
    # Add your Pylon-related source files here if needed
 "logger.h" "logger.cpp" "MotionTypes.h" "MotionConfigManager.h" "MotionConfigManager.cpp"     "MotionConfigEditor.h" "MotionConfigEditor.cpp" "toolbar.h" "toolbar.cpp" "GraphVisualizer.h" "GraphVisualizer.cpp" "DraggableNode.h" "DraggableNode.cpp")

# Include directories - directly include SDL2, ImGui, and Pylon
target_include_directories(CMakeProject2 PRIVATE
    ${SDL2_INCLUDE_DIR}
    ${IMGUI_DIR}
    ${ACSC_DIR}  # Added to include the directory with ACSC.h
    ${PYLON_INCLUDE_DIR}  # Added Pylon include directory
)

# Link libraries - directly reference libraries
target_link_libraries(CMakeProject2 PRIVATE
    ${SDL2MAIN_LIBRARY}
    ${SDL2_LIBRARY}
    ${OPENGL_LIBRARIES}
    ${ACSC_LIBRARY}  # ACSC library
    ${PYLON_LIBRARIES}  # Added Pylon libraries
    nlohmann_json::nlohmann_json
)

# For Windows, we need to copy the SDL2.dll
if(WIN32)
    # Simple command to copy SDL2.dll
    add_custom_command(TARGET CMakeProject2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SDL2_DLL}
        $<TARGET_FILE_DIR:CMakeProject2>
    )
    
    # Copy Pylon DLLs to the output directory
    add_custom_command(TARGET CMakeProject2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PYLON_ROOT}/Runtime/x64"
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