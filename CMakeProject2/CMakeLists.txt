# Simplified CMakeLists.txt for CMakeProject2 with SDL2, ImGui, Pylon SDK, and PI GCS2 support
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
# Set SDL2 paths directly - using simple direct references

set(SDL2_INCLUDE_DIR "${SDL2_DIR}/include")







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





#set eziio
set(EZIIO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/eziio")
set(EZIIO_DIR_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/external/eziio/include_x86_CPP")
set(EZIIO_DIR_INCLUDE_x64 "${CMAKE_CURRENT_SOURCE_DIR}/external/eziio/include_x64_CPP")


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

# Add PI controller source files
set(PI_SOURCES
    "include/motions/pi_controller.h"
    "include/motions/pi_controller.cpp"
)

# Add source to this project's executable
add_executable(CMakeProject2 
    "CMakeProject2.cpp" 
    "CMakeProject2.h" 
    "Version.h"
    "include/random.cpp" 
    "include/random.h"
    "include/randomwindow.cpp"
    "include/randomwindow.h"
    ${IMGUI_SOURCES}
    "include/tcp_client.h"
    "include/tcp_client.cpp"
    "include/client_manager.h"
    "include/client_manager.cpp"
    "Include/motions/acs_monitor.h" 
    "include/motions/acs_monitor.cpp"
    "include/camera/camera_window.h"
    "include/camera/camera_window.cpp"
    ${PI_SOURCES}  # Add the PI controller sources
    # Add your Pylon-related source files here if needed
    "include/logger.h" 
    "include/logger.cpp" 
    "Include/motions/MotionTypes.h" 
    "Include/motions/MotionConfigManager.h" 
    "include/motions/MotionConfigManager.cpp"     
    "Include/motions/MotionConfigEditor.h" 
    "include/motions/MotionConfigEditor.cpp" 
    "include/ui/toolbar.h" 
    "include/ui/toolbar.cpp" 
    "include/ui/GraphVisualizer.h" 
    "include/ui/GraphVisualizer.cpp" 
    "include/ui/DraggableNode.h" 
    "include/ui/DraggableNode.cpp" 
    "include/camera/pylon_camera.h" 
    "include/camera/pylon_camera.cpp" 
    "include/camera/pylon_camera_test.cpp" 
    "include/camera/pylon_camera_test.h"
    "include/motions/pi_controller_manager.h" 
    "include/motions/pi_controller_manager.cpp" 
    "include/motions/acs_controller.h" 
    "include/motions/acs_controller.cpp" 
 "include/motions/acs_controller_manager.h" 
 "include/motions/acs_controller_manager.cpp" 
 "include/eziio/EziIO_Manager.h" 
 "include/eziio/EziIO_Manager.cpp" 
 "include/eziio/EziIO_UI.h" 
 "include/eziio/EziIO_UI.cpp"
 "IOConfigManager.h"
 "IOConfigManager.cpp"
 "include/data/data_client_manager.h" 
 "include/data/data_client_manager.cpp"
 "include/eziio/PneumaticSlide.h"
 "include/eziio/PneumaticSlide.cpp"
 "include/eziio/PneumaticManager.h"
 "include/eziio/PneumaticManager.cpp"
 "include/eziio/PneumaticUI.h"
 "include/eziio/PneumaticUI.cpp"
 "include/motions/pi_analog_reader.h"
 "include/motions/pi_analog_reader.cpp" 
 "include/motions/pi_analog_manager.h" 
 "include/motions/pi_analog_manager.cpp" 
 "include/motions/motion_control_layer.h" 
 "include/motions/motion_control_layer.cpp"
 "include/ui/ToolbarMenu.h" 
 "include/ui/ToolbarMenu.cpp" 
 "include/ui/ToggleableUIAdapter.h"
 "include/ui/ControllerAdapters.h" 
 "include/ui/DataClientManagerAdapter.h"
 "include/data/product_config_manager.h" 
 "include/data/product_config_manager.cpp"
 "include/eziio/IOControlPanel.h"
 "include/eziio/IOControlPanel.cpp"
 "include/cld101x_client.h"
 "include/cld101x_client.cpp"
 "include/cld101x_manager.h"
 "include/cld101x_manager.cpp"
 "include/python_process_managaer.h"
 "include/motions/global_jog_panel.h"
 "include/motions/global_jog_panel.cpp"
 "include/data/global_data_store.h"
 "include/data/global_data_store.cpp"
 "implot/implot.h"
 "implot/implot_internal.h"
 "implot/implot.cpp"
 "implot/implot_items.cpp"
 "implot/implot_demo.cpp"
 "include/data/DataChartManager.h" 
 "include/data/DataChartManager.cpp"
 "include/scanning/scanning_algorithm.h"
 "include/scanning/scanning_parameters.h"
 "include/scanning/scan_data_collector.h"
 "include/scanning/scanning_ui.h"
 "include/scanning/scanning_algorithm.cpp"
 "include/scanning/scan_data_collector.cpp"
 "include/scanning/scanning_ui.cpp"
 "include/machine_operations.h"
 "include/machine_operations.cpp"
 "include/ProcessStep.h"
 "include/ProcessStep.cpp"
 "include/SequenceStep.h"
 "include/SequenceStep.cpp"
 "InitializationWindow.h"
 "InitializationWindow.cpp"
 "include/HexRightControllerWindow.h"
 "include/HexRightControllerWindow.cpp"
 "include/HexControllerWindow.h"
 "include/HexControllerWindow.cpp"
 "include/ProcessBuilders.h"
 "include/ProcessBuilders.cpp"
 "include/MockUserInteractionManager.h"
 "include/ProcessRunner.cpp"
 "include/ProcessControlPanel.h"
 "include/ProcessControlPanel.cpp"
 "include/cld101x_operations.h"
 "include/cld101x_operations.cpp"
 "PerformanceOverlay.h"
 "FpsDisplay.h" 
 "include/ui/VerticalToolbarMenu.h"
 "include/ui/VerticalToolbarMenu.cpp"
 "include/ui/HierarchicalControllerAdapters.h"
 "include/ui/MotionControlHierarchicalAdapter.h"
 "include/camera/PylonCameraAdapter.h"
 "include/script/script_parser.h"
 "include/script/script_parser.cpp"
 "include/script/script_executor.h"
 "include/script/script_executor.cpp"
 "include/script/script_editor_ui.h"
 "include/script/script_editor_ui.cpp"
 "include/ui/MotionGraphic.h"
 "include/ui/MotionGraphic.cpp"
 "include/script/script_runner.h"
 "include/script/script_runner.cpp" 
 "include/script/ScriptRunnerAdapter.h"
 "include/script/script_ui_manager.h"
 "include/script/print_operation.h"
 "include/script/print_operation.cpp"
 "include/script/script_print_viewer.h"
 "include/script/script_print_viewer.cpp"
 "include/ui/ToolbarStateManager.h" 
 "include/camera/CameraExposureManager.h" 
 "include/camera/CameraExposureManager.cpp" 
 "include/camera/CameraExposureTestUI.h"
 "include/camera/CameraExposureTestUI.cpp" 
 "include/scanning/SequentialOptimizedScanner.h"
 "include/scanning/SequentialOptimizedScanner.cpp"
 "include/scanning/optimized_scanning_ui.h"
 "include/scanning/optimized_scanning_ui.cpp"
 "include/ModuleConfig.h"
 "include/scanning/AdaptivePowerScanner.h" "include/scanning/AdaptivePowerScanner.cpp" "include/scanning/AdaptiveScanningUI.h" "include/scanning/AdaptiveScanningUI.cpp" "Programming/MachineBlockUI.h" "Programming/MachineBlockUI.cpp" "Programming/BlockSequenceConverter.h" "Programming/BlockSequenceConverter.cpp"    "Programming/BlockPropertyRenderers.h" "Programming/BlockPropertyRenderers.cpp" "Programming/virtual_machine_operations.h" "Programming/virtual_machine_operations_adapter.h" "Programming/ProgramManager.h" "Programming/ProgramManager.cpp" "Programming/FeedbackUI.h" "Programming/FeedbackUI.cpp" "Programming/UserPromptUI.h" "Programming/UserPromptUI.cpp" "include/SMU/keithley2400_client.h" "include/SMU/keithley2400_client.cpp" "include/SMU/keithley2400_manager.h" "include/SMU/keithley2400_manager.cpp")




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

# Include directories - directly include SDL2, ImGui, Pylon, and PI GCS2
target_include_directories(CMakeProject2 PRIVATE
    ${SDL2_INCLUDE_DIR}
    ${IMGUI_DIR}
    ${ACSC_DIR}  # Added to include the directory with ACSC.h
    ${PYLON_INCLUDE_DIR}  # Added Pylon include directory
    ${PI_INCLUDE_DIR}  # Added PI GCS2 include directory
    ${EZIIO_DIR_INCLUDE}
    ${EZIIO_DIR_INCLUDE_x64}
    ${SDL2_TTF_INCLUDE_DIR}
)

# Link libraries - directly reference libraries
target_link_libraries(CMakeProject2 PRIVATE
    ${SDL2MAIN_LIBRARY}
    ${SDL2_LIBRARY}
    ${OPENGL_LIBRARIES}
    ${ACSC_LIBRARY}  # ACSC library
    ${PYLON_LIBRARIES}  # Added Pylon libraries
    ${PI_LIBRARY}  # Added PI GCS2 library
    ${EZIIO_LIBRARY}  # Added EziMOTIONPlusE library
    ${SDL2_TTF_LIBRARY}  # Added SDL2_ttf library
    nlohmann_json::nlohmann_json
)

# For Windows, we need to copy the SDL2.dll and PI_GCS2_DLL
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
    
    # Copy PI_GCS2_DLL to the output directory
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        # 64-bit build
        add_custom_command(TARGET CMakeProject2 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PI_LIB_DIR}/PI_GCS2_DLL_x64.dll"
            $<TARGET_FILE_DIR:CMakeProject2>
        )
    else()
        # 32-bit build
        add_custom_command(TARGET CMakeProject2 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PI_LIB_DIR}/PI_GCS2_DLL.dll"
            $<TARGET_FILE_DIR:CMakeProject2>
        )
    endif()

    # Copy ezimotionipluse.dll to the output directory
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        # 64-bit build
        add_custom_command(TARGET CMakeProject2 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EZIIO_DIR_INCLUDE_x64}/EziMOTIONPlusE.dll"
            $<TARGET_FILE_DIR:CMakeProject2>
        )
    else()
        # 32-bit build
        add_custom_command(TARGET CMakeProject2 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EZIIO_DIR_INCLUDE}/EziMOTIONPlusE.dll"
            $<TARGET_FILE_DIR:CMakeProject2>
        )
    endif()

    


endif()

# For Windows, set subsystem
if(WIN32)
    if(MSVC)
        set_target_properties(CMakeProject2 PROPERTIES
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