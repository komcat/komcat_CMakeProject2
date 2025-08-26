# PowerSupply Module CMakeLists.txt
cmake_minimum_required(VERSION 3.12)
project(PowerSupplyModule)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set VISA paths for PowerSupply module
set(VISA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../external/visa")
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
    message(STATUS "Checked paths:")
    message(STATUS "  Include: ${VISA_INCLUDE_DIR}/visa.h")
    message(STATUS "  Library: ${VISA_LIBRARY}")
endif()

# PowerSupply Test Target
if(VISA_FOUND)
    add_executable(testSPDPowerSupply
        "SPDPowerSupply.h"
        "SPDPowerSupply.cpp"
        "test_spd_power_supply.cpp"
    )
    
    target_include_directories(testSPDPowerSupply PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${VISA_INCLUDE_DIR}
    )
    
    target_link_libraries(testSPDPowerSupply PRIVATE
        ${VISA_LIBRARY}
        ws2_32  # Windows socket library
    )
    
    target_compile_features(testSPDPowerSupply PRIVATE cxx_std_17)
    
    if(WIN32 AND MSVC)
        set_target_properties(testSPDPowerSupply PROPERTIES
            LINK_FLAGS "/SUBSYSTEM:CONSOLE"
        )
    endif()
    
    message(STATUS "testSPDPowerSupply target created successfully")
    
    # Optional: Set output directory to match main project structure
    set_target_properties(testSPDPowerSupply PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
    )
    
else()
    message(STATUS "testSPDPowerSupply target skipped (VISA not found)")
endif()

# Optional: Add install target
if(VISA_FOUND)
    install(TARGETS testSPDPowerSupply
        RUNTIME DESTINATION bin
        COMPONENT PowerSupplyTools
    )
endif()