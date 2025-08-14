# Simplified CMakeLists.txt for CMakeProject2 with SDL2, ImGui, Pylon SDK, and PI GCS2 support
cmake_minimum_required(VERSION 3.12)
project(Project4)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

add_subdirectory(nlohmann_json)

# Define paths to SDL2 and ImGui - adjust these if needed
set(SDL2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/sdl2")
# Set SDL2 paths directly - using simple direct references

set(SDL2_INCLUDE_DIR "${SDL2_DIR}/include")


# Add SQLite configuration
set(SQLITE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/sqlite")
set(SQLITE_INCLUDE_DIR "${SQLITE_DIR}")


# SQLite source files
set(SQLITE_SOURCES
    "${SQLITE_DIR}/sqlite3.c"
)

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui-docking")
# Define path to ACSC files
set(ACSC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")


# Add PI GCS2 paths
set(PI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/PI")
set(PI_INCLUDE_DIR "${PI_ROOT}")
set(PI_LIB_DIR "${PI_ROOT}")

# Set Pylon SDK path - adjust this if your installation is different
set(PYLON_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/pylon 8")
set(PYLON_INCLUDE_DIR "${PYLON_ROOT}/Development/include")
set(PYLON_LIB_DIR "${PYLON_ROOT}/Development/lib/x64")


set(RAYLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/raylib")
set(RAYLIB_INCLUDE_DIR "${RAYLIB_DIR}/include")

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(RAYLIB_LIBRARY "${RAYLIB_DIR}/lib/raylib.lib")
    endif()
endif()


set(FREETYPE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/freetype")
# FreeType paths
set(FREETYPE_INCLUDE_DIR "${FREETYPE_DIR}/include")



#freetype configuration
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)

        # FreeType 2.13.3 libraries - 64-bit
        if(FREETYPE_USE_DLL)
            # DLL version (recommended for easier deployment)
            set(FREETYPE_LIBRARY "${FREETYPE_DIR}/lib/win64/freetype.lib")
            set(FREETYPE_DLL "${FREETYPE_DIR}/lib/win64/freetype.dll")
            message(STATUS "Using FreeType 2.13.3 DLL (64-bit)")
        else()
            # Static version (no DLL needed)
            set(FREETYPE_LIBRARY "${FREETYPE_DIR}/lib/static/vs2015-2022/win64/freetype.lib")
            set(FREETYPE_DLL "")
            message(STATUS "Using FreeType 2.13.3 Static Library (64-bit)")
        endif()
    else()
 
        # FreeType 2.13.3 libraries - 32-bit
        if(FREETYPE_USE_DLL)
            set(FREETYPE_LIBRARY "${FREETYPE_DIR}/lib/win32/freetype.lib")
            set(FREETYPE_DLL "${FREETYPE_DIR}/lib/win32/freetype.dll")
            message(STATUS "Using FreeType 2.13.3 DLL (32-bit)")
        else()
            set(FREETYPE_LIBRARY "${FREETYPE_DIR}/lib/static/vs2015-2022/win32/freetype.lib")
            set(FREETYPE_DLL "")
            message(STATUS "Using FreeType 2.13.3 Static Library (32-bit)")
        endif()
    endif()

else()

    set(FREETYPE_LIBRARY "freetype")
endif()

option(FREETYPE_USE_DLL "Use FreeType DLL instead of static library" ON)



#set eziio
set(EZIIO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/eziio")
set(EZIIO_DIR_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/external/eziio/include_x86_CPP")
set(EZIIO_DIR_INCLUDE_x64 "${CMAKE_CURRENT_SOURCE_DIR}/external/eziio/include_x64_CPP")



# Set HALCON paths based on your installation
set(HALCON_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/Halcon25.05-Progress")
set(HALCON_INCLUDE_DIR "${HALCON_ROOT}/include")



# Handle different platforms for HALCON library files
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        # 64-bit build
        set(HALCON_LIB_DIR "${HALCON_ROOT}/lib/x64-win64")
        set(HALCON_BIN_DIR "${HALCON_ROOT}/bin/x64-win64")
        set(HALCON_LIBRARIES
            "${HALCON_LIB_DIR}/halcon.lib"
            "${HALCON_LIB_DIR}/halconcpp.lib"
        )
    else()
        # 32-bit build (if needed)
        set(HALCON_LIB_DIR "${HALCON_ROOT}/lib/x86sse2-win32")
        set(HALCON_BIN_DIR "${HALCON_ROOT}/bin/x86sse2-win32")
        set(HALCON_LIBRARIES
            "${HALCON_LIB_DIR}/halcon.lib"
            "${HALCON_LIB_DIR}/halconcpp.lib"
        )
    endif()
else()
    # Linux configuration (if needed later)
    set(HALCON_LIB_DIR "${HALCON_ROOT}/lib/x64-linux")
    set(HALCON_LIBRARIES
        "${HALCON_LIB_DIR}/libhalcon.so"
        "${HALCON_LIB_DIR}/libhalconcpp.so"
    )
endif()

# Check if HALCON installation exists
if(NOT EXISTS ${HALCON_ROOT})
    message(WARNING "HALCON not found at ${HALCON_ROOT}. HALCON features will be disabled.")
    set(HALCON_FOUND FALSE)
else()
    message(STATUS "HALCON found at: ${HALCON_ROOT}")
    set(HALCON_FOUND TRUE)
endif()

# Add stb_image path (adjust path as needed)
set(STB_IMAGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/stb")


# Handle different platforms for library files
if(WIN32)
    # For Windows, directly set library paths
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        # 64-bit build
        set(SDL2_LIBRARY "${SDL2_DIR}/lib/x64/SDL2.lib")
        set(SDL2MAIN_LIBRARY "${SDL2_DIR}/lib/x64/SDL2main.lib")
        set(SDL2_DLL "${SDL2_DIR}/lib/x64/SDL2.dll")
        set(ACSC_LIBRARY "${ACSC_DIR}/ACSCL_x64.LIB")
        # Set PI library for 64-bit
        set(PI_LIBRARY "${PI_LIB_DIR}/PI_GCS2_DLL_x64.lib")
        set(EZIIO_LIBRARY "${EZIIO_DIR_INCLUDE_x64}/EziMOTIONPlusE.lib")
    else()
        # 32-bit build
        set(SDL2_LIBRARY "${SDL2_DIR}/lib/x86/SDL2.lib")
        set(SDL2MAIN_LIBRARY "${SDL2_DIR}/lib/x86/SDL2main.lib")
        set(SDL2_DLL "${SDL2_DIR}/lib/x86/SDL2.dll")
        set(ACSC_LIBRARY "${ACSC_DIR}/ACSCL_x86.LIB")
        # Set PI library for 32-bit
        set(PI_LIBRARY "${PI_LIB_DIR}/PI_GCS2_DLL.lib")
        set(EZIIO_LIBRARY "${EZIIO_DIR_INCLUDE}/EziMOTIONPlusE.lib")
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
    
    # Set PI library for Linux
    # set(PI_LIBRARY "${PI_LIB_DIR}/libpi_gcs2_dll.so")
endif()

# ImGui source files - directly referenced (UPDATED TO INCLUDE IMPLOT)
set(IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
    "${IMGUI_DIR}/misc/freetype/imgui_freetype.cpp"  # FreeType support
)



# ADD IMPLOT SOURCES - Add this section after IMGUI_SOURCES
set(IMPLOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/implot")
set(IMPLOT_SOURCES
    "${IMPLOT_DIR}/implot.cpp"
    "${IMPLOT_DIR}/implot_demo.cpp"
    "${IMPLOT_DIR}/implot_items.cpp"
)

# Check if ImPlot exists
if(EXISTS "${IMPLOT_DIR}/implot.cpp")
    message(STATUS "ImPlot found at: ${IMPLOT_DIR}")
    set(IMPLOT_AVAILABLE TRUE)
else()
    message(WARNING "ImPlot not found at: ${IMPLOT_DIR}")
    set(IMPLOT_AVAILABLE FALSE)
    set(IMPLOT_SOURCES "")
endif()


# OpenGL - use basic approach rather than find_package
if(WIN32)
    # Windows OpenGL library is typically named opengl32
    set(OPENGL_LIBRARIES "opengl32.lib")
else()
    # Linux/Mac - typical OpenGL library
    set(OPENGL_LIBRARIES "-lGL")
endif()

# Add PI controller source files
set(PI_SOURCES
    "include/motions/pi_controller.h"
    "include/motions/pi_controller.cpp"
)


# Set IDS uEye SDK paths
set(IDS_UEYE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/uEye")
set(IDS_UEYE_INCLUDE_DIR "${IDS_UEYE_DIR}/include")

# Handle different platforms for IDS uEye library files
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        # 64-bit build
        set(IDS_UEYE_LIB_DIR "${IDS_UEYE_DIR}/lib")
        set(IDS_UEYE_BIN_DIR "${IDS_UEYE_DIR}/bin")
        set(IDS_UEYE_LIBRARY "${IDS_UEYE_LIB_DIR}/ueye_api_64.lib")
        set(IDS_UEYE_DLL "${IDS_UEYE_BIN_DIR}/ueye_api_64.dll")
    else()
        # 32-bit build
        set(IDS_UEYE_LIB_DIR "${IDS_UEYE_DIR}/lib")
        set(IDS_UEYE_BIN_DIR "${IDS_UEYE_DIR}/bin")
        set(IDS_UEYE_LIBRARY "${IDS_UEYE_LIB_DIR}/ueye_api.lib")
        set(IDS_UEYE_DLL "${IDS_UEYE_BIN_DIR}/ueye_api.dll")
    endif()
else()
    # Linux configuration (if needed later)
    set(IDS_UEYE_LIB_DIR "${IDS_UEYE_DIR}/lib")
    set(IDS_UEYE_LIBRARY "${IDS_UEYE_LIB_DIR}/libueye_api.so")
endif()

# Check if IDS uEye installation exists
if(NOT EXISTS ${IDS_UEYE_DIR})
    message(WARNING "IDS uEye SDK not found at ${IDS_UEYE_DIR}. IDS camera features will be disabled.")
    set(IDS_UEYE_FOUND FALSE)
else()
    message(STATUS "IDS uEye SDK found at: ${IDS_UEYE_DIR}")
    set(IDS_UEYE_FOUND TRUE)
endif()


# Set HALCON paths based on your installation
set(MAINUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mainUI")
set(INCL_CAMERA "${CMAKE_CURRENT_SOURCE_DIR}/include/camera")
set(INCL_MOTIONS "${CMAKE_CURRENT_SOURCE_DIR}/include/motions")
set(INCL_DATA "${CMAKE_CURRENT_SOURCE_DIR}/include/data")
set(INCL_OPS "${CMAKE_CURRENT_SOURCE_DIR}/include/ops")
set(INCL_PROGRAM "${CMAKE_CURRENT_SOURCE_DIR}/Programming")
set(INCL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(INCL_PROCESSES "${CMAKE_CURRENT_SOURCE_DIR}/Processes")
set(INCL_Uaa3_PROCESSES "${CMAKE_CURRENT_SOURCE_DIR}/Processes/UAA3ProcessBuilders")
set(INCL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")


file(GLOB_RECURSE P4_SOURCES "src/*.cpp" "src/*.c")
file(GLOB_RECURSE P4_HEADERS "src/*.h" "src/*.hpp")

add_executable(Project4 
${P4_SOURCES} 
${P4_HEADERS}
${IMGUI_SOURCES}
${SQLITE_SOURCES}  # Add SQLite source
${IMPLOT_SOURCES}

 "src/ui/services/data/DataMonitorService.cpp")



# Include directories - directly include SDL2, ImGui, Pylon, and PI GCS2
target_include_directories(Project4 PRIVATE
  ${SDL2_INCLUDE_DIR}
  ${IMGUI_DIR}
  ${ACSC_DIR}  # Added to include the directory with ACSC.h
  ${PYLON_INCLUDE_DIR}  # Added Pylon include directory
  ${PI_INCLUDE_DIR}  # Added PI GCS2 include directory
  ${EZIIO_DIR_INCLUDE}
  ${EZIIO_DIR_INCLUDE_x64}
  ${SDL2_TTF_INCLUDE_DIR}
  ${SQLITE_INCLUDE_DIR}  # Add SQLite include
  ${RAYLIB_INCLUDE_DIR}  # Add this line
  ${HALCON_INCLUDE_DIR}  # Add this line for HALCON
  ${STB_IMAGE_DIR}  # Add this line
  ${MAINUI_DIR}
  ${INCL_CAMERA}
  ${INCL_OPS}
  ${INCL_PROGRAM}
  ${INCL_DIR}
  ${INCL_PROCESSES}
  ${INCL_Uaa3_PROCESSES}
  ${FREETYPE_INCLUDE_DIR}
  ${IDS_UEYE_INCLUDE_DIR}
  ${INCL_SRC}
  ${INCL_DATA}
  ${INCL_MOTIONS}
)

# Link libraries - directly reference libraries
target_link_libraries(Project4 PRIVATE
  ${SDL2MAIN_LIBRARY}
  ${SDL2_LIBRARY}
  ${OPENGL_LIBRARIES}
  ${ACSC_LIBRARY}  # ACSC library
  ${PYLON_LIBRARIES}  # Added Pylon libraries
  ${PI_LIBRARY}  # Added PI GCS2 library
  ${EZIIO_LIBRARY}  # Added EziMOTIONPlusE library
  ${SDL2_TTF_LIBRARY}  # Added SDL2_ttf library
  nlohmann_json::nlohmann_json
  ${HALCON_LIBRARIES}  # Add this line for HALCON
  ${RAYLIB_LIBRARY}  # Add this line
      # Raylib Windows dependencies
  winmm     # For timeBeginPeriod/timeEndPeriod
  gdi32     # For Windows GDI
  user32    # For Windows user interface
  shell32   # For Windows shell
  ${FREETYPE_LIBRARY}
  ${IDS_UEYE_LIBRARY}
)





#-----------------------


# SQLite compile definitions
target_compile_definitions(Project4 PRIVATE
    SQLITE_ENABLE_FTS5
    SQLITE_ENABLE_JSON1
    SQLITE_ENABLE_RTREE
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_COLUMN_METADATA

)


# Add this section to your existing CMakeLists.txt file
# (after your add_executable() statement)

# Create resources directory in the build folder
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/resources")

# Define the source icon file path
set(ICON_SOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.bmp")

# Check if the icon file exists
if(EXISTS "${ICON_SOURCE_PATH}")
    # Copy the icon file to the build directory
    file(COPY "${ICON_SOURCE_PATH}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/resources")
    message(STATUS "Icon file will be copied to build directory")
else()
    message(WARNING "Icon file not found at: ${ICON_SOURCE_PATH}")
endif()



# For Windows, we need to copy the SDL2.dll and PI_GCS2_DLL
if(WIN32)
    # Simple command to copy SDL2.dll
    add_custom_command(TARGET Project4 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SDL2_DLL}
        $<TARGET_FILE_DIR:Project4>
    )
    
    # Copy Pylon DLLs to the output directory
    add_custom_command(TARGET Project4 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PYLON_ROOT}/Runtime/x64"
        $<TARGET_FILE_DIR:Project4>
    )
    
    # Copy PI_GCS2_DLL to the output directory
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        # 64-bit build
        add_custom_command(TARGET Project4 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PI_LIB_DIR}/PI_GCS2_DLL_x64.dll"
            $<TARGET_FILE_DIR:Project4>
        )
    else()
        # 32-bit build
        add_custom_command(TARGET Project4 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PI_LIB_DIR}/PI_GCS2_DLL.dll"
            $<TARGET_FILE_DIR:Project4>
        )
    endif()

    # Copy ezimotionipluse.dll to the output directory
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        # 64-bit build
        add_custom_command(TARGET Project4 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EZIIO_DIR_INCLUDE_x64}/EziMOTIONPlusE.dll"
            $<TARGET_FILE_DIR:Project4>
        )
    else()
        # 32-bit build
        add_custom_command(TARGET Project4 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EZIIO_DIR_INCLUDE}/EziMOTIONPlusE.dll"
            $<TARGET_FILE_DIR:Project4>
        )
    endif()

    


endif()

# For Windows, set subsystem
if(WIN32)
    if(MSVC)
        set_target_properties(Project4 PROPERTIES
            LINK_FLAGS "/SUBSYSTEM:CONSOLE"
        )
    endif()
endif()



# ==== VERSION MANAGEMENT SECTION ====

# Function to auto-increment build number
function(auto_increment_build)
    # Check if PowerShell is available
    find_program(POWERSHELL_EXE powershell HINTS "C:/Windows/System32/WindowsPowerShell/v1.0/")
    
    if(POWERSHELL_EXE)
        # Path to the update script (relative to CMakeProject2 folder)
        set(UPDATE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/update_version.ps1")
        
        # Check if the script exists
        if(EXISTS "${UPDATE_SCRIPT}")
            message(STATUS "Auto-incrementing build number...")
            
            # Execute the PowerShell script to increment build
            execute_process(
                COMMAND ${POWERSHELL_EXE} -ExecutionPolicy Bypass -File "${UPDATE_SCRIPT}" build
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE PS_OUTPUT
                ERROR_VARIABLE PS_ERROR
                RESULT_VARIABLE PS_RESULT
            )
            
            if(PS_RESULT EQUAL 0)
                message(STATUS "Build number incremented successfully")
                # Parse the output to show the new version
                string(REGEX MATCH "Updated to version: ([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)" 
                       VERSION_MATCH "${PS_OUTPUT}")
                if(VERSION_MATCH)
                    message(STATUS "New version: ${CMAKE_MATCH_1}")
                endif()
            else()
                message(WARNING "Failed to increment build number: ${PS_ERROR}")
            endif()
        else()
            message(WARNING "update_version.ps1 not found at: ${UPDATE_SCRIPT}")
        endif()
    else()
        message(WARNING "PowerShell not found - build number will not be auto-incremented")
    endif()
endfunction()

# Auto-increment build number every time CMake runs
auto_increment_build()

# ==== CUSTOM TARGETS FOR MANUAL VERSION CONTROL ====

# Add custom targets for version management
if(POWERSHELL_EXE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/update_version.ps1")
    set(UPDATE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/update_version.ps1")
    
    # Target to increment build number
    add_custom_target(version-build
        COMMAND ${POWERSHELL_EXE} -ExecutionPolicy Bypass -File "${UPDATE_SCRIPT}" build
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Incrementing build number"
    )
    
    # Target to increment patch version
    add_custom_target(version-patch
        COMMAND ${POWERSHELL_EXE} -ExecutionPolicy Bypass -File "${UPDATE_SCRIPT}" patch
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Incrementing patch version"
    )
    
    # Target to increment minor version
    add_custom_target(version-minor
        COMMAND ${POWERSHELL_EXE} -ExecutionPolicy Bypass -File "${UPDATE_SCRIPT}" minor
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Incrementing minor version"
    )
    
    # Target to increment major version
    add_custom_target(version-major
        COMMAND ${POWERSHELL_EXE} -ExecutionPolicy Bypass -File "${UPDATE_SCRIPT}" major
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Incrementing major version"
    )
    
    # Target to show current version
    add_custom_target(version-show
        COMMAND ${POWERSHELL_EXE} -ExecutionPolicy Bypass -File "${UPDATE_SCRIPT}" show
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Showing current version"
    )
    
    message(STATUS "Version management targets added:")
    message(STATUS "  - version-build  : Increment build number")
    message(STATUS "  - version-patch  : Increment patch version") 
    message(STATUS "  - version-minor  : Increment minor version")
    message(STATUS "  - version-major  : Increment major version")
    message(STATUS "  - version-show   : Show current version")
endif()

# ==== OPTIONAL: PRE-BUILD VERSION INCREMENT ====
# Uncomment this section if you want the build number to increment before every build
# (instead of every CMake configure)

# add_custom_command(
#     TARGET CMakeProject2 PRE_BUILD
#     COMMAND ${POWERSHELL_EXE} -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/update_version.ps1" build
#     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#     COMMENT "Auto-incrementing build number before build"
# )

# Copy fonts directory to build output
add_custom_command(TARGET Project4 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory 
    $<TARGET_FILE_DIR:Project4>/assets/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/CascadiaCode-Regular.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/JuliaMono-Regular.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/NotoSans-Regular.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/NotoColorEmoji.ttf"
    $<TARGET_FILE_DIR:Project4>/assets/fonts/
    COMMENT "Copying font files to build directory"
)



# Add this section to your existing CMakeLists.txt file
# (after your existing POST_BUILD commands, before the end of the file)

# Define configuration files to copy
set(CONFIG_FILES
    "camera_calibration.json"
    "camera_exposure_config.json"
    "camera_to_object_offset.json"
    "data_display_config.json"
    "DataServerConfig.json"
    "io_panel_config.json"
    "IOConfig.json"
    "motion_config.json"
    "program.json"
    "script_runner_config.json"
    "smu_config.json"
    "toolbar_state.json"
    "transformation_matrix.json"
    "camera_config.json"
)

# Function to copy config files for a target (copy if not exist or newer)
function(copy_config_files Project4)
    foreach(CONFIG_FILE ${CONFIG_FILES})
        # Method 1: Using copy_if_different (already handles both conditions)
        add_custom_command(TARGET Project4 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_FILE}"
            $<TARGET_FILE_DIR:Project4>/
            COMMENT "Copying ${CONFIG_FILE} if not exist or newer"
        )
        
        # Alternative Method 2: More explicit check (uncomment if you prefer this approach)
        # add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        #     COMMAND ${CMAKE_COMMAND} -E copy_if_different
        #     "${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_FILE}"
        #     "$<TARGET_FILE_DIR:${TARGET_NAME}>/${CONFIG_FILE}"
        #     COMMENT "Copying ${CONFIG_FILE} (if not exist or source is newer)"
        #     DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_FILE}"
        # )
    endforeach()
endfunction()

# Copy configuration files for both executables
copy_config_files(CMakeProject2)
copy_config_files(uaa3App)

# Add verification message
add_custom_command(TARGET Project4 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Configuration files synchronized to build directory"
    COMMENT "Configuration file copy completed"
)

# Copy DLLs for Windows
if(WIN32)
    # Copy SDL2.dll
    add_custom_command(TARGET Project4 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SDL2_DLL}
        $<TARGET_FILE_DIR:Project4>
        COMMENT "Copying SDL2.dll to output directory"
    )
    
    # Copy FreeType DLL if using DLL version
    if(FREETYPE_USE_DLL AND EXISTS "${FREETYPE_DLL}")
        add_custom_command(TARGET Project4 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${FREETYPE_DLL}
            $<TARGET_FILE_DIR:Project4>
            COMMENT "Copying FreeType 2.13.3 DLL to output directory"
        )
        message(STATUS "FreeType DLL will be copied: ${FREETYPE_DLL}")
    elseif(FREETYPE_USE_DLL AND NOT EXISTS "${FREETYPE_DLL}")
        message(WARNING "FreeType DLL not found at: ${FREETYPE_DLL}")
        message(WARNING "Make sure FreeType DLL is in the correct location")
    endif()
endif()

# Add DLL copying for Windows (add this section around line 800, after existing DLL copies)
if(WIN32)
    # Copy IDS uEye DLL to the output directory for CMakeProject2
    add_custom_command(TARGET Project4 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${IDS_UEYE_DLL}
        $<TARGET_FILE_DIR:Project4>
        COMMENT "Copying IDS uEye DLL to CMakeProject2 output directory"
    )
    
    # Copy IDS uEye DLL to the output directory for uaa3App
    add_custom_command(TARGET Project4 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${IDS_UEYE_DLL}
        $<TARGET_FILE_DIR:Project4>
        COMMENT "Copying IDS uEye DLL to uaa3App output directory"
    )
endif()