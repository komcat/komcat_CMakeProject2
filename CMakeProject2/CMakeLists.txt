# Simplified CMakeLists.txt for uaa3App with SDL2, ImGui, Pylon SDK, and PI GCS2 support
cmake_minimum_required(VERSION 3.12)
project(CMakeProject2)




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
set(FREETYPE_INCLUDE_DIR "${FREETYPE_DIR}/include")

# FreeType configuration
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

# Set eziio
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
    "${IMGUI_DIR}/misc/freetype/imgui_freetype.cpp"
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


# Set VISA paths for PowerSupply module
set(VISA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/visa")
set(VISA_INCLUDE_DIR "${VISA_DIR}/include")

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        # Try 64-bit first, fall back to 32-bit
        if(EXISTS "${VISA_DIR}/Lib_x64/visa64.lib")
            set(VISA_LIBRARY "${VISA_DIR}/Lib_x64/visa64.lib")
        else()
            set(VISA_LIBRARY "${VISA_DIR}/lib/visa32.lib")
        endif()
    else()
        set(VISA_LIBRARY "${VISA_DIR}/lib/visa32.lib")
    endif()
endif()

# Check if VISA exists
if(EXISTS "${VISA_INCLUDE_DIR}/visa.h" AND EXISTS "${VISA_LIBRARY}")
    set(VISA_FOUND TRUE)
    message(STATUS "VISA found at: ${VISA_DIR}")
else()
    set(VISA_FOUND FALSE)
    message(WARNING "VISA not found - PowerSupply test will be disabled")
endif()


# Set include directory paths
set(MAINUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mainUI")
set(INCL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(INCL_TCP "${CMAKE_CURRENT_SOURCE_DIR}/include/tcp")
set(INCL_CLD101x "${CMAKE_CURRENT_SOURCE_DIR}/include/cld101x")
set(INCL_SPLASHSCREEN "${CMAKE_CURRENT_SOURCE_DIR}/include/splashscreen")
set(INCL_CAMERA "${CMAKE_CURRENT_SOURCE_DIR}/include/camera")
set(INCL_DATA "${CMAKE_CURRENT_SOURCE_DIR}/include/data")
set(INCL_MOTIONS "${CMAKE_CURRENT_SOURCE_DIR}/include/motions")
set(INCL_EZIIO "${CMAKE_CURRENT_SOURCE_DIR}/include/eziio")
set(INCL_SMU "${CMAKE_CURRENT_SOURCE_DIR}/include/SMU")
set(INCL_SCANNING "${CMAKE_CURRENT_SOURCE_DIR}/include/scanning")
set(INCL_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/include/script")
set(INCL_POWERSUPPLY "${CMAKE_CURRENT_SOURCE_DIR}/include/PowerSupply")
set(INCL_UI "${CMAKE_CURRENT_SOURCE_DIR}/include/ui")
set(INCLUDE_HALCON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/halcon")
set(INCLUDE_VISION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/vision")
set(INCL_OPS "${CMAKE_CURRENT_SOURCE_DIR}/include/ops")
set(INCL_PRODUCTS "${CMAKE_CURRENT_SOURCE_DIR}/include/Products")
set(INCL_PROGRAM "${CMAKE_CURRENT_SOURCE_DIR}/Programming")

set(INCL_PROCESSES "${CMAKE_CURRENT_SOURCE_DIR}/Processes")
set(INCL_Uaa3_PROCESSES "${CMAKE_CURRENT_SOURCE_DIR}/Processes/UAA3ProcessBuilders")


# Add PowerSupply module
#add_subdirectory(include/PowerSupply)


# Add source to this project's executable
add_executable(uaa3App 
  "uaa3App.cpp" 
  "CMakeProject2.h" 
  "Version.h"
   
  
  
  
  ${IMGUI_SOURCES}
  ${SQLITE_SOURCES}
  "include/tcp/tcp_client.h"
  "include/tcp/tcp_client.cpp"
  "include/tcp/client_manager.h"
  "include/tcp/client_manager.cpp"
  "Include/motions/acs_monitor.h" 
  "include/motions/acs_monitor.cpp"
  "include/camera/camera_window.h"
  "include/camera/camera_window.cpp"
  ${PI_SOURCES}
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
  "include/cld101x/cld101x_client.h"
  "include/cld101x/cld101x_client.cpp"
  "include/cld101x/cld101x_manager.h"
  "include/cld101x/cld101x_manager.cpp"
  "include/python_process_managaer.h"
  "include/motions/global_jog_panel.h"
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
  #"InitializationWindow.h"
  #"InitializationWindow.cpp"
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
  "include/cld101x/cld101x_operations.h"
  "include/cld101x/cld101x_operations.cpp"
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
  "include/scanning/AdaptivePowerScanner.h"
  "include/scanning/AdaptivePowerScanner.cpp"
  "include/scanning/AdaptiveScanningUI.h"
  "include/scanning/AdaptiveScanningUI.cpp"
  "Programming/MachineBlockUI.h"
  "Programming/MachineBlockUI.cpp"
  "Programming/BlockSequenceConverter.h"
  "Programming/BlockSequenceConverter.cpp"
  "Programming/BlockPropertyRenderers.h"
  "Programming/BlockPropertyRenderers.cpp"
  "Programming/virtual_machine_operations.h"
  "Programming/virtual_machine_operations_adapter.h"
  "Programming/ProgramManager.h"
  "Programming/ProgramManager.cpp"
  "Programming/FeedbackUI.h" 
  "Programming/FeedbackUI.cpp"
  "Programming/UserPromptUI.h"
  "Programming/UserPromptUI.cpp"
  "include/SMU/keithley2400_client.h"
  "include/SMU/keithley2400_client.cpp" 
  "include/SMU/keithley2400_manager.h"
  "include/SMU/keithley2400_manager.cpp"
  "include/data/DigitalDisplayWithChart.h"
  "include/data/DigitalDisplayWithChart.cpp"
  "include/SMU/keithley2400_operations.h" 
  "include/SMU/keithley2400_operations.cpp"
  "Programming/MacroManager.h" 
  "Programming/MacroManager.cpp"
  "Programming/MacroEditState.h"
  "mainUI/MenuManager_uaa3.h"
  "include/ui/MenuManager.cpp"
  "include/data/DatabaseManager.h"
  "include/data/OperationResultsManager.h"
  "include/data/DatabaseManager.cpp" 
  "include/data/OperationResultsManager.cpp"
  "include/ui/OperationsDisplayUI.h"
  "include/ui/OperationsDisplayUI.cpp" 
  "raylibclass.h"
  "raylibclass.cpp" 
  "RaylibToggleButton.h"
  "StatusPage.h"
  "StatusPage.cpp" 
  "VisualizePage.h"
  "VisualizePage.cpp"
  "RealtimeChartPage.h" 
  "RealtimeChartPage.cpp"
  "include/siphog/siphog_client.h"
  "include/siphog/siphog_client.cpp"
  "include/halcon/CircleDetectionUI.h" 
  "include/halcon/CircleDetectionUI.cpp"
  "mainUI/MainUIManager.h"
  "mainUI/MainUIManager.cpp"
  "mainUI/UIConfigEditor.h"
  "mainUI/UIConfigEditor.cpp"
  "mainUI/UIConfigEditor_Devices.cpp"
  "mainUI/UIConfigEditor_Positions.cpp"
  "mainUI/UIConfigEditor_Helpers.cpp"
  "mainUI/UIConfigEditor_Graphs.cpp"
  "mainUI/UIConfigEditor_Details.cpp" 
  "mainUI/UIConfigVisualizer.h" 
  "mainUI/UIConfigVisualizer.cpp"
  "mainUI/UIConfigVisualizer_Graph.cpp"
  "mainUI/UIConfigVisualizer_Input.cpp" 
  "mainUI/UIConfigVisualizer_Helpers.cpp"
  "mainUI/NodePropertiesHandler.h" 
  "mainUI/NodePropertiesHandler.cpp" 
  "mainUI/UIJogWindow.h"
  "mainUI/UIJogWindow.cpp"
  "mainUI/PIPanelUI.h"
  "mainUI/PIPanelUI.cpp"
  "mainUI/ACSPanelUI.h" 
  "mainUI/ACSPanelUI.cpp" 
  "mainUI/IOPanelUI.h"
  "mainUI/IOPanelUI.cpp"
  "mainUI/UIPneumaticPanel.h" 
  "mainUI/UIPneumaticPanel.cpp"
  "include/camera/CameraManager.h"
  "include/camera/CameraManager.cpp"
  "mainUI/UICameraPanel.h"
  "mainUI/UICameraPanel.cpp" 
  "mainUI/CameraFeedDisplay.h"
  "mainUI/CameraFeedDisplay.cpp" 
  "mainUI/UICameraPanelLiveVideo.h"
  "mainUI/UICameraPanelSingleGrab.h"
  "mainUI/UICameraPanelUtility.h" 
  "mainUI/UICameraPanelLiveVideo.cpp"
  "mainUI/UICameraPanelSingleGrab.cpp" 
  "mainUI/UICameraPanelUtility.cpp" 
  "include/motions/global_jog_panel_core.cpp"
  "include/motions/global_jog_panel_movement.cpp"
  "include/motions/global_jog_panel_device.cpp" 
  "include/motions/global_jog_panel_ui.cpp" 
  "include/motions/global_jog_panel_ui_positions.cpp" 
  "mainUI/TCPDataManagerUI.h" 
  "mainUI/TCPDataManagerUI.cpp"
  "mainUI/GlobalDataStoreViewerUI.h" 
  "mainUI/GlobalDataStoreViewerUI.cpp"
  "mainUI/CLD101xEquipmentUI.h" 
  "mainUI/CLD101xEquipmentUI.cpp"
  "mainUI/UISMUPanel.h" 
  "mainUI/UISMUPanel.cpp"
  "include/ops/motion_ops.h"
  "include/ops/motion_ops_movement.cpp"
  "include/ops/io_ops.h"
  "include/ops/vision_ops.h"
  "include/ops/motion_ops_acsc_specific.cpp"
  "include/ops/motion_ops_scanning.cpp" 
  "include/ops/motion_ops_position_related.cpp" 
  "include/ops/motion_ops.cpp"
  "include/ops/io_ops.cpp"
  "include/ops/vision_ops.cpp"
  "mainUI/MacroPanelUI.h"
  "mainUI/MacroPanelCameraHandler.h" 
  "mainUI/MacroPanelCameraHandler.cpp"
  "mainUI/MacroPanelUI_Main.cpp"
  "mainUI/MacroPanelUI_Panels.cpp"
  "mainUI/MacroPanelUI_Execution.cpp" 
  "mainUI/MacroPanelUI_FileDialog.cpp"
  "mainUI/ProgramCardRenderer.h"
  "mainUI/ProgramCardRenderer.cpp"
  "mainUI/MenuManager_uaa3.h" 
  "mainUI/MenuManager_uaa3.cpp"
  "mainUI/RaylibDebugWindow.h" 
  "mainUI/RaylibDebugWindow.cpp" 
  "mainUI/LiveVideoSubscriber.h"
  "mainUI/LiveVideoSubscriber.cpp"
  "include/camera/CameraFrameData.h"
  "mainUI/RunPageUI.h" 
  "mainUI/RunPageUI.cpp" 
  "Processes/ProcessFilterManager.h"
  "Processes/ProcessFilterManager.cpp" 
  "Processes/UAA3ProcessBuilders/uaa3_process_builders.h" 
  "Processes/UAA3ProcessBuilders/Core/uaa3_probing_sequence.cpp"
  "Processes/UAA3ProcessBuilders/Core/uaa3_pick_place_sequences.cpp"
  "Processes/UAA3ProcessBuilders/Core/uaa3_uv_curing_sequence.cpp"
  "Processes/UAA3ProcessBuilders/Core/uaa3_reject_sequences.cpp" 
  "Processes/UAA3ProcessBuilders/Calibration/uaa3_needle_calibration.cpp"
  "Processes/UAA3ProcessBuilders/Calibration/uaa3_dispense_calibration.cpp" 
  "Processes/UAA3ProcessBuilders/Dispensing/uaa3_dispense_epoxy.cpp"
  "Processes/UAA3ProcessBuilders/uaa3_utility_functions.cpp" 
  "Processes/UAA3ProcessBuilders/Core/uaa3_initialzation_sequences.cpp"
  "include/camera/CameraConfigManager.h" 
  "include/camera/CameraConfigManager.cpp"
  "mainUI/UAA3DatabaseManager.h"
  "mainUI/UAA3DatabaseManager.cpp"
  "mainUI/UAA3DatabaseManager_JsonOps.cpp" 
  "mainUI/UAA3DatabaseManager_Utils.cpp"
  "mainUI/UAA3DatabaseManager_Global.cpp" 
  "mainUI/ConfigDatabaseUtils.h" 
  "mainUI/ConfigDatabaseUtils.cpp"
  "mainUI/ConfigFileWatchDog.h"
  "mainUI/ConfigFileWatchdog.cpp"
  "Processes/ProcessRegistry.h"
  "Processes/ProcessInitializer.cpp"
  "Processes/NewProcesses_Template.cpp"
  "Processes/SAA3ProcessBuilders/NewProcesses_SAA3.cpp"
  "Processes/SAA3ProcessBuilders/NewProcesses_SAA3.h" 
  "include/camera/ids_camera_test.h" 
  "include/camera/ids_camera_test.cpp"
  "include/camera/IDSCameraUI.h" 
  "include/camera/IDSCameraUI.cpp"
  "include/camera/ICameraHardware.h" 
  "include/camera/PylonCameraAdapter.cpp" 
  "include/camera/IDSCameraAdapter.h" 
  "include/camera/IDSCameraAdapter.cpp" 
  "include/camera/CameraHardwareFactory.h" 
  "include/camera/CameraHardwareFactory.cpp" 
  "mainUI/UIVisionPanel.h"  
  "include/halcon/VisionCircleDetection.h" 
  "include/halcon/VisionCircleDetection.cpp" 
  "mainUI/UINodeListWindow.h" 
  "mainUI/UINodeListWindow.cpp" 
  "include/vision/VisionPresetManager.h" 
  "include/vision/VisionPresetManager.cpp" 
  "mainUI/UIVisionPanel_Main.cpp" 
  "mainUI/UIVisionPanel_Initialization.cpp" 
  "mainUI/UIVisionPanel_Camera.cpp" 
  "mainUI/UIVisionPanel_Detection.cpp" 
  "mainUI/UIVisionPanel_Parameters.cpp" 
  "mainUI/UIVisionPanel_Presets.cpp" 
  "mainUI/UIVisionPanel_AutoExecution.cpp" 
  "mainUI/UIVisionPanel_Nodes.cpp" 
  "mainUI/UIVisionPanel_Image.cpp" 
  "mainUI/UIVisionPanel_NodePresets.cpp" 
  "mainUI/UIVisionPanel_SaveImages.cpp" 
  "include/Products/ProductReferenceManager.h" 
  "mainUI/DatumUI.h" 
  "include/Products/ProductReferenceManager.cpp"  
  "mainUI/DatumUI_Main.cpp" 
  "mainUI/DatumUI_LeftPanel.cpp" 
  "mainUI/DatumUI_MiddlePanel.cpp" 
  "mainUI/DatumUI_RightPanel.cpp" 
  "mainUI/DatumUI_Visual.cpp" 
  "mainUI/DatumUI_Events.cpp" 
  "mainUI/DatumUI_Utils.cpp" 
  "mainUI/DatumUI_Dialogs.cpp" 
  "include/Products/ProductReferenceManager_Products.cpp" 
  "include/Products/ProductReferenceManager_Points.cpp" 
  "include/Products/ProductReferenceManager_Edges.cpp" 
  "include/Products/ProductReferenceManager_Datum.cpp" 
  "include/Products/ProductReferenceManager_Validation.cpp" 
  "include/Products/ProductReferenceManager_Utils.cpp" 
  "include/Products/ProductReferenceManager_Calculations.cpp" 
  "include/Products/ProductReferenceManager_Persistence.cpp"  
  "mainUI/VisionCoordinateCalculator.h" 
  "mainUI/VisionCoordinateCalculator.cpp" 
  "mainUI/UIVisionPanel_EnhancedResults.cpp" 
  "include/Products/ModuleAlignment.h" 
  "include/Products/ModuleAlignment.cpp" 
  "mainUI/ModuleAlignmentUI.h" 
  "mainUI/ModuleAlignmentUI.cpp" 
  "include/AppContext.h" 
  "include/ops/IOperations.h" 
  "include/ops/BaseOperations.h" 
 "RaylibCameraManager.h"
 "RaylibCameraManager.cpp"
 "include/AppContext.cpp"
 
 "Processes/AuroraProcesses/NewProcesses_Aurora.h"
 "Processes/AuroraProcesses/NewProcesses_Aurora.cpp"
  "include/data/DUTDataRecorder.h"
  "include/data/DUTDataRecorder.cpp"

   "include/DUTOperations.cpp"
   "include/ops/UserInputOperations.h"
   "include/ops/UserInputOperation.cpp"
 "include/data/SQLiteConnection.h"
 "include/data/SQLiteConnection.cpp"
 "include/ApplicationInitializer.h"
 "include/ApplicationInitializer.cpp"
 "include/ConsoleProgressDisplay.h" 
 "include/splashscreen/SimpleSplashScreen.h"
 "include/splashscreen/SimpleSplashScreen.cpp" 
 "include/splashscreen/BitmapFont.h"
 "include/splashscreen/BitmapFont.cpp"
 "include/ops/ManualAdjustmentOperation.h"
 "include/ops/ManualAdjustmentOperation.cpp" 
 "include/PowerSupply/SPDPowerSupply.cpp"
 "include/PowerSupply/SPDPowerSupply.h"
 "include/PowerSupply/SPDPowerSupplyManager.cpp"
 "include/PowerSupply/SPDPowerSupplyManager.h"
  "mainUI/UISPDPowerPanel.h"
  "mainUI/UISPDPowerPanel.cpp"
  "include/data/IDataProvider.h"
  "include/data/SPDDataProviderAdapter.h"
  "mainUI/SPDPowerSupplyUI.h" 
  "mainUI/SPDPowerSupplyUI.cpp" 
 "include/PowerSupply/SPD_Ops.h" 
 "include/PowerSupply/SPD_Ops.cpp"
 "include/PowerSupply/SPDOperations.h"
 "include/PowerSupply/SPDOperations.cpp"
 "include/ACSC_wrapper.h"
 "include/motions/MotionDataSubscriber.h"
  "include/motions/MotionDataSubscriber.cpp"
  "include/eziio/EziIO_Observer.h"
  "include/eziio/EziIO_Observer.cpp"   
  "include/data/data_subscriber_interface.h"
  "include/data/DataClient_Ops.h"
  "include/data/DataClient_Ops.cpp"
  "include/scanning/grid_scanner.h"
  "include/scanning/grid_scanner.cpp"
  "include/scanning/grid_scanner_ui.h"
  "include/scanning/grid_scanner_ui.cpp"
  "include/scanning/i_scan_motion_controller.h"
  "include/scanning/PIScanMotionAdapter.h"
   "include/scanning/grid_scanner_manager.h"
   "include/scanning/grid_volume_scanner.h"
    "include/scanning/grid_volume_scanner.cpp" "include/scanning/grid_volume_scanner_ui.h" "include/scanning/grid_volume_scanner_ui.cpp" "include/scanning/grid_volume_scanner_manager.h"  "include/vision/VisionCameraExposureManager.h" "include/vision/VisionCameraExposureManager.cpp" "include/vision/VisionCameraExposureUI.h" "include/vision/VisionCameraExposureUI.cpp" "include/data/DUTDatabaseViewerUI.h" "include/data/DUTDatabaseViewerUI.cpp" "include/CoordinateSystem/CoordinateSystem.h" "include/CoordinateSystem/CoordinateSystem.cpp" "include/MachineOperationsExtended.h" "include/MachineOperationsExtended.cpp" "include/motions/IGantryManager.h" "include/motions/ACSGantryAdapter.h"   "include/SequenceOperationsExtended.h")

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



# Include directories
target_include_directories(uaa3App PRIVATE
  ${SDL2_INCLUDE_DIR}
  ${IMGUI_DIR}
  ${ACSC_DIR}
  ${PYLON_INCLUDE_DIR}
  ${PI_INCLUDE_DIR}
  ${EZIIO_DIR_INCLUDE}
  ${EZIIO_DIR_INCLUDE_x64}
  ${SDL2_TTF_INCLUDE_DIR}
  ${SQLITE_INCLUDE_DIR}
  ${RAYLIB_INCLUDE_DIR}
  ${HALCON_INCLUDE_DIR}
  ${STB_IMAGE_DIR}
  ${MAINUI_DIR}
  ${INCL_CAMERA}
  ${INCL_OPS}
  ${INCL_PROGRAM}
  ${INCL_DIR}
  ${INCL_DATA}
  ${INCL_MOTIONS}
  ${INCL_EZIIO}
  ${INCL_SMU}
  ${INCL_SCANNING}
  ${INCL_SCRIPT}
  ${INCL_POWERSUPPLY}
  ${INCL_UI}
  ${INCL_PROCESSES}
  ${INCL_CLD101x}
  ${INCL_SPLASHSCREEN}
  ${INCL_TCP}
  ${INCL_Uaa3_PROCESSES}
  ${FREETYPE_INCLUDE_DIR}
  ${IDS_UEYE_INCLUDE_DIR}
  ${INCLUDE_HALCON_DIR}
  ${INCLUDE_VISION_DIR}
  ${INCL_PRODUCTS}
  ${VISA_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(uaa3App PRIVATE
  ${SDL2MAIN_LIBRARY}
  ${SDL2_LIBRARY}
  ${OPENGL_LIBRARIES}
  ${ACSC_LIBRARY}
  ${PYLON_LIBRARIES}
  ${PI_LIBRARY}
  ${EZIIO_LIBRARY}
  ${SDL2_TTF_LIBRARY}
  nlohmann_json::nlohmann_json
  ${HALCON_LIBRARIES}
  ${RAYLIB_LIBRARY}
  # Raylib Windows dependencies
  winmm     # For timeBeginPeriod/timeEndPeriod
  gdi32     # For Windows GDI
  user32    # For Windows user interface
  shell32   # For Windows shell
  ${FREETYPE_LIBRARY}
  ${IDS_UEYE_LIBRARY}
  ${VISA_LIBRARY}
)

# SQLite compile definitions
target_compile_definitions(uaa3App PRIVATE
    SQLITE_ENABLE_FTS5
    SQLITE_ENABLE_JSON1
    SQLITE_ENABLE_RTREE
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_COLUMN_METADATA

)






















#compile warning suppression emoji
if(MSVC)
    target_compile_options(uaa3App PRIVATE /wd4566)
endif()





# Copy DLLs for Windows
if(WIN32)
    # Copy SDL2.dll
    add_custom_command(TARGET uaa3App POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SDL2_DLL}
        $<TARGET_FILE_DIR:uaa3App>
        COMMENT "Copying SDL2.dll to output directory"
    )
    
    # Copy Pylon DLLs to the output directory
    add_custom_command(TARGET uaa3App POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PYLON_ROOT}/Runtime/x64"
        $<TARGET_FILE_DIR:uaa3App>
        COMMENT "Copying Pylon DLLs to output directory"
    )
    
    # Copy PI_GCS2_DLL to the output directory
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        # 64-bit build
        add_custom_command(TARGET uaa3App POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PI_LIB_DIR}/PI_GCS2_DLL_x64.dll"
            $<TARGET_FILE_DIR:uaa3App>
            COMMENT "Copying PI GCS2 DLL (64-bit) to output directory"
        )
    else()
        # 32-bit build
        add_custom_command(TARGET uaa3App POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PI_LIB_DIR}/PI_GCS2_DLL.dll"
            $<TARGET_FILE_DIR:uaa3App>
            COMMENT "Copying PI GCS2 DLL (32-bit) to output directory"
        )
    endif()

    # Copy ezimotionipluse.dll to the output directory
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        # 64-bit build
        add_custom_command(TARGET uaa3App POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EZIIO_DIR_INCLUDE_x64}/EziMOTIONPlusE.dll"
            $<TARGET_FILE_DIR:uaa3App>
            COMMENT "Copying EziMOTIONPlusE DLL (64-bit) to output directory"
        )
    else()
        # 32-bit build
        add_custom_command(TARGET uaa3App POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EZIIO_DIR_INCLUDE}/EziMOTIONPlusE.dll"
            $<TARGET_FILE_DIR:uaa3App>
            COMMENT "Copying EziMOTIONPlusE DLL (32-bit) to output directory"
        )
    endif()

    # Copy FreeType DLL if using DLL version
    if(FREETYPE_USE_DLL AND EXISTS "${FREETYPE_DLL}")
        add_custom_command(TARGET uaa3App POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${FREETYPE_DLL}
            $<TARGET_FILE_DIR:uaa3App>
            COMMENT "Copying FreeType 2.13.3 DLL to output directory"
        )
        message(STATUS "FreeType DLL will be copied: ${FREETYPE_DLL}")
    elseif(FREETYPE_USE_DLL AND NOT EXISTS "${FREETYPE_DLL}")
        message(WARNING "FreeType DLL not found at: ${FREETYPE_DLL}")
        message(WARNING "Make sure FreeType DLL is in the correct location")
    endif()

    # Copy IDS uEye DLL to the output directory
    add_custom_command(TARGET uaa3App POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${IDS_UEYE_DLL}
        $<TARGET_FILE_DIR:uaa3App>
        COMMENT "Copying IDS uEye DLL to output directory"
    )
endif()

# For Windows, set subsystem
if(WIN32)
    if(MSVC)
        set_target_properties(uaa3App PROPERTIES
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
                # Simply show the PowerShell output (contains version info)
                if(PS_OUTPUT)
                    message(STATUS "PowerShell output: ${PS_OUTPUT}")
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

# Copy fonts directory to build output
add_custom_command(TARGET uaa3App POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory 
    $<TARGET_FILE_DIR:uaa3App>/assets/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/CascadiaCode-Regular.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/JuliaMono-Regular.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/NotoSans-Regular.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/NotoColorEmoji.ttf"
    #"${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Roboto-Regular.ttf"
    $<TARGET_FILE_DIR:uaa3App>/assets/fonts/
    COMMENT "Copying font files to build directory"
)

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
    "keithley6482_devices_config.json"
)

# Copy configuration files
foreach(CONFIG_FILE ${CONFIG_FILES})
    add_custom_command(TARGET uaa3App POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_FILE}"
        $<TARGET_FILE_DIR:uaa3App>/
        COMMENT "Copying ${CONFIG_FILE} if not exist or newer"
    )
endforeach()

# Add verification message
add_custom_command(TARGET uaa3App POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Configuration files synchronized to build directory"
    COMMENT "Configuration file copy completed"
)